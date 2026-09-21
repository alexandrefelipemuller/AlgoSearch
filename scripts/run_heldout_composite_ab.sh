#!/usr/bin/env bash
# Etapa 3 (Moção 2) A/B: composite-primitives operator (--composite-bias)
# against the frozen held-out baseline, in the loop-bias=0.0 isolation arm
# (no skeleton draw at all -- only the free grammar plus, when enabled, the
# new two-loop composite family). Three problems:
#   substring -- Test A (composition): the solution is a recombination of
#                section 24's catalogued primitives; success = the search
#                composes it unaided.
#   lis       -- Test B (open discovery): the solution needs the catalogue's
#                predicted SIXTH one-off primitive (read several computed
#                cells and take a max), which the composite family cannot
#                express by construction; expected to fail, and that failure
#                is the measured frontier (input for Moção 4), NOT a reason
#                to add another ad-hoc primitive.
#   cycle     -- reserve, reported separately (new two-pointer shape).
#
# Usage: scripts/run_heldout_composite_ab.sh [output_root]

set -uo pipefail
cd "$(dirname "$0")/.."

OUT_ROOT="${1:-data/heldout_composite_ab_$(date +%Y%m%d_%H%M)}"
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

echo "=== $(date '+%Y-%m-%d %H:%M:%S') held-out composite A/B starting, out=$OUT_ROOT ===" | tee "$OUT_ROOT/driver.log"
echo "beam=$BEAM candidates=$CANDIDATES size=$SIZE step-limit=$STEP_LIMIT vars=$NUM_VARS max-n=$MAX_N seeds=$SEEDS wall=${WALL}s loop-bias=0.0" | tee -a "$OUT_ROOT/driver.log"

for composite in 0 0.5 1.0; do
  for problem in substring lis cycle; do
    for s in $(seq 0 $((SEEDS - 1))); do
      seed=$((SEED_BASE + s))
      tag="${problem}_composite${composite}_seed${seed}"
      echo "=== $(date '+%Y-%m-%d %H:%M:%S') starting $tag ===" | tee -a "$OUT_ROOT/driver.log"
      timeout "$WALL" "$BIN" \
        --problem "$problem" --max-n "$MAX_N" --num-vars "$NUM_VARS" \
        --beam-width "$BEAM" --candidates "$CANDIDATES" --max-program-size "$SIZE" \
        --step-limit "$STEP_LIMIT" --seed "$seed" --loop-bias 0.0 \
        --composite-bias "$composite" \
        --output "$OUT_ROOT/solutions_$tag.json" \
        > "$OUT_ROOT/log_$tag.txt" 2>&1
      rc=$?
      echo "=== $(date '+%Y-%m-%d %H:%M:%S') finished $tag (exit=$rc) ===" | tee -a "$OUT_ROOT/driver.log"
    done
  done
done

echo "HELDOUT_COMPOSITE_AB_DONE $(date '+%Y-%m-%d %H:%M:%S')" | tee -a "$OUT_ROOT/driver.log"
