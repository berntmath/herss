import os

import pytest

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PySide6.QtWidgets import QApplication, QMessageBox  # noqa: E402

from topoed import plotconfig as pc  # noqa: E402
from topoed.ui.mainwindow import MainWindow  # noqa: E402

from .results_helpers import TOPOLOGY_TEXT, make_run_folder  # noqa: E402

TOPOLOGY_TAB, CONFIGURATOR_TAB, PLOT_TAB = 0, 1, 2


@pytest.fixture(scope="module")
def app():
    return QApplication.instance() or QApplication([])


@pytest.fixture
def case(tmp_path):
    folder = tmp_path / "case"
    folder.mkdir()
    (folder / "topology.txt").write_text(TOPOLOGY_TEXT, encoding="utf-8")
    make_run_folder(folder / "output")
    return folder


def _make_window(topology_path):
    win = MainWindow()
    win.show()
    assert win.open_file(topology_path)
    return win


def _close(win):
    win.doc.undo_stack.setClean()
    win.doc.layout_dirty = False
    win.session.dirty = False
    win.close()


@pytest.fixture
def window(app, case):
    win = _make_window(case / "topology.txt")
    yield win
    _close(win)


def curves(item):
    return [c for c in item.listDataItems()]


def test_tabs_exist(window):
    assert [window.tabs.tabText(i) for i in range(3)] == ["Topology", "Configurator", "Plot"]
    assert window.tabs.currentIndex() == TOPOLOGY_TAB


def test_results_folder_is_found_and_loaded(window):
    assert window.session.config.results_dir == "output"
    assert window.session.run is not None and len(window.session.run.nodes) == 6
    assert not window.session.dirty  # a guess is not an edit
    assert not window._is_modified()


def test_plot_tab_draws_the_default_right_panel(window):
    window.tabs.setCurrentIndex(PLOT_TAB)
    tab = window.plot_tab
    assert [len(tab.plot_items[p]) for p in pc.PANELS] == [0, 0, 4]
    assert "Add plots in the Configurator" in tab.panels["left"].hint.text()
    total_production = tab.plot_items["right"][1]
    (curve,) = curves(total_production)
    assert list(curve.getData()[1]) == [3, 4, 5]
    assert tab.status.text().startswith("6 nodes")


def test_plots_follow_config_changes_and_share_the_time_axis(window):
    window.tabs.setCurrentIndex(PLOT_TAB)
    spec = pc.PlotSpec("Fill", "left", series=[pc.SeriesSpec("Res_fr", node_type="RESERVOIR")])
    window.session.update_config(lambda cfg: cfg.plots.append(spec))
    tab = window.plot_tab
    (fill,) = tab.plot_items["left"]
    assert len(curves(fill)) == 3
    master = tab.plot_items["left"][0]
    for item in tab.plot_items["right"]:
        assert item.getViewBox().linkedView(0) is master.getViewBox()
    assert window._is_modified() and window.windowTitle().startswith("topology.txt*")


def test_notices_are_shown_for_skipped_nodes(window):
    window.tabs.setCurrentIndex(PLOT_TAB)
    text = window.plot_tab.notices.text()
    assert "Inflow is missing for nodes 2" in text  # R_C has no Inflow


def test_changes_while_hidden_are_drawn_when_shown(window):
    spec = pc.PlotSpec("Fill", "middle", series=[pc.SeriesSpec("Res_fr", node_type="RESERVOIR")])
    window.session.update_config(lambda cfg: cfg.plots.append(spec))
    assert window.plot_tab.plot_items["middle"] == []
    window.tabs.setCurrentIndex(PLOT_TAB)
    assert len(window.plot_tab.plot_items["middle"]) == 1


