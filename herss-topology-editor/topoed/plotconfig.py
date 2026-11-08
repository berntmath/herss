"""Plot configuration: the model, its JSON file, selector resolution and aggregation; no Qt.

The configuration lives in <topology file name>.plots.json next to the topology and is never
written into the topology file. Unknown keys are kept when loading and saving.
"""

from __future__ import annotations

import json
import os
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from . import catalog
from .model import Topology
from .results import NodeResult, ResultsError, RunData

SCHEMA_VERSION = 1
PANELS = ("left", "middle", "right")
KINDS = ("timeseries",)
AGGREGATES = ("none", "sum", "mean")

ERROR, WARNING = "error", "warning"


class ConfigError(ValueError):
    """The plot configuration file cannot be read."""


@dataclass
class SeriesSpec:
    """One entry of a plot: an explicit node, or a selector over all nodes of a type."""

    variable: str
    node: int | None = None
    node_type: str | None = None
    node_ids: list[int] | None = None
    aggregate: str = "none"
    label: str | None = None
    extra: dict = field(default_factory=dict)

    @property
    def is_selector(self) -> bool:
        return self.node is None


@dataclass
class PlotSpec:
    title: str
    panel: str
    kind: str = "timeseries"
    series: list[SeriesSpec] = field(default_factory=list)
    extra: dict = field(default_factory=dict)


@dataclass
class PlotConfig:
    schema_version: int = SCHEMA_VERSION
    results_dir: str = ""
    plots: list[PlotSpec] = field(default_factory=list)
    extra: dict = field(default_factory=dict)

    def plots_in(self, panel: str) -> list[PlotSpec]:
        return [p for p in self.plots if p.panel == panel]


# ---- the JSON file -------------------------------------------------------------------
def sidecar_path(topology_path: str | os.PathLike[str]) -> Path:
    return Path(str(topology_path) + ".plots.json")


def _series_from_dict(data: dict) -> SeriesSpec:
    if not isinstance(data, dict) or not isinstance(data.get("variable"), str):
        raise ConfigError("a series needs a 'variable' name")
    has_node, has_type = "node" in data, "node_type" in data
    if has_node == has_type:
        raise ConfigError("a series needs either 'node' or 'node_type'")
    known = {"variable", "node", "node_type", "node_ids", "aggregate", "label"}
    try:
        node = int(data["node"]) if has_node else None
        ids = [int(i) for i in data["node_ids"]] if data.get("node_ids") is not None else None
    except (TypeError, ValueError) as exc:
        raise ConfigError(f"bad node id in series {data.get('variable')}: {exc}") from exc
    return SeriesSpec(
        variable=data["variable"],
        node=node,
        node_type=str(data["node_type"]) if has_type else None,
        node_ids=ids,
        aggregate=str(data.get("aggregate", "none")),
        label=None if data.get("label") is None else str(data["label"]),
        extra={k: v for k, v in data.items() if k not in known},
    )


def _series_to_dict(spec: SeriesSpec) -> dict:
    out: dict = {}
    if spec.is_selector:
        out["node_type"] = spec.node_type
        if spec.node_ids is not None:
            out["node_ids"] = list(spec.node_ids)
    else:
        out["node"] = spec.node
    out["variable"] = spec.variable
    if spec.aggregate != "none":
        out["aggregate"] = spec.aggregate
    if spec.label is not None:
        out["label"] = spec.label
    return {**out, **spec.extra}


def parse_config(data: object) -> PlotConfig:
    if not isinstance(data, dict):
        raise ConfigError("the plot configuration must be a JSON object")
    version = data.get("schema_version", SCHEMA_VERSION)
    if not isinstance(version, int) or version > SCHEMA_VERSION:
        raise ConfigError(f"schema_version {version!r} is newer than this editor understands ({SCHEMA_VERSION})")
    plots = []
    for entry in data.get("plots", []):
        if not isinstance(entry, dict):
            raise ConfigError("each plot must be a JSON object")
        known = {"title", "panel", "kind", "series"}
        plots.append(
            PlotSpec(
                title=str(entry.get("title", "")),
                panel=str(entry.get("panel", "")),
                kind=str(entry.get("kind", "timeseries")),
                series=[_series_from_dict(s) for s in entry.get("series", [])],
                extra={k: v for k, v in entry.items() if k not in known},
            )
        )
    known = {"schema_version", "results_dir", "plots"}
    return PlotConfig(
        schema_version=version,
        results_dir=str(data.get("results_dir", "")),
        plots=plots,
        extra={k: v for k, v in data.items() if k not in known},
    )


