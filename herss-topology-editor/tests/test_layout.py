from pathlib import Path

from topoed import layout, topofile

FIXTURES = Path(__file__).parent / "fixtures"


def test_pump_loop_does_not_stretch_the_layout():
    topo = topofile.load(FIXTURES / "fabelvassdraget.txt")
    positions = layout.auto_layout(topo)
    assert set(positions) == set(topo.ids())
    columns = {x for x, _ in positions.values()}
    # 13 stages is the real cascade length; without cycle breaking the pump loop
    # pushed nodes into one column per node (19).
    assert len(columns) <= 13


def test_layout_flows_downstream():
    topo = topofile.load(FIXTURES / "mini_pump_test.txt")
    pos = layout.auto_layout(topo)
    assert pos[3][0] > pos[1][0]  # powerstation right of its reservoir
    assert pos[4][0] > pos[3][0]  # tailwater reservoir right of the powerstation


def test_sidecar_roundtrip_and_bad_file(tmp_path):
    topology = tmp_path / "t.txt"
    layout.save_positions(topology, {3: (1.5, -2.0), 1: (0.0, 4.0)})
    assert layout.load_positions(topology) == {1: (0.0, 4.0), 3: (1.5, -2.0)}
    layout.sidecar_path(topology).write_text("not json")
    assert layout.load_positions(topology) == {}
