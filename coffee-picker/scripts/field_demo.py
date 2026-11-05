"""Film the whole robot on a 50% slope: walk the row, pick a plant, walk on.

    MUJOCO_GL=osmesa python scripts/field_demo.py            # ~30 min headless
    MUJOCO_GL=osmesa python scripts/field_demo.py --grade 0.6

Writes results/field_pick.mp4 (2x speed), results/field_pick_frames.png and
results/field_pick.json (summary, stability per phase, every attempt).
Left: a camera following the robot from downhill. Right: close-up of the plant.
"""

import argparse
import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.dirname(__file__))

from coffeesim.field import FieldConfig, FieldCrab  # noqa: E402
from pick_demo import FPS, RESULTS, SPEEDUP, Recorder, attempt_log, key_moments, write_film  # noqa: E402

FOLLOW = {"track": "crab", "offset": (0.0, 0.25, 0.35), "distance": 2.6, "azimuth": -40, "elevation": -18}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--grade", type=float, default=0.5)
    ap.add_argument("--mu", type=float, default=1.2)
    ap.add_argument("--procs", type=int, default=os.cpu_count())
    ap.add_argument("--out", default="field_pick")
    a = ap.parse_args()
    os.makedirs(RESULTS, exist_ok=True)
    rec = Recorder()
    crab = FieldCrab(field=FieldConfig(grade=a.grade, mu=a.mu), seed=a.seed, on_frame=rec,
                     frame_dt=SPEEDUP / FPS)
    summary = crab.episode()
    print(f"physics done: {len(rec.frames)} frames", flush=True)
    status = [f[1] for f in rec.frames]
    walk_in = next(i for i, s in enumerate(status) if s.startswith("Walking along"))
    walk_out = len(status) - 1
    stills = [walk_in + (len([s for s in status if s.startswith("Walking along")]) // 2)]
    stills += key_moments(rec.frames)[1:3] + [walk_out]
    write_film(crab.info.xml, rec.frames, a.out, views=(FOLLOW, "film_close"), stills=stills, procs=a.procs)
    with open(os.path.join(RESULTS, f"{a.out}.json"), "w") as f:
        json.dump({"seed": a.seed, "summary": summary, "picks": attempt_log(crab)}, f, indent=1)
    print(json.dumps(summary, indent=1))


if __name__ == "__main__":
    main()
