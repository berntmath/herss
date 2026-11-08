"""Plot tab: three panels (left, middle, right) of stacked time series that share one time axis."""

from __future__ import annotations

import pyqtgraph as pg
from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QHBoxLayout,
    QLabel,
    QPushButton,
    QSplitter,
    QStackedLayout,
    QVBoxLayout,
    QWidget,
)

from .. import plotconfig
from .plotsession import PlotSession

pg.setConfigOptions(background="w", foreground="k", antialias=False)

PALETTE = ["#1f77b4", "#d62728", "#2ca02c", "#ff7f0e", "#9467bd", "#8c564b", "#e377c2", "#17becf", "#7f7f7f", "#bcbd22"]
_MAX_MESSAGES = 8


class _Panel(QWidget):
    """One column: a title and either a stack of plots or a hint when it has none."""

    def __init__(self, name: str) -> None:
        super().__init__()
        self.name = name
        self.graphics = pg.GraphicsLayoutWidget()
        self.hint = QLabel()
        self.hint.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.hint.setWordWrap(True)
        self.hint.setStyleSheet("color: #666;")
        self._stack = QStackedLayout()
        self._stack.addWidget(self.hint)
        self._stack.addWidget(self.graphics)
        title = QLabel(name.capitalize())
        title.setStyleSheet("font-weight: bold;")
        layout = QVBoxLayout(self)
        layout.setContentsMargins(2, 2, 2, 2)
        layout.addWidget(title)
        layout.addLayout(self._stack, 1)

    def show_hint(self, text: str) -> None:
        self.hint.setText(text)
        self._stack.setCurrentWidget(self.hint)

    def show_plots(self) -> None:
        self._stack.setCurrentWidget(self.graphics)


class PlotTab(QWidget):
    def __init__(self, session: PlotSession) -> None:
        super().__init__()
        self.session = session
        self._stale = True
        self.plot_items: dict[str, list[pg.PlotItem]] = {name: [] for name in plotconfig.PANELS}
        self.messages: list[str] = []

        self.status = QLabel()
        self.reset_button = QPushButton("Reset view")
        self.reset_button.clicked.connect(self.reset_view)
        top = QHBoxLayout()
        top.addWidget(self.status, 1)
        top.addWidget(self.reset_button)

        self.panels = {name: _Panel(name) for name in plotconfig.PANELS}
        splitter = QSplitter(Qt.Orientation.Horizontal)
        for panel in self.panels.values():
            splitter.addWidget(panel)
        self.notices = QLabel()
        self.notices.setWordWrap(True)
        self.notices.setStyleSheet("color: #b45f06;")

        layout = QVBoxLayout(self)
        layout.addLayout(top)
        layout.addWidget(splitter, 1)
        layout.addWidget(self.notices)

        for signal in (session.configChanged, session.runChanged, session.topologyChanged):
            signal.connect(self._mark_stale)

    # ---- refreshing -------------------------------------------------------
    def _mark_stale(self) -> None:
        self._stale = True
        if self.isVisible():
            self.refresh()

    def showEvent(self, event) -> None:
        super().showEvent(event)
        if self._stale:
            self.refresh()

    def reset_view(self) -> None:
        for items in self.plot_items.values():
            for item in items:
                item.enableAutoRange()

    def refresh(self) -> None:
        self._stale = False
        session = self.session
        session.refresh_check()
        self.messages = []
        for name, panel in self.panels.items():
            panel.graphics.clear()
            self.plot_items[name] = []

        run = session.run
        self.status.setText(session.check.summary())
        if run is None:
            for panel in self.panels.values():
                panel.show_hint("No results loaded.\nChoose a results folder in the Configurator.")
            self._show_messages()
            return

        topology = session.doc.topology
        master: pg.PlotItem | None = None
        for name, panel in self.panels.items():
            specs = [s for s in session.config.plots_in(name) if self._drawable(s)]
            for row, spec in enumerate(specs):
                result = plotconfig.evaluate_plot(spec, topology, run)
                item = self._add_plot(panel, result, row, last=row == len(specs) - 1)
                self.plot_items[name].append(item)
                if master is None:
                    master = item
                else:
                    item.setXLink(master)
            if specs:
                panel.show_plots()
            else:
                panel.show_hint("No plots in this panel.\nAdd plots in the Configurator.")
        self._show_messages()

    def _drawable(self, spec: plotconfig.PlotSpec) -> bool:
        if spec.kind in plotconfig.KINDS:
            return True
        self.messages.append(f"{spec.title}: plot kind '{spec.kind}' is not supported")
        return False

    def _add_plot(self, panel: _Panel, result: plotconfig.PlotResult, row: int, last: bool) -> pg.PlotItem:
        item = panel.graphics.addPlot(row=row, col=0, axisItems={"bottom": pg.DateAxisItem(utcOffset=0)})
        title = result.plot.title or "(untitled)"
        item.setTitle(title if result.lines else f"{title} (no data)", size="9pt")
        item.showGrid(x=True, y=True, alpha=0.25)
        item.setClipToView(True)
        item.setDownsampling(auto=True, mode="peak")
        item.getAxis("left").setWidth(60)
        item.getAxis("left").enableAutoSIPrefix(False)
        units = result.units
        item.setLabel("left", units[0] if len(units) == 1 else ("mixed units" if units else ""))
        if not last:
            item.getAxis("bottom").setStyle(showValues=False)
        if len(result.lines) > 1:
            legend = item.addLegend(offset=(5, 5), labelTextSize="8pt")
            legend.setBrush(pg.mkBrush(255, 255, 255, 220))
        for k, line in enumerate(result.lines):
            pen = pg.mkPen(PALETTE[k % len(PALETTE)], width=1.5)
            item.plot(line.times, line.values, pen=pen, name=line.label)
        self.messages += [f"{title}: {notice}" for notice in result.notices]
        return item

    def _show_messages(self) -> None:
        shown = self.messages[:_MAX_MESSAGES]
        if len(self.messages) > _MAX_MESSAGES:
            shown.append(f"... and {len(self.messages) - _MAX_MESSAGES} more")
        self.notices.setText("\n".join(shown))
        self.notices.setVisible(bool(shown))
