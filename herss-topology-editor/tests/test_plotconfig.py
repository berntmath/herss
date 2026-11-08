from __future__ import annotations

import json

import pytest

from topoed import plotconfig as pc
from topoed import results, topofile

from .results_helpers import TOPOLOGY_TEXT, make_run_folder


@pytest.fixture
def topology():
    return topofile.parse(TOPOLOGY_TEXT)


@pytest.fixture
def run(tmp_path):
    return results.load_run(make_run_folder(tmp_path / "out"))


# ---- JSON file -------------------------------------------------------------------------
SAMPLE = {
    "schema_version": 1,
    "results_dir": "output",
    "future_key": {"a": 1},
    "plots": [
        {
            "title": "Reservoir fill",
            "panel": "left",
            "kind": "timeseries",
            "colour_scheme": "dark",
            "series": [
                {"node_type": "RESERVOIR", "variable": "Res_fr", "line_width": 2},
                {"node": 3, "variable": "Power", "label": "Station A"},
                {"node_type": "RESERVOIR", "node_ids": [0, 1], "variable": "Inflow", "aggregate": "sum"},
            ],
        }
    ],
}


def test_round_trip_keeps_unknown_keys(tmp_path):
    topology_file = tmp_path / "topology.txt"
    config = pc.parse_config(SAMPLE)
    pc.save_config(topology_file, config)
    assert pc.sidecar_path(topology_file).name == "topology.txt.plots.json"
    saved = json.loads(pc.sidecar_path(topology_file).read_text())
    assert saved == SAMPLE
    assert pc.config_to_dict(pc.load_config(topology_file)) == SAMPLE


def test_series_kinds_are_parsed():
    series = pc.parse_config(SAMPLE).plots[0].series
    assert series[0].is_selector and series[0].node_type == "RESERVOIR" and series[0].node_ids is None
    assert not series[1].is_selector and series[1].node == 3 and series[1].label == "Station A"
    assert series[2].node_ids == [0, 1] and series[2].aggregate == "sum"


def test_load_config_without_file(tmp_path):
    assert pc.load_config(tmp_path / "topology.txt") is None


@pytest.mark.parametrize(
    "text",
    [
        "{not json",
        "[]",
        json.dumps({"schema_version": 99}),
        json.dumps({"plots": [{"title": "t", "panel": "left", "series": [{"variable": "x"}]}]}),
        json.dumps({"plots": [{"title": "t", "panel": "left", "series": [{"node": 1, "node_type": "RESERVOIR", "variable": "x"}]}]}),
        json.dumps({"plots": [{"title": "t", "panel": "left", "series": [{"node": "one", "variable": "x"}]}]}),
    ],
)
def test_broken_files_raise_config_error(tmp_path, text):
    topology_file = tmp_path / "topology.txt"
    pc.sidecar_path(topology_file).write_text(text)
    with pytest.raises(pc.ConfigError):
        pc.load_config(topology_file)


# ---- system summary ---------------------------------------------------------------------
def test_default_config_is_the_right_panel_summary(topology):
    config = pc.default_config(topology)
    assert {p.panel for p in config.plots} == {"right"}
    assert [p.title for p in config.plots] == ["Price", "Total production", "Total reservoir inflow", "Outflow of RIVER"]
    last = config.plots[-1].series[0]
    assert (last.node, last.variable) == (5, "tot_outflow")
    assert pc.check_config(config, topology) == []


def test_summary_skips_what_the_topology_lacks():
    topology = topofile.parse("NODE CHANNEL 0 A\nENDNODE\nNODE CHANNEL 1 B\nENDNODE\n")
    assert [p.title for p in pc.default_config(topology).plots] == ["Outflow of B"]
    assert pc.default_config(topofile.parse("")).plots == []


def test_summary_evaluates_on_a_run(topology, run):
    for plot in pc.default_config(topology).plots:
        result = pc.evaluate_plot(plot, topology, run)
        # R_C has no Inflow column, so only the inflow plot carries a notice
        expected = ["Inflow is missing for nodes 2 (skipped)"] if plot.title == "Total reservoir inflow" else []
        assert len(result.lines) == 1 and result.notices == expected, plot.title
    production = pc.evaluate_plot(pc.default_config(topology).plots[1], topology, run).lines[0]
    assert list(production.values) == [3, 4, 5]


# ---- selectors and aggregation --------------------------------------------------------------
def test_selector_follows_the_topology(topology):
    spec = pc.SeriesSpec("Res_fr", node_type="RESERVOIR")
    assert pc.selected_node_ids(spec, topology) == [0, 1, 2]
    topology.nodes.append(topofile.parse("NODE RESERVOIR 9 NEW\nENDNODE\n").nodes[0])
    assert pc.selected_node_ids(spec, topology) == [0, 1, 2, 9]
    spec.node_ids = [1, 9, 3]
    assert pc.selected_node_ids(spec, topology) == [1, 9]


