#!/usr/bin/env bash
# FINDINGS.md section 117b: 30-seed attribution A/B of the section 117 drops.
#
# Usage: scripts/run_first_index_split_ab.sh output_root
set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:?output_root}"
mkdir -p "$D"
for p in frequent_mode_or_none peak_element elements_twice; do
  for arm in base FI PS; do for s in $(seq 63000 63029); do echo "$arm $p $s"; done; done
done | xargs -P 8 -L1 sh -c '
  arm=$0; problem=$1; seed=$2
  set --
  [ "$arm" = FI ] && set -- --first-index
  [ "$arm" = PS ] && set -- --prefix-split
  timeout 180 ./algosearch --problem $problem --max-n 8 --seed $seed --threads 1 --beam-width 500 \
    --candidates 1500 --max-program-size 150 --step-limit 20000 "$@" --benchmark --benchmark-seeds 1 \
    > '"$D"'/log_${arm}_${problem}_${seed}.txt 2>&1
  echo "$arm $problem $seed exit=$? $(date +%T)" >> '"$D"'/driver.log'
echo "FIPS_AB_DONE $(date +%T)" >> "$D/driver.log"
