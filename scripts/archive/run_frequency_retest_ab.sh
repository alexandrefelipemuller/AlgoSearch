#!/usr/bin/env bash
# FINDINGS.md section 76: 30-seed retest of second_smallest for the
# frequency piece. Arms: base (current default) vs + --combinator-frequency.
# One --benchmark --benchmark-seeds 1 process per run.
#
# Usage: scripts/run_frequency_retest_ab.sh output_root
# Env: SEEDS (30), SEED_BASE (25000), JOBS (8).

set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:?output_root}"
SEEDS="${SEEDS:-30}"; SEED_BASE="${SEED_BASE:-25000}"; JOBS="${JOBS:-8}"
mkdir -p "$D"
for problem in second_smallest; do for arm in base FR; do
  for s in $(seq 0 $((SEEDS - 1))); do echo "$arm $problem $((SEED_BASE + s))"; done
done; done | xargs -P "$JOBS" -L1 sh -c '
  arm=$0; problem=$1; seed=$2
  set --
  [ "$arm" = FR ] && set -- --combinator-frequency
  timeout 180 ./algosearch --problem $problem --max-n 8 --seed $seed --threads 1 --beam-width 500 \
    --candidates 1500 --max-program-size 150 --step-limit 20000 "$@" --benchmark --benchmark-seeds 1 \
    > '"$D"'/log_${arm}_${problem}_${seed}.txt 2>&1
  echo "$arm $problem $seed exit=$? $(date +%T)" >> '"$D"'/driver.log'
echo "FREQUENCY_RETEST_DONE $(date +%T)" >> "$D/driver.log"
