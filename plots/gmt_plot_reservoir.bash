#!/usr/bin/env bash

#********************************************************************************
# Project:      The Hydraulic Economic River System Simulator (HERSS)
# Filename:     gmt_plot_reservoir.bash
# Developer:    Bernt Viggo Matheussen (Bernt.Viggo.Matheussen@aenergi.no)
# Organization: Å Energi, www.ae.no
#
# This software is released under the MIT license:
# Copyright (c) <2024> <Å Energi, Bernt Viggo Matheussen>
#********************************************************************************

# This script uses Generic Mapping Tools 
# https://www.generic-mapping-tools.org/ 
# gmt --version 6.5.0

# It reads the node output file for a reservoir.
# It creates a plot with three subplots: 
# The top plot is the inflow series. 
# The second plot is hatch outflow, overflow and total outflow.
# The lowest plot is the reservoir filling level in percent of the maximum filling level.
# Data rows are identified by their four-digit year, independent of header length.



set -euo pipefail

OUTBASE="res_a"

INDIR="../data/res_casc_A/output/"

# node0_RES_A.txt
# node1_RES_B.txt
# node2_CHAN2.txt
# node3_CHAN3.txt
# node4_PSTAT_B.txt  
# node5_CHANNEL5.txt

# outstate.txt
# riversystem_ResCascA_output.txt
# reservoirs_ResCascA_out.txt

INPUT="${INDIR}/node0_RES_A.txt"


for input_file in "${INPUT}" ; do
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
        FORMAT_TIME_SECONDARY_MAP abbreviated \
        FORMAT_DATE_MAP "o yyyy" \
        TIME_EPOCH 1970-01-01T00:00:00 \
        TIME_UNIT s

    # RESERVOIR node 0 RES_A
    # reservoir_init_fr= 0.46000
    # yyyy mm dd hh [m3/s] [Euro/MWh] [fr] [m3/s] [Mm3] [masl] [fr] [Euro]         [m3/s]     [m3/s]    [m3/s]   [m3/s]    [m3/s] 
    # yyyy mm dd hh Inflow Price Action Up_Inflow Res_Mm3 Res_masl Res_fr lrw_cost tunnelflow hatchflow overflow auto_qmin tot_outflow
    # 2022 9 1 0 1.5000 24.8800 0.7000 0.0000 4.9067 851.7259 0.4341 0.0000 0.0000 4.2000 0.0000 0.0000 4.2000 
    # 2022 9 2 0 1.3500 25.8500 0.8000 0.0000 4.6086 851.4172 0.4010 0.0000 0.0000 4.8000 0.0000 0.0000 4.8000 


    # Plot inflow 
    awk '$1 ~ /^[0-9][0-9][0-9][0-9]$/ {
        yyyy = $1; mm = $2; dd = $3; hh = 12
        printf "%04d-%02d-%02dT%02d:00 %s\n", yyyy, mm, dd, hh, $5
    }' "${INPUT}" | \
    gmt plot \
        -R2022-09-01T00:00/2022-10-01T00:00/-0.5/30 \
        -JX16c/4c \
        -f0T \
        -W1.0p,blue \
        -BWSen+t"Testdata Res_Casc_A" \
        -Bpxf1d \
        -Bpya5f1+l"Inflow [m@+3@+/s]" \


    # After the inflow plot:
    echo "(a)" | gmt text -F+cTL+f11p,Helvetica-Bold -D0.15c/-0.2c


    # Plot hatch outflow 
    awk '$1 ~ /^[0-9][0-9][0-9][0-9]$/ {
        yyyy = $1; mm = $2; dd = $3; hh = 12
        printf "%04d-%02d-%02dT%02d:00 %s\n", yyyy, mm, dd, hh, $14
    }' "${INPUT}" | \
    gmt plot \
        -R2022-09-01T00:00/2022-10-01T00:00/-0.5/30 \
        -JX16c/4c -Y-4.6c \
        -f0T \
        -W1.0p,green \
        -BWSen \
        -Bpxf1d \
        -Bpya5f1+l"Outflow [m@+3@+/s]" \
 

    # Plot overflow 
    awk '$1 ~ /^[0-9][0-9][0-9][0-9]$/ {
        yyyy = $1; mm = $2; dd = $3; hh = 12
        printf "%04d-%02d-%02dT%02d:00 %s\n", yyyy, mm, dd, hh, $15
    }' "${INPUT}" | \
    gmt plot \
        -R2022-09-01T00:00/2022-10-01T00:00/-0.5/30 \
        -JX16c/4c \
        -f0T \
        -W1.0p,red \
        -BWSen \

    # After the outflow panel (hatch + overflow):
    echo "(b)" | gmt text -F+cTL+f11p,Helvetica-Bold -D0.15c/-0.2c

    # Legend for middle panel
    gmt legend -DjTR+w3.2c+o0.1c/0.1c -F+gwhite << EOF
S 0.0c - 0.5c - 1.0p,green 0.4c Hatch outflow
S 0.0c - 0.5c - 1.0p,red   0.4c Overflow
EOF


    # Reservoir filling in percent. 
    awk '$1 ~ /^[0-9][0-9][0-9][0-9]$/ {
        yyyy = $1; mm = $2; dd = $3; hh = 12
        printf "%04d-%02d-%02dT%02d:00 %s\n", yyyy, mm, dd, hh, 100*$11
    }' "${INPUT}" | \
    gmt plot \
        -R2022-09-01T00:00/2022-10-01T00:00/-2.0/150 \
        -JX16c/4c -Y-4.6c \
        -f0T \
        -W1.0p,blue \
        -BWSen \
        -Bpxa1d \
        -Bpya20f20+l"Reservoir filling [%]" \
        -Bsxa1O

    # Plot black dotted line at 100% filling level.
    awk '$1 ~ /^[0-9][0-9][0-9][0-9]$/ {
        yyyy = $1; mm = $2; dd = $3; hh = 12
        printf "%04d-%02d-%02dT%02d:00 %s\n", yyyy, mm, dd, hh, 100
    }' "${INPUT}" | \
    gmt plot \
        -R2022-09-01T00:00/2022-10-01T00:00/-2.0/150 \
        -JX16c/4c \
        -f0T \
        -W1.0p,black,. \
        -BWSen

    # After the reservoir filling plot:
    echo "(c)" | gmt text -F+cTL+f11p,Helvetica-Bold -D0.15c/-0.2c


gmt end show
