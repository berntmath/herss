"""Meaning-based connections (downstream, pump source/target, ...) read from and written to nodes."""

from __future__ import annotations

from dataclasses import dataclass

from .model import Node
from .schema import CONNECTIONS, UNUSED, ConnSpec
from .textutil import to_float, to_int


@dataclass(frozen=True)
class Connection:
    source_id: int
    spec: ConnSpec
    target_id: int | None  # None: not set / negative id (HERSS "unused")
    raw: str | None  # the stored token, shown when it is not a valid id

    @property
    def kind(self) -> str:
        return self.spec.kind


def specs_for(node: Node) -> tuple[ConnSpec, ...]:
    return CONNECTIONS.get(node.node_type, ())


def _token(node: Node, spec: ConnSpec) -> str | None:
    if spec.keyword is None:
        return node.header_extra[spec.index] if spec.index < len(node.header_extra) else None
    return node.get_token(spec.keyword, spec.index)


def read_connection(node: Node, spec: ConnSpec) -> Connection:
    raw = _token(node, spec)
    if spec.gate_index is not None and spec.keyword is not None:
        gate = to_int(node.get_token(spec.keyword, spec.gate_index) or "")
        if gate is None or gate < 0:
            return Connection(node.node_id, spec, None, None)
    value = to_int(raw) if raw is not None else None
    target = value if value is not None and value >= 0 else None
    return Connection(node.node_id, spec, target, raw)


def connections_of(node: Node) -> list[Connection]:
    return [read_connection(node, spec) for spec in specs_for(node)]


def _spec(node: Node, kind: str) -> ConnSpec:
    for spec in specs_for(node):
        if spec.kind == kind:
            return spec
    raise KeyError(f"{node.node_type} has no {kind!r} connection")


def _level(node: Node, keyword: str) -> float:
    value = to_float(node.get_token(keyword) or "")
    if value is None:
        raise ValueError(f"set {keyword} on this reservoir before turning the link on")
    return value


def _fmt(value: float) -> str:
    return f"{value:.6f}".rstrip("0").rstrip(".")


# Starting values for outlets that need more than a node id. They are valid for HERSS but
# only placeholders for the real model; edit them in the Properties panel.
def _create_hatch(node: Node, target: int) -> None:
    node.set_block("OUTLET_HATCH", [f"OUTLET_HATCH {target} 0.0 1.0 {_fmt(_level(node, 'LRW'))}"])


def _create_spillway(node: Node, target: int) -> None:
    if node.find("OVERFLOW_CURVE") is not None:
        raise ValueError("a reservoir cannot use both a spillway and an overflow curve; disconnect the overflow curve first")
    node.set_block("SPILLWAY", [f"SPILLWAY {target} 2.1 10.0 {_fmt(_level(node, 'HRW'))}"])


def _create_overflow(node: Node, target: int) -> None:
    if node.find("SPILLWAY") is not None:
        raise ValueError("a reservoir cannot use both a spillway and an overflow curve; disconnect the spillway first")
    hrw = _level(node, "HRW")
    rows = [(hrw, "0.0"), (hrw + 2, "10.0"), (hrw + 5, "60.0")]
    node.set_block(
        "OVERFLOW_CURVE",
        [f"OVERFLOW_CURVE {len(rows)} {target}", *(f"{_fmt(masl)}\t{flow}" for masl, flow in rows)],
    )


def _create_auto_qmin(node: Node, target: int) -> None:
    node.set_block(
        "OUTLET_AUTO_QMIN",
        [f"OUTLET_AUTO_QMIN 1 {target} {_fmt(_level(node, 'LRW'))}", "01.01\t31.12\t0.0"],
    )


_CREATORS = {
    "hatch": _create_hatch,
    "spillway": _create_spillway,
    "overflow": _create_overflow,
    "auto_qmin": _create_auto_qmin,
}


def activation_note(kind: str) -> str | None:
    """Side effect of switching a link on that the user must handle outside the topology file."""
    if kind == "hatch":
        return (
            "The hatch needs its own column in the actions file; HERSS stops if the number of "
            "action columns does not match the topology."
        )
    return None


def set_target(node: Node, kind: str, target_id: int) -> None:
    spec = _spec(node, kind)
    current = read_connection(node, spec)
    if current.target_id is None and kind in _CREATORS:
        _CREATORS[kind](node, target_id)
        return
    if current.target_id is None and not spec.creatable:
        raise ValueError(f"{spec.label} cannot be created from here")
    value = str(target_id)
    if spec.keyword is None:
        extra = node.header_extra
        while len(extra) <= spec.index:
            extra.append(UNUSED)
        extra[spec.index] = value
    else:
        node.set_token(spec.keyword, spec.index, value)


def clear_target(node: Node, kind: str) -> None:
    spec = _spec(node, kind)
    if spec.clear == "none":
        raise ValueError(f"{spec.label} cannot be left unconnected in HERSS")
    if spec.keyword is None:
        node.header_extra[spec.index : spec.index + 1] = [UNUSED]
    elif node.find(spec.keyword) is None:
        return
    elif spec.clear == "remove":
        node.remove_entry(spec.keyword)
    elif spec.clear == "line":
        node.set_block(spec.keyword, [f"{spec.keyword} {UNUSED}"])
    else:
        node.set_token(spec.keyword, spec.index, UNUSED)
