"""Configurator tab: where the results are, and which plots appear in which panel."""

from __future__ import annotations

import copy
from dataclasses import replace
from pathlib import Path

from PySide6.QtCore import Qt
from PySide6.QtGui import QBrush, QColor
from PySide6.QtWidgets import (
    QComboBox,
    QFileDialog,
    QFormLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QListWidget,
    QListWidgetItem,
    QMenu,
    QPushButton,
    QSplitter,
    QStackedWidget,
    QTreeWidget,
    QTreeWidgetItem,
    QVBoxLayout,
    QCheckBox,
    QWidget,
)

from .. import catalog, plotconfig, results
from ..textutil import to_int
from .plotsession import PlotSession

_COLORS = {plotconfig.ERROR: "#c1121f", plotconfig.WARNING: "#b45f06"}
_ROLE = Qt.ItemDataRole.UserRole
_MODES = ("All nodes of a type", "One node")


def _form_widget(rows: list[tuple[str, QWidget]]) -> tuple[QWidget, QFormLayout]:
    page = QWidget()
    form = QFormLayout(page)
    for label, widget in rows:
        form.addRow(label, widget)
    return page, form


class ConfiguratorTab(QWidget):
    def __init__(self, session: PlotSession) -> None:
        super().__init__()
        self.session = session
        self._stale = True
        self._loading = False  # form widgets are being filled, ignore their signals
        self._editing = False  # the change comes from a form widget, keep the form as it is
        self._sel: tuple[int, int | None] | None = None  # (plot index, series index or None)
        self._issues: list[plotconfig.ConfigIssue] = []
        self._build_ui()
        session.configChanged.connect(self._on_config_changed)
        for signal in (session.runChanged, session.topologyChanged):
            signal.connect(self._on_external_change)

    # ---- building the widgets ---------------------------------------------
    def _build_ui(self) -> None:
        self.results_edit = QLineEdit()
        self.results_edit.setReadOnly(True)
        self.browse_button = QPushButton("Browse...")
        self.find_button = QPushButton("Find")
        self.find_button.setToolTip("Look for ./output/ or the OUTPUTDIR of the global file next to the topology")
        self.reload_button = QPushButton("Reload")
        self.browse_button.clicked.connect(self._browse)
        self.find_button.clicked.connect(self._find)
        self.reload_button.clicked.connect(lambda: self.session.reload_results())
        row = QHBoxLayout()
        row.addWidget(QLabel("Results folder"))
        row.addWidget(self.results_edit, 1)
        for button in (self.browse_button, self.find_button, self.reload_button):
            row.addWidget(button)
        self.results_status = QLabel()
        self.results_status.setWordWrap(True)

        self.error_label = QLabel()
        self.error_label.setWordWrap(True)
        self.error_label.setStyleSheet("color: #c1121f;")
        self.replace_button = QPushButton("Replace with the default configuration")
        self.replace_button.clicked.connect(self.session.replace_broken_config)
        self.error_box = QWidget()
        error_layout = QHBoxLayout(self.error_box)
        error_layout.setContentsMargins(0, 0, 0, 0)
        error_layout.addWidget(self.error_label, 1)
        error_layout.addWidget(self.replace_button)

        self.tree = QTreeWidget()
        self.tree.setHeaderHidden(True)
        self.tree.currentItemChanged.connect(self._on_tree_selected)
        self.add_plot_button = QPushButton("Add plot")
        self.add_series_button = QPushButton("Add series")
        self.quick_button = QPushButton("Quick add")
        self.quick_menu = QMenu(self.quick_button)
        self.quick_menu.aboutToShow.connect(self._fill_quick_menu)
        self.quick_button.setMenu(self.quick_menu)
        self.add_plot_button.clicked.connect(self._add_plot)
        self.add_series_button.clicked.connect(self._add_series)
        buttons = QHBoxLayout()
        for button in (self.add_plot_button, self.add_series_button, self.quick_button):
            buttons.addWidget(button)
        left = QWidget()
        left_layout = QVBoxLayout(left)
        left_layout.setContentsMargins(0, 0, 0, 0)
        left_layout.addWidget(self.tree, 1)
        left_layout.addLayout(buttons)

        self.editor = QStackedWidget()
        self._build_empty_page()
        self._build_plot_page()
        self._build_series_page()

        self.splitter = QSplitter(Qt.Orientation.Horizontal)
        self.splitter.addWidget(left)
        self.splitter.addWidget(self.editor)
        self.splitter.setStretchFactor(0, 1)
        self.splitter.setStretchFactor(1, 1)

        self.message = QLabel()
        self.message.setStyleSheet("color: #b45f06;")
        self.problems = QListWidget()
        self.problems.setMaximumHeight(110)

        layout = QVBoxLayout(self)
        layout.addLayout(row)
        layout.addWidget(self.results_status)
        layout.addWidget(self.error_box)
        layout.addWidget(self.splitter, 1)
        layout.addWidget(self.message)
        layout.addWidget(QLabel("Problems"))
        layout.addWidget(self.problems)

    def _build_empty_page(self) -> None:
        hint = QLabel("Select a plot or a series, or add a plot.\n\nEach plot is drawn in the left, middle or right panel of the Plot tab.")
        hint.setWordWrap(True)
        hint.setAlignment(Qt.AlignmentFlag.AlignTop)
        self.empty_page = hint
        self.editor.addWidget(hint)

    def _build_plot_page(self) -> None:
        self.title_edit = QLineEdit()
        self.panel_combo = QComboBox()
        self.panel_combo.addItems(plotconfig.PANELS)
        self.kind_combo = QComboBox()
        self.kind_combo.addItems(plotconfig.KINDS)
        self.up_button = QPushButton("Move up")
        self.down_button = QPushButton("Move down")
        self.delete_plot_button = QPushButton("Delete plot")
        buttons = QWidget()
        row = QHBoxLayout(buttons)
        row.setContentsMargins(0, 0, 0, 0)
        for button in (self.up_button, self.down_button, self.delete_plot_button):
            row.addWidget(button)
        self.plot_page, _ = _form_widget(
            [("Title", self.title_edit), ("Panel", self.panel_combo), ("Kind", self.kind_combo), ("", buttons)]
        )
        self.editor.addWidget(self.plot_page)
        self.title_edit.textEdited.connect(self._on_title)
        self.panel_combo.activated.connect(self._on_panel)
        self.kind_combo.activated.connect(self._on_kind)
        self.up_button.clicked.connect(lambda: self._move(-1))
        self.down_button.clicked.connect(lambda: self._move(1))
        self.delete_plot_button.clicked.connect(self._delete_plot)

    def _build_series_page(self) -> None:
        self.mode_combo = QComboBox()
        self.mode_combo.addItems(_MODES)
        self.type_combo = QComboBox()
        self.all_nodes_check = QCheckBox("All nodes of this type")
        self.nodes_list = QListWidget()
        self.nodes_list.setMaximumHeight(120)
        self.node_combo = QComboBox()
        self.variable_combo = QComboBox()
        self.variable_combo.setEditable(True)
        self.unit_label = QLabel()
        variable_row = QWidget()
        row = QHBoxLayout(variable_row)
        row.setContentsMargins(0, 0, 0, 0)
        row.addWidget(self.variable_combo, 1)
        row.addWidget(self.unit_label)
        self.aggregate_combo = QComboBox()
        self.label_edit = QLineEdit()
        self.label_edit.setPlaceholderText("default")
        self.delete_series_button = QPushButton("Delete series")
        self.series_page, self._series_form = _form_widget(
            [
                ("Select", self.mode_combo),
                ("Node type", self.type_combo),
                ("", self.all_nodes_check),
                ("Nodes", self.nodes_list),
                ("Node", self.node_combo),
                ("Variable", variable_row),
                ("Combine", self.aggregate_combo),
                ("Label", self.label_edit),
                ("", self.delete_series_button),
            ]
        )
        self.editor.addWidget(self.series_page)
        self.mode_combo.activated.connect(self._on_mode)
        self.type_combo.activated.connect(self._on_type)
        self.all_nodes_check.clicked.connect(self._on_all_nodes)
        self.nodes_list.itemChanged.connect(self._on_node_checked)
        self.node_combo.activated.connect(self._on_node)
        self.variable_combo.activated.connect(lambda _i: self._on_variable())
        self.variable_combo.lineEdit().editingFinished.connect(self._on_variable)
        self.aggregate_combo.activated.connect(self._on_aggregate)
        self.label_edit.textEdited.connect(self._on_label)
        self.delete_series_button.clicked.connect(self._delete_series)

    # ---- refreshing -------------------------------------------------------
    def showEvent(self, event) -> None:
        super().showEvent(event)
        if self._stale:
            self._refresh(form=True)

    def _on_config_changed(self) -> None:
        if self.isVisible():
            self._refresh(form=not self._editing)
        else:
            self._stale = True

    def _on_external_change(self) -> None:
        if self.isVisible():
            self._refresh(form=True)
        else:
            self._stale = True

    def _refresh(self, form: bool) -> None:
        self._stale = False
        session = self.session
        session.refresh_check()
        self._issues = plotconfig.check_config(session.config, session.doc.topology, session.run)
        self._update_results_row()
        self._rebuild_tree()
        self._update_problems()
        if form:
            self._load_form()

    def _update_results_row(self) -> None:
        session = self.session
        self.results_edit.setText(session.config.results_dir)
        folder = session.results_folder()
        self.results_edit.setToolTip(str(folder) if folder else "")
        check = session.check
        self.results_status.setText(check.summary())
        color = "#c1121f" if not check.found else "#b45f06" if check.warnings else "#2b7a0b"
        self.results_status.setStyleSheet(f"color: {color};")
        broken = bool(session.config_error)
        self.error_box.setVisible(broken)
        self.error_label.setText(session.config_error)
        self.splitter.setEnabled(not broken)
        self.find_button.setEnabled(session.doc.path is not None)

    def _update_problems(self) -> None:
        self.problems.clear()
        for issue in self._issues:
            item = QListWidgetItem(str(issue))
            item.setForeground(QBrush(QColor(_COLORS[issue.severity])))
            self.problems.addItem(item)

    def _worst(self, plot_index: int, series_index: int | None) -> str | None:
        found = [
            i.severity
            for i in self._issues
            if i.plot_index == plot_index and (series_index is None or i.series_index == series_index)
        ]
        if plotconfig.ERROR in found:
            return plotconfig.ERROR
        return plotconfig.WARNING if found else None

    def _rebuild_tree(self) -> None:
        config = self.session.config
        topology = self.session.doc.topology
        self.tree.blockSignals(True)
        self.tree.clear()
        headers = {name: name.capitalize() for name in plotconfig.PANELS}
        if any(p.panel not in plotconfig.PANELS for p in config.plots):
            headers["?"] = "Other (not a valid panel)"
        current = None
        for name, text in headers.items():
            header = QTreeWidgetItem([text])
            header.setData(0, _ROLE, None)
            font = header.font(0)
            font.setBold(True)
            header.setFont(0, font)
            self.tree.addTopLevelItem(header)
            for index, plot in enumerate(config.plots):
                if (plot.panel if plot.panel in plotconfig.PANELS else "?") != name:
                    continue
                item = QTreeWidgetItem([f"{plot.title or '(untitled)'}"])
                item.setData(0, _ROLE, (index, None))
                self._paint(item, self._worst(index, None))
                header.addChild(item)
                if self._sel == (index, None):
                    current = item
                for s_index, spec in enumerate(plot.series):
                    child = QTreeWidgetItem([plotconfig.describe_series(spec, topology)])
                    child.setData(0, _ROLE, (index, s_index))
                    self._paint(child, self._worst(index, s_index))
                    item.addChild(child)
                    if self._sel == (index, s_index):
                        current = child
            header.setExpanded(True)
        for item in self._all_items():
            item.setExpanded(True)
        if current is not None:
            self.tree.setCurrentItem(current)
        self.tree.blockSignals(False)

    def _all_items(self) -> list[QTreeWidgetItem]:
        items = []
        for i in range(self.tree.topLevelItemCount()):
            header = self.tree.topLevelItem(i)
            for j in range(header.childCount()):
                items.append(header.child(j))
        return items

    @staticmethod
    def _paint(item: QTreeWidgetItem, severity: str | None) -> None:
        if severity:
            item.setForeground(0, QBrush(QColor(_COLORS[severity])))

    def _on_tree_selected(self, current: QTreeWidgetItem | None, _previous) -> None:
        data = current.data(0, _ROLE) if current is not None else None
        self._sel = tuple(data) if data is not None else None
        self.message.clear()
        self._load_form()

    # ---- the selected plot or series ----------------------------------------
    def _selected(self) -> tuple[plotconfig.PlotSpec | None, plotconfig.SeriesSpec | None]:
        if self._sel is None:
            return None, None
        plots = self.session.config.plots
        plot_index, series_index = self._sel
        if plot_index >= len(plots):
            self._sel = None
            return None, None
        plot = plots[plot_index]
        if series_index is None:
            return plot, None
        if series_index >= len(plot.series):
            self._sel = (plot_index, None)
            return plot, None
        return plot, plot.series[series_index]

    def _variable_options(self, spec: plotconfig.SeriesSpec) -> dict[str, str]:
        """Variable name -> unit for the nodes of a series: the catalog first, then the result columns."""
        topology = self.session.doc.topology
        run = self.session.run
        if spec.is_selector:
            nodes = [n for n in topology.nodes if n.node_type == spec.node_type]
            types = [spec.node_type] if spec.node_type in catalog.NODE_TYPES else []
        else:
            node = topology.node_by_id(spec.node)
            nodes = [node] if node else []
            types = [node.node_type] if node else []
        generators = max((to_int(n.get_token("NR_GENERATORS") or "") or 1 for n in nodes if n.node_type == "PSTATION"), default=1)
        options: dict[str, str] = {}
        for node_type in types:
            for name, unit in catalog.variables_for(node_type, generators):
                options.setdefault(name, unit)
        for node in nodes:
            result = run.nodes.get(node.node_id) if run else None
            for column in result.columns if result else ():
                options.setdefault(column, result.unit(column))
        return options

    def _load_form(self) -> None:
        self._loading = True
        try:
            plot, spec = self._selected()
            self.add_series_button.setEnabled(plot is not None)
            if plot is None:
                self.editor.setCurrentWidget(self.empty_page)
            elif spec is None:
                self._load_plot_page(plot)
            else:
                self._load_series_page(spec)
        finally:
            self._loading = False

    def _load_plot_page(self, plot: plotconfig.PlotSpec) -> None:
        self.editor.setCurrentWidget(self.plot_page)
        self.title_edit.setText(plot.title)
        self._set_combo(self.panel_combo, list(plotconfig.PANELS), plot.panel)
        self._set_combo(self.kind_combo, list(plotconfig.KINDS), plot.kind)
        siblings = [i for i, p in enumerate(self.session.config.plots) if p.panel == plot.panel]
        position = siblings.index(self._sel[0]) if self._sel and self._sel[0] in siblings else 0
        self.up_button.setEnabled(position > 0)
        self.down_button.setEnabled(position < len(siblings) - 1)

    def _load_series_page(self, spec: plotconfig.SeriesSpec) -> None:
        topology = self.session.doc.topology
        self.editor.setCurrentWidget(self.series_page)
        selector = spec.is_selector
        self.mode_combo.setCurrentIndex(0 if selector else 1)
        form = self._series_form
        form.setRowVisible(self.type_combo, selector)
        form.setRowVisible(self.all_nodes_check, selector)
        form.setRowVisible(self.nodes_list, selector and spec.node_ids is not None)
        form.setRowVisible(self.node_combo, not selector)
        form.setRowVisible(self.aggregate_combo, selector)

        if selector:
            self._set_combo(self.type_combo, list(catalog.NODE_TYPES), spec.node_type or "")
            self.all_nodes_check.setChecked(spec.node_ids is None)
            self.nodes_list.clear()
            wanted = set(spec.node_ids or [])
            present = sorted(n.node_id for n in topology.nodes if n.node_type == spec.node_type)
            for node_id in present:
                self._add_node_item(f"{node_id} {topology.node_by_id(node_id).name}", node_id, node_id in wanted)
            for node_id in sorted(wanted - set(present)):
                self._add_node_item(f"{node_id} (not in the topology)", node_id, True)
        else:
            self.node_combo.clear()
            for node in sorted(topology.nodes, key=lambda n: n.node_id):
                self.node_combo.addItem(f"{node.node_id} {node.name} ({node.node_type})", node.node_id)
            if topology.node_by_id(spec.node) is None:
                self.node_combo.addItem(f"{spec.node} (not in the topology)", spec.node)
            self.node_combo.setCurrentIndex(self.node_combo.findData(spec.node))

        options = self._variable_options(spec)
        self._fill_combo(self.variable_combo, list(options))
        self.variable_combo.setEditText(spec.variable)
        self.unit_label.setText(f"[{options[spec.variable]}]" if options.get(spec.variable) else "")
        self._set_combo(self.aggregate_combo, list(plotconfig.AGGREGATES), spec.aggregate)
        self.label_edit.setText(spec.label or "")

    def _add_node_item(self, text: str, node_id: int, checked: bool) -> None:
        item = QListWidgetItem(text)
        item.setFlags(item.flags() | Qt.ItemFlag.ItemIsUserCheckable)
        item.setCheckState(Qt.CheckState.Checked if checked else Qt.CheckState.Unchecked)
        item.setData(_ROLE, node_id)
        self.nodes_list.addItem(item)

    @staticmethod
    def _fill_combo(combo: QComboBox, values: list[str]) -> None:
        if [combo.itemText(i) for i in range(combo.count())] != values:
            combo.clear()
            combo.addItems(values)

    @classmethod
    def _set_combo(cls, combo: QComboBox, values: list[str], current: str) -> None:
        """Fill a combo box; a value from the file that is not allowed is shown but not offered."""
        cls._fill_combo(combo, values if current in values else [*values, current])
        combo.setCurrentText(current)

    # ---- editing ----------------------------------------------------------
    def _edit(self, change) -> None:
        if self._loading:
            return
        self._editing = True
        try:
            self.session.update_config(change)
        finally:
            self._editing = False

    def _edit_plot(self, change) -> None:
        if self._sel is None:
            return
        index = self._sel[0]
        self._edit(lambda cfg: change(cfg.plots[index]))

    def _edit_series(self, change) -> None:
        if self._sel is None or self._sel[1] is None:
            return
        plot_index, series_index = self._sel
        self._edit(lambda cfg: change(cfg.plots[plot_index].series[series_index]))

    def _reload_form(self) -> None:
        self._load_form()

    def _on_title(self, text: str) -> None:
        self._edit_plot(lambda plot: setattr(plot, "title", text))

    def _on_panel(self, index: int) -> None:
        self._edit_plot(lambda plot: setattr(plot, "panel", plotconfig.PANELS[index]))
        self._reload_form()

    def _on_kind(self, index: int) -> None:
        self._edit_plot(lambda plot: setattr(plot, "kind", plotconfig.KINDS[index]))

    def _move(self, step: int) -> None:
        if self._sel is None:
            return
        index = self._sel[0]
        plots = self.session.config.plots
        siblings = [i for i, p in enumerate(plots) if p.panel == plots[index].panel]
        target = siblings.index(index) + step
        if not 0 <= target < len(siblings):
            return
        other = siblings[target]

        def swap(cfg: plotconfig.PlotConfig) -> None:
            cfg.plots[index], cfg.plots[other] = cfg.plots[other], cfg.plots[index]

        self._edit(swap)
        self._sel = (other, None)
        self._refresh(form=True)

    def _add_plot(self) -> None:
        plot = plotconfig.PlotSpec("New plot", "left")
        self._edit(lambda cfg: cfg.plots.append(plot))
        self._sel = (len(self.session.config.plots) - 1, None)
        self._refresh(form=True)
        self.title_edit.setFocus()
        self.title_edit.selectAll()

    def _delete_plot(self) -> None:
        if self._sel is None:
            return
        index = self._sel[0]
        self._edit(lambda cfg: cfg.plots.pop(index))
        self._sel = None
        self._refresh(form=True)

    def _add_series(self) -> None:
        plot, _ = self._selected()
        if plot is None:
            return
        topology = self.session.doc.topology
        present = {n.node_type for n in topology.nodes}
        node_type = next((t for t in catalog.NODE_TYPES if t in present), "RESERVOIR")
        generators = 1
        variable = catalog.variables_for(node_type, generators)[0][0]
        plot_index = self._sel[0]
        spec = plotconfig.SeriesSpec(variable, node_type=node_type)
        self._edit(lambda cfg: cfg.plots[plot_index].series.append(spec))
        self._sel = (plot_index, len(self.session.config.plots[plot_index].series) - 1)
        self._refresh(form=True)

    def _delete_series(self) -> None:
        if self._sel is None or self._sel[1] is None:
            return
        plot_index, series_index = self._sel
        self._edit(lambda cfg: cfg.plots[plot_index].series.pop(series_index))
        self._sel = (plot_index, None)
        self._refresh(form=True)

    def _on_mode(self, index: int) -> None:
        topology = self.session.doc.topology

        def convert(spec: plotconfig.SeriesSpec) -> None:
            spec.aggregate = "none"
            if index == 0 and not spec.is_selector:
                node = topology.node_by_id(spec.node)
                present = {n.node_type for n in topology.nodes}
                spec.node_type = node.node_type if node else next((t for t in catalog.NODE_TYPES if t in present), "RESERVOIR")
                spec.node, spec.node_ids = None, None
            elif index == 1 and spec.is_selector:
                ids = plotconfig.selected_node_ids(spec, topology)
                spec.node = ids[0] if ids else (min(topology.ids()) if topology.nodes else 0)
                spec.node_type, spec.node_ids = None, None

        self._edit_series(convert)
        self._reload_form()

    def _on_type(self, index: int) -> None:
        node_type = self.type_combo.itemText(index)

        def change(spec: plotconfig.SeriesSpec) -> None:
            spec.node_type, spec.node_ids = node_type, None
            if spec.variable not in dict(catalog.variables_for(node_type, 1)):
                spec.variable = catalog.variables_for(node_type, 1)[0][0]

        self._edit_series(change)
        self._reload_form()

    def _on_all_nodes(self, checked: bool) -> None:
        topology = self.session.doc.topology

        def change(spec: plotconfig.SeriesSpec) -> None:
            spec.node_ids = None if checked else plotconfig.selected_node_ids(replace(spec, node_ids=None), topology)

        self._edit_series(change)
        self._reload_form()

    def _on_node_checked(self, _item: QListWidgetItem) -> None:
        if self._loading:
            return
        checked = [
            self.nodes_list.item(i).data(_ROLE)
            for i in range(self.nodes_list.count())
            if self.nodes_list.item(i).checkState() == Qt.CheckState.Checked
        ]
        self._edit_series(lambda spec: setattr(spec, "node_ids", checked))

    def _on_node(self, index: int) -> None:
        node_id = self.node_combo.itemData(index)
        self._edit_series(lambda spec: setattr(spec, "node", node_id))
        self._reload_form()

    def _on_variable(self) -> None:
        text = self.variable_combo.currentText().strip()
        _, spec = self._selected()
        if self._loading or spec is None or not text or text == spec.variable:
            return
        self._edit_series(lambda s: setattr(s, "variable", text))
        self._reload_form()

    def _on_aggregate(self, index: int) -> None:
        _, spec = self._selected()
        if self._loading or spec is None:
            return
        aggregate = self.aggregate_combo.itemText(index)
        problem = plotconfig.sum_error(
            replace(spec, aggregate=aggregate), self.session.doc.topology, self.session.run
        )
        if problem:
            self.message.setText(f"{problem}; the setting was not changed")
            self._reload_form()
            return
        self.message.clear()
        self._edit_series(lambda s: setattr(s, "aggregate", aggregate))

    def _on_label(self, text: str) -> None:
        self._edit_series(lambda spec: setattr(spec, "label", text or None))

    # ---- quick add ----------------------------------------------------------
    def _fill_quick_menu(self) -> None:
        self.quick_menu.clear()
        entries = plotconfig.quick_plots(self.session.doc.topology)
        if not entries:
            self.quick_menu.addAction("The topology has no nodes to plot").setEnabled(False)
        for label, plots in entries:
            action = self.quick_menu.addAction(label)
            action.triggered.connect(lambda _checked=False, plots=plots: self._add_quick(plots))

    def _add_quick(self, plots: list[plotconfig.PlotSpec]) -> None:
        added = copy.deepcopy(plots)
        self._edit(lambda cfg: cfg.plots.extend(added))
        self._sel = (len(self.session.config.plots) - len(added), None)
        self._refresh(form=True)

    # ---- results folder -----------------------------------------------------
    def _browse(self) -> None:
        folder = self.session.results_folder()
        start = str(folder if folder and folder.is_dir() else (self.session.doc.path.parent if self.session.doc.path else Path.home()))
        chosen = QFileDialog.getExistingDirectory(self, "Results folder", start)
        if chosen:
            self.session.set_results_folder(Path(chosen))

    def _find(self) -> None:
        path = self.session.doc.path
        found = results.find_results_dir(path) if path else None
        if found is None:
            self.message.setText("No results folder found next to the topology; use Browse")
            return
        self.message.clear()
        self.session.set_results_folder(found)
