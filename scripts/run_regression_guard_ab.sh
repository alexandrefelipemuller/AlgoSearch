#!/usr/bin/env bash
# FINDINGS.md section 89: base (current default) vs RG (--regression-guard 12).
# (i) min_gap and max_gap_sorted, 30 matched seeds; (ii) every registered
# array/scalar problem, 10 matched seeds. One --benchmark --benchmark-seeds 1
# process per run.
#
# Usage: scripts/run_regression_guard_ab.sh output_root
# Env: JOBS (8).

set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:?output_root}"; JOBS="${JOBS:-8}"
mkdir -p "$D/i" "$D/ii"
for problem in min_gap max_gap_sorted; do for arm in base RG; do for s in $(seq 40000 40029); do
  echo "$arm $problem:8:-:180 $s i"
done; done; done > "$D/jobs.txt"
LIST=$(sed -n '/^LIST="/,/"$/p' scripts/run_distinct_ab.sh | tr -d '\\\n' | sed 's/^LIST="//; s/"$//')
for job in $LIST; do for arm in base RG; do for s in $(seq 41000 41009); do
  echo "$arm $job $s ii"
done; done; done >> "$D/jobs.txt"
xargs -P "$JOBS" -L1 sh -c '
  arm=$0; job=$1; seed=$2; part=$3
  IFS=: read problem maxn nv wall <<EOT
$job
EOT
  set --
  [ "$nv" != - ] && set -- "$@" --num-vars "$nv"
  [ "$arm" = RG ] && set -- "$@" --regression-guard 12
  timeout $wall ./algosearch --problem $problem --max-n $maxn --seed $seed --threads 1 --beam-width 500 \
    --candidates 1500 --max-program-size 150 --step-limit 20000 "$@" --benchmark --benchmark-seeds 1 \
    > '"$D"'/$part/log_${arm}_${problem}_${seed}.txt 2>&1
  echo "$part $arm $problem $seed exit=$? $(date +%T)" >> '"$D"'/driver.log' < "$D/jobs.txt"
echo "REGRESSION_GUARD_DONE $(date +%T)" >> "$D/driver.log"
