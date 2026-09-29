#!/usr/bin/env bash
# FINDINGS.md section 40: combinator v1 vs v2 (--combinator-adjacent), isolation arm,
# --benchmark with the exhaustive check, 5 seeds (seed 5000). 6 targets + 12 v1-solved (dilution).
cd "$(dirname "$0")/../.."
OUT=data/combinator_v2_benchmark_20260929
B="--num-vars 8 --beam-width 500 --candidates 1500 --max-program-size 150 --step-limit 5000 --seed 5000 --benchmark --benchmark-seeds 5 --threads 2 --loop-bias 0.0 --combinator-bias 0.5"
JOBS="is_sorted:8 count_descents:8 strictly_increasing:8 has_adjacent_equal:8 longest_run:8 max_adjacent_gap:8 sum:8 min:8 max:8 count:8 contains:8 single_number:6 reverse:8 sorting:5 substring:6 palindrome:8 inversion_count:8 has_duplicate:8"
for job in $JOBS; do
  IFS=: read p n <<< "$job"
  { /usr/bin/time -f "%e" -o $OUT/v1_$p.time timeout 1500 ./algosearch --problem $p --max-n $n $B > $OUT/v1_$p.txt 2>&1; } &
  { /usr/bin/time -f "%e" -o $OUT/v2_$p.time timeout 1500 ./algosearch --problem $p --max-n $n $B --combinator-adjacent > $OUT/v2_$p.txt 2>&1; } &
  wait
done
echo DONE > $OUT/DONE
