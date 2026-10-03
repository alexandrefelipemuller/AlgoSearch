#!/usr/bin/env bash
# three_sum with high diversity, many seeds, large budget, --benchmark per seed.
# Two arms: skel (default grammar, ThreeSum skeleton available) and noskel
# (ALGOSEARCH_DISABLE_SKELETON=threesum: only the combinator/free grammar).
# One seed per process with its own wall, so a stalled seed (growth
# corruption) only loses itself -- --benchmark prints nothing until the end.
#
# Usage: scripts/run_three_sum_diversity.sh output_root
# Env: SEEDS (48), SEED_BASE (40000), WALL (900), JOBS (8).

set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:?output_root}"
SEEDS="${SEEDS:-48}"; SEED_BASE="${SEED_BASE:-40000}"; WALL="${WALL:-900}"; JOBS="${JOBS:-8}"
mkdir -p "$D"
DIV="--diversity-rate 0.2 --shape-niche-rate 0.2 --behavior-niche-rate 0.15 --score-band-rate 0.15 --crossover-rate 0.3 --post-solve-sizes 15 --composite-bias 0.3"
for arm in skel noskel; do
  for s in $(seq 0 $((SEEDS - 1))); do echo "$arm $((SEED_BASE + s))"; done
done | xargs -P "$JOBS" -L1 sh -c '
  arm=$0; seed=$1
  if [ "$arm" = noskel ]; then export ALGOSEARCH_DISABLE_SKELETON=threesum; fi
  timeout '"$WALL"' ./algosearch --problem three_sum --max-n 8 --num-vars 10 --beam-width 1000 --candidates 3000 \
    --max-program-size 300 --step-limit 20000 --seed $seed --threads 1 '"$DIV"' --benchmark --benchmark-seeds 1 \
    > '"$D"'/log_${arm}_${seed}.txt 2>&1
  echo "$arm $seed exit=$? $(date +%T)" >> '"$D"'/driver.log'
echo "THREE_SUM_DONE $(date +%T)" >> "$D/driver.log"
