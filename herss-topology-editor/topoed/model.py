"""Lossless in-memory model of a HERSS topology file.

Nodes keep their raw lines; edits replace individual tokens so everything the
editor does not touch (comments, unknown keywords, spacing) is written back as read.
"""

from __future__ import annotations

from dataclasses import dataclass, field

from .textutil import (
    is_blank_or_comment,
    replace_token,
    split_lines,
    to_int,
    tokens,
    truncate_tokens,
)

# Keywords whose second token is a row count; that many data lines follow.
COUNTED_KEYWORDS = frozenset(
    {
        "RESERVOIR_CURVE",
        "OVERFLOW_CURVE",
        "OUTLET_AUTO_QMIN",
        "QMIN",
        "TURBINE_CURVE",
        "UNIFORM_NORMALIZED_CURVE",
        "PUMP_CURVE",
    }
)


@dataclass(frozen=True)
class Entry:
    """One keyword line of a node body, with the data rows that follow it."""

    line_index: int
    keyword: str
    tokens: list[str]
    row_indices: tuple[int, ...] = ()


@dataclass
class Node:
    node_type: str
    node_id: int
    name: str
    header_extra: list[str] = field(default_factory=list)
    body: list[str] = field(default_factory=list)
    leading: list[str] = field(default_factory=list)
    header_line: str | None = None
    end_line: str = "ENDNODE\n"
    eol: str = "\n"

    # ---- header -----------------------------------------------------------
    def render_header(self) -> str:
        want = ["NODE", self.node_type, str(self.node_id), self.name, *self.header_extra]
        if self.header_line is None:
            return " ".join(want) + self.eol
        have = tokens(self.header_line)
        line = self.header_line
        for i, token in enumerate(want):
            if i < len(have):
                same = have[i] == token or (i == 2 and to_int(have[i]) == self.node_id)
                if not same:
                    line = replace_token(line, i, token)
            else:
                line = replace_token(line, i, token)
        if len(want) < len(have):
            line = truncate_tokens(line, len(want))
        return line

    # ---- body scanning ----------------------------------------------------
    def entries(self) -> list[Entry]:
        result: list[Entry] = []
        i, n = 0, len(self.body)
        while i < n:
            if is_blank_or_comment(self.body[i]):
                i += 1
                continue
            toks = tokens(self.body[i])
            rows: list[int] = []
            if toks[0] in COUNTED_KEYWORDS and len(toks) > 1:
                count = to_int(toks[1])
                if count is not None and count > 0:
                    j = i + 1
                    while j < n and len(rows) < count:
                        if not is_blank_or_comment(self.body[j]):
                            rows.append(j)
                        j += 1
            result.append(Entry(i, toks[0], toks, tuple(rows)))
            i = rows[-1] + 1 if rows else i + 1
        return result

    def find_all(self, keyword: str) -> list[Entry]:
        return [e for e in self.entries() if e.keyword == keyword]

    def find(self, keyword: str, occurrence: int = 0) -> Entry | None:
        matches = self.find_all(keyword)
        return matches[occurrence] if occurrence < len(matches) else None

    # ---- single tokens ----------------------------------------------------
    def get_token(self, keyword: str, index: int = 1, occurrence: int = 0) -> str | None:
        entry = self.find(keyword, occurrence)
        if entry is None or index >= len(entry.tokens):
            return None
        return entry.tokens[index]

    def set_token(self, keyword: str, index: int, value: str, occurrence: int = 0) -> None:
        entry = self.find(keyword, occurrence)
        if entry is None:
            if index != 1:
                raise KeyError(f"{keyword} is not present, cannot set token {index}")
            self.append_lines([f"{keyword} {value}"])
            return
        self.body[entry.line_index] = replace_token(self.body[entry.line_index], index, value)

    def append_lines(self, texts: list[str]) -> None:
        """Add lines (without line endings) after the last keyword line or table row."""
        last = -1
        for entry in self.entries():
            last = max(last, entry.row_indices[-1] if entry.row_indices else entry.line_index)
        self.body[last + 1 : last + 1] = [text + self.eol for text in texts]

    def set_block(self, keyword: str, texts: list[str], occurrence: int = 0) -> None:
        """Replace a keyword line and its rows by `texts` in place, or append them when absent."""
        entry = self.find(keyword, occurrence)
        if entry is None:
            self.append_lines(texts)
            return
        for idx in sorted(entry.row_indices, reverse=True):
            del self.body[idx]
        self.body[entry.line_index : entry.line_index + 1] = [text + self.eol for text in texts]

    def replace_entry(self, keyword: str, new_tokens: list[str], occurrence: int = 0) -> None:
        """Rewrite a keyword line (and drop its data rows) with `new_tokens`."""
        entry = self.find(keyword, occurrence)
        text = " ".join([keyword, *new_tokens]) + self.eol
        if entry is None:
            self.body.append(text)
            return
        for idx in sorted(entry.row_indices, reverse=True):
            del self.body[idx]
        self.body[entry.line_index] = text

    def remove_entry(self, keyword: str, occurrence: int = 0) -> None:
        entry = self.find(keyword, occurrence)
        if entry is None:
            return
        for idx in sorted((entry.line_index, *entry.row_indices), reverse=True):
            del self.body[idx]

    # ---- tables -----------------------------------------------------------
    def get_rows(self, keyword: str, occurrence: int = 0) -> list[list[str]]:
        entry = self.find(keyword, occurrence)
        if entry is None:
            return []
        return [tokens(self.body[i]) for i in entry.row_indices]

    def set_rows(self, keyword: str, rows: list[list[str]], occurrence: int = 0) -> None:
        entry = self.find(keyword, occurrence)
        if entry is None:
            raise KeyError(keyword)
        old = [self.body[i] for i in entry.row_indices]
        new_lines: list[str] = []
        for r, row in enumerate(rows):
            if r < len(old) and tokens(old[r]) == row:
                new_lines.append(old[r])
            else:
                new_lines.append("\t".join(row) + self.eol)
        for idx in sorted(entry.row_indices, reverse=True):
            del self.body[idx]
        self.body[entry.line_index + 1 : entry.line_index + 1] = new_lines
        self.body[entry.line_index] = replace_token(
            self.body[entry.line_index], 1, str(len(rows))
        )

    # ---- raw access -------------------------------------------------------
    def raw_body_text(self) -> str:
        return "".join(self.body)

    def set_raw_body_text(self, text: str) -> None:
        lines = split_lines(text)
        if lines and not lines[-1].endswith("\n"):
            lines[-1] += self.eol
        self.body = lines


@dataclass
class Topology:
    nodes: list[Node] = field(default_factory=list)
    trailer: list[str] = field(default_factory=list)
    eol: str = "\n"

    def node_by_id(self, node_id: int) -> Node | None:
        for node in self.nodes:
            if node.node_id == node_id:
                return node
        return None

    def ids(self) -> list[int]:
        return [n.node_id for n in self.nodes]
