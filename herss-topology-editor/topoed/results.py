"""Reader for the output of a HERSS run; no Qt.

A run folder is read from manifest.json plus the node CSV files when a manifest exists,
otherwise from the legacy node<ID>_<NAME>.txt (or .csv) files. Columns of a node are loaded
on first access. Format source: ../src/outputformats.cpp and the node writers in ../src/.
"""

from __future__ import annotations

import csv
import json
import os
import re
from dataclasses import dataclass, field
from pathlib import Path, PurePosixPath

import numpy as np

from . import catalog
from .model import Topology

NODE_FILE_RE = re.compile(r"^node(\d+)_(.*)\.(txt|csv)$")
_DATE_HEADER = ["yyyy", "mm", "dd", "hh"]
_MAX_GLOBAL_FILE_BYTES = 64 * 1024


class ResultsError(ValueError):
    """The results folder or one of its files cannot be used."""


@dataclass
class Table:
    times: np.ndarray  # seconds since 1970-01-01 UTC
    columns: dict[str, np.ndarray]


@dataclass
class _Parsed:
    node_type: str
    names: list[str]
    units: dict[str, str]
    table: Table | None
    skipped: int = 0


def _epoch_seconds(year, month, day, hour) -> np.ndarray:
    year, month, day, hour = (np.asarray(a, dtype=np.int64) for a in (year, month, day, hour))
    months = (year - 1970).astype("datetime64[Y]").astype("datetime64[M]") + (month - 1).astype("timedelta64[M]")
    days = months.astype("datetime64[D]") + (day - 1).astype("timedelta64[D]")
    stamps = days.astype("datetime64[s]") + hour.astype("timedelta64[h]")
    return stamps.astype(np.float64)


def _parse_txt(path: Path, header_only: bool) -> _Parsed:
    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError as exc:
        raise ResultsError(f"{path}: {exc}") from exc

    node_type = "UNKNOWN"
    first = True
    headers: list[list[str]] = []
    names: list[str] = []
    units: dict[str, str] = {}
    rows: list[list[float]] = []
    skipped = 0
    in_data = False

    for line in lines:
        toks = line.split()
        if not toks:
            continue
        if first:
            first = False
            head = line.lstrip("# \t").split()
            if head:
                node_type = catalog.LEGACY_TYPE_NAMES.get(head[0], head[0])
        if not in_data:
            if [t.lower() for t in toks[:4]] == _DATE_HEADER:
                headers.append(toks)
                continue
            if not toks[0][0].isdigit():
                continue  # preamble
            if not headers:
                raise ResultsError(f"{path}: data starts before any column header")
            in_data = True
            names = headers[-1][4:]
            if len(headers) >= 2 and len(headers[-2]) == len(headers[-1]):
                units = {n: catalog.normalize_unit(u) for n, u in zip(names, headers[-2][4:])}
            if header_only:
                return _Parsed(node_type, names, units, None)
        if len(toks) != len(names) + 4:
            skipped += 1
            continue
        try:
            rows.append([float(t) for t in toks])
        except ValueError:
            skipped += 1

    if not in_data:
        if headers:  # header but no data rows
            names = headers[-1][4:]
            return _Parsed(node_type, names, units, Table(np.empty(0), {n: np.empty(0) for n in names}))
        raise ResultsError(f"{path}: no column header found")

    values = np.array(rows, dtype=np.float64).reshape(-1, len(names) + 4)
    times = _epoch_seconds(values[:, 0], values[:, 1], values[:, 2], values[:, 3])
    table = Table(times, {n: values[:, 4 + i] for i, n in enumerate(names)})
    return _Parsed(node_type, names, units, table, skipped)


