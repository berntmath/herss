#!/home/bernt/py_env/herss_env/bin/python
# -*- coding: utf-8 -*-


# Generates a synthetic HERSS input dataset from a topology file.
# Usage: geninput.py <topologyfile>   (no folder in the name means the current folder)
# Writes global.txt, price.txt, inflow.txt, actions.txt and start_state.txt in the folder of the topology.
# The column layout comes from diagnose_topology.py, which mirrors the HERSS readers.

import argparse
import datetime as dt
from pathlib import Path

import numpy as np

from diagnose_topology import diagnose, print_report


def timestamps(start, n_steps, step_hours):
    step = dt.timedelta(hours=step_hours)
    return [(start + i * step).strftime("%Y%m%d%H") for i in range(n_steps)]


def write_global(folder, topology_name):
    (folder / "global.txt").write_text(
        "SYSTEMNAME synthetic\n\n"
        "INPUTDIR ./\n"
        "ACTIONFILE actions.txt\n"
        "INFLOWFILE inflow.txt\n"
        "PRICEFILE price.txt\n"
        f"TOPOLOGYFILE {topology_name}\n"
        "STARTSTATEFILE start_state.txt\n\n"
        "OUTSTATEFILE out_state.txt\n"
        "WRITE_NODEFILES 1\n"
        "OUTPUTDIR ./output/\n",
        encoding="utf-8",
    )


def write_price(folder, stamps, step_hours, rng):
    t = np.arange(len(stamps)) * step_hours
    daily = 80 * np.sin(2 * np.pi * (t % 24) / 24 - 2.0)
    weekly = -40 * (((t // 24) % 7) >= 5)
    price = np.maximum(400 + daily + weekly + rng.normal(0, 20, len(t)), 1.0)
    with open(folder / "price.txt", "w", encoding="utf-8") as f:
        f.write(f"RESTPRICE\t{price.mean():.0f}\nDate\tPrice\n")
        for s, p in zip(stamps, price):
            f.write(f"{s}\t{p:.2f}\n")


def write_inflow(folder, stamps, step_hours, report, rng):
    cols = report.inflow_columns()
    t = np.arange(len(stamps)) * step_hours
    series = []
    for _ in cols:
        base = rng.uniform(0.2, 2.0)
        seasonal = 1 + 0.5 * np.sin(2 * np.pi * t / (24 * 365))
        drift = np.exp(np.cumsum(rng.normal(0, 0.01, len(t))))
        series.append(base * seasonal * drift)
    with open(folder / "inflow.txt", "w", encoding="utf-8") as f:
        f.write("Date_NodeID\t" + "\t".join(cols) + "\n")
        for i, s in enumerate(stamps):
            f.write(s + "\t" + "\t".join(f"{c[i]:.3f}" for c in series) + "\n")


def write_actions(folder, stamps, report):
    cols = report.action_columns()
    row = "\t".join("0.00" for _ in cols)
    with open(folder / "actions.txt", "w", encoding="utf-8") as f:
        f.write("Date_NodeID\t" + "\t".join(cols) + "\n")
        for s in stamps:
            f.write(f"{s}\t{row}\n")


def write_start_state(folder, report, rng):
    lines = ["# Synthetic start state"]
    for n in report.nodes:
        if n.type == "RESERVOIR":
            lines.append(f"NODE RESERVOIR {n.id} {n.name} {rng.uniform(0.5, 0.9):.2f}")
        elif n.type in ("INTAKE", "PSTATION", "PUMP"):
            lines.append(f"NODE {n.type} {n.id} {n.name} 0.0")
        elif n.type == "CHANNEL":
            vals = " ".join(["0.001"] * n.channel_states)
            lines.append(f"NODE CHANNEL {n.id} {n.name} {n.channel_method} N {n.channel_states} {vals}")
    (folder / "start_state.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    ap = argparse.ArgumentParser(description="Generate a synthetic HERSS dataset from a topology file.")
    ap.add_argument("topology", help="topology file; its folder (default ./) receives the generated files")
    ap.add_argument("--start", default="2022-09-01", help="first timestep, YYYY-MM-DD")
    ap.add_argument("--steps", type=int, default=24 * 30)
    ap.add_argument("--step-hours", type=int, default=1)
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args()

    topo = Path(args.topology)
    if not topo.is_file():
        ap.error(f"topology file not found: {topo}")
    report = diagnose(topo)
    if report.errors:
        print_report(report, layout=False)
        raise SystemExit(1)

    folder = topo.parent  # Path("topology.txt").parent is "."
    rng = np.random.default_rng(args.seed)
    stamps = timestamps(dt.datetime.fromisoformat(args.start), args.steps, args.step_hours)

    (folder / "output").mkdir(exist_ok=True)
    write_global(folder, topo.name)
    write_price(folder, stamps, args.step_hours, rng)
    write_inflow(folder, stamps, args.step_hours, report, rng)
    write_actions(folder, stamps, report)
    write_start_state(folder, report, rng)
    print(f"Wrote dataset for {len(report.nodes)} nodes, {args.steps} steps, to {folder}")


if __name__ == "__main__":
    main()





