"""Small result folders for the plot tests."""

from __future__ import annotations

from pathlib import Path

TOPOLOGY_TEXT = (
    "NODE RESERVOIR 0 R_A\nENDNODE\n"
    "NODE RESERVOIR 1 R_B\nENDNODE\n"
    "NODE RESERVOIR 2 R_C\nENDNODE\n"
    "NODE PSTATION 3 PS_A\nENDNODE\n"
    "NODE PSTATION 4 PS_B\nENDNODE\n"
    "NODE CHANNEL 5 RIVER\nENDNODE\n"
)
STAMPS = ["2022-09-01T00:00", "2022-09-01T01:00", "2022-09-01T02:00"]


def write_csv(folder: Path, node_id: int, name: str, columns: dict[str, list[float]]) -> None:
    lines = ["datetime," + ",".join(columns)]
    for i, stamp in enumerate(STAMPS):
        lines.append(stamp + "," + ",".join(str(col[i]) for col in columns.values()))
    (folder / f"node{node_id}_{name}.csv").write_text("\n".join(lines) + "\n")


def make_run_folder(folder: Path) -> Path:
    """Results for TOPOLOGY_TEXT. R_C has no Inflow column, on purpose."""
    folder.mkdir(parents=True, exist_ok=True)
    for node_id, name, inflow, fr in ((0, "R_A", [1, 2, 3], [.1, .2, .3]), (1, "R_B", [10, 20, 30], [.4, .5, .6])):
        write_csv(folder, node_id, name, {"Inflow": inflow, "Price": [50, 60, 70], "Res_fr": fr,
                                          "tot_inflow": inflow, "tot_outflow": [1, 1, 1]})
    write_csv(folder, 2, "R_C", {"Price": [50, 60, 70], "Res_fr": [.7, .8, .9], "tot_outflow": [1, 1, 1]})
    for node_id, name, power in ((3, "PS_A", [1, 1, 1]), (4, "PS_B", [2, 3, 4])):
        write_csv(folder, node_id, name, {"Up_Inflow": [1, 1, 1], "Price": [50, 60, 70],
                                          "Action_g0": [.8, .8, .8], "tot_outflow": [1, 1, 1], "Power": power})
    write_csv(folder, 5, "RIVER", {"Up_Inflow": [1, 1, 1], "Storage_Mm3": [.1, .1, .1],
                                   "tot_outflow": [4, 5, 6], "Qmin_Cost": [0, 0, 0]})
    return folder
