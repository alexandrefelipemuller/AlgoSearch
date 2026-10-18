#!/usr/bin/env bash
# FINDINGS.md section 113: screen of --arg-sentinel on its targets and rulers.
# Arms: base (current default) and AS (--arg-sentinel).
#
# Usage: scripts/run_arg_sentinel_screen.sh output_root
# Env: SEEDS (10), SEED_BASE (58000), JOBS (8).

set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:?output_root}"
SEEDS="${SEEDS:-10}"; SEED_BASE="${SEED_BASE:-58000}"; JOBS="${JOBS:-8}"
mkdir -p "$D"
LIST="majority_value longest_run_value longest_increasing_run_end frequent_mode_or_none longest_run_start plurality_value"
for problem in $LIST; do for arm in base AS; do for s in $(seq 0 $((SEEDS - 1))); do
  echo "$arm $problem $((SEED_BASE + s))"
done; done; done | xargs -P "$JOBS" -L1 sh -c '
  arm=$0; problem=$1; seed=$2
  set --
  [ "$arm" = AS ] && set -- --arg-sentinel
  timeout 180 ./algosearch --problem $problem --max-n 8 --seed $seed --threads 1 --beam-width 500 \
    --candidates 1500 --max-program-size 150 --step-limit 20000 "$@" --benchmark --benchmark-seeds 1 \
    > '"$D"'/log_${arm}_${problem}_${seed}.txt 2>&1
  echo "$arm $problem $seed exit=$? $(date +%T)" >> '"$D"'/driver.log'
echo "ARG_SENTINEL_SCREEN_DONE $(date +%T)" >> "$D/driver.log"
