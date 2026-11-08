#!/home/bernt/py_env/herss_env/bin/python
# -*- coding: utf-8 -*-
# Run with: /home/bernt/py_env/herss_env/bin/python -m pytest test_diagnose_topology.py

from pathlib import Path

import pytest

from diagnose_topology import diagnose, main

DATA = Path(__file__).resolve().parent.parent / "data"
PUMP = DATA / "mini_pump_test" / "topology.txt"
NONLINEAR = DATA / "nonlinear_naturalchannel" / "topology.txt"
INTAKE = DATA / "utahps_intake" / "topology_utahps.txt"
GENINPUT = DATA / "geninput" / "topology.txt"
# Topologies that run in HERSS today; the unit tests must not flag them.
CLEAN = sorted(DATA.glob("*/topology*.txt"))


def run(tmp_path, text):
    path = tmp_path / "topology.txt"
    path.write_text(text, encoding="utf-8")
    return diagnose(path)


def mutate(source, old, new, count=1):
    text = source.read_text(encoding="utf-8")
    assert old in text, f"fixture no longer contains {old!r}"
    return text.replace(old, new, count)


def messages(report, severity="ERROR"):
    return [i.message for i in report.issues if i.severity == severity]


def assert_error(report, fragment):
    found = [m for m in messages(report) if fragment in m]
    assert found, f"no error containing {fragment!r}; got {messages(report)}"


@pytest.mark.parametrize("path", CLEAN, ids=lambda p: p.parent.name)
def test_working_topologies_have_no_errors(path):
    assert diagnose(path).errors == []


def test_layout_matches_pump_fixture():
    report = diagnose(PUMP)
    assert report.inflow_columns() == ["1", "4"]
    assert report.action_columns() == ["0", "3_0"]


def test_layout_matches_intake_fixture():
    report = diagnose(INTAKE)
    assert report.inflow_columns() == ["0", "1"]
    assert report.action_columns() == ["2_0"]
    channel = next(n for n in report.nodes if n.type == "CHANNEL")
    assert (channel.channel_method, channel.channel_states) == ("A_CASCADEDLINRES", 3)


def test_hatch_adds_an_action_column(tmp_path):
    text = mutate(PUMP, "OUTLET_HATCH -9999", "OUTLET_HATCH 2 0.0 1.0 95")
    assert run(tmp_path, text).action_columns() == ["0", "1", "3_0"]


def test_non_ascii_names_are_read_as_utf8(tmp_path):
    report = run(tmp_path, mutate(PUMP, "NODE CHANNEL 2 River_A", "NODE CHANNEL 2 Bjørnarå"))
    assert report.errors == []
    assert any(n.name == "Bjørnarå" for n in report.nodes)


def test_nodes_out_of_id_order(tmp_path):
    lines = PUMP.read_text(encoding="utf-8").splitlines()
    first = next(i for i, l in enumerate(lines) if l.startswith("NODE RESERVOIR 1"))
    second = next(i for i, l in enumerate(lines) if l.startswith("NODE CHANNEL 2"))
    third = next(i for i, l in enumerate(lines) if l.startswith("NODE PSTATION 3"))
    swapped = lines[:first] + lines[second:third] + lines[first:second] + lines[third:]
    assert_error(run(tmp_path, "\n".join(swapped)), "IDs must be 0..")


def test_duplicate_id(tmp_path):
    assert_error(run(tmp_path, mutate(PUMP, "NODE CHANNEL 2", "NODE CHANNEL 1")), "duplicate node ID 1")


def test_missing_endnode(tmp_path):
    assert_error(run(tmp_path, mutate(PUMP, "ENDNODE", "", count=1)), "missing ENDNODE")


def test_unknown_node_type(tmp_path):
    assert_error(run(tmp_path, mutate(PUMP, "NODE CHANNEL 2", "NODE RIVER 2")), "node type 'RIVER'")


def test_downstream_node_does_not_exist(tmp_path):
    assert_error(run(tmp_path, mutate(PUMP, "OUTLET_TUNNEL 3", "OUTLET_TUNNEL 9")), "node 9, which does not exist")


def test_channel_needs_downstream_id(tmp_path):
    assert_error(run(tmp_path, mutate(PUMP, "NODE CHANNEL 2 River_A 4", "NODE CHANNEL 2 River_A")), "needs a downstream ID")


def test_link_to_lower_id_is_a_warning(tmp_path):
    report = run(tmp_path, mutate(PUMP, "NODE CHANNEL 2 River_A 4", "NODE CHANNEL 2 River_A 1"))
    assert any("lower ID" in m for m in messages(report, "WARNING"))


