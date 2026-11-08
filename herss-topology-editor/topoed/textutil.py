"""Whitespace-token helpers that edit a raw line while preserving all other bytes."""

import re

_TOKEN = re.compile(r"[^ \t\r\n]+")


_LINE = re.compile(r"[^\n]*\n|[^\n]+")


def split_lines(text: str) -> list[str]:
    # Split on '\n' only (like std::getline), keeping each line ending.
    return _LINE.findall(text)


def tokens(line: str) -> list[str]:
    return _TOKEN.findall(line)


def is_blank_or_comment(line: str) -> bool:
    # Mirrors TopologyParser::loadFile: trimmed line that is empty or starts with '#'.
    stripped = line.strip(" \t\r\n")
    return not stripped or stripped[0] == "#"


def split_eol(line: str) -> tuple[str, str]:
    body = line.rstrip("\r\n")
    return body, line[len(body):]


def replace_token(line: str, index: int, value: str) -> str:
    """Replace token `index`, or append it when `index` equals the token count."""
    body, eol = split_eol(line)
    spans = [m.span() for m in _TOKEN.finditer(body)]
    if index < len(spans):
        start, end = spans[index]
        return body[:start] + value + body[end:] + eol
    if index == len(spans):
        return body + " " + value + eol
    raise IndexError(f"line has {len(spans)} tokens, cannot set token {index}")


def truncate_tokens(line: str, count: int) -> str:
    """Keep only the first `count` tokens (and the original line ending)."""
    body, eol = split_eol(line)
    spans = [m.span() for m in _TOKEN.finditer(body)]
    if count >= len(spans):
        return line
    return body[: spans[count - 1][1]] + eol


def to_int(token: str) -> int | None:
    try:
        return int(token)
    except ValueError:
        return None


def to_float(token: str) -> float | None:
    try:
        return float(token)
    except ValueError:
        return None
