# HERSS Topology Editor (topoed)

A graphical tool for creating and editing HERSS topology files and for plotting HERSS results. HERSS remains the simulation engine.

## Status
Early development. Version 0.2 adds a Configurator and a Plot tab to the topology editor (open, edit, validate, save with a node canvas).

## Requirements
Python 3.11 or newer, PySide6, numpy and pyqtgraph.

```
python -m venv .venv
.venv/bin/pip install -e ".[dev]"
```

## Run
```
.venv/bin/topoed [topology.txt]
```
or `python topoed.py [topology.txt]`, or `python -m topoed [topology.txt]`.

- Drag nodes to arrange them; drag from the dot on a node's right edge to another node to draw a link.
- Pan by dragging empty canvas with the left button, or drag anywhere with the middle button. The mouse
  wheel and the arrow keys also scroll. Ctrl+wheel zooms; View > Fit to window (Ctrl+0) shows everything.
- Select a link and press Delete to disconnect it (where HERSS allows an unconnected link).
- Edit values in the Properties panel. Anything the forms do not cover can be edited in the Raw text tab.
- Canvas positions are stored beside the topology as `<file>.layout.json`; the topology file never contains layout data.
- Typed forms exist for RESERVOIR, PSTATION, CHANNEL and INTAKE. PUMP nodes are shown, validated and connectable, and their fields are edited as raw text.
- Reservoir outlets (tunnel, hatch, overflow curve, spillway, auto-Qmin) are switched on by choosing a target node
  in the Properties panel, or by dragging from the reservoir to a node and picking the outlet. Parameters that
  HERSS needs are filled with placeholder values to edit. A spillway and an overflow curve exclude each other, and
  an active hatch needs its own column in the actions file.

## Plotting results
The window has three tabs: Topology (the canvas above), Configurator and Plot. The Configurator and Plot tabs read
the output of a HERSS run and never change the topology.

- **Results folder.** The Configurator has a Results folder field (Browse, Find, Reload). When it is empty the editor
  looks for `./output/` next to the topology, then for the `OUTPUTDIR` of a global file in the same folder that names
  the topology. The status line tells whether a `manifest.json` or only node files were found, whether the manifest
  belongs to this topology, and whether the results are older than the topology file.
  Set `WRITE_NODEFILES 1` in the global file; `NODEFILE_FORMAT CSV` and `MANIFEST_FILE manifest.json` are optional.
- **Plots.** Each plot has a title, a panel (left, middle or right) and one or more series. A series is one node and
  variable, or all nodes of a type (for example the fill fraction of all reservoirs). Selectors follow the topology, so
  a new reservoir shows up without editing the configuration. Series of a selector can be combined with `sum` or
  `mean`; `sum` is refused for levels, fractions and prices. Quick add offers ready-made plots, and the right panel
  starts as a system summary (price, total production, total reservoir inflow, outflow of the last node) that can be
  edited or deleted like any other plot.
- **Plot tab.** Draws the three panels as stacks of plots that share one time axis. Mouse wheel zooms and dragging pans.
  Entries that point to a missing node or variable are kept in the configuration and shown as problems.
- The configuration is stored beside the topology as `<file>.plots.json` and is written by File > Save when it has been
  edited. It is never written into the topology file.

## Tests
```
QT_QPA_PLATFORM=offscreen .venv/bin/python -m pytest
```
The fixtures in `tests/fixtures` are copies of the topologies in `../data`; `tests/fixtures/results` holds small
manifest and legacy result folders. The engine tests in
`tests/test_engine.py` run `../src/herss.exe` (or `$HERSS_EXE`) on temporary copies and are skipped
when it is not built.

## HERSS compatibility
The editor exports HERSS topology files. See `../src/` for the authoritative
format and node behavior.

Unedited files are written back byte for byte, and an edit changes only the tokens it touches.
Comments, unknown keywords and spacing are preserved. Node ids are never renumbered; validation
reports duplicate ids, missing references, pump ordering and similar problems instead.
