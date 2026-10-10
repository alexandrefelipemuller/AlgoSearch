#!/usr/bin/env bash
# FINDINGS.md section 90: the pointer-like problems and two rulers, base (current
# default) vs + --follow-extended, 30 matched seeds; then a
# byte-identity spot check on two non-pointer problems. One --benchmark
# --benchmark-seeds 1 process per run.
#
# Usage: scripts/run_follow_extended_ab.sh output_root
# Env: SEEDS (30), SEED_BASE (43000), JOBS (8).

set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:?output_root}"
SEEDS="${SEEDS:-30}"; SEED_BASE="${SEED_BASE:-43000}"; JOBS="${JOBS:-8}"
mkdir -p "$D"
# problem:max-n:num-vars(- = not passed):wall
LIST="cycle:5:-:180 walk_length:5:-:180 visits_zero:5:-:180 cycle_length:5:-:180 last_node:5:-:180 \
graph_reachability:6:6:240 substring:6:-:180 cycle_entry:5:-:180 second_last_node:5:-:180"
run() {
  xargs -P "$JOBS" -L1 sh -c '
    arm=$0; job=$1; seed=$2
    IFS=: read problem maxn nv wall <<EOT
$job
EOT
    set --
    [ "$nv" != - ] && set -- "$@" --num-vars "$nv"
    [ "$arm" = FX ] && set -- "$@" --follow-extended
    timeout $wall ./algosearch --problem $problem --max-n $maxn --seed $seed --threads 1 --beam-width 500 \
      --candidates 1500 --max-program-size 150 --step-limit 20000 "$@" --benchmark --benchmark-seeds 1 \
      > '"$D"'/log_${arm}_${problem}_${seed}.txt 2>&1
    echo "$arm $problem $seed exit=$? $(date +%T)" >> '"$D"'/driver.log'
}
for job in $LIST; do for arm in base FX; do for s in $(seq 0 $((SEEDS - 1))); do
  echo "$arm $job $((SEED_BASE + s))"
done; done; done | run
for job in max_gap_sorted:8:-:180 second_smallest:8:-:180; do for arm in base FX; do for s in 0 1 2; do
  echo "$arm $job $((SEED_BASE + s))"
done; done; done | run
echo "FOLLOW_EXTENDED_DONE $(date +%T)" >> "$D/driver.log"
