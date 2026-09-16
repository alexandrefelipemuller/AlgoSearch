#!/usr/bin/env bash
# Modest-budget benchmark sweep across all 10 problems in --help, using
# current best-known defaults (loopAccumulatorBias=0.4, everything else
# at its confirmed-default value -- see FINDINGS.md sections 5-9). Each
# run uses --benchmark, which independently re-verifies every SUCCESS
# candidate found across benchmark-seeds seeds via Benchmark.cpp's
# adversarial magnitude-shift check before reporting it -- so a solution
# printed here has already survived that check, not just the training/
# held-out score.
#
# "Modest" budget: candidates=1500 (vs 5000-8000 in the deep/final
# batteries), max-program-size=150 (vs 300-800), 5 seeds/problem, wall
# capped per-problem via `timeout` as a safety net (the engine itself has
# no internal wall-clock cutoff -- see FINDINGS.md section 8).
#
# Usage: scripts/run_full_battery_modest.sh [output_root]

set -uo pipefail
cd "$(dirname "$0")/.."

OUT_ROOT="${1:-data/modest_battery_$(date +%Y%m%d_%H%M)}"
mkdir -p "$OUT_ROOT"
BIN=./algosearch

BEAM=500
CANDIDATES=1500
STEP_LIMIT=5000
SEEDS=5
SEED_BASE=1000
WALL=240

# problem:max-n:num-vars
JOBS=(
  "sorting:5:4"
  "min:10:4"
  "max:10:4"
  "sum:10:4"
  "contains:10:4"
  "contains_ordered:10:4"
  "is_prime:20:4"
  "gcd:20:4"
  "factorial:15:5"
  "fibonacci:20:4"
)

echo "=== $(date '+%Y-%m-%d %H:%M:%S') modest battery starting, out=$OUT_ROOT ===" | tee "$OUT_ROOT/driver.log"

for job in "${JOBS[@]}"; do
  IFS=: read -r problem maxn numvars <<< "$job"
  echo "=== $(date '+%Y-%m-%d %H:%M:%S') starting $problem (max-n=$maxn vars=$numvars beam=$BEAM candidates=$CANDIDATES size=150 seeds=$SEEDS wall=${WALL}s) ===" | tee -a "$OUT_ROOT/driver.log"
  timeout "$WALL" "$BIN" \
    --problem "$problem" --max-n "$maxn" --num-vars "$numvars" \
    --beam-width "$BEAM" --candidates "$CANDIDATES" --max-program-size 150 \
    --step-limit "$STEP_LIMIT" --seed "$SEED_BASE" \
    --benchmark --benchmark-seeds "$SEEDS" \
    > "$OUT_ROOT/log_${problem}.txt" 2>&1
  rc=$?
  echo "=== $(date '+%Y-%m-%d %H:%M:%S') finished $problem (exit=$rc) ===" | tee -a "$OUT_ROOT/driver.log"
done

echo "MODEST_BATTERY_DONE $(date '+%Y-%m-%d %H:%M:%S')" | tee -a "$OUT_ROOT/driver.log"
