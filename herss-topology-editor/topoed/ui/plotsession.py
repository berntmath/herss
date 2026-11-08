"""State shared by the Configurator and Plot tabs: the plot config and the loaded results.

The session follows the Document (topology and path) but never changes the topology.
"""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

from PySide6.QtCore import QObject, Signal

from .. import plotconfig, results
from ..document import Document


class PlotSession(QObject):
    configChanged = Signal()  # the plot configuration or its error state changed
    runChanged = Signal()  # results were (re)loaded or the results folder changed
    topologyChanged = Signal()  # the topology was edited (not loaded)

    def __init__(self, doc: Document) -> None:
        super().__init__()
        self.doc = doc
        self.config = plotconfig.PlotConfig()
        self.config_error = ""  # set when the plots file exists but cannot be read
        self.dirty = False
        self.run: results.RunData | None = None
        self.check = results.ResultsCheck(False, warnings=["No results folder"])
        self._path: Path | None = None
        self._from_file = False
        doc.changed.connect(self._on_document_changed)
        doc.pathChanged.connect(self._on_path_changed)

    # ---- following the document -------------------------------------------
    def _on_document_changed(self, origin: str) -> None:
        if origin == "load":
            self._load()
        else:
            self.topologyChanged.emit()

    def _on_path_changed(self) -> None:
        """Keep the results folder valid when the topology is saved under another name."""
        new = self.doc.path
        old, self._path = self._path, new
        if old is None or new is None or old == new or not self.config.results_dir:
            return
        folder = results.resolve_results_dir(old, self.config.results_dir)
        if folder is not None:
            self.config.results_dir = results.results_dir_setting(new, folder)

    def _load(self) -> None:
        path = self.doc.path
        self._path = path
        self.config_error = ""
        self._from_file = False
        config = None
        if path is not None:
            try:
                config = plotconfig.load_config(path)
            except plotconfig.ConfigError as exc:
                self.config_error = str(exc)
        self._from_file = config is not None
        self.config = config or plotconfig.default_config(self.doc.topology)
        self.dirty = False
        if not self.config.results_dir and path is not None:
            guess = results.find_results_dir(path)
            if guess is not None:
                self.config.results_dir = results.results_dir_setting(path, guess)
        self.reload_results(announce=False)
        self.configChanged.emit()
        self.runChanged.emit()

    # ---- results ----------------------------------------------------------
    def results_folder(self) -> Path | None:
        return results.resolve_results_dir(self._path, self.config.results_dir)

    def reload_results(self, announce: bool = True) -> None:
        self.check = results.check_results(self.results_folder(), self._path, self.doc.topology)
        self.run = self.check.run
        if announce:
            self.runChanged.emit()

    def refresh_check(self) -> None:
        """Compare the loaded results with the current topology again, without reading files."""
        if self.run is not None:
            self.check = results.check_results(self.results_folder(), self._path, self.doc.topology, self.run)

    def set_results_folder(self, folder: Path | None) -> None:
        self.config.results_dir = "" if folder is None else results.results_dir_setting(self._path, folder)
        self.dirty = True
        self.reload_results(announce=False)
        self.configChanged.emit()
        self.runChanged.emit()

    # ---- configuration ----------------------------------------------------
    def update_config(self, edit: Callable[[plotconfig.PlotConfig], None]) -> None:
        edit(self.config)
        self.dirty = True
        self.configChanged.emit()

    def replace_broken_config(self) -> None:
        """Start from the default configuration after the plots file could not be read."""
        self.config_error = ""
        self.config = plotconfig.default_config(self.doc.topology)
        self.dirty = True
        self.configChanged.emit()

    def save(self) -> None:
        """Write the plots file when it was edited or already exists next to the topology."""
        path = self.doc.path
        if path is None or self.config_error or not (self.dirty or self._from_file):
            return
        plotconfig.save_config(path, self.config)
        self._from_file = True
        self.dirty = False
