#!/usr/bin/env bash
# FINDINGS.md section 47: should --combinator-bias become the default?
# Unlike section 42 (both arms at --num-vars 8), the off arm here is the
# TRUE current default: the modest battery's exact command (its per-problem
# --num-vars for suite A, nothing passed for suite C). The on arm is the same
# command plus --combinator-bias, which resolveNumVars raises to 8 vars --
# exactly what flipping the default would do. Output layout matches
# scripts/summarize_prune_ab.py.
#
# Usage: scripts/run_combinator_true_default_ab.sh output_root
# Env: SEEDS (15), SEED_BASE (7000), BIAS (0.5), PAIRS (2 seed pairs at once).

set -uo pipefail
cd "$(dirname "$0")/.."
OUT_ROOT="${1:?output_root}"
SEEDS="${SEEDS:-15}"
SEED_BASE="${SEED_BASE:-7000}"
BIAS="${BIAS:-0.5}"
PAIRS="${PAIRS:-2}"
mkdir -p "$OUT_ROOT"
TIMES="$OUT_ROOT/times.tsv"
echo -e "suite\tproblem\tseed\tarm\texit\twall_s" > "$TIMES"
echo "SEEDS=$SEEDS SEED_BASE=$SEED_BASE BIAS=$BIAS PAIRS=$PAIRS modest-battery budget" > "$OUT_ROOT/config.txt"

run_arm() {  # suite problem maxn numvars seed arm wall
  local suite=$1 problem=$2 maxn=$3 numvars=$4 seed=$5 arm=$6 wall=$7 flags=()
  local tag="${suite}_${problem}_seed${seed}_${arm}"
  [ "$numvars" != - ] && flags+=(--num-vars "$numvars")
  [ "$arm" = on ] && flags+=(--combinator-bias "$BIAS")
  local t0; t0=$(date +%s.%N)
  timeout "$wall" ./algosearch --problem "$problem" --max-n "$maxn" --seed "$seed" --threads 2 \
    --beam-width 500 --candidates 1500 --max-program-size 150 --step-limit 5000 "${flags[@]}" \
    --output "$OUT_ROOT/solutions_$tag.json" --checkpoint-path "$OUT_ROOT/solutions_$tag.json" \
    > "$OUT_ROOT/log_$tag.txt" 2> "$OUT_ROOT/err_$tag.txt"
  local rc=$?
  echo -e "$suite\t$problem\t$seed\t$arm\t$rc\t$(echo "$(date +%s.%N) - $t0" | bc)" >> "$TIMES"
}

run_suite() {  # suite wall jobs (problem:maxn:numvars, numvars "-" = not passed)...
  local suite=$1 wall=$2; shift 2
  for job in "$@"; do
    IFS=: read -r problem maxn numvars <<< "$job"
    for s in $(seq 0 $((SEEDS - 1))); do
      seed=$((SEED_BASE + s))
      run_arm "$suite" "$problem" "$maxn" "$numvars" "$seed" off "$wall" &
      run_arm "$suite" "$problem" "$maxn" "$numvars" "$seed" on "$wall" &
      if (( (s + 1) % PAIRS == 0 )); then wait; fi
    done
    wait
  done
}

run_suite A 240 sorting:5:4 min:10:4 max:10:4 sum:10:4 contains:10:4 contains_ordered:10:4 is_prime:20:5 \
  gcd:20:4 factorial:15:5 fibonacci:20:4 count:10:4 average:10:5 reverse:10:4 power:10:6 three_sum:8:6 \
  single_number:6:4 graph_reachability:6:6
run_suite C 180 substring:6:- palindrome:8:- inversion_count:8:- has_duplicate:8:- is_sorted:8:- \
  count_descents:8:- strictly_increasing:8:- has_adjacent_equal:8:- longest_run:8:- max_adjacent_gap:8:- \
  cycle:5:- lis:5:-
echo "COMBINATOR_TRUE_DEFAULT_DONE $(date '+%Y-%m-%d %H:%M:%S')" >> "$OUT_ROOT/driver.log"
