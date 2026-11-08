from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

from PySide6.QtCore import QTimer, Qt
from PySide6.QtGui import QAction, QBrush, QColor, QKeySequence
from PySide6.QtWidgets import (
    QDockWidget,
    QFileDialog,
    QListWidget,
    QListWidgetItem,
    QMainWindow,
    QMenu,
    QMessageBox,
    QTabWidget,
)

from .. import connections, layout, schema, topofile, validate
from ..document import Document
from ..model import Topology
from .canvas import CanvasScene, CanvasView
from .configurator import ConfiguratorTab
from .dialogs import AddNodeDialog
from .inspector import Inspector
from .plotsession import PlotSession
from .plottab import PlotTab

_EDIT_ERRORS = (ValueError, KeyError, IndexError)
_SEVERITY_COLORS = {validate.ERROR: "#c1121f", validate.WARNING: "#b45f06"}


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.doc = Document()
        self.issues: list[validate.Issue] = []
        self.selected_index: int | None = None

        self.scene = CanvasScene()
        self.view = CanvasView(self.scene)
        self.session = PlotSession(self.doc)
        self.configurator = ConfiguratorTab(self.session)
        self.plot_tab = PlotTab(self.session)
        self.tabs = QTabWidget()
        self.tabs.addTab(self.view, "Topology")
        self.tabs.addTab(self.configurator, "Configurator")
        self.tabs.addTab(self.plot_tab, "Plot")
        self.setCentralWidget(self.tabs)
        self._docks: list[QDockWidget] = []
        self._dock_hidden: dict[QDockWidget, bool] = {}
        self._topology_actions: list[QAction] = []
        self._undo_redo: list[tuple[QAction, QKeySequence.StandardKey]] = []
        self.node_list = QListWidget()
        self.inspector = Inspector()
        self.issue_list = QListWidget()
        self._add_dock("Nodes", self.node_list, Qt.DockWidgetArea.LeftDockWidgetArea)
        self._add_dock("Properties", self.inspector, Qt.DockWidgetArea.RightDockWidgetArea)
        self._add_dock("Issues", self.issue_list, Qt.DockWidgetArea.BottomDockWidgetArea)
        self.resize(1400, 850)
        self.resizeDocks([self.inspector.parentWidget()], [430], Qt.Orientation.Horizontal)
        self.resizeDocks([self.issue_list.parentWidget()], [150], Qt.Orientation.Vertical)

        self._build_actions()
        self.statusBar()

        self.doc.changed.connect(self._on_document_changed)
        self.doc.modifiedChanged.connect(lambda _m: self._update_title())
        self.doc.pathChanged.connect(self._update_title)
        self.session.configChanged.connect(self._update_title)
        self.tabs.currentChanged.connect(self._on_tab_changed)
        self.scene.nodeSelected.connect(self._on_canvas_selected)
        self.scene.connectRequested.connect(self._on_connect_requested)
        self.scene.positionChanged.connect(self.doc.set_position)
        self.node_list.currentRowChanged.connect(lambda row: self._select_index(row if row >= 0 else None, "list"))
        self.issue_list.itemActivated.connect(self._on_issue_activated)
        self.inspector.edit.connect(self._apply)
        self.inspector.notice.connect(lambda text: self.statusBar().showMessage(text, 6000))

        self.doc.new()

    def _add_dock(self, title: str, widget, area: Qt.DockWidgetArea) -> None:
        dock = QDockWidget(title, self)
        dock.setWidget(widget)
        self.addDockWidget(area, dock)
        self._docks.append(dock)

    def _on_tab_changed(self, index: int) -> None:
        """The docks and the editing actions belong to the Topology tab."""
        on_topology = index == 0
        for dock in self._docks:
            if on_topology:
                dock.setVisible(not self._dock_hidden.pop(dock, False))
            else:
                self._dock_hidden.setdefault(dock, dock.isHidden())
                dock.hide()
        for act in self._topology_actions:
            act.setEnabled(on_topology)
        self._edit_menu.menuAction().setEnabled(on_topology)
        for act, key in self._undo_redo:
            act.setShortcut(QKeySequence(key) if on_topology else QKeySequence())

    # ---- actions ----------------------------------------------------------
    def _build_actions(self) -> None:
        def action(text: str, slot, shortcut=None) -> QAction:
            act = QAction(text, self)
            act.triggered.connect(slot)
            if shortcut is not None:
                act.setShortcut(QKeySequence(shortcut))
            return act

        file_menu = self.menuBar().addMenu("&File")
        file_menu.addAction(action("&New", self._new, QKeySequence.StandardKey.New))
        file_menu.addAction(action("&Open...", self._open_dialog, QKeySequence.StandardKey.Open))
        file_menu.addAction(action("&Save", self._save, QKeySequence.StandardKey.Save))
        file_menu.addAction(action("Save &As...", self._save_as, QKeySequence.StandardKey.SaveAs))
        file_menu.addSeparator()
        file_menu.addAction(action("&Quit", self.close, QKeySequence.StandardKey.Quit))

        edit_menu = self.menuBar().addMenu("&Edit")
        undo = self.doc.undo_stack.createUndoAction(self, "&Undo")
        undo.setShortcut(QKeySequence.StandardKey.Undo)
        redo = self.doc.undo_stack.createRedoAction(self, "&Redo")
        redo.setShortcut(QKeySequence.StandardKey.Redo)
        edit_menu.addAction(undo)
        edit_menu.addAction(redo)
        edit_menu.addSeparator()
        add_node = action("&Add node...", self._add_node, "Ctrl+Shift+N")
        delete = action("&Delete selected", self._delete_selected, QKeySequence.StandardKey.Delete)
        edit_menu.addAction(add_node)
        edit_menu.addAction(delete)
        self._edit_menu = edit_menu
        self._undo_redo = [(undo, QKeySequence.StandardKey.Undo), (redo, QKeySequence.StandardKey.Redo)]
        self._topology_actions = [add_node, delete]

        view_menu = self.menuBar().addMenu("&View")
        fit = action("&Fit to window", self.view.fit_all, "Ctrl+0")
        auto = action("&Auto layout", self._auto_layout)
        view_menu.addAction(fit)
        view_menu.addAction(auto)
        self._topology_actions += [fit, auto]

    # ---- document -> UI ---------------------------------------------------
    def _on_document_changed(self, origin: str) -> None:
        topo = self.doc.topology
        self.doc.ensure_positions()
        self.issues = validate.validate(topo)
        if origin == "load":
            self.selected_index = None
        elif self.selected_index is not None and self.selected_index >= len(topo.nodes):
            self.selected_index = len(topo.nodes) - 1 if topo.nodes else None

        severity: dict[int, str] = {}
        for issue in self.issues:
            if issue.node_id is not None and severity.get(issue.node_id) != validate.ERROR:
                severity[issue.node_id] = issue.severity

        self.node_list.blockSignals(True)
        self.node_list.clear()
        for node in topo.nodes:
            item = QListWidgetItem(f"{node.node_id:>3}  {node.name}  [{node.node_type}]")
            color = _SEVERITY_COLORS.get(severity.get(node.node_id, ""))
            if color:
                item.setForeground(QBrush(QColor(color)))
            self.node_list.addItem(item)
        if self.selected_index is not None:
            self.node_list.setCurrentRow(self.selected_index)
        self.node_list.blockSignals(False)

        self.issue_list.clear()
        for issue in self.issues:
            item = QListWidgetItem(str(issue))
            item.setForeground(QBrush(QColor(_SEVERITY_COLORS[issue.severity])))
            item.setData(Qt.ItemDataRole.UserRole, issue.node_id)
            self.issue_list.addItem(item)

        self.scene.rebuild(topo, self.doc.positions, self.issues)
        if self.selected_index is not None:
            self.scene.select_node(topo.nodes[self.selected_index].node_id)
        if origin != "inspector":
            self.inspector.show_node(topo, self.selected_index)
        if origin == "load":
            QTimer.singleShot(0, self.view.fit_all)

        errors = sum(1 for i in self.issues if i.severity == validate.ERROR)
        self.statusBar().showMessage(
            f"{len(topo.nodes)} nodes, {errors} errors, {len(self.issues) - errors} warnings"
        )
        self._update_title()

    def _is_modified(self) -> bool:
        return self.doc.is_modified() or self.session.dirty

    def _update_title(self) -> None:
        name = self.doc.path.name if self.doc.path else "untitled"
        self.setWindowTitle(f"{name}{'*' if self._is_modified() else ''} - topoed")

    # ---- selection --------------------------------------------------------
    def _index_of_id(self, node_id: int) -> int | None:
        for i, node in enumerate(self.doc.topology.nodes):
            if node.node_id == node_id:
                return i
        return None

    def _select_index(self, index: int | None, source: str) -> None:
        self.selected_index = index
        topo = self.doc.topology
        if source != "list":
            self.node_list.blockSignals(True)
            self.node_list.setCurrentRow(index if index is not None else -1)
            self.node_list.blockSignals(False)
        if source != "canvas":
            self.scene.select_node(topo.nodes[index].node_id if index is not None else None)
        self.inspector.show_node(topo, index)

    def _on_canvas_selected(self, node_id) -> None:
        if node_id is None:
            # An edge or nothing is selected; keep the inspector on the last node.
            return
        index = self._index_of_id(node_id)
        if index != self.selected_index:
            self._select_index(index, "canvas")

    def _on_issue_activated(self, item: QListWidgetItem) -> None:
        node_id = item.data(Qt.ItemDataRole.UserRole)
        if node_id is not None and (index := self._index_of_id(node_id)) is not None:
            self._select_index(index, "issues")

    # ---- editing ----------------------------------------------------------
    def _apply(self, label: str, edit: Callable[[Topology], None], origin: str = "structure") -> bool:
        before = self._links_with_side_effects()
        try:
            changed = self.doc.apply(label, edit, origin)
        except _EDIT_ERRORS as exc:
            self.statusBar().showMessage(f"{label}: {exc}", 8000)
            self.inspector.show_node(self.doc.topology, self.selected_index)
            return False
        for _node_id, kind in sorted(self._links_with_side_effects() - before):
            if note := connections.activation_note(kind):
                self.statusBar().showMessage(note, 15000)
        return changed

    def _links_with_side_effects(self) -> set[tuple[int, str]]:
        return {
            (node.node_id, conn.kind)
            for node in self.doc.topology.nodes
            for conn in connections.connections_of(node)
            if conn.target_id is not None and connections.activation_note(conn.kind)
        }

    def _add_node(self) -> None:
        dialog = AddNodeDialog(self.doc.topology, self)
        if dialog.exec() != dialog.DialogCode.Accepted:
            return
        node_type, node_id, name, targets = dialog.values()

        def edit(topo: Topology) -> None:
            node = schema.make_node(node_type, node_id, name, targets)
            node.leading = [topo.eol]
            at = next((i for i, n in enumerate(topo.nodes) if n.node_id > node_id), len(topo.nodes))
            topo.nodes.insert(at, node)

        center = self.view.mapToScene(self.view.viewport().rect().center())
        if self.doc.topology.nodes:
            self.doc.set_position(node_id, center.x(), center.y())
        self._apply(f"Add {node_type}", edit)
        index = self._index_of_id(node_id)
        if index is not None:
            self._select_index(index, "add")

    def _delete_selected(self) -> None:
        edge = self.scene.selected_edge()
        if edge is not None:
            owner_id, kind = edge
            index = self._index_of_id(owner_id)
            if index is not None:
                self._apply(
                    "Disconnect",
                    lambda topo: connections.clear_target(topo.nodes[index], kind),
                )
            return
        if self.selected_index is None:
            return
        node = self.doc.topology.nodes[self.selected_index]
        referrers = sorted(
            {
                f"{other.node_id} ({conn.spec.label})"
                for other in self.doc.topology.nodes
                if other is not node
                for conn in connections.connections_of(other)
                if conn.target_id == node.node_id
            }
        )
        text = f"Delete {node.node_type} {node.node_id} ({node.name})?"
        if referrers:
            text += "\n\nThese references will be left pointing at a missing node:\n" + "\n".join(referrers)
        if QMessageBox.question(self, "Delete node", text) != QMessageBox.StandardButton.Yes:
            return
        index = self.selected_index
        self.selected_index = None
        self._apply("Delete node", lambda topo: topo.nodes.pop(index))

    def _on_connect_requested(self, owner_id: int, target_id: int, global_pos) -> None:
        if owner_id == target_id:
            self.statusBar().showMessage("A node cannot connect to itself", 5000)
            return
        index = self._index_of_id(owner_id)
        if index is None:
            return
        node = self.doc.topology.nodes[index]
        target_name = next((n.name for n in self.doc.topology.nodes if n.node_id == target_id), str(target_id))
        options = [
            c.spec
            for c in connections.connections_of(node)
            if c.spec.creatable or c.target_id is not None
        ]
        if not options:
            self.statusBar().showMessage(f"{node.node_type} {node.name} has no link that can be drawn", 6000)
            return

        def connect(kind: str, label: str) -> None:
            self._apply(
                f"Set {label.lower()}",
                lambda topo: connections.set_target(topo.nodes[index], kind, target_id),
            )

        if len(options) == 1:
            connect(options[0].kind, options[0].label)
            return
        labels = [f"{spec.label} -> {target_name}" for spec in options]
        chosen = self._choose_link(labels, global_pos)
        if chosen is not None:
            connect(options[chosen].kind, options[chosen].label)

    def _choose_link(self, labels: list[str], global_pos) -> int | None:
        menu = QMenu(self)
        actions = [menu.addAction(text) for text in labels]
        picked = menu.exec(global_pos)
        return actions.index(picked) if picked in actions else None

    def _auto_layout(self) -> None:
        self.doc.positions = layout.auto_layout(self.doc.topology)
        self.doc._set_layout_dirty(True)
        self._on_document_changed("structure")
        self.view.fit_all()

    # ---- files ------------------------------------------------------------
    def _maybe_save(self) -> bool:
        if not self._is_modified():
            return True
        answer = QMessageBox.question(
            self,
            "Unsaved changes",
            "Save changes before continuing?",
            QMessageBox.StandardButton.Save | QMessageBox.StandardButton.Discard | QMessageBox.StandardButton.Cancel,
        )
        if answer == QMessageBox.StandardButton.Cancel:
            return False
        if answer == QMessageBox.StandardButton.Save:
            return self._save()
        return True

    def _new(self) -> None:
        if self._maybe_save():
            self.doc.new()

    def _open_dialog(self) -> None:
        if not self._maybe_save():
            return
        path, _ = QFileDialog.getOpenFileName(self, "Open topology", "", "Topology files (*.txt);;All files (*)")
        if path:
            self.open_file(path)

    def open_file(self, path: str | Path) -> bool:
        try:
            self.doc.open(path)
        except (OSError, topofile.TopologyParseError) as exc:
            QMessageBox.critical(self, "Cannot open topology", f"{path}\n\n{exc}")
            return False
        return True

    def _save(self) -> bool:
        if self.doc.path is None:
            return self._save_as()
        return self._write(None)

    def _save_as(self) -> bool:
        path, _ = QFileDialog.getSaveFileName(self, "Save topology", str(self.doc.path or ""), "Topology files (*.txt);;All files (*)")
        return bool(path) and self._write(path)

    def _write(self, path: str | None) -> bool:
        errors = [i for i in self.issues if i.severity == validate.ERROR]
        if errors:
            answer = QMessageBox.warning(
                self,
                "Topology has errors",
                f"HERSS is likely to reject this topology ({len(errors)} errors). Save anyway?",
                QMessageBox.StandardButton.Save | QMessageBox.StandardButton.Cancel,
            )
            if answer != QMessageBox.StandardButton.Save:
                return False
        try:
            self.doc.save(path)
            self.session.save()
        except OSError as exc:
            QMessageBox.critical(self, "Cannot save topology", str(exc))
            return False
        self._update_title()
        self.statusBar().showMessage(f"Saved {self.doc.path}", 5000)
        return True

    def closeEvent(self, event) -> None:
        if self._maybe_save():
            event.accept()
        else:
            event.ignore()
