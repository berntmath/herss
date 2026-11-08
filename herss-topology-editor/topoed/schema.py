"""Node-type knowledge for the editor, taken from the HERSS parsers in ../src."""

from __future__ import annotations

from dataclasses import dataclass

from .model import Node

RESERVOIR = "RESERVOIR"
PSTATION = "PSTATION"
CHANNEL = "CHANNEL"
PUMP = "PUMP"
INTAKE = "INTAKE"

NODE_TYPES = (RESERVOIR, PSTATION, CHANNEL, PUMP, INTAKE)
MAX_NR_NODES = 250
UNUSED = "-9999"


@dataclass(frozen=True)
class FieldSpec:
    keyword: str
    label: str
    kind: str = "float"  # float | int | bool
    index: int = 1
    show_always: bool = False
    group: str = ""


@dataclass(frozen=True)
class ConnSpec:
    kind: str
    label: str
    keyword: str | None  # None: stored in the NODE line after the name
    index: int  # token index in the keyword line, or position in header_extra
    clear: str = "none"  # token | line | remove | none: how the link is marked unused
    creatable: bool = False
    gate_index: int | None = None  # token that must be >= 0 for the link to be active

    @property
    def negative_is_error(self) -> bool:
        # HERSS rejects a negative id for these links; the others use it for "unused".
        return self.clear in ("none", "remove") or self.gate_index is not None


# Single-token editable fields. Fields with index > 1 belong to a multi-column line.
FIELDS: dict[str, tuple[FieldSpec, ...]] = {
    RESERVOIR: (
        FieldSpec("HRW", "HRW [masl]", show_always=True),
        FieldSpec("LRW", "LRW [masl]", show_always=True),
        FieldSpec("RES_PENALTY", "Reservoir penalty", show_always=True),
        FieldSpec("FLOODLEVEL_PENALTY", "Flood level penalty"),
        FieldSpec("FAST_OVERFLOW", "Fast overflow", "bool"),
        FieldSpec("WIDTH_M", "Width [m]", group="Geometry"),
        FieldSpec("LENGTH_M", "Length [m]", group="Geometry"),
        FieldSpec("THETA", "Theta [deg]", group="Geometry"),
        FieldSpec("BOTTOM_MASL", "Bottom [masl]", group="Geometry"),
        FieldSpec("OUTLET_HATCH", "Hatch min Q [m3/s]", index=2, group="Hatch"),
        FieldSpec("OUTLET_HATCH", "Hatch max Q [m3/s]", index=3, group="Hatch"),
        FieldSpec("OUTLET_HATCH", "Hatch level [masl]", index=4, group="Hatch"),
        FieldSpec("SPILLWAY", "Spillway C", index=2, group="Spillway"),
        FieldSpec("SPILLWAY", "Spillway L [m]", index=3, group="Spillway"),
        FieldSpec("SPILLWAY", "Spillway level [masl]", index=4, group="Spillway"),
        FieldSpec("OUTLET_AUTO_QMIN", "Outlet level [masl]", index=3, group="Auto Qmin outlet"),
    ),
    PSTATION: (
        FieldSpec("NR_GENERATORS", "Number of generators", "int", show_always=True),
        FieldSpec("STATIC_GENERATOR_EFFICIENCY", "Generator efficiency", show_always=True),
        FieldSpec("HEADLOSSCOEF", "Head loss coefficient", show_always=True),
        FieldSpec("POWSTAT_MASL", "Power station [masl]", show_always=True),
        FieldSpec("POWSTAT_MIN_DISCHARGE", "Min discharge [m3/s]"),
        FieldSpec("POWSTAT_STARTSTOP", "Start/stop cost"),
        FieldSpec("LOCAL_ENERGY_EQUIVALENT", "Local energy equivalent"),
        FieldSpec("SHARED_PENSTOCK", "Shared penstock", "bool"),
        FieldSpec("AUTO_QMIN", "Auto Qmin"),
        FieldSpec("MAX_ADJUST", "Max adjust"),
    ),
    CHANNEL: (
        FieldSpec("N_CASCADE_LINRES", "Cascaded linear reservoirs", "int", show_always=True),
        FieldSpec("K_TRAVELTIME_HOURS", "Travel time [h]", show_always=True),
        FieldSpec("N_REACHES", "Reaches", "int"),
        FieldSpec("DECAY", "Decay"),
        FieldSpec("TRAVELTIME", "Travel time (old format)"),
        FieldSpec("ROUTING_DELAY_HOURS", "Routing delay [h]"),
        FieldSpec("REFERENCE_FLOW", "Reference flow"),
        FieldSpec("REFERENCE_TRAVEL_TIME", "Reference travel time"),
        FieldSpec("NONLINEAR_COEFFICIENT", "Nonlinear coefficient"),
        FieldSpec("MANNING_LENGTH_M", "Manning length [m]"),
        FieldSpec("MANNING_BOTTOM_WIDTH_M", "Manning bottom width [m]"),
        FieldSpec("MANNING_SIDE_SLOPE", "Manning side slope"),
        FieldSpec("MANNING_SLOPE", "Manning slope"),
        FieldSpec("MANNING_ROUGHNESS_N", "Manning roughness n"),
        FieldSpec("MANNING_INITIAL_FLOW", "Manning initial flow"),
    ),
    INTAKE: (
        FieldSpec("CAPTURE_CAPACITY", "Capture capacity [m3/s]", show_always=True),
        FieldSpec("CONST_HATCH_RELEASE", "Constant hatch release [m3/s]", show_always=True),
        FieldSpec("VOLUME", "Volume [Mm3]", show_always=True),
    ),
    PUMP: (),
}

