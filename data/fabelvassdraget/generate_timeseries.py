#!/usr/bin/env python3
"""
Terje Sandø, pump-station work, July 2026, in collab with Claude.

generate_timeseries.py
===========================================================================
Generates actions.txt, inflowseries.txt and pricefile.txt for the fictional
FABELVASSDRAGET test dataset (see topology.txt for the full system sketch).
Deterministic (no randomness) so the run is fully reproducible; uses only
the Python standard library (no pandas/numpy dependency).

72 hourly time steps starting 2026-01-15 00:00 (solidly inside the "winter"
QMIN/AUTO_QMIN season for this system, see topology.txt).

Run with: python3 generate_timeseries.py
"""

import math
from datetime import datetime, timedelta

START = datetime(2026, 1, 15, 0, 0, 0)
NSTEPS = 72  # 72 hourly steps = exactly 3 days


def date_strings():
    return [(START + timedelta(hours=t)).strftime("%Y%m%d%H") for t in range(NSTEPS)]


def write_pricefile(path):
    lines = ["RESTPRICE\t30", "Date\tPrice"]
    for t, d in enumerate(date_strings()):
        # Daily day/night cycle: cheap at night (~04:00), expensive in the
        # evening (~19:00). Base 35 EUR/MWh, +/- 22 amplitude.
        hour_of_day = t % 24
        price = 35.0 + 22.0 * math.sin((hour_of_day - 4.0) / 24.0 * 2.0 * math.pi - math.pi / 2.0)
        price = max(5.0, price)
        lines.append(f"{d}\t{price:.2f}")
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")


def write_inflowseries(path):
    # (idnr, baseline_m3s, amplitude_fraction, phase_hours)
    reservoirs = [
        (2, 15.0, 0.15, 0),    # SKYTRONA - big mountain catchment
        (3, 1.0, 0.20, 3),     # DUGGTJERN - small headwater tarn
        (6, 2.0, 0.20, 6),     # KLARVIK - mid-valley, modest local catchment
        (10, 4.0, 0.15, 9),    # SKYFONNA - mountain catchment
        (12, 5.0, 0.10, 12),   # TROLLDAMMEN - already receives a lot from upstream
        (14, 1.0, 0.25, 15),   # SALTVIKA - small local catchment near the sea
    ]
    header = "Date_NodeID\t" + "\t".join(str(idnr) for idnr, _, _, _ in reservoirs)
    lines = [header]
    for t, d in enumerate(date_strings()):
        row = [d]
        for idnr, baseline, amp_frac, phase in reservoirs:
            val = baseline * (1.0 + amp_frac * math.sin((t + phase) / 24.0 * 2.0 * math.pi))
            row.append(f"{val:.3f}")
        lines.append("\t".join(row))
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")


def block(t, on_ranges):
    """Return 1.0 if t (hour index) falls in any [start,end) range in on_ranges, else 0.0."""
    for start, end in on_ranges:
        if start <= t < end:
            return 1.0
    return 0.0


def write_actions(path):
    header_cols = [
        "4_0", "4_1",          # TORDENFOSS (2 generators)
        "13_0", "13_1",        # DYPFALL (2 generators)
        "15_0", "15_1",        # TIDEVERK (2 generators)
        "11_0", "11_1", "11_2",  # HEIMFOSS (3 generators)
        "1",                   # OPPLOFT (pump, SALTVIKA -> TROLLDAMMEN)
        "0",                   # HEISVERKET (pump, TROLLDAMMEN -> SKYFONNA)
        "2",                   # SKYTRONA hatch
        "3",                   # DUGGTJERN hatch
        "6",                   # KLARVIK hatch
        "10",                  # SKYFONNA hatch
        "14",                  # SALTVIKA hatch
    ]
    lines = ["Date_NodeID\t" + "\t".join(header_cols)]

    for t, d in enumerate(date_strings()):
        # TORDENFOSS: gentle sine ramp 0.3-0.9
        p1 = 0.6 + 0.3 * math.sin(t / 24.0 * 2.0 * math.pi)
        # DYPFALL: deliberate ON/OFF blocks to exercise POWSTAT_STARTSTOP cost
        p2 = block(t, [(12, 36), (48, 72)]) * 0.8
        # TIDEVERK: mostly high (helps satisfy MUNNINGEN's sea qmin), dips low
        # for a stretch (hours 20-30) to deliberately trigger the qmin cost.
        p3 = 0.15 if 20 <= t < 30 else 0.75
        # HEIMFOSS: moderate sine ramp 0.2-0.7
        p4 = 0.45 + 0.25 * math.sin((t + 6) / 24.0 * 2.0 * math.pi)
        # OPPLOFT (pump): run during the cheapest night hours
        pump_opploft = block(t, [(0, 6), (24, 30), (48, 54)]) * 0.7
        # HEISVERKET (pump): run during a different (also cheap-ish) window
        pump_heisverket = block(t, [(6, 10), (30, 34), (54, 58)]) * 0.6
        # Hatches: modest, varying releases, occasionally fully closed
        h_skytrona = max(0.0, 0.2 + 0.15 * math.sin((t + 2) / 18.0 * 2.0 * math.pi))
        h_duggtjern = max(0.0, 0.15 + 0.15 * math.sin((t + 5) / 20.0 * 2.0 * math.pi))
        h_klarvik = max(0.0, 0.25 + 0.2 * math.sin((t + 8) / 16.0 * 2.0 * math.pi))
        h_skyfonna = max(0.0, 0.2 + 0.2 * math.sin((t + 11) / 22.0 * 2.0 * math.pi))
        h_saltvika = max(0.0, 0.3 + 0.25 * math.sin((t + 14) / 14.0 * 2.0 * math.pi))

        vals = [p1, p1, p2, p2, p3, p3, p4, p4, p4,
                pump_opploft, pump_heisverket,
                h_skytrona, h_duggtjern, h_klarvik, h_skyfonna, h_saltvika]
        vals = [max(0.0, min(1.0, v)) for v in vals]
        row = [d] + [f"{v:.4f}" for v in vals]
        lines.append("\t".join(row))

    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    write_pricefile("pricefile.txt")
    write_inflowseries("inflowseries.txt")
    write_actions("actions.txt")
    print(f"Wrote {NSTEPS} hourly rows starting {START.isoformat()} to pricefile.txt, inflowseries.txt, actions.txt")