def config_to_dict(config: PlotConfig) -> dict:
    plots = []
    for plot in config.plots:
        plots.append(
            {
                "title": plot.title,
                "panel": plot.panel,
                "kind": plot.kind,
                "series": [_series_to_dict(s) for s in plot.series],
                **plot.extra,
            }
        )
    return {
        "schema_version": config.schema_version,
        "results_dir": config.results_dir,
        "plots": plots,
        **config.extra,
    }


def load_config(topology_path: str | os.PathLike[str]) -> PlotConfig | None:
    """The saved configuration, or None when there is no file. A broken file raises ConfigError."""
    path = sidecar_path(topology_path)
    if not path.exists():
        return None
    try:
        return parse_config(json.loads(path.read_text(encoding="utf-8")))
    except (OSError, ValueError) as exc:  # ConfigError and json.JSONDecodeError are ValueErrors
        raise ConfigError(f"{path.name}: {exc}") from exc


def save_config(topology_path: str | os.PathLike[str], config: PlotConfig) -> None:
    payload = json.dumps(config_to_dict(config), indent=1, ensure_ascii=False) + "\n"
    sidecar_path(topology_path).write_text(payload, encoding="utf-8")


# ---- the generated system summary ----------------------------------------------------
_OUTFLOW_COLUMN = {"RESERVOIR": "tot_outflow", "PSTATION": "tot_outflow", "CHANNEL": "tot_outflow",
                   "INTAKE": "total_outflow_m3s"}


def system_summary_plots(topology: Topology) -> list[PlotSpec]:
    """Price, total production, total reservoir inflow and the outflow of the last node."""
    plots: list[PlotSpec] = []
    ordered = sorted(topology.nodes, key=lambda n: n.node_id)
    types = {n.node_type for n in ordered}

    priced = next((n for t in ("RESERVOIR", "PSTATION") for n in ordered if n.node_type == t), None)
    if priced is not None:
        plots.append(PlotSpec("Price", "right", series=[SeriesSpec("Price", node=priced.node_id)]))
    if "PSTATION" in types:
        plots.append(PlotSpec("Total production", "right", series=[
            SeriesSpec("Power", node_type="PSTATION", aggregate="sum", label="Total production")]))
    if "RESERVOIR" in types:
        plots.append(PlotSpec("Total reservoir inflow", "right", series=[
            SeriesSpec("Inflow", node_type="RESERVOIR", aggregate="sum", label="Total inflow")]))
    last = ordered[-1] if ordered else None
    if last is not None and last.node_type in _OUTFLOW_COLUMN:
        plots.append(PlotSpec(f"Outflow of {last.name}", "right", series=[
            SeriesSpec(_OUTFLOW_COLUMN[last.node_type], node=last.node_id, label=f"Outflow {last.name}")]))
    return plots


def default_config(topology: Topology) -> PlotConfig:
    """A new configuration: only the right panel has plots, from the system summary."""
    return PlotConfig(plots=system_summary_plots(topology))


# ---- selector resolution -------------------------------------------------------------
def selected_node_ids(spec: SeriesSpec, topology: Topology) -> list[int]:
    """Node IDs a series refers to in the current topology (explicit IDs are returned as given)."""
    if not spec.is_selector:
        return [spec.node]
    ids = sorted(n.node_id for n in topology.nodes if n.node_type == spec.node_type)
    if spec.node_ids is not None:
        wanted = set(spec.node_ids)
        ids = [i for i in ids if i in wanted]
    return ids


@dataclass
class Line:
    label: str
    node_ids: tuple[int, ...]
    variable: str
    unit: str
    times: np.ndarray
    values: np.ndarray


@dataclass
class PlotResult:
    plot: PlotSpec
    lines: list[Line] = field(default_factory=list)
    notices: list[str] = field(default_factory=list)

    @property
    def units(self) -> list[str]:
        return sorted({line.unit for line in self.lines})


def _node_line(node: NodeResult, variable: str, label: str | None) -> Line:
    return Line(label or node.name, (node.node_id,), variable, catalog.normalize_unit(node.unit(variable)),
                node.times(), node.values(variable))