def test_all_reservoirs_in_one_plot(topology, run):
    plot = pc.PlotSpec("Fill", "left", series=[pc.SeriesSpec("Res_fr", node_type="RESERVOIR")])
    result = pc.evaluate_plot(plot, topology, run)
    assert [line.label for line in result.lines] == ["R_A", "R_B", "R_C"]
    assert result.units == ["fr"] and result.notices == []


def test_sum_of_inflow_skips_nodes_without_the_variable(topology, run):
    spec = pc.SeriesSpec("Inflow", node_type="RESERVOIR", aggregate="sum")
    lines, notices = pc.evaluate_series(spec, topology, run)
    assert len(lines) == 1 and lines[0].node_ids == (0, 1)
    assert list(lines[0].values) == [11, 22, 33]
    assert lines[0].unit == "m3/s"
    assert notices == ["Inflow is missing for nodes 2 (skipped)"]


def test_mean_aggregate(topology, run):
    spec = pc.SeriesSpec("Power", node_type="PSTATION", aggregate="mean", label="Mean power")
    (line,), _ = pc.evaluate_series(spec, topology, run)
    assert line.label == "Mean power" and list(line.values) == [1.5, 2.0, 2.5]


def test_sum_of_a_fraction_is_rejected(topology, run):
    spec = pc.SeriesSpec("Res_fr", node_type="RESERVOIR", aggregate="sum")
    lines, notices = pc.evaluate_series(spec, topology, run)
    assert lines == [] and "sum is not allowed for Res_fr [fr]" in notices[0]


def test_explicit_series_problems_are_notices(topology, run):
    assert pc.evaluate_series(pc.SeriesSpec("Power", node=42), topology, run)[1] == ["node 42 has no results"]
    lines, notices = pc.evaluate_series(pc.SeriesSpec("nope", node=3), topology, run)
    assert lines == [] and "has no nope" in notices[0]


def test_selector_without_matching_nodes(run):
    topology = topofile.parse("NODE CHANNEL 5 RIVER\nENDNODE\n")
    lines, notices = pc.evaluate_series(pc.SeriesSpec("Power", node_type="PSTATION"), topology, run)
    assert lines == [] and "no PSTATION nodes" in notices[0]


def test_mixed_units_and_variables_are_labelled(topology, run):
    plot = pc.PlotSpec("Mixed", "middle", series=[
        pc.SeriesSpec("Power", node=3), pc.SeriesSpec("Price", node=3)])
    result = pc.evaluate_plot(plot, topology, run)
    assert [line.label for line in result.lines] == ["PS_A · Power", "PS_A · Price"]
    assert result.notices == ["mixed units in one plot: Euro/MWh, MWh"]


def test_aggregation_skips_a_different_time_axis(topology, run):
    run.nodes[4].table().times[:] += 3600  # PS_B no longer matches PS_A
    spec = pc.SeriesSpec("Power", node_type="PSTATION", aggregate="sum")
    (line,), notices = pc.evaluate_series(spec, topology, run)
    assert line.node_ids == (3,) and "different time axis" in notices[0]


# ---- checking a configuration -----------------------------------------------------------------
def test_check_config_flags_but_keeps_bad_entries(topology, run):
    config = pc.PlotConfig(plots=[
        pc.PlotSpec("Bad panel", "top", series=[pc.SeriesSpec("Power", node=3)]),
        pc.PlotSpec("Gone", "left", series=[pc.SeriesSpec("Power", node=42)]),
        pc.PlotSpec("Types", "left", kind="scatter", series=[
            pc.SeriesSpec("Power", node_type="TURBINE"),
            pc.SeriesSpec("Res_fr", node_type="RESERVOIR", aggregate="sum"),
            pc.SeriesSpec("Inflow", node_type="RESERVOIR", aggregate="median"),
            pc.SeriesSpec("Nope", node=3),
            pc.SeriesSpec("Power", node_type="PSTATION", node_ids=[3, 77]),
        ]),
        pc.PlotSpec("Empty", "middle"),
    ])
    text = "\n".join(str(i) for i in pc.check_config(config, topology, run))
    assert "panel 'top' must be one of left, middle, right" in text
    assert "node 42 is not in the topology" in text
    assert "plot kind 'scatter' is not supported" in text
    assert "unknown node type 'TURBINE'" in text
    assert "sum is not allowed for Res_fr [fr]" in text
    assert "aggregate 'median'" in text
    assert "has no variable 'Nope'" in text
    assert "node 77 is not a PSTATION" in text
    assert "plot 4 'Empty': the plot has no series" in text
    assert len(config.plots) == 4 and len(config.plots[2].series) == 5


def test_check_config_accepts_generator_actions(topology):
    config = pc.PlotConfig(plots=[pc.PlotSpec("A", "left", series=[pc.SeriesSpec("Action_g3", node_type="PSTATION")])])
    assert pc.check_config(config, topology) == []
