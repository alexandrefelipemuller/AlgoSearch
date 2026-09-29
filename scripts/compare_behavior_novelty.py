#!/usr/bin/env python3
"""Compares two ALGOSEARCH_DEBUG_BEHAVIOR measurement dirs (FINDINGS.md sections 35-36).

Per config: new-behavior rate (1 - duplicate rate) for fresh and mutant
candidates, runtime-error and timeout share of all instance executions (from
the ordinary search log), and per-seed outcome (SUCCESS / max n_reached).

Usage: scripts/compare_behavior_novelty.py CONTROL_DIR TREATMENT_DIR
"""
import collections, glob, json, os, re, sys

def behavior(d):
    agg = collections.defaultdict(collections.Counter)
    for f in glob.glob(os.path.join(d, 'err_*.txt')):
        tag = os.path.basename(f)[4:-4].rsplit('_', 1)[0]
        for m in re.finditer(r'origin=(\w+) candidates=(\d+) dup_full=(\d+)', open(f).read()):
            agg[(tag, m.group(1))]['cand'] += int(m.group(2))
            agg[(tag, m.group(1))]['dup'] += int(m.group(3))
    return agg

def exec_share(d):
    out = collections.defaultdict(lambda: [0, 0, 0])  # executions, errors, timeouts
    for f in glob.glob(os.path.join(d, 'log_*.txt')):
        tag = os.path.basename(f)[4:-4].rsplit('_', 1)[0]
        inst = 0
        for line in open(f):
            m = re.match(r'instances = (\d+)', line)
            if m: inst = int(m.group(1))
            m = re.search(r'candidates = (\d+) .*timeouts = (\d+) \| errors = (\d+)', line)
            if m:
                out[tag][0] += int(m.group(1)) * inst
                out[tag][1] += int(m.group(3))
                out[tag][2] += int(m.group(2))
    return out

def outcomes(d):
    out = collections.defaultdict(list)
    for f in sorted(glob.glob(os.path.join(d, 'sol_*.json'))):
        tag = os.path.basename(f)[4:-5].rsplit('_', 1)[0]
        try: recs = json.load(open(f))
        except ValueError: continue
        succ = any(r.get('final_status') == 'SUCCESS' for r in recs)
        n = max((r.get('n_reached', 0) for r in recs), default=0)
        out[tag].append(('S' if succ else '-') + str(n))
    return out

c, t = sys.argv[1], sys.argv[2]
bc, bt, ec, et, oc, ot = behavior(c), behavior(t), exec_share(c), exec_share(t), outcomes(c), outcomes(t)
print(f"{'config':16}{'new fresh off/on':>18}{'new mutant off/on':>19}{'errors off/on':>16}{'timeouts off/on':>17}  outcome off | on")
for tag in sorted({k[0] for k in bc} | {k[0] for k in bt}):
    def rate(b, o):
        x = b.get((tag, o))
        return 100 * (1 - x['dup'] / x['cand']) if x and x['cand'] else float('nan')
    def share(e, i):
        x = e.get(tag)
        return 100 * x[i] / x[0] if x and x[0] else float('nan')
    print(f"{tag:16}{rate(bc,'fresh'):>8.1f}%/{rate(bt,'fresh'):<7.1f}%{rate(bc,'mutant'):>8.1f}%/{rate(bt,'mutant'):<7.1f}%"
          f"{share(ec,1):>7.1f}%/{share(et,1):<6.1f}%{share(ec,2):>7.1f}%/{share(et,2):<6.1f}%  "
          f"{' '.join(oc.get(tag, []))} | {' '.join(ot.get(tag, []))}")