def evaluate_series(spec: SeriesSpec, topology: Topology, run: RunData) -> tuple[list[Line], list[str]]:
    """The lines of one series and the notices for what was skipped."""
    notices: list[str] = []
    ids = selected_node_ids(spec, topology)

    if not spec.is_selector:
        node = run.nodes.get(spec.node)
        if node is None:
            return [], [f"node {spec.node} has no results"]
        if spec.variable not in node.columns:
            return [], [f"node {spec.node} {node.name} has no {spec.variable}"]
        try:
            return [_node_line(node, spec.variable, spec.label)], notices
        except ResultsError as exc:
            return [], [str(exc)]

    if not ids:
        return [], [f"the topology has no {spec.node_type} nodes to plot {spec.variable} for"]

    parts: list[NodeResult] = []
    no_results, no_variable = [], []
    for node_id in ids:
        node = run.nodes.get(node_id)
        if node is None:
            no_results.append(node_id)
        elif spec.variable not in node.columns:
            no_variable.append(node_id)
        else:
            parts.append(node)
    if no_results:
        notices.append("no results for nodes " + ", ".join(map(str, no_results)))
    if no_variable:
        notices.append(f"{spec.variable} is missing for nodes " + ", ".join(map(str, no_variable)) + " (skipped)")
    if not parts:
        return [], notices

    try:
        if spec.aggregate == "none":
            return [_node_line(n, spec.variable, None) for n in parts], notices
        return _aggregate(spec, parts, notices)
    except ResultsError as exc:
        return [], notices + [str(exc)]


def _aggregate(spec: SeriesSpec, parts: list[NodeResult], notices: list[str]) -> tuple[list[Line], list[str]]:
    if spec.aggregate not in AGGREGATES:
        return [], notices + [f"unknown aggregate '{spec.aggregate}'"]
    unit = next((u for u in (catalog.normalize_unit(n.unit(spec.variable)) for n in parts) if u), "")
    if spec.aggregate == "sum" and not catalog.is_additive(unit):
        return [], notices + [f"sum is not allowed for {spec.variable} [{unit or 'unknown unit'}]"]

    base = parts[0].times()
    used = [parts[0]]
    for node in parts[1:]:
        if np.array_equal(node.times(), base):
            used.append(node)
        else:
            notices.append(f"node {node.node_id} {node.name} has a different time axis (skipped)")
    stacked = np.vstack([n.values(spec.variable) for n in used])
    values = stacked.sum(axis=0) if spec.aggregate == "sum" else stacked.mean(axis=0)
    label = spec.label or f"{spec.aggregate} {spec.variable} ({len(used)} {spec.node_type})"
    return [Line(label, tuple(n.node_id for n in used), spec.variable, unit, base, values)], notices


def evaluate_plot(plot: PlotSpec, topology: Topology, run: RunData) -> PlotResult:
    result = PlotResult(plot)
    for spec in plot.series:
        lines, notices = evaluate_series(spec, topology, run)
        result.lines += lines
        result.notices += notices
    if len({line.variable for line in result.lines}) > 1:
        for line in result.lines:
            line.label = f"{line.label} · {line.variable}"
    if len(result.units) > 1:
        result.notices.append("mixed units in one plot: " + ", ".join(u or "?" for u in result.units))
    return result


# ---- describing and checking a configuration ------------------------------------------
def describe_series(spec: SeriesSpec, topology: Topology) -> str:
    """A one-line description of a series for lists."""
    if spec.is_selector:
        text = f"all {spec.node_type} · {spec.variable}"
        if spec.node_ids is not None:
            text = f"{spec.node_type} {', '.join(map(str, spec.node_ids))} · {spec.variable}"
    else:
        node = topology.node_by_id(spec.node)
        text = f"{spec.node} {node.name if node else '(missing)'} · {spec.variable}"
    if spec.is_selector and spec.aggregate != "none":
        text += f" · {spec.aggregate}"
    return text


def sum_error(spec: SeriesSpec, topology: Topology, run: RunData | None = None) -> str | None:
    """Why a sum over the selected nodes is not allowed, or None when it is (or is not asked for)."""
    if not spec.is_selector or spec.aggregate != "sum":
        return None
    unit = catalog.catalog_unit(spec.node_type or "", spec.variable)
    if run is not None:
        ids = selected_node_ids(spec, topology)
        unit = next((run.nodes[i].unit(spec.variable) for i in ids if i in run.nodes), unit)
    if catalog.is_additive(unit):
        return None
    return f"sum is not allowed for {spec.variable} [{unit or 'unknown unit'}]"


