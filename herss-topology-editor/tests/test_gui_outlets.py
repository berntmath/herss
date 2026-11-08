import os
from pathlib import Path

import pytest

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PySide6.QtCore import QPoint, Qt  # noqa: E402
from PySide6.QtTest import QTest  # noqa: E402
from PySide6.QtWidgets import QApplication, QComboBox, QGroupBox, QLabel  # noqa: E402

from topoed.ui.mainwindow import MainWindow  # noqa: E402

FIXTURES = Path(__file__).parent / "fixtures"
# Order of the reservoir connection rows in the Properties panel.
TUNNEL, HATCH, OVERFLOW, SPILLWAY, AUTO_QMIN = range(5)


@pytest.fixture(scope="module")
def app():
    return QApplication.instance() or QApplication([])


@pytest.fixture
def window(app, tmp_path):
    path = tmp_path / "topology.txt"
    path.write_bytes((FIXTURES / "mini_pump_test.txt").read_bytes())
    win = MainWindow()
    win.show()
    assert win.open_file(path)
    win.node_list.setCurrentRow(1)  # RESERVOIR 1: tunnel and overflow on, hatch and auto Qmin off
    app.processEvents()
    yield win
    win.doc.undo_stack.setClean()
    win.doc.layout_dirty = False
    win.close()


def page(window):
    return window.inspector._scroll.widget()


def combo(window, row):
    return page(window).findChildren(QComboBox)[row]


def reservoir(window):
    return window.doc.topology.node_by_id(1)


def pick(window, app, row, node_id):
    """Choose a node in a connection drop-down the way a user does: open it and use the keyboard."""
    box = combo(window, row)
    target = box.findData(node_id)
    steps = target - box.currentIndex()
    key = Qt.Key.Key_Down if steps > 0 else Qt.Key.Key_Up
    box.setFocus()
    # One real key press applies one edit, which rebuilds the panel, so re-find the box each time.
    for _ in range(abs(steps)):
        QTest.keyClick(combo(window, row), key)
        app.processEvents()
        if combo(window, row).currentData() == node_id:
            break


def test_the_outlet_rows_are_enabled(window):
    for row in (HATCH, OVERFLOW, SPILLWAY, AUTO_QMIN):
        assert combo(window, row).isEnabled(), row


def test_hatch_can_be_switched_on_in_the_inspector(window, app):
    assert combo(window, HATCH).currentData() is None
    window.inspector._commit_connection("hatch", "Outlet hatch", 2)
    app.processEvents()
    assert reservoir(window).find("OUTLET_HATCH").tokens == ["OUTLET_HATCH", "2", "0.0", "1.0", "90"]
    assert combo(window, HATCH).currentData() == 2
    labels = {w.text() for w in page(window).findChildren(QLabel)}
    assert {"Hatch min Q [m3/s]", "Hatch max Q [m3/s]", "Hatch level [masl]"} <= labels
    assert "actions file" in window.statusBar().currentMessage()


def test_hatch_can_be_switched_on_with_the_keyboard(window, app):
    pick(window, app, HATCH, 0)
    assert combo(window, HATCH).currentData() == 0
    assert reservoir(window).get_token("OUTLET_HATCH", 1) == "0"
    assert len(reservoir(window).find("OUTLET_HATCH").tokens) == 5


def test_auto_qmin_shows_its_period_table_and_level(window, app):
    window.inspector._commit_connection("auto_qmin", "Auto Qmin outlet", 2)
    app.processEvents()
    titles = {b.title() for b in page(window).findChildren(QGroupBox)}
    assert "OUTLET_AUTO_QMIN" in titles and "Auto Qmin outlet" in titles
    labels = {w.text() for w in page(window).findChildren(QLabel)}
    assert "Outlet level [masl]" in labels


def test_switching_an_outlet_off_in_the_inspector(window, app):
    window.inspector._commit_connection("hatch", "Outlet hatch", 2)
    app.processEvents()
    window.inspector._commit_connection("hatch", "Outlet hatch", None)
    app.processEvents()
    assert reservoir(window).find("OUTLET_HATCH").tokens == ["OUTLET_HATCH", "-9999"]
    assert combo(window, HATCH).currentData() is None


def test_spillway_is_refused_while_an_overflow_curve_exists(window, app):
    before = window.doc.topology.nodes[1].raw_body_text()
    window.inspector._commit_connection("spillway", "Spillway", 2)
    app.processEvents()
    assert "both a spillway and an overflow curve" in window.statusBar().currentMessage()
    assert reservoir(window).raw_body_text() == before
    assert not window.doc.undo_stack.canUndo()


def test_overflow_can_be_swapped_for_a_spillway(window, app):
    window.inspector._commit_connection("overflow", "Overflow curve", None)
    window.inspector._commit_connection("spillway", "Spillway", 2)
    app.processEvents()
    assert reservoir(window).find("OVERFLOW_CURVE") is None
    assert reservoir(window).get_token("SPILLWAY", 1) == "2"
    assert not [i for i in window.issues if i.severity == "error"], [str(i) for i in window.issues]


def test_dragging_to_a_node_offers_every_outlet_and_applies_the_choice(window, app, monkeypatch):
    offered = []

    def choose_hatch(labels, _pos):
        offered.extend(labels)
        return next(i for i, text in enumerate(labels) if text.startswith("Outlet hatch"))

    monkeypatch.setattr(window, "_choose_link", choose_hatch)
    window._on_connect_requested(1, 2, QPoint(0, 0))
    app.processEvents()
    names = [t.split(" -> ")[0] for t in offered]
    assert names == ["Outlet tunnel", "Outlet hatch", "Overflow curve", "Spillway", "Auto Qmin outlet"]
    assert reservoir(window).get_token("OUTLET_HATCH", 1) == "2"
    assert window.doc.undo_stack.canUndo()
    window.doc.undo_stack.undo()
    assert reservoir(window).find("OUTLET_HATCH").tokens == ["OUTLET_HATCH", "-9999"]


def test_a_new_outlet_is_drawn_on_the_canvas(window, app):
    from topoed.ui.canvas import EdgeItem

    window.inspector._commit_connection("auto_qmin", "Auto Qmin outlet", 2)
    app.processEvents()
    edges = {(e.owner_id, e.kind) for e in window.scene.items() if isinstance(e, EdgeItem)}
    assert (1, "auto_qmin") in edges


def test_disconnecting_an_outlet_edge_with_delete(window, app):
    from topoed.ui.canvas import EdgeItem

    edge = next(e for e in window.scene.items() if isinstance(e, EdgeItem) and (e.owner_id, e.kind) == (1, "overflow"))
    edge.setSelected(True)
    window._delete_selected()
    app.processEvents()
    assert reservoir(window).find("OVERFLOW_CURVE") is None
    assert [i.message for i in window.issues if i.node_id == 1 and i.severity == "error"] == []