def _parse_csv(path: Path, header_only: bool) -> _Parsed:
    try:
        with path.open(encoding="utf-8", newline="") as handle:
            reader = csv.reader(handle)
            header = next(reader, None)
            if not header or header[0] != "datetime":
                raise ResultsError(f"{path}: first column must be 'datetime'")
            names = header[1:]
            if header_only:
                return _Parsed("UNKNOWN", names, {}, None)
            stamps: list[str] = []
            rows: list[list[float]] = []
            skipped = 0
            for row in reader:
                if not row:
                    continue
                try:
                    if len(row) != len(names) + 1:
                        raise ValueError
                    values = [float(v) for v in row[1:]]
                except ValueError:
                    skipped += 1
                    continue
                stamps.append(row[0])
                rows.append(values)
    except OSError as exc:
        raise ResultsError(f"{path}: {exc}") from exc
    try:
        times = np.array(stamps, dtype="datetime64[m]").astype("datetime64[s]").astype(np.float64)
    except ValueError as exc:
        raise ResultsError(f"{path}: bad datetime value ({exc})") from exc
    values = np.array(rows, dtype=np.float64).reshape(-1, len(names))
    return _Parsed("UNKNOWN", names, {}, Table(times, {n: values[:, i] for i, n in enumerate(names)}), skipped)


def _parse(path: Path, header_only: bool = False) -> _Parsed:
    return (_parse_csv if path.suffix.lower() == ".csv" else _parse_txt)(path, header_only)


@dataclass
class NodeResult:
    node_id: int
    name: str
    node_type: str
    columns: list[str]
    units: dict[str, str]
    path: Path | None = None
    downstream: list[tuple[str, int]] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)
    _table: Table | None = field(default=None, repr=False)

    def table(self) -> Table:
        if self._table is None:
            if self.path is None:
                raise ResultsError(f"node {self.node_id} {self.name} has no result file")
            parsed = _parse(self.path)
            if parsed.skipped:
                self.warnings.append(f"{self.path.name}: skipped {parsed.skipped} malformed rows")
            self._table = parsed.table
        return self._table

    def times(self) -> np.ndarray:
        return self.table().times

    def values(self, variable: str) -> np.ndarray:
        try:
            return self.table().columns[variable]
        except KeyError:
            raise ResultsError(f"node {self.node_id} {self.name} has no column {variable}") from None

    def unit(self, variable: str) -> str:
        """Unit from the file or manifest, else from the catalog; "" when unknown."""
        unit = self.units.get(variable, "")
        return unit or catalog.catalog_unit(self.node_type, variable)


@dataclass
class RunData:
    folder: Path
    source: str  # "manifest" or "legacy"
    nodes: dict[int, NodeResult]
    manifest: dict | None = None

    @property
    def topology_name(self) -> str | None:
        """File name of the topology the run was made from, as recorded in the manifest."""
        if not self.manifest:
            return None
        name = str(self.manifest.get("inputs", {}).get("topology", ""))
        return PurePosixPath(name.replace("\\", "/")).name or None

    def nodes_of_type(self, node_type: str) -> list[NodeResult]:
        return [n for _, n in sorted(self.nodes.items()) if n.node_type == node_type]

    def variables(self, node_ids: list[int] | None = None) -> dict[str, str]:
        """Variable name -> unit over the given nodes (all by default), in first-seen order."""
        found: dict[str, str] = {}
        for node_id in sorted(self.nodes) if node_ids is None else node_ids:
            node = self.nodes.get(node_id)
            for column in node.columns if node else ():
                if not found.get(column):
                    found[column] = node.unit(column)
        return found

    def last_node_id(self) -> int | None:
        return max(self.nodes) if self.nodes else None


