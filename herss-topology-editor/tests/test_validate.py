from pathlib import Path

import pytest

from topoed import connections, topofile, validate
from topoed.schema import make_node

FIXTURES = Path(__file__).parent / "fixtures"
FIXTURE_FILES = sorted(FIXTURES.glob("*.txt"))


def errors(topo):
    return [i for i in validate.validate(topo) if i.severity == validate.ERROR]


def messages(topo):
    return [str(i) for i in validate.validate(topo)]


@pytest.mark.parametrize("path", FIXTURE_FILES, ids=lambda p: p.stem)
def test_real_topologies_have_no_errors(path):
    topo = topofile.load(path)
    assert errors(topo) == [], messages(topo)


def load_pump():
    return topofile.load(FIXTURES / "mini_pump_test.txt")


def test_connections_by_meaning():
    topo = load_pump()
    found = {
        (n.node_id, c.kind): c.target_id
        for n in topo.nodes
        for c in connections.connections_of(n)
        if c.target_id is not None
    }
    assert found[(0, "pump_source")] == 4
    assert found[(0, "pump_target")] == 1
    assert found[(1, "tunnel")] == 3
    assert found[(1, "overflow")] == 2
    assert found[(2, "downstream")] == 4
    assert found[(3, "downlink")] == 4
    assert (4, "tunnel") not in found  # OUTLET_TUNNEL -9999
    assert (5, "downstream") not in found  # -9 means sea


def test_intake_connections():
    topo = topofile.load(FIXTURES / "utahps_intake.txt")
    intake = next(n for n in topo.nodes if n.node_type == "INTAKE")
    targets = {c.kind: c.target_id for c in connections.connections_of(intake)}
    assert targets == {"floodwater": 3, "capture": 1, "hatch_release": 3}


def test_set_and_clear_connection_edit_one_line():
    topo = load_pump()
    reservoir = topo.node_by_id(4)
    connections.set_target(reservoir, "tunnel", 5)
    assert reservoir.get_token("OUTLET_TUNNEL") == "5"
    connections.clear_target(reservoir, "tunnel")
    assert reservoir.get_token("OUTLET_TUNNEL") == "-9999"
    connections.clear_target(reservoir, "hatch")
    assert reservoir.get_token("OUTLET_HATCH") == "-9999"


def test_clear_required_link_is_rejected():
    topo = load_pump()
    with pytest.raises(ValueError):
        connections.clear_target(topo.node_by_id(0), "pump_source")


def test_creating_hatch_activates_it_with_valid_defaults():
    topo = load_pump()
    reservoir = topo.node_by_id(4)
    connections.set_target(reservoir, "hatch", 5)
    assert reservoir.get_token("OUTLET_HATCH", 1) == "5"
    assert len(reservoir.find("OUTLET_HATCH").tokens) == 5
    assert errors(topo) == [], messages(topo)


def test_duplicate_and_missing_ids_are_errors():
    topo = load_pump()
    topo.node_by_id(5).node_id = 2
    text = "\n".join(messages(topo))
    assert "duplicate node id 2" in text
    assert "id must be in 0..5" not in text  # 2 is in range
    topo = load_pump()
    topo.node_by_id(5).node_id = 9
    text = "\n".join(messages(topo))
    assert "id must be in 0..5" in text


def test_reference_to_missing_node():
    topo = load_pump()
    connections.set_target(topo.node_by_id(3), "downlink", 42)
    assert any("missing node 42" in m for m in messages(topo))


def test_self_reference():
    topo = load_pump()
    connections.set_target(topo.node_by_id(3), "downlink", 3)
    assert any("points to the node itself" in m for m in messages(topo))


def test_pump_must_precede_source_and_target():
    topo = load_pump()
    pump = topo.node_by_id(0)
    pump.node_id = 4  # now duplicate of reservoir 4 and above its target 1
    text = "\n".join(messages(topo))
    assert "must be lower than its target id 1" in text or "must be lower" in text


def test_pump_ordering_error_with_distinct_ids():
    pump = (
        "NODE PUMP 2 P\nPUMP_SOURCE_IDNR 0\nPUMP_TARGET_IDNR 1\nPUMP_INLET_MASL 1\n"
        "MAX_DISCHARGE 1\nHEADLOSSCOEF 0\nSTATIC_MOTOR_EFFICIENCY 1\nPUMP_STARTSTOP 0\n"
        "UNIFORM_NORMALIZED_CURVE 11\n" + "0.0\t0\n" * 11 + "ENDNODE\n"
    )
    res = "NODE RESERVOIR {} R{}\nHRW 2\nLRW 1\nRES_PENALTY 1\nRESERVOIR_CURVE 1\n1\t1\nENDNODE\n"
    topo = topofile.parse(res.format(0, 0) + res.format(1, 1) + pump)
    text = "\n".join(messages(topo))
    assert "pump id 2 must be lower than its source id 0" in text
    assert "pump id 2 must be lower than its target id 1" in text


def test_pump_source_must_be_reservoir_and_distinct():
    topo = load_pump()
    pump = topo.node_by_id(0)
    connections.set_target(pump, "pump_source", 3)  # a PSTATION
    assert any("must be a RESERVOIR" in m for m in messages(topo))
    connections.set_target(pump, "pump_source", 1)  # equals target
    assert any("source equals target" in m for m in messages(topo))


def test_file_order_warning():
    topo = load_pump()
    topo.nodes[1], topo.nodes[2] = topo.nodes[2], topo.nodes[1]
    assert any("ascending id order" in m for m in messages(topo))


def test_table_count_mismatch():
    topo = topofile.parse("NODE RESERVOIR 0 A\nRESERVOIR_CURVE 3\n1 2\n3 4\nENDNODE\n")
    assert any("declares 3 rows but only 2" in m for m in messages(topo))


def test_pump_range_checks():
    topo = load_pump()
    topo.node_by_id(0).set_token("STATIC_MOTOR_EFFICIENCY", 1, "1.5")
    assert any("STATIC_MOTOR_EFFICIENCY must be in (0, 1]" in m for m in messages(topo))


@pytest.mark.parametrize("node_type", ["PSTATION", "CHANNEL"])
def test_new_nodes_have_no_errors(node_type):
    topo = load_pump()
    topo.nodes.append(make_node(node_type, 6, "Fresh"))
    assert not [e for e in errors(topo) if e.node_id == 6], messages(topo)


def test_new_reservoir_needs_an_outlet_before_it_is_valid():
    topo = load_pump()
    topo.nodes.append(make_node("RESERVOIR", 6, "Fresh"))
    assert [i.message[:9] for i in errors(topo) if i.node_id == 6] == ["no outlet"]
    connections.set_target(topo.node_by_id(6), "tunnel", 3)
    assert not [e for e in errors(topo) if e.node_id == 6], messages(topo)


def test_new_intake_and_pump_use_given_targets():
    topo = load_pump()
    topo.nodes.append(make_node("INTAKE", 6, "In", targets=(2,)))
    topo.nodes.append(make_node("PUMP", 7, "P2", targets=(1, 4)))
    assert [e for e in errors(topo) if e.node_id == 6] == []
    assert any("must be lower" in m for m in messages(topo) if "node 7" in m)
    with pytest.raises(ValueError):
        make_node("PUMP", 8, "P", targets=(1,))
