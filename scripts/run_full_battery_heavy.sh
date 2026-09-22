#!/usr/bin/env bash
# Heavy-budget sweep across every problem currently in --help, using a
# budget substantially larger than README.md's "modest" sweep
# (beam=500 candidates=1500 size=150) -- see scripts/run_full_battery_modest.sh
# for that baseline. Same current grammar defaults, --benchmark on every run
# so nothing gets reported without independent adversarial re-verification.
#
# Heavy budget: beam-width=2500 (5x modest), candidates=8000 (~5.3x modest),
# max-program-size=400 (~2.7x modest), step-limit=20000 (4x modest),
# 8 benchmark seeds (vs 5). Wall-clock capped per-problem via `timeout` as a
# safety net -- the engine itself has no internal wall-clock cutoff (see
# FINDINGS.md section 8).
#
# Usage: scripts/run_full_battery_heavy.sh [output_root]

set -uo pipefail
cd "$(dirname "$0")/.."

OUT_ROOT="${1:-data/heavy_battery_$(date +%Y%m%d_%H%M)}"
mkdir -p "$OUT_ROOT"
BIN=./algosearch

BEAM=2500
CANDIDATES=8000
MAX_SIZE=400
STEP_LIMIT=20000
SEEDS=8
SEED_BASE=1000
WALL=1800

# problem:max-n:num-vars
JOBS=(
  "sorting:5:4"
  "min:10:4"
  "max:10:4"
  "sum:10:4"
  "contains:10:4"
  "contains_ordered:10:4"
  "is_prime:20:5"
  "gcd:20:4"
  "factorial:15:5"
  "fibonacci:20:4"
  "count:10:4"
  "average:10:5"
  "reverse:10:4"
  "power:10:6"
  "three_sum:8:6"
  "single_number:6:4"
  "graph_reachability:6:6"
  "shortest_path:6:7"
  "tsp:6:12"
  "dtw:5:12"
  "lis:4:6"
  "substring:5:6"
  "cycle:6:6"
)

echo "=== $(date '+%Y-%m-%d %H:%M:%S') heavy battery starting, out=$OUT_ROOT ===" | tee "$OUT_ROOT/driver.log"

for job in "${JOBS[@]}"; do
  IFS=: read -r problem maxn numvars <<< "$job"
  echo "=== $(date '+%Y-%m-%d %H:%M:%S') starting $problem (max-n=$maxn vars=$numvars beam=$BEAM candidates=$CANDIDATES size=$MAX_SIZE seeds=$SEEDS wall=${WALL}s) ===" | tee -a "$OUT_ROOT/driver.log"
  timeout "$WALL" "$BIN" \
    --problem "$problem" --max-n "$maxn" --num-vars "$numvars" \
    --beam-width "$BEAM" --candidates "$CANDIDATES" --max-program-size "$MAX_SIZE" \
    --step-limit "$STEP_LIMIT" --seed "$SEED_BASE" \
    --benchmark --benchmark-seeds "$SEEDS" \
    > "$OUT_ROOT/log_${problem}.txt" 2>&1
  rc=$?
  echo "=== $(date '+%Y-%m-%d %H:%M:%S') finished $problem (exit=$rc) ===" | tee -a "$OUT_ROOT/driver.log"
done

echo "HEAVY_BATTERY_DONE $(date '+%Y-%m-%d %H:%M:%S')" | tee -a "$OUT_ROOT/driver.log"
