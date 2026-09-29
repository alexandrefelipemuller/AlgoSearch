#!/usr/bin/env bash
# Generic matched-seed A/B of one opt-in generator flag in the isolation arm
# (loop-bias 0.0, no skeleton drawable), same layout as
# run_prune_generation_ab.sh so scripts/summarize_prune_ab.py reads it:
# off/on runs of one seed execute concurrently (--threads 4 each).
#
# Usage: FLAG=--role-typed-vars scripts/run_flag_ab.sh output_root
# Env: FLAG (required), SEEDS (10), SEED_BASE (2000), PROBLEMS
# ("problem:max-n:num-vars ..."), BUDGET (extra search args), WALL (300).

set -uo pipefail
cd "$(dirname "$0")/.."

: "${FLAG:?set FLAG to the treatment flag, e.g. --role-typed-vars}"
OUT_ROOT="${1:?output_root}"
SEEDS="${SEEDS:-10}"
SEED_BASE="${SEED_BASE:-2000}"
PROBLEMS="${PROBLEMS:-sum:8:4 min:8:4 max:8:4 count:8:4 contains:8:4 sorting:5:4 reverse:8:4}"
BUDGET="${BUDGET:---beam-width 500 --candidates 1500 --max-program-size 150 --step-limit 5000}"
WALL="${WALL:-300}"
mkdir -p "$OUT_ROOT"
TIMES="$OUT_ROOT/times.tsv"
echo -e "suite\tproblem\tseed\tarm\texit\twall_s" > "$TIMES"
{ echo "FLAG=$FLAG SEEDS=$SEEDS SEED_BASE=$SEED_BASE WALL=$WALL"; echo "PROBLEMS=$PROBLEMS"; echo "BUDGET=$BUDGET --loop-bias 0.0"; } > "$OUT_ROOT/config.txt"

run_arm() {  # problem maxn numvars seed arm
  local problem=$1 maxn=$2 numvars=$3 seed=$4 arm=$5
  local tag="B_${problem}_seed${seed}_${arm}"
  local flag=""
  [ "$arm" = on ] && flag="$FLAG"
  local t0; t0=$(date +%s.%N)
  timeout "$WALL" ./algosearch --problem "$problem" --max-n "$maxn" --num-vars "$numvars" --seed "$seed" \
    --threads 4 --loop-bias 0.0 $BUDGET $flag --output "$OUT_ROOT/solutions_$tag.json" \
    > "$OUT_ROOT/log_$tag.txt" 2> "$OUT_ROOT/err_$tag.txt"
  local rc=$?
  local t1; t1=$(date +%s.%N)
  echo -e "B\t$problem\t$seed\t$arm\t$rc\t$(echo "$t1 - $t0" | bc)" >> "$TIMES"
}

for job in $PROBLEMS; do
  IFS=: read -r problem maxn numvars <<< "$job"
  for s in $(seq 0 $((SEEDS - 1))); do
    seed=$((SEED_BASE + s))
    echo "=== $(date '+%Y-%m-%d %H:%M:%S') $problem seed $seed ===" >> "$OUT_ROOT/driver.log"
    run_arm "$problem" "$maxn" "$numvars" "$seed" off &
    run_arm "$problem" "$maxn" "$numvars" "$seed" on &
    wait
  done
done
echo "FLAG_AB_DONE $(date '+%Y-%m-%d %H:%M:%S')" >> "$OUT_ROOT/driver.log"
