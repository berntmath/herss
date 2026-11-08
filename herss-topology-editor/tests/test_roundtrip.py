from pathlib import Path

import pytest

from topoed import topofile
from topoed.model import Node

FIXTURES = Path(__file__).parent / "fixtures"
FIXTURE_FILES = sorted(FIXTURES.glob("*.txt"))


def read(path: Path) -> str:
    return path.read_bytes().decode("utf-8", errors="surrogateescape")


def test_fixtures_present():
    assert len(FIXTURE_FILES) >= 15


@pytest.mark.parametrize("path", FIXTURE_FILES, ids=lambda p: p.stem)
def test_unedited_roundtrip_is_identical(path):
    text = read(path)
    assert topofile.render(topofile.parse(text)) == text


@pytest.mark.parametrize("path", FIXTURE_FILES, ids=lambda p: p.stem)
def test_save_load_roundtrip(path, tmp_path):
    out = tmp_path / "topology.txt"
    topofile.save(topofile.load(path), out)
    assert out.read_bytes() == path.read_bytes()


def test_node_counts_match_fixture():
    topo = topofile.load(FIXTURES / "mini_pump_test.txt")
    assert [(n.node_type, n.node_id, n.name) for n in topo.nodes] == [
        ("PUMP", 0, "PUMP"),
        ("RESERVOIR", 1, "Reservoir_A"),
        ("CHANNEL", 2, "River_A"),
        ("PSTATION", 3, "HYDRO_POWERSTATION_A"),
        ("RESERVOIR", 4, "Reservoir_B"),
        ("CHANNEL", 5, "River_B"),
    ]
    assert topo.node_by_id(2).header_extra == ["4"]


def changed_lines(before: str, after: str) -> list[tuple[str, str]]:
    a, b = before.splitlines(), after.splitlines()
    assert len(a) == len(b)
    return [(x, y) for x, y in zip(a, b) if x != y]


def test_edit_single_token_changes_only_that_line():
    path = FIXTURES / "mini_pump_test.txt"
    text = read(path)
    topo = topofile.parse(text)
    topo.node_by_id(3).set_token("DOWNLINK_IDNR", 1, "5")
    assert changed_lines(text, topofile.render(topo)) == [
        ("DOWNLINK_IDNR 4", "DOWNLINK_IDNR 5")
    ]


def test_edit_preserves_spacing_of_other_tokens():
    text = read(FIXTURES / "mini_pump_test.txt")
    topo = topofile.parse(text)
    topo.node_by_id(3).set_token("STATIC_GENERATOR_EFFICIENCY", 1, "0.95")
    assert changed_lines(text, topofile.render(topo)) == [
        ("STATIC_GENERATOR_EFFICIENCY\t0.96", "STATIC_GENERATOR_EFFICIENCY\t0.95")
    ]


def test_edit_channel_downstream_in_header():
    text = read(FIXTURES / "mini_pump_test.txt")
    topo = topofile.parse(text)
    topo.node_by_id(2).header_extra = ["5"]
    assert changed_lines(text, topofile.render(topo)) == [
        ("NODE CHANNEL 2 River_A 4", "NODE CHANNEL 2 River_A 5")
    ]


def test_rename_and_renumber_header():
    text = read(FIXTURES / "mini_pump_test.txt")
    topo = topofile.parse(text)
    node = topo.node_by_id(5)
    node.name = "Outlet"
    node.node_id = 9
    assert changed_lines(text, topofile.render(topo)) == [
        ("NODE CHANNEL 5 River_B -9", "NODE CHANNEL 9 Outlet -9")
    ]


def test_unchanged_leading_zero_id_keeps_header():
    topo = topofile.parse("NODE RESERVOIR 07 A\nENDNODE\n")
    assert topofile.render(topo) == "NODE RESERVOIR 07 A\nENDNODE\n"


def test_table_edit_keeps_unchanged_rows_verbatim():
    text = read(FIXTURES / "mini_pump_test.txt")
    topo = topofile.parse(text)
    node = topo.node_by_id(1)
    rows = node.get_rows("RESERVOIR_CURVE")
    assert rows[1] == ["90", "5.0"]
    rows[1] = ["90", "5.5"]
    node.set_rows("RESERVOIR_CURVE", rows)
    assert changed_lines(text, topofile.render(topo)) == [("90\t5.0", "90\t5.5")]


def test_table_add_row_updates_count():
    topo = topofile.parse(read(FIXTURES / "mini_pump_test.txt"))
    node = topo.node_by_id(1)
    rows = node.get_rows("RESERVOIR_CURVE") + [["120", "80.0"]]
    node.set_rows("RESERVOIR_CURVE", rows)
    again = topofile.parse(topofile.render(topo)).node_by_id(1)
    assert again.get_token("RESERVOIR_CURVE", 1) == "6"
    assert again.get_rows("RESERVOIR_CURVE")[-1] == ["120", "80.0"]
    # Entries after the table are still found (rows were not mistaken for keywords).
    assert again.get_token("OVERFLOW_CURVE", 2) == "2"


def test_comments_between_table_rows_are_kept():
    text = (
        "NODE RESERVOIR 0 A\nRESERVOIR_CURVE 2\n1 2\n# note\n3 4\nHRW 5\nENDNODE\n"
    )
    node = topofile.parse(text).nodes[0]
    assert node.get_rows("RESERVOIR_CURVE") == [["1", "2"], ["3", "4"]]
    assert node.get_token("HRW") == "5"


def test_unknown_keywords_are_preserved_on_edit():
    text = "NODE CHANNEL 0 A -9\nFUTURE_KEYWORD 1 2 3\nQMIN -9999\nENDNODE\n"
    topo = topofile.parse(text)
    topo.nodes[0].header_extra = ["1"]
    assert topofile.render(topo) == text.replace("-9\n", "1\n", 1)


def test_missing_final_newline_and_crlf_survive():
    for text in ("NODE CHANNEL 0 A -9\r\nENDNODE", "NODE CHANNEL 0 A -9\r\nENDNODE\r\n"):
        assert topofile.render(topofile.parse(text)) == text


def test_appending_node_after_unterminated_last_line():
    topo = topofile.parse("NODE CHANNEL 0 A -9\nENDNODE")
    topo.nodes.append(Node("CHANNEL", 1, "B", ["-9"]))
    assert topofile.render(topo) == (
        "NODE CHANNEL 0 A -9\nENDNODE\nNODE CHANNEL 1 B -9\nENDNODE\n"
    )


def test_non_utf8_bytes_roundtrip(tmp_path):
    path = tmp_path / "t.txt"
    path.write_bytes(b"# \xe5 latin-1\nNODE CHANNEL 0 A -9\nENDNODE\n")
    out = tmp_path / "o.txt"
    topofile.save(topofile.load(path), out)
    assert out.read_bytes() == path.read_bytes()


@pytest.mark.parametrize(
    "text, fragment",
    [
        ("NODE CHANNEL 0 A -9\n", "no ENDNODE"),
        ("ENDNODE\n", "outside a node"),
        ("NODE CHANNEL 0 A\nNODE CHANNEL 1 B\nENDNODE\n", "before ENDNODE"),
        ("NODE CHANNEL x A\nENDNODE\n", "not an integer"),
        ("NODE CHANNEL 0\nENDNODE\n", "NODE line needs"),
    ],
)
def test_structural_errors_are_reported(text, fragment):
    with pytest.raises(topofile.TopologyParseError, match=fragment):
        topofile.parse(text)
