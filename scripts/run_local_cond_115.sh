#!/usr/bin/env bash
# FINDINGS.md section 115: make test, then screen, then joint battery for
# --local-cond-mutation 0.3, in one detached process.
#
# Usage: scripts/run_local_cond_115.sh
set -uo pipefail
cd "$(dirname "$0")/.."
S=data/local_cond_screen_20261014
J=data/local_cond_joint_battery_20261014
mkdir -p "$S" "$J"
make test > "$J/make_test.log" 2>&1
echo "MAKE_TEST exit=$? $(tail -1 "$J/make_test.log")" >> "$J/driver.log"
run() {  # arm problem maxn nv seed dir
  set -- "$@"
  arm=$1; problem=$2; maxn=$3; nv=$4; seed=$5; dir=$6
  extra=""
  [ "$nv" != - ] && extra="--num-vars $nv"
  [ "$arm" = LC ] && extra="$extra --local-cond-mutation 0.3"
  wall=180; [ "$nv" != - ] && wall=240
  timeout $wall ./algosearch --problem $problem --max-n $maxn --seed $seed --threads 1 --beam-width 500 \
    --candidates 1500 --max-program-size 150 --step-limit 20000 $extra --benchmark --benchmark-seeds 1 \
    > $dir/log_${arm}_${problem}_${seed}.txt 2>&1
  echo "$arm $problem $seed exit=$? $(date +%T)" >> $dir/driver.log
}
export -f run
SCREEN="majority_value frequent_mode_or_none peak_element search_rotated equilibrium_point"
for p in $SCREEN; do for arm in base LC; do for s in $(seq 60000 60009); do
  echo "$arm $p 8 - $s $S"; done; done; done | xargs -P 8 -L1 bash -c 'run "$@"' _
echo "SCREEN_DONE $(date +%T)" >> "$S/driver.log"
# Joint battery list: section 110's 73 problems, then the 16 added since.
LIST=$(sed -n '/^LIST="/,/"$/p' scripts/run_reference_battery.sh | tr -d '"\\' | sed 's/^LIST=//')
NEW="sum_excess_over_first sum_of_drops sum_squared_steps sum_abs_from_last sum_unique_values even_frequency_values \
max_repeated_value triple_frequency_values min_repeated_value longest_increasing_run_end frequent_mode_or_none \
longest_run_start plurality_value peak_element search_rotated equilibrium_point"
{ for job in $LIST; do IFS=: read p maxn nv wall <<EOT
$job
EOT
    for s in $(seq 55000 55009); do echo "LC $p $maxn $nv $s $J"; done; done
  for p in $NEW; do for arm in base LC; do for s in $(seq 55000 55009); do echo "$arm $p 8 - $s $J"; done; done; done; } \
  | xargs -P 8 -L1 bash -c 'run "$@"' _
echo "LC115_ALL_DONE $(date +%T)" >> "$J/driver.log"
