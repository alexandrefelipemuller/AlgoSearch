#!/usr/bin/env bash
# FINDINGS.md section 65: single-hole ablation of every indispensable
# skeleton on the problem(s) it serves (section 56). One --ablate-skeleton
# run per pair, sequential (each uses all threads).
#
# Usage: scripts/run_skeleton_ablation.sh output_root
# Env: SEED (1).

set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:?output_root}"; SEED="${SEED:-1}"
mkdir -p "$D"
# skeleton:problem:max-n:num-vars
for job in recurrence:gcd:20:4 recurrence:fibonacci:20:4 nesteddivisor:is_prime:20:5 \
           nesteddivisornative:is_prime:20:5 repeatedmultiply:factorial:15:5 repeatedmultiply:power:10:6 \
           average:average:10:5 threesum:three_sum:8:6 graphreachability:graph_reachability:6:6 \
           selectionsort:sorting:5:4; do
  IFS=: read sk problem maxn nv <<EOT
$job
EOT
  timeout 3600 ./algosearch --problem "$problem" --max-n "$maxn" --num-vars "$nv" --seed "$SEED" --threads 8 \
    --ablate-skeleton "$sk" > "$D/${sk}_${problem}.txt" 2>&1
  echo "$sk $problem exit=$? $(date +%T)" >> "$D/driver.log"
done
echo "ABLATION_DONE $(date +%T)" >> "$D/driver.log"
