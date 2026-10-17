#!/usr/bin/env bash
# Section 118c A/B: --profile-novelty 0.5 (control) vs + --profile-ban, matched
# seeds, both arms in parallel (4 threads each), seeds sequential.
# PROBLEM / MAX_N / WALL env vars override sorting / 3000 / 1200.
# Usage: scripts/run_profile_ban_ab.sh output_root seed...
set -uo pipefail
cd "$(dirname "$0")/.."
OUT_ROOT="$1"; shift
PROBLEM="${PROBLEM:-sorting}"; MAX_N="${MAX_N:-3000}"; WALL="${WALL:-1200}"
mkdir -p "$OUT_ROOT"
run() {
  local arm="$1" seed="$2"; shift 2
  local dir="$OUT_ROOT/seed_$seed/$arm"
  mkdir -p "$dir"
  stdbuf -oL -eL ./algosearch --problem "$PROBLEM" --max-n "$MAX_N" --num-vars 8 \
    --beam-width 500 --candidates 3000 --max-program-size 2000000 \
    --step-limit 8000 --seed "$seed" --loop-bias 0.4 \
    --post-solve-sizes 150 --composite-bias 0.15 --recursion-call-bias 0.1 \
    --second-array-bias 0.05 --diversity-rate 0.1 --crossover-rate 0.05 \
    --shape-niche-rate 0.05 --behavior-niche-rate 0.05 --speed-bonus-weight 0.02 \
    --threads 4 --profile-novelty 0.5 "$@" --wall-seconds "$WALL" \
    --output "$dir/solutions.json" --checkpoint-path "$dir/solutions.json" \
    > "$dir/log.txt" 2>&1
}
for seed in "$@"; do
  echo "=== $(date '+%F %T') seed $seed starting ===" | tee -a "$OUT_ROOT/driver.log"
  run novelty "$seed" &
  run novelty_ban "$seed" --profile-ban &
  wait
done
echo "PROFILE_BAN_AB_DONE $(date '+%F %T')" | tee -a "$OUT_ROOT/driver.log"
