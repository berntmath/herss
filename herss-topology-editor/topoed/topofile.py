"""Read and write HERSS topology files without altering untouched text."""

from __future__ import annotations

import os
import tempfile
from pathlib import Path

from .model import Node, Topology
from .textutil import is_blank_or_comment, split_lines, to_int, tokens


class TopologyParseError(ValueError):
    def __init__(self, message: str, line_number: int):
        super().__init__(f"line {line_number}: {message}")
        self.line_number = line_number


def parse(text: str) -> Topology:
    lines = split_lines(text)
    eol = "\r\n" if lines and lines[0].endswith("\r\n") else "\n"
    topo = Topology(eol=eol)
    pending: list[str] = []
    current: Node | None = None

    for number, line in enumerate(lines, 1):
        if is_blank_or_comment(line):
            (current.body if current else pending).append(line)
            continue
        toks = tokens(line)
        keyword = toks[0]
        if keyword == "NODE":
            if current is not None:
                raise TopologyParseError("NODE found before ENDNODE of the previous node", number)
            if len(toks) < 4:
                raise TopologyParseError("NODE line needs: NODE <TYPE> <IDNR> <NAME>", number)
            node_id = to_int(toks[2])
            if node_id is None:
                raise TopologyParseError(f"node id {toks[2]!r} is not an integer", number)
            current = Node(
                node_type=toks[1],
                node_id=node_id,
                name=toks[3],
                header_extra=toks[4:],
                leading=pending,
                header_line=line,
                eol="\r\n" if line.endswith("\r\n") else "\n",
            )
            pending = []
        elif keyword == "ENDNODE":
            if current is None:
                raise TopologyParseError("ENDNODE outside a node", number)
            current.end_line = line
            topo.nodes.append(current)
            current = None
        else:
            (current.body if current else pending).append(line)

    if current is not None:
        raise TopologyParseError(
            f"node {current.node_id} ({current.name}) has no ENDNODE", len(lines)
        )
    topo.trailer = pending
    return topo


def render(topo: Topology) -> str:
    pieces: list[str] = []
    for node in topo.nodes:
        pieces.extend(node.leading)
        pieces.append(node.render_header())
        pieces.extend(node.body)
        pieces.append(node.end_line)
    pieces.extend(topo.trailer)
    # Only the final line of a file may lack a line ending.
    for i in range(len(pieces) - 1):
        if not pieces[i].endswith("\n"):
            pieces[i] += topo.eol
    return "".join(pieces)


def load(path: str | os.PathLike[str]) -> Topology:
    data = Path(path).read_bytes()
    # surrogateescape keeps non-UTF-8 bytes intact through a round trip.
    return parse(data.decode("utf-8", errors="surrogateescape"))


def save(topo: Topology, path: str | os.PathLike[str]) -> None:
    target = Path(path)
    data = render(topo).encode("utf-8", errors="surrogateescape")
    fd, tmp_name = tempfile.mkstemp(dir=target.parent, prefix=target.name + ".", suffix=".tmp")
    try:
        with os.fdopen(fd, "wb") as handle:
            handle.write(data)
        if target.exists():
            os.chmod(tmp_name, target.stat().st_mode & 0o7777)
        os.replace(tmp_name, target)
    except BaseException:
        Path(tmp_name).unlink(missing_ok=True)
        raise
