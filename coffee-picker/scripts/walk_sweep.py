"""Walk the crab along the contour across layouts, postures, gaits, grades and
grip; write results/walk_sweep.csv.

    python scripts/walk_sweep.py            # ~250 runs, ~6 min on 4 cores
    python scripts/walk_sweep.py --quick    # a handful, for a smoke test
"""

import argparse
import csv
import itertools
import os
import sys
import time
from multiprocessing import Pool

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from coffeesim.walker import GAITS, LAYOUTS, POSTURES, Walker  # noqa: E402

GRADES = [0.0, 0.2, 0.4, 0.6, 0.8, 1.0]
# Foot grip (Coulomb mu). ASSUMPTION: slick wet soil, firm soil, spiked feet
# that bite. Field protocol item 4 measures the real ones.
MUS = [0.5, 0.8, 1.2]
BASKET_KG = 6.0  # a full basket, about 4,000 cherries, hung on the downhill side


def jobs(quick=False):
    if quick:
        return [("deep", "lean", "tripod", 0.6, 0.8, 0.0), ("classic", "tilted", "wave", 0.4, 0.8, 0.0)]
    out = [(lay, post, gait, g, mu, 0.0) for lay, post, gait, g, mu in
           itertools.product(LAYOUTS, POSTURES, GAITS, GRADES, MUS)]
    out += [(lay, "lean", gait, g, mu, BASKET_KG) for lay, gait, g, mu in
            itertools.product(LAYOUTS, GAITS, GRADES, MUS)]
    return out


def one(args):
    layout, posture, gait, grade, mu, payload = args
    r = Walker(layout=layout, gait=gait, posture=posture, grade=grade, mu=mu, payload=payload).run()
    return dict(layout=layout, posture=posture, gait=gait, grade=grade, mu=mu, payload=payload,
                outcome=r.outcome, distance=r.distance, downhill_drift=r.downhill_drift,
                max_tilt_deg=r.max_tilt_deg, margin_p5=r.margin_p5, overreach_frac=r.overreach_frac,
                torque_coxa=r.peak_torque.get("coxa", 0), torque_femur=r.peak_torque.get("femur", 0),
                torque_tibia=r.peak_torque.get("tibia", 0), friction_use=r.friction_use)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--quick", action="store_true")
    ap.add_argument("--out", default=os.path.join(os.path.dirname(__file__), "..", "results", "walk_sweep.csv"))
    ap.add_argument("--procs", type=int, default=os.cpu_count())
    a = ap.parse_args()
    todo = jobs(a.quick)
    t0 = time.time()
    rows = []
    with Pool(a.procs) as pool:
        for i, row in enumerate(pool.imap_unordered(one, todo), 1):
            rows.append(row)
            if i % 25 == 0 or i == len(todo):
                print(f"{i}/{len(todo)} runs, {time.time() - t0:.0f}s", flush=True)
    rows.sort(key=lambda r: (r["payload"], r["layout"], r["posture"], r["gait"], r["mu"], r["grade"]))
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(a.out, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0]))
        w.writeheader()
        w.writerows(rows)
    print(f"wrote {len(rows)} rows to {a.out}")


if __name__ == "__main__":
    main()
