"""Run the real HERSS engine on topologies saved by topoed.

Skipped when the engine binary is not built. Only data sets that the engine runs
successfully with their original topology are used.
"""

import os
import shutil
import subprocess
from pathlib import Path

import pytest

from topoed import topofile

ROOT = Path(__file__).resolve().parents[2]
HERSS = Path(os.environ.get("HERSS_EXE", ROOT / "src" / "herss.exe"))
DATA = ROOT / "data"
DATASETS = ["channel_cascadedlinres", "channel_lagged_cascadedlinres", "channel_manning",
            "nonlinear_naturalchannel", "utahps_intake"]

pytestmark = pytest.mark.skipif(not HERSS.exists(), reason="HERSS engine binary not built")


def run_engine(workdir: Path) -> subprocess.CompletedProcess:
    config = next(workdir.glob("global*.txt"))
    return subprocess.run([str(HERSS), config.name], cwd=workdir, capture_output=True, text=True, timeout=120)


def result_files(workdir: Path) -> dict[str, bytes]:
    names = ("node*.txt", "riversystem*", "reservoirs*", "outstate*")
    return {p.name: p.read_bytes() for pattern in names for p in workdir.glob(pattern)}


def topology_name(workdir: Path) -> str:
    return next(workdir.glob("topology*.txt")).name


@pytest.mark.parametrize("dataset", DATASETS)
def test_saved_topology_gives_identical_engine_results(dataset, tmp_path):
    reference, saved = tmp_path / "reference", tmp_path / "saved"
    shutil.copytree(DATA / dataset, reference)
    shutil.copytree(DATA / dataset, saved)

    topo_file = saved / topology_name(saved)
    topofile.save(topofile.load(topo_file), topo_file)
    assert topo_file.read_bytes() == (reference / topology_name(reference)).read_bytes()

    first, second = run_engine(reference), run_engine(saved)
    assert first.returncode == 0, first.stdout[-500:]
    assert second.returncode == 0, second.stdout[-500:]
    assert "ended normally" in second.stdout
    assert result_files(saved) == result_files(reference)


def test_edited_topology_runs_in_the_engine(tmp_path):
    work = tmp_path / "edit"
    shutil.copytree(DATA / "utahps_intake", work)
    path = work / topology_name(work)
    topo = topofile.load(path)
    channel = next(n for n in topo.nodes if n.node_type == "CHANNEL")
    channel.set_token("K_TRAVELTIME_HOURS", 1, "6")
    topofile.save(topo, path)
    result = run_engine(work)
    assert result.returncode == 0, result.stdout[-500:]
    assert "ended normally" in result.stdout
