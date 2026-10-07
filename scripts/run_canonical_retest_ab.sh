#!/usr/bin/env bash
# FINDINGS.md section 74: 30-seed retest of section 73's two decisive
# problems. Arms: base (current default) vs + --nested-canonical-tests.
# One --benchmark --benchmark-seeds 1 process per run.
#
# Usage: scripts/run_canonical_retest_ab.sh output_root
# Env: SEEDS (30), SEED_BASE (23000), JOBS (8).

set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:?output_root}"
SEEDS="${SEEDS:-30}"; SEED_BASE="${SEED_BASE:-23000}"; JOBS="${JOBS:-8}"
mkdir -p "$D"
for problem in has_majority count_unique; do for arm in base CT; do
  for s in $(seq 0 $((SEEDS - 1))); do echo "$arm $problem $((SEED_BASE + s))"; done
done; done | xargs -P "$JOBS" -L1 sh -c '
  arm=$0; problem=$1; seed=$2
  set --
  [ "$arm" = CT ] && set -- --nested-canonical-tests
  timeout 180 ./algosearch --problem $problem --max-n 8 --seed $seed --threads 1 --beam-width 500 \
    --candidates 1500 --max-program-size 150 --step-limit 20000 "$@" --benchmark --benchmark-seeds 1 \
    > '"$D"'/log_${arm}_${problem}_${seed}.txt 2>&1
  echo "$arm $problem $seed exit=$? $(date +%T)" >> '"$D"'/driver.log'
echo "CANONICAL_RETEST_DONE $(date +%T)" >> "$D/driver.log"
