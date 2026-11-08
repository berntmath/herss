#!/usr/bin/bash

set -euo pipefail

# BVM 26 Sep, 2026 

cd "$(dirname "${BASH_SOURCE[0]}")"

K=(0 6 12 18 24)
n_linres=(1 2 3 4)
HERSS="../../src/herss.exe"

topology_backup=$(mktemp)
cp topology.txt "$topology_backup"
trap 'cp "$topology_backup" topology.txt; rm -f "$topology_backup"' EXIT

for travel_time in "${K[@]}"; do
	for linres_count in "${n_linres[@]}"; do
		printf 'Running K=%s n_linres=%s\n' "$travel_time" "$linres_count"
		sed -i -E "s/^N_CASCADE_LINRES[[:space:]]+.*/N_CASCADE_LINRES ${linres_count}/" topology.txt
		sed -i -E "s/^K_TRAVELTIME_HOURS[[:space:]]+.*/K_TRAVELTIME_HOURS ${travel_time}/" topology.txt
		"$HERSS" global.txt
		cp node1_CHANNEL_1.txt "node1_CHANNEL_1_K${travel_time}_n_linres${linres_count}.txt"
	done
done


# N_CASCADE_LINRES 3
# K_TRAVELTIME_HOURS 12
