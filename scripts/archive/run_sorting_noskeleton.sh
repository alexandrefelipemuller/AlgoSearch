#!/usr/bin/env bash
# FINDINGS.md section 45, run D: sorting with ALL hand-written sort skeletons
# disabled (selection/heap/merge) and the combinator off -- can the search
# discover a general sort on its own? 8 seeds x 2 h wall, 4 in parallel.
# Each seed writes a checkpointed solutions file, so a timeout loses nothing.
#
# Usage: scripts/run_sorting_noskeleton.sh [output_root]

set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:-data/sorting_noskeleton_$(date +%Y%m%d)}"
mkdir -p "$D"
export ALGOSEARCH_DISABLE_SKELETON=selectionsort,heapsort,mergesort
seq 20000 20007 | xargs -P 4 -I{} sh -c "timeout 7200 stdbuf -oL ./algosearch --problem sorting --max-n 100 --num-vars 10 --beam-width 500 --candidates 1500 --max-program-size 3000 --step-limit 50000 --seed {} --threads 2 --diversity-rate 0.2 --shape-niche-rate 0.2 --behavior-niche-rate 0.15 --score-band-rate 0.15 --crossover-rate 0.3 --post-solve-sizes 15 --composite-bias 0.3 --recursion-call-bias 0.1 --second-array-bias 0.05 --combinator-bias 0 --output $D/solutions_{}.json --checkpoint-path $D/solutions_{}.json > $D/log_{}.txt 2>&1; echo seed {} exit=\$? \$(date +%T)" >> "$D/driver.log"
echo "NOSKELETON_DONE $(date +%T)" >> "$D/driver.log"
