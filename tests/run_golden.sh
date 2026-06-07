#!/bin/sh
# Golden test driver (FEAT-003 / FEAT-021).
# Usage: run_golden.sh <xs-binary> <fixtures-dir> <golden-dir>
#
# For each <golden-dir>/<name>.txt, runs:
#   <xs> --dump --width=800 file://<fixtures-dir>/<name>.html
# and diffs against the golden. Exits non-zero on any mismatch.

set -eu

XS="$1"
FIX="$2"
GOLD="$3"

fail=0
for g in "$GOLD"/*.txt; do
    [ -e "$g" ] || continue
    name=$(basename "$g" .txt)
    html="$FIX/$name.html"
    if [ ! -f "$html" ]; then
        echo "MISSING FIXTURE: $html"
        fail=1
        continue
    fi
    actual=$("$XS" --dump --width=800 "file://$html" 2>/dev/null)
    expected=$(cat "$g")
    if [ "$actual" = "$expected" ]; then
        echo "GOLDEN PASS $name"
    else
        echo "GOLDEN FAIL $name"
        printf '%s\n' "$expected" > "/tmp/xs_golden_expected_$name.txt"
        printf '%s\n' "$actual"   > "/tmp/xs_golden_actual_$name.txt"
        diff "/tmp/xs_golden_expected_$name.txt" "/tmp/xs_golden_actual_$name.txt" || true
        fail=1
    fi
done

exit $fail
