#!/usr/bin/env bash
# A/B test: adaptiveSkeletonBiasRate vs. the fixed loopAccumulatorBias=0.4
# default, matched conditions (same seeds, same problems, same time
# budget), per the experiment design in the conversation that added
# GenConfig::adaptiveSkeletonBiasRate. See FINDINGS.md section 5-7 for the
# background this builds on before reading results.
#
# Arms:
#   baseline   -- loop-bias=0.4 fixed, adaptive-skeleton-bias-rate=0.0
#                 (must reproduce today's default behavior exactly)
#   adapt_r01  -- loop-bias starts at 0.4, adaptive rate 0.1 (slow EMA)
#   adapt_r03  -- loop-bias starts at 0.4, adaptive rate 0.3 (fast EMA)
#
# Problems: min, max, sum, contains (NOT sorting -- sorting has a known
# budget mismatch, see FINDINGS.md section 8; do that as a separate,
# larger-budget follow-up once this closes).
# Seeds: 1..5, identical across arms.
# Budget: 300s wall-clock per run (matches the budget used for the
# min/max/sum/contains comparisons already in FINDINGS.md), enforced via
# `timeout` since the engine has no internal wall-clock cutoff -- a
# candidate that times out produces no solutions.json (see FINDINGS.md
# section 8, same failure mode as budget-starved sorting runs).
#
# Usage: scripts/run_adaptive_bias_experiment.sh [output_root]
# Output: <output_root>/<arm>/sol_<problem>_s<seed>.json
#         <output_root>/<arm>/log_<problem>_s<seed>.csv   (adaptive bias curve; adapt arms only)
#         <output_root>/<arm>/stdout_<problem>_s<seed>.txt
#         <output_root>/summary.csv                        (one row per run)

set -euo pipefail
cd "$(dirname "$0")/.."

OUT_ROOT="${1:-data/adaptive_bias}"
BIN=./algosearch
TIME_BUDGET=300

PROBLEMS=(min max sum contains)
SEEDS=(1 2 3 4 5)

# arm_name:loop_bias:adaptive_rate
ARMS=(
  "baseline:0.4:0.0"
  "adapt_r01:0.4:0.1"
  "adapt_r03:0.4:0.3"
)

COMMON_ARGS=(--max-n 5 --beam-width 500 --max-program-size 30 --step-limit 5000 --candidates 800 --num-vars 4)

echo "Building..."
make -j"$(nproc)" >/dev/null

mkdir -p "$OUT_ROOT"
SUMMARY="$OUT_ROOT/summary.csv"
echo "arm,problem,seed,loop_bias_start,adaptive_rate,run_time_s,status,first_level,confirmed_levels,program_size,training_score,heldout_score,final_status,final_loop_bias,final_skeleton_rate,generalized" > "$SUMMARY"

total=$(( ${#ARMS[@]} * ${#PROBLEMS[@]} * ${#SEEDS[@]} ))
n=0

for arm_spec in "${ARMS[@]}"; do
  IFS=':' read -r arm loop_bias rate <<< "$arm_spec"
  arm_dir="$OUT_ROOT/$arm"
  mkdir -p "$arm_dir"

  for problem in "${PROBLEMS[@]}"; do
    for seed in "${SEEDS[@]}"; do
      n=$((n + 1))
      tag="${problem}_s${seed}"
      sol_path="$arm_dir/sol_${tag}.json"
      log_path="$arm_dir/log_${tag}.csv"
      stdout_path="$arm_dir/stdout_${tag}.txt"
      echo "[$n/$total] arm=$arm problem=$problem seed=$seed"

      start=$(date +%s.%N)
      set +e
      timeout "$TIME_BUDGET" "$BIN" --problem "$problem" --seed "$seed" \
        "${COMMON_ARGS[@]}" \
        --loop-bias "$loop_bias" \
        --adaptive-skeleton-bias-rate "$rate" \
        --adaptive-bias-log "$log_path" \
        --output "$sol_path" \
        > "$stdout_path" 2>&1
      rc=$?
      set -e
      end=$(date +%s.%N)
      run_time=$(awk -v a="$start" -v b="$end" 'BEGIN{printf "%.1f", b-a}')

      if [ "$rc" -eq 124 ]; then
        status="TIMEOUT"
      elif [ "$rc" -ne 0 ]; then
        status="ERROR_rc${rc}"
      elif [ ! -s "$sol_path" ]; then
        status="NO_SOLUTION"
      else
        status="OK"
      fi

      if [ "$status" = "OK" ]; then
        # Last record = furthest level reached in the last promotion cascade.
        read -r first_level confirmed_levels program_size training_score heldout_score final_status <<< "$(
          jq -r '.[-1] | "\(.first_level) \(.confirmed_levels) \(.program_size) \(.training_score) \(.heldout_score) \(.final_status)"' "$sol_path"
        )"
      else
        first_level="NA"; confirmed_levels="NA"; program_size="NA"
        training_score="NA"; heldout_score="NA"; final_status="NA"
      fi

      final_loop_bias="NA"; final_skeleton_rate="NA"
      if [ -s "$log_path" ]; then
        last_row=$(tail -n 1 "$log_path")
        if [ "$last_row" != "round,level,bias_before,skeleton_rate,bias_after,best_score" ]; then
          final_loop_bias=$(echo "$last_row" | awk -F',' '{print $5}')
          final_skeleton_rate=$(echo "$last_row" | awk -F',' '{print $4}')
        fi
      fi

      # NOTE: deliberately does NOT require final_status=="SUCCESS" --
      # FINDINGS.md section 4 documents invarianceLookahead as pure
      # bookkeeping (it labels a level but never gates search progression),
      # and a program can be a genuine full-generalizer yet still show
      # UNCONFIRMED simply because maxLevel is too close to firstLevel for
      # the lookahead window to fit. The held-out score matching training
      # 100% is the actual generalization signal, same definition used in
      # data/bias_map_summary.csv.
      generalized="no"
      if [ "$status" = "OK" ]; then
        train_num="${training_score%%/*}"; train_den="${training_score##*/}"
        held_num="${heldout_score%%/*}"; held_den="${heldout_score##*/}"
        if [ "$train_num" = "$train_den" ] && [ "$held_num" = "$held_den" ] && [ "$held_den" != "?" ]; then
          generalized="yes"
        fi
      fi

      echo "$arm,$problem,$seed,$loop_bias,$rate,$run_time,$status,$first_level,$confirmed_levels,$program_size,$training_score,$heldout_score,$final_status,$final_loop_bias,$final_skeleton_rate,$generalized" >> "$SUMMARY"
    done
  done
done

echo
echo "Done. Summary: $SUMMARY"
echo "Combined generalization rate per arm:"
awk -F',' 'NR>1 {tot[$1]++; if ($NF=="yes") gen[$1]++} END {for (a in tot) printf "  %-12s %d/%d\n", a, gen[a]+0, tot[a]}' "$SUMMARY"
