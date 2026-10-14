#!/usr/bin/env bash
# FINDINGS.md section 115b: 30-seed A/B of --local-cond-mutation 0.3.
#
# Usage: scripts/run_local_cond_ab.sh output_root
set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:?output_root}"
mkdir -p "$D"
for p in majority_value frequent_mode_or_none peak_element search_rotated equilibrium_point; do
  for arm in base LC; do for s in $(seq 61000 61029); do echo "$arm $p $s"; done; done
done | xargs -P 8 -L1 sh -c '
  arm=$0; problem=$1; seed=$2
  set --
  [ "$arm" = LC ] && set -- --local-cond-mutation 0.3
  timeout 180 ./algosearch --problem $problem --max-n 8 --seed $seed --threads 1 --beam-width 500 \
    --candidates 1500 --max-program-size 150 --step-limit 20000 "$@" --benchmark --benchmark-seeds 1 \
    > '"$D"'/log_${arm}_${problem}_${seed}.txt 2>&1
  echo "$arm $problem $seed exit=$? $(date +%T)" >> '"$D"'/driver.log'
echo "LC_AB_DONE $(date +%T)" >> "$D/driver.log"
