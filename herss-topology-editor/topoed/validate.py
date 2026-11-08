"""Validation mirroring what HERSS checks when it loads a topology (see ../src)."""

from __future__ import annotations

import re
from collections import Counter
from dataclasses import dataclass

from .connections import connections_of
from .model import COUNTED_KEYWORDS, Node, Topology
from .schema import (
    FIELDS,
    INTAKE,
    MAX_NR_NODES,
    NODE_TYPES,
    PSTATION,
    PUMP,
    RESERVOIR,
)
from .textutil import is_blank_or_comment, to_float, to_int

ERROR = "error"
WARNING = "warning"

_NUMERIC_TABLES = frozenset(
    {"RESERVOIR_CURVE", "OVERFLOW_CURVE", "TURBINE_CURVE", "UNIFORM_NORMALIZED_CURVE", "PUMP_CURVE"}
)


@dataclass(frozen=True)
class Issue:
    severity: str
    node_id: int | None
    message: str

    def __str__(self) -> str:
        where = f"node {self.node_id}: " if self.node_id is not None else ""
        return f"{self.severity}: {where}{self.message}"


def validate(topo: Topology) -> list[Issue]:
    issues: list[Issue] = []

    def add(severity: str, node: Node | None, message: str) -> None:
        issues.append(Issue(severity, node.node_id if node else None, message))

    count = len(topo.nodes)
    if count > MAX_NR_NODES:
        add(ERROR, None, f"{count} nodes exceed the HERSS maximum of {MAX_NR_NODES}")

    stray = [ln for n in topo.nodes for ln in n.leading] + list(topo.trailer)
    if any(not is_blank_or_comment(ln) for ln in stray):
        add(WARNING, None, "text outside NODE...ENDNODE blocks is kept but ignored by HERSS")

    id_counts = Counter(n.node_id for n in topo.nodes)
    ids = set(id_counts)
    for node in topo.nodes:
        if id_counts[node.node_id] > 1:
            add(ERROR, node, f"duplicate node id {node.node_id}")
        if not 0 <= node.node_id < count:
            add(ERROR, node, f"id must be in 0..{count - 1} (the number of NODE blocks is {count})")
        if node.node_type not in NODE_TYPES:
            add(ERROR, node, f"unknown node type {node.node_type!r}")

    # HERSS fills its node-type table by order of appearance but reads it by id.
    if [n.node_id for n in topo.nodes] != sorted(n.node_id for n in topo.nodes):
        add(WARNING, None, "nodes are not in ascending id order in the file; HERSS indexes node types by file order")

    by_id = {n.node_id: n for n in topo.nodes if id_counts[n.node_id] == 1}
    for node in topo.nodes:
        _check_connections(node, ids, by_id, add)
        _check_fields(node, add)
        _check_tables(node, add)
        _check_periods(node, add)
        if node.node_type == PUMP:
            _check_pump(node, by_id, add)
        elif node.node_type == RESERVOIR:
            _check_reservoir(node, add)
        elif node.node_type == PSTATION:
            _check_pstation(node, add)
    return issues


def _check_connections(node, ids, by_id, add) -> None:
    for conn in connections_of(node):
        spec = conn.spec
        entry_present = (
            node.find(spec.keyword) is not None if spec.keyword else len(node.header_extra) > spec.index
        )
        if conn.raw is None:
            required = spec.kind in ("pump_source", "pump_target") or node.node_type == INTAKE
            if required and not entry_present:
                add(ERROR, node, f"missing {spec.keyword}")
            continue
        value = to_int(conn.raw)
        if value is None:
            add(ERROR, node, f"{spec.label}: {conn.raw!r} is not an integer node id")
            continue
        if conn.target_id is None:
            if spec.negative_is_error and entry_present and _gate_open(node, spec):
                add(ERROR, node, f"{spec.label}: HERSS requires a non-negative node id")
            continue
        if conn.target_id == node.node_id:
            add(ERROR, node, f"{spec.label} points to the node itself")
        elif conn.target_id not in ids:
            add(ERROR, node, f"{spec.label} refers to missing node {conn.target_id}")
        elif spec.kind == "hatch":
            entry = node.find("OUTLET_HATCH")
            if entry is not None and len(entry.tokens) != 5:
                add(ERROR, node, "OUTLET_HATCH needs 5 columns: id minQ maxQ level")


