# HERSS Topology Editor Agent Guidance

## Purpose
This is a separate Python GUI for creating and editing HERSS topology files and
for plotting HERSS results. It has three tabs: Topology, Configurator and Plot.
HERSS remains the simulation engine. Do not port its simulation logic into this
project; reading its output files is in scope.

## Source of truth
Before changing topology import, export, connections, IDs, or validation, inspect
the relevant HERSS C++ implementation under `../src/`.

Useful starting points:
- `../src/herss.h`: node types and shared model definitions
- `../src/globalconfig.cpp`: topology loading and node counting
- `../src/herss.cpp`: node setup, connectors, and simulation order
- `../src/reservoir.cpp`, `../src/channel.cpp`, `../src/powerstation.cpp`,
  `../src/pump.cpp`, and `../src/intake.cpp`: node-specific topology fields

The topology format is node-type-specific. Do not assume every node uses the
same header or connector syntax; verify behavior in the corresponding parser.

## IDs and connections
- Preserve node IDs as explicit model data; canvas position is only presentation.
- Validate duplicate IDs and references to missing nodes.
- HERSS simulates nodes in ascending ID order. Pump IDs must precede their
  source and target reservoir IDs. Do not silently renumber nodes.
- Represent connections by meaning (for example, downstream, pump source, or
  pump target), not only as visually drawn arrows.

## Results and plotting
Source of truth for output files: `../src/outputformats.cpp` (CSV and
`manifest.json`) and the node writers in `../src/` (`reservoir.cpp`,
`powerstation.cpp`, `channel.cpp`, `pump.cpp`, `intake.cpp`). Verify column
names and units there before adding them to any catalog. `../dashboard/herssdash`
is a design reference only; do not import it or depend on pandas/Plotly.

- Read results from a run's output folder: `manifest.json` plus node CSV files
  first, legacy `node<ID>_<NAME>.txt` files as fallback. Load columns lazily.
- Keep `results.py` (reader) and `plotconfig.py` (config model, selector
  resolution, aggregation) free of Qt, below `ui/`. The tabs only render them.
- All three tabs share one `Document`. The Configurator and Plot tabs never
  modify the topology.

### Locating results
Do not assume `./output/` exists. `OUTPUTDIR` is set in the HERSS global file
(`../src/globalconfig.cpp`), is used as written (relative to where `herss.exe`
was started) and may be anything. Node files, CSV and the manifest are optional.
- The Configurator has a "Results folder" field with a Browse button. The Plot
  tab uses that folder. It is saved in the plot config as `results_dir`,
  relative to the topology file.
- When the field is empty, fill it by guessing, in this order: `./output/` next
  to the topology; else the `OUTPUTDIR` of a global file in the topology's folder
  whose `TOPOLOGYFILE` names this topology. If both fail, leave it empty and
  ask the user. The explicit setting stays the source of truth.
- Check the folder and show the result next to the field: manifest or legacy
  TXT only, `inputs.topology` of the manifest matching the topology (or, with no
  manifest, node IDs and names matching), and results older than the topology
  file (warn, still allow plotting).
- Put lookup and checks in `results.py` as plain functions (for example
  `find_results_dir`, `check_results`) so they are testable without Qt. Test
  fixtures for: `./output/` present, `OUTPUTDIR` elsewhere, no folder, and a
  stale or mismatched manifest.

### Plot configuration file
- Stored as JSON next to the topology: `<topology file name>.plots.json` (for
  example `topology.txt.plots.json`, like `.layout.json`), with a
  `schema_version`. Never write plot settings into the topology file; HERSS
  stops on unknown keywords in some node types.
- Preserve unknown keys when loading and saving, like the topology rules below.
- Series refer to nodes by explicit node ID and variable name, never by canvas
  position or list index.
- A series entry is either explicit (`node`, `variable`) or a selector
  (`node_type`, optional `node_ids`, `variable`) resolved against the current
  topology, so a new reservoir appears in an "all reservoirs" plot without
  editing the file.
- An optional `aggregate` combines the selected series into one line:
  `none` (default, one line per node), `sum`, `mean`. Allow `sum` only for
  additive units (flow, volume, energy, money, power); reject it for levels
  and fractions (`fr`, `masl`, `m`). Variables missing for some selected
  nodes are skipped with a notice, not an error.
- Entries that point to a missing node or variable are kept in the file and
  flagged in the Configurator. Never drop them silently.

### Panels
- The Plot tab has three panels: `left`, `middle`, `right`. Each panel is a
  vertical stack of plots that share one time axis with all other plots.
- Each plot in the file names its panel. A plot has a title, a kind
  (`timeseries` first; others later) and its series.
- The right panel starts from a generated "system summary" preset (price,
  total production, total reservoir inflow, outflow of the last node). The
  preset is written into the config as ordinary plots, so the user can edit
  or delete them. Do not hard-code the right panel in the Plot tab.

### Results tests
- Add fixture-based tests for the reader (manifest, CSV and legacy TXT, mixed
  time steps) and for config round trips, selector resolution and aggregation.
- GUI tests use offscreen Qt and real key/mouse events. Never delete a widget
  inside its own signal handler (use `takeWidget` and `deleteLater`).

## File safety
- Never silently drop topology fields the editor does not understand.
- Add or update fixture-based tests when changing import or export behavior.
- Keep exported topology compatible with HERSS and test it against a real,
  representative topology file when possible.
  