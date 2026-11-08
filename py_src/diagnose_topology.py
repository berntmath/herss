#!/home/bernt/py_env/herss_env/bin/python
# -*- coding: utf-8 -*-


# Program to diagnose the topology input file.
#
# The rules mirror the C++ readers in this directory: globalconfig.cpp, herss.cpp,
# riversystem.cpp, reservoir.cpp, powerstation.cpp, channel.cpp, intake.cpp, pump.cpp and
# qmin.cpp. Exit status is 1 when errors are found, so geninput.py can refuse bad input.

from __future__ import annotations

import argparse
import datetime
import math
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path



# Limits from herss.h
MAX_NR_NODES = 250
MAX_NR_GENERATORS = 6
MAX_NR_POINTS_CURVE = 50
MAX_QMIN_PERIODS = 5
N_UNIFORM_EFF_CURVE_POINTS = 11
MAX_TRAVELTIME_HOURS = 240
MOUNT_EVEREST_MASL = 8848.0
VERY_LARGE_NUMBER = 999999999.0

ERROR, WARNING = "ERROR", "WARNING"
NODE_TYPES = ("RESERVOIR", "PSTATION", "CHANNEL", "PUMP", "INTAKE")
PUMP_LINKS = ("PUMP_SOURCE_IDNR", "PUMP_TARGET_IDNR")
INT_RE = re.compile(r"[+-]?\d+")
DDMM_RE = re.compile(r"(\d{2})\.(\d{2})")

# Keywords HERSS reads per node type. Unknown keywords are fatal in CHANNEL, ignored elsewhere.
RESERVOIR_KEYS = {
    "HRW", "LRW", "RES_PENALTY", "FLOODLEVEL_PENALTY", "FILLING_PENALTY", "FAST_OVERFLOW", "SPILLWAY",
    "OUTLET_HATCH", "OUTLET_TUNNEL", "OUTLET_AUTO_QMIN", "RESERVOIR_CURVE", "OVERFLOW_CURVE",
    "RESERVOIR_GEOMETRY", "WIDTH_M", "LENGTH_M", "THETA", "BOTTOM_MASL", "LOCAL_ENERGY_EQUIVALENT",
}
PSTATION_KEYS = {
    "DOWNLINK_IDNR", "STATIC_GENERATOR_EFFICIENCY", "HEADLOSSCOEF", "SHARED_PENSTOCK",
    "POWSTAT_MASL", "POWSTAT_MIN_DISCHARGE", "POWSTAT_STARTSTOP", "LOCAL_ENERGY_EQUIVALENT",
    "AUTO_QMIN", "MAX_ADJUST", "NR_GENERATORS",
}
CHANNEL_KEYS = {
    "ROUTING_DELAY_HOURS", "N_REACHES", "REFERENCE_FLOW", "REFERENCE_TRAVEL_TIME",
    "NONLINEAR_COEFFICIENT", "MANNING_LENGTH_M", "MANNING_BOTTOM_WIDTH_M", "MANNING_SIDE_SLOPE",
    "MANNING_SLOPE", "MANNING_ROUGHNESS_N", "MANNING_INITIAL_FLOW", "K_TRAVELTIME_HOURS",
    "N_CASCADE_LINRES", "QMIN",
}
INTAKE_KEYS = {
    "FLOODWATER_IDNR", "CAPTURE_CAPACITY", "CAPCAP_DOWN_IDNR", "CONST_HATCH_RELEASE",
    "HATCHREL_DOWN_IDNR", "VOLUME",
}
PUMP_KEYS = {
    "PUMP_SOURCE_IDNR", "PUMP_TARGET_IDNR", "PUMP_INLET_MASL", "PUMP_STARTSTOP", "MAX_DISCHARGE",
    "HEADLOSSCOEF", "STATIC_MOTOR_EFFICIENCY", "UNIFORM_NORMALIZED_CURVE", "PUMP_CURVE",
}
MANNING_KEYS = (
    "MANNING_LENGTH_M", "MANNING_BOTTOM_WIDTH_M", "MANNING_SIDE_SLOPE", "MANNING_SLOPE",
    "MANNING_ROUGHNESS_N", "MANNING_INITIAL_FLOW",
)


@dataclass
class Row:
    no: int
    text: str
    tokens: list[str]


@dataclass
class Issue:
    severity: str
    line: int | None
    node: str | None
    message: str


@dataclass
class Link:
    label: str
    target: int
    row: Row


@dataclass
class Node:
    type: str
    id: int
    name: str
    header: Row
    body: list[Row] = field(default_factory=list)
    closed: bool = False
    links: list[Link] = field(default_factory=list)
    generators: int = 0
    has_hatch: bool = False
    channel_method: str | None = None
    channel_states: int | None = None
    hrw: float | None = None
    powstat_masl: float | None = None


