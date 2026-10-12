#!/usr/bin/env bash
# FINDINGS.md section 103: base (current default) vs TRX (--combinator-triple,
# threesum skeleton disabled) on three_sum.
#
# Usage: scripts/run_combinator_triple_ab.sh output_root
# Env: SEEDS (30), SEED_BASE (50000), JOBS (8).

set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:?output_root}"
SEEDS="${SEEDS:-30}"; SEED_BASE="${SEED_BASE:-50000}"; JOBS="${JOBS:-8}"
mkdir -p "$D"
for arm in base TRX; do
  for s in $(seq 0 $((SEEDS - 1))); do echo "$arm $((SEED_BASE + s))"; done
done | xargs -P "$JOBS" -L1 sh -c '
  arm=$0; seed=$1
  set --
  if [ "$arm" = TRX ]; then set -- --combinator-triple; export ALGOSEARCH_DISABLE_SKELETON=threesum; fi
  timeout 240 ./algosearch --problem three_sum --max-n 8 --num-vars 6 --seed $seed --threads 1 --beam-width 500 \
    --candidates 1500 --max-program-size 150 --step-limit 20000 "$@" --benchmark --benchmark-seeds 1 \
    > '"$D"'/log_${arm}_three_sum_${seed}.txt 2>&1
  echo "$arm three_sum $seed exit=$? $(date +%T)" >> '"$D"'/driver.log'
echo "COMBINATOR_TRIPLE_AB_DONE $(date +%T)" >> "$D/driver.log"
