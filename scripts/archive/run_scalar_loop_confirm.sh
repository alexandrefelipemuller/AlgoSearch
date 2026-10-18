#!/usr/bin/env bash
# FINDINGS.md section 100, section 99 amended step 3: 30-seed confirmation
# of the four candidate skeletons. Arms: base (seeds 46010-46029; 46000-46009
# come from the A/B) and SLR (--scalar-loop with repeatedmultiply,
# recurrence, nesteddivisor and nesteddivisornative off; all 30 seeds).
#
# Usage: scripts/run_scalar_loop_confirm.sh output_root
# Env: JOBS (8).

set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:?output_root}"; JOBS="${JOBS:-8}"
mkdir -p "$D"
LIST="factorial:15:5:240 power:10:6:240 gcd:20:4:240 fibonacci:20:4:240 is_prime:20:5:240"
for job in $LIST; do
  for s in $(seq 46010 46029); do echo "base $job $s"; done
  for s in $(seq 46000 46029); do echo "SLR $job $s"; done
done | xargs -P "$JOBS" -L1 sh -c '
  arm=$0; job=$1; seed=$2
  IFS=: read problem maxn nv wall <<EOT
$job
EOT
  set -- --num-vars "$nv"
  if [ "$arm" = SLR ]; then
    set -- "$@" --scalar-loop
    export ALGOSEARCH_DISABLE_SKELETON=repeatedmultiply,recurrence,nesteddivisor,nesteddivisornative
  fi
  timeout $wall ./algosearch --problem $problem --max-n $maxn --seed $seed --threads 1 --beam-width 500 \
    --candidates 1500 --max-program-size 150 --step-limit 20000 "$@" --benchmark --benchmark-seeds 1 \
    > '"$D"'/log_${arm}_${problem}_${seed}.txt 2>&1
  echo "$arm $problem $seed exit=$? $(date +%T)" >> '"$D"'/driver.log'
echo "CONFIRM_DONE $(date +%T)" >> "$D/driver.log"
