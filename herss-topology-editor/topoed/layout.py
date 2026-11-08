"""Canvas positions: a sidecar JSON file plus an automatic left-to-right layout.

Positions are presentation only and never written into the topology file.
"""

from __future__ import annotations

import json
import os
from pathlib import Path

from .connections import connections_of
from .model import Topology

Positions = dict[int, tuple[float, float]]

X_STEP = 260.0
Y_STEP = 110.0


def sidecar_path(topology_path: str | os.PathLike[str]) -> Path:
    return Path(str(topology_path) + ".layout.json")


def load_positions(topology_path: str | os.PathLike[str]) -> Positions:
    path = sidecar_path(topology_path)
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
        return {int(k): (float(v[0]), float(v[1])) for k, v in data["nodes"].items()}
    except (OSError, ValueError, KeyError, TypeError, IndexError):
        return {}


def save_positions(topology_path: str | os.PathLike[str], positions: Positions) -> None:
    payload = {"version": 1, "nodes": {str(k): [x, y] for k, (x, y) in sorted(positions.items())}}
    sidecar_path(topology_path).write_text(json.dumps(payload, indent=1) + "\n", encoding="utf-8")


def flow_edges(topo: Topology) -> list[tuple[int, int]]:
    """Edges in the direction water flows (a pump sits between its source and target)."""
    edges: list[tuple[int, int]] = []
    for node in topo.nodes:
        for conn in connections_of(node):
            if conn.target_id is None:
                continue
            if conn.kind == "pump_source":
                edges.append((conn.target_id, node.node_id))
            else:
                edges.append((node.node_id, conn.target_id))
    return edges


def _drop_back_edges(ids: list[int], edges: list[tuple[int, int]]) -> list[tuple[int, int]]:
    """Remove the edges that close a cycle (e.g. a pump loop) so depths stay small."""
    targets = {b for _, b in edges}
    order = sorted(ids, key=lambda i: (i in targets, i))  # start the search at sources
    adjacent: dict[int, list[int]] = {i: [] for i in ids}
    for a, b in edges:
        adjacent[a].append(b)
    state: dict[int, int] = {}  # 1: on the DFS stack, 2: finished
    back: set[tuple[int, int]] = set()
    for root in order:
        if root in state:
            continue
        state[root] = 1
        stack = [(root, iter(adjacent[root]))]
        while stack:
            node, successors = stack[-1]
            for nxt in successors:
                if state.get(nxt) == 1:
                    back.add((node, nxt))
                elif nxt not in state:
                    state[nxt] = 1
                    stack.append((nxt, iter(adjacent[nxt])))
                    break
            else:
                state[node] = 2
                stack.pop()
    return [e for e in edges if e not in back]


def auto_layout(topo: Topology) -> Positions:
    ids = sorted({n.node_id for n in topo.nodes})
    depth = {i: 0 for i in ids}
    edges = [(a, b) for a, b in flow_edges(topo) if a in depth and b in depth and a != b]
    edges = _drop_back_edges(ids, edges)
    for _ in range(len(ids)):
        changed = False
        for a, b in edges:
            if depth[b] < depth[a] + 1:
                depth[b] = depth[a] + 1
                changed = True
        if not changed:
            break
    rows: dict[int, int] = {}
    positions: Positions = {}
    for node_id in ids:
        row = rows.get(depth[node_id], 0)
        rows[depth[node_id]] = row + 1
        positions[node_id] = (depth[node_id] * X_STEP, row * Y_STEP)
    return positions
