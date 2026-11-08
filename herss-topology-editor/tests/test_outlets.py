import pytest

from topoed import connections, topofile, validate
from topoed.schema import make_node

OUTLET_KINDS = ["hatch", "overflow", "spillway", "auto_qmin"]


def errors(topo):
    return [i for i in validate.validate(topo) if i.severity == validate.ERROR]


def fresh():
    """A downstream channel (id 0) and a reservoir (id 1) with no outlets switched on."""
    topo = topofile.parse("")
    topo.nodes.extend([make_node("CHANNEL", 0, "River"), make_node("RESERVOIR", 1, "Dam")])
    return topo


def reservoir(topo):
    return topo.node_by_id(1)


def active(node):
    return {c.kind: c.target_id for c in connections.connections_of(node) if c.target_id is not None}


@pytest.mark.parametrize("kind", OUTLET_KINDS)
def test_outlet_can_be_switched_on_and_gives_a_valid_topology(kind):
    topo = fresh()
    connections.set_target(reservoir(topo), kind, 0)
    assert active(reservoir(topo)) == {kind: 0}
    assert errors(topo) == [], [str(i) for i in errors(topo)]
    again = topofile.parse(topofile.render(topo))
    assert active(reservoir(again)) == {kind: 0}


@pytest.mark.parametrize("kind", OUTLET_KINDS)
def test_switching_on_then_off_restores_the_original_text(kind):
    topo = fresh()
    before = topofile.render(topo)
    connections.set_target(reservoir(topo), kind, 0)
    assert topofile.render(topo) != before
    connections.clear_target(reservoir(topo), kind)
    assert topofile.render(topo) == before


def test_generated_lines():
    topo = fresh()
    node = reservoir(topo)
    for kind in OUTLET_KINDS:
        if kind != "spillway":
            connections.set_target(node, kind, 0)
    assert node.get_token("OUTLET_HATCH", 0) == "OUTLET_HATCH"
    assert node.find("OUTLET_HATCH").tokens == ["OUTLET_HATCH", "0", "0.0", "1.0", "90"]
    assert node.find("OUTLET_AUTO_QMIN").tokens == ["OUTLET_AUTO_QMIN", "1", "0", "90"]
    assert node.get_rows("OUTLET_AUTO_QMIN") == [["01.01", "31.12", "0.0"]]
    assert node.find("OVERFLOW_CURVE").tokens == ["OVERFLOW_CURVE", "3", "0"]
    assert node.get_rows("OVERFLOW_CURVE") == [["100", "0.0"], ["102", "10.0"], ["105", "60.0"]]


def test_spillway_line():
    topo = fresh()
    connections.set_target(reservoir(topo), "spillway", 0)
    assert reservoir(topo).find("SPILLWAY").tokens == ["SPILLWAY", "0", "2.1", "10.0", "100"]


def test_new_lines_replace_the_placeholder_in_place():
    topo = fresh()
    node = reservoir(topo)
    before = node.body[:]
    connections.set_target(node, "hatch", 0)
    changed = [i for i, (a, b) in enumerate(zip(before, node.body)) if a != b]
    assert len(changed) == 1 and len(node.body) == len(before)
    assert before[changed[0]].startswith("OUTLET_HATCH -9999")


def test_curve_rows_follow_their_keyword_and_other_entries_survive():
    topo = fresh()
    node = reservoir(topo)
    connections.set_target(node, "overflow", 0)
    connections.set_target(node, "auto_qmin", 0)
    assert node.get_token("HRW") == "100.0"
    assert node.get_rows("RESERVOIR_CURVE") == [["90", "0.0"], ["100", "10.0"]]
    assert node.get_token("OUTLET_TUNNEL") == "-9999"
    assert errors(topo) == []


def test_spillway_and_overflow_curve_exclude_each_other():
    topo = fresh()
    node = reservoir(topo)
    connections.set_target(node, "overflow", 0)
    before = topofile.render(topo)
    with pytest.raises(ValueError, match="both a spillway and an overflow curve"):
        connections.set_target(node, "spillway", 0)
    assert topofile.render(topo) == before
    connections.clear_target(node, "overflow")
    connections.set_target(node, "spillway", 0)
    with pytest.raises(ValueError, match="both a spillway and an overflow curve"):
        connections.set_target(node, "overflow", 0)


def test_outlet_needs_reservoir_levels_to_exist():
    topo = fresh()
    node = reservoir(topo)
    node.remove_entry("HRW")
    with pytest.raises(ValueError, match="HRW"):
        connections.set_target(node, "spillway", 0)
    node.remove_entry("LRW")
    with pytest.raises(ValueError, match="LRW"):
        connections.set_target(node, "hatch", 0)


