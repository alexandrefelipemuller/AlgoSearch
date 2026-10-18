#!/usr/bin/env bash
# FINDINGS.md section 58: 30-seed retest of the two problems on which
# --combinator-state missed section 56's no-regression bound. Arms: base
# (current default) vs base + --combinator-state. One --benchmark
# --benchmark-seeds 1 process per run.
#
# Usage: scripts/run_state_retest_ab.sh output_root
# Env: SEEDS (30), SEED_BASE (14000), JOBS (8).

set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:?output_root}"
SEEDS="${SEEDS:-30}"; SEED_BASE="${SEED_BASE:-14000}"; JOBS="${JOBS:-8}"
mkdir -p "$D"
for job in graph_reachability:6:6:240 second_smallest:8:-:180; do for arm in base ST; do
  for s in $(seq 0 $((SEEDS - 1))); do echo "$arm $job $((SEED_BASE + s))"; done
done; done | xargs -P "$JOBS" -L1 sh -c '
  arm=$0; job=$1; seed=$2
  IFS=: read problem maxn nv wall <<EOT
$job
EOT
  set --
  [ "$nv" != - ] && set -- "$@" --num-vars "$nv"
  [ "$arm" = ST ] && set -- "$@" --combinator-state
  timeout $wall ./algosearch --problem $problem --max-n $maxn --seed $seed --threads 1 --beam-width 500 \
    --candidates 1500 --max-program-size 150 --step-limit 5000 "$@" --benchmark --benchmark-seeds 1 \
    > '"$D"'/log_${arm}_${problem}_${seed}.txt 2>&1
  echo "$arm $problem $seed exit=$? $(date +%T)" >> '"$D"'/driver.log'
echo "STATE_RETEST_DONE $(date +%T)" >> "$D/driver.log"
