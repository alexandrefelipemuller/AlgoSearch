#!/usr/bin/env bash
# A/B for Moção 1 (Etapa 2): the divergence engine (--post-solve-sizes) against
# the frozen held-out baseline, same binary and matched budget, on the three
# problems deliberately outside the skeleton library (lis, substring, cycle).
# Four arms per problem:
#   bias 0.4 / post-solve-sizes 0   -- control, project default (re-run with the
#                                      new binary so the comparison is matched)
#   bias 0.4 / post-solve-sizes 20  -- treatment, default bias + divergence window
#   bias 0.0 / post-solve-sizes 0   -- control, no-skeleton isolation
#   bias 0.0 / post-solve-sizes 20  -- treatment, no-skeleton + divergence window
#
# Usage: scripts/run_heldout_divergence_ab.sh [output_root]

set -uo pipefail
cd "$(dirname "$0")/.."

OUT_ROOT="${1:-data/heldout_divergence_ab_$(date +%Y%m%d_%H%M)}"
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

echo "=== $(date '+%Y-%m-%d %H:%M:%S') held-out divergence A/B starting, out=$OUT_ROOT ===" | tee "$OUT_ROOT/driver.log"
echo "beam=$BEAM candidates=$CANDIDATES size=$SIZE step-limit=$STEP_LIMIT vars=$NUM_VARS max-n=$MAX_N seeds=$SEEDS wall=${WALL}s" | tee -a "$OUT_ROOT/driver.log"

for bias in 0.4 0.0; do
  for post in 0 20; do
    for problem in lis substring cycle; do
      for s in $(seq 0 $((SEEDS - 1))); do
        seed=$((SEED_BASE + s))
        tag="${problem}_bias${bias}_post${post}_seed${seed}"
        echo "=== $(date '+%Y-%m-%d %H:%M:%S') starting $tag ===" | tee -a "$OUT_ROOT/driver.log"
        timeout "$WALL" "$BIN" \
          --problem "$problem" --max-n "$MAX_N" --num-vars "$NUM_VARS" \
          --beam-width "$BEAM" --candidates "$CANDIDATES" --max-program-size "$SIZE" \
          --step-limit "$STEP_LIMIT" --seed "$seed" --loop-bias "$bias" \
          --post-solve-sizes "$post" \
          --output "$OUT_ROOT/solutions_$tag.json" \
          > "$OUT_ROOT/log_$tag.txt" 2>&1
        rc=$?
        echo "=== $(date '+%Y-%m-%d %H:%M:%S') finished $tag (exit=$rc) ===" | tee -a "$OUT_ROOT/driver.log"
      done
    done
  done
done

echo "HELDOUT_DIVERGENCE_AB_DONE $(date '+%Y-%m-%d %H:%M:%S')" | tee -a "$OUT_ROOT/driver.log"
