import os
from pathlib import Path

import pytest

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PySide6.QtCore import QPointF, Qt  # noqa: E402
from PySide6.QtTest import QTest  # noqa: E402
from PySide6.QtWidgets import QApplication, QComboBox, QLineEdit, QPushButton, QSpinBox  # noqa: E402

from topoed import layout, topofile  # noqa: E402
from topoed.ui.canvas import EdgeItem, NodeItem  # noqa: E402
from topoed.ui.mainwindow import MainWindow  # noqa: E402

FIXTURES = Path(__file__).parent / "fixtures"


@pytest.fixture(scope="module")
def app():
    return QApplication.instance() or QApplication([])


@pytest.fixture
def window(app, tmp_path):
    work = tmp_path / "topology.txt"
    work.write_bytes((FIXTURES / "mini_pump_test.txt").read_bytes())
    win = MainWindow()
    assert win.open_file(work)
    yield win
    win.doc.undo_stack.setClean()
    win.doc.layout_dirty = False
    win.close()


def items(window, cls):
    return [i for i in window.scene.items() if isinstance(i, cls)]


def test_canvas_matches_model(window):
    assert {i.node_id for i in items(window, NodeItem)} == {0, 1, 2, 3, 4, 5}
    edges = {(e.owner_id, e.kind) for e in items(window, EdgeItem)}
    assert edges == {
        (0, "pump_source"), (0, "pump_target"), (1, "tunnel"), (1, "overflow"),
        (2, "downstream"), (3, "downlink"), (4, "overflow"),
    }
    assert window.node_list.count() == 6


def test_select_node_shows_inspector(window):
    window.node_list.setCurrentRow(1)
    assert window.selected_index == 1
    assert window.scene.selected_node_id() == 1
    assert "RESERVOIR 1" in window.inspector._title.text()


def test_connect_by_drag_updates_file_and_undo(window):
    path = window.doc.path
    original = path.read_bytes()
    index = 3  # PSTATION 3, tailwater 4
    window._on_connect_requested(3, 5, QPointF(0, 0).toPoint())
    assert window.doc.topology.nodes[index].get_token("DOWNLINK_IDNR") == "5"
    assert window.doc.is_modified()
    window._save()
    saved = path.read_bytes().decode().splitlines()
    before = original.decode().splitlines()
    assert [(a, b) for a, b in zip(before, saved) if a != b] == [("DOWNLINK_IDNR 4", "DOWNLINK_IDNR 5")]
    window.doc.undo_stack.undo()
    assert window.doc.topology.nodes[index].get_token("DOWNLINK_IDNR") == "4"


def test_disconnect_edge_with_delete(window):
    edge = next(e for e in items(window, EdgeItem) if (e.owner_id, e.kind) == (1, "tunnel"))
    edge.setSelected(True)
    window._delete_selected()
    assert window.doc.topology.node_by_id(1).get_token("OUTLET_TUNNEL") == "-9999"


def test_required_link_cannot_be_disconnected(window):
    edge = next(e for e in items(window, EdgeItem) if (e.owner_id, e.kind) == (3, "downlink"))
    edge.setSelected(True)
    window._delete_selected()  # DOWNLINK_IDNR may be cleared: a powerstation may be last in the system
    assert window.doc.topology.node_by_id(3).get_token("DOWNLINK_IDNR") == "-9999"
    pump_edge = next(e for e in items(window, EdgeItem) if (e.owner_id, e.kind) == (0, "pump_target"))
    pump_edge.setSelected(True)
    window._delete_selected()
    assert window.doc.topology.node_by_id(0).get_token("PUMP_TARGET_IDNR") == "1"
    assert "cannot be left unconnected" in window.statusBar().currentMessage()


def test_inspector_field_edit_is_single_line_change(window):
    window.node_list.setCurrentRow(1)
    window.inspector.edit.emit("x", lambda t: t.nodes[1].set_token("HRW", 1, "111.5"), "inspector")
    assert window.doc.topology.nodes[1].get_token("HRW") == "111.5"
    window._save()
    before = (FIXTURES / "mini_pump_test.txt").read_text().splitlines()
    after = window.doc.path.read_text().splitlines()
    assert [(a, b) for a, b in zip(before, after) if a != b] == [("HRW 110.0", "HRW 111.5")]


