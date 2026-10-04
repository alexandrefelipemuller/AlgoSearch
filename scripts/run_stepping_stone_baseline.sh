#!/usr/bin/env bash
# FINDINGS.md section 50, arms D and N: the 11 post-freeze problems on the
# frozen default (D) and with the sort skeletons disabled (N). Modest budget
# (section 47), 15 matched seeds, one --benchmark --benchmark-seeds 1 process
# per (arm, problem, seed): a seed counts as a success iff at least one program
# survives the exhaustive re-check ("N genuinely generalizing solution(s)
# kept" with N >= 1).
#
# Usage: scripts/run_stepping_stone_baseline.sh output_root
# Env: SEEDS (15), SEED_BASE (8000), WALL (180), JOBS (8).

set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:?output_root}"
SEEDS="${SEEDS:-15}"; SEED_BASE="${SEED_BASE:-8000}"; WALL="${WALL:-180}"; JOBS="${JOBS:-8}"
mkdir -p "$D"
PROBLEMS="median second_smallest count_distinct min_gap max_gap_sorted max_subarray_sum longest_increasing_run \
max_profit count_leaders has_majority trapping_rain_water"
for p in $PROBLEMS; do for arm in D N; do for s in $(seq 0 $((SEEDS - 1))); do
  echo "$arm $p $((SEED_BASE + s))"
done; done; done | xargs -P "$JOBS" -L1 sh -c '
  arm=$0; problem=$1; seed=$2
  if [ "$arm" = N ]; then export ALGOSEARCH_DISABLE_SKELETON=selectionsort,heapsort,mergesort; fi
  timeout '"$WALL"' ./algosearch --problem $problem --max-n 8 --seed $seed --threads 1 --beam-width 500 \
    --candidates 1500 --max-program-size 150 --step-limit 5000 --benchmark --benchmark-seeds 1 \
    > '"$D"'/log_${arm}_${problem}_${seed}.txt 2>&1
  echo "$arm $problem $seed exit=$? $(date +%T)" >> '"$D"'/driver.log'
echo "STEPPING_STONE_BASELINE_DONE $(date +%T)" >> "$D/driver.log"