def test_self_reference(tmp_path):
    assert_error(run(tmp_path, mutate(PUMP, "OUTLET_TUNNEL 3", "OUTLET_TUNNEL 1")), "points to the node itself")


def test_hrw_must_exceed_lrw(tmp_path):
    assert_error(run(tmp_path, mutate(PUMP, "LRW 90.0", "LRW 120.0")), "must be lower than HRW")


def test_levels_outside_reservoir_curve(tmp_path):
    assert_error(run(tmp_path, mutate(PUMP, "HRW 110.0", "HRW 500.0")), "outside the reservoir storage range")


def test_curve_must_increase(tmp_path):
    assert_error(run(tmp_path, mutate(PUMP, "100\t20.0", "88\t20.0")), "strictly increasing")


def test_curve_row_count(tmp_path):
    assert_error(run(tmp_path, mutate(PUMP, "RESERVOIR_CURVE 5", "RESERVOIR_CURVE 6")), "RESERVOIR_CURVE")


def test_reservoir_without_outlet(tmp_path):
    text = mutate(PUMP, "OUTLET_TUNNEL 3", "OUTLET_TUNNEL -9999")
    assert not any("no outlet" in m for m in messages(run(tmp_path, text)))  # the overflow curve is an outlet
    text = mutate(PUMP, "OVERFLOW_CURVE 3 2\n110\t0.0\n112\t10.0\n115\t60.0\n", "")
    text = text.replace("FAST_OVERFLOW FALSE", "FAST_OVERFLOW TRUE", 1).replace("OUTLET_TUNNEL 3", "OUTLET_TUNNEL -9999", 1)
    assert_error(run(tmp_path, text), "no outlet")


def test_spillway_and_overflow_curve_exclude_each_other(tmp_path):
    text = mutate(PUMP, "FAST_OVERFLOW FALSE", "FAST_OVERFLOW FALSE\nSPILLWAY 2 2.1 10.0 110")
    assert_error(run(tmp_path, text), "exclude each other")


def test_hatch_needs_five_columns(tmp_path):
    assert_error(run(tmp_path, mutate(PUMP, "OUTLET_HATCH -9999", "OUTLET_HATCH 2 0.0 1.0")), "5 columns")


def test_geometry_and_curve_cannot_be_combined(tmp_path):
    assert_error(run(tmp_path, mutate(PUMP, "FAST_OVERFLOW FALSE", "FAST_OVERFLOW FALSE\nBOTTOM_MASL 80")), "cannot be combined")


def test_generator_block_is_positional(tmp_path):
    assert_error(run(tmp_path, mutate(PUMP, "GENERATOR 0", "GENERATOR 1")), "expected 'GENERATOR 0'")


def test_generator_max_discharge_above_turbine_curve(tmp_path):
    assert_error(run(tmp_path, mutate(PUMP, "GENERATOR_MAX_DISCHARGE 2.0", "GENERATOR_MAX_DISCHARGE 3.0")),
                 "above the largest flow")


def test_too_many_generators(tmp_path):
    assert_error(run(tmp_path, mutate(PUMP, "NR_GENERATORS 1", "NR_GENERATORS 7")), "NR_GENERATORS")


def test_missing_required_pstation_value(tmp_path):
    assert_error(run(tmp_path, mutate(PUMP, "POWSTAT_MASL 70.0", "")), "POWSTAT_MASL is missing")


def test_unknown_keyword_is_fatal_in_channel_but_ignored_elsewhere(tmp_path):
    channel = run(tmp_path, mutate(PUMP, "\nQMIN -9999", "\nQMIN -9999\nBOGUS 1"))
    assert_error(channel, "unknown keyword 'BOGUS'")
    station = run(tmp_path, mutate(PUMP, "AUTO_QMIN -9999", "AUTO_QMIN -9999\nBOGUS 1"))
    assert station.errors == []
    assert any("BOGUS" in m for m in messages(station, "WARNING"))


def test_old_channel_keywords_are_rejected(tmp_path):
    assert_error(run(tmp_path, mutate(PUMP, "K_TRAVELTIME_HOURS 1", "TRAVELTIME 1")), "TRAVELTIME is no longer supported")


def test_channel_without_routing_method(tmp_path):
    assert_error(run(tmp_path, mutate(PUMP, "N_CASCADE_LINRES 3", "")), "cannot determine the routing method")


@pytest.mark.parametrize("value", ["0.9", "0", "-1.2"])
def test_nonlinear_channel_coefficient_below_one(tmp_path, value):
    assert_error(run(tmp_path, mutate(NONLINEAR, "NONLINEAR_COEFFICIENT 1.3", f"NONLINEAR_COEFFICIENT {value}")),
                 "NONLINEAR_COEFFICIENT must be >= 1")


