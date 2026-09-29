#!/usr/bin/env python3
"""Summarizes a run_prune_generation_ab.sh output directory (FINDINGS.md section 33).

Per (suite, problem, arm), over matched seeds:
  success   -- runs with a GENUINE SUCCESS: first found below the run's max_level.
               A SUCCESS first found AT max_level is vacuous (the invariance
               lookahead is capped at max_level, so it checks nothing there --
               FINDINGS.md section 37) and is counted separately as "vac".
  n_reached -- mean of each run's max n_reached
  confirmed -- mean of each run's max confirmed_levels
  wall      -- mean wall seconds (times.tsv), plus how many pairs the on-arm was faster in
Then phase 0: pooled [prune-debug] hit rates per rule, off arm only (the unpruned grammar).

Usage: scripts/summarize_prune_ab.py data/prune_generation_ab_YYYYMMDD
"""
import collections
import csv
import glob
import json
import os
import re
import sys


def run_stats(root, suite, problem, seed, arm):
    tag = f"{suite}_{problem}_seed{seed}_{arm}"
    path = os.path.join(root, f"solutions_{tag}.json")
    try:
        records = json.load(open(path))
    except (OSError, ValueError):
        return None  # killed by timeout before writing, or still running
    succ = [r for r in records if r.get("final_status") == "SUCCESS"]
    max_level = int(records[0]["config"]["max_level"]) if records else 0
    success = any(r.get("first_level", 0) < max_level for r in succ)
    vacuous = bool(succ) and not success
    n_reached = max((r.get("n_reached", 0) for r in records), default=0)
    confirmed = max((r.get("confirmed_levels", 0) for r in records), default=0)
    return success, n_reached, confirmed, vacuous


def main():
    root = sys.argv[1]
    times = {}
    with open(os.path.join(root, "times.tsv")) as f:
        for row in csv.DictReader(f, delimiter="\t"):
            times[(row["suite"], row["problem"], row["seed"], row["arm"])] = (float(row["wall_s"]), int(row["exit"]))

    groups = collections.OrderedDict()
    for (suite, problem, seed, arm) in times:
        groups.setdefault((suite, problem), set()).add(seed)

    print(f"{'suite':5} {'problem':20} {'success off/on':>15} {'n_reached off/on':>18} {'wall_s off/on':>17} {'on faster':>9} {'timeouts off/on':>15} {'confirmed off/on':>17} {'vac off/on':>11}")
    for (suite, problem), seeds in groups.items():
        agg = {arm: {"succ": 0, "vac": 0, "n": [], "conf": [], "wall": [], "to": 0, "runs": 0} for arm in ("off", "on")}
        faster = pairs = 0
        for seed in sorted(seeds):
            walls = {}
            for arm in ("off", "on"):
                if (suite, problem, seed, arm) not in times:
                    continue
                wall, rc = times[(suite, problem, seed, arm)]
                a = agg[arm]
                a["runs"] += 1
                a["wall"].append(wall)
                walls[arm] = wall
                if rc == 124:
                    a["to"] += 1
                st = run_stats(root, suite, problem, seed, arm)
                if st:
                    a["succ"] += st[0]
                    a["vac"] += st[3]
                    a["n"].append(st[1])
                    a["conf"].append(st[2])
            if len(walls) == 2:
                pairs += 1
                faster += walls["on"] < walls["off"]
        mean = lambda xs: sum(xs) / len(xs) if xs else float("nan")
        off, on = agg["off"], agg["on"]
        print(f"{suite:5} {problem:20} {off['succ']:>6}/{off['runs']} {on['succ']:>3}/{on['runs']:<3} "
              f"{mean(off['n']):>8.1f}/{mean(on['n']):<8.1f} {mean(off['wall']):>8.1f}/{mean(on['wall']):<8.1f} "
              f"{faster:>4}/{pairs:<4} {off['to']:>8}/{on['to']:<6} {mean(off['conf']):>8.1f}/{mean(on['conf']):<8.1f} {off['vac']:>5}/{on['vac']}")

    print("\nPhase 0 -- [prune-debug] pooled over the OFF arm (unpruned grammar):")
    for suite in sorted({s for s, _ in groups}):
        hits = collections.Counter()
        of = collections.Counter()
        for path in glob.glob(os.path.join(root, f"err_{suite}_*_off.txt")):
            for m in re.finditer(r"\[prune-debug\] (\w+) hits=(\d+) of=(\d+)", open(path).read()):
                hits[m.group(1)] += int(m.group(2))
                of[m.group(1)] += int(m.group(3))
        print(f"  suite {suite}:")
        for rule in hits:
            rate = hits[rule] / of[rule] if of[rule] else 0.0
            print(f"    {rule:24} {hits[rule]:>12} / {of[rule]:<12} = {100 * rate:6.2f}%")
        exhausted = 0
        for path in glob.glob(os.path.join(root, f"err_{suite}_*_on.txt")):
            m = re.search(r"redraws_exhausted=(\d+)", open(path).read())
            exhausted += int(m.group(1)) if m else 0
        print(f"    (on arm: redraws_exhausted = {exhausted})")


if __name__ == "__main__":
    main()
