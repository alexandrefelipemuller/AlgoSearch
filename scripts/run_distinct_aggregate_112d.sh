#!/usr/bin/env bash
# FINDINGS.md section 112d: unit tests, then screen, then joint battery, in one
# detached process that survives a dropped connection.
set -uo pipefail
cd "$(dirname "$0")/.."
J=data/distinct_aggregate_joint_battery_112d_20261013
mkdir -p "$J"
make test > "$J/make_test.log" 2>&1
echo "MAKE_TEST exit=$? $(tail -1 "$J/make_test.log")" >> "$J/driver.log"
scripts/run_distinct_aggregate_screen_112d.sh data/distinct_aggregate_screen_112d_20261013
scripts/run_distinct_aggregate_joint_battery_112d.sh "$J"
echo "DA112D_ALL_DONE $(date +%T)" >> "$J/driver.log"
