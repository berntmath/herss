"""Run the real HERSS engine on topologies whose reservoir outlets were switched on by topoed."""

import shutil

import pytest

from topoed import connections, topofile

from .test_engine import DATA, HERSS, result_files, run_engine, topology_name

pytestmark = pytest.mark.skipif(not HERSS.exists(), reason="HERSS engine binary not built")

HJELLE = 1  # reservoir in the utahps_intake example
CHANNEL = 3


def prepared(tmp_path, edit):
    work = tmp_path / "work"
    shutil.copytree(DATA / "utahps_intake", work)
    path = work / topology_name(work)
    topo = topofile.load(path)
    edit(topo.node_by_id(HJELLE))
    topofile.save(topo, path)
    return work


def assert_runs(work):
    result = run_engine(work)
    assert result.returncode == 0, result.stdout[-600:]
    assert "ended normally" in result.stdout


def test_baseline_runs(tmp_path):
    assert_runs(prepared(tmp_path, lambda node: None))


def test_auto_qmin_outlet_is_accepted(tmp_path):
    assert_runs(prepared(tmp_path, lambda node: connections.set_target(node, "auto_qmin", CHANNEL)))


def test_spillway_replacing_the_overflow_curve_is_accepted(tmp_path):
    # At HRW (757) HERSS itself stops on this data set, even for a hand-written line; 759 is above the simulated range.
    def edit(node):
        connections.clear_target(node, "overflow")
        connections.set_target(node, "spillway", CHANNEL)
        node.set_token("SPILLWAY", 4, "759")

    assert_runs(prepared(tmp_path, edit))


def test_recreated_overflow_curve_is_accepted(tmp_path):
    def edit(node):
        connections.clear_target(node, "overflow")
        connections.set_target(node, "overflow", CHANNEL)

    assert_runs(prepared(tmp_path, edit))


def add_hatch_action_column(work):
    path = next(work.glob("actions*.txt"))
    lines = path.read_text().splitlines()
    out = []
    for i, line in enumerate(lines):
        tokens = line.split()
        out.append("\t".join([tokens[0], "1" if i == 0 else "0.0", *tokens[1:]]))
    path.write_text("\n".join(out) + "\n")


def test_hatch_is_accepted_when_the_actions_file_has_its_column(tmp_path):
    work = prepared(tmp_path, lambda node: connections.set_target(node, "hatch", CHANNEL))
    add_hatch_action_column(work)
    assert_runs(work)


def test_hatch_without_an_actions_column_stops_the_engine(tmp_path):
    work = prepared(tmp_path, lambda node: connections.set_target(node, "hatch", CHANNEL))
    result = run_engine(work)
    assert result.returncode != 0
    assert "does not match number of action nodes" in (work / "herss_4.0.01_20261001.log").read_text() or \
        "Please revisit input" in result.stdout + result.stderr


def test_switching_the_outlets_off_again_gives_identical_engine_results(tmp_path):
    reference = tmp_path / "reference"
    shutil.copytree(DATA / "utahps_intake", reference)

    def edit(node):
        for kind in ("hatch", "auto_qmin"):
            connections.set_target(node, kind, CHANNEL)
            connections.clear_target(node, kind)

    work = prepared(tmp_path, edit)
    assert (work / topology_name(work)).read_bytes() == (reference / topology_name(reference)).read_bytes()
    assert run_engine(reference).returncode == 0 and run_engine(work).returncode == 0
    assert result_files(work) == result_files(reference)