CONNECTIONS: dict[str, tuple[ConnSpec, ...]] = {
    RESERVOIR: (
        ConnSpec("tunnel", "Outlet tunnel", "OUTLET_TUNNEL", 1, "token", True),
        ConnSpec("hatch", "Outlet hatch", "OUTLET_HATCH", 1, "line", True),
        ConnSpec("overflow", "Overflow curve", "OVERFLOW_CURVE", 2, "remove", True),
        ConnSpec("spillway", "Spillway", "SPILLWAY", 1, "remove", True),
        ConnSpec("auto_qmin", "Auto Qmin outlet", "OUTLET_AUTO_QMIN", 2, "line", True, gate_index=1),
    ),
    PSTATION: (ConnSpec("downlink", "Tailwater", "DOWNLINK_IDNR", 1, "token", True),),
    CHANNEL: (ConnSpec("downstream", "Downstream", None, 0, "token", True),),
    PUMP: (
        ConnSpec("pump_source", "Pump source", "PUMP_SOURCE_IDNR", 1, creatable=True),
        ConnSpec("pump_target", "Pump target", "PUMP_TARGET_IDNR", 1, creatable=True),
    ),
    INTAKE: (
        ConnSpec("floodwater", "Flood water", "FLOODWATER_IDNR", 1, creatable=True),
        ConnSpec("capture", "Captured water", "CAPCAP_DOWN_IDNR", 1, creatable=True),
        ConnSpec("hatch_release", "Hatch release", "HATCHREL_DOWN_IDNR", 1, creatable=True),
    ),
}

TABLE_COLUMNS: dict[str, tuple[str, ...]] = {
    "RESERVOIR_CURVE": ("masl", "Mm3"),
    "OVERFLOW_CURVE": ("masl", "m3/s"),
    "OUTLET_AUTO_QMIN": ("start DD.MM", "end DD.MM", "m3/s"),
    "QMIN": ("start DD.MM", "end DD.MM", "m3/s", "penalty cost"),
    "TURBINE_CURVE": ("m3/s", "efficiency %"),
    "UNIFORM_NORMALIZED_CURVE": ("Q normalized", "efficiency %"),
    "PUMP_CURVE": ("m3/s", "efficiency %"),
}

_UNIFORM_CURVE = "UNIFORM_NORMALIZED_CURVE 11\n" + "".join(
    f"{q}\t{e}\n"
    for q, e in zip(
        ("0.0", "0.1", "0.2", "0.3", "0.4", "0.5", "0.6", "0.7", "0.8", "0.9", "1.0"),
        (0, 40, 65, 80, 88, 92, 92, 90, 87, 83, 78),
    )
)

_TEMPLATES: dict[str, str] = {
    RESERVOIR: (
        "HRW 100.0\nLRW 90.0\nRES_PENALTY 300\nFLOODLEVEL_PENALTY 300\n"
        "RESERVOIR_CURVE 2\n90\t0.0\n100\t10.0\n"
        "FAST_OVERFLOW FALSE\nOUTLET_HATCH -9999\nOUTLET_TUNNEL -9999\nOUTLET_AUTO_QMIN -9999\n"
    ),
    PSTATION: (
        "DOWNLINK_IDNR -9999\nNR_GENERATORS 1\nGENERATOR 0\n" + _UNIFORM_CURVE +
        "GENERATOR_MAX_DISCHARGE 2.0\nSTATIC_GENERATOR_EFFICIENCY 0.96\nHEADLOSSCOEF 0.1\n"
        "SHARED_PENSTOCK FALSE\nPOWSTAT_MASL 70.0\nPOWSTAT_MIN_DISCHARGE 0.0\n"
        "POWSTAT_STARTSTOP 2.0\nLOCAL_ENERGY_EQUIVALENT 0.0785\nAUTO_QMIN -9999\nMAX_ADJUST -9999\n"
    ),
    CHANNEL: "N_CASCADE_LINRES 3\nK_TRAVELTIME_HOURS 1\nQMIN -9999\n",
    PUMP: (
        "PUMP_SOURCE_IDNR {t0}\nPUMP_TARGET_IDNR {t1}\nPUMP_INLET_MASL 50.0\nMAX_DISCHARGE 2.0\n"
        "HEADLOSSCOEF 0.05\nSTATIC_MOTOR_EFFICIENCY 0.97\nPUMP_STARTSTOP 100.0\n" + _UNIFORM_CURVE
    ),
    INTAKE: (
        "FLOODWATER_IDNR {t0}\nCAPTURE_CAPACITY 10.0\nCAPCAP_DOWN_IDNR {t0}\n"
        "CONST_HATCH_RELEASE 0.5\nHATCHREL_DOWN_IDNR {t0}\nVOLUME 0.2\n"
    ),
}


def make_node(node_type: str, node_id: int, name: str, targets: tuple[int, ...] = ()) -> Node:
    """Create a node with HERSS-complete default fields.

    `targets` supplies existing node ids for the links the type requires
    (INTAKE: one, PUMP: source and target); they are never invented.
    """
    if node_type not in _TEMPLATES:
        raise ValueError(f"unknown node type {node_type!r}")
    needed = {INTAKE: 1, PUMP: 2}.get(node_type, 0)
    if len(targets) < needed:
        raise ValueError(f"{node_type} needs {needed} existing target node(s)")
    node = Node(node_type, node_id, name, ["-9999"] if node_type == CHANNEL else [])
    padded = list(targets) + [0, 0]
    node.set_raw_body_text(_TEMPLATES[node_type].format(t0=padded[0], t1=padded[1]))
    return node
