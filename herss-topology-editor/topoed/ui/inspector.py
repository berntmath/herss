"""Property editor for the selected node. Edits are emitted; the window applies them to the Document."""

from __future__ import annotations

from collections.abc import Callable

from PySide6.QtCore import QRegularExpression, Signal
from PySide6.QtGui import QFontDatabase, QRegularExpressionValidator
from PySide6.QtWidgets import (
    QComboBox,
    QFormLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QPlainTextEdit,
    QPushButton,
    QScrollArea,
    QSpinBox,
    QTableWidget,
    QTableWidgetItem,
    QTabWidget,
    QVBoxLayout,
    QWidget,
)

from .. import connections, topofile
from ..model import Node, Topology
from ..schema import FIELDS, TABLE_COLUMNS, FieldSpec
from ..textutil import to_float, to_int

EditFn = Callable[[Topology], None]


class Inspector(QWidget):
    edit = Signal(str, object, str)  # label, EditFn, origin ("inspector" keeps the form, "structure" rebuilds it)
    notice = Signal(str)

    def __init__(self) -> None:
        super().__init__()
        self._topology: Topology | None = None
        self._index: int | None = None
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        self._title = QLabel("No node selected")
        self._title.setStyleSheet("font-weight: bold; padding: 4px;")
        layout.addWidget(self._title)
        self._tabs = QTabWidget()
        layout.addWidget(self._tabs, 1)
        self._scroll = QScrollArea()
        self._scroll.setWidgetResizable(True)
        self._raw = QPlainTextEdit()
        self._raw.setFont(QFontDatabase.systemFont(QFontDatabase.SystemFont.FixedFont))
        self._raw.setLineWrapMode(QPlainTextEdit.LineWrapMode.NoWrap)
        raw_page = QWidget()
        raw_layout = QVBoxLayout(raw_page)
        raw_layout.addWidget(QLabel("Everything between the NODE and ENDNODE lines, kept exactly as written."))
        raw_layout.addWidget(self._raw, 1)
        apply_button = QPushButton("Apply raw text")
        apply_button.clicked.connect(self._apply_raw)
        raw_layout.addWidget(apply_button)
        self._tabs.addTab(self._scroll, "Properties")
        self._tabs.addTab(raw_page, "Raw text")

    # ---- public -----------------------------------------------------------
    def show_node(self, topology: Topology, index: int | None) -> None:
        self._topology, self._index = topology, index
        if index is None or index >= len(topology.nodes):
            self._index = None
            self._title.setText("No node selected")
            self._set_page(QWidget())
            self._raw.setPlainText("")
            self._tabs.setEnabled(False)
            return
        node = topology.nodes[index]
        self._tabs.setEnabled(True)
        self._title.setText(f"{node.node_type} {node.node_id}: {node.name}")
        self._raw.setPlainText(node.raw_body_text())
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.addWidget(self._header_box(node))
        conn_box = self._connections_box(node, topology)
        if conn_box is not None:
            layout.addWidget(conn_box)
        for box in self._field_boxes(node):
            layout.addWidget(box)
        for box in self._table_boxes(node):
            layout.addWidget(box)
        layout.addStretch(1)
        self._set_page(page)

    def _set_page(self, page: QWidget) -> None:
        # setWidget() deletes the old page at once, which crashes when one of its
        # widgets is the sender of the signal being handled.
        old = self._scroll.takeWidget()
        if old is not None:
            old.hide()
            old.deleteLater()
        self._scroll.setWidget(page)

    # ---- helpers ----------------------------------------------------------
    def _run(self, label: str, fn: Callable[[Node], None], origin: str) -> None:
        index = self._index
        assert index is not None
        self.edit.emit(label, lambda topo: fn(topo.nodes[index]), origin)

    def _refresh(self) -> None:
        if self._topology is not None:
            self.show_node(self._topology, self._index)

    # ---- header -----------------------------------------------------------
    def _header_box(self, node: Node) -> QGroupBox:
        box = QGroupBox("Node")
        form = QFormLayout(box)
        form.addRow("Type", QLabel(node.node_type))

        id_spin = QSpinBox()
        id_spin.setRange(0, 9999)
        id_spin.setValue(node.node_id)
        id_spin.setToolTip("Changing the id does not update references from other nodes.")
        id_spin.editingFinished.connect(lambda: self._commit_id(id_spin.value()))
        form.addRow("Id", id_spin)

        name = QLineEdit(node.name)
        name.setValidator(QRegularExpressionValidator(QRegularExpression(r"\S+")))
        name.setToolTip("HERSS reads the name as a single word.")
        name.editingFinished.connect(lambda: self._commit_name(name.text()))
        form.addRow("Name", name)
        return box

    def _commit_id(self, value: int) -> None:
        if self._topology and self._index is not None and self._topology.nodes[self._index].node_id != value:
            self._run("Change node id", lambda n: setattr(n, "node_id", value), "structure")

    def _commit_name(self, text: str) -> None:
        text = text.strip()
        if text and self._topology and self._index is not None and self._topology.nodes[self._index].name != text:
            self._run("Rename node", lambda n: setattr(n, "name", text), "structure")

    # ---- connections ------------------------------------------------------
    def _connections_box(self, node: Node, topology: Topology) -> QGroupBox | None:
        conns = connections.connections_of(node)
        if not conns:
            return None
        box = QGroupBox("Connections")
        form = QFormLayout(box)
        known = {n.node_id for n in topology.nodes}
        for conn in conns:
            combo = QComboBox()
            combo.addItem("(not connected)", None)
            if conn.spec.clear == "none":
                combo.model().item(0).setEnabled(False)
            for other in topology.nodes:
                combo.addItem(f"{other.node_id}: {other.name} [{other.node_type}]", other.node_id)
            if conn.target_id is not None and conn.target_id not in known:
                combo.addItem(f"missing node {conn.target_id}", conn.target_id)
            elif conn.target_id is None and conn.raw is not None and to_int(conn.raw) is None:
                combo.addItem(f"invalid: {conn.raw}", None)
                combo.setCurrentIndex(combo.count() - 1)
            if conn.target_id is not None:
                combo.setCurrentIndex(max(combo.findData(conn.target_id), 0))
            active = conn.target_id is not None
            combo.setEnabled(active or conn.spec.creatable)
            if not combo.isEnabled():
                combo.setToolTip("Needs extra parameters; edit it in the Raw text tab.")
            combo.activated.connect(
                lambda _i, c=combo, k=conn.spec.kind, lb=conn.spec.label: self._commit_connection(k, lb, c.currentData())
            )
            form.addRow(conn.spec.label, combo)
        return box

    def _commit_connection(self, kind: str, label: str, target: int | None) -> None:
        def edit(node: Node) -> None:
            if target is None:
                connections.clear_target(node, kind)
            else:
                connections.set_target(node, kind, target)

        self._run(f"Set {label.lower()}", edit, "structure")

    # ---- single-value fields ----------------------------------------------
    def _field_boxes(self, node: Node) -> list[QGroupBox]:
        groups: dict[str, QFormLayout] = {}
        boxes: list[QGroupBox] = []

        def form_for(name: str) -> QFormLayout:
            if name not in groups:
                box = QGroupBox(name or "Parameters")
                groups[name] = QFormLayout(box)
                boxes.append(box)
            return groups[name]

        for spec in FIELDS.get(node.node_type, ()):
            if spec.show_always or node.get_token(spec.keyword, spec.index) is not None:
                self._add_field(form_for(spec.group), node, spec, spec.label, 0)
        if node.node_type == "PSTATION":
            for g in range(len(node.find_all("GENERATOR_MAX_DISCHARGE"))):
                spec = FieldSpec("GENERATOR_MAX_DISCHARGE", "", show_always=True)
                self._add_field(form_for("Generators"), node, spec, f"Generator {g} max discharge [m3/s]", g)
        return boxes

    def _add_field(self, form: QFormLayout, node: Node, spec: FieldSpec, label: str, occurrence: int) -> None:
        current = node.get_token(spec.keyword, spec.index, occurrence) or ""
        if spec.kind == "bool":
            widget = QComboBox()
            widget.addItems(["TRUE", "FALSE"])
            if current in ("TRUE", "FALSE"):
                widget.setCurrentText(current)
            else:
                widget.insertItem(0, current)
                widget.setCurrentIndex(0)
            widget.activated.connect(lambda _i, w=widget: self._commit_field(spec, occurrence, w.currentText(), current))
        else:
            widget = QLineEdit(current)
            widget.editingFinished.connect(lambda w=widget: self._commit_field(spec, occurrence, w.text(), current))
        form.addRow(label, widget)

    def _commit_field(self, spec: FieldSpec, occurrence: int, text: str, previous: str) -> None:
        text = text.strip()
        if text == previous or not text:
            return
        if spec.kind == "float" and to_float(text) is None or spec.kind == "int" and to_int(text) is None:
            self.notice.emit(f"{spec.keyword}: {text!r} is not a valid {spec.kind}")
            self._refresh()
            return
        label = f"Edit {spec.keyword}"
        self._run(label, lambda n: n.set_token(spec.keyword, spec.index, text, occurrence), "inspector")

    # ---- tables -----------------------------------------------------------
    def _table_boxes(self, node: Node) -> list[QGroupBox]:
        boxes: list[QGroupBox] = []
        seen: dict[str, int] = {}
        for entry in node.entries():
            occurrence = seen.get(entry.keyword, 0)
            seen[entry.keyword] = occurrence + 1
            if not entry.row_indices:
                continue
            rows = node.get_rows(entry.keyword, occurrence)
            boxes.append(self._table_box(entry.keyword, occurrence, rows))
        return boxes

    def _table_box(self, keyword: str, occurrence: int, rows: list[list[str]]) -> QGroupBox:
        known = TABLE_COLUMNS.get(keyword, ())
        width = max([len(known), *(len(r) for r in rows)])
        headers = [known[c] if c < len(known) else f"col {c + 1}" for c in range(width)]
        title = keyword if occurrence == 0 else f"{keyword} #{occurrence + 1}"
        box = QGroupBox(title)
        layout = QVBoxLayout(box)
        table = QTableWidget(len(rows), width)
        table.setHorizontalHeaderLabels(headers)
        for r, row in enumerate(rows):
            for c, text in enumerate(row):
                table.setItem(r, c, QTableWidgetItem(text))
            for c in range(len(row), width):
                table.setItem(r, c, QTableWidgetItem(""))
        table.setMinimumHeight(min(len(rows), 8) * 26 + 34)
        table.itemChanged.connect(lambda _item: self._commit_table(table, keyword, occurrence, rows))
        layout.addWidget(table)

        buttons = QHBoxLayout()
        add = QPushButton("Add row")
        add.clicked.connect(lambda: self._change_rows(keyword, occurrence, lambda r: r + [list(r[-1])]))
        remove = QPushButton("Remove selected rows")
        remove.clicked.connect(lambda: self._remove_rows(table, keyword, occurrence))
        buttons.addWidget(add)
        buttons.addWidget(remove)
        buttons.addStretch(1)
        layout.addLayout(buttons)
        return box

    def _commit_table(self, table: QTableWidget, keyword: str, occurrence: int, original: list[list[str]]) -> None:
        rows: list[list[str]] = []
        for r in range(table.rowCount()):
            cells = [(table.item(r, c).text() if table.item(r, c) else "").strip() for c in range(table.columnCount())]
            while cells and not cells[-1]:
                cells.pop()
            if not cells or "" in cells or any(len(t.split()) != 1 for t in cells):
                self.notice.emit(f"{keyword}: every cell needs exactly one value")
                self._refresh()
                return
            rows.append(cells)
        if rows != original:
            self._run(f"Edit {keyword}", lambda n: n.set_rows(keyword, rows, occurrence), "inspector")

    def _remove_rows(self, table: QTableWidget, keyword: str, occurrence: int) -> None:
        drop = {i.row() for i in table.selectedIndexes()}
        if drop:
            self._change_rows(keyword, occurrence, lambda rows: [r for i, r in enumerate(rows) if i not in drop])

    def _change_rows(self, keyword: str, occurrence: int, change: Callable[[list[list[str]]], list[list[str]]]) -> None:
        def edit(node: Node) -> None:
            rows = change(node.get_rows(keyword, occurrence))
            if not rows:
                raise ValueError(f"{keyword} needs at least one row; remove it in the Raw text tab instead")
            node.set_rows(keyword, rows, occurrence)

        self._run(f"Change rows of {keyword}", edit, "structure")

    # ---- raw ----------------------------------------------------------------
    def _apply_raw(self) -> None:
        if self._index is None:
            return
        text = self._raw.toPlainText()

        def edit(node: Node) -> None:
            node.set_raw_body_text(text)
            # Parsing raises if the text contains NODE/ENDNODE lines that would break the block.
            topofile.parse(topofile.render(Topology(nodes=[node], eol=node.eol)))

        self._run("Edit raw text", edit, "structure")
