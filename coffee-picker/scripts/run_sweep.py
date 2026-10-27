"""Sweep comb rating, tine count, speed and threshold spread; write results/sweep.csv.

    python scripts/run_sweep.py            # full grid, ~10 min on 4 cores
    python scripts/run_sweep.py --quick    # 1 seed, 2 ratings, for a smoke test
"""

import argparse
import csv
import itertools
import os
import sys
import time
from multiprocessing import Pool

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from coffeesim.params import STAGES, Comb  # noqa: E402
from coffeesim.trial import run_trial  # noqa: E402

FULL = dict(fruit_force=[2.5, 3.5, 4.5, 5.5, 7.0], n_tines=[12, 24], speed=[0.1, 0.3],
            cv=[0.15, 0.30, 0.45], seed=[0, 1, 2, 3])
QUICK = dict(fruit_force=[3.5, 5.5], n_tines=[12], speed=[0.2], cv=[0.30], seed=[0])


def one(args):
    fruit_force, n_tines, speed, cv, seed = args
    r = run_trial(Comb(fruit_force=fruit_force, n_tines=n_tines, speed=speed), cv=cv, seed=seed)
    row = dict(fruit_force=fruit_force, n_tines=n_tines, speed=speed, cv=cv, seed=seed,
               sweep_time=round(r.sweep_time, 3), dropped_in_settle=r.dropped_in_settle,
               target_yield=r.target_yield, green_loss=r.green_loss,
               semi_taken=r.semi_taken, purity=r.purity)
    for s in STAGES:
        row[f"n_{s}"] = r.total[s]
        row[f"off_{s}"] = r.removed[s]
    return row


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--quick", action="store_true")
    ap.add_argument("--out", default=os.path.join(os.path.dirname(__file__), "..", "results", "sweep.csv"))
    ap.add_argument("--procs", type=int, default=os.cpu_count())
    a = ap.parse_args()
    grid = QUICK if a.quick else FULL
    jobs = list(itertools.product(*grid.values()))
    t0 = time.time()
    with Pool(a.procs) as pool:
        rows = []
        for i, row in enumerate(pool.imap_unordered(one, jobs), 1):
            rows.append(row)
            if i % 20 == 0 or i == len(jobs):
                print(f"{i}/{len(jobs)} runs, {time.time() - t0:.0f}s", flush=True)
    rows.sort(key=lambda r: tuple(r[k] for k in grid))
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(a.out, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0]))
        w.writeheader()
        w.writerows(rows)
    print(f"wrote {len(rows)} rows to {a.out}")


if __name__ == "__main__":
    main()
