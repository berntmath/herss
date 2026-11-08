"""Interactive node/edge canvas. It only shows the model; every change goes through Document."""

from __future__ import annotations

import math

from PySide6.QtCore import QPoint, QPointF, QRectF, Qt, Signal
from PySide6.QtGui import (
    QBrush,
    QColor,
    QFont,
    QFontMetrics,
    QPainter,
    QPainterPath,
    QPainterPathStroker,
    QPen,
    QPolygonF,
    QTransform,
)
from PySide6.QtWidgets import (
    QGraphicsItem,
    QGraphicsLineItem,
    QGraphicsObject,
    QGraphicsPathItem,
    QGraphicsPolygonItem,
    QGraphicsScene,
    QGraphicsSimpleTextItem,
    QGraphicsView,
)

from ..connections import connections_of
from ..model import Topology
from ..schema import CHANNEL, INTAKE, PSTATION, PUMP, RESERVOIR
from ..validate import ERROR, WARNING, Issue

NODE_W, NODE_H, PORT = 150.0, 54.0, 16.0

# The canvas is effectively unbounded so panning is never limited by the diagram's size.
SCENE_EXTENT = 100_000.0
MIN_ZOOM, MAX_ZOOM = 0.01, 8.0
ZOOM_STEP = 1.15
NODE_COLORS = {
    RESERVOIR: "#9ad1f5",
    PSTATION: "#ffd166",
    CHANNEL: "#b7e4c7",
    PUMP: "#d9b8f5",
    INTAKE: "#f5b5b5",
}

# colour, pen style
EDGE_STYLES = {
    "tunnel": ("#1d3557", Qt.PenStyle.SolidLine),
    "hatch": ("#457b9d", Qt.PenStyle.SolidLine),
    "overflow": ("#d62828", Qt.PenStyle.DashLine),
    "spillway": ("#e76f51", Qt.PenStyle.DashLine),
    "auto_qmin": ("#2a9d8f", Qt.PenStyle.DotLine),
    "downlink": ("#1d3557", Qt.PenStyle.SolidLine),
    "downstream": ("#1b7a74", Qt.PenStyle.SolidLine),
    "pump_source": ("#7b2cbf", Qt.PenStyle.SolidLine),
    "pump_target": ("#7b2cbf", Qt.PenStyle.SolidLine),
    "floodwater": ("#d62828", Qt.PenStyle.DashLine),
    "capture": ("#2a9d8f", Qt.PenStyle.SolidLine),
    "hatch_release": ("#457b9d", Qt.PenStyle.DashDotLine),
}


def _border_point(center: QPointF, toward: QPointF) -> QPointF:
    dx, dy = toward.x() - center.x(), toward.y() - center.y()
    if dx == 0 and dy == 0:
        return center
    scale = min(
        (NODE_W / 2 + 2) / abs(dx) if dx else math.inf,
        (NODE_H / 2 + 2) / abs(dy) if dy else math.inf,
    )
    return QPointF(center.x() + dx * scale, center.y() + dy * scale)