def _gate_open(node: Node, spec) -> bool:
    if spec.gate_index is None:
        return True
    gate = to_int(node.get_token(spec.keyword, spec.gate_index) or "")
    return gate is not None and gate >= 0


def _check_fields(node: Node, add) -> None:
    for spec in FIELDS.get(node.node_type, ()):
        value = node.get_token(spec.keyword, spec.index)
        if value is None:
            continue
        if spec.kind == "bool":
            if value not in ("TRUE", "FALSE"):
                add(ERROR if spec.keyword == "FAST_OVERFLOW" else WARNING, node,
                    f"{spec.keyword} must be TRUE or FALSE, found {value!r}")
        elif spec.kind == "int":
            if to_int(value) is None:
                add(WARNING, node, f"{spec.keyword}: {value!r} is not an integer")
        elif to_float(value) is None:
            add(WARNING, node, f"{spec.keyword}: {value!r} is not a number")


def _check_tables(node: Node, add) -> None:
    for entry in node.entries():
        if entry.keyword not in COUNTED_KEYWORDS or len(entry.tokens) < 2:
            continue
        declared = to_int(entry.tokens[1])
        if declared is not None and declared > 0 and len(entry.row_indices) < declared:
            add(ERROR, node, f"{entry.keyword} declares {declared} rows but only {len(entry.row_indices)} follow")
        if entry.keyword in _NUMERIC_TABLES:
            for idx in entry.row_indices:
                row = node.body[idx].split()
                if len(row) < 2 or any(to_float(t) is None for t in row[:2]):
                    add(WARNING, node, f"{entry.keyword}: row {row!r} should have two numbers")


def _num(node: Node, keyword: str) -> float | None:
    token = node.get_token(keyword)
    return to_float(token) if token is not None else None


def _check_pump(node: Node, by_id, add) -> None:
    for keyword in ("PUMP_INLET_MASL", "MAX_DISCHARGE", "HEADLOSSCOEF",
                    "STATIC_MOTOR_EFFICIENCY", "PUMP_STARTSTOP"):
        if node.get_token(keyword) is None:
            add(ERROR, node, f"missing {keyword}")
    if node.find("UNIFORM_NORMALIZED_CURVE") is None and node.find("PUMP_CURVE") is None:
        add(ERROR, node, "no efficiency curve (UNIFORM_NORMALIZED_CURVE or PUMP_CURVE)")

    source = to_int(node.get_token("PUMP_SOURCE_IDNR") or "")
    target = to_int(node.get_token("PUMP_TARGET_IDNR") or "")
    if source is not None and source == target:
        add(ERROR, node, "a pump cannot pump a reservoir into itself (source equals target)")
    for label, ref in (("source", source), ("target", target)):
        if ref is None or ref not in by_id:
            continue  # missing or invalid references are reported by the connection check
        if by_id[ref].node_type != RESERVOIR:
            add(ERROR, node, f"pump {label} {ref} must be a RESERVOIR")
        if ref <= node.node_id:
            add(ERROR, node, f"pump id {node.node_id} must be lower than its {label} id {ref} (HERSS simulates in ascending id order)")

    ranges = (
        ("MAX_DISCHARGE", lambda v: v > 0, "must be > 0"),
        ("HEADLOSSCOEF", lambda v: v >= 0, "must be >= 0"),
        ("STATIC_MOTOR_EFFICIENCY", lambda v: 0 < v <= 1, "must be in (0, 1]"),
        ("PUMP_STARTSTOP", lambda v: v >= 0, "must be >= 0"),
        ("PUMP_INLET_MASL", lambda v: -1000 <= v <= 8848, "must be within -1000..8848"),
    )
    for keyword, ok, text in ranges:
        value = _num(node, keyword)
        if value is not None and not ok(value):
            add(ERROR, node, f"{keyword} {text}")