def find_manifest(folder: Path) -> Path | None:
    """manifest.json, or any other JSON file in the folder that has the manifest keys."""
    default = folder / "manifest.json"
    if default.is_file():
        return default
    for candidate in sorted(folder.glob("*.json")):
        try:
            content = json.loads(candidate.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            continue
        if isinstance(content, dict) and "schema_version" in content and "nodes" in content:
            return candidate
    return None


def _load_manifest(manifest_path: Path) -> RunData:
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        entries = manifest["nodes"]
    except (OSError, ValueError, KeyError, TypeError) as exc:
        raise ResultsError(f"{manifest_path}: not a HERSS manifest ({exc})") from exc

    folder = manifest_path.parent
    nodes: dict[int, NodeResult] = {}
    for entry in entries:
        try:
            files = entry.get("files", {})
            path = None
            for kind in ("csv", "txt"):
                if kind in files and (folder / files[kind]).is_file():
                    path = folder / files[kind]
                    break
            columns = entry.get("columns", [])
            node_type = catalog.LEGACY_TYPE_NAMES.get(entry["type"], entry["type"])
            node = NodeResult(
                node_id=int(entry["id"]),
                name=str(entry["name"]),
                node_type=node_type,
                columns=[c["name"] for c in columns],
                units={c["name"]: catalog.normalize_unit(c.get("unit", "")) for c in columns},
                path=path,
                downstream=[(d["via"], int(d["to"])) for d in entry.get("downstream", [])],
            )
        except (KeyError, TypeError, ValueError) as exc:
            raise ResultsError(f"{manifest_path}: bad node entry ({exc})") from exc
        if path is None and files:
            node.warnings.append(f"none of the listed files exist in {folder}")
        elif path is not None and path.suffix.lower() == ".txt":
            # The TXT file carries units that the manifest may leave empty.
            try:
                txt = _parse_txt(path, header_only=True)
                node.units = {**txt.units, **{k: v for k, v in node.units.items() if v}}
            except ResultsError as exc:
                node.warnings.append(str(exc))
        nodes[node.node_id] = node
    return RunData(folder, "manifest", nodes, manifest)


def _load_legacy(folder: Path) -> RunData:
    found: dict[int, tuple[str, Path]] = {}
    for path in sorted(folder.iterdir()):
        match = NODE_FILE_RE.match(path.name)
        if not match or not path.is_file():
            continue
        node_id = int(match.group(1))
        if node_id in found and path.suffix.lower() != ".csv":
            continue  # prefer CSV, it is faster to read
        found[node_id] = (match.group(2), path)
    if not found:
        raise ResultsError(f"no manifest and no node<ID>_<NAME>.txt files in {folder}")

    nodes: dict[int, NodeResult] = {}
    for node_id, (name, path) in sorted(found.items()):
        head = _parse(path, header_only=True)
        node_type = head.node_type
        if node_type == "UNKNOWN":
            node_type = _guess_type(head.names)
        nodes[node_id] = NodeResult(node_id, name, node_type, head.names, head.units, path)
    return RunData(folder, "legacy", nodes)


def _guess_type(names: list[str]) -> str:
    """The node type of a CSV file without a manifest, from its column names."""
    found = set(names)
    if "Res_fr" in found:
        return "RESERVOIR"
    if any(n.startswith("Action_g") for n in names):
        return "PSTATION"
    if "Storage_Mm3" in found:
        return "CHANNEL"
    if "Pumped_m3s" in found:
        return "PUMP"
    if "fill_fraction" in found:
        return "INTAKE"
    return "UNKNOWN"


def load_run(folder: str | os.PathLike[str]) -> RunData:
    """Load the run in an output folder, preferring the manifest and its CSV files."""
    folder = Path(folder)
    if not folder.is_dir():
        raise ResultsError(f"{folder} is not a folder")
    manifest = find_manifest(folder)
    return _load_manifest(manifest) if manifest else _load_legacy(folder)


# ---- locating the results folder -------------------------------------------------
def looks_like_results(folder: Path) -> bool:
    if not folder.is_dir():
        return False
    if find_manifest(folder) is not None:
        return True
    return any(NODE_FILE_RE.match(p.name) for p in folder.iterdir())


def _read_global_file(path: Path) -> dict[str, str]:
    """KEY -> value of a HERSS global file; empty when it is not one."""
    try:
        if path.stat().st_size > _MAX_GLOBAL_FILE_BYTES:
            return {}
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return {}
    values: dict[str, str] = {}
    for line in text.splitlines():
        parts = line.split()
        if len(parts) >= 2 and not parts[0].startswith("#"):
            values[parts[0]] = parts[1]
    return values if "TOPOLOGYFILE" in values else {}


def find_results_dir(topology_path: str | os.PathLike[str]) -> Path | None:
    """Guess the results folder: ./output/ next to the topology, else the OUTPUTDIR of a
    global file in the same folder that names this topology. None when neither has results."""
    topology = Path(topology_path)
    base = topology.parent

    guess = base / "output"
    if looks_like_results(guess):
        return guess

    for candidate in sorted(base.glob("*.txt")):
        values = _read_global_file(candidate)
        topology_name = PurePosixPath(values.get("TOPOLOGYFILE", "").replace("\\", "/")).name
        if not topology_name or topology_name != topology.name or "OUTPUTDIR" not in values:
            continue
        # HERSS uses OUTPUTDIR relative to where it was started; the global file's folder is the usual place.
        folder = base / values["OUTPUTDIR"]
        if looks_like_results(folder):
            return folder
    return None


def resolve_results_dir(topology_path: str | os.PathLike[str] | None, setting: str) -> Path | None:
    """The folder named by the `results_dir` setting, relative to the topology file."""
    if not setting:
        return None
    path = Path(setting)
    if path.is_absolute() or topology_path is None:
        return path
    return Path(topology_path).parent / path


def results_dir_setting(topology_path: str | os.PathLike[str] | None, folder: Path) -> str:
    """The `results_dir` value to store for a folder: relative to the topology when possible."""
    if topology_path is None:
        return str(folder)
    try:
        return os.path.relpath(folder, Path(topology_path).parent)
    except ValueError:  # another drive on Windows
        return str(folder)


# ---- checking a results folder ---------------------------------------------------
@dataclass
class ResultsCheck:
    found: bool
    source: str = ""  # "manifest", "legacy" or ""
    node_count: int = 0
    notes: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)
    run: RunData | None = field(default=None, repr=False)

    def summary(self) -> str:
        if not self.found:
            return self.warnings[0] if self.warnings else "No results found"
        kind = "manifest" if self.source == "manifest" else "no manifest, node files only"
        parts = [f"{self.node_count} nodes, {kind}", *self.notes]
        return "; ".join(parts + self.warnings)


