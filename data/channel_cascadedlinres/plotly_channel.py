#!/home/bernt/py_env/herss_env/bin/python
# -*- coding: utf-8 -*-


from datetime import datetime
from pathlib import Path

import plotly.graph_objects as go


DATA_PATTERN = "node1_CHANNEL_1_K*_n_linres*.txt"


def read_channel_data():
	data_files = sorted(
		Path(__file__).parent.glob(DATA_PATTERN),
		key=lambda path: (
			int(path.stem.split("_K", 1)[1].split("_n_linres", 1)[0]),
			int(path.stem.rsplit("_n_linres", 1)[1]),
		),
	)
	if not data_files:
		raise FileNotFoundError(f"No data files match {DATA_PATTERN!r}")

	series = []
	for data_file in data_files:
		timestamps = []
		up_inflow = []
		tot_outflow = []

		for line in data_file.read_text(encoding="utf-8").splitlines():
			fields = line.split()
			if len(fields) < 7 or not fields[0].isdigit():
				continue

			timestamps.append(datetime(*(int(value) for value in fields[:4])))
			up_inflow.append(float(fields[4]))
			tot_outflow.append(float(fields[6]))

		if not timestamps:
			raise ValueError(f"No data rows found in {data_file.name}")
		series.append((data_file, timestamps, up_inflow, tot_outflow))

	reference_timestamps = series[0][1]
	reference_inflow = series[0][2]
	for data_file, timestamps, up_inflow, _ in series[1:]:
		if timestamps != reference_timestamps or up_inflow != reference_inflow:
			raise ValueError(
				f"Timestamps or Up_Inflow differ from {series[0][0].name} "
				f"in {data_file.name}"
			)

	outflow_series = [
		(
			f"K={data_file.stem.split('_K', 1)[1].split('_n_linres', 1)[0]}, "
			f"n_linres={data_file.stem.rsplit('_n_linres', 1)[1]}",
			timestamps,
			tot_outflow,
		)
		for data_file, timestamps, _, tot_outflow in series
	]
	return reference_timestamps, reference_inflow, outflow_series


def build_figure():
	timestamps, up_inflow, outflow_series = read_channel_data()
	figure = go.Figure()
	figure.add_scatter(
		x=timestamps,
		y=up_inflow,
		mode="lines",
		name="Up_Inflow",
		line={"color": "black", "width": 4},
	)
	for name, timestamps, tot_outflow in outflow_series:
		figure.add_scatter(
			x=timestamps,
			y=tot_outflow,
			mode="lines",
			name=name,
		)
	figure.update_layout(
		title="Channel 1: Up_Inflow and tot_outflow comparisons",
		xaxis_title="Time",
		yaxis_title="Flow (m3/s)",
		hovermode="x unified",
		showlegend=True,
	)
	return figure


if __name__ == "__main__":
	output_file = Path(__file__).with_name("channel_plot.html")
	build_figure().write_html(output_file, include_plotlyjs=True, auto_open=True)


