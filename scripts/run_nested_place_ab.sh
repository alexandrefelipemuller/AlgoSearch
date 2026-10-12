#!/usr/bin/env bash
# FINDINGS.md section 102: base (current default) vs PLX (--nested-place,
# selectionsort skeleton disabled) on the four problems the skeleton serves.
#
# Usage: scripts/run_nested_place_ab.sh output_root
# Env: SEEDS (30), SEED_BASE (49000), JOBS (8).

set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:?output_root}"
SEEDS="${SEEDS:-30}"; SEED_BASE="${SEED_BASE:-49000}"; JOBS="${JOBS:-8}"
mkdir -p "$D"
for arm in base PLX; do
  for job in sorting:5:240 count_distinct:8:180 min_gap:8:180 max_gap_sorted:8:180; do
    for s in $(seq 0 $((SEEDS - 1))); do echo "$arm $job $((SEED_BASE + s))"; done
  done
done | xargs -P "$JOBS" -L1 sh -c '
  arm=$0; job=$1; seed=$2
  IFS=: read problem maxn wall <<EOT
$job
EOT
  set --
  if [ "$arm" = PLX ]; then set -- --nested-place; export ALGOSEARCH_DISABLE_SKELETON=selectionsort; fi
  timeout $wall ./algosearch --problem $problem --max-n $maxn --seed $seed --threads 1 --beam-width 500 \
    --candidates 1500 --max-program-size 150 --step-limit 20000 "$@" --benchmark --benchmark-seeds 1 \
    > '"$D"'/log_${arm}_${problem}_${seed}.txt 2>&1
  echo "$arm $problem $seed exit=$? $(date +%T)" >> '"$D"'/driver.log'
echo "NESTED_PLACE_AB_DONE $(date +%T)" >> "$D/driver.log"
