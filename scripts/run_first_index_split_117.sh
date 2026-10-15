#!/usr/bin/env bash
# FINDINGS.md section 117: make test, screen (base / FI / FIPS), then joint
# battery (FIPS), in one detached process.
set -uo pipefail
cd "$(dirname "$0")/.."
S=data/first_index_split_screen_20261014
J=data/first_index_split_joint_battery_20261014
mkdir -p "$S" "$J"
make test > "$J/make_test.log" 2>&1
echo "MAKE_TEST exit=$? $(tail -1 "$J/make_test.log")" >> "$J/driver.log"
run() {  # arm problem maxn nv seed dir
  arm=$1; problem=$2; maxn=$3; nv=$4; seed=$5; dir=$6
  extra=""
  [ "$nv" != - ] && extra="--num-vars $nv"
  case $arm in FI) extra="$extra --first-index" ;; FIPS) extra="$extra --first-index --prefix-split" ;; esac
  wall=180; [ "$nv" != - ] && wall=240
  timeout $wall ./algosearch --problem $problem --max-n $maxn --seed $seed --threads 1 --beam-width 500 \
    --candidates 1500 --max-program-size 150 --step-limit 20000 $extra --benchmark --benchmark-seeds 1 \
    > $dir/log_${arm}_${problem}_${seed}.txt 2>&1
  echo "$arm $problem $seed exit=$? $(date +%T)" >> $dir/driver.log
}
export -f run
SCREEN="search_rotated first_greater_than_first equilibrium_point count_balance_points second_index_of_key first_split_two_to_one"
for p in $SCREEN; do for arm in base FI FIPS; do for s in $(seq 62000 62009); do
  echo "$arm $p 8 - $s $S"; done; done; done | xargs -P 8 -L1 bash -c 'run "$@"' _
echo "SCREEN_DONE $(date +%T)" >> "$S/driver.log"
LIST=$(sed -n '/^LIST="/,/"$/p' scripts/run_reference_battery.sh | tr -d '"\\' | sed 's/^LIST=//')
OLD16="sum_excess_over_first sum_of_drops sum_squared_steps sum_abs_from_last sum_unique_values even_frequency_values \
max_repeated_value triple_frequency_values min_repeated_value longest_increasing_run_end frequent_mode_or_none \
longest_run_start plurality_value peak_element search_rotated equilibrium_point"
NEW4="first_greater_than_first second_index_of_key count_balance_points first_split_two_to_one"
{ for job in $LIST; do IFS=: read p maxn nv wall <<EOT
$job
EOT
    for s in $(seq 55000 55009); do echo "FIPS $p $maxn $nv $s $J"; done; done
  for p in $OLD16; do for s in $(seq 55000 55009); do echo "FIPS $p 8 - $s $J"; done; done
  for p in $NEW4; do for arm in base FIPS; do for s in $(seq 55000 55009); do echo "$arm $p 8 - $s $J"; done; done; done; } \
  | xargs -P 8 -L1 bash -c 'run "$@"' _
echo "FIPS117_ALL_DONE $(date +%T)" >> "$J/driver.log"
