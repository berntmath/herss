from __future__ import annotations

from PySide6.QtCore import QRegularExpression
from PySide6.QtGui import QRegularExpressionValidator
from PySide6.QtWidgets import (
    QComboBox,
    QDialog,
    QDialogButtonBox,
    QFormLayout,
    QLabel,
    QLineEdit,
    QSpinBox,
    QVBoxLayout,
    QWidget,
)

from ..model import Topology
from ..schema import INTAKE, NODE_TYPES, PUMP, RESERVOIR


def next_free_id(topology: Topology) -> int:
    used = set(topology.ids())
    candidate = 0
    while candidate in used:
        candidate += 1
    return candidate


class AddNodeDialog(QDialog):
    def __init__(self, topology: Topology, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setWindowTitle("Add node")
        self._topology = topology
        form = QFormLayout()

        self._type = QComboBox()
        self._type.addItems(NODE_TYPES)
        form.addRow("Type", self._type)

        self._id = QSpinBox()
        self._id.setRange(0, 9999)
        self._id.setValue(next_free_id(topology))
        form.addRow("Id", self._id)

        self._name = QLineEdit()
        self._name.setValidator(QRegularExpressionValidator(QRegularExpression(r"\S+")))
        self._name.setPlaceholderText("single word")
        form.addRow("Name", self._name)

        self._first = QComboBox()
        self._second = QComboBox()
        self._first_label, self._second_label = QLabel(), QLabel()
        form.addRow(self._first_label, self._first)
        form.addRow(self._second_label, self._second)

        self._hint = QLabel()
        self._hint.setWordWrap(True)
        self._buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        self._buttons.accepted.connect(self.accept)
        self._buttons.rejected.connect(self.reject)
        layout = QVBoxLayout(self)
        layout.addLayout(form)
        layout.addWidget(self._hint)
        layout.addWidget(self._buttons)

        for widget in (self._type, self._id, self._first, self._second):
            signal = widget.currentIndexChanged if isinstance(widget, QComboBox) else widget.valueChanged
            signal.connect(self._update)
        self._type.currentIndexChanged.connect(self._fill_targets)
        self._name.textChanged.connect(self._update)
        self._fill_targets()

    def _fill_targets(self) -> None:
        node_type = self._type.currentText()
        self._first.clear()
        self._second.clear()
        pool = [n for n in self._topology.nodes if node_type != PUMP or n.node_type == RESERVOIR]
        for combo in (self._first, self._second):
            for node in pool:
                combo.addItem(f"{node.node_id}: {node.name}", node.node_id)
        if node_type == PUMP and self._second.count() > 1:
            self._second.setCurrentIndex(1)
        labels = {INTAKE: ("Downstream node", ""), PUMP: ("Source reservoir", "Target reservoir")}.get(node_type, ("", ""))
        for label, combo, text in ((self._first_label, self._first, labels[0]), (self._second_label, self._second, labels[1])):
            label.setText(text)
            label.setVisible(bool(text))
            combo.setVisible(bool(text))
        self._update()

    def _update(self) -> None:
        node_type = self._type.currentText()
        problems: list[str] = []
        if not self._name.text().strip():
            problems.append("enter a name")
        if self._id.value() in self._topology.ids():
            problems.append(f"id {self._id.value()} is already used")
        if node_type == INTAKE and self._first.count() == 0:
            problems.append("an INTAKE needs an existing node to deliver to")
        if node_type == PUMP:
            if self._first.count() < 2:
                problems.append("a PUMP needs two reservoirs")
            elif self._first.currentData() == self._second.currentData():
                problems.append("source and target must differ")
        notes = ""
        if node_type == PUMP:
            notes = "A pump id must be lower than both reservoir ids; HERSS will reject the file otherwise. "
        self._hint.setText(notes + ("; ".join(problems).capitalize() if problems else ""))
        self._buttons.button(QDialogButtonBox.StandardButton.Ok).setEnabled(not problems)

    def values(self) -> tuple[str, int, str, tuple[int, ...]]:
        node_type = self._type.currentText()
        targets: tuple[int, ...] = ()
        if node_type == INTAKE:
            targets = (self._first.currentData(),)
        elif node_type == PUMP:
            targets = (self._first.currentData(), self._second.currentData())
        return node_type, self._id.value(), self._name.text().strip(), targets
