#!/usr/bin/env bash
# FINDINGS.md section 108: isolation screen for binarysearch and heapsort.
#
# Usage: scripts/run_speed_skeleton_isolation.sh output_root
# Env: SEEDS (10), SEED_BASE (54000), JOBS (8).

set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:?output_root}"
SEEDS="${SEEDS:-10}"; SEED_BASE="${SEED_BASE:-54000}"; JOBS="${JOBS:-8}"
mkdir -p "$D"
# arm=disabled skeleton ("-" = none)=problem:max-n,...
ARMS="base=-=contains_ordered:10,sorting:5 nobs=binarysearch=contains_ordered:10 noheap=heapsort=sorting:5"
for line in $ARMS; do
  IFS='=' read arm dis jobs <<EOT
$line
EOT
  for job in $(echo "$jobs" | tr , ' '); do for s in $(seq 0 $((SEEDS - 1))); do
    echo "$arm $dis $job $((SEED_BASE + s))"
  done; done
done | xargs -P "$JOBS" -L1 sh -c '
  arm=$0; dis=$1; job=$2; seed=$3
  IFS=: read problem maxn <<EOT
$job
EOT
  [ "$dis" != - ] && export ALGOSEARCH_DISABLE_SKELETON="$dis"
  timeout 240 ./algosearch --problem $problem --max-n $maxn --num-vars 4 --seed $seed --threads 1 --beam-width 500 \
    --candidates 1500 --max-program-size 150 --step-limit 20000 --benchmark --benchmark-seeds 1 \
    > '"$D"'/log_${arm}_${problem}_${seed}.txt 2>&1
  echo "$arm $problem $seed exit=$? $(date +%T)" >> '"$D"'/driver.log'
echo "SPEED_ISOLATION_DONE $(date +%T)" >> "$D/driver.log"
