#!/usr/bin/env bash
# Section 118b prototype: sorting with swap forbidden, alone vs + novelty + ban.
# Same profile as section 98 (sorting, 20 min per seed). Arms run in parallel,
# 4 threads each. Usage: scripts/run_forbid_swap_proto.sh [output_root] [seed] [wall_seconds]
set -uo pipefail
cd "$(dirname "$0")/.."
OUT_ROOT="${1:-data/forbid_swap_proto_$(date +%Y%m%d)}"
SEED="${2:-1}"
WALL="${3:-1200}"
mkdir -p "$OUT_ROOT"
run() {
  local arm="$1"; shift
  mkdir -p "$OUT_ROOT/$arm"
  stdbuf -oL -eL ./algosearch --problem sorting --max-n 3000 --num-vars 8 \
    --beam-width 500 --candidates 3000 --max-program-size 2000000 \
    --step-limit 8000 --seed "$SEED" --loop-bias 0.4 \
    --post-solve-sizes 150 --composite-bias 0.15 --recursion-call-bias 0.1 \
    --second-array-bias 0.05 --diversity-rate 0.1 --crossover-rate 0.05 \
    --shape-niche-rate 0.05 --behavior-niche-rate 0.05 --speed-bonus-weight 0.02 \
    --threads 4 --forbid-stmt swap "$@" --wall-seconds "$WALL" \
    --output "$OUT_ROOT/$arm/solutions.json" --checkpoint-path "$OUT_ROOT/$arm/solutions.json" \
    > "$OUT_ROOT/$arm/log.txt" 2>&1
}
run noswap &
run noswap_ban --profile-novelty 0.5 --profile-ban &
wait
echo "FORBID_SWAP_PROTO_DONE $(date '+%Y-%m-%d %H:%M:%S')" | tee -a "$OUT_ROOT/driver.log"
