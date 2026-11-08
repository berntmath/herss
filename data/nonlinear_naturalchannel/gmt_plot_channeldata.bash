#!/usr/bin/env bash

#********************************************************************************
# Project:      The Hydraulic Economic River System Simulator (HERSS)
# Filename:     gmt_plot_channeldata.bash
# Developer:    Bernt Viggo Matheussen (Bernt.Viggo.Matheussen@aenergi.no)
# Organization: Å Energi, www.ae.no#
#
# This software is released under the MIT license:
# Copyright (c) <2024> <Å Energi, Bernt Viggo Matheussen>
#********************************************************************************

# This script plots the output time series for the uTAHPS case using GMT.
# https://www.generic-mapping-tools.org/ 
# gmt --version 6.5.0


set -euo pipefail

OUTBASE="outputseries"
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
NODE1="${SCRIPT_DIR}/node1_CHANNEL_1.txt"

if [[ ! -f "${NODE1}" ]]; then
    printf 'Input file not found: %s\n' "${NODE1}" >&2
    exit 1
fi

read -r START_TIME END_TIME < <(awk '
    NR > 5 {
        time = sprintf("%04d-%02d-%02dT%02d", $1, $2, $3, $4)
        if (!first) first = time
        last = time
    }
    END { print first, last }
' "${NODE1}")

if [[ -z "${START_TIME}" || -z "${END_TIME}" ]]; then
    printf 'No channel data found in %s\n' "${NODE1}" >&2
    exit 1
fi

MAX_FLOW="$(awk 'NR > 5 { if ($5 > max) max = $5; if ($7 > max) max = $7 } END { print max }' "${NODE1}")"
Y_MAX="$(awk -v max="${MAX_FLOW}" 'BEGIN { printf "%.1f", max * 1.1 }')"

gmt begin "${OUTBASE}" png,pdf
    gmt set \
        MAP_FRAME_TYPE plain \
        FONT_TITLE 14p,Helvetica-Bold \
        FONT_LABEL 12p,Helvetica \
        FONT_ANNOT_PRIMARY 9p,Helvetica \
        FORMAT_DATE_MAP yyyy-mm-dd \
        FORMAT_CLOCK_MAP hh:mm \
        TIME_EPOCH 1970-01-01T00:00:00 \
        TIME_UNIT s

    awk 'NR > 5 {
        printf "%04d-%02d-%02dT%02d:00 %s\n", $1, $2, $3, $4, $5
    }' "${NODE1}" | \
    gmt plot \
        -R"${START_TIME}:00"/"${END_TIME}:00"/0/"${Y_MAX}" \
        -JX24c/10c \
        -f0T \
        -W1.2p,blue \
        -BWSen+t"Channel 1: Up_Inflow and tot_outflow" \
        -Bpxa6Hf1h+l"Time" \
        -Bsxa1D+l"Date" \
        -Bpya0.5f0.5+l"Flow [m@+3@+/s]"

    awk 'NR > 5 {
        printf "%04d-%02d-%02dT%02d:00 %s\n", $1, $2, $3, $4, $7
    }' "${NODE1}" | \
    gmt plot \
        -R"${START_TIME}:00"/"${END_TIME}:00"/0/"${Y_MAX}" \
        -JX24c/10c \
        -f0T \
        -W1.2p,red

    gmt legend -DjTR+o0.2c -F+p0.5p+gwhite <<EOF
S 0.3c - 0.6c blue 1.2p 0.7c Up_Inflow
S 0.3c - 0.6c red 1.2p 0.7c tot_outflow
EOF

gmt end show
