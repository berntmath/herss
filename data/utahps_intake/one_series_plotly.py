#!/home/bernt/py_env/herss_env/bin/python
# -*- coding: utf-8 -*-


from datetime import datetime
from pathlib import Path

import plotly.graph_objects as go
from plotly.subplots import make_subplots


DATA_FILE = Path(__file__).with_name("node0_G_INTAKE.txt")


def read_channel_data():
	series = {
		"inflow_m3s": [],
		"storage_Mm3": [],
		"fill_fraction": [],
		"hatch_release_m3s": [],
		"captured_m3s": [],
		"flooding_m3s": [],
		"total_outflow_m3s": [],
	}
	timestamps = []

	for line in DATA_FILE.read_text(encoding="utf-8").splitlines():
		fields = line.split()
		if len(fields) < 7 or not fields[0].isdigit():
			continue

		timestamps.append(datetime(*(int(value) for value in fields[:4])))
		for key, value in zip(series, fields[4:11]):
			series[key].append(float(value))

	return timestamps, series

def build_figure():
	timestamps, series = read_channel_data()
	figure = make_subplots(
		rows=3,
		cols=1,
		shared_xaxes=True,
		vertical_spacing=0.06,
		subplot_titles=("Inflow", "Storage and Fill Fraction", "Intake and Outflow Rates"),
		specs=[[{}], [{"secondary_y": True}], [{}]],
	)
	figure.add_scatter(
		x=timestamps, y=series["inflow_m3s"], mode="lines", name="Inflow",
		row=1, col=1,
	)
	figure.add_scatter(
		x=timestamps, y=series["storage_Mm3"], mode="lines", name="Storage",
		row=2, col=1, secondary_y=False,
	)
	figure.add_scatter(
		x=timestamps, y=series["fill_fraction"], mode="lines", name="Fill fraction",
		row=2, col=1, secondary_y=True,
	)
	for key, name in (
		("hatch_release_m3s", "Hatch release"),
		("captured_m3s", "Captured flow"),
		("flooding_m3s", "Flooding"),
		("total_outflow_m3s", "Total outflow"),
	):
		figure.add_scatter(
			x=timestamps, y=series[key], mode="lines", name=name,
			row=3, col=1,
		)
	figure.update_yaxes(title_text="Flow (m3/s)", row=1, col=1)
	figure.update_yaxes(title_text="Storage (Mm3)", row=2, col=1, secondary_y=False)
	figure.update_yaxes(title_text="Fill fraction", row=2, col=1, secondary_y=True)
	figure.update_yaxes(title_text="Flow (m3/s)", row=3, col=1)
	figure.update_layout(
		title="UTAHPS Intake Node 0 Timeseries",
		xaxis3_title="Time",
		hovermode="x unified",
		height=900,
	)
	return figure


if __name__ == "__main__":
	output_file = Path(__file__).with_name("intake_plot.html")
	build_figure().write_html(output_file, include_plotlyjs=True, auto_open=True)


