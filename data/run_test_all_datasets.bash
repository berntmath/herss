#!/usr/bin/bash

echo "Testing all datasets that are part of HERSS repo"






# Each dataset runs in a temp copy so tracked input/output files stay untouched.
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HERSS="$ROOT/../src/herss.exe"
TIMEOUT=${TIMEOUT:-600}
REPORT_DIR=${REPORT_DIR:-/tmp/herss_dataset_test}

rm -rf "$REPORT_DIR"; mkdir -p "$REPORT_DIR"

pass=0; fail=0; failed=()

for dir in "$ROOT"/*/; do
    name=$(basename "$dir")
    global=$(ls "$dir"global*.txt 2>/dev/null | head -n1)
    if [ -z "$global" ]; then
        echo "SKIP  $name (no global*.txt)"
        continue
    fi

    work=$(mktemp -d)
    cp -r "$dir". "$work"/
    rm -f "$work"/herss_*.log   # stale logs committed in some datasets
    mkdir -p "$work/output"

    ( cd "$work" && timeout "$TIMEOUT" "$HERSS" "$(basename "$global")" > run.out 2>&1 )
    rc=$?

    warns=$(cat "$work"/herss_*.log 2>/dev/null | grep -c '\[WRN\]')
    cp "$work/run.out" "$REPORT_DIR/$name.out"
    cp "$work"/herss_*.log "$REPORT_DIR/$name.log" 2>/dev/null

    if [ $rc -eq 0 ]; then
        echo "PASS  $name (warnings: $warns)"
        pass=$((pass+1))
    else
        echo "FAIL  $name (exit $rc, warnings: $warns)"
        tail -n 6 "$work/run.out" | sed 's/^/      /'
        fail=$((fail+1)); failed+=("$name")
    fi
    rm -rf "$work"
done

echo "Passed: $pass  Failed: $fail  ${failed[*]}"
echo "Logs and screen output in $REPORT_DIR"
[ $fail -eq 0 ]

