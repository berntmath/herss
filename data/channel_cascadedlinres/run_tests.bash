#!/usr/bin/bash

set -euo pipefail

# BVM 26 Sep, 2026 

cd "$(dirname "${BASH_SOURCE[0]}")"

K=(8 12)
n_linres=(10 20 30)
HERSS="../../src/herss.exe"

topology_backup=$(mktemp)
state_backup=$(mktemp)
cp topology.txt "$topology_backup"
cp start_state.txt "$state_backup"
trap 'cp "$topology_backup" topology.txt; cp "$state_backup" start_state.txt; rm -f "$topology_backup" "$state_backup"' EXIT

for travel_time in "${K[@]}"; do
	for linres_count in "${n_linres[@]}"; do
		printf 'Running K=%s n_linres=%s\n' "$travel_time" "$linres_count"
		sed -i -E "s/^N_CASCADE_LINRES[[:space:]]+.*/N_CASCADE_LINRES ${linres_count}/" topology.txt
		sed -i -E "s/^K_TRAVELTIME_HOURS[[:space:]]+.*/K_TRAVELTIME_HOURS ${travel_time}/" topology.txt
		awk -v count="$linres_count" '
			$1 == "NODE" && $2 == "CHANNEL" && $3 == "1" && $4 == "CHANNEL_1" {
				if (found++) {
					print "Duplicate CHANNEL_1 state entries" > "/dev/stderr"
					exit 1
				}
				method = 5
				if ($method == "METHOD") method++
				if ($(method) != "A_CASCADEDLINRES" || $(method + 1) != "N") {
					print "CHANNEL_1 start state is not an A_CASCADEDLINRES state" > "/dev/stderr"
					exit 1
				}
				total = 0
				for (i = method + 3; i <= NF; i++) total += $i
				for (i = 1; i <= method; i++) printf "%s ", $i
				printf "N %d", count
				for (i = 1; i <= count; i++) printf " %.12g", total / count
				print ""
				next
			}
			{ print }
			END {
				if (!found) {
					print "CHANNEL_1 start state not found" > "/dev/stderr"
					exit 1
				}
			}
		' "$state_backup" > start_state.txt
		"$HERSS" global.txt
		cp node1_CHANNEL_1.txt "node1_CHANNEL_1_K${travel_time}_n_linres${linres_count}.txt"
	done
done


# N_CASCADE_LINRES 3
# K_TRAVELTIME_HOURS 12
