#!/usr/bin/env bash
# FINDINGS.md section 38 gate: clean new-behavior rate, isolation arm, all arms at --num-vars 8
# (the combinator's binding context needs it; control and composite matched on it).
cd "$(dirname "$0")/../.."
OUT=data/combinator_gate_20260929
B="--beam-width 500 --candidates 1500 --max-program-size 150 --step-limit 5000 --loop-bias 0.0 --num-vars 8"
job() { arm=$1; tag=$2; shift 2; ALGOSEARCH_DEBUG_BEHAVIOR=1 timeout 400 ./algosearch "$@" $B --threads 2 --output $OUT/$arm/sol_$tag.json > $OUT/$arm/log_$tag.txt 2> $OUT/$arm/err_$tag.txt; }
for arm in control composite combinator; do
  mkdir -p $OUT/$arm
  case $arm in control) F="";; composite) F="--composite-bias 1.0";; combinator) F="--combinator-bias 0.5";; esac
  for seed in 1000 1001; do
    job $arm sum_$seed --problem sum --max-n 8 --seed $seed $F &
    job $arm min_$seed --problem min --max-n 8 --seed $seed $F &
    job $arm contains_$seed --problem contains --max-n 8 --seed $seed $F &
    job $arm reverse_$seed --problem reverse --max-n 8 --seed $seed $F &
    wait
    job $arm sorting_$seed --problem sorting --max-n 5 --seed $seed $F &
    job $arm count_$seed --problem count --max-n 8 --seed $seed $F &
    job $arm single_number_$seed --problem single_number --max-n 6 --seed $seed $F &
    job $arm max_$seed --problem max --max-n 8 --seed $seed $F &
    wait
  done
done
echo DONE