@pytest.mark.parametrize("value", ["1", "1.0", "2.5"])
def test_nonlinear_channel_coefficient_of_one_or_more_is_accepted(tmp_path, value):
    report = run(tmp_path, mutate(NONLINEAR, "NONLINEAR_COEFFICIENT 1.3", f"NONLINEAR_COEFFICIENT {value}"))
    assert report.errors == []


def test_channel_qmin_periods(tmp_path):
    good = "\nQMIN 2\n01.10 31.03 0.5 100\n01.04 30.09 1.0 100"
    assert run(tmp_path, mutate(PUMP, "\nQMIN -9999", good)).errors == []
    assert_error(run(tmp_path, mutate(PUMP, "\nQMIN -9999", "\nQMIN 1\n1.4 30.09 1.0 100")), "zero-padded DD.MM")
    assert_error(run(tmp_path, mutate(PUMP, "\nQMIN -9999", "\nQMIN 1\n31.04 30.09 1.0 100")), "does not exist")
    gap = run(tmp_path, mutate(PUMP, "\nQMIN -9999", "\nQMIN 1\n01.04 30.09 1.0 100"))
    assert any("without a requirement" in m for m in messages(gap, "WARNING"))


def test_pump_must_have_lower_id_than_its_reservoirs(tmp_path):
    text = mutate(PUMP, "PUMP_TARGET_IDNR 1", "PUMP_TARGET_IDNR 0")
    assert_error(run(tmp_path, text), "points to the node itself")
    text = PUMP.read_text(encoding="utf-8").replace("NODE PUMP 0", "NODE PUMP 2").replace("NODE CHANNEL 2", "NODE CHANNEL 0")
    assert any("must be higher than the pump's own ID" in m for m in messages(run(tmp_path, text)))


def test_pump_endpoints_must_be_reservoirs(tmp_path):
    assert_error(run(tmp_path, mutate(PUMP, "PUMP_SOURCE_IDNR 4", "PUMP_SOURCE_IDNR 3")), "must be a RESERVOIR")


def test_pump_needs_an_efficiency_curve(tmp_path):
    lines = PUMP.read_text(encoding="utf-8").splitlines()
    start = next(i for i, l in enumerate(lines) if l.startswith("UNIFORM_NORMALIZED_CURVE"))
    assert_error(run(tmp_path, "\n".join(lines[:start] + lines[start + 12:])), "no efficiency curve")


def test_intake_needs_valid_downstream_ids(tmp_path):
    assert_error(run(tmp_path, mutate(INTAKE, "FLOODWATER_IDNR 3", "FLOODWATER_IDNR 99")), "does not exist")


BOTSVATN_OVERFLOW = "OVERFLOW_CURVE 4 6\n551\t0.0\n553\t59.4\n555\t168.0\n600\t7203.0\n"


def test_spillway_with_reservoir_curve(tmp_path):
    text = mutate(GENINPUT, BOTSVATN_OVERFLOW, "SPILLWAY 6 2.1 10.0 551\n")
    assert_error(run(tmp_path, text), "SPILLWAY cannot be combined with RESERVOIR_CURVE")


def test_spillway_level_below_reservoir_bottom(tmp_path):
    text = mutate(GENINPUT, BOTSVATN_OVERFLOW, "SPILLWAY 6 2.1 10.0 100\n")
    assert_error(run(tmp_path, text), "spillway level = 100.0 masl is outside")


def test_reservoir_curve_must_extend_above_hrw(tmp_path):
    text = mutate(GENINPUT, "RESERVOIR_CURVE 3\n90\t0.0\n100\t10.0\n105\t60.0\n",
                  "RESERVOIR_CURVE 2\n90\t0.0\n100\t10.0\n")
    assert_error(run(tmp_path, text), "RESERVOIR_CURVE ends at 100.0 masl, not above HRW")


def test_powerstation_above_feeding_reservoir(tmp_path):
    text = mutate(GENINPUT, "POWSTAT_MASL 560.0", "POWSTAT_MASL 690.0")
    assert_error(run(tmp_path, text), "the head is never positive")


def test_cli_exit_status(tmp_path, capsys):
    bad = tmp_path / "bad.txt"
    bad.write_text(mutate(PUMP, "LRW 90.0", "LRW 120.0"), encoding="utf-8")
    assert main([str(PUMP), "--no-layout"]) == 0
    assert main([str(bad), "--no-layout"]) == 1
    assert main([str(tmp_path / "missing.txt")]) == 1
    capsys.readouterr()
