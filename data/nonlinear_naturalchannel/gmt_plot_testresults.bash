#!/usr/bin/env bash

#********************************************************************************
# Project:      The Hydraulic Economic River System Simulator (HERSS)
# Filename:     gmt_plot_testresults.bash
# Developer:    Bernt Viggo Matheussen (Bernt.Viggo.Matheussen@aenergi.no)
# Organization: Å Energi, www.ae.no#
#
# This software is released under the MIT license:
# Copyright (c) <2024> <Å Energi, Bernt Viggo Matheussen>
#********************************************************************************

# Plot Up_Inflow and all tot_outflow simulation results using GMT.
# https://www.generic-mapping-tools.org/ 
# gmt --version 6.5.0


set -euo pipefail

OUTBASE="channel_testresults"
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

RESULT_FILES=( "${SCRIPT_DIR}"/node1_CHANNEL_1_K*_n_linres*.txt )
if (( ${#RESULT_FILES[@]} == 0 )); then
    printf 'No simulation result files found in %s\n' "${SCRIPT_DIR}" >&2
    exit 1
fi

COLORS=(
    '#2166ac' '#b2182b' '#1b7837' '#e08214' '#762a83'
    '#018571' '#c51b7d' '#a6611a' '#2c7fb8' '#5aae61'
    '#4d4d4d' '#de77ae' '#4393c3' '#d6604d' '#80cdc1'
    '#9970ab' '#fdb863' '#92c5de' '#8c510a' '#35978f'
)

if (( ${#RESULT_FILES[@]} > ${#COLORS[@]} )); then
    printf 'Found %d result files, but only %d plot colors are configured\n' \
        "${#RESULT_FILES[@]}" "${#COLORS[@]}" >&2
    exit 1
fi

FIRST_FILE="${RESULT_FILES[0]}"
read -r START_TIME END_TIME < <(awk '
    NR > 5 {
        time = sprintf("%04d-%02d-%02dT%02d", $1, $2, $3, $4)
        if (!first) first = time
        last = time
    }
    END { print first, last }
' "${FIRST_FILE}")

if [[ -z "${START_TIME}" || -z "${END_TIME}" ]]; then
    printf 'No channel data found in %s\n' "${FIRST_FILE}" >&2
    exit 1
fi

MAX_FLOW="$(awk '
    FNR > 5 {
        if ($7 > max) max = $7
        if (FILENAME == ARGV[1] && $5 > max) max = $5
    }
    END { print max + 0 }
' "${RESULT_FILES[@]}")"
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
    }' "${FIRST_FILE}" | \
    gmt plot \
        -R"${START_TIME}:00"/"${END_TIME}:00"/0/"${Y_MAX}" \
        -JX24c/10c \
        -f0T \
        -W1.2p,black \
        -BWSen+t"Channel 1: Up_Inflow and tot_outflow by simulation" \
        -Bpxa6Hf1h+l"Time" \
        -Bsxa1D+l"Date" \
        -Bpya0.5f0.5+l"Flow [m@+3@+/s]"

    for index in "${!RESULT_FILES[@]}"; do
        result_file="${RESULT_FILES[index]}"
        color="${COLORS[index]}"
        awk 'NR > 5 {
            printf "%04d-%02d-%02dT%02d:00 %s\n", $1, $2, $3, $4, $7
        }' "${result_file}" | \
        gmt plot \
            -R"${START_TIME}:00"/"${END_TIME}:00"/0/"${Y_MAX}" \
            -JX24c/10c \
            -f0T \
            -W1.2p,"${color}"
    done

    gmt set FONT_ANNOT_PRIMARY 7p,Helvetica
    {
        printf 'N 2\n'
        printf 'S 0.3c - 0.6c black 1.2p 0.7c Up_Inflow (first file)\n'
        for index in "${!RESULT_FILES[@]}"; do
            result_file="${RESULT_FILES[index]}"
            color="${COLORS[index]}"
            label="${result_file##*/node1_CHANNEL_1_}"
            label="${label%.txt}"
            travel_time="${label%%_n_linres*}"
            linres_count="${label##*_n_linres}"
            printf 'S 0.3c - 0.6c %s 1.2p 0.7c K=%s, n_linres=%s\n' \
                "${color}" "${travel_time#K}" "${linres_count}"
        done
    } | gmt legend -DJMR+o0.3c+w10c -F+p0.5p+gwhite

gmt end show
