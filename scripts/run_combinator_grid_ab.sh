#!/usr/bin/env bash
# FINDINGS.md section 109: base (current default) vs DPX (--combinator-grid,
# dtw skeleton disabled) on dtw.
#
# Usage: scripts/run_combinator_triple_ab.sh output_root
# Env: SEEDS (30), SEED_BASE (50000), JOBS (8).

set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:?output_root}"
SEEDS="${SEEDS:-30}"; SEED_BASE="${SEED_BASE:-55000}"; JOBS="${JOBS:-8}"
mkdir -p "$D"
for arm in base DPX; do
  for s in $(seq 0 $((SEEDS - 1))); do echo "$arm $((SEED_BASE + s))"; done
done | xargs -P "$JOBS" -L1 sh -c '
  arm=$0; seed=$1
  set --
  if [ "$arm" = DPX ]; then set -- --combinator-grid; export ALGOSEARCH_DISABLE_SKELETON=dtw; fi
  timeout 300 ./algosearch --problem dtw --max-n 5 --num-vars 12 --seed $seed --threads 1 --beam-width 500 \
    --candidates 1500 --max-program-size 150 --step-limit 20000 "$@" --benchmark --benchmark-seeds 1 \
    > '"$D"'/log_${arm}_dtw_${seed}.txt 2>&1
  echo "$arm dtw $seed exit=$? $(date +%T)" >> '"$D"'/driver.log'
echo "COMBINATOR_GRID_AB_DONE $(date +%T)" >> "$D/driver.log"
