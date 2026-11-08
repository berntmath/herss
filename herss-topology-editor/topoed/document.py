"""The open topology: undo/redo, dirty tracking, file and layout persistence.

Each edit is applied to a copy of the topology, so a failing edit leaves the
document untouched. Undo restores the rendered text, which is lossless.
"""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

from PySide6.QtCore import QObject, Signal
from PySide6.QtGui import QUndoCommand, QUndoStack

from . import layout, topofile
from .model import Topology


class _SnapshotCommand(QUndoCommand):
    def __init__(self, doc: Document, label: str, before: str, after: str, origin: str):
        super().__init__(label)
        self._doc, self._before, self._after, self._origin = doc, before, after, origin

    def redo(self) -> None:
        self._doc._restore(self._after, self._origin)

    def undo(self) -> None:
        self._doc._restore(self._before, "undo")


class Document(QObject):
    changed = Signal(str)  # origin of the change: "structure", "inspector", "undo", "load"
    modifiedChanged = Signal(bool)
    pathChanged = Signal()

    def __init__(self) -> None:
        super().__init__()
        self.topology = Topology()
        self.path: Path | None = None
        self.positions: layout.Positions = {}
        self.layout_dirty = False
        self.undo_stack = QUndoStack(self)
        self.undo_stack.cleanChanged.connect(lambda _clean: self.modifiedChanged.emit(self.is_modified()))

    # ---- state ------------------------------------------------------------
    def is_modified(self) -> bool:
        return not self.undo_stack.isClean() or self.layout_dirty

    def _restore(self, text: str, origin: str) -> None:
        self.topology = topofile.parse(text)
        self.changed.emit(origin)

    # ---- editing ----------------------------------------------------------
    def apply(self, label: str, edit: Callable[[Topology], None], origin: str = "structure") -> bool:
        """Run `edit` on a copy of the topology; returns False when nothing changed."""
        before = topofile.render(self.topology)
        working = topofile.parse(before)
        edit(working)
        after = topofile.render(working)
        if after == before:
            return False
        self.undo_stack.push(_SnapshotCommand(self, label, before, after, origin))
        return True

    def set_position(self, node_id: int, x: float, y: float) -> None:
        if self.positions.get(node_id) != (x, y):
            self.positions[node_id] = (x, y)
            self._set_layout_dirty(True)

    def ensure_positions(self) -> None:
        for node_id, pos in layout.auto_layout(self.topology).items():
            self.positions.setdefault(node_id, pos)

    def _set_layout_dirty(self, dirty: bool) -> None:
        was = self.is_modified()
        self.layout_dirty = dirty
        if self.is_modified() != was:
            self.modifiedChanged.emit(self.is_modified())

    # ---- files ------------------------------------------------------------
    def reset(self, topology: Topology, path: Path | None, positions: layout.Positions) -> None:
        self.topology, self.path, self.positions = topology, path, positions
        self.layout_dirty = False
        self.undo_stack.clear()
        self.undo_stack.setClean()
        self.ensure_positions()
        self.pathChanged.emit()
        self.modifiedChanged.emit(False)
        self.changed.emit("load")

    def new(self) -> None:
        self.reset(Topology(), None, {})

    def open(self, path: str | Path) -> None:
        path = Path(path)
        topology = topofile.load(path)
        self.reset(topology, path, layout.load_positions(path))

    def save(self, path: str | Path | None = None) -> None:
        target = Path(path) if path is not None else self.path
        if target is None:
            raise ValueError("no file name")
        old = self.path
        topofile.save(self.topology, target)
        carry_layout = old is not None and old != target and layout.sidecar_path(old).exists()
        if self.positions and (self.layout_dirty or carry_layout or layout.sidecar_path(target).exists()):
            present = {n.node_id for n in self.topology.nodes}
            layout.save_positions(target, {k: v for k, v in self.positions.items() if k in present})
        self.path = target
        self.undo_stack.setClean()
        self._set_layout_dirty(False)
        self.pathChanged.emit()
        self.modifiedChanged.emit(self.is_modified())
