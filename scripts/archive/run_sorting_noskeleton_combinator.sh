#!/usr/bin/env bash
# FINDINGS.md section 46: sorting with all sort skeletons disabled, combinator
# on. Arm D' = section 45's run D plus --combinator-bias 0.5 (seeds
# 20000-20007, 30 min each, 4 at a time); arm E = --benchmark diversity count
# (exhaustive check for sorting) over 200 seeds (30000-30199) in 4 processes.
#
# Usage: scripts/run_sorting_noskeleton_combinator.sh [output_root]

set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:-data/sorting_noskeleton_combinator_$(date +%Y%m%d)}"
mkdir -p "$D"
export ALGOSEARCH_DISABLE_SKELETON=selectionsort,heapsort,mergesort
DIV="--diversity-rate 0.2 --shape-niche-rate 0.2 --behavior-niche-rate 0.15 --score-band-rate 0.15 --crossover-rate 0.3 --post-solve-sizes 15 --composite-bias 0.3"
seq 20000 20007 | xargs -P 4 -I{} sh -c "timeout 1800 ./algosearch --problem sorting --max-n 100 --num-vars 10 --beam-width 500 --candidates 1500 --max-program-size 3000 --step-limit 50000 --seed {} --threads 2 $DIV --recursion-call-bias 0.1 --second-array-bias 0.05 --combinator-bias 0.5 --output $D/armD_solutions_{}.json --checkpoint-path $D/armD_solutions_{}.json > $D/armD_log_{}.txt 2>&1; echo armD seed {} exit=\$? \$(date +%T)" >> "$D/driver.log"
for k in 0 1 2 3; do
  base=$((30000 + k * 50))
  timeout 5400 ./algosearch --problem sorting --max-n 6 --num-vars 8 --beam-width 500 --candidates 1500 \
    --max-program-size 150 --step-limit 5000 --seed $base --threads 2 $DIV --combinator-bias 0.5 \
    --benchmark --benchmark-seeds 50 > "$D/armE_log_${base}.txt" 2>&1 &
done
wait
echo "armE done $(date +%T)" >> "$D/driver.log"
echo "COMBINATOR_DONE $(date +%T)" >> "$D/driver.log"
