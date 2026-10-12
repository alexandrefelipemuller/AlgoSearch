#!/usr/bin/env bash
# FINDINGS.md section 101: can the scalar-loop piece with the index range
# replace the average skeleton? Arms on average, 30 matched seeds: base
# (current default) and IRX (--scalar-loop-index-range, average skeleton off).
#
# Usage: scripts/run_index_range_average_ab.sh output_root
# Env: SEEDS (30), SEED_BASE (48000), JOBS (8).

set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:?output_root}"
SEEDS="${SEEDS:-30}"; SEED_BASE="${SEED_BASE:-48000}"; JOBS="${JOBS:-8}"
mkdir -p "$D"
for arm in base IRX; do for s in $(seq 0 $((SEEDS - 1))); do
  echo "$arm $((SEED_BASE + s))"
done; done | xargs -P "$JOBS" -L1 sh -c '
  arm=$0; seed=$1
  set --
  if [ "$arm" = IRX ]; then
    set -- --scalar-loop-index-range
    export ALGOSEARCH_DISABLE_SKELETON=average
  fi
  timeout 240 ./algosearch --problem average --max-n 10 --num-vars 5 --seed $seed --threads 1 --beam-width 500 \
    --candidates 1500 --max-program-size 150 --step-limit 20000 "$@" --benchmark --benchmark-seeds 1 \
    > '"$D"'/log_${arm}_average_${seed}.txt 2>&1
  echo "$arm average $seed exit=$? $(date +%T)" >> '"$D"'/driver.log'
echo "INDEX_RANGE_AVERAGE_DONE $(date +%T)" >> "$D/driver.log"