def test_unsupported_kind_and_bad_series_do_not_break_the_tab(window):
    cfg_plots = [
        pc.PlotSpec("Scatter", "left", kind="scatter", series=[pc.SeriesSpec("Power", node=3)]),
        pc.PlotSpec("Gone", "left", series=[pc.SeriesSpec("Power", node=42)]),
    ]
    window.session.update_config(lambda cfg: cfg.plots.extend(cfg_plots))
    window.tabs.setCurrentIndex(PLOT_TAB)
    assert len(window.plot_tab.plot_items["left"]) == 1  # only "Gone" is drawn, without data
    text = window.plot_tab.notices.text()
    assert "plot kind 'scatter' is not supported" in text and "node 42 has no results" in text


def test_docks_and_edit_actions_belong_to_the_topology_tab(window):
    dock = window.inspector.parentWidget()
    assert not dock.isHidden() and window._edit_menu.menuAction().isEnabled()
    window.tabs.setCurrentIndex(CONFIGURATOR_TAB)
    assert dock.isHidden() and not window._edit_menu.menuAction().isEnabled()
    assert all(not a.isEnabled() for a in window._topology_actions)
    window.tabs.setCurrentIndex(TOPOLOGY_TAB)
    assert not dock.isHidden() and window._edit_menu.menuAction().isEnabled()


def test_a_dock_closed_by_the_user_stays_closed(window):
    dock = window.inspector.parentWidget()
    dock.hide()
    window.tabs.setCurrentIndex(PLOT_TAB)
    window.tabs.setCurrentIndex(TOPOLOGY_TAB)
    assert dock.isHidden()


def test_no_results_shows_hints(app, tmp_path):
    topology = tmp_path / "topology.txt"
    topology.write_text(TOPOLOGY_TEXT, encoding="utf-8")
    win = _make_window(topology)
    try:
        win.tabs.setCurrentIndex(PLOT_TAB)
        assert win.session.run is None
        assert all(not items for items in win.plot_tab.plot_items.values())
        assert "No results loaded" in win.plot_tab.panels["right"].hint.text()
    finally:
        _close(win)


def test_save_writes_the_plots_file_only_when_needed(window, case, monkeypatch):
    monkeypatch.setattr(QMessageBox, "warning", staticmethod(lambda *a, **k: QMessageBox.StandardButton.Save))
    sidecar = pc.sidecar_path(case / "topology.txt")
    assert window._save() and not sidecar.exists()  # nothing was edited

    spec = pc.PlotSpec("Fill", "left", series=[pc.SeriesSpec("Res_fr", node_type="RESERVOIR")])
    window.session.update_config(lambda cfg: cfg.plots.append(spec))
    assert window._save()
    assert sidecar.exists() and not window.session.dirty and not window._is_modified()

    reopened = _make_window(case / "topology.txt")
    try:
        assert [p.title for p in reopened.session.config.plots] == ["Price", "Total production",
                                                                    "Total reservoir inflow", "Outflow of RIVER", "Fill"]
        assert reopened.session.config.results_dir == "output"
    finally:
        _close(reopened)


def test_a_broken_plots_file_is_not_overwritten(app, case, monkeypatch):
    sidecar = pc.sidecar_path(case / "topology.txt")
    sidecar.write_text("{broken")
    win = _make_window(case / "topology.txt")
    try:
        assert "topology.txt.plots.json" in win.session.config_error
        monkeypatch.setattr(QMessageBox, "warning", staticmethod(lambda *a, **k: QMessageBox.StandardButton.Save))
        win.session.update_config(lambda cfg: cfg.plots.clear())
        win._save()
        assert sidecar.read_text() == "{broken"
    finally:
        _close(win)


def test_save_as_keeps_the_results_folder_valid(window, case, tmp_path, monkeypatch):
    monkeypatch.setattr(QMessageBox, "warning", staticmethod(lambda *a, **k: QMessageBox.StandardButton.Save))
    sub = tmp_path / "elsewhere"
    sub.mkdir()
    window.session.set_results_folder(case / "output")
    assert window._write(str(sub / "moved.txt"))
    saved = pc.load_config(sub / "moved.txt")
    assert saved.results_dir == os.path.relpath(case / "output", sub)
    assert window.session.results_folder().resolve() == (case / "output").resolve()