class NodeItem(QGraphicsObject):
    positionChanged = Signal(int, float, float)

    def __init__(self, node_id: int, node_type: str, name: str, severity: str | None):
        super().__init__()
        self.node_id, self.node_type, self.name, self.severity = node_id, node_type, name, severity
        self.edges: list[EdgeItem] = []
        self.setFlags(
            QGraphicsItem.GraphicsItemFlag.ItemIsMovable
            | QGraphicsItem.GraphicsItemFlag.ItemIsSelectable
            | QGraphicsItem.GraphicsItemFlag.ItemSendsGeometryChanges
        )
        self.setZValue(1)
        self.setToolTip(f"{node_type} {node_id}: {name}")

    def boundingRect(self) -> QRectF:
        return QRectF(-NODE_W / 2 - 3, -NODE_H / 2 - 3, NODE_W + PORT + 6, NODE_H + 6)

    def port_rect(self) -> QRectF:
        return QRectF(NODE_W / 2 - 2, -7, PORT - 2, 14)

    def paint(self, painter: QPainter, option, widget=None) -> None:
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        border = {ERROR: "#d00000", WARNING: "#f77f00"}.get(self.severity, "#333333")
        width = 3 if self.severity else 1.2
        if self.isSelected():
            painter.setBrush(QBrush(QColor("#fff3b0")))
        else:
            painter.setBrush(QBrush(QColor(NODE_COLORS.get(self.node_type, "#dddddd"))))
        painter.setPen(QPen(QColor(border), width))
        painter.drawRoundedRect(QRectF(-NODE_W / 2, -NODE_H / 2, NODE_W, NODE_H), 8, 8)

        painter.setBrush(QBrush(QColor("#555555")))
        painter.setPen(QPen(QColor("#333333"), 1))
        painter.drawEllipse(self.port_rect())

        bold = QFont()
        bold.setBold(True)
        painter.setFont(bold)
        painter.setPen(QColor("#111111"))
        title = QFontMetrics(bold).elidedText(
            f"{self.node_id}  {self.name}", Qt.TextElideMode.ElideRight, int(NODE_W - 12)
        )
        painter.drawText(QRectF(-NODE_W / 2 + 6, -NODE_H / 2 + 6, NODE_W - 12, 20), Qt.AlignmentFlag.AlignLeft, title)
        small = QFont()
        small.setPointSizeF(max(small.pointSizeF() - 1, 7))
        painter.setFont(small)
        painter.setPen(QColor("#444444"))
        painter.drawText(QRectF(-NODE_W / 2 + 6, 6, NODE_W - 12, 18), Qt.AlignmentFlag.AlignLeft, self.node_type)

    def itemChange(self, change, value):
        if change == QGraphicsItem.GraphicsItemChange.ItemPositionHasChanged:
            for edge in self.edges:
                edge.update_path()
            self.positionChanged.emit(self.node_id, value.x(), value.y())
        return super().itemChange(change, value)


class EdgeItem(QGraphicsPathItem):
    def __init__(self, owner_id: int, kind: str, label: str, src: NodeItem, dst: NodeItem, offset: float):
        super().__init__()
        self.owner_id, self.kind, self.src, self.dst, self.offset = owner_id, kind, src, dst, offset
        self._color, self._style = EDGE_STYLES.get(kind, ("#333333", Qt.PenStyle.SolidLine))
        self._arrow = QGraphicsPolygonItem(self)
        self._label = QGraphicsSimpleTextItem(label, self)
        self._label.setBrush(QBrush(QColor(self._color)))
        self.setFlag(QGraphicsItem.GraphicsItemFlag.ItemIsSelectable)
        self.setZValue(0)
        self.setToolTip(f"{label} (node {owner_id})")
        src.edges.append(self)
        dst.edges.append(self)
        self._restyle()
        self.update_path()

    def _restyle(self) -> None:
        color = QColor("#ff8800") if self.isSelected() else QColor(self._color)
        pen = QPen(color, 3 if self.isSelected() else 1.8)
        pen.setStyle(self._style)
        self.setPen(pen)
        self._arrow.setBrush(QBrush(color))
        self._arrow.setPen(QPen(color, 1))

    def update_path(self) -> None:
        a, b = self.src.pos(), self.dst.pos()
        mid = (a + b) / 2
        dx, dy = b.x() - a.x(), b.y() - a.y()
        length = math.hypot(dx, dy) or 1.0
        # Backward edges arc away from the row of nodes instead of running through them.
        bend = self.offset + (60.0 if dx < -NODE_W / 2 else 0.0)
        control = QPointF(mid.x() - dy / length * bend * 2, mid.y() + dx / length * bend * 2)
        p1, p2 = _border_point(a, control), _border_point(b, control)
        path = QPainterPath(p1)
        path.quadTo(control, p2)
        self.setPath(path)

        angle = math.atan2(p2.y() - control.y(), p2.x() - control.x())
        size = 11.0
        left = QPointF(p2.x() - size * math.cos(angle - 0.4), p2.y() - size * math.sin(angle - 0.4))
        right = QPointF(p2.x() - size * math.cos(angle + 0.4), p2.y() - size * math.sin(angle + 0.4))
        self._arrow.setPolygon(QPolygonF([p2, left, right]))
        at = path.pointAtPercent(0.5)
        self._label.setPos(at.x() + 4, at.y() - 16)

    def shape(self) -> QPainterPath:
        stroker = QPainterPathStroker()
        stroker.setWidth(12)
        return stroker.createStroke(self.path())

    def itemChange(self, change, value):
        if change == QGraphicsItem.GraphicsItemChange.ItemSelectedHasChanged:
            self._restyle()
        return super().itemChange(change, value)


