#!/home/bernt/py_env/herss_env/bin/python
# -*- coding: utf-8 -*-


from datetime import datetime
from pathlib import Path

import plotly.graph_objects as go


#DATA_FILE = Path(__file__).with_name("node1_CHANNEL_1_K12_n_linres4.txt")
DATA_FILE = Path(__file__).with_name("node1_CHANNEL_1.txt")





def read_channel_data():
	timestamps = []
	up_inflow = []
	tot_outflow = []

	for line in DATA_FILE.read_text(encoding="utf-8").splitlines():
		fields = line.split()
		if len(fields) < 7 or not fields[0].isdigit():
			continue

		timestamps.append(datetime(*(int(value) for value in fields[:4])))
		up_inflow.append(float(fields[4]))
		tot_outflow.append(float(fields[6]))

	return timestamps, up_inflow, tot_outflow

def build_figure():
	timestamps, up_inflow, tot_outflow = read_channel_data()
	figure = go.Figure()
	figure.add_scatter(x=timestamps, y=up_inflow, mode="lines", name="Up_Inflow")
	figure.add_scatter(x=timestamps, y=tot_outflow, mode="lines", name="tot_outflow")
	figure.update_layout(
		title="Channel 1: Up_Inflow and tot_outflow",
		xaxis_title="Time",
		yaxis_title="Flow (m3/s)",
		hovermode="x unified",
	)
	return figure


if __name__ == "__main__":
	output_file = Path(__file__).with_name("channel_plot.html")
	build_figure().write_html(output_file, include_plotlyjs=True, auto_open=True)


