#!/bin/sh
# Smoke test driver: every fixture must --dump without crashing and produce
# non-empty output. Catches segfaults/regressions across the whole pipeline.
# Usage: run_smoke.sh <xs-binary> <fixtures-dir>

set -u
XS="$1"
FIX="$2"

fail=0
for html in "$FIX"/*.html; do
    [ -e "$html" ] || continue
    name=$(basename "$html")
    out=$("$XS" --dump --width=800 "file://$html" 2>/dev/null)
    rc=$?
    if [ $rc -ne 0 ]; then
        echo "SMOKE FAIL $name (exit $rc)"
        fail=1
    elif [ -z "$out" ]; then
        echo "SMOKE FAIL $name (empty output)"
        fail=1
    else
        echo "SMOKE PASS $name"
    fi
done

exit $fail
