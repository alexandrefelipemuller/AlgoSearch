#!/usr/bin/env bash
# FINDINGS.md section 104: base (current default) vs GRX (--combinator-relax,
# graphreachability skeleton disabled) on graph_reachability.
#
# Usage: scripts/run_combinator_triple_ab.sh output_root
# Env: SEEDS (30), SEED_BASE (50000), JOBS (8).

set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:?output_root}"
SEEDS="${SEEDS:-30}"; SEED_BASE="${SEED_BASE:-51000}"; JOBS="${JOBS:-8}"
mkdir -p "$D"
for arm in base GRX; do
  for s in $(seq 0 $((SEEDS - 1))); do echo "$arm $((SEED_BASE + s))"; done
done | xargs -P "$JOBS" -L1 sh -c '
  arm=$0; seed=$1
  set --
  if [ "$arm" = GRX ]; then set -- --combinator-relax; export ALGOSEARCH_DISABLE_SKELETON=graphreachability; fi
  timeout 240 ./algosearch --problem graph_reachability --max-n 6 --num-vars 6 --seed $seed --threads 1 --beam-width 500 \
    --candidates 1500 --max-program-size 150 --step-limit 20000 "$@" --benchmark --benchmark-seeds 1 \
    > '"$D"'/log_${arm}_graph_reachability_${seed}.txt 2>&1
  echo "$arm graph_reachability $seed exit=$? $(date +%T)" >> '"$D"'/driver.log'
echo "COMBINATOR_RELAX_AB_DONE $(date +%T)" >> "$D/driver.log"
