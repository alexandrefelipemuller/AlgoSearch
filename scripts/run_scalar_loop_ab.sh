#!/usr/bin/env bash
# FINDINGS.md section 100: A/B of the generic scalar-loop piece. Arms: base
# (current default), SL (+ --scalar-loop) and SLX (SL with the five scalar
# skeletons disabled: can the piece replace them?). Scalar targets plus
# skeleton-using controls. One --benchmark --benchmark-seeds 1 per run.
#
# Usage: scripts/run_scalar_loop_ab.sh output_root
# Env: SEEDS (10), SEED_BASE (46000), JOBS (8).

set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:?output_root}"
SEEDS="${SEEDS:-10}"; SEED_BASE="${SEED_BASE:-46000}"; JOBS="${JOBS:-8}"
mkdir -p "$D"
# problem:max-n:num-vars(- = not passed):wall
LIST="average:10:5:240 factorial:15:5:240 power:10:6:240 gcd:20:4:240 fibonacci:20:4:240 is_prime:20:5:240 \
sorting:5:4:240 three_sum:8:6:240 graph_reachability:6:6:240 count_distinct:8:-:180"
for job in $LIST; do for arm in base SL SLX; do for s in $(seq 0 $((SEEDS - 1))); do
  echo "$arm $job $((SEED_BASE + s))"
done; done; done | xargs -P "$JOBS" -L1 sh -c '
  arm=$0; job=$1; seed=$2
  IFS=: read problem maxn nv wall <<EOT
$job
EOT
  set --
  [ "$nv" != - ] && set -- "$@" --num-vars "$nv"
  case "$arm" in SL|SLX) set -- "$@" --scalar-loop;; esac
  [ "$arm" = SLX ] && export ALGOSEARCH_DISABLE_SKELETON=average,repeatedmultiply,recurrence,nesteddivisor,nesteddivisornative
  timeout $wall ./algosearch --problem $problem --max-n $maxn --seed $seed --threads 1 --beam-width 500 \
    --candidates 1500 --max-program-size 150 --step-limit 20000 "$@" --benchmark --benchmark-seeds 1 \
    > '"$D"'/log_${arm}_${problem}_${seed}.txt 2>&1
  echo "$arm $problem $seed exit=$? $(date +%T)" >> '"$D"'/driver.log'
echo "SCALAR_LOOP_AB_DONE $(date +%T)" >> "$D/driver.log"
