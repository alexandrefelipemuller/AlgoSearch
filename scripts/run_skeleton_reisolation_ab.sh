#!/usr/bin/env bash
# FINDINGS.md section 99: re-run section 56's skeleton isolation against the
# current default (seven pieces promoted since). One arm per indispensable
# skeleton (or pair), each disabling only that skeleton, run on the problems
# section 56 said it serves; base runs every problem. Matched fresh seeds.
#
# Usage: scripts/run_skeleton_reisolation_ab.sh output_root
# Env: SEEDS (10), SEED_BASE (45000), JOBS (8).

set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:?output_root}"
SEEDS="${SEEDS:-10}"; SEED_BASE="${SEED_BASE:-45000}"; JOBS="${JOBS:-8}"
mkdir -p "$D"
# arm=disabled skeletons ("-" = none) | problems as problem:max-n:num-vars:wall
ARMS="base=-=average:10:5:240,factorial:15:5:240,power:10:6:240,sorting:5:4:240,count_distinct:8:-:180,min_gap:8:-:180,max_gap_sorted:8:-:180,three_sum:8:6:240,gcd:20:4:240,fibonacci:20:4:240,is_prime:20:5:240,graph_reachability:6:6:240
noavg=average=average:10:5:240
normul=repeatedmultiply=factorial:15:5:240,power:10:6:240
nosel=selectionsort=sorting:5:4:240,count_distinct:8:-:180,min_gap:8:-:180,max_gap_sorted:8:-:180
no3sum=threesum=three_sum:8:6:240
norec=recurrence=gcd:20:4:240,fibonacci:20:4:240
nondiv=nesteddivisor,nesteddivisornative=is_prime:20:5:240
nograph=graphreachability=graph_reachability:6:6:240"
for line in $ARMS; do
  IFS='=' read arm dis jobs <<EOT
$line
EOT
  for job in $(echo "$jobs" | tr , ' '); do for s in $(seq 0 $((SEEDS - 1))); do
    echo "$arm $dis $job $((SEED_BASE + s))"
  done; done
done | xargs -P "$JOBS" -L1 sh -c '
  arm=$0; dis=$1; job=$2; seed=$3
  IFS=: read problem maxn nv wall <<EOT
$job
EOT
  set --
  [ "$nv" != - ] && set -- "$@" --num-vars "$nv"
  [ "$dis" != - ] && export ALGOSEARCH_DISABLE_SKELETON="$dis"
  timeout $wall ./algosearch --problem $problem --max-n $maxn --seed $seed --threads 1 --beam-width 500 \
    --candidates 1500 --max-program-size 150 --step-limit 20000 "$@" --benchmark --benchmark-seeds 1 \
    > '"$D"'/log_${arm}_${problem}_${seed}.txt 2>&1
  echo "$arm $problem $seed exit=$? $(date +%T)" >> '"$D"'/driver.log'
echo "REISOLATION_DONE $(date +%T)" >> "$D/driver.log"
