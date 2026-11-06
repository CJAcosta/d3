"""How close does each leg controller get to the grip limit? Writes
results/grip_sweep.csv, results/grip_summary.md and results/grip_control.png.

    python scripts/grip_sweep.py            # ~125 runs, ~7 min on 4 cores; resumable
    python scripts/grip_sweep.py --summary  # only rebuild the summary and chart

The recommended crab (deep footprint, body leaning half the slope) walks 1 m
along the contour at 5%-grade steps near each grip's limit, with stiff joint
servos ("position") and with force-shared feet ("force"). Results append to
the CSV as they finish, so an interrupted sweep picks up where it stopped.
"""

import argparse
import csv
import json
import os
import sys
import time
from multiprocessing import Pool

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from coffeesim.walker import Walker  # noqa: E402

RESULTS = os.path.join(os.path.dirname(__file__), "..", "results")
CSV = os.path.join(RESULTS, "grip_sweep.csv")
FIELDS = ["control", "gait", "mu", "grade", "outcome", "distance", "downhill_drift", "max_tilt_deg",
          "margin_p5", "torque_coxa", "torque_femur", "torque_tibia"]
# Grades from well inside each grip's limit to just past it, in 5% steps.
GRADES = {0.5: [round(0.25 + 0.05 * i, 2) for i in range(7)],
          0.8: [round(0.40 + 0.05 * i, 2) for i in range(10)],
          1.2: [round(0.60 + 0.05 * i, 2) for i in range(14)]}

INK, INK2, MUTED = "#0b0b0b", "#52514e", "#898781"
GRID, AXIS, SURFACE = "#e1e0d9", "#c3c2b7", "#fcfcfb"
SERIES = {"position": "#2a78d6", "force": "#eb6834", "ideal": "#898781"}


def jobs():
    return [(ctl, gait, mu, g) for ctl in ("position", "force") for gait in ("tripod", "wave")
            for mu, grades in GRADES.items() for g in grades]


def one(job):
    ctl, gait, mu, grade = job
    r = Walker(layout="deep", gait=gait, posture="lean", grade=grade, mu=mu, control=ctl).run()
    return dict(control=ctl, gait=gait, mu=mu, grade=grade, outcome=r.outcome, distance=r.distance,
                downhill_drift=r.downhill_drift, max_tilt_deg=r.max_tilt_deg, margin_p5=r.margin_p5,
                torque_coxa=r.peak_torque["coxa"], torque_femur=r.peak_torque["femur"],
                torque_tibia=r.peak_torque["tibia"])


def load():
    if not os.path.exists(CSV):
        return []
    with open(CSV) as f:
        return list(csv.DictReader(f))


def sweep(procs):
    done = {(r["control"], r["gait"], float(r["mu"]), float(r["grade"])) for r in load()}
    todo = [j for j in jobs() if j not in done]
    print(f"{len(done)} runs already done, {len(todo)} to go", flush=True)
    os.makedirs(RESULTS, exist_ok=True)
    new = not os.path.exists(CSV)
    t0 = time.time()
    with open(CSV, "a", newline="") as f, Pool(procs) as pool:
        w = csv.DictWriter(f, fieldnames=FIELDS)
        if new:
            w.writeheader()
        for i, row in enumerate(pool.imap_unordered(one, todo), 1):
            w.writerow(row)
            f.flush()
            if i % 10 == 0 or i == len(todo):
                print(f"{i}/{len(todo)} runs, {time.time() - t0:.0f}s", flush=True)


def limit(rows):
    """Steepest grade walked, counting only an unbroken run from the gentlest."""
    best = None
    for r in sorted(rows, key=lambda r: float(r["grade"])):
        if r["outcome"] != "walked":
            break
        best = float(r["grade"])
    return best


def coverage(slopes, grade):
    pts = sorted([(0.0, 0.0)] + [(int(k) / 100, v) for k, v in slopes["share_at_or_below"].items()])
    for (g0, c0), (g1, c1) in zip(pts, pts[1:]):
        if g0 <= grade <= g1:
            return c0 + (c1 - c0) * (grade - g0) / (g1 - g0)
    return pts[-1][1]


