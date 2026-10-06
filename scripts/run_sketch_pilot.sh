#!/usr/bin/env bash
# FINDINGS.md section 68: SMT sketch-filling pilot. For each of section 66's
# 20 scrambled draws (seeds 100-119) of gcd and factorial: export the
# sketch, solve it (Z3, CEGIS), verify the fills with the real interpreter.
#
# Usage: scripts/run_sketch_pilot.sh output_root bridge_binary python_with_z3
# Env: JOBS (8), TIMEOUT (600), UNROLL (8), WIDTH (8), MAXVAL (10).

set -uo pipefail
cd "$(dirname "$0")/.."
D="${1:?output_root}"; BRIDGE="${2:?bridge}"; PY="${3:?python}"
JOBS="${JOBS:-8}"; TIMEOUT="${TIMEOUT:-600}"; UNROLL="${UNROLL:-8}"; WIDTH="${WIDTH:-8}"; MAXVAL="${MAXVAL:-10}"
mkdir -p "$D"
for p in gcd factorial; do for s in $(seq 100 119); do echo "$p $s"; done; done | \
  xargs -P "$JOBS" -L1 sh -c '
    p=$0; s=$1; base='"$D"'/${p}_${s}
    '"$BRIDGE"' export $p $s > $base.json
    timeout $(('"$TIMEOUT"' + 60)) '"$PY"' scripts/sketch_solve.py $base.json $base.fills --timeout '"$TIMEOUT"' \
      --unroll '"$UNROLL"' --width '"$WIDTH"' --max-value '"$MAXVAL"' > $base.log 2>&1
    [ -f $base.fills ] && '"$BRIDGE"' verify $p $s $base.fills >> $base.log 2>&1
    echo "$p $s $(grep -h RESULT $base.log) $(grep -h VERIFY $base.log)" >> '"$D"'/driver.log'
echo "SKETCH_PILOT_DONE $(date +%T)" >> "$D/driver.log"
