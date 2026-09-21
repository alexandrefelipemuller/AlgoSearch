#!/usr/bin/env bash
# Long "how many different sorting algorithms can the search find and
# archive" run: sorting is known to have many structurally distinct correct
# algorithms (selection, bubble/insertion via swap-based skeletons, heap,
# merge, divide-and-conquer, ...), so this run combines the two mechanisms
# built this session (--post-solve-sizes divergence window,
# --composite-bias) with the pre-existing diversity/recursion machinery to
# see how many DISTINCT confirmed algorithms accumulate over a big budget,
# not just whether sorting is solved (it always is, in seconds, per
# FINDINGS.md).
#
# Parameters were calibrated empirically before launch (2026-09-20): a
# heavier profile (beam=2000, candidates=6000, ALL diversity/niche/crossover
# mechanisms at once) was tested first and found to cost far more than
# linear -- likely O(beam*candidates) niche/crossover bookkeeping -- so this
# script uses a smaller, VERIFIED-throughput profile (beam=500,
# candidates=3000) which produced 400+ distinct archived programs in ~90-100s
# for a single divergence window at max-program-size=300. --max-n is set
# high enough that climbing through many levels, each re-opening its own
# window, is expected to consume the wall budget -- but the search has no
# internal wall-clock cutoff (FINDINGS.md section 8), so `timeout` below is
# the actual budget control; if the ladder reaches --max-n first, the run
# ends early and reports whatever wall time it actually used, which is fine.
#
# Uses --checkpoint-path so the solutions file is overwritten after every
# level promotion, not only on natural completion -- an external
# `timeout`/interruption at any point still leaves a valid, current
# solutions.json on disk (may lag by the in-progress level, since no
# promotion follows it yet).
#
# IMPORTANT: this deliberately does NOT pass --benchmark. main.cpp's
# --benchmark flag routes to a completely different code path
# (runBenchmarkMode) that runs `--benchmark-seeds` independent, INDEPENDENT
# quick verification searches instead of the single long exploration run --
# it ignores --checkpoint-path/--output entirely and exits once all seeds
# finish (which, for a problem the skeleton grammar solves in seconds like
# sorting, is fast and produces none of the divergence-window archive this
# script exists to collect). Do not add --benchmark here.
#
# Usage: scripts/run_sorting_20h.sh [output_root] [wall_seconds]

set -uo pipefail
cd "$(dirname "$0")/.."

OUT_ROOT="${1:-data/sorting_20h_$(date +%Y%m%d_%H%M)}"
WALL="${2:-72000}"  # 20 hours
mkdir -p "$OUT_ROOT"
BIN=./algosearch

BEAM=500
CANDIDATES=3000
SIZE=20000
STEP_LIMIT=8000
MAX_N=3000
NUM_VARS=8
SEED=42

echo "=== $(date '+%Y-%m-%d %H:%M:%S') sorting 20h run starting, out=$OUT_ROOT wall=${WALL}s ===" | tee "$OUT_ROOT/driver.log"
echo "beam=$BEAM candidates=$CANDIDATES size=$SIZE step-limit=$STEP_LIMIT max-n=$MAX_N num-vars=$NUM_VARS seed=$SEED" | tee -a "$OUT_ROOT/driver.log"

timeout "$WALL" stdbuf -oL -eL "$BIN" \
  --problem sorting --max-n "$MAX_N" --num-vars "$NUM_VARS" \
  --beam-width "$BEAM" --candidates "$CANDIDATES" --max-program-size "$SIZE" \
  --step-limit "$STEP_LIMIT" --seed "$SEED" --loop-bias 0.4 \
  --post-solve-sizes 150 --composite-bias 0.15 --recursion-call-bias 0.1 \
  --second-array-bias 0.05 --diversity-rate 0.1 --crossover-rate 0.05 \
  --shape-niche-rate 0.05 --behavior-niche-rate 0.05 --speed-bonus-weight 0.02 \
  --threads 0 \
  --output "$OUT_ROOT/solutions.json" \
  --checkpoint-path "$OUT_ROOT/solutions.json" \
  > "$OUT_ROOT/log.txt" 2>&1
rc=$?

echo "=== $(date '+%Y-%m-%d %H:%M:%S') sorting 20h run finished (exit=$rc) ===" | tee -a "$OUT_ROOT/driver.log"
echo "SORTING_20H_DONE $(date '+%Y-%m-%d %H:%M:%S')" | tee -a "$OUT_ROOT/driver.log"