def summarize():
    rows = load()
    with open(os.path.join(RESULTS, "pr_slopes.json")) as f:
        slopes = json.load(f)
    best = {}
    for ctl in ("position", "force"):
        for mu in GRADES:
            options = []
            for gait in ("tripod", "wave"):
                rs = [r for r in rows if r["control"] == ctl and r["gait"] == gait and float(r["mu"]) == mu]
                g = limit(rs)
                if g is not None:
                    at = next(r for r in rs if float(r["grade"]) == g)
                    options.append((g, gait, at))
            best[(ctl, mu)] = max(options, key=lambda o: o[0]) if options else (None, None, None)
    md = ["# Leg control against the grip limit", "",
          "Deep footprint, body leaning half the slope, 1 m along the contour at 4 cm/s, 5%-grade steps. "
          "\"Walked\": covered the metre, slid under 10 cm, tilted under 25 degrees. Best of tripod and "
          "wave gait. Ideal: slides only once the grade passes the grip.", "",
          "| Foot grip | Stiff joint servos | Force-shared feet | Ideal |", "|---|---|---|---|"]
    for mu in GRADES:
        cells = []
        for ctl in ("position", "force"):
            g, gait, _ = best[(ctl, mu)]
            cells.append(f"{g:.0%} ({g / mu:.0%} of grip) → {coverage(slopes, g):.0%} of belt, {gait}"
                         if g is not None else "none")
        md.append(f"| {mu:g} | " + " | ".join(cells) + f" | {mu:.0%} → {coverage(slopes, mu):.0%} of belt |")
    md += ["", "## At the steepest grade each walked", "",
           "| Grip | Control | Grade | Downhill drift | Margin | Peak joint torque (coxa / femur / tibia) |",
           "|---|---|---|---|---|---|"]
    for mu in GRADES:
        for ctl in ("position", "force"):
            g, gait, at = best[(ctl, mu)]
            if at is None:
                continue
            md.append(f"| {mu:g} | {ctl} | {g:.0%} | {100 * float(at['downhill_drift']):.0f} cm | "
                      f"{100 * float(at['margin_p5']):.0f} cm | {float(at['torque_coxa']):.0f} / "
                      f"{float(at['torque_femur']):.0f} / {float(at['torque_tibia']):.0f} N m |")
    with open(os.path.join(RESULTS, "grip_summary.md"), "w") as f:
        f.write("\n".join(md) + "\n")
    print("\n".join(md))
    chart(best, slopes)


def chart(best, slopes):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    plt.rcParams.update({"font.family": "sans-serif", "font.size": 10, "text.color": INK,
                         "axes.labelcolor": INK2, "xtick.color": MUTED, "ytick.color": MUTED})
    fig, ax = plt.subplots(figsize=(9, 4.6), dpi=150, facecolor=SURFACE)
    ax.set_facecolor(SURFACE)
    mus = list(GRADES)
    w = 0.26
    labels = {"position": "Stiff joint servos", "force": "Force-shared feet", "ideal": "Ideal (slides when grade > grip)"}
    for i, key in enumerate(("position", "force", "ideal")):
        vals = []
        for mu in mus:
            g = mu if key == "ideal" else best[(key, mu)][0]
            vals.append(100 * coverage(slopes, g) if g is not None else 0.0)
        xs = [k + (i - 1) * w for k in range(len(mus))]
        ax.bar(xs, vals, width=w - 0.03, color=SERIES[key], label=labels[key])
        for x, v in zip(xs, vals):
            ax.text(x, v + 1.5, f"{v:.0f}%", ha="center", fontsize=8.5, color=INK2)
    ax.set_xticks(range(len(mus)))
    ax.set_xticklabels([f"Foot grip {mu:g}" for mu in mus], color=INK2)
    ax.set_ylim(0, 108)
    ax.yaxis.set_major_formatter(matplotlib.ticker.PercentFormatter())
    ax.set_ylabel("Share of the coffee belt it can walk")
    ax.grid(True, axis="y", color=GRID, linewidth=0.6)
    ax.set_axisbelow(True)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(AXIS)
    ax.set_title("Same crab, two leg controllers: how much of the slope each can use",
                 loc="left", fontsize=11, color=INK, pad=10)
    ax.legend(loc="upper left", frameon=False, fontsize=8.5, labelcolor=INK2)
    fig.tight_layout()
    fig.savefig(os.path.join(RESULTS, "grip_control.png"), facecolor=SURFACE)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--summary", action="store_true")
    ap.add_argument("--procs", type=int, default=os.cpu_count())
    a = ap.parse_args()
    if not a.summary:
        sweep(a.procs)
    summarize()


if __name__ == "__main__":
    main()
