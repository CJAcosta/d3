"""Turn results/walk_sweep.csv and results/pr_slopes.json into
results/walk_summary.md and results/walk_coverage.png.

    python scripts/walk_summary.py
"""

import csv
import json
import os
from collections import defaultdict

RESULTS = os.path.join(os.path.dirname(__file__), "..", "results")
LABEL = {("classic", "tilted"): "Classic crab, body tilts with ground",
         ("classic", "level"): "Classic crab, body level",
         ("classic", "lean"): "Classic crab, body leans half",
         ("deep", "tilted"): "Deep crab, body tilts with ground",
         ("deep", "level"): "Deep crab, body level",
         ("deep", "lean"): "Deep crab, body leans half"}
ORDER = [("classic", "tilted"), ("deep", "tilted"), ("classic", "level"), ("deep", "level"),
         ("classic", "lean"), ("deep", "lean")]

# Reference palette (dataviz skill), light surface.
INK, INK2, MUTED = "#0b0b0b", "#52514e", "#898781"
GRID, AXIS, SURFACE = "#e1e0d9", "#c3c2b7", "#fcfcfb"
SERIES = {0.5: "#2a78d6", 0.8: "#eb6834", 1.2: "#1baf7a"}


def load():
    with open(os.path.join(RESULTS, "walk_sweep.csv")) as f:
        rows = list(csv.DictReader(f))
    for r in rows:
        for k in ("grade", "mu", "payload", "distance", "downhill_drift", "max_tilt_deg", "margin_p5",
                  "overreach_frac", "torque_coxa", "torque_femur", "torque_tibia"):
            r[k] = float(r[k])
    with open(os.path.join(RESULTS, "pr_slopes.json")) as f:
        slopes = json.load(f)
    return rows, slopes


def coverage(slopes, grade):
    """Share of the coffee belt at or below `grade` (fraction), interpolated."""
    pts = [(0.0, 0.0)] + [(int(k) / 100, v) for k, v in slopes["share_at_or_below"].items()]
    pts.sort()
    for (g0, c0), (g1, c1) in zip(pts, pts[1:]):
        if g0 <= grade <= g1:
            return c0 + (c1 - c0) * (grade - g0) / (g1 - g0)
    return pts[-1][1]


def max_grade(rows):
    """Steepest grade walked, counting only an unbroken run of successes from flat."""
    best, why = 0.0, ""
    for r in sorted(rows, key=lambda r: r["grade"]):
        if r["outcome"] != "walked":
            why = r["outcome"]
            break
        best = r["grade"]
    return best, why