def check_results(
    folder: Path | None,
    topology_path: str | os.PathLike[str] | None = None,
    topology: Topology | None = None,
    run: RunData | None = None,
) -> ResultsCheck:
    """Describe a results folder and how well it fits the open topology. Pass an already
    loaded `run` of the same folder to avoid reading it again."""
    if folder is None or not folder.is_dir():
        return ResultsCheck(False, warnings=["No results folder"])
    if run is None:
        try:
            run = load_run(folder)
        except ResultsError as exc:
            return ResultsCheck(False, warnings=[str(exc)])

    check = ResultsCheck(True, run.source, len(run.nodes), run=run)

    topology_file = Path(topology_path) if topology_path is not None else None
    if run.topology_name and topology_file is not None and run.topology_name != topology_file.name:
        check.warnings.append(f"results were made from {run.topology_name}, not {topology_file.name}")

    if topology is not None:
        by_id = {n.node_id: n for n in topology.nodes}
        extra = sorted(set(run.nodes) - set(by_id))
        missing = sorted(set(by_id) - set(run.nodes))
        if extra:
            check.warnings.append("results have nodes not in the topology: " + ", ".join(map(str, extra)))
        if missing:
            check.warnings.append("no results for nodes: " + ", ".join(map(str, missing)))
        renamed = [i for i in sorted(set(run.nodes) & set(by_id)) if run.nodes[i].name != by_id[i].name]
        if renamed:
            check.warnings.append("node names differ from the topology: " + ", ".join(map(str, renamed)))

    if topology_file is not None and topology_file.is_file():
        stamps = [p.stat().st_mtime for p in _result_files(run) if p.exists()]
        if stamps and max(stamps) < topology_file.stat().st_mtime:
            check.warnings.append("results are older than the topology file")
    return check


def _result_files(run: RunData) -> list[Path]:
    files = [n.path for n in run.nodes.values() if n.path is not None]
    manifest = find_manifest(run.folder) if run.source == "manifest" else None
    return files + ([manifest] if manifest else [])
