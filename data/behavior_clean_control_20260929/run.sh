#!/usr/bin/env bash
# FINDINGS.md section 36 control re-run with the clean-novelty tally (no role typing).
# ALGOSEARCH_DEBUG_BEHAVIOR=1, no behavior change. Run from repo root.
cd "$(dirname "$0")/../.."
OUT=data/behavior_clean_control_20260929
job() { tag=$1; shift; ALGOSEARCH_DEBUG_BEHAVIOR=1 timeout 400 ./algosearch "$@" --threads 2 --output $OUT/sol_$tag.json > $OUT/log_$tag.txt 2> $OUT/err_$tag.txt; }
B="--beam-width 500 --candidates 1500 --max-program-size 150 --step-limit 5000"
for seed in 1000 1001; do
  job iso_sum_$seed      --problem sum --max-n 8 --num-vars 4 --loop-bias 0.0 $B --seed $seed &
  job iso_min_$seed      --problem min --max-n 8 --num-vars 4 --loop-bias 0.0 $B --seed $seed &
  job iso_sorting_$seed  --problem sorting --max-n 5 --num-vars 4 --loop-bias 0.0 $B --seed $seed &
  job iso_gcd_$seed      --problem gcd --max-n 12 --num-vars 4 --loop-bias 0.0 $B --seed $seed &
  wait
  job def_reverse_$seed   --problem reverse --max-n 10 --num-vars 4 $B --seed $seed &
  job def_factorial_$seed --problem factorial --max-n 15 --num-vars 5 $B --seed $seed &
  job comp_substring_$seed --problem substring --max-n 6 --num-vars 8 --loop-bias 0.0 --composite-bias 1.0 --seed $seed &
  job xover_min_$seed    --problem min --max-n 8 --num-vars 4 --loop-bias 0.0 $B --crossover-rate 0.2 --seed $seed &
  wait
done
echo DONE