@dataclass
class Report:
    path: str
    issues: list[Issue]
    nodes: list[Node]

    @property
    def errors(self) -> list[Issue]:
        return [i for i in self.issues if i.severity == ERROR]

    @property
    def warnings(self) -> list[Issue]:
        return [i for i in self.issues if i.severity == WARNING]

    def inflow_columns(self) -> list[str]:
        return [str(n.id) for n in self.nodes if n.type in ("RESERVOIR", "INTAKE")]

    def action_columns(self) -> list[str]:
        cols: list[str] = []
        for n in self.nodes:
            if n.type == "PSTATION":
                cols += [f"{n.id}_{g}" for g in range(n.generators)]
            elif n.type == "PUMP" or (n.type == "RESERVOIR" and n.has_hatch):
                cols.append(str(n.id))
        return cols


class Diagnoser:
    def __init__(self) -> None:
        self.issues: list[Issue] = []
        self.node: Node | None = None

    # ---- reporting and value parsing -------------------------------------------------
    def report(self, severity: str, row: Row | None, message: str) -> None:
        label = f"{self.node.id} {self.node.name}" if self.node else None
        self.issues.append(Issue(severity, row.no if row else None, label, message))

    def error(self, row: Row | None, message: str) -> None:
        self.report(ERROR, row, message)

    def warn(self, row: Row | None, message: str) -> None:
        self.report(WARNING, row, message)

    def num(self, row, token, what, lo=None, hi=None, integer=False):
        """Parse a token the way HERSS would, but reject garbage that atof/atoi turn into 0."""
        value: float | int | None
        if integer:
            value = int(token) if INT_RE.fullmatch(token) else None
        else:
            try:
                value = float(token)
            except ValueError:
                value = None
            if value is not None and not math.isfinite(value):
                value = None
        if value is None:
            self.error(row, f"{what}: '{token}' is not {'an integer' if integer else 'a number'}")
            return None
        if (lo is not None and value < lo) or (hi is not None and value > hi):
            rng = f"{'-inf' if lo is None else lo} .. {'inf' if hi is None else hi}"
            self.error(row, f"{what} = {token} is outside the allowed range ({rng})")
            return None
        return value

    def field_num(self, fields, key, lo=None, hi=None, integer=False, required=True, severity=ERROR, note=""):
        row = fields.get(key)
        if row is None:
            if required:
                self.report(severity, self.node.header, f"{key} is missing{note}")
            return None
        if len(row.tokens) < 2:
            self.error(row, f"{key} has no value")
            return None
        return self.num(row, row.tokens[1], key, lo, hi, integer)

    def positive(self, fields, key, upper=None):
        value = self.field_num(fields, key, 0, upper)
        if value is not None and value <= 0:
            self.error(fields[key], f"{key} must be greater than 0")
            return None
        return value

    def true_false(self, fields, key) -> bool | None:
        row = fields.get(key)
        if row is None:
            return None
        value = row.tokens[1] if len(row.tokens) > 1 else ""
        if value not in ("TRUE", "FALSE"):
            self.error(row, f"{key} must be TRUE or FALSE, got '{value}'")
            return None
        return value == "TRUE"

    # ---- structure ------------------------------------------------------------------
    def read_rows(self, path: Path) -> list[Row]:
        # Same filtering as TopologyParser::loadFile: trim, drop empty lines and '#' lines.
        rows = []
        text = path.read_text(encoding="utf-8")
        for no, raw in enumerate(text.splitlines(), start=1):
            line = raw.strip(" \t\r\n")
            if line and not line.startswith("#"):
                rows.append(Row(no, line, line.split()))
        return rows

    def header(self, row: Row) -> Node:
        t = row.tokens
        node = Node(t[1] if len(t) > 1 else "?", -1, t[3] if len(t) > 3 else "?", row)
        self.node = None
        if len(t) < 4:
            self.error(row, "expected 'NODE <TYPE> <ID> <NAME>'")
        if node.type not in NODE_TYPES:
            self.error(row, f"node type '{node.type}' must be one of {', '.join(NODE_TYPES)}")
        if len(t) > 2:
            node_id = self.num(row, t[2], "node ID", 0, integer=True)
            node.id = -1 if node_id is None else int(node_id)
        return node

    def split_nodes(self, rows: list[Row]) -> list[Node]:
        nodes: list[Node] = []
        cur: Node | None = None
        for row in rows:
            kw = row.tokens[0]
            if kw == "NODE":
                if cur and not cur.closed:
                    self.node = cur
                    self.error(cur.header, f"missing ENDNODE before the next NODE (line {row.no})")
                cur = self.header(row)
                nodes.append(cur)
            elif kw == "ENDNODE":
                if cur is None or cur.closed:
                    self.node = None
                    self.warn(row, "ENDNODE without an open NODE")
                else:
                    cur.closed = True
            elif cur is None or cur.closed:
                self.node = None
                self.warn(row, f"'{kw}' is outside any NODE block and is ignored by HERSS")
            else:
                cur.body.append(row)
        if cur and not cur.closed:
            self.node = cur
            self.error(cur.header, "missing ENDNODE at the end of the file")
        return nodes

    def check_ids(self, nodes: list[Node]) -> None:
        self.node = None
        if len(nodes) > MAX_NR_NODES:
            self.error(None, f"{len(nodes)} nodes exceeds MAX_NR_NODES ({MAX_NR_NODES}) in herss.h")
        seen: dict[int, Node] = {}
        for n in nodes:
            if n.id < 0:
                continue
            if n.id in seen:
                self.error(n.header, f"duplicate node ID {n.id} (first used on line {seen[n.id].header.no})")
            seen.setdefault(n.id, n)
        if len(seen) == len(nodes) and all(n.id >= 0 for n in nodes):
            for pos, n in enumerate(nodes):
                if n.id != pos:
                    self.error(
                        n.header,
                        f"node ID {n.id} is NODE number {pos} in the file; IDs must be 0..{len(nodes) - 1} "
                        "in file order (globalconfig.cpp and riversystem.cpp index node types by the "
                        "position of the NODE line, but look nodes up by ID)",
                    )
        names: dict[str, int] = {}
        for n in nodes:
            if n.name in names:
                self.warn(n.header, f"node name '{n.name}' is also used on line {names[n.name]}; "
                          "state-file rows are matched on ID and name")
            names.setdefault(n.name, n.header.no)

    # ---- body scanning --------------------------------------------------------------
    def scan(self, node, known, blocks, special=None):
        """Split a node body into keyword rows and the data rows that follow count keywords.

        blocks maps a keyword to True when a negative count means 'not used'.
        """
        special = special or {}
        fields: dict[str, Row] = {}
        data: dict[str, list[Row]] = {}
        body = node.body
        i = 0
        while i < len(body):
            row = body[i]
            kw = row.tokens[0]
            i += 1
            if node.type == "CHANNEL" and kw in ("TRAVELTIME", "DECAY"):
                self.error(row, f"{kw} is no longer supported; use K_TRAVELTIME_HOURS and N_CASCADE_LINRES")
                continue
            if kw not in known:
                if node.type == "CHANNEL":
                    self.error(row, f"unknown keyword '{kw}'; HERSS stops on it in a CHANNEL")
                else:
                    self.warn(row, f"unknown keyword '{kw}' is ignored by HERSS")
                continue
            if kw in fields:
                self.warn(row, f"{kw} is given more than once; HERSS uses the last one")
            fields[kw] = row
            if kw in special:
                i = special[kw](node, row, i)
            elif kw in blocks:
                count = self.num(row, row.tokens[1] if len(row.tokens) > 1 else "", f"{kw} point count", integer=True)
                if count is None:
                    continue
                if count < 0 and not blocks[kw]:
                    self.error(row, f"{kw} point count must not be negative")
                    continue
                count = max(count, 0)
                rows = body[i:i + count]
                if len(rows) < count:
                    self.error(row, f"{kw} announces {count} data rows but only {len(rows)} follow before ENDNODE")
                data[kw] = rows
                i += len(rows)
        return fields, data

    def curve(self, fields, data, key, strict_y=False, max_pts=None):
        """Read 'KEY n' followed by n rows of 'x y'. Returns [(x, y)] or [] when unusable."""
        head = fields.get(key)
        rows = data.get(key)
        if head is None or rows is None:
            return []
        declared = int(head.tokens[1])
        if declared < 2:
            self.error(head, f"{key} needs at least 2 points")
            return []
        if max_pts is not None and declared > max_pts:
            self.error(head, f"{key} has {declared} points; the maximum is {max_pts}")
            return []
        pts = []
        for r in rows:
            xy = [self.num(r, tok, f"{key} value") for tok in r.tokens[:2]]
            if len(xy) < 2 or None in xy:
                if len(xy) < 2:
                    self.error(r, f"{key} row needs two values, got '{r.text}'")
                continue
            pts.append((xy[0], xy[1]))
        if len(pts) != declared:
            return []
        xs, ys = [p[0] for p in pts], [p[1] for p in pts]
        if any(b <= a for a, b in zip(xs, xs[1:])):
            self.error(head, f"{key}: the first column must be strictly increasing")
            return []
        if strict_y and any(b <= a for a, b in zip(ys, ys[1:])):
            self.error(head, f"{key}: the second column must be strictly increasing "
                       "(HERSS inverts this curve)")
            return []
        if xs[-1] - xs[0] < 1e-4 or max(ys) - min(ys) < 1e-4:
            self.error(head, f"{key}: x or y range is smaller than 0.0001 (ArrayCurve refuses it)")
            return []
        return pts

    def level_in_range(self, row, what, value, low, high):
        if value is None or (low is None and high is None):
            return
        if (low is not None and value < low) or (high is not None and value > high):
            span = f"{low} .. {high}" if high is not None else f">= {low}"
            self.error(row, f"{what} = {value} masl is outside the reservoir storage range ({span}); "
                       "HERSS cannot look up a volume there")

    def check_qmin(self, rows: list[Row], with_cost: bool) -> None:
        """Seasonal periods 'DD.MM DD.MM m3/s [cost]' as validated by Qmin::validatePeriods."""
        spans = []
        for r in rows:
            t = r.tokens
            need = 4 if with_cost else 3
            if len(t) < need:
                self.error(r, "expected 'DD.MM DD.MM discharge_m3s" + (" cost'" if with_cost else "'"))
                continue
            days = [self.ddmm(r, t[0]), self.ddmm(r, t[1])]
            self.num(r, t[2], "period discharge", 0)
            if with_cost:
                self.num(r, t[3], "period penalty cost", 0)
            if None not in days:
                spans.append((days[0], days[1]))
        if len(spans) != len(rows):
            return
        crossing = [s for s in spans if s[0] > s[1]]
        if len(crossing) > 1:
            self.error(rows[0], "only one period may cross the year boundary (e.g. 01.10 31.03)")
        cover = [0] * 367
        for start, end in spans:
            for d in range(1, 367):
                if (start <= d <= end) if start <= end else (d >= start or d <= end):
                    cover[d] += 1
        gaps = [d for d in range(1, 367) if cover[d] == 0]
        if gaps:
            self.warn(rows[0], f"periods leave {len(gaps)} day(s) of the year without a requirement "
                      f"(first uncovered day: {self.doy_text(gaps[0])})")
        overlap = [d for d in range(1, 367) if cover[d] > 1]
        if overlap:
            self.warn(rows[0], f"periods overlap (first overlapping day: {self.doy_text(overlap[0])}); "
                      "HERSS uses the first matching period")

    @staticmethod
    def doy_text(doy: int) -> str:
        d = datetime.date(2000, 1, 1) + datetime.timedelta(days=doy - 1)
        return f"{d.day:02d}.{d.month:02d}"

    def ddmm(self, row, token) -> int | None:
        m = DDMM_RE.fullmatch(token)
        if not m:
            self.error(row, f"date '{token}' must be zero-padded DD.MM, e.g. 01.04")
            return None
        try:
            return datetime.date(2000, int(m.group(2)), int(m.group(1))).timetuple().tm_yday
        except ValueError:
            self.error(row, f"date '{token}' does not exist (year 2000 calendar)")
            return None

    # ---- node types -----------------------------------------------------------------
    def check_reservoir(self, node: Node) -> None:
        f, data = self.scan(node, RESERVOIR_KEYS,
                            {"RESERVOIR_CURVE": False, "OVERFLOW_CURVE": False, "OUTLET_AUTO_QMIN": True})
        hrw = self.field_num(f, "HRW", 0, MOUNT_EVEREST_MASL)
        lrw = self.field_num(f, "LRW", 0, MOUNT_EVEREST_MASL)
        if hrw is not None and lrw is not None and lrw >= hrw:
            self.error(f["LRW"], f"LRW ({lrw}) must be lower than HRW ({hrw})")
        node.hrw = hrw
        self.field_num(f, "RES_PENALTY", 0, VERY_LARGE_NUMBER)
        fill_row = f.get("FILLING_PENALTY")
        if fill_row:
            if len(fill_row.tokens) != 3:
                self.error(fill_row, "FILLING_PENALTY needs 2 values: <f0> <P>")
            else:
                f0 = self.num(fill_row, fill_row.tokens[1], "FILLING_PENALTY f0", 0, 1)
                if f0 is not None and f0 <= 0:
                    self.error(fill_row, "FILLING_PENALTY f0 must be larger than 0 and at most 1")
                self.num(fill_row, fill_row.tokens[2], "FILLING_PENALTY P", 0, VERY_LARGE_NUMBER)
        fast = self.true_false(f, "FAST_OVERFLOW")

        has_curve, has_geom = "RESERVOIR_CURVE" in f, "BOTTOM_MASL" in f
        low = high = None
        if has_curve and has_geom:
            self.error(f["BOTTOM_MASL"], "RESERVOIR_CURVE and the geometry keywords cannot be combined")
        elif not has_curve and not has_geom:
            self.error(node.header, "no storage description: add RESERVOIR_CURVE or WIDTH_M, "
                       "LENGTH_M, THETA and BOTTOM_MASL")
        elif has_curve:
            pts = self.curve(f, data, "RESERVOIR_CURVE", strict_y=True, max_pts=MAX_NR_POINTS_CURVE)
            if pts:
                low, high = pts[0][0], pts[-1][0]
        else:
            self.positive(f, "WIDTH_M")
            self.positive(f, "LENGTH_M")
            theta = self.field_num(f, "THETA", 0, 90)
            if theta is not None and not 0 < theta < 90:
                self.error(f["THETA"], "THETA must be between 0 and 90 degrees (exclusive)")
            low = self.field_num(f, "BOTTOM_MASL", 0, MOUNT_EVEREST_MASL)
        self.level_in_range(f.get("HRW"), "HRW", hrw, low, high)
        self.level_in_range(f.get("LRW"), "LRW", lrw, low, high)
        if high is not None and has_curve and hrw is not None and high <= hrw:
            self.error(f["RESERVOIR_CURVE"], f"RESERVOIR_CURVE ends at {high} masl, not above HRW ({hrw}); "
                       "any water above HRW stops HERSS with 'add more volume at the top of your reservoir curve'")

        outlets = 0
        row = f.get("OUTLET_HATCH")
        if row:
            target = self.field_num(f, "OUTLET_HATCH", integer=True)
            if target is not None and target >= 0:
                outlets += 1
                node.has_hatch = True
                node.links.append(Link("OUTLET_HATCH", target, row))
                if len(row.tokens) != 5:
                    self.error(row, "OUTLET_HATCH needs 5 columns: <down_id> <qmin> <qmax> <hatch_masl>")
                else:
                    qmin = self.num(row, row.tokens[2], "hatch qmin", 0, VERY_LARGE_NUMBER)
                    qmax = self.num(row, row.tokens[3], "hatch qmax", 0, VERY_LARGE_NUMBER)
                    masl = self.num(row, row.tokens[4], "hatch_masl", 0, MOUNT_EVEREST_MASL)
                    if qmin is not None and qmax is not None and qmin > qmax:
                        self.warn(row, f"hatch qmin ({qmin}) is larger than qmax ({qmax})")
                    self.level_in_range(row, "hatch_masl", masl, low, high)

        row = f.get("OUTLET_TUNNEL")
        if row:
            target = self.field_num(f, "OUTLET_TUNNEL", integer=True)
            if target is not None and target >= 0:
                outlets += 1
                node.links.append(Link("OUTLET_TUNNEL", target, row))

        row = f.get("OUTLET_AUTO_QMIN")
        if row:
            periods = self.field_num(f, "OUTLET_AUTO_QMIN", integer=True)
            if periods is not None and periods >= 0:
                outlets += 1
                if len(row.tokens) < 4:
                    self.error(row, "OUTLET_AUTO_QMIN needs <nr_periods> <down_id> <outlet_masl>")
                else:
                    target = self.num(row, row.tokens[2], "OUTLET_AUTO_QMIN downstream ID", 0, integer=True)
                    if target is not None:
                        node.links.append(Link("OUTLET_AUTO_QMIN", target, row))
                    masl = self.num(row, row.tokens[3], "outlet_masl", 0, MOUNT_EVEREST_MASL)
                    if masl is not None and low is not None and masl < low:
                        self.error(row, f"outlet_masl ({masl}) is below the lowest storage level ({low})")
                if not 1 <= periods <= MAX_QMIN_PERIODS:
                    self.error(row, f"OUTLET_AUTO_QMIN period count must be 1..{MAX_QMIN_PERIODS}")
                else:
                    self.check_qmin(data.get("OUTLET_AUTO_QMIN", []), with_cost=False)

        spill, over = f.get("SPILLWAY"), f.get("OVERFLOW_CURVE")
        if spill and over:
            self.error(spill, "SPILLWAY and OVERFLOW_CURVE exclude each other")
        if spill:
            outlets += 1
            if len(spill.tokens) != 5:
                self.error(spill, "SPILLWAY needs <down_id> <C> <L> <spillway_level_masl>")
            else:
                target = self.num(spill, spill.tokens[1], "SPILLWAY downstream ID", 0, integer=True)
                if target is not None:
                    node.links.append(Link("SPILLWAY", target, spill))
                self.num(spill, spill.tokens[2], "spillway C", 0, 100)
                self.num(spill, spill.tokens[3], "spillway L", 0, VERY_LARGE_NUMBER)
                level = self.num(spill, spill.tokens[4], "spillway level", 0, MOUNT_EVEREST_MASL)
                self.level_in_range(spill, "spillway level", level, low, high)
            if has_curve and fast is not True:
                self.error(spill, "SPILLWAY cannot be combined with RESERVOIR_CURVE: HERSS computes the spillway "
                           "volume from the geometry keywords. Use RESERVOIR_GEOMETRY or OVERFLOW_CURVE")
        if over:
            outlets += 1
            if len(over.tokens) < 3:
                self.error(over, "OVERFLOW_CURVE needs <nr_points> <down_id>")
            else:
                target = self.num(over, over.tokens[2], "OVERFLOW_CURVE downstream ID", 0, integer=True)
                if target is not None:
                    node.links.append(Link("OVERFLOW_CURVE", target, over))
            self.curve(f, data, "OVERFLOW_CURVE", strict_y=True, max_pts=MAX_NR_POINTS_CURVE)
        if not spill and not over and fast is not True:
            self.error(node.header, "no overflow method: add OVERFLOW_CURVE, SPILLWAY or FAST_OVERFLOW TRUE")
        if outlets == 0:
            self.error(node.header, "no outlet: set at least one of OUTLET_HATCH, OUTLET_TUNNEL, "
                       "OUTLET_AUTO_QMIN, OVERFLOW_CURVE or SPILLWAY")

    def generators(self, node: Node, row: Row, i: int) -> int:
        """Parse the positional generator blocks that follow NR_GENERATORS."""
        body = node.body
        n = self.num(row, row.tokens[1] if len(row.tokens) > 1 else "", "NR_GENERATORS",
                     1, MAX_NR_GENERATORS, integer=True)
        if n is None:
            return i
        node.generators = int(n)
        for g in range(int(n)):
            def take():
                nonlocal i
                if i >= len(body):
                    return None
                i += 1
                return body[i - 1]

            r = take()
            if r is None or r.tokens[0] != "GENERATOR" or len(r.tokens) < 2 or r.tokens[1] != str(g):
                self.error(r or row, f"expected 'GENERATOR {g}' directly after "
                           f"{'NR_GENERATORS' if g == 0 else 'the previous generator'}")
                return i
            h = take()
            if h is None or h.tokens[0] not in ("TURBINE_CURVE", "UNIFORM_NORMALIZED_CURVE"):
                self.error(h or r, f"expected TURBINE_CURVE or UNIFORM_NORMALIZED_CURVE for generator {g}")
                return i
            count = self.num(h, h.tokens[1] if len(h.tokens) > 1 else "", f"{h.tokens[0]} point count", 0, integer=True)
            if count is None:
                return i
            uniform = h.tokens[0] == "UNIFORM_NORMALIZED_CURVE"
            if uniform and count != N_UNIFORM_EFF_CURVE_POINTS:
                self.error(h, f"UNIFORM_NORMALIZED_CURVE needs exactly {N_UNIFORM_EFF_CURVE_POINTS} points")
                return i
            rows = body[i:i + count]
            i += len(rows)
            if len(rows) < count:
                self.error(h, f"{h.tokens[0]} announces {count} rows but only {len(rows)} follow")
                return i
            qs, effs = self.efficiency_rows(h.tokens[0], rows, uniform)
            m = take()
            if m is None or m.tokens[0] != "GENERATOR_MAX_DISCHARGE":
                self.error(m or h, f"expected GENERATOR_MAX_DISCHARGE directly after the curve of generator {g}")
                return i
            qmax = self.num(m, m.tokens[1] if len(m.tokens) > 1 else "", "GENERATOR_MAX_DISCHARGE", 0)
            if qmax is not None and not uniform and qs and qmax > max(qs) + 1e-9:
                self.error(m, f"GENERATOR_MAX_DISCHARGE ({qmax}) is above the largest flow on the turbine curve "
                           f"({max(qs)}); HERSS cannot look up efficiency there")
            if not uniform and qs and effs:
                self.curve_shape(h, rows, qs, effs)
        return i

    def efficiency_rows(self, what, rows, uniform):
        qs, effs = [], []
        for r in rows:
            if len(r.tokens) < 2:
                self.error(r, f"{what} row needs two values, got '{r.text}'")
                continue
            q = self.num(r, r.tokens[0], f"{what} flow", 0)
            e = self.num(r, r.tokens[1], f"{what} efficiency", 0, 100)
            if q is not None and e is not None:
                qs.append(q)
                effs.append(e)
        return qs, effs

    def curve_shape(self, head, rows, qs, effs):
        if len(qs) != len(rows):
            return
        if any(b <= a for a, b in zip(qs, qs[1:])):
            self.error(head, f"{head.tokens[0]}: flow column must be strictly increasing")
        elif qs[-1] - qs[0] < 1e-4 or max(effs) - min(effs) < 1e-4:
            self.error(head, f"{head.tokens[0]}: flow or efficiency range is smaller than 0.0001")
        elif qs[0] > 0:
            self.warn(head, f"{head.tokens[0]} starts at flow {qs[0]}; start it at 0.0 so small flows can be looked up")

    def check_pstation(self, node: Node) -> None:
        f, _ = self.scan(node, PSTATION_KEYS, {}, {"NR_GENERATORS": self.generators})
        row = f.get("DOWNLINK_IDNR")
        target = self.field_num(f, "DOWNLINK_IDNR", integer=True)
        if row and target is not None and target >= 0:
            node.links.append(Link("DOWNLINK_IDNR", target, row))
        if "NR_GENERATORS" not in f:
            self.error(node.header, "NR_GENERATORS is missing")
        self.field_num(f, "HEADLOSSCOEF", 0, VERY_LARGE_NUMBER)
        node.powstat_masl = self.field_num(f, "POWSTAT_MASL", -1000, MOUNT_EVEREST_MASL)
        self.field_num(f, "LOCAL_ENERGY_EQUIVALENT", 0, VERY_LARGE_NUMBER)
        note = "; HERSS starts it at -1e7"
        eff = self.field_num(f, "STATIC_GENERATOR_EFFICIENCY", severity=WARNING, note=note)
        if eff is not None and not 0 < eff <= 1:
            self.error(f["STATIC_GENERATOR_EFFICIENCY"], "STATIC_GENERATOR_EFFICIENCY must be in (0, 1]")
        self.field_num(f, "POWSTAT_MIN_DISCHARGE", 0, severity=WARNING, note=note)
        self.field_num(f, "POWSTAT_STARTSTOP", 0, severity=WARNING, note=note)
        self.true_false(f, "SHARED_PENSTOCK")
        adjust = self.field_num(f, "MAX_ADJUST", integer=True, required=False)
        if adjust is not None and adjust > -1:
            self.error(f["MAX_ADJUST"], "MAX_ADJUST is work in progress and stops HERSS; use -9999")

    def check_channel(self, node: Node) -> None:
        t = node.header.tokens
        if len(t) < 5:
            self.error(node.header, "CHANNEL needs a downstream ID: 'NODE CHANNEL <ID> <NAME> <DOWN_ID>' "
                       "(-9999 for none); HERSS reads a missing one as node 0")
        else:
            down = self.num(node.header, t[4], "CHANNEL downstream ID", integer=True)
            if down is not None and down >= 0:
                node.links.append(Link("downstream", int(down), node.header))
        f, data = self.scan(node, CHANNEL_KEYS, {"QMIN": True})

        cascade = "K_TRAVELTIME_HOURS" in f and "N_CASCADE_LINRES" in f
        nonlinear = all(k in f for k in ("NONLINEAR_COEFFICIENT", "REFERENCE_FLOW", "REFERENCE_TRAVEL_TIME", "N_REACHES"))
        manning = all(k in f for k in MANNING_KEYS) and "N_REACHES" in f
        if cascade:
            node.channel_method = "B_LAGGED_CASCADEDLINRES" if "ROUTING_DELAY_HOURS" in f else "A_CASCADEDLINRES"
            self.field_num(f, "K_TRAVELTIME_HOURS", 0, MAX_TRAVELTIME_HOURS)
            states = self.field_num(f, "N_CASCADE_LINRES", 0, integer=True)
            if states == 0:
                self.warn(f["N_CASCADE_LINRES"], "N_CASCADE_LINRES is 0; a channel needs at least one linear reservoir")
            node.channel_states = None if states is None else int(states)
            if node.channel_method.startswith("B"):
                self.field_num(f, "ROUTING_DELAY_HOURS", 0, integer=True)
        elif nonlinear:
            node.channel_method = "C_NONLINEARRESCASCADE"
            for key in ("NONLINEAR_COEFFICIENT", "REFERENCE_FLOW", "REFERENCE_TRAVEL_TIME"):
                value = self.field_num(f, key)
                if key == "NONLINEAR_COEFFICIENT" and value is not None and value < 1:
                    self.error(f[key], "NONLINEAR_COEFFICIENT must be >= 1 (1 is a linear reservoir)")
            reaches = self.field_num(f, "N_REACHES", 1, integer=True)
            node.channel_states = None if reaches is None else int(reaches)
        elif manning:
            node.channel_method = "D_MANNING"
            for key in ("MANNING_LENGTH_M", "MANNING_BOTTOM_WIDTH_M", "MANNING_SLOPE", "MANNING_ROUGHNESS_N"):
                self.positive(f, key)
            self.field_num(f, "MANNING_SIDE_SLOPE", 0)
            self.field_num(f, "MANNING_INITIAL_FLOW")
            reaches = self.field_num(f, "N_REACHES", 1, integer=True)
            node.channel_states = None if reaches is None else int(reaches)
        else:
            self.error(node.header, "cannot determine the routing method; give K_TRAVELTIME_HOURS and "
                       "N_CASCADE_LINRES (A/B), or NONLINEAR_COEFFICIENT, REFERENCE_FLOW, "
                       "REFERENCE_TRAVEL_TIME and N_REACHES (C), or all MANNING_* keys and N_REACHES (D)")

        row = f.get("QMIN")
        if row:
            periods = self.field_num(f, "QMIN", integer=True)
            if periods is not None and periods > 0:
                if periods > MAX_QMIN_PERIODS:
                    self.error(row, f"QMIN period count must be 1..{MAX_QMIN_PERIODS}")
                else:
                    self.check_qmin(data.get("QMIN", []), with_cost=True)

    def check_intake(self, node: Node) -> None:
        f, _ = self.scan(node, INTAKE_KEYS, {})
        for key in ("FLOODWATER_IDNR", "CAPCAP_DOWN_IDNR", "HATCHREL_DOWN_IDNR"):
            target = self.field_num(f, key, 0, integer=True, note="; HERSS needs a valid node ID")
            if target is not None:
                node.links.append(Link(key, int(target), f[key]))
        for key in ("CAPTURE_CAPACITY", "CONST_HATCH_RELEASE", "VOLUME"):
            self.field_num(f, key, 0)

    def check_pump(self, node: Node) -> None:
        f, data = self.scan(node, PUMP_KEYS, {"UNIFORM_NORMALIZED_CURVE": False, "PUMP_CURVE": False})
        for key in PUMP_LINKS:
            target = self.field_num(f, key, 0, integer=True)
            if target is not None:
                node.links.append(Link(key, int(target), f[key]))
        self.positive(f, "MAX_DISCHARGE", VERY_LARGE_NUMBER)
        self.field_num(f, "HEADLOSSCOEF", 0, VERY_LARGE_NUMBER)
        eff = self.field_num(f, "STATIC_MOTOR_EFFICIENCY")
        if eff is not None and not 0 < eff <= 1:
            self.error(f["STATIC_MOTOR_EFFICIENCY"], "STATIC_MOTOR_EFFICIENCY must be in (0, 1]")
        self.field_num(f, "PUMP_INLET_MASL", -1000, MOUNT_EVEREST_MASL)
        self.field_num(f, "PUMP_STARTSTOP", 0, VERY_LARGE_NUMBER)
        if "UNIFORM_NORMALIZED_CURVE" not in f and "PUMP_CURVE" not in f:
            self.error(node.header, "no efficiency curve: add UNIFORM_NORMALIZED_CURVE or PUMP_CURVE")
        if "UNIFORM_NORMALIZED_CURVE" in data:
            rows = data["UNIFORM_NORMALIZED_CURVE"]
            if len(rows) != N_UNIFORM_EFF_CURVE_POINTS:
                self.error(f["UNIFORM_NORMALIZED_CURVE"],
                           f"UNIFORM_NORMALIZED_CURVE needs exactly {N_UNIFORM_EFF_CURVE_POINTS} points")
            else:
                self.efficiency_rows("UNIFORM_NORMALIZED_CURVE", rows, True)
        if "PUMP_CURVE" in data:
            rows = data["PUMP_CURVE"]
            qs, effs = self.efficiency_rows("PUMP_CURVE", rows, False)
            self.curve_shape(f["PUMP_CURVE"], rows, qs, effs)

    # ---- connections between nodes --------------------------------------------------
    def check_links(self, nodes: list[Node]) -> None:
        by_id = {n.id: n for n in nodes if n.id >= 0}
        incoming: set[int] = set()
        outgoing: set[int] = set()
        for node in nodes:
            self.node = node
            for link in node.links:
                target = by_id.get(link.target)
                if target is None:
                    self.error(link.row, f"{link.label} points to node {link.target}, which does not exist")
                    continue
                if link.target == node.id:
                    self.error(link.row, f"{link.label} points to the node itself")
                    continue
                if (link.label == "OUTLET_TUNNEL" and target.type == "PSTATION" and node.hrw is not None
                        and target.powstat_masl is not None and target.powstat_masl >= node.hrw):
                    self.error(link.row, f"powerstation {target.id} {target.name} is at POWSTAT_MASL "
                               f"{target.powstat_masl}, not below HRW ({node.hrw}) of this reservoir; "
                               "the head is never positive")
                if link.label in PUMP_LINKS:
                    if target.type != "RESERVOIR":
                        self.error(link.row, f"{link.label} must be a RESERVOIR, node {target.id} is a {target.type}")
                    if link.target < node.id:
                        self.error(link.row, f"{link.label} ({link.target}) must be higher than the pump's own ID "
                                   f"({node.id}); nodes are simulated in ascending ID order")
                    continue
                outgoing.add(node.id)
                if target.type == "PUMP":
                    self.error(link.row, f"{link.label} points to PUMP node {target.id}, which cannot receive water")
                    continue
                incoming.add(target.id)
                if link.target < node.id:
                    self.warn(link.row, f"{link.label} sends water to node {link.target}, which has a lower ID; "
                              "nodes are simulated in ascending ID order, so the water arrives a step late or not at all")
        last = max(by_id) if by_id else None
        for node in nodes:
            self.node = node
            if node.type in ("PSTATION", "CHANNEL") and node.id not in incoming:
                self.warn(node.header, "no node sends water here, so it will never carry any")
            if node.type == "PUMP" or node.id < 0:
                continue
            if node.id != last and node.id not in outgoing:
                self.warn(node.header, f"no downstream node; the water leaves the system here but HERSS only "
                          f"counts outflow from the last node ({last}) in its water balance")
            if node.id == last and node.id in outgoing:
                self.warn(node.header, "the last node has a downstream link; the water balance "
                          "counts the outflow of this node as leaving the system")
        self.node = None

    # ---- driver ---------------------------------------------------------------------
    def run(self, path: Path) -> Report:
        rows = self.read_rows(path)
        nodes = self.split_nodes(rows)
        if not nodes:
            self.error(None, "the file contains no NODE blocks")
        self.check_ids(nodes)
        checks = {
            "RESERVOIR": self.check_reservoir,
            "PSTATION": self.check_pstation,
            "CHANNEL": self.check_channel,
            "INTAKE": self.check_intake,
            "PUMP": self.check_pump,
        }
        for node in nodes:
            self.node = node
            if node.type in checks:
                checks[node.type](node)
        self.check_links(nodes)
        self.issues.sort(key=lambda i: (i.line if i.line is not None else 0, i.severity != ERROR))
        return Report(str(path), self.issues, nodes)


