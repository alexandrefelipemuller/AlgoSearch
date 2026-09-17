#!/usr/bin/env bash
# Cross-skeleton study: for each problem that has a designated "own"
# skeleton (gcd/fibonacci -> recurrence, is_prime -> nesteddivisor,
# factorial/power -> repeatedmultiply, average -> average, sorting ->
# selectionsort, count/min/max/sum/contains -> counting), force EVERY
# skeleton in turn via ALGOSEARCH_FORCE_SKELETON (see the env-var
# override in ProgramGenerator.cpp's genStmt) and see whether the search
# ever converges using a skeleton that wasn't built for it. `mod` is
# included as a wildcard control -- it has no designated skeleton at all.
#
# Usage: scripts/run_cross_skeleton_study.sh [output_root]
set -uo pipefail
cd "$(dirname "$0")/.."

OUT_ROOT="${1:-data/cross_skeleton_study_$(date +%Y%m%d_%H%M)}"
mkdir -p "$OUT_ROOT"
BIN=./algosearch

SKELETONS=(counting recurrence nesteddivisor selectionsort repeatedmultiply average)
PROBLEMS=(gcd fibonacci is_prime factorial power average sorting count mod)
SEEDS=(1 2 3)

BEAM=300
CANDIDATES=1000
SIZE=80
STEP_LIMIT=5000
WALL=30

echo "=== $(date '+%Y-%m-%d %H:%M:%S') cross-skeleton study starting, out=$OUT_ROOT ===" | tee "$OUT_ROOT/driver.log"

run_one() {
  local problem=$1 skeleton=$2 seed=$3
  local out="$OUT_ROOT/${problem}__${skeleton}__s${seed}.txt"
  ALGOSEARCH_FORCE_SKELETON="$skeleton" timeout "$WALL" "$BIN" \
    --problem "$problem" --max-n 8 --num-vars 6 \
    --beam-width "$BEAM" --candidates "$CANDIDATES" --max-program-size "$SIZE" \
    --step-limit "$STEP_LIMIT" --seed "$seed" \
    > "$out" 2>&1
}

pids=()
count=0
for problem in "${PROBLEMS[@]}"; do
  for skeleton in "${SKELETONS[@]}"; do
    for seed in "${SEEDS[@]}"; do
      run_one "$problem" "$skeleton" "$seed" &
      pids+=($!)
      count=$((count+1))
      if [ $((count % 8)) -eq 0 ]; then
        wait "${pids[@]}"
        pids=()
      fi
    done
  done
done
wait "${pids[@]}" 2>/dev/null

echo "CROSS_SKELETON_STUDY_DONE $(date '+%Y-%m-%d %H:%M:%S'), $count runs" | tee -a "$OUT_ROOT/driver.log"
