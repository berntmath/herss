import os
from pathlib import Path

import pytest

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PySide6.QtCore import QPoint, QPointF, Qt  # noqa: E402
from PySide6.QtGui import QWheelEvent  # noqa: E402
from PySide6.QtTest import QTest  # noqa: E402
from PySide6.QtWidgets import QApplication  # noqa: E402

from topoed.ui.canvas import NodeItem  # noqa: E402
from topoed.ui.mainwindow import MainWindow  # noqa: E402

FIXTURES = Path(__file__).parent / "fixtures"
LEFT, MIDDLE = Qt.MouseButton.LeftButton, Qt.MouseButton.MiddleButton
NO_MOD = Qt.KeyboardModifier.NoModifier


@pytest.fixture(scope="module")
def app():
    return QApplication.instance() or QApplication([])


def make_window(app, tmp_path, fixture):
    path = tmp_path / "topology.txt"
    path.write_bytes((FIXTURES / fixture).read_bytes())
    win = MainWindow()
    win.resize(1400, 850)
    win.show()
    assert win.open_file(path)
    app.processEvents()
    win.view.fit_all()
    app.processEvents()
    return win


@pytest.fixture(params=["mini_pump_test.txt", "fabelvassdraget.txt"])
def window(request, app, tmp_path):
    win = make_window(app, tmp_path, request.param)
    yield win
    win.doc.undo_stack.setClean()
    win.doc.layout_dirty = False
    win.close()


def anchor_in_view(window) -> QPoint:
    """Viewport position of a fixed scene point; it moves exactly as much as the view pans."""
    return window.view.mapFromScene(QPointF(0.0, 0.0))


def empty_spot(window) -> QPoint:
    view = window.view
    rect = view.viewport().rect()
    for y in range(10, rect.height(), 15):
        for x in range(10, rect.width(), 15):
            if not view.items(QPoint(x, y)):
                return QPoint(x, y)
    raise AssertionError("no empty canvas area")


def node_spot(window) -> QPoint:
    item = next(i for i in window.scene.items() if isinstance(i, NodeItem))
    return window.view.mapFromScene(item.pos() + QPointF(-30, 0))  # left of the port, inside the body


def drag(window, app, button, start: QPoint, delta: QPoint) -> None:
    viewport = window.view.viewport()
    QTest.mousePress(viewport, button, NO_MOD, start)
    for fraction in (0.25, 0.5, 1.0):
        QTest.mouseMove(viewport, start + QPoint(int(delta.x() * fraction), int(delta.y() * fraction)))
    QTest.mouseRelease(viewport, button, NO_MOD, start + delta)
    app.processEvents()


def wheel(window, app, dy: int, modifiers=NO_MOD) -> None:
    viewport = window.view.viewport()
    centre = QPointF(viewport.rect().center())
    event = QWheelEvent(
        centre, viewport.mapToGlobal(centre), QPoint(0, 0), QPoint(0, dy),
        Qt.MouseButton.NoButton, modifiers, Qt.ScrollPhase.NoScrollPhase, False,
    )
    QApplication.sendEvent(viewport, event)
    app.processEvents()


def test_left_drag_on_empty_canvas_pans_right_after_fit(window, app):
    before = anchor_in_view(window)
    drag(window, app, LEFT, empty_spot(window), QPoint(-120, -90))
    moved = anchor_in_view(window) - before
    assert (moved.x(), moved.y()) == pytest.approx((-120, -90), abs=2)


def test_left_drag_pans_in_every_direction(window, app):
    for delta in (QPoint(150, 0), QPoint(-150, 0), QPoint(0, 110), QPoint(0, -110)):
        before = anchor_in_view(window)
        drag(window, app, LEFT, empty_spot(window), delta)
        moved = anchor_in_view(window) - before
        assert (moved.x(), moved.y()) == pytest.approx((delta.x(), delta.y()), abs=2), delta


def test_middle_drag_pans_even_over_a_node_and_does_not_move_it(window, app):
    node = next(i for i in window.scene.items() if isinstance(i, NodeItem))
    pos = node.pos()
    before = anchor_in_view(window)
    drag(window, app, MIDDLE, node_spot(window), QPoint(-100, 70))
    moved = anchor_in_view(window) - before
    assert (moved.x(), moved.y()) == pytest.approx((-100, 70), abs=2)
    assert node.pos() == pos
    assert not window.doc.layout_dirty
    assert window.view.viewport().cursor().shape() != Qt.CursorShape.ClosedHandCursor


def test_left_drag_on_a_node_still_moves_the_node_not_the_view(window, app):
    node = next(i for i in window.scene.items() if isinstance(i, NodeItem))
    pos, before = node.pos(), anchor_in_view(window)
    drag(window, app, LEFT, node_spot(window), QPoint(60, 40))
    assert node.pos() != pos
    assert anchor_in_view(window) == before
    assert window.doc.layout_dirty


def test_there_is_room_to_pan_a_full_screen_beyond_the_diagram(window, app):
    view = window.view
    rect = window.scene.itemsBoundingRect()
    scale = view.transform().m11()
    scene_rect = view.sceneRect()
    viewport = view.viewport().rect()
    assert scene_rect.left() <= rect.left() - viewport.width() / scale
    assert scene_rect.right() >= rect.right() + viewport.width() / scale
    assert scene_rect.top() <= rect.top() - viewport.height() / scale
    assert scene_rect.bottom() >= rect.bottom() + viewport.height() / scale


def test_plain_wheel_scrolls_and_does_not_zoom(window, app):
    scale, before = window.view.transform().m11(), anchor_in_view(window)
    wheel(window, app, -120)
    assert window.view.transform().m11() == pytest.approx(scale)
    assert anchor_in_view(window).y() != before.y()


def test_ctrl_wheel_zooms_in_and_out_within_limits(window, app):
    view = window.view
    start = view.transform().m11()
    wheel(window, app, 120, Qt.KeyboardModifier.ControlModifier)
    assert view.transform().m11() > start
    wheel(window, app, -120, Qt.KeyboardModifier.ControlModifier)
    assert view.transform().m11() == pytest.approx(start)
    for _ in range(80):
        wheel(window, app, 120, Qt.KeyboardModifier.ControlModifier)
    assert view.transform().m11() <= 8.0 + 1e-6
    for _ in range(200):
        wheel(window, app, -120, Qt.KeyboardModifier.ControlModifier)
    assert view.transform().m11() >= 0.01 - 1e-6


def test_arrow_keys_pan(window, app):
    window.view.setFocus()
    window.scene.clearSelection()
    before = anchor_in_view(window)
    QTest.keyClick(window.view, Qt.Key.Key_Left)
    QTest.keyClick(window.view, Qt.Key.Key_Up)
    app.processEvents()
    after = anchor_in_view(window)
    assert after.x() > before.x() and after.y() > before.y()


def test_editing_does_not_shift_the_view(window, app):
    drag(window, app, LEFT, empty_spot(window), QPoint(-140, 80))
    before = anchor_in_view(window)
    index = next(i for i, n in enumerate(window.doc.topology.nodes) if n.node_type == "CHANNEL")
    window._apply("rename", lambda topo: setattr(topo.nodes[index], "name", "Renamed"))
    app.processEvents()
    assert anchor_in_view(window) == before
