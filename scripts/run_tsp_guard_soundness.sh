#!/usr/bin/env bash
# FINDINGS.md section 43 test: TSP guard enumeration with vs. without the
# per-evaluation soundness labels, selection on V = 2..6 and 2..8 (held-out
# V = maxN+1, maxN+2), two seeds for the instance generator.
# METAMORPHIC=1 adds the section 44 arm (labels + weights scaled x10, x100).
# Usage: scripts/run_tsp_guard_soundness.sh output_root
set -uo pipefail
cd "$(dirname "$0")/.."
OUT_ROOT="${1:?output_root}"
mkdir -p "$OUT_ROOT"
for seed in 42 7; do
  for maxn in 6 8; do
    ./algosearch --problem tsp --num-vars 12 --max-n $maxn --seed $seed --threads 8 --fill-tsp-guard \
      > "$OUT_ROOT/plain_seed${seed}_maxn${maxn}.txt" 2>&1
    ./algosearch --problem tsp --num-vars 12 --max-n $maxn --seed $seed --threads 8 --fill-tsp-guard --soundness-labels \
      > "$OUT_ROOT/sound_seed${seed}_maxn${maxn}.txt" 2>&1
    if [ "${METAMORPHIC:-0}" = 1 ]; then  # section 44: labels + weight scaling x{1,10,100}
      ./algosearch --problem tsp --num-vars 12 --max-n $maxn --seed $seed --threads 8 --fill-tsp-guard --soundness-labels \
        --metamorphic > "$OUT_ROOT/meta_seed${seed}_maxn${maxn}.txt" 2>&1
    fi
  done
done
grep -H "held-out\|would rank\|unsafe\|SANITY\|fastest correct" -A1 "$OUT_ROOT"/*.txt
