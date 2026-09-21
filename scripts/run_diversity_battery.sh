#!/usr/bin/env bash
# Diversity battery: does turning on the two mechanisms built this session
# (Moção 1's --post-solve-sizes divergence window and Moção 2's
# --composite-bias operator) increase the number of DISTINCT solution
# programs found across a representative sweep of problems, without
# regressing the level each problem already reaches at today's default?
#
# Two arms per problem, matched seed/budget:
#   control    -- today's confirmed default (loop-bias=0.4, everything else off)
#   treatment  -- control + --post-solve-sizes 15 --composite-bias 0.3
#
# "Diversity" is measured directly from each run's solutions_*.json: number
# of DISTINCT programId values recorded (the divergence window's archive
# only records a NEW champion when its program text differs, so a jump here
# is a real count of different programs found, not a proxy).
#
# Usage: scripts/run_diversity_battery.sh [output_root]

set -uo pipefail
cd "$(dirname "$0")/.."

OUT_ROOT="${1:-data/diversity_battery_$(date +%Y%m%d_%H%M)}"
mkdir -p "$OUT_ROOT"
BIN=./algosearch

BEAM=300
CANDIDATES=800
SIZE=80
STEP_LIMIT=4000
SEEDS=3
SEED_BASE=3001
WALL=90

# problem:max-n:num-vars -- representative subset of the original 21 (mix of
# scalar, array, and structural problems) plus all 3 held-out problems.
JOBS=(
  "sorting:5:4"
  "min:8:4"
  "max:8:4"
  "sum:8:4"
  "gcd:15:4"
  "is_prime:15:5"
  "factorial:10:5"
  "contains:8:4"
  "three_sum:6:6"
  "graph_reachability:5:6"
  "lis:5:6"
  "substring:5:6"
  "cycle:5:5"
)

echo "=== $(date '+%Y-%m-%d %H:%M:%S') diversity battery starting, out=$OUT_ROOT ===" | tee "$OUT_ROOT/driver.log"
echo "beam=$BEAM candidates=$CANDIDATES size=$SIZE step-limit=$STEP_LIMIT seeds=$SEEDS wall=${WALL}s" | tee -a "$OUT_ROOT/driver.log"

for job in "${JOBS[@]}"; do
  IFS=: read -r problem maxn numvars <<< "$job"
  for arm in control treatment; do
    for s in $(seq 0 $((SEEDS - 1))); do
      seed=$((SEED_BASE + s))
      tag="${problem}_${arm}_seed${seed}"
      if [ "$arm" = "control" ]; then
        extra_flags=()
      else
        extra_flags=(--post-solve-sizes 15 --composite-bias 0.3)
      fi
      echo "=== $(date '+%Y-%m-%d %H:%M:%S') starting $tag ===" | tee -a "$OUT_ROOT/driver.log"
      timeout "$WALL" "$BIN" \
        --problem "$problem" --max-n "$maxn" --num-vars "$numvars" \
        --beam-width "$BEAM" --candidates "$CANDIDATES" --max-program-size "$SIZE" \
        --step-limit "$STEP_LIMIT" --seed "$seed" --loop-bias 0.4 \
        "${extra_flags[@]}" \
        --output "$OUT_ROOT/solutions_$tag.json" \
        > "$OUT_ROOT/log_$tag.txt" 2>&1
      rc=$?
      echo "=== $(date '+%Y-%m-%d %H:%M:%S') finished $tag (exit=$rc) ===" | tee -a "$OUT_ROOT/driver.log"
    done
  done
done

echo "DIVERSITY_BATTERY_DONE $(date '+%Y-%m-%d %H:%M:%S')" | tee -a "$OUT_ROOT/driver.log"