def main():
    rows, slopes = load()
    groups = defaultdict(list)
    for r in rows:
        groups[(r["layout"], r["posture"], r["gait"], r["mu"], r["payload"])].append(r)
    best = {}  # (layout, posture, mu, payload) -> (grade, gait, why, row at that grade)
    for (lay, post, gait, mu, pay), rs in groups.items():
        g, why = max_grade(rs)
        at = next((r for r in rs if r["grade"] == g), None)
        key = (lay, post, mu, pay)
        if key not in best or g > best[key][0]:
            best[key] = (g, gait, why, at)
    mus = sorted({r["mu"] for r in rows})

    md = ["# Crab walking along the contour", "",
          f"{len(rows)} runs. Each walks 1 m sideways along a slope at 4 cm/s. \"Walked\" means it "
          "covered the metre, slid less than 10 cm downhill and tilted less than 25 degrees.", "",
          f"Coffee belt: {slopes['area_km2']} km2 between {slopes['elevation_band_m'][0]:.0f} and "
          f"{slopes['elevation_band_m'][1]:.0f} m, median grade {slopes['percentiles']['p50']:.0f}% "
          "(`results/pr_slopes.json`).", "",
          "## Steepest grade walked, and share of the coffee belt that is no steeper", "",
          "Best of tripod and wave gait, on a 20%-step grid. In brackets: what stopped it at the next "
          "grade up (a slide often ends in a tip, and is then logged as one).", "",
          "| Robot | " + " | ".join(f"grip {mu:g}" for mu in mus) + " |",
          "|---" * (len(mus) + 1) + "|"]
    for lay, post in ORDER:
        cells = []
        for mu in mus:
            g, gait, why, _ = best[(lay, post, mu, 0.0)]
            reach = (f"{g:.0%} → {coverage(slopes, g):.0%} of belt" if g > 0 else
                     f"under 20% → under {coverage(slopes, 0.2):.0%} of belt")
            cells.append(reach + (f" ({why} at the next step)" if why else ""))
        md.append(f"| {LABEL[(lay, post)]} | " + " | ".join(cells) + " |")
    md.append("| Ideal: slides only when grade > grip | " +
              " | ".join(f"{mu:.0%} → {coverage(slopes, mu):.0%} of belt" for mu in mus) + " |")

    md += ["", "## With a full 6 kg basket on the downhill side (body leaning)", "",
           "| Robot | " + " | ".join(f"grip {mu:g}" for mu in mus) + " |", "|---" * (len(mus) + 1) + "|"]
    for lay in ("classic", "deep"):
        cells = []
        for mu in mus:
            g, gait, why, _ = best[(lay, "lean", mu, 6.0)]
            cells.append((f"{g:.0%}" if g > 0 else "under 20%") + (f" ({why} at the next step)" if why else ""))
        md.append(f"| {LABEL[(lay, 'lean')]} | " + " | ".join(cells) + " |")

    md += ["", "## Margin and joint torque at the steepest grade walked (grip 1.2, no basket)", "",
           "Margin: how far the centre of mass stays inside the feet (5th percentile over the walk). "
           "Torques are peaks across all legs.", "",
           "| Robot | Grade | Gait | Margin | Coxa | Femur | Tibia |", "|---|---|---|---|---|---|---|"]
    for lay, post in ORDER:
        g, gait, _, at = best[(lay, post, 1.2, 0.0)]
        if at is None:
            md.append(f"| {LABEL[(lay, post)]} | none | | | | | |")
            continue
        md.append(f"| {LABEL[(lay, post)]} | {g:.0%} | {gait} | {at['margin_p5'] * 100:.0f} cm | "
                  f"{at['torque_coxa']:.0f} N m | {at['torque_femur']:.0f} N m | {at['torque_tibia']:.0f} N m |")
    with open(os.path.join(RESULTS, "walk_summary.md"), "w") as f:
        f.write("\n".join(md) + "\n")
    print("\n".join(md))
    chart(best, slopes, mus)


def chart(best, slopes, mus):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    plt.rcParams.update({"font.family": "sans-serif", "font.size": 10, "text.color": INK,
                         "axes.labelcolor": INK2, "xtick.color": MUTED, "ytick.color": MUTED})
    names = [LABEL[k] for k in ORDER] + ["Ideal: slides only when grade > grip"]
    fig, ax = plt.subplots(figsize=(10, 5.2), dpi=150, facecolor=SURFACE)
    ax.set_facecolor(SURFACE)
    h = 0.26
    for i, mu in enumerate(mus):
        vals = [100 * coverage(slopes, best[(lay, post, mu, 0.0)][0]) for lay, post in ORDER]
        vals.append(100 * coverage(slopes, mu))
        ys = [len(names) - 1 - k + (1 - i) * h for k in range(len(names))]
        ax.barh(ys, vals, height=h - 0.03, color=SERIES[mu], label=f"Foot grip {mu:g}")
        under = 100 * coverage(slopes, 0.2)
        for y, v in zip(ys, vals):
            if v > 0:
                ax.text(v + 1, y, f"{v:.0f}%", va="center", fontsize=8.5, color=INK2)
            elif i == 1:  # one label for a row with no bars
                ax.text(1, y, f"under {under:.0f}% at every grip", va="center", fontsize=8.5, color=INK2)
    ax.set_yticks(range(len(names)))
    ax.set_yticklabels(reversed(names), color=INK2)
    ax.set_xlim(0, 105)
    ax.set_xlabel("Share of Puerto Rico's coffee belt the crab can walk (no steeper than its limit)")
    ax.xaxis.set_major_formatter(matplotlib.ticker.PercentFormatter())
    ax.grid(True, axis="x", color=GRID, linewidth=0.6)
    ax.set_axisbelow(True)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(AXIS)
    ax.set_title("Walking sideways along the contour: where each crab design gives out",
                 loc="left", fontsize=11, color=INK, pad=10)
    ax.legend(loc="upper right", frameon=False, fontsize=8.5, labelcolor=INK2)
    fig.tight_layout()
    fig.savefig(os.path.join(RESULTS, "walk_coverage.png"), facecolor=SURFACE)


if __name__ == "__main__":
    main()
