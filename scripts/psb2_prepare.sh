#!/usr/bin/env bash
# Section 126: downloads the official PSB2 datasets (Helmuth and Kelly) for the seven integer problems run here.
# The edge cases are small; the random file has a million lines, so only the first ~6 MB (about 50,000 lines) are taken.
# Usage: scripts/psb2_prepare.sh [dir]   (default data/psb2; point ALGOSEARCH_PSB2_DIR at it)
set -euo pipefail
D="${1:-data/psb2}"
for p in fuel-cost basement gcd find-pair coin-sums luhn paired-digits; do
  mkdir -p "$D/$p"
  base="https://psb2-datasets.s3.amazonaws.com/PSB2/datasets/$p/$p"
  curl -sf -o "$D/$p/$p-edge.json" "$base-edge.json"
  curl -sf -r 0-6000000 -o "$D/$p/$p-random.json" "$base-random.json"
  sed -i '$d' "$D/$p/$p-random.json"   # the range cut leaves a partial last line
  echo "$p edge=$(wc -l < "$D/$p/$p-edge.json") random=$(wc -l < "$D/$p/$p-random.json")"
done