def _check_reservoir(node: Node, add) -> None:
    for keyword in ("HRW", "LRW", "RES_PENALTY"):
        if node.get_token(keyword) is None:
            add(WARNING, node, f"missing {keyword}")
    if node.find("RESERVOIR_CURVE") is None and node.find("BOTTOM_MASL") is None:
        add(WARNING, node, "no RESERVOIR_CURVE or geometry (BOTTOM_MASL) defined")
    hrw, lrw = _num(node, "HRW"), _num(node, "LRW")
    if hrw is not None and lrw is not None and hrw <= lrw:
        add(WARNING, node, f"HRW ({hrw:g}) should be above LRW ({lrw:g})")

    if not any(c.target_id is not None for c in connections_of(node)):
        add(ERROR, node, "no outlet: HERSS needs an OUTLET_TUNNEL, OUTLET_HATCH, OVERFLOW_CURVE, SPILLWAY or OUTLET_AUTO_QMIN")
    if node.find("SPILLWAY") is not None and node.find("OVERFLOW_CURVE") is not None:
        add(ERROR, node, "SPILLWAY and OVERFLOW_CURVE cannot both be used")
    _check_auto_qmin_level(node, add)


def _check_auto_qmin_level(node: Node, add) -> None:
    entry = node.find("OUTLET_AUTO_QMIN")
    if entry is None or len(entry.tokens) < 4 or (to_int(entry.tokens[1]) or -1) < 0:
        return
    level = to_float(entry.tokens[3])
    curve = node.get_rows("RESERVOIR_CURVE")
    lowest = to_float(curve[0][0]) if curve and curve[0] else None
    bottom = _num(node, "BOTTOM_MASL")
    for limit, what in ((lowest, "lowest reservoir curve point"), (bottom, "BOTTOM_MASL")):
        if level is not None and limit is not None and level < limit:
            add(ERROR, node, f"OUTLET_AUTO_QMIN level {level:g} is below the {what} ({limit:g})")


_MONTH_DAYS = (31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31)
_LEADING_INT = re.compile(r"[ \t]*[+-]?\d+")


def _c_atoi(text: str) -> int:
    match = _LEADING_INT.match(text)
    return int(match.group()) if match else 0


def _qmin_date(token: str) -> tuple[int, int] | None:
    # HERSS reads DD.MM with substr(0,2) and substr(3,2); returns (month, day) when valid.
    if len(token) < 3:
        return None
    day, month = _c_atoi(token[0:2]), _c_atoi(token[3:5])
    if not 1 <= month <= 12 or not 1 <= day <= _MONTH_DAYS[month - 1]:
        return None
    return month, day


def _check_periods(node: Node, add) -> None:
    for keyword in ("OUTLET_AUTO_QMIN", "QMIN"):
        entry = node.find(keyword)
        if entry is None or not entry.row_indices:
            continue
        periods = to_int(entry.tokens[1]) or 0
        if keyword == "OUTLET_AUTO_QMIN" and not 1 <= periods <= 5:
            add(ERROR, node, f"OUTLET_AUTO_QMIN needs 1 to 5 periods, found {periods}")
        crossing = 0
        for idx in entry.row_indices:
            row = node.body[idx].split()
            start = _qmin_date(row[0]) if len(row) > 0 else None
            end = _qmin_date(row[1]) if len(row) > 1 else None
            if start is None or end is None:
                add(ERROR, node, f"{keyword}: row {row!r} needs valid zero-padded dates, e.g. 01.04 30.09")
                continue
            crossing += start > end
            discharge = to_float(row[2]) if len(row) > 2 else None
            # HERSS ignores the penalty column for OUTLET_AUTO_QMIN.
            penalty = to_float(row[3]) if keyword == "QMIN" and len(row) > 3 else None
            if discharge is None or discharge < 0 or (penalty is not None and penalty < 0):
                add(ERROR, node, f"{keyword}: row {row!r} needs a non-negative discharge and penalty")
        if crossing > 1:
            add(ERROR, node, f"{keyword}: only one period may cross the year boundary")


def _check_pstation(node: Node, add) -> None:
    declared = to_int(node.get_token("NR_GENERATORS") or "")
    found = len(node.find_all("GENERATOR"))
    if declared is None:
        add(WARNING, node, "missing NR_GENERATORS")
    elif not 0 <= declared <= 6:
        add(ERROR, node, f"NR_GENERATORS {declared} is outside 0..6")
    elif declared != found:
        add(WARNING, node, f"NR_GENERATORS is {declared} but {found} GENERATOR blocks are present")
