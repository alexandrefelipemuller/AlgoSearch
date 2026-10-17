#!/usr/bin/env bash
# Runs triage_sorting.py on every arm directory under a run root, then prints
# a per-seed table of generalizing behavior classes and core exponents.
# Usage: scripts/triage_ab_diversity.sh output_root [problem]
set -uo pipefail
cd "$(dirname "$0")/.."
ROOT="$1"; PROBLEM="${2:-sorting}"
for d in "$ROOT"/seed_*/*/; do
  [ -f "$d/triage.json" ] || python3 -I scripts/triage_sorting.py "$d" --problem "$PROBLEM" > "$d/triage.out" 2>&1
done
python3 -I - "$ROOT" <<'P'
import glob, json, os, statistics, sys
root = sys.argv[1]
by = {}
for f in sorted(glob.glob(os.path.join(root, 'seed_*', '*', 'triage.json'))):
    parts = f.split(os.sep)
    seed, arm = parts[-3], parts[-2]
    ok = [r for r in json.load(open(f))['results'] if r['status'] == 'OK']
    by.setdefault(seed, {})[arm] = {
        'classes': {r['behavior'] for r in ok},
        'families': {round(r['core_exponent'], 1) for r in ok if r.get('core_exponent') is not None},
        'call': any(r['uses_call'] for r in ok),
    }
print('seed | arm | classes | core exponents | classes only here')
tot = {}
for seed, arms in sorted(by.items()):
    for arm, v in sorted(arms.items()):
        other = [o for a, o in arms.items() if a != arm]
        only = len(v['classes'] - (other[0]['classes'] if other else set()))
        print(f"{seed} | {arm} | {len(v['classes'])} | {sorted(v['families'])} | {only}")
        tot.setdefault(arm, []).append(len(v['classes']))
print()
for arm, xs in sorted(tot.items()):
    print(f"{arm}: classes per seed {xs}  mean {statistics.mean(xs):.2f}  median {statistics.median(xs)}")
a, b = tot.get('novelty'), tot.get('novelty_ban')
if a and b and len(a) == len(b):
    d = [y - x for x, y in zip(a, b)]
    print('paired diff (ban - novelty):', d, 'wins', sum(x > 0 for x in d), 'ties', sum(x == 0 for x in d), 'losses', sum(x < 0 for x in d))
P
