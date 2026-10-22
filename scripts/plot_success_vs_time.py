#!/usr/bin/env python3
"""Success-versus-time curves for the PSB2 integer subset (section 139 logs). Usage: scripts/plot_success_vs_time.py [run_dir] [out_prefix]
A run counts as solved at the time its log says the search finished, if its JSON records 2,000/2,000 unseen cases; unsolved runs are
censored at the budget. Writes <out_prefix>.pdf, .png and _data.csv."""
import csv, glob, re, sys
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

D = sys.argv[1] if len(sys.argv) > 1 else "data/psb2_100_20261022"
OUT = sys.argv[2] if len(sys.argv) > 2 else "paper/figures/success_vs_time"
BUDGET = 900.0
# Fixed hue per problem (reference categorical order), with a line style and marker so the figure also reads in grayscale.
PROBLEMS = [
    ("gcd", "#2a78d6", "-", "o"), ("basement", "#eb6834", "--", "s"), ("find_pair", "#1baf7a", "-.", "^"),
    ("paired_digits", "#eda100", ":", "D"), ("fuel_cost", "#e87ba4", "-", "o"), ("luhn", "#008300", "--", "s"),
    ("coin_sums", "#6250d6", "-.", "^"),
]
INK, GRID = "#0b0b0b", "#d8d7d2"

def times(problem):
    out = []
    for log in sorted(glob.glob(f"{D}/psb2_{problem}/seed_*.log")):
        text = open(log, errors="ignore").read()
        m = re.search(r"Search finished in ([0-9.]+)s", text)
        try:
            solved = '"adversarial_passed": 2000' in open(log[:-4] + ".json").read()
        except OSError:
            solved = False
        out.append(float(m.group(1)) if (m and solved) else None)
    return out

def curve(ts, grid):
    n = len(ts)
    return [sum(1 for t in ts if t is not None and t <= x) / n for x in grid]

data = {p: times(p) for p, *_ in PROBLEMS}
with open(OUT + "_data.csv", "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["problem", "seed_index", "solve_time_s"])
    for p, ts in data.items():
        for i, t in enumerate(ts):
            w.writerow([p, i, "" if t is None else f"{t:.1f}"])

plt.rcParams.update({"font.size": 8, "axes.edgecolor": INK, "text.color": INK, "axes.labelcolor": INK,
                     "xtick.color": INK, "ytick.color": INK, "font.family": "serif"})
fig, (a, b) = plt.subplots(1, 2, figsize=(7.0, 2.7), sharey=True)
fast = [p for p in PROBLEMS if p[0] in ("gcd", "basement", "find_pair", "paired_digits")]
slow = [p for p in PROBLEMS if p[0] in ("fuel_cost", "luhn", "coin_sums")]
import numpy as np
for ax, group, xs, log in ((a, fast, np.concatenate([[0.5], np.geomspace(1, 900, 400)]), True),
                           (b, slow, np.linspace(0, 900, 451), False)):
    for name, color, ls, mk in group:
        ys = curve(data[name], xs)
        lines = ax.plot(xs, ys, color=color, ls=ls, lw=1.5, drawstyle="steps-post")
        solved = sum(t is not None for t in data[name])
        k = max(1, len(xs) // 6)
        ax.plot(xs[::k][1:], np.array(ys)[::k][1:], ls="none", marker=mk, ms=3.5, color=color, mec="#fcfcfb", mew=0.5)
        lines[-1].set_label(f"{name.replace('_', ' ')} ({solved}/100)")
    if log:
        ax.set_xscale("log")
        ax.set_xlim(1, 900)
    else:
        ax.set_xlim(0, 900)
    ax.set_xlabel("time since start of the run (s)" + (", log scale" if log else ""))
    ax.set_ylim(0, 1.03)
    ax.grid(True, color=GRID, lw=0.5)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)
    ax.legend(frameon=False, loc="lower right" if log else "upper left", fontsize=7, handlelength=2.6)
a.set_ylabel("fraction of 100 runs solved")
a.set_title("(a) problems solved in seconds", fontsize=8, loc="left")
b.set_title("(b) problems that use the budget", fontsize=8, loc="left")
fig.tight_layout()
fig.savefig(OUT + ".pdf")
fig.savefig(OUT + ".png", dpi=200)
for name, *_ in PROBLEMS:
    ts = sorted(t for t in data[name] if t is not None)
    print(name, len(ts), "solved; median %.0f s, p90 %.0f s" % (ts[len(ts) // 2], ts[int(0.9 * len(ts)) - 1]) if ts else "none")
