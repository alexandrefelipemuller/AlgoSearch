#!/usr/bin/env bash
# FINDINGS.md section 113: unit tests, then screen, then joint battery, in one
# detached process that survives a dropped connection.
set -uo pipefail
cd "$(dirname "$0")/.."
J=data/arg_sentinel_joint_battery_20261014
mkdir -p "$J"
make test > "$J/make_test.log" 2>&1
echo "MAKE_TEST exit=$? $(tail -1 "$J/make_test.log")" >> "$J/driver.log"
scripts/run_arg_sentinel_screen.sh data/arg_sentinel_screen_20261014
scripts/run_arg_sentinel_joint_battery.sh "$J"
echo "AS113_ALL_DONE $(date +%T)" >> "$J/driver.log"
