#!/usr/bin/env bash
# FINDINGS.md section 42 test: does turning the combinator on (v2 axes, now
# the default family) ALONGSIDE the skeleton library (loop-bias 0.4, the
# project default) regress any problem? Matched-seed A/B in the layout
# scripts/summarize_prune_ab.py reads (suite A = the modest battery's 17
# problems and budgets, suite C = the combinator-era problems, all at
# --num-vars 8 since the combinator's binding context needs it).
#
# Usage: scripts/run_combinator_default_battery.sh output_root
# Env: SEEDS (5), SEED_BASE (6000), BIAS (0.5).

set -uo pipefail
cd "$(dirname "$0")/.."
OUT_ROOT="${1:?output_root}"
SEEDS="${SEEDS:-5}"
SEED_BASE="${SEED_BASE:-6000}"
BIAS="${BIAS:-0.5}"
mkdir -p "$OUT_ROOT"
TIMES="$OUT_ROOT/times.tsv"
echo -e "suite\tproblem\tseed\tarm\texit\twall_s" > "$TIMES"
echo "SEEDS=$SEEDS SEED_BASE=$SEED_BASE BIAS=$BIAS loop-bias=0.4 num-vars=8" > "$OUT_ROOT/config.txt"

run_arm() {  # suite problem maxn seed arm wall
  local suite=$1 problem=$2 maxn=$3 seed=$4 arm=$5 wall=$6
  local tag="${suite}_${problem}_seed${seed}_${arm}" flag=""
  [ "$arm" = on ] && flag="--combinator-bias $BIAS"
  local t0; t0=$(date +%s.%N)
  timeout "$wall" ./algosearch --problem "$problem" --max-n "$maxn" --num-vars 8 --seed "$seed" --threads 4 \
    --loop-bias 0.4 --beam-width 500 --candidates 1500 --max-program-size 150 --step-limit 5000 $flag \
    --output "$OUT_ROOT/solutions_$tag.json" > "$OUT_ROOT/log_$tag.txt" 2> "$OUT_ROOT/err_$tag.txt"
  local rc=$?
  echo -e "$suite\t$problem\t$seed\t$arm\t$rc\t$(echo "$(date +%s.%N) - $t0" | bc)" >> "$TIMES"
}

run_suite() {  # suite wall jobs...
  local suite=$1 wall=$2; shift 2
  for job in "$@"; do
    IFS=: read -r problem maxn <<< "$job"
    for s in $(seq 0 $((SEEDS - 1))); do
      seed=$((SEED_BASE + s))
      run_arm "$suite" "$problem" "$maxn" "$seed" off "$wall" &
      run_arm "$suite" "$problem" "$maxn" "$seed" on "$wall" &
      wait
    done
  done
}

run_suite A 240 sorting:5 min:10 max:10 sum:10 contains:10 contains_ordered:10 is_prime:20 gcd:20 factorial:15 \
  fibonacci:20 count:10 average:10 reverse:10 power:10 three_sum:8 single_number:6 graph_reachability:6
run_suite C 300 substring:6 palindrome:8 inversion_count:8 has_duplicate:8 is_sorted:8 count_descents:8 \
  strictly_increasing:8 has_adjacent_equal:8 longest_run:8 max_adjacent_gap:8 cycle:5 lis:5
echo "COMBINATOR_DEFAULT_BATTERY_DONE $(date '+%Y-%m-%d %H:%M:%S')" >> "$OUT_ROOT/driver.log"