def diagnose(path: str | Path) -> Report:
    return Diagnoser().run(Path(path))


def print_report(report: Report, layout: bool = True) -> None:
    counts = {t: sum(1 for n in report.nodes if n.type == t) for t in NODE_TYPES}
    summary = ", ".join(f"{c} {t}" for t, c in counts.items() if c)
    print(f"{report.path}: {len(report.nodes)} nodes ({summary})")
    for i in report.issues:
        where = f"line {i.line}" if i.line is not None else "file"
        node = f" [node {i.node}]" if i.node else ""
        print(f"{i.severity:<7} {where}{node}: {i.message}")
    if layout and not report.errors:
        print("\nInput files this topology needs:")
        print(f"  inflow file   Date_NodeID  {' '.join(report.inflow_columns())}")
        print(f"  actions file  Date_NodeID  {' '.join(report.action_columns())}")
        for n in report.nodes:
            if n.type == "CHANNEL":
                print(f"  channel {n.id} {n.name}: {n.channel_method}, {n.channel_states} stored values in the state file")
    print(f"\n{len(report.errors)} error(s), {len(report.warnings)} warning(s)")


def main(argv: list[str] | None = None) -> int:
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser(description="Diagnose a HERSS topology file.")
    parser.add_argument("topology", help="path to the topology file to diagnose")
    parser.add_argument("--no-layout", action="store_true", help="do not print the derived input file layout")
    args = parser.parse_args(argv)
    try:
        report = diagnose(args.topology)
    except (OSError, UnicodeDecodeError) as exc:
        print(f"ERROR   cannot read {args.topology}: {exc}", file=sys.stderr)
        return 1
    print_report(report, layout=not args.no_layout)
    return 1 if report.errors else 0


if __name__ == "__main__":
    sys.exit(main())