def quick_plots(topology: Topology) -> list[tuple[str, list[PlotSpec]]]:
    """Ready-made plots for the quick-add menu, only for the node types the topology has."""
    types = {n.node_type for n in topology.nodes}
    entries: list[tuple[str, list[PlotSpec]]] = []

    def selector(title: str, panel: str, node_type: str, variable: str, aggregate: str = "none") -> PlotSpec:
        label = title if aggregate != "none" else None
        return PlotSpec(title, panel, series=[SeriesSpec(variable, node_type=node_type, aggregate=aggregate, label=label)])

    if "RESERVOIR" in types:
        entries += [
            ("Reservoir fill, all reservoirs (left)", [selector("Reservoir fill", "left", "RESERVOIR", "Res_fr")]),
            ("Reservoir content, all reservoirs (left)", [selector("Reservoir content", "left", "RESERVOIR", "Res_Mm3")]),
            ("Total reservoir inflow (right)", [selector("Total reservoir inflow", "right", "RESERVOIR", "Inflow", "sum")]),
        ]
    if "PSTATION" in types:
        entries += [
            ("Power, all power stations (middle)", [selector("Power", "middle", "PSTATION", "Power")]),
            ("Outflow, all power stations (middle)", [selector("Station outflow", "middle", "PSTATION", "tot_outflow")]),
            ("Total production (right)", [selector("Total production", "right", "PSTATION", "Power", "sum")]),
        ]
    if "CHANNEL" in types:
        entries.append(("Outflow, all channels (middle)", [selector("Channel outflow", "middle", "CHANNEL", "tot_outflow")]))
    summary = system_summary_plots(topology)
    if summary:
        entries.append(("System summary (right panel)", summary))
    return entries


@dataclass
class ConfigIssue:
    severity: str
    plot_index: int | None
    message: str
    series_index: int | None = None
    plot_title: str = ""

    def __str__(self) -> str:
        where = ""
        if self.plot_index is not None:
            where = f"plot {self.plot_index + 1}"
            if self.plot_title:
                where += f" '{self.plot_title}'"
            if self.series_index is not None:
                where += f", series {self.series_index + 1}"
            where += ": "
        return f"{self.severity.upper()}: {where}{self.message}"


def check_config(config: PlotConfig, topology: Topology, run: RunData | None = None) -> list[ConfigIssue]:
    """What is wrong with the entries of a configuration; entries are never removed."""
    issues: list[ConfigIssue] = []
    by_id = {n.node_id: n for n in topology.nodes}

    for index, plot in enumerate(config.plots):
        if plot.panel not in PANELS:
            issues.append(ConfigIssue(ERROR, index, f"panel '{plot.panel}' must be one of {', '.join(PANELS)}"))
        if plot.kind not in KINDS:
            issues.append(ConfigIssue(WARNING, index, f"plot kind '{plot.kind}' is not supported and is not drawn"))
        if not plot.series:
            issues.append(ConfigIssue(WARNING, index, "the plot has no series"))
        for s_index, spec in enumerate(plot.series):
            def add(severity: str, _index: int, message: str, _s: int = s_index) -> None:
                issues.append(ConfigIssue(severity, _index, message, _s))

            _check_series(spec, index, by_id, topology, run, add)
    for issue in issues:
        issue.plot_title = config.plots[issue.plot_index].title
    return issues


def _check_series(spec, index, by_id, topology, run, add) -> None:
    if spec.aggregate not in AGGREGATES:
        add(ERROR, index, f"{spec.variable}: aggregate '{spec.aggregate}' must be one of {', '.join(AGGREGATES)}")

    if spec.is_selector:
        if spec.node_type not in catalog.NODE_TYPES:
            add(ERROR, index, f"unknown node type '{spec.node_type}'")
            return
        ids = selected_node_ids(spec, topology)
        if not ids:
            add(WARNING, index, f"no {spec.node_type} nodes in the topology for {spec.variable}")
        for missing in sorted(set(spec.node_ids or []) - {i for i in by_id if by_id[i].node_type == spec.node_type}):
            add(WARNING, index, f"node {missing} is not a {spec.node_type} in the topology")
        node_types = [spec.node_type]
    else:
        node = by_id.get(spec.node)
        if node is None:
            add(WARNING, index, f"node {spec.node} is not in the topology ({spec.variable} is kept)")
            return
        ids = [spec.node]
        node_types = [node.node_type]

    known = {name for t in node_types for name, _ in catalog.variables_for(t)}
    if spec.variable not in known and not spec.variable.startswith("Action_g"):
        if run is None or not any(spec.variable in run.nodes[i].columns for i in ids if i in run.nodes):
            add(WARNING, index, f"{', '.join(node_types)} has no variable '{spec.variable}'")

    if (problem := sum_error(spec, topology, run)) is not None:
        add(ERROR, index, problem)
