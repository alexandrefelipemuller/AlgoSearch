#!/usr/bin/env bash
# Heavy-compute, no-skeleton brute-force push on gcd/is_prime/factorial:
# throw a lot of beam width / candidates / program size / seeds at the
# raw grammar (loopAccumulatorBias=0.0 -- no canonical for-loop skeleton,
# see FINDINGS.md section 9 for why this matters) and see whether more
# compute alone finds solutions the modest-budget sweep (with the
# skeleton bias ON) already found easily. If it doesn't, the
# ALGOSEARCH_DEBUG_LANDSCAPE diagnostic is left on so the driver.log has
# per-round beam-composition data to investigate the shortfall with,
# without needing to rerun.
#
# Each problem runs sequentially (full core count each, not split three
# ways) with a generous wall-clock cap per problem.
#
# Usage: scripts/run_pure_bruteforce_20260916.sh [output_root]

set -uo pipefail
cd "$(dirname "$0")/.."

OUT_ROOT="${1:-data/pure_bruteforce_deep_$(date +%Y%m%d_%H%M)}"
mkdir -p "$OUT_ROOT"
BIN=./algosearch

BEAM=1500
CANDIDATES=5000
SIZE=400
STEP_LIMIT=8000
SEEDS=10
SEED_BASE=2000
WALL=900

export ALGOSEARCH_DEBUG_LANDSCAPE=1

# problem:max-n:num-vars
JOBS=(
  "gcd:20:4"
  "is_prime:20:4"
  "factorial:15:5"
)

echo "=== $(date '+%Y-%m-%d %H:%M:%S') pure brute-force (loop-bias=0.0) starting, out=$OUT_ROOT ===" | tee "$OUT_ROOT/driver.log"

for job in "${JOBS[@]}"; do
  IFS=: read -r problem maxn numvars <<< "$job"
  echo "=== $(date '+%Y-%m-%d %H:%M:%S') starting $problem (max-n=$maxn vars=$numvars beam=$BEAM candidates=$CANDIDATES size=$SIZE seeds=$SEEDS wall=${WALL}s, no skeleton) ===" | tee -a "$OUT_ROOT/driver.log"
  timeout "$WALL" "$BIN" \
    --problem "$problem" --max-n "$maxn" --num-vars "$numvars" \
    --beam-width "$BEAM" --candidates "$CANDIDATES" --max-program-size "$SIZE" \
    --step-limit "$STEP_LIMIT" --seed "$SEED_BASE" --loop-bias 0.0 \
    --benchmark --benchmark-seeds "$SEEDS" \
    > "$OUT_ROOT/log_${problem}.txt" 2> "$OUT_ROOT/landscape_${problem}.txt"
  rc=$?
  echo "=== $(date '+%Y-%m-%d %H:%M:%S') finished $problem (exit=$rc) ===" | tee -a "$OUT_ROOT/driver.log"
done

echo "PURE_BRUTEFORCE_DEEP_DONE $(date '+%Y-%m-%d %H:%M:%S')" | tee -a "$OUT_ROOT/driver.log"
