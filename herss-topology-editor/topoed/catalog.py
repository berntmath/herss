"""Variables and units of the HERSS node output files; no Qt.

Source of truth: the node writers in ../src/ (reservoir.cpp, powerstation.cpp, channel.cpp,
pump.cpp, intake.cpp). The manifest and the file headers override this when they are present.
"""

from __future__ import annotations

import re

Variables = tuple[tuple[str, str], ...]  # (column name, unit)

_RESERVOIR: Variables = (
    ("Inflow", "m3/s"), ("Price", "Euro/MWh"), ("Action", "fr"), ("Up_Inflow", "m3/s"),
    ("Res_Mm3", "Mm3"), ("Res_masl", "masl"), ("Res_fr", "fr"), ("lrw_cost", "Euro"),
    ("tunnelflow", "m3/s"), ("hatchflow", "m3/s"), ("overflow", "m3/s"), ("auto_qmin", "m3/s"),
    ("pump_in", "m3/s"), ("pump_out", "m3/s"), ("tot_inflow", "m3/s"), ("tot_outflow", "m3/s"),
)
_PSTATION_HEAD: Variables = (("Up_Inflow", "m3/s"), ("Price", "Euro/MWh"))
_PSTATION_TAIL: Variables = (
    ("tot_outflow", "m3/s"), ("auto_qmin", "m3/s"), ("Hnetto", "m"), ("Hbrutto", "m"),
    ("Power", "MWh"), ("est_eekv", "GWh/Mm3"), ("income", "Euro"), ("tot_cost", "Euro"),
    ("startstopCost", "Euro"), ("adjust_cost", "Euro"), ("cost_aggressive_actions", "Euro"),
    ("profit", "Euro"),
)
_CHANNEL: Variables = (
    ("Up_Inflow", "m3/s"), ("Storage_Mm3", "Mm3"), ("tot_outflow", "m3/s"), ("Qmin_Cost", "Euro"),
)
_INTAKE: Variables = (
    ("inflow_m3s", "m3/s"), ("storage_Mm3", "Mm3"), ("fill_fraction", "fr"),
    ("hatch_release_m3s", "m3/s"), ("captured_m3s", "m3/s"), ("flooding_m3s", "m3/s"),
    ("total_outflow_m3s", "m3/s"),
)
_PUMP: Variables = (
    ("Pumped_m3s", "m3/s"), ("Power_MW", "MW"), ("Cost_Euro", "Euro"), ("startstopCost", "Euro"),
)

NODE_TYPES = ("RESERVOIR", "PSTATION", "CHANNEL", "PUMP", "INTAKE")

# The legacy power station file says POWERSTATION, the topology and manifest say PSTATION.
LEGACY_TYPE_NAMES = {"POWERSTATION": "PSTATION"}

# Only these quantities add up over nodes; levels, fractions and prices do not.
ADDITIVE_UNITS = frozenset({"m3/s", "Mm3", "MWh", "MW", "GWh", "Euro"})

_ACTION_COLUMN = re.compile(r"^Action_g(\d+)$")
_FRACTION_UNIT = re.compile(r"^fr(_g\d+)?$")


def variables_for(node_type: str, n_generators: int = 1) -> Variables:
    """Output columns of a node type. A power station has one Action_gN column per generator."""
    if node_type == "RESERVOIR":
        return _RESERVOIR
    if node_type == "PSTATION":
        actions = tuple((f"Action_g{g}", "fr") for g in range(max(n_generators, 1)))
        return _PSTATION_HEAD + actions + _PSTATION_TAIL
    return {"CHANNEL": _CHANNEL, "INTAKE": _INTAKE, "PUMP": _PUMP}.get(node_type, ())


def normalize_unit(unit: str) -> str:
    """Strip brackets and map the per-generator fraction units (fr_g0, fr_g1, ...) to fr."""
    unit = unit.strip()
    if len(unit) >= 2 and unit[0] == "[" and unit[-1] == "]":
        unit = unit[1:-1]
    return "fr" if _FRACTION_UNIT.match(unit) else unit


def catalog_unit(node_type: str, variable: str) -> str:
    """Unit of a column from the catalog, or "" when unknown."""
    if _ACTION_COLUMN.match(variable):
        return "fr"
    for name, unit in variables_for(node_type):
        if name == variable:
            return unit
    return ""


def is_additive(unit: str) -> bool:
    return normalize_unit(unit) in ADDITIVE_UNITS
