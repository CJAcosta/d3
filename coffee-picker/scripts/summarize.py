"""Turn results/sweep.csv into results/summary.md and results/tradeoff.png.

    python scripts/summarize.py
"""

import csv
import os
import sys
from collections import defaultdict

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from coffeesim import selectivity, throughput  # noqa: E402
from coffeesim.params import STAGES, TARGET_STAGES, Branch  # noqa: E402

HERE = os.path.dirname(__file__)
RESULTS = os.path.join(HERE, "..", "results")
KEYS = ("fruit_force", "n_tines", "speed", "cv")

# Field benchmark: selective vibration at 55 Hz, 10.1 mm (MDPI Agriculture
# 15(10):1036, 2025): 89.50% of ripe and 15.86% of green fruit detached.
VIBRATION = {"target_yield": 0.8950, "green_loss": 0.1586}

# Reference palette (dataviz skill), light surface.
INK, INK2, MUTED = "#0b0b0b", "#52514e", "#898781"
GRID, AXIS, SURFACE = "#e1e0d9", "#c3c2b7", "#fcfcfb"
BLUE, ORANGE = "#2a78d6", "#eb6834"
BLUE_RAMP = {0.15: "#86b6ef", 0.30: "#2a78d6", 0.45: "#104281"}


def load(path):
    with open(path) as f:
        rows = list(csv.DictReader(f))
    for r in rows:
        for k, v in r.items():
            r[k] = float(v)
    return rows


def pool(rows):
    """Sum fruit counts over seeds for each setting; rates from the pooled counts."""
    acc = defaultdict(lambda: defaultdict(float))
    for r in rows:
        a = acc[tuple(r[k] for k in KEYS)]
        a["sweep_time"] = r["sweep_time"]
        a["runs"] += 1
        for s in STAGES:
            a[f"n_{s}"] += r[f"n_{s}"]
            a[f"off_{s}"] += r[f"off_{s}"]
    out = {}
    for key, a in acc.items():
        n = lambda ss: sum(a[f"n_{s}"] for s in ss)
        off = lambda ss: sum(a[f"off_{s}"] for s in ss)
        out[key] = dict(target_yield=off(TARGET_STAGES) / n(TARGET_STAGES),
                        green_loss=off(["green"]) / n(["green"]),
                        semi_taken=off(["semi"]) / n(["semi"]),
                        purity=off(TARGET_STAGES) / max(1, off(STAGES)),
                        sweep_time=a["sweep_time"],
                        target_per_branch=n(TARGET_STAGES) / a["runs"])
    return out


def pct(x):
    return f"{100 * x:.0f}%"


def best(pooled, cv, max_green=0.10):
    """Highest-yield setting at this CV that keeps green loss under the cap."""
    ok = [(v["target_yield"], k) for k, v in pooled.items() if k[3] == cv and v["green_loss"] <= max_green]
    return max(ok)[1] if ok else None


def table(pooled, cv):
    forces = sorted({k[0] for k in pooled})
    combos = sorted({(k[1], k[2]) for k in pooled})
    head = "| Rating per fruit | " + " | ".join(f"{int(n)} tines, {s:g} m/s" for n, s in combos) + " | Perfect cap |"
    lines = [head, "|---" * (len(combos) + 2) + "|"]
    mix = Branch().stage_mix
    for f in forces:
        cells = []
        for n, s in combos:
            v = pooled[(f, n, s, cv)]
            cells.append(f"{pct(v['target_yield'])} / {pct(v['green_loss'])} / {pct(v['purity'])}")
        ide = selectivity.ideal(f, mix, cv)
        cells.append(f"{pct(ide['target_yield'])} / {pct(ide['green_loss'])} / {pct(ide['purity'])}")
        lines.append(f"| {f:g} N | " + " | ".join(cells) + " |")
    return "\n".join(lines)


