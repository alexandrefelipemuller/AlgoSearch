#!/usr/bin/env bash
# Many short sorting runs (FINDINGS.md section 93): section 92's profile
# (scripts/run_sorting_20h.sh), one run per seed, sequential since each run
# uses every thread. Tests whether algorithmic diversity comes from seed
# variety rather than from one long budget, which section 92 showed stalls.
# Triage each with scripts/triage_sorting.py.
#
# PROBLEM and MAX_N env vars override sorting / 3000 (section 96); EXTRA
# appends flags (section 97).
# Usage: scripts/run_sorting_seeds.sh [output_root] [wall_seconds_per_seed] [seeds...]

set -uo pipefail
cd "$(dirname "$0")/.."

OUT_ROOT="${1:-data/sorting_seeds_$(date +%Y%m%d_%H%M)}"
WALL="${2:-1200}"
shift 2 2>/dev/null
SEEDS=("$@")
[ $# -eq 0 ] && SEEDS=(1 2 3 4 5 6 7 8)
mkdir -p "$OUT_ROOT"

for seed in "${SEEDS[@]}"; do
  dir="$OUT_ROOT/seed_$seed"
  mkdir -p "$dir"
  echo "=== $(date '+%Y-%m-%d %H:%M:%S') seed $seed starting wall=${WALL}s ===" | tee -a "$OUT_ROOT/driver.log"
  timeout "$WALL" stdbuf -oL -eL ./algosearch \
    --problem "${PROBLEM:-sorting}" --max-n "${MAX_N:-3000}" --num-vars 8 \
    --beam-width 500 --candidates 3000 --max-program-size 20000 \
    --step-limit 8000 --seed "$seed" --loop-bias 0.4 \
    --post-solve-sizes 150 --composite-bias 0.15 --recursion-call-bias 0.1 \
    --second-array-bias 0.05 --diversity-rate 0.1 --crossover-rate 0.05 \
    --shape-niche-rate 0.05 --behavior-niche-rate 0.05 --speed-bonus-weight 0.02 \
    --threads 0 ${EXTRA:-} \
    --output "$dir/solutions.json" --checkpoint-path "$dir/solutions.json" \
    > "$dir/log.txt" 2>&1
  rc=$?
  echo "=== $(date '+%Y-%m-%d %H:%M:%S') seed $seed finished (exit=$rc) ===" | tee -a "$OUT_ROOT/driver.log"
done
echo "SORTING_SEEDS_DONE $(date '+%Y-%m-%d %H:%M:%S')" | tee -a "$OUT_ROOT/driver.log"
