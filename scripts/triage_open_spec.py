#!/usr/bin/env python3
"""Section 120 triage for open-specification runs (any_peak, peak_element, any_duplicate).

Re-executes every archived program with the Python port in triage_sorting.py, keeps those that satisfy the
problem's property on many random inputs (duplicates, negatives, n up to 256), and groups them by behavior class =
the vector of output[0] on a fixed input set (so different valid answers give different classes). Also reports
the fitted step exponent on random inputs of n = 32..256.

Usage: scripts/triage_open_spec.py RUN_DIR PROBLEM [--num-vars 8]   (RUN_DIR holds seed_*.json)
"""
import glob, hashlib, json, math, os, random, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import triage_sorting as T

def is_peak(a, i):
    n = len(a)
    return 0 <= i < n and (i == 0 or a[i] >= a[i - 1]) and (i == n - 1 or a[i] >= a[i + 1])

def check(problem, inp, out):
    if len(out) != len(inp): return False
    if problem in ('any_peak', 'peak_element'):
        if problem == 'peak_element':
            exp = next((i for i in range(len(inp)) if is_peak(inp, i)), 0)
            return out[0] == exp
        return is_peak(inp, out[0])
    dup = [v for v in set(inp) if inp.count(v) >= 2]
    return (out[0] in dup) if dup else out[0] == -1

def main():
    run_dir, problem = sys.argv[1], sys.argv[2]
    nv = int(sys.argv[sys.argv.index('--num-vars') + 1]) if '--num-vars' in sys.argv else 8
    rng = random.Random(7)
    tests = []
    for _ in range(300):
        n = rng.choice([1, 2, 3, 4, 5, 8, 13, 32, 100, 256])
        hi = rng.choice([2, n, 3 * n])
        tests.append([rng.randint(-hi // 2, hi) for _ in range(n)])
    fixed = [[rng.randint(0, 6) for _ in range(rng.randint(2, 12))] for _ in range(40)]
    big = {n: [rng.randint(0, 3 * n) for _ in range(n)] for n in (32, 64, 128, 256)}
    rows, seeds_solved = [], set()
    for f in sorted(glob.glob(os.path.join(run_dir, 'seed_*.json'))):
        seed = int(os.path.basename(f)[5:-5])
        seen = set()
        for r in json.load(open(f)):
            src = r['program']
            if src in seen: continue
            seen.add(src)
            try:
                prog = T.Parser(T.tokenize(src)).program()
            except Exception:
                continue
            m = T.Machine(prog, nv, False, 5_000_000, None)
            try:
                if not all(check(problem, a, m.run(a)) for a in tests): continue
                sig = tuple(m.run(a)[0] for a in fixed)
                steps = []
                for n in (32, 64, 128, 256):
                    m.run(big[n]); steps.append(m.steps)
            except T.Abort:
                continue
            xs = [math.log(n) for n in (32, 64, 128, 256)]; ys = [math.log(max(s, 1)) for s in steps]
            xb, yb = sum(xs) / 4, sum(ys) / 4
            slope = sum((x - xb) * (y - yb) for x, y in zip(xs, ys)) / sum((x - xb) ** 2 for x in xs)
            seeds_solved.add(seed)
            rows.append({'seed': seed, 'size': r['program_size'], 'exp': round(slope, 2),
                         'class': hashlib.sha1(repr(sig).encode()).hexdigest()[:8], 'program': src})
    classes = {}
    for r in rows:
        classes.setdefault(r['class'], []).append(r)
    print('%s: verified in %d seeds, %d programs, %d behavior classes, min exponent %s' % (
        problem, len(seeds_solved), len(rows), len(classes), min((r['exp'] for r in rows), default=None)))
    for c, rs in sorted(classes.items(), key=lambda kv: -len(kv[1])):
        best = min(rs, key=lambda r: r['size'])
        print('  class %s: %d programs in %d seeds, smallest %d, exponent %s' % (
            c, len(rs), len({r['seed'] for r in rs}), best['size'], best['exp']))
        print('    ' + best['program'].replace('\n', ' '))
    json.dump(rows, open(os.path.join(run_dir, 'triage_open_spec.json'), 'w'), indent=1)

main()
