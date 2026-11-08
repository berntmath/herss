from __future__ import annotations

import os
import shutil
from pathlib import Path

import numpy as np
import pytest

from topoed import catalog, results, topofile

FIXTURES = Path(__file__).parent / "fixtures" / "results"
EPOCH_2022_09_01 = 1661990400.0  # 2022-09-01T00:00 UTC

TOPOLOGY_TEXT = (
    "NODE INTAKE 0 G_INTAKE\nENDNODE\n"
    "NODE RESERVOIR 1 RES\nENDNODE\n"
    "NODE PSTATION 2 PS\nENDNODE\n"
    "NODE CHANNEL 3 Bjørnara\nENDNODE\n"
)


def _case(tmp_path: Path, run: str, results_name: str | None = "output") -> Path:
    """A case folder with a topology file and a copy of a results fixture inside results_name."""
    case = tmp_path / "case"
    case.mkdir()
    (case / "topology.txt").write_text(TOPOLOGY_TEXT, encoding="utf-8")
    if results_name is not None:
        shutil.copytree(FIXTURES / run, case / results_name if results_name != "." else case, dirs_exist_ok=True)
    return case


# ---- manifest and CSV ---------------------------------------------------------
def test_manifest_run_structure():
    run = results.load_run(FIXTURES / "manifest_run")
    assert run.source == "manifest"
    assert sorted(run.nodes) == [0, 1, 2, 3]
    assert [run.nodes[i].node_type for i in range(4)] == ["INTAKE", "RESERVOIR", "PSTATION", "CHANNEL"]
    assert run.nodes[3].name == "Bjørnara"
    assert run.topology_name == "topology.txt"
    assert run.nodes[0].downstream == [("CAPCAP_DOWN_IDNR", 1)]
    assert run.last_node_id() == 3


def test_manifest_units_fall_back_to_catalog_and_normalise():
    run = results.load_run(FIXTURES / "manifest_run")
    assert run.nodes[0].unit("storage_Mm3") == "Mm3"  # manifest unit is empty
    assert run.nodes[0].unit("fill_fraction") == "fr"
    assert run.nodes[2].unit("Action_g0") == "fr"  # manifest says fr_g0
    assert run.nodes[1].unit("Price") == "Euro/MWh"


def test_csv_values_and_mixed_time_steps():
    run = results.load_run(FIXTURES / "manifest_run")
    node = run.nodes[1]
    assert list(node.values("Res_fr")) == pytest.approx([0.50, 0.51, 0.52, 0.53])
    times = node.times()
    assert times[0] == EPOCH_2022_09_01
    assert list(np.diff(times)) == [3600.0, 3600.0, 79200.0]  # hourly, then a jump to the next day


def test_columns_are_loaded_lazily():
    run = results.load_run(FIXTURES / "manifest_run")
    assert all(n._table is None for n in run.nodes.values())
    run.nodes[2].values("Power")
    assert run.nodes[2]._table is not None and run.nodes[1]._table is None


def test_unknown_column_is_an_error():
    run = results.load_run(FIXTURES / "manifest_run")
    with pytest.raises(results.ResultsError):
        run.nodes[1].values("nope")


def test_variables_over_nodes():
    run = results.load_run(FIXTURES / "manifest_run")
    found = run.variables([1, 2])
    assert found["Res_fr"] == "fr"
    assert found["Power"] == "MWh"
    assert found["Price"] == "Euro/MWh"
    assert list(found).count("Price") == 1


def test_manifest_with_missing_file_warns_and_fails_on_access(tmp_path):
    folder = tmp_path / "run"
    shutil.copytree(FIXTURES / "manifest_run", folder)
    (folder / "node1_RES.csv").unlink()
    run = results.load_run(folder)
    assert run.nodes[1].warnings
    with pytest.raises(results.ResultsError):
        run.nodes[1].values("Res_fr")


# ---- legacy TXT ------------------------------------------------------------------
def test_legacy_run_types_and_units():
    run = results.load_run(FIXTURES / "legacy_run")
    assert run.source == "legacy"
    assert sorted(run.nodes) == [0, 1, 2, 3, 4]
    types = {i: n.node_type for i, n in run.nodes.items()}
    assert types == {0: "RESERVOIR", 1: "PSTATION", 2: "CHANNEL", 3: "PUMP", 4: "INTAKE"}
    assert run.nodes[1].unit("Action_g0") == "fr"  # [fr_g0] in the file
    assert run.nodes[0].unit("Res_fr") == "fr"
    assert run.nodes[3].unit("Pumped_m3s") == "m3/s"  # no units line, from the catalog
    assert run.nodes[4].unit("fill_fraction") == "fr"  # names line only


def test_legacy_unpadded_and_padded_dates():
    run = results.load_run(FIXTURES / "legacy_run")
    assert run.nodes[0].times()[0] == EPOCH_2022_09_01
    assert run.nodes[4].times()[0] == EPOCH_2022_09_01  # "2022 09 01 00"
    pump = run.nodes[3].times()
    assert list(np.diff(pump)) == [86400.0, 86400.0]  # daily steps


def test_legacy_malformed_row_is_skipped_with_a_warning():
    run = results.load_run(FIXTURES / "legacy_run")
    node = run.nodes[2]
    assert len(node.values("Storage_Mm3")) == 4
    assert any("skipped 1" in w for w in node.warnings)