def test_retargeting_an_active_outlet_changes_only_that_token():
    topo = fresh()
    topo.nodes.append(make_node("CHANNEL", 2, "Other"))
    node = reservoir(topo)
    connections.set_target(node, "overflow", 0)
    rows_before = node.get_rows("OVERFLOW_CURVE")
    connections.set_target(node, "overflow", 2)
    assert node.find("OVERFLOW_CURVE").tokens == ["OVERFLOW_CURVE", "3", "2"]
    assert node.get_rows("OVERFLOW_CURVE") == rows_before


def test_clearing_an_inactive_outlet_changes_nothing():
    topo = fresh()
    before = topofile.render(topo)
    for kind in ("hatch", "overflow", "spillway", "auto_qmin", "tunnel"):
        connections.clear_target(reservoir(topo), kind)
    assert topofile.render(topo) == before


def test_activation_note_only_for_hatch():
    assert "actions file" in connections.activation_note("hatch")
    assert all(connections.activation_note(k) is None for k in ("tunnel", "overflow", "spillway", "auto_qmin"))


# ---- validation of the outlet rules HERSS enforces -----------------------------------
def messages(topo):
    return [i.message for i in errors(topo)]


def test_reservoir_without_any_outlet_is_an_error():
    topo = fresh()
    assert any(m.startswith("no outlet") for m in messages(topo))
    connections.set_target(reservoir(topo), "tunnel", 0)
    assert not any(m.startswith("no outlet") for m in messages(topo))


def test_spillway_together_with_overflow_curve_is_an_error():
    topo = fresh()
    node = reservoir(topo)
    connections.set_target(node, "overflow", 0)
    node.append_lines(["SPILLWAY 0 2.1 10.0 100"])
    assert "SPILLWAY and OVERFLOW_CURVE cannot both be used" in messages(topo)


def test_auto_qmin_level_below_the_reservoir_curve_is_an_error():
    topo = fresh()
    node = reservoir(topo)
    connections.set_target(node, "auto_qmin", 0)
    node.set_token("OUTLET_AUTO_QMIN", 3, "80")
    assert any("below the lowest reservoir curve point" in m for m in messages(topo))
    node.set_token("OUTLET_AUTO_QMIN", 3, "95")
    assert not any("below" in m for m in messages(topo))


@pytest.mark.parametrize(
    "rows, fragment",
    [
        (["1.4 30.09 1.0"], "valid zero-padded dates"),
        (["31.04 30.09 1.0"], "valid zero-padded dates"),
        (["01.13 30.09 1.0"], "valid zero-padded dates"),
        (["01.04 30.09 -1.0"], "non-negative discharge"),
        (["01.04 30.09 abc"], "non-negative discharge"),
        (["01.10 31.03 1.0", "01.11 30.04 1.0"], "only one period may cross the year boundary"),
    ],
)
def test_auto_qmin_period_rules(rows, fragment):
    topo = fresh()
    node = reservoir(topo)
    connections.set_target(node, "auto_qmin", 0)
    node.set_block("OUTLET_AUTO_QMIN", [f"OUTLET_AUTO_QMIN {len(rows)} 0 95", *rows])
    assert any(fragment in m for m in messages(topo)), messages(topo)


def test_auto_qmin_accepts_a_leap_day_and_a_single_year_crossing_period():
    topo = fresh()
    node = reservoir(topo)
    connections.set_target(node, "auto_qmin", 0)
    node.set_block("OUTLET_AUTO_QMIN", ["OUTLET_AUTO_QMIN 2 0 95", "01.10 29.02 1.0", "01.03 30.09 2.0"])
    assert errors(topo) == [], messages(topo)


def test_auto_qmin_period_count_limit():
    topo = fresh()
    node = reservoir(topo)
    connections.set_target(node, "auto_qmin", 0)
    rows = [f"01.0{m} 2{m}.0{m} 1.0" for m in range(1, 7)]
    node.set_block("OUTLET_AUTO_QMIN", [f"OUTLET_AUTO_QMIN 6 0 95", *rows])
    assert "OUTLET_AUTO_QMIN needs 1 to 5 periods, found 6" in messages(topo)


def test_channel_qmin_periods_are_checked_too():
    topo = fresh()
    channel = topo.node_by_id(0)
    channel.set_block("QMIN", ["QMIN 1", "01.04 30.09 1.0 -2.0"])
    assert any("non-negative discharge and penalty" in m for m in messages(topo))
