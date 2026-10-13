#!/usr/bin/env bash
# FINDINGS.md section 112b: screen then joint battery, chained so one detached
# process survives a dropped connection.
set -uo pipefail
cd "$(dirname "$0")/.."
scripts/run_distinct_aggregate_screen.sh data/distinct_aggregate_screen_112b_20261013
scripts/run_distinct_aggregate_joint_battery.sh data/distinct_aggregate_joint_battery_112b_20261013
echo "DA112B_ALL_DONE $(date +%T)" >> data/distinct_aggregate_joint_battery_112b_20261013/driver.log
