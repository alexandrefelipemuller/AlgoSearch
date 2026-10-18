#!/usr/bin/env bash
# Section 119 A/B: generic divide-and-conquer shell. Arms ctrl / shell /
# shell_noloop (see FINDINGS.md section 119 amendment), matched seeds.
# Two jobs in parallel (4 threads each). Usage: scripts/run_dc_shell_ab.sh output_root
set -uo pipefail
cd "$(dirname "$0")/.."
OUT_ROOT="$1"; WALL="${WALL:-120}"
PROBLEMS="${PROBLEMS:-sum min max single_number contains}"  # lanes: first half / second half
EXTRA_FLAGS="${EXTRA_FLAGS:-}"  # e.g. --start-level 2 for problems trivial at N=1
mkdir -p "$OUT_ROOT"
COMMON="--max-n 10 --num-vars 7 --beam-width 500 --candidates 1500 --max-program-size 150 --step-limit 5000 \
  --recursion-call-bias 0.1 --post-solve-sizes 150 --profile-novelty 0.5 --profile-ban --threads 4 --wall-seconds $WALL $EXTRA_FLAGS"
run() {
  local problem="$1" seed="$2" arm="$3"; shift 3
  local dir="$OUT_ROOT/$problem/seed_$seed"; mkdir -p "$dir"
  ./algosearch --problem "$problem" --seed "$seed" $COMMON "$@" --output "$dir/$arm.json" > "$dir/$arm.log" 2>&1
}
jobs_for() {  # one problem per parallel lane
  local problem="$1"
  for seed in 3001 3002 3003 3004 3005 3006 3007 3008 3009 3010; do
    run "$problem" "$seed" ctrl
    run "$problem" "$seed" shell --dc-shell 0.15
    run "$problem" "$seed" shell_noloop --dc-shell 0.15 --no-scalar-loop --combinator-bias 0
  done
  echo "$problem done $(date '+%F %T')" >> "$OUT_ROOT/driver.log"
}
set -- $PROBLEMS
LANE_A=(); LANE_B=()
i=0; for p in "$@"; do if [ $((i % 2)) -eq 0 ]; then LANE_A+=("$p"); else LANE_B+=("$p"); fi; i=$((i+1)); done
( for p in "${LANE_A[@]}"; do jobs_for "$p"; done ) &
( for p in "${LANE_B[@]}"; do jobs_for "$p"; done ) &
wait
echo "DC_SHELL_AB_DONE $(date '+%F %T')" >> "$OUT_ROOT/driver.log"
