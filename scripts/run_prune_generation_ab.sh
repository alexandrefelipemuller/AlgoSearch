#!/usr/bin/env bash
# FINDINGS.md section 33, phases 0+1: A/B of --prune-generation (redraw
# v = v, arr[e] = arr[e], swap(e, e), constant if/while conditions and
# always-erroring literal operands at generation time) against the
# unpruned grammar, matched seeds. ALGOSEARCH_DEBUG_PRUNE_COUNTS is set in
# BOTH arms, so the off arm's stderr is also phase 0's measurement of how
# often each trivial shape is generated.
#
# Prediction registered before running: pruning is a generation-efficiency
# change only -- equal or lower wall time, same success/level-reached
# distribution. A drop in success on any problem would mean the "trivial"
# draws were doing useful work (e.g. `v = v` as a neutral placeholder a later
# point mutation turns into something real), and would be reported as such.
#
# Suite A: default config (loop-bias 0.4), the modest battery's 17 problems
#          and budgets -- the no-regression check.
# Suite B: isolation (loop-bias 0.0, no skeleton drawable) -- where the free
#          grammar does all the work and pruning should matter most; plus
#          section 32's composite pair.
#
# The off/on runs for one seed execute concurrently (--threads 4 each on an
# 8-core box), so machine-load noise hits both arms of a pair equally.
#
# Usage: scripts/run_prune_generation_ab.sh [output_root]
# Env overrides for follow-up confirmation runs: SEEDS, SEED_BASE, and
# ONLY="suite:problem" to run just that one job (e.g. ONLY=A:reverse).

set -uo pipefail
cd "$(dirname "$0")/.."

OUT_ROOT="${1:-data/prune_generation_ab_$(date +%Y%m%d_%H%M)}"
mkdir -p "$OUT_ROOT"
BIN=./algosearch

SEEDS="${SEEDS:-5}"
SEED_BASE="${SEED_BASE:-1000}"
ONLY="${ONLY:-}"
THREADS=4
TIMES="$OUT_ROOT/times.tsv"
echo -e "suite\tproblem\tseed\tarm\texit\twall_s" > "$TIMES"

run_arm() {  # suite problem seed arm wall args...
  local suite=$1 problem=$2 seed=$3 arm=$4 wall=$5; shift 5
  local tag="${suite}_${problem}_seed${seed}_${arm}"
  local flag=""
  [ "$arm" = on ] && flag="--prune-generation"
  local t0; t0=$(date +%s.%N)
  ALGOSEARCH_DEBUG_PRUNE_COUNTS=1 timeout "$wall" "$BIN" --problem "$problem" --seed "$seed" --threads "$THREADS" \
    "$@" $flag --output "$OUT_ROOT/solutions_$tag.json" > "$OUT_ROOT/log_$tag.txt" 2> "$OUT_ROOT/err_$tag.txt"
  local rc=$?
  local t1; t1=$(date +%s.%N)
  echo -e "$suite\t$problem\t$seed\t$arm\t$rc\t$(echo "$t1 - $t0" | bc)" >> "$TIMES"
}

run_pair() {  # suite problem wall args...
  local suite=$1 problem=$2 wall=$3; shift 3
  [ -n "$ONLY" ] && [ "$ONLY" != "$suite:$problem" ] && return
  for s in $(seq 0 $((SEEDS - 1))); do
    local seed=$((SEED_BASE + s))
    echo "=== $(date '+%Y-%m-%d %H:%M:%S') $suite $problem seed $seed ===" | tee -a "$OUT_ROOT/driver.log"
    run_arm "$suite" "$problem" "$seed" off "$wall" "$@" &
    run_arm "$suite" "$problem" "$seed" on "$wall" "$@" &
    wait
  done
}

echo "=== $(date '+%Y-%m-%d %H:%M:%S') prune-generation A/B starting, out=$OUT_ROOT ===" | tee "$OUT_ROOT/driver.log"

# Suite A -- same problem:max-n:num-vars and budgets as run_full_battery_modest.sh.
SUITE_A=(
  "sorting:5:4" "min:10:4" "max:10:4" "sum:10:4" "contains:10:4" "contains_ordered:10:4"
  "is_prime:20:5" "gcd:20:4" "factorial:15:5" "fibonacci:20:4" "count:10:4" "average:10:5"
  "reverse:10:4" "power:10:6" "three_sum:8:6" "single_number:6:4" "graph_reachability:6:6"
)
for job in "${SUITE_A[@]}"; do
  IFS=: read -r problem maxn numvars <<< "$job"
  run_pair A "$problem" 240 --max-n "$maxn" --num-vars "$numvars" \
    --beam-width 500 --candidates 1500 --max-program-size 150 --step-limit 5000
done

# Suite B -- isolation arm, free grammar only.
SUITE_B=("sum:8:4" "min:8:4" "max:8:4" "count:8:4" "contains:8:4" "sorting:5:4" "gcd:12:4" "single_number:6:4")
for job in "${SUITE_B[@]}"; do
  IFS=: read -r problem maxn numvars <<< "$job"
  run_pair B "$problem" 300 --max-n "$maxn" --num-vars "$numvars" --loop-bias 0.0 \
    --beam-width 500 --candidates 1500 --max-program-size 150 --step-limit 5000
done
for problem in substring dominated_window; do
  run_pair B "${problem}" 300 --max-n 6 --num-vars 8 --loop-bias 0.0 --composite-bias 1.0
done

echo "PRUNE_GENERATION_AB_DONE $(date '+%Y-%m-%d %H:%M:%S')" | tee -a "$OUT_ROOT/driver.log"
