#!/usr/bin/env bash
# FINDINGS.md section 56: two questions in one battery on every registered
# array/scalar problem, matched fresh seeds. Arms: base (current default),
# ST (+ --combinator-state: should the stateful pieces become default?) and
# STX (ST with EVERY hand-written skeleton disabled: which skeletons are
# still indispensable?). One --benchmark --benchmark-seeds 1 process per run.
#
# Usage: scripts/run_state_isolation_ab.sh output_root
# Env: SEEDS (10), SEED_BASE (13000), JOBS (8).

set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:?output_root}"
SEEDS="${SEEDS:-10}"; SEED_BASE="${SEED_BASE:-13000}"; JOBS="${JOBS:-8}"
mkdir -p "$D"
# problem:max-n:num-vars(- = not passed):wall
LIST="sorting:5:4:240 min:10:4:240 max:10:4:240 sum:10:4:240 contains:10:4:240 contains_ordered:10:4:240 \
is_prime:20:5:240 gcd:20:4:240 factorial:15:5:240 fibonacci:20:4:240 count:10:4:240 average:10:5:240 \
reverse:10:4:240 power:10:6:240 three_sum:8:6:240 single_number:6:4:240 graph_reachability:6:6:240 \
substring:6:-:180 palindrome:8:-:180 inversion_count:8:-:180 has_duplicate:8:-:180 is_sorted:8:-:180 \
count_descents:8:-:180 strictly_increasing:8:-:180 has_adjacent_equal:8:-:180 longest_run:8:-:180 \
max_adjacent_gap:8:-:180 cycle:5:-:180 lis:5:-:180 median:8:-:180 second_smallest:8:-:180 \
count_distinct:8:-:180 min_gap:8:-:180 max_gap_sorted:8:-:180 max_subarray_sum:8:-:180 \
longest_increasing_run:8:-:180 max_profit:8:-:180 count_leaders:8:-:180 has_majority:8:-:180 \
trapping_rain_water:8:-:180 max_rise:8:-:180 min_step:8:-:180 count_runs:8:-:180 count_ascending_runs:8:-:180 \
longest_non_increasing_run:8:-:180 min_subarray_sum:8:-:180 longest_decreasing_subsequence:8:-:180 \
max_sum_increasing_subsequence:8:-:180 ceiling_water:8:-:180"
for job in $LIST; do for arm in base ST STX; do for s in $(seq 0 $((SEEDS - 1))); do
  echo "$arm $job $((SEED_BASE + s))"
done; done; done | xargs -P "$JOBS" -L1 sh -c '
  arm=$0; job=$1; seed=$2
  IFS=: read problem maxn nv wall <<EOT
$job
EOT
  set --
  [ "$nv" != - ] && set -- "$@" --num-vars "$nv"
  case "$arm" in ST|STX) set -- "$@" --combinator-state;; esac
  if [ "$arm" = STX ]; then
    export ALGOSEARCH_DISABLE_SKELETON=average,binarysearch,counting,divideconquer,dtw,graphreachability,heapsort,mergesort,nesteddivisor,nesteddivisornative,recurrence,repeatedmultiply,selectionsort,shortestpath,threesum,tspbranchandbound,tspbruteforce
  fi
  timeout $wall ./algosearch --problem $problem --max-n $maxn --seed $seed --threads 1 --beam-width 500 \
    --candidates 1500 --max-program-size 150 --step-limit 5000 "$@" --benchmark --benchmark-seeds 1 \
    > '"$D"'/log_${arm}_${problem}_${seed}.txt 2>&1
  echo "$arm $problem $seed exit=$? $(date +%T)" >> '"$D"'/driver.log'
echo "STATE_ISOLATION_DONE $(date +%T)" >> "$D/driver.log"
