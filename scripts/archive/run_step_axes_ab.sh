#!/usr/bin/env bash
# FINDINGS.md section 52: default (S0) vs default + --combinator-step (S1) on
# the post-freeze step problems, the design targets, controls and a
# no-dilution set. One --benchmark --benchmark-seeds 1 process per run.
#
# Usage: scripts/run_step_axes_ab.sh output_root
# Env: SEEDS (15), SEED_BASE (8000), WALL (180), JOBS (8).

set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:?output_root}"
SEEDS="${SEEDS:-15}"; SEED_BASE="${SEED_BASE:-8000}"; WALL="${WALL:-180}"; JOBS="${JOBS:-8}"
mkdir -p "$D"
JOBS_LIST="max_rise:8 min_step:8 count_runs:8 count_ascending_runs:8 count_distinct:8 min_gap:8 max_gap_sorted:8 \
max_subarray_sum:8 longest_increasing_run:8 trapping_rain_water:8 is_sorted:8 count_descents:8 \
strictly_increasing:8 has_adjacent_equal:8 sorting:5"
for job in $JOBS_LIST; do for arm in S0 S1; do for s in $(seq 0 $((SEEDS - 1))); do
  echo "$arm ${job%%:*} ${job##*:} $((SEED_BASE + s))"
done; done; done | xargs -P "$JOBS" -L1 sh -c '
  arm=$0; problem=$1; maxn=$2; seed=$3; flag=""
  [ "$arm" = S1 ] && flag=--combinator-step
  timeout '"$WALL"' ./algosearch --problem $problem --max-n $maxn --seed $seed --threads 1 --beam-width 500 \
    --candidates 1500 --max-program-size 150 --step-limit 5000 $flag --benchmark --benchmark-seeds 1 \
    > '"$D"'/log_${arm}_${problem}_${seed}.txt 2>&1
  echo "$arm $problem $seed exit=$? $(date +%T)" >> '"$D"'/driver.log'
echo "STEP_AXES_DONE $(date +%T)" >> "$D/driver.log"
