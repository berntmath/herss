#!/usr/bin/env bash

#********************************************************************************
# Project:      The Hydraulic Economic River System Simulator (HERSS)
# Filename:     gmt_plot_inputseries.bash
# Developer:    Bernt Viggo Matheussen (Bernt.Viggo.Matheussen@aenergi.no)
# Organization: Å Energi, www.ae.no#
#
# This software is released under the MIT license:
# Copyright (c) <2024> <Å Energi, Bernt Viggo Matheussen>
#********************************************************************************

# This script plots the input time series for the uTAHPS case using GMT.
# https://www.generic-mapping-tools.org/ 
# gmt --version 6.5.0

# It reads the inflow series, price series, and action series from text
# files and creates a plot with three subplots: 
# one for each time series. The x-axis is shared across all subplots
# and represents time, while the y-axes represent the respective values
#  of inflow, price, and action.
# Aborts before starting GMT if any required input is not a regular file.



set -euo pipefail

OUTBASE="inputseries"

INDIR="../data/mini_utahps_hourly/"
INPUT="${INDIR}/inflowseries.txt"

PRICE_INPUT="${INDIR}/pricefile.txt"
ACTION_INPUT="${INDIR}/actions.txt"



for input_file in "${INPUT}" "${PRICE_INPUT}" "${ACTION_INPUT}"; do
    if [[ ! -f "${input_file}" ]]; then
        printf 'Error: required input file not found: %s\n' "${input_file}" >&2
        exit 1
    fi
done





gmt begin "${OUTBASE}" png,pdf
    gmt set \
        MAP_FRAME_TYPE plain \
        FONT_TITLE 14p,Helvetica-Bold \
        FONT_LABEL 12p,Helvetica \
        FONT_ANNOT_PRIMARY 10p,Helvetica \
        FONT_ANNOT_SECONDARY 11p,Helvetica \
        FORMAT_DATE_MAP yyyy-mm-dd \
        FORMAT_CLOCK_MAP hh:mm \
        TIME_EPOCH 1970-01-01T00:00:00 \
        TIME_UNIT s

    awk 'NR > 1 {
        yyyy = substr($1,1,4)
        mm   = substr($1,5,2)
        dd   = substr($1,7,2)
        hh   = substr($1,9,2)
        printf "%s-%s-%sT%s:00 %s\n", yyyy, mm, dd, hh, $2
    }' "${INPUT}" | \
    gmt plot \
        -R2026-05-07T00:00/2026-05-08T23:00/-0.5/30 \
        -JX16c/4c \
        -f0T \
        -W1.0p,blue \
        -BWSen+t"Input timeseries for uTAHPS" \
        -Bpxf1h \
        -Bpya5f1+l"Inflow [m@+3@+/s]"


    awk 'NR > 2 {
        yyyy = substr($1,1,4)
        mm   = substr($1,5,2)
        dd   = substr($1,7,2)
        hh   = substr($1,9,2)
        printf "%s-%s-%sT%s:00 %s\n", yyyy, mm, dd, hh, $2
    }' "${PRICE_INPUT}" | \
    gmt plot \
        -R2026-05-07T00:00/2026-05-08T23:00/0/110 \
        -JX16c/4c -Y-4.4c \
        -f0T \
        -W1.0p,red \
        -BWSen \
        -Bpxf1h \
        -Bpya20f10+l"Price [Euro/MWh]"


    awk 'NR > 2 {
        yyyy = substr($1,1,4)
        mm   = substr($1,5,2)
        dd   = substr($1,7,2)
        hh   = substr($1,9,2)
        printf "%s-%s-%sT%s:00 %s\n", yyyy, mm, dd, hh, $2
    }' "${ACTION_INPUT}" | \
    gmt plot \
        -R2026-05-07T00:00/2026-05-08T23:00/-0.05/1.2 \
        -JX16c/4c -Y-4.4c \
        -f0T \
        -W1.0p,black \
        -BWSen \
        -Bpxa6Hf1h+l"Time" \
        -Bsxa1D+l"Date" \
        -Bpya.2f.2+l"Action [fr]"

gmt end show
