#!/usr/bin/env bash

# Run with: bash test_gmt_plot_inputseries.bash
# GMT is stubbed so these checks do not open a viewer or create plots.
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
TEST_DIR="$(mktemp -d)"
trap 'rm -rf -- "${TEST_DIR}"' EXIT

mkdir -p "${TEST_DIR}/plots" "${TEST_DIR}/data/mini_utahps_hourly" "${TEST_DIR}/bin"
cp "${SCRIPT_DIR}/gmt_plot_inputseries.bash" "${TEST_DIR}/plots/"
export GMT_TEST_LOG="${TEST_DIR}/gmt.log"
cat > "${TEST_DIR}/bin/gmt" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' "$*" >> "${GMT_TEST_LOG}"
if [[ "$1" == plot ]]; then
    cat > /dev/null
fi
EOF
chmod +x "${TEST_DIR}/bin/gmt"
export PATH="${TEST_DIR}/bin:${PATH}"
cd "${TEST_DIR}/plots"

inputs=(inflowseries.txt pricefile.txt actions.txt)
for input in "${inputs[@]}"; do
    printf 'header\nheader\n2026050700 1\n' > "../data/mini_utahps_hourly/${input}"
done

check_abort() {
    local expected_file="$1" status=0
    rm -f -- "${GMT_TEST_LOG}"
    bash ./gmt_plot_inputseries.bash > "${TEST_DIR}/stdout" 2> "${TEST_DIR}/stderr" || status=$?
    if [[ "${status}" -ne 1 ]] ||
        ! grep -Fq "Error: required input file not found: ../data/mini_utahps_hourly//${expected_file}" "${TEST_DIR}/stderr" ||
        [[ -e "${GMT_TEST_LOG}" ]]; then
        printf 'FAIL: expected abort before GMT for %s (exit %s)\n' "${expected_file}" "${status}" >&2
        cat "${TEST_DIR}/stderr" >&2
        exit 1
    fi
}

for input in "${inputs[@]}"; do
    mv "../data/mini_utahps_hourly/${input}" "${TEST_DIR}/saved-input"
    check_abort "${input}"
    mv "${TEST_DIR}/saved-input" "../data/mini_utahps_hourly/${input}"
    printf 'PASS: missing %s aborts before GMT\n' "${input}"
done

mv ../data/mini_utahps_hourly/actions.txt "${TEST_DIR}/saved-input"
mkdir ../data/mini_utahps_hourly/actions.txt
check_abort actions.txt
rmdir ../data/mini_utahps_hourly/actions.txt
mv "${TEST_DIR}/saved-input" ../data/mini_utahps_hourly/actions.txt
printf 'PASS: directory in place of input aborts before GMT\n'

bash ./gmt_plot_inputseries.bash
[[ "$(grep -c '^plot ' "${GMT_TEST_LOG}")" -eq 3 ]]
grep -Fxq 'begin inputseries png,pdf' "${GMT_TEST_LOG}"
grep -Fxq 'end show' "${GMT_TEST_LOG}"
printf 'PASS: all inputs present runs all three plots and completes\n'