def test_sidecar_layout_is_written_and_reloaded(window):
    window.doc.set_position(2, 123.0, 45.0)
    window._save()
    sidecar = layout.sidecar_path(window.doc.path)
    assert sidecar.exists()
    assert layout.load_positions(window.doc.path)[2] == (123.0, 45.0)
    window.open_file(window.doc.path)
    assert window.doc.positions[2] == (123.0, 45.0)


def test_add_and_delete_node(window):
    from topoed import schema

    def edit(topo):
        node = schema.make_node("CHANNEL", 6, "Extra")
        topo.nodes.append(node)

    window._apply("Add", edit)
    assert window.doc.topology.node_by_id(6) is not None
    assert len(items(window, NodeItem)) == 7
    window.doc.undo_stack.undo()
    assert window.doc.topology.node_by_id(6) is None


def test_raw_text_edit_rejects_block_breaking_text(window):
    window.node_list.setCurrentRow(2)
    window.inspector._raw.setPlainText("N_CASCADE_LINRES 3\nENDNODE\nFOO\n")
    window.inspector._apply_raw()
    assert "ENDNODE" in window.statusBar().currentMessage() or "outside" in window.statusBar().currentMessage()
    assert window.doc.topology.node_by_id(2).get_token("N_CASCADE_LINRES") == "3"
    assert not window.doc.undo_stack.canUndo()


def test_save_without_edits_is_byte_identical(window):
    original = window.doc.path.read_bytes()
    window._save()
    assert window.doc.path.read_bytes() == original


def test_every_fixture_opens_in_the_gui(app, tmp_path):
    for path in sorted(FIXTURES.glob("*.txt")):
        win = MainWindow()
        copy = tmp_path / path.name
        copy.write_bytes(path.read_bytes())
        assert win.open_file(copy), path
        topo = topofile.load(copy)
        assert win.node_list.count() == len(topo.nodes)
        for i in range(len(topo.nodes)):
            win.node_list.setCurrentRow(i)  # builds the inspector for every node
        win.doc.undo_stack.setClean()
        win.close()


def _page(window):
    return window.inspector._scroll.widget()


def test_rename_through_the_name_field_does_not_crash(window, app):
    window.node_list.setCurrentRow(1)
    name = next(w for w in _page(window).findChildren(QLineEdit) if w.text() == "Reservoir_A")
    name.setFocus()
    name.selectAll()
    QTest.keyClicks(name, "Upper")
    QTest.keyClick(name, Qt.Key.Key_Return)  # a real key event, as when typing in the GUI
    app.processEvents()
    assert window.doc.topology.nodes[1].name == "Upper"
    assert "Upper" in window.node_list.item(1).text()


def test_id_and_connection_edits_through_widgets_do_not_crash(window, app):
    window.node_list.setCurrentRow(3)
    combo = _page(window).findChildren(QComboBox)[0]  # tailwater
    QTest.keyClick(combo, Qt.Key.Key_Down)  # moves from node 4 to node 5
    app.processEvents()
    assert window.doc.topology.nodes[3].get_token("DOWNLINK_IDNR") == "5"

    spin = _page(window).findChildren(QSpinBox)[0]
    spin.setFocus()
    spin.selectAll()
    QTest.keyClicks(spin, "8")
    QTest.keyClick(spin, Qt.Key.Key_Return)
    app.processEvents()
    assert window.doc.topology.nodes[3].node_id == 8


def test_table_buttons_do_not_crash(window, app):
    window.node_list.setCurrentRow(1)
    add = next(b for b in _page(window).findChildren(QPushButton) if b.text() == "Add row")
    QTest.mouseClick(add, Qt.MouseButton.LeftButton)
    app.processEvents()
    assert len(window.doc.topology.nodes[1].get_rows("RESERVOIR_CURVE")) == 6
