import os
import shutil

import pytest

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PySide6.QtCore import QPoint, Qt  # noqa: E402
from PySide6.QtTest import QTest  # noqa: E402
from PySide6.QtWidgets import QApplication, QFileDialog  # noqa: E402

from topoed import plotconfig as pc  # noqa: E402
from topoed.ui.mainwindow import MainWindow  # noqa: E402

from .results_helpers import TOPOLOGY_TEXT, make_run_folder  # noqa: E402

CONFIGURATOR_TAB, PLOT_TAB = 1, 2


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


def _open(topology_path):
    win = MainWindow()
    win.resize(1400, 900)
    win.show()
    assert win.open_file(topology_path)
    win.tabs.setCurrentIndex(CONFIGURATOR_TAB)
    QApplication.processEvents()
    return win


def _close(win):
    win.doc.undo_stack.setClean()
    win.doc.layout_dirty = False
    win.session.dirty = False
    win.close()


@pytest.fixture
def window(app, case):
    win = _open(case / "topology.txt")
    yield win
    _close(win)


def click(widget):
    QTest.mouseClick(widget, Qt.MouseButton.LeftButton)


def click_check_box(box):
    QTest.mouseClick(box, Qt.MouseButton.LeftButton, pos=QPoint(8, box.height() // 2))


def tree_items(tab):
    return {
        (i.parent().text(0) if i.parent() else None, i.text(0)): i
        for i in _walk(tab.tree.invisibleRootItem())
    }


def _walk(item):
    for k in range(item.childCount()):
        child = item.child(k)
        yield child
        yield from _walk(child)


def select(tab, *path_text):
    """Click the tree item with this text, like a user would."""
    for item in _walk(tab.tree.invisibleRootItem()):
        if item.text(0) == path_text[-1] and (len(path_text) == 1 or item.parent().text(0) == path_text[-2]):
            QApplication.processEvents()
            QTest.mouseClick(tab.tree.viewport(), Qt.MouseButton.LeftButton, pos=tab.tree.visualItemRect(item).center())
            return item
    raise AssertionError(f"no tree item {path_text}")


def type_text(line_edit, text, enter=False):
    line_edit.selectAll()
    QTest.keyClicks(line_edit, text)
    if enter:
        QTest.keyClick(line_edit, Qt.Key.Key_Return)


def step_combo(combo, key=Qt.Key.Key_Down):
    QTest.keyClick(combo, key)


def test_results_row_and_default_tree(window):
    tab = window.configurator
    assert tab.results_edit.text() == "output"
    assert tab.results_status.text().startswith("6 nodes")
    items = tree_items(tab)
    assert [k for k in items if k[0] == "Right"] == [
        ("Right", "Price"), ("Right", "Total production"),
        ("Right", "Total reservoir inflow"), ("Right", "Outflow of RIVER")]
    assert ("Right", "Price") in items and ("Price", "1 R_B · Price") not in items
    assert tab.tree.topLevelItemCount() == 3


def test_selecting_a_plot_shows_its_editor(window):
    tab = window.configurator
    select(tab, "Right", "Total production")
    assert tab.editor.currentWidget() is tab.plot_page
    assert tab.title_edit.text() == "Total production" and tab.panel_combo.currentText() == "right"
    select(tab, "Total production", "all PSTATION · Power · sum")
    assert tab.editor.currentWidget() is tab.series_page
    assert tab.type_combo.currentText() == "PSTATION" and tab.variable_combo.currentText() == "Power"
    assert tab.aggregate_combo.currentText() == "sum" and tab.label_edit.text() == "Total production"
    assert tab.unit_label.text() == "[MWh]"


def test_typing_a_title_updates_the_config_and_tree(window):
    tab = window.configurator
    select(tab, "Right", "Price")
    tab.title_edit.selectAll()
    QTest.keyClicks(tab.title_edit, "Spot price")
    assert window.session.config.plots[0].title == "Spot price"
    assert tab.title_edit.text() == "Spot price"  # the form was not reloaded under the cursor
    assert ("Right", "Spot price") in tree_items(tab)
    assert window.session.dirty and window.windowTitle().startswith("topology.txt*")


def test_add_plot_and_series_with_the_buttons(window):
    tab = window.configurator
    click(tab.add_plot_button)
    assert window.session.config.plots[-1].title == "New plot"
    assert tab.editor.currentWidget() is tab.plot_page
    type_text(tab.title_edit, "Fill")
    click(tab.add_series_button)
    plot = window.session.config.plots[-1]
    assert plot.title == "Fill" and len(plot.series) == 1
    assert (plot.series[0].node_type, plot.series[0].variable) == ("RESERVOIR", "Inflow")
    assert tab.editor.currentWidget() is tab.series_page

    type_text(tab.variable_combo.lineEdit(), "Res_fr", enter=True)
    assert plot.series[0].variable == "Res_fr"
    assert tab.unit_label.text() == "[fr]"
    assert "all RESERVOIR · Res_fr" in tree_items(tab)[("Fill", "all RESERVOIR · Res_fr")].text(0)


def test_panel_combo_moves_a_plot(window):
    tab = window.configurator
    click(tab.add_plot_button)
    assert window.session.config.plots[-1].panel == "left"
    step_combo(tab.panel_combo)  # left -> middle
    assert window.session.config.plots[-1].panel == "middle"
    assert ("Middle", "New plot") in tree_items(tab)
    window.tabs.setCurrentIndex(PLOT_TAB)
    assert len(window.plot_tab.plot_items["middle"]) == 1


def test_sum_of_a_fraction_is_refused(window):
    tab = window.configurator
    click(tab.add_plot_button)
    click(tab.add_series_button)
    type_text(tab.variable_combo.lineEdit(), "Res_fr", enter=True)
    step_combo(tab.aggregate_combo)  # none -> sum
    series = window.session.config.plots[-1].series[0]
    assert series.aggregate == "none"
    assert "sum is not allowed for Res_fr [fr]" in tab.message.text()
    assert tab.aggregate_combo.currentText() == "none"

    type_text(tab.variable_combo.lineEdit(), "Inflow", enter=True)
    step_combo(tab.aggregate_combo)
    assert series.aggregate == "sum" and tab.message.text() == ""


def test_switching_between_all_nodes_and_one_node(window):
    tab = window.configurator
    click(tab.add_plot_button)
    click(tab.add_series_button)
    step_combo(tab.mode_combo)  # all nodes of a type -> one node
    series = window.session.config.plots[-1].series[0]
    assert not series.is_selector and series.node == 0
    assert tab.node_combo.currentText().startswith("0 R_A")
    step_combo(tab.node_combo)
    assert series.node == 1
    step_combo(tab.mode_combo, Qt.Key.Key_Up)
    assert series.is_selector and series.node_type == "RESERVOIR" and series.node is None


def test_choosing_some_nodes_of_a_type(window):
    tab = window.configurator
    click(tab.add_plot_button)
    click(tab.add_series_button)
    series = window.session.config.plots[-1].series[0]
    click_check_box(tab.all_nodes_check)  # off: every node is listed and checked
    assert series.node_ids == [0, 1, 2] and tab.nodes_list.count() == 3
    item = tab.nodes_list.item(1)
    rect = tab.nodes_list.visualItemRect(item)
    QTest.mouseClick(tab.nodes_list.viewport(), Qt.MouseButton.LeftButton, pos=QPoint(rect.left() + 10, rect.center().y()))
    assert series.node_ids == [0, 2]
    click_check_box(tab.all_nodes_check)  # on again
    assert series.node_ids is None


def test_node_type_change_picks_a_valid_variable(window):
    tab = window.configurator
    click(tab.add_plot_button)
    click(tab.add_series_button)
    series = window.session.config.plots[-1].series[0]
    step_combo(tab.type_combo)  # RESERVOIR -> PSTATION
    assert series.node_type == "PSTATION" and series.variable == "Up_Inflow"


def test_unknown_node_ids_in_a_selector_are_kept(window):
    window.session.update_config(lambda cfg: cfg.plots.append(pc.PlotSpec("Old", "left", series=[
        pc.SeriesSpec("Res_fr", node_type="RESERVOIR", node_ids=[1, 77])])))
    tab = window.configurator
    select(tab, "Old", "RESERVOIR 1, 77 · Res_fr")
    texts = [tab.nodes_list.item(i).text() for i in range(tab.nodes_list.count())]
    assert "77 (not in the topology)" in texts
    item = tab.nodes_list.item(0)  # R_A, unchecked: checking it keeps 77
    item.setCheckState(Qt.CheckState.Checked)
    assert window.session.config.plots[-1].series[0].node_ids == [0, 1, 77]


def test_move_up_and_down_within_a_panel(window):
    tab = window.configurator
    titles = lambda: [p.title for p in window.session.config.plots]  # noqa: E731
    select(tab, "Right", "Total production")
    assert tab.up_button.isEnabled()
    click(tab.up_button)
    assert titles()[:2] == ["Total production", "Price"]
    assert not tab.up_button.isEnabled()
    click(tab.down_button)
    assert titles()[:2] == ["Price", "Total production"]
    assert tab.title_edit.text() == "Total production"  # the moved plot stays selected


def test_delete_series_and_plot(window):
    tab = window.configurator
    select(tab, "Total production", "all PSTATION · Power · sum")
    click(tab.delete_series_button)
    assert window.session.config.plots[1].series == []
    assert tab.editor.currentWidget() is tab.plot_page
    assert any("has no series" in tab.problems.item(i).text() for i in range(tab.problems.count()))
    click(tab.delete_plot_button)
    assert [p.title for p in window.session.config.plots] == ["Price", "Total reservoir inflow", "Outflow of RIVER"]
    assert tab.editor.currentWidget() is tab.empty_page


def test_quick_add(window):
    tab = window.configurator
    tab._fill_quick_menu()
    actions = {a.text(): a for a in tab.quick_menu.actions()}
    assert "Reservoir fill, all reservoirs (left)" in actions
    actions["Reservoir fill, all reservoirs (left)"].trigger()
    plot = window.session.config.plots[-1]
    assert (plot.title, plot.panel) == ("Reservoir fill", "left")
    assert plot.series[0].node_type == "RESERVOIR" and plot.series[0].variable == "Res_fr"
    assert ("Left", "Reservoir fill") in tree_items(tab)
    # a second click adds an independent copy
    actions["Reservoir fill, all reservoirs (left)"].trigger()
    assert window.session.config.plots[-1] is not window.session.config.plots[-2]


def test_problems_are_listed_and_coloured(window):
    bad = pc.PlotSpec("Bad", "left", series=[
        pc.SeriesSpec("Res_fr", node_type="RESERVOIR", aggregate="sum"), pc.SeriesSpec("Power", node=42)])
    window.session.update_config(lambda cfg: cfg.plots.append(bad))
    tab = window.configurator
    texts = [tab.problems.item(i).text() for i in range(tab.problems.count())]
    assert any("sum is not allowed for Res_fr [fr]" in t for t in texts)
    assert any("node 42 is not in the topology" in t for t in texts)
    items = tree_items(tab)
    assert items[("Left", "Bad")].foreground(0).color().name() == "#c1121f"
    assert items[("Bad", "42 (missing) · Power")].foreground(0).color().name() == "#b45f06"
    assert bad in window.session.config.plots and len(bad.series) == 2  # nothing was dropped


def test_browse_sets_the_results_folder(window, case, tmp_path, monkeypatch):
    other = make_run_folder(tmp_path / "other_results")
    monkeypatch.setattr(QFileDialog, "getExistingDirectory", staticmethod(lambda *a, **k: str(other)))
    tab = window.configurator
    click(tab.browse_button)
    assert window.session.config.results_dir == "../other_results"
    assert tab.results_edit.text() == "../other_results"
    assert window.session.dirty and tab.results_status.text().startswith("6 nodes")
    click(tab.find_button)  # back to ./output/
    assert window.session.config.results_dir == "output"


def test_find_reports_when_nothing_is_found(app, tmp_path):
    topology = tmp_path / "topology.txt"
    topology.write_text(TOPOLOGY_TEXT, encoding="utf-8")
    win = _open(topology)
    try:
        tab = win.configurator
        assert tab.results_edit.text() == "" and "No results folder" in tab.results_status.text()
        click(tab.find_button)
        assert "No results folder found" in tab.message.text()
        assert not win.session.dirty
    finally:
        _close(win)


def test_reload_picks_up_new_results(window, case):
    shutil.rmtree(case / "output")
    click(window.configurator.reload_button)
    assert window.session.run is None and "No results folder" in window.configurator.results_status.text()
    make_run_folder(case / "output")
    click(window.configurator.reload_button)
    assert window.session.run is not None


def test_broken_plots_file_disables_the_editor_until_replaced(app, case):
    pc.sidecar_path(case / "topology.txt").write_text("{broken")
    win = _open(case / "topology.txt")
    try:
        tab = win.configurator
        assert not tab.error_box.isHidden() and not tab.splitter.isEnabled()
        assert "topology.txt.plots.json" in tab.error_label.text()
        click(tab.replace_button)
        assert tab.error_box.isHidden() and tab.splitter.isEnabled()
        assert [p.title for p in win.session.config.plots][0] == "Price"
    finally:
        _close(win)


def test_changes_made_while_hidden_show_up_when_the_tab_is_shown(window):
    window.tabs.setCurrentIndex(PLOT_TAB)
    window.session.update_config(lambda cfg: cfg.plots.append(pc.PlotSpec("Later", "left")))
    window.tabs.setCurrentIndex(CONFIGURATOR_TAB)
    assert ("Left", "Later") in tree_items(window.configurator)


def test_topology_edits_refresh_the_node_lists(window):
    tab = window.configurator
    window.session.update_config(lambda cfg: cfg.plots.append(pc.PlotSpec("P", "left", series=[
        pc.SeriesSpec("Res_fr", node_type="RESERVOIR", node_ids=[0])])))
    select(tab, "P", "RESERVOIR 0 · Res_fr")
    assert tab.nodes_list.count() == 3
    window.tabs.setCurrentIndex(0)
    window.doc.apply("drop R_C", lambda topo: topo.nodes.pop(2))
    window.tabs.setCurrentIndex(CONFIGURATOR_TAB)
    assert tab.nodes_list.count() == 2
