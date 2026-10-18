#!/usr/bin/env bash
# FINDINGS.md section 120: open-specification problems (property checkers) vs the canonical peak_element control.
# Usage: scripts/run_open_spec.sh output_root   Env: SEEDS (10), SEED_BASE (59000), JOBS (6), PROBLEMS.
set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:?output_root}"
SEEDS="${SEEDS:-10}"; SEED_BASE="${SEED_BASE:-59000}"; JOBS="${JOBS:-6}"
LIST="${PROBLEMS:-any_peak peak_element any_duplicate}"
mkdir -p "$D"
for problem in $LIST; do for s in $(seq 0 $((SEEDS - 1))); do
  echo "$problem $((SEED_BASE + s))"
done; done | xargs -P "$JOBS" -L1 sh -c '
  problem=$0; seed=$1
  mkdir -p '"$D"'/$problem
  timeout 240 ./algosearch --problem $problem --max-n 8 --seed $seed --threads 1 --beam-width 500 \
    --candidates 1500 --max-program-size 150 --step-limit 20000 --post-solve-sizes 150 --start-level "${START_LEVEL:-2}" \
    --output '"$D"'/$problem/seed_$seed.json > '"$D"'/$problem/seed_$seed.log 2>&1
  echo "$problem $seed exit=$? $(date +%T)" >> '"$D"'/driver.log'
echo "OPEN_SPEC_DONE $(date +%T)" >> "$D/driver.log"
