#!/home/bernt/py_env/herss_env/bin/python


import pandas as pd
import matplotlib.pyplot as plt
from pathlib import Path


base = Path(__file__).parent

# --- inflow (node 0 only) ---
inflow = pd.read_csv(base / "inflowseries.txt", sep="\t")
inflow["Date"] = pd.to_datetime(inflow["Date_NodeID"], format="%Y%m%d")
inflow = inflow.rename(columns={"0": "inflow_m3s"})

# --- price (skip RESTPRICE marker line) ---
price = pd.read_csv(base / "pricefile.txt", sep="\t", skiprows=1)
price["Date"] = pd.to_datetime(price["Date"], format="%Y%m%d%H")

# --- simulation node output (multiple node blocks) ---
def read_nodeout(path):
    blocks, name, cols, rows = {}, None, None, []
    for line in Path(path).read_text().splitlines():
        if line.startswith("Node:"):
            if name: blocks[name] = pd.DataFrame(rows, columns=cols).apply(pd.to_numeric)
            name = line.split("name=")[1].strip(); rows = []; cols = None
        elif line.startswith("time"):
            cols = line.split()
        elif line.strip():
            rows.append(line.split())
    if name: blocks[name] = pd.DataFrame(rows, columns=cols).apply(pd.to_numeric)
    return blocks

nodes = read_nodeout(base / "ad_nodeout.txt")

fig, ax = plt.subplots(4, 1, figsize=(12, 9), sharex=True)

ax[0].plot(inflow["Date"], inflow["inflow_m3s"]); ax[0].set_ylabel("Inflow m³/s")
ax[1].plot(price["Date"], price["Price"], color="orange"); ax[1].set_ylabel("Price")

print(nodes.keys()  )

vanarosen_df = nodes["VANAROSEN"]
vanarosen_df["time_index"] = range(len(vanarosen_df))

ax[2].plot(price["Date"], vanarosen_df["up_inflow_m3s"], label="up_inflow_m3s")
ax[2].plot(price["Date"], vanarosen_df["tot_outflow_m3s"], label="tot_outflow_m3s")
ax[2].set_ylabel("Flow m³/s")
ax[2].legend()

# ['HJELLE', 'SVOLETJONN', 'VANAROSEN']

hjelle_df = nodes["HJELLE"]
ax[3].plot(price["Date"], hjelle_df["res_fr"], label="res_fr")
ax[3].axhline(y=0.0, color="red", linestyle="--", linewidth=1)
ax[3].axhline(y=1.0, color="red", linestyle="--", linewidth=1)
ax[3].set_ylim(-0.5, 1.2)
ax[3].set_ylabel("res_fr")
ax[3].legend()




#print(nodes["HJELLE"])
# ict_keys(['HJELLE', 'SVOLETJONN', 'VANAROSEN'])



plt.tight_layout(); plt.savefig(base / "ad_plot.png"); 
plt.show()