class CanvasScene(QGraphicsScene):
    nodeSelected = Signal(object)  # node id or None
    connectRequested = Signal(int, int, object)  # owner id, target id, global QPoint
    positionChanged = Signal(int, float, float)

    def __init__(self) -> None:
        super().__init__()
        self._nodes: dict[int, NodeItem] = {}
        self._quiet = False
        self._connect_from: NodeItem | None = None
        self._rubber: QGraphicsLineItem | None = None
        self.setSceneRect(-SCENE_EXTENT, -SCENE_EXTENT, 2 * SCENE_EXTENT, 2 * SCENE_EXTENT)
        self.selectionChanged.connect(self._on_selection_changed)

    # ---- model -> scene ---------------------------------------------------
    def rebuild(self, topo: Topology, positions: dict[int, tuple[float, float]], issues: list[Issue]) -> None:
        keep_node = self.selected_node_id()
        keep_edge = self.selected_edge()
        self._quiet = True
        self.clear()
        self._nodes = {}
        self._rubber = None
        severity: dict[int, str] = {}
        for issue in issues:
            if issue.node_id is not None and severity.get(issue.node_id) != ERROR:
                severity[issue.node_id] = issue.severity

        for node in topo.nodes:
            if node.node_id in self._nodes:
                continue  # duplicate ids: draw the first, the issue list reports the rest
            item = NodeItem(node.node_id, node.node_type, node.name, severity.get(node.node_id))
            item.setPos(*positions.get(node.node_id, (0.0, 0.0)))
            item.positionChanged.connect(self._on_item_moved)
            self.addItem(item)
            self._nodes[node.node_id] = item

        pair_counts: dict[tuple[int, int], int] = {}
        for node in topo.nodes:
            for conn in connections_of(node):
                if conn.target_id is None or node.node_id not in self._nodes or conn.target_id not in self._nodes:
                    continue
                # Draw in the direction water flows; the pump source link reads source -> pump.
                a, b = (conn.target_id, node.node_id) if conn.kind == "pump_source" else (node.node_id, conn.target_id)
                key = (min(a, b), max(a, b))
                index = pair_counts.get(key, 0)
                pair_counts[key] = index + 1
                offset = (index + 1) // 2 * 14 * (1 if index % 2 else -1)
                self.addItem(EdgeItem(node.node_id, conn.kind, conn.spec.label, self._nodes[a], self._nodes[b], offset))

        if keep_node in self._nodes:
            self._nodes[keep_node].setSelected(True)
        elif keep_edge is not None:
            for item in self.items():
                if isinstance(item, EdgeItem) and (item.owner_id, item.kind) == keep_edge:
                    item.setSelected(True)
        self._quiet = False

    def select_node(self, node_id: int | None) -> None:
        self._quiet = True
        self.clearSelection()
        if node_id in self._nodes:
            self._nodes[node_id].setSelected(True)
        self._quiet = False

    # ---- selection --------------------------------------------------------
    def selected_node_id(self) -> int | None:
        for item in self.selectedItems():
            if isinstance(item, NodeItem):
                return item.node_id
        return None

    def selected_edge(self) -> tuple[int, str] | None:
        for item in self.selectedItems():
            if isinstance(item, EdgeItem):
                return item.owner_id, item.kind
        return None

    def _on_selection_changed(self) -> None:
        if not self._quiet:
            self.nodeSelected.emit(self.selected_node_id())

    def _on_item_moved(self, node_id: int, x: float, y: float) -> None:
        if not self._quiet:
            self.positionChanged.emit(node_id, x, y)

    def node_at(self, scene_pos: QPointF) -> NodeItem | None:
        for item in self.items(scene_pos):
            if isinstance(item, NodeItem):
                return item
        return None

    # ---- drag from a node's port to connect -------------------------------
    def mousePressEvent(self, event) -> None:
        if event.button() == Qt.MouseButton.LeftButton:
            item = self.itemAt(event.scenePos(), QTransform())
            if isinstance(item, NodeItem) and item.port_rect().contains(item.mapFromScene(event.scenePos())):
                self._connect_from = item
                self._rubber = self.addLine(
                    item.pos().x(), item.pos().y(), event.scenePos().x(), event.scenePos().y(),
                    QPen(QColor("#ff8800"), 2, Qt.PenStyle.DashLine),
                )
                event.accept()
                return
        super().mousePressEvent(event)

    def mouseMoveEvent(self, event) -> None:
        if self._connect_from is not None and self._rubber is not None:
            origin = self._connect_from.pos()
            self._rubber.setLine(origin.x(), origin.y(), event.scenePos().x(), event.scenePos().y())
            event.accept()
            return
        super().mouseMoveEvent(event)

    def mouseReleaseEvent(self, event) -> None:
        if self._connect_from is not None:
            source, self._connect_from = self._connect_from, None
            if self._rubber is not None:
                self.removeItem(self._rubber)
                self._rubber = None
            target = self.node_at(event.scenePos())
            if target is not None:
                self.connectRequested.emit(source.node_id, target.node_id, event.screenPos())
            event.accept()
            return
        super().mouseReleaseEvent(event)


