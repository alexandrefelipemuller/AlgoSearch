#!/usr/bin/env bash
# FINDINGS.md section 91: cumulative time audit. OLD = section 61's default
# (the nine pieces promoted since then all off) vs NEW = current default.
# One --benchmark --benchmark-seeds 1 process per run.
#
# Usage: scripts/run_time_audit.sh output_root
# Env: SEEDS (10), SEED_BASE (44000), JOBS (8).

set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:?output_root}"
SEEDS="${SEEDS:-10}"; SEED_BASE="${SEED_BASE:-44000}"; JOBS="${JOBS:-8}"
mkdir -p "$D"
LIST=$(sed -n '/^LIST="/,/"$/p' scripts/run_distinct_ab.sh | tr -d '\\\n' | sed 's/^LIST="//; s/"$//')
LIST="$LIST walk_length:5:-:180 visits_zero:5:-:180 cycle_length:5:-:180 last_node:5:-:180 cycle_entry:5:-:180 second_last_node:5:-:180"
for job in $LIST; do for arm in OLD NEW; do for s in $(seq 0 $((SEEDS - 1))); do
  echo "$arm $job $((SEED_BASE + s))"
done; done; done | xargs -P "$JOBS" -L1 sh -c '
  arm=$0; job=$1; seed=$2
  IFS=: read problem maxn nv wall <<EOT
$job
EOT
  set --
  [ "$nv" != - ] && set -- "$@" --num-vars "$nv"
  [ "$arm" = OLD ] && set -- "$@" --no-combinator-state --no-combinator-nested --no-combinator-frequency \
    --no-follow-pointer-inputs --no-combinator-orthogonal --no-combinator-argbest --no-combinator-distinct \
    --regression-guard 0 --no-follow-extended
  timeout $wall ./algosearch --problem $problem --max-n $maxn --seed $seed --threads 1 --beam-width 500 \
    --candidates 1500 --max-program-size 150 --step-limit 20000 "$@" --benchmark --benchmark-seeds 1 \
    > '"$D"'/log_${arm}_${problem}_${seed}.txt 2>&1
  echo "$arm $problem $seed exit=$? $(date +%T)" >> '"$D"'/driver.log'
echo "TIME_AUDIT_DONE $(date +%T)" >> "$D/driver.log"
