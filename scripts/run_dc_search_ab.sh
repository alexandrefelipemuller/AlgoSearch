#!/usr/bin/env bash
# FINDINGS.md section 121: one-call interval shell on any_peak. Arms base / search / search_noloop, matched seeds.
# Usage: scripts/run_dc_search_ab.sh output_root   Env: SEEDS (10), SEED_BASE (59000), JOBS (6), PROBLEM (any_peak).
set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:?output_root}"
SEEDS="${SEEDS:-10}"; SEED_BASE="${SEED_BASE:-59000}"; JOBS="${JOBS:-6}"; PROBLEM="${PROBLEM:-any_peak}"
mkdir -p "$D"
for s in $(seq 0 $((SEEDS - 1))); do for arm in ${ARMS:-base search search_noloop}; do
  echo "$arm $((SEED_BASE + s))"
done; done | xargs -P "$JOBS" -L1 sh -c '
  arm=$0; seed=$1
  case $arm in
    base) extra="" ;;
    search) extra="--dc-shell-search 0.15" ;;
    search_noloop) extra="--dc-shell-search 0.15 --no-scalar-loop --combinator-bias 0" ;;
    searchkey) extra="--dc-shell-search 0.15 --dc-shell-search-key 0.5" ;;
  esac
  mkdir -p '"$D"'/$arm
  timeout 240 ./algosearch --problem '"$PROBLEM"' --max-n 8 --seed $seed --threads 1 --beam-width 500 \
    --candidates 1500 --max-program-size 150 --step-limit 20000 --post-solve-sizes 150 --start-level 2 \
    --recursion-call-bias 0.1 --profile-novelty 0.5 --profile-ban $extra \
    --output '"$D"'/$arm/seed_$seed.json > '"$D"'/$arm/seed_$seed.log 2>&1
  echo "$arm $seed exit=$? $(date +%T)" >> '"$D"'/driver.log'
echo "DC_SEARCH_AB_DONE $(date +%T)" >> "$D/driver.log"