class CanvasView(QGraphicsView):
    """Left-drag on empty canvas or middle-drag anywhere pans; wheel scrolls; Ctrl+wheel zooms."""

    def __init__(self, scene: CanvasScene) -> None:
        super().__init__(scene)
        self.setRenderHint(QPainter.RenderHint.Antialiasing)
        self.setDragMode(QGraphicsView.DragMode.ScrollHandDrag)
        self.setTransformationAnchor(QGraphicsView.ViewportAnchor.AnchorUnderMouse)
        self.setBackgroundBrush(QBrush(QColor("#fafafa")))
        # The scrollbar thumbs of a huge scene rectangle are meaningless; panning is by mouse, wheel and keys.
        self.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        self.setVerticalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        self._pan_last: QPoint | None = None

    def mousePressEvent(self, event) -> None:
        if event.button() == Qt.MouseButton.MiddleButton:
            self._pan_last = event.position().toPoint()
            self.viewport().setCursor(Qt.CursorShape.ClosedHandCursor)
            event.accept()
            return
        super().mousePressEvent(event)

    def mouseMoveEvent(self, event) -> None:
        if self._pan_last is not None:
            here = event.position().toPoint()
            delta, self._pan_last = here - self._pan_last, here
            self.horizontalScrollBar().setValue(self.horizontalScrollBar().value() - delta.x())
            self.verticalScrollBar().setValue(self.verticalScrollBar().value() - delta.y())
            event.accept()
            return
        super().mouseMoveEvent(event)

    def mouseReleaseEvent(self, event) -> None:
        if self._pan_last is not None and event.button() == Qt.MouseButton.MiddleButton:
            self._pan_last = None
            self.viewport().setCursor(Qt.CursorShape.OpenHandCursor)
            event.accept()
            return
        super().mouseReleaseEvent(event)

    def wheelEvent(self, event) -> None:
        if event.modifiers() & Qt.KeyboardModifier.ControlModifier:
            self.zoom(ZOOM_STEP if event.angleDelta().y() > 0 else 1 / ZOOM_STEP)
            event.accept()
        else:
            super().wheelEvent(event)

    def zoom(self, factor: float) -> None:
        current = self.transform().m11()
        if (factor > 1 and current >= MAX_ZOOM) or (factor < 1 and current <= MIN_ZOOM):
            return
        factor = min(max(current * factor, MIN_ZOOM), MAX_ZOOM) / current
        self.scale(factor, factor)

    def fit_all(self) -> None:
        rect = self.scene().itemsBoundingRect()
        if not rect.isNull():
            self.fitInView(rect.adjusted(-40, -40, 40, 40), Qt.AspectRatioMode.KeepAspectRatio)