def chart(pooled, config, path):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    plt.rcParams.update({"font.family": "sans-serif", "font.size": 10, "text.color": INK,
                         "axes.labelcolor": INK2, "xtick.color": MUTED, "ytick.color": MUTED})
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.6), dpi=150, facecolor=SURFACE)
    n_tines, speed = config
    forces = sorted({k[0] for k in pooled})
    mix = Branch().stage_mix

    def style(ax, title):
        ax.set_facecolor(SURFACE)
        ax.set_title(title, loc="left", fontsize=11, color=INK, pad=10)
        ax.set_xlabel("Green fruit knocked off")
        ax.set_ylabel("Ripe + overripe fruit taken")
        ax.set_xlim(0, 100)
        ax.set_ylim(0, 102)
        ax.grid(True, color=GRID, linewidth=0.6)
        ax.set_axisbelow(True)
        for side in ("top", "right"):
            ax.spines[side].set_visible(False)
        for side in ("left", "bottom"):
            ax.spines[side].set_color(AXIS)
        ax.xaxis.set_major_formatter(matplotlib.ticker.PercentFormatter())
        ax.yaxis.set_major_formatter(matplotlib.ticker.PercentFormatter())

    # Left: simulated comb vs a perfect cap, with the vibration field result.
    ax = axes[0]
    style(ax, "The simulated comb against a perfect force cap (CV 0.30)")
    fs = np.linspace(1.2, 12, 200)
    ide = [selectivity.ideal(f, mix, 0.30) for f in fs]
    ax.plot([100 * d["green_loss"] for d in ide], [100 * d["target_yield"] for d in ide],
            color=MUTED, linewidth=2, linestyle=(0, (4, 3)), label="Perfect cap (every fruit feels exactly F)")
    sim = [pooled[(f, n_tines, speed, 0.30)] for f in forces]
    xs, ys = [100 * v["green_loss"] for v in sim], [100 * v["target_yield"] for v in sim]
    ax.plot(xs, ys, color=BLUE, linewidth=2, marker="o", markersize=6,
            markeredgecolor=SURFACE, markeredgewidth=1.5, label=f"Simulated comb ({int(n_tines)} tines, {speed:g} m/s)")
    for f, x, y in zip(forces, xs, ys):
        if f in (forces[0], forces[len(forces) // 2], forces[-1]):
            off = (4, 7) if f == forces[len(forces) // 2] else (7, -12)
            ax.annotate(f"{f:g} N", (x, y), textcoords="offset points", xytext=off, fontsize=9, color=INK2)
    ax.plot(100 * VIBRATION["green_loss"], 100 * VIBRATION["target_yield"], linestyle="none", marker="D",
            markersize=7, color=ORANGE, markeredgecolor=SURFACE, markeredgewidth=1.5,
            label="Selective vibration, field (Brazil 2025)")
    ax.annotate("vibration, field", (100 * VIBRATION["green_loss"], 100 * VIBRATION["target_yield"]),
                textcoords="offset points", xytext=(8, -16), fontsize=9, color=INK2)
    ax.legend(loc="lower right", frameon=False, fontsize=8.5, labelcolor=INK2)

    # Right: same comb, three assumptions about fruit-to-fruit spread.
    ax = axes[1]
    style(ax, "Same comb, three guesses at fruit-to-fruit spread")
    labels = []
    for cv, color in BLUE_RAMP.items():
        sim = [pooled[(f, n_tines, speed, cv)] for f in forces]
        xs, ys = [100 * v["green_loss"] for v in sim], [100 * v["target_yield"] for v in sim]
        ax.plot(xs, ys, color=color, linewidth=2, marker="o", markersize=6,
                markeredgecolor=SURFACE, markeredgewidth=1.5, label=f"Pull-off force CV {cv:.2f}")
        labels.append((xs[0], ys[0], f"CV {cv:.2f}"))
    # Label each curve beside its gentlest-rating point, where the curves are
    # furthest apart; nudge labels so they keep at least 7 points between them.
    labels.sort(key=lambda t: t[1])
    placed = []
    for x, y, text in labels:
        y_text = max([y - 2] + [p + 7 for p in placed])
        placed.append(y_text)
        ax.annotate(text, (x + 2, y_text), fontsize=9, color=INK2, va="center")
    ax.legend(loc="lower right", frameon=False, fontsize=8.5, labelcolor=INK2)

    fig.tight_layout(w_pad=3)
    fig.savefig(path, facecolor=SURFACE)


def main():
    rows = load(os.path.join(RESULTS, "sweep.csv"))
    pooled = pool(rows)
    cvs = sorted({k[3] for k in pooled})
    seeds = len({r["seed"] for r in rows})
    dropped = sum(r["dropped_in_settle"] for r in rows)

    picks = {cv: best(pooled, cv) for cv in cvs}
    ref = picks.get(0.30) or max(pooled, key=lambda k: pooled[k]["target_yield"])
    config = (ref[1], ref[2])
    chart(pooled, config, os.path.join(RESULTS, "tradeoff.png"))

    md = ["# Comb sweep results", "",
          f"{len(rows)} runs: {len(pooled)} settings x {seeds} branches (seeds). "
          f"Each branch has 8 nodes and ~73 fruit. Fruit dropped before the comb arrived: {int(dropped)}.",
          "", "Cells read **ripe+overripe taken / green knocked off / ripe+overripe share of what was taken**.",
          "Rates pool all fruit across seeds. \"Perfect cap\" is the analytic best case (`coffeesim/selectivity.py`).", ""]
    for cv in cvs:
        md += [f"## Pull-off force spread CV {cv:.2f}", "", table(pooled, cv), ""]
    md += ["## Best setting with green loss at or under 10%", "",
           "| CV | Setting | Taken | Green lost | Semi taken | Purity |", "|---|---|---|---|---|---|"]
    for cv in cvs:
        k = picks[cv]
        if k is None:
            md.append(f"| {cv:.2f} | none qualifies | | | | |")
            continue
        v = pooled[k]
        md.append(f"| {cv:.2f} | {k[0]:g} N, {int(k[1])} tines, {k[2]:g} m/s | {pct(v['target_yield'])} | "
                  f"{pct(v['green_loss'])} | {pct(v['semi_taken'])} | {pct(v['purity'])} |")

    v = pooled[ref]
    rate = throughput.CombRate(target_per_branch=v["target_per_branch"], target_yield=v["target_yield"],
                               speed=ref[2])
    human = throughput.human_fruit_per_s()
    single = throughput.single_pick_fruit_per_s()
    md += ["", "## Throughput at the CV 0.30 pick", "",
           "| Picker | Fruit/s | kg cherry/h | Harvest purity |", "|---|---|---|---|",
           f"| Person, selective (Colombia) | {human:.2f} | {throughput.kg_per_h(human):.1f} | high |",
           f"| One single-fruit arm (7.53 s/fruit) | {single:.2f} | {throughput.kg_per_h(single):.1f} | high |",
           f"| One comb arm (this sweep, 4 s reposition, 70% duty) | {rate.fruit_per_s:.2f} | "
           f"{throughput.kg_per_h(rate.fruit_per_s):.1f} | {pct(v['purity'])} |",
           f"| Brudden DSC-18 shaker (Puerto Rico field) | | {throughput.PR_SHAKER_KG_PER_H:.1f} | "
           f"{pct(1 - throughput.PR_SHAKER_UNRIPE_SHARE)} |", ""]
    with open(os.path.join(RESULTS, "summary.md"), "w") as f:
        f.write("\n".join(md))
    print("\n".join(md))


if __name__ == "__main__":
    main()
