#!/usr/bin/env bash

# Run with: bash test_gmt_plot_reservoir.bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
TEST_DIR="$(mktemp -d)"
trap 'rm -rf -- "${TEST_DIR}"' EXIT
mkdir -p "${TEST_DIR}/plots" "${TEST_DIR}/data/res_casc_A/output" "${TEST_DIR}/bin"
cp "${SCRIPT_DIR}/gmt_plot_reservoir.bash" "${TEST_DIR}/plots/"
export GMT_TEST_DIR="${TEST_DIR}"
cat > "${TEST_DIR}/bin/gmt" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
if [[ "$1" == plot ]]; then
    index="$(cat "${GMT_TEST_DIR}/count")"
    cat > "${GMT_TEST_DIR}/series-${index}"
    printf '%s\n' "$((index + 1))" > "${GMT_TEST_DIR}/count"
elif [[ "$1" == text || "$1" == legend ]]; then
    cat > /dev/null
fi
EOF
chmod +x "${TEST_DIR}/bin/gmt"
export PATH="${TEST_DIR}/bin:${PATH}"
cd "${TEST_DIR}/plots"

for header_format in legacy current; do
    {
        printf 'RESERVOIR node 0 RES_A\nreservoir_init_fr= 0.86000\n'
        if [[ "${header_format}" == current ]]; then
            printf 'Filling at HRW [Mm3] = 10.00000\n'
            printf 'Filling at LRW [Mm3] = 1.00000\n'
            printf 'Active reservoir capacity [Mm3] = 9.00000\n'
        fi
        printf 'yyyy mm dd hh [m3/s] [Euro/MWh] [fr]\n'
        printf 'yyyy mm dd hh Inflow Price Action Up_Inflow Res_Mm3 Res_masl Res_fr lrw_cost tunnelflow hatchflow overflow auto_qmin tot_outflow\n'
        printf '2022 9 1 0 1.5000 24.8800 0.7000 0.0000 8.5067 855.4537 0.8341 0.0000 0.0000 4.2000 0.0000 0.0000 4.2000\n'
        printf '2022 9 2 0 1.3500 25.8500 0.8000 0.0000 8.2086 855.1450 0.8010 0.0000 0.0000 4.8000 0.0000 0.0000 4.8000\n'
    } > ../data/res_casc_A/output/node0_RES_A.txt

    printf '0\n' > "${TEST_DIR}/count"
    bash ./gmt_plot_reservoir.bash
    [[ "$(cat "${TEST_DIR}/count")" -eq 5 ]]
    first_values=(1.5000 4.2000 0.0000 83.41 100)
    second_values=(1.3500 4.8000 0.0000 80.1 100)
    for index in 0 1 2 3 4; do
        printf '2022-09-01T12:00 %s\n2022-09-02T12:00 %s\n' \
            "${first_values[index]}" "${second_values[index]}" > "${TEST_DIR}/expected"
        diff -u "${TEST_DIR}/expected" "${TEST_DIR}/series-${index}"
    done
    printf 'PASS: %s headers produce correct data in all five series\n' "${header_format}"
done
