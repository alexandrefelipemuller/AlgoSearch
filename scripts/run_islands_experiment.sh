#!/usr/bin/env bash
# Islands experiment, take 2. The first run confounded the test: it
# included loop-bias=0.4 as one of the islands, which was ALREADY known
# (FINDINGS.md section 9) to solve gcd/factorial cleanly on its own -- so
# island 0 just won every time regardless of migration, proving nothing
# about whether mixing strategies helps. This version deliberately
# EXCLUDES the known-good 0.4 bias from the island mix, and adds solo
# (single-population) runs at each of the mix's individual bias values as
# controls, so "islands beat every one of their own ingredients run
# alone" is a real, falsifiable claim instead of an artifact of including
# a proven winner in the mix.
#
# Scope: gcd and is_prime only -- the two problems still genuinely open
# per FINDINGS.md (factorial's earlier "wall" turned out to be CPU
# contention, not a search limitation; see section 10's correction).
set -uo pipefail
cd "$(dirname "$0")/.."

OUT_ROOT="${1:-data/islands_experiment2_$(date +%Y%m%d_%H%M)}"
mkdir -p "$OUT_ROOT"
BIN=./algosearch

MIX="0.0,0.15,0.25,0.35"          # none of these alone is the known-good 0.4
MIX_BIASES=(0.0 0.15 0.25 0.35)
MIGRATION_INTERVAL=15
MIGRATION_RATE=0.15
BEAM=500
CANDIDATES=1500
SIZE=400
STEP_LIMIT=8000
SEEDS=3
SEED_BASE=4000
WALL=300

PROBLEMS=("gcd:20:4" "is_prime:20:4")

echo "=== $(date '+%Y-%m-%d %H:%M:%S') islands experiment (take 2) starting, out=$OUT_ROOT ===" | tee "$OUT_ROOT/driver.log"

for job in "${PROBLEMS[@]}"; do
  IFS=: read -r problem maxn numvars <<< "$job"

  echo "=== $(date '+%Y-%m-%d %H:%M:%S') [$problem] islands mix=$MIX ===" | tee -a "$OUT_ROOT/driver.log"
  timeout "$WALL" "$BIN" \
    --problem "$problem" --max-n "$maxn" --num-vars "$numvars" \
    --beam-width "$BEAM" --candidates "$CANDIDATES" --max-program-size "$SIZE" \
    --step-limit "$STEP_LIMIT" --seed "$SEED_BASE" \
    --island-loop-biases "$MIX" --migration-interval "$MIGRATION_INTERVAL" --migration-rate "$MIGRATION_RATE" \
    --benchmark --benchmark-seeds "$SEEDS" \
    > "$OUT_ROOT/log_${problem}_islands.txt" 2>&1
  echo "=== $(date '+%Y-%m-%d %H:%M:%S') [$problem] islands finished (exit=$?) ===" | tee -a "$OUT_ROOT/driver.log"

  for bias in "${MIX_BIASES[@]}"; do
    echo "=== $(date '+%Y-%m-%d %H:%M:%S') [$problem] solo control loop-bias=$bias ===" | tee -a "$OUT_ROOT/driver.log"
    timeout "$WALL" "$BIN" \
      --problem "$problem" --max-n "$maxn" --num-vars "$numvars" \
      --beam-width "$BEAM" --candidates "$CANDIDATES" --max-program-size "$SIZE" \
      --step-limit "$STEP_LIMIT" --seed "$SEED_BASE" --loop-bias "$bias" \
      --benchmark --benchmark-seeds "$SEEDS" \
      > "$OUT_ROOT/log_${problem}_solo_${bias}.txt" 2>&1
    echo "=== $(date '+%Y-%m-%d %H:%M:%S') [$problem] solo loop-bias=$bias finished (exit=$?) ===" | tee -a "$OUT_ROOT/driver.log"
  done
done

echo "ISLANDS_EXPERIMENT2_DONE $(date '+%Y-%m-%d %H:%M:%S')" | tee -a "$OUT_ROOT/driver.log"
