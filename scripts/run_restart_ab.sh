#!/usr/bin/env bash
# Section 95 A/B: --restart-on-stall K inside one process vs section 93's
# 8 fixed seeds x 20 min (control, data/sorting_seeds_20261010, not rerun).
# Same profile as sections 92-93; each arm gets the same 2 h 40 of compute
# via --wall-seconds 9600; master seed fixed for replay. Arms run
# sequentially (each uses every thread). Triage with scripts/triage_sorting.py.
#
# Usage: scripts/run_restart_ab.sh [output_root] [K...]

set -uo pipefail
cd "$(dirname "$0")/.."

OUT_ROOT="${1:-data/restart_ab_$(date +%Y%m%d_%H%M)}"
shift 2>/dev/null
KS=("$@")
[ $# -eq 0 ] && KS=(200 300)
mkdir -p "$OUT_ROOT"

for k in "${KS[@]}"; do
  dir="$OUT_ROOT/k_$k"
  mkdir -p "$dir"
  echo "=== $(date '+%Y-%m-%d %H:%M:%S') K=$k starting ===" | tee -a "$OUT_ROOT/driver.log"
  stdbuf -oL -eL ./algosearch \
    --problem sorting --max-n 3000 --num-vars 8 \
    --beam-width 500 --candidates 3000 --max-program-size 2000000 \
    --step-limit 8000 --seed 1 --loop-bias 0.4 \
    --post-solve-sizes 150 --composite-bias 0.15 --recursion-call-bias 0.1 \
    --second-array-bias 0.05 --diversity-rate 0.1 --crossover-rate 0.05 \
    --shape-niche-rate 0.05 --behavior-niche-rate 0.05 --speed-bonus-weight 0.02 \
    --threads 0 --restart-on-stall "$k" --wall-seconds 9600 \
    --output "$dir/solutions.json" --checkpoint-path "$dir/solutions.json" \
    > "$dir/log.txt" 2>&1
  rc=$?
  echo "=== $(date '+%Y-%m-%d %H:%M:%S') K=$k finished (exit=$rc) ===" | tee -a "$OUT_ROOT/driver.log"
done
echo "RESTART_AB_DONE $(date '+%Y-%m-%d %H:%M:%S')" | tee -a "$OUT_ROOT/driver.log"
