#!/usr/bin/env bash
# Baseline for the held-out discovery benchmark (FINDINGS.md open thread /
# section 24): the three problems deliberately NOT in the skeleton library
# (lis, substring, cycle), run with the CURRENT search as-is, before any
# engine/grammar change from the approved 3->1->2 plan. Two arms per problem:
#   bias 0.4  -- the project's default (skeleton pool available, unforced)
#   bias 0.0  -- no skeleton draw at all (the isolation setting, FINDINGS #5/#7)
# Fixed matched budget for every run; 5 seeds each; wall-capped per run as a
# safety net (the engine has no internal wall-clock cutoff).
#
# Usage: scripts/run_heldout_baseline.sh [output_root]

set -uo pipefail
cd "$(dirname "$0")/.."

OUT_ROOT="${1:-data/heldout_baseline_$(date +%Y%m%d_%H%M)}"
mkdir -p "$OUT_ROOT"
BIN=./algosearch

BEAM=500
CANDIDATES=800
SIZE=60
STEP_LIMIT=10000
NUM_VARS=6
MAX_N=5
SEED_BASE=2001
SEEDS=5
WALL=600

echo "=== $(date '+%Y-%m-%d %H:%M:%S') held-out baseline starting, out=$OUT_ROOT ===" | tee "$OUT_ROOT/driver.log"
echo "beam=$BEAM candidates=$CANDIDATES size=$SIZE step-limit=$STEP_LIMIT vars=$NUM_VARS max-n=$MAX_N seeds=$SEEDS wall=${WALL}s" | tee -a "$OUT_ROOT/driver.log"

for bias in 0.4 0.0; do
  for problem in lis substring cycle; do
    for s in $(seq 0 $((SEEDS - 1))); do
      seed=$((SEED_BASE + s))
      tag="${problem}_bias${bias}_seed${seed}"
      echo "=== $(date '+%Y-%m-%d %H:%M:%S') starting $tag ===" | tee -a "$OUT_ROOT/driver.log"
      timeout "$WALL" "$BIN" \
        --problem "$problem" --max-n "$MAX_N" --num-vars "$NUM_VARS" \
        --beam-width "$BEAM" --candidates "$CANDIDATES" --max-program-size "$SIZE" \
        --step-limit "$STEP_LIMIT" --seed "$seed" --loop-bias "$bias" \
        --output "$OUT_ROOT/solutions_$tag.json" \
        > "$OUT_ROOT/log_$tag.txt" 2>&1
      rc=$?
      echo "=== $(date '+%Y-%m-%d %H:%M:%S') finished $tag (exit=$rc) ===" | tee -a "$OUT_ROOT/driver.log"
    done
  done
done

echo "HELDOUT_BASELINE_DONE $(date '+%Y-%m-%d %H:%M:%S')" | tee -a "$OUT_ROOT/driver.log"
