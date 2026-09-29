#!/usr/bin/env bash
# FINDINGS.md section 39: post-freeze problems + held-out ruler, --benchmark (now with the
# exhaustive small-n check), 5 seeds, three arms at --num-vars 8.
cd "$(dirname "$0")/../.."
OUT=data/post_freeze_benchmark_20260929
B="--num-vars 8 --beam-width 500 --candidates 1500 --max-program-size 150 --step-limit 5000 --seed 4000 --benchmark --benchmark-seeds 5 --threads 2"
for job in palindrome:8 inversion_count:8 has_duplicate:8 is_sorted:8 cycle:5 lis:5; do
  IFS=: read p n <<< "$job"
  timeout 1500 ./algosearch --problem $p --max-n $n $B --loop-bias 0.0 > $OUT/iso_$p.txt 2>&1 &
  timeout 1500 ./algosearch --problem $p --max-n $n $B --loop-bias 0.0 --combinator-bias 0.5 > $OUT/comb_$p.txt 2>&1 &
  timeout 1500 ./algosearch --problem $p --max-n $n $B --loop-bias 0.4 > $OUT/skel_$p.txt 2>&1 &
  wait
done
echo DONE
