#!/usr/bin/env bash
# FINDINGS.md section 114: external held-out set, current default only.
# Runs make test first, then every problem x seed, in one detached process.
#
# Usage: scripts/run_external_heldout.sh output_root
# Env: SEEDS (10), SEED_BASE (59000), JOBS (8).

set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:?output_root}"
SEEDS="${SEEDS:-10}"; SEED_BASE="${SEED_BASE:-59000}"; JOBS="${JOBS:-8}"
mkdir -p "$D"
make test > "$D/make_test.log" 2>&1
echo "MAKE_TEST exit=$? $(tail -1 "$D/make_test.log")" >> "$D/driver.log"
LIST="peak_element search_rotated equilibrium_point"
for problem in $LIST; do for s in $(seq 0 $((SEEDS - 1))); do
  echo "$problem $((SEED_BASE + s))"
done; done | xargs -P "$JOBS" -L1 sh -c '
  problem=$0; seed=$1
  timeout 180 ./algosearch --problem $problem --max-n 8 --seed $seed --threads 1 --beam-width 500 \
    --candidates 1500 --max-program-size 150 --step-limit 20000 --benchmark --benchmark-seeds 1 \
    > '"$D"'/log_base_${problem}_${seed}.txt 2>&1
  echo "base $problem $seed exit=$? $(date +%T)" >> '"$D"'/driver.log'
echo "EXTERNAL_HELDOUT_DONE $(date +%T)" >> "$D/driver.log"