def test_csv_without_manifest_guesses_the_type(tmp_path):
    folder = tmp_path / "run"
    folder.mkdir()
    for path in (FIXTURES / "manifest_run").glob("*.csv"):
        shutil.copy(path, folder / path.name)
    run = results.load_run(folder)
    assert run.source == "legacy"
    assert [run.nodes[i].node_type for i in range(4)] == ["INTAKE", "RESERVOIR", "PSTATION", "CHANNEL"]


def test_missing_or_empty_folder(tmp_path):
    with pytest.raises(results.ResultsError):
        results.load_run(tmp_path / "nope")
    with pytest.raises(results.ResultsError):
        results.load_run(tmp_path)


# ---- locating the results folder -------------------------------------------------
def test_find_results_dir_output_next_to_topology(tmp_path):
    case = _case(tmp_path, "manifest_run", "output")
    assert results.find_results_dir(case / "topology.txt") == case / "output"


def test_find_results_dir_uses_outputdir_of_the_global_file(tmp_path):
    case = _case(tmp_path, "legacy_run", "elsewhere")
    (case / "global_case.txt").write_text(
        "SYSTEMNAME x\nINPUTDIR ./\nTOPOLOGYFILE topology.txt\nOUTPUTDIR ./elsewhere/\n"
    )
    assert results.find_results_dir(case / "topology.txt") == case / "./elsewhere/"


def test_find_results_dir_outputdir_is_the_case_folder(tmp_path):
    case = _case(tmp_path, "legacy_run", ".")
    (case / "global.txt").write_text("TOPOLOGYFILE topology.txt\nOUTPUTDIR ./\n")
    assert results.find_results_dir(case / "topology.txt") == case / "."


def test_find_results_dir_ignores_global_file_for_another_topology(tmp_path):
    case = _case(tmp_path, "legacy_run", "elsewhere")
    (case / "global.txt").write_text("TOPOLOGYFILE other.txt\nOUTPUTDIR ./elsewhere/\n")
    assert results.find_results_dir(case / "topology.txt") is None


def test_find_results_dir_nothing_found(tmp_path):
    case = _case(tmp_path, "legacy_run", None)
    assert results.find_results_dir(case / "topology.txt") is None


def test_results_dir_setting_round_trip(tmp_path):
    case = _case(tmp_path, "legacy_run", "output")
    topology = case / "topology.txt"
    setting = results.results_dir_setting(topology, case / "output")
    assert setting == "output"
    assert results.resolve_results_dir(topology, setting) == case / "output"
    assert results.resolve_results_dir(topology, "") is None
    assert results.resolve_results_dir(topology, str(tmp_path)) == tmp_path


# ---- checking a results folder -----------------------------------------------------
def test_check_results_clean_manifest_run(tmp_path):
    case = _case(tmp_path, "manifest_run")
    topology = topofile.parse(TOPOLOGY_TEXT)
    future = os.stat(case / "topology.txt").st_mtime + 100
    for path in (case / "output").iterdir():
        os.utime(path, (future, future))
    check = results.check_results(case / "output", case / "topology.txt", topology)
    assert check.found and check.source == "manifest" and check.node_count == 4
    assert check.warnings == []
    assert "manifest" in check.summary()


def test_check_results_legacy_run_without_topology_info(tmp_path):
    check = results.check_results(FIXTURES / "legacy_run")
    assert check.found and check.source == "legacy" and check.node_count == 5
    assert "node files only" in check.summary()


def test_check_results_manifest_from_another_topology(tmp_path):
    case = _case(tmp_path, "manifest_run")
    (case / "topology.txt").rename(case / "mine.txt")
    check = results.check_results(case / "output", case / "mine.txt", topofile.parse(TOPOLOGY_TEXT))
    assert any("topology.txt" in w and "mine.txt" in w for w in check.warnings)


def test_check_results_stale_results(tmp_path):
    case = _case(tmp_path, "manifest_run")
    old = os.stat(case / "topology.txt").st_mtime - 1000
    for path in (case / "output").iterdir():
        os.utime(path, (old, old))
    check = results.check_results(case / "output", case / "topology.txt", topofile.parse(TOPOLOGY_TEXT))
    assert any("older than the topology" in w for w in check.warnings)


def test_check_results_node_mismatch(tmp_path):
    case = _case(tmp_path, "manifest_run")
    text = "NODE RESERVOIR 1 OTHER\nENDNODE\nNODE CHANNEL 7 NEW\nENDNODE\n"
    check = results.check_results(case / "output", case / "topology.txt", topofile.parse(text))
    joined = " | ".join(check.warnings)
    assert "not in the topology: 0, 2, 3" in joined
    assert "no results for nodes: 7" in joined
    assert "names differ" in joined and "1" in joined


def test_check_results_no_folder(tmp_path):
    for folder in (None, tmp_path / "missing", tmp_path):
        check = results.check_results(folder)
        assert not check.found and check.summary()


# ---- catalog -----------------------------------------------------------------------
def test_catalog_power_station_generators():
    names = [n for n, _ in catalog.variables_for("PSTATION", 3)]
    assert [n for n in names if n.startswith("Action_g")] == ["Action_g0", "Action_g1", "Action_g2"]
    assert names[:2] == ["Up_Inflow", "Price"] and names[-1] == "profit"


def test_catalog_additive_units():
    assert catalog.is_additive("m3/s") and catalog.is_additive("[MWh]") and catalog.is_additive("Euro")
    assert not catalog.is_additive("fr") and not catalog.is_additive("fr_g1")
    assert not catalog.is_additive("masl") and not catalog.is_additive("Euro/MWh") and not catalog.is_additive("")
