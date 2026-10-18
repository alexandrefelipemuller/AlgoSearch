#!/usr/bin/env bash
# FINDINGS section 126: seven integer problems of PSB2 under the suite's protocol (200 training cases, 2,000 unseen test
# cases, 60,000,000 program executions per run, success = every training and test case passes). Current default config.
# Usage: scripts/run_psb2.sh output_root   Env: SEEDS (30), SEED_BASE (70000), JOBS (8), WALL (900), PROBLEMS, DATA (data/psb2).
set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:?output_root}"
SEEDS="${SEEDS:-30}"; SEED_BASE="${SEED_BASE:-70000}"; JOBS="${JOBS:-8}"; WALL="${WALL:-900}"
LIST="${PROBLEMS:-psb2_gcd psb2_fuel_cost psb2_paired_digits psb2_basement psb2_find_pair psb2_coin_sums psb2_luhn}"
export ALGOSEARCH_PSB2_DIR="${DATA:-data/psb2}"
mkdir -p "$D"
for s in $(seq 0 $((SEEDS - 1))); do for problem in $LIST; do
  echo "$problem $((SEED_BASE + s))"
done; done | xargs -P "$JOBS" -L1 sh -c '
  problem=$0; seed=$1
  mkdir -p '"$D"'/$problem
  timeout $(( '"$WALL"' + 60 )) ./algosearch --problem $problem --max-n 1 --num-vars 7 --seed $seed --threads 1 \
    --beam-width 500 --candidates 1500 --max-program-size 150 --step-limit 20000 \
    --max-executions 60000000 --wall-seconds '"$WALL"' \
    --output '"$D"'/$problem/seed_$seed.json > '"$D"'/$problem/seed_$seed.log 2>&1
  echo "$problem $seed exit=$? $(date +%T)" >> '"$D"'/driver.log'
echo "PSB2_DONE $(date +%T)" >> "$D/driver.log"
