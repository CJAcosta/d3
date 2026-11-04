"""Film the two-arm single-fruit picker: results/single_pick.mp4 plus stills.

    MUJOCO_GL=osmesa python scripts/pick_demo.py          # headless (apt: libosmesa6, ffmpeg)
    MUJOCO_GL=osmesa python scripts/pick_demo.py --seed 3

Left: the whole robot. Right: close-up of the branch. The strip below shows
what the robot is doing and what has ended up in the basket. Played at 2x.
Writes results/single_pick.json with every attempt.

Two phases, because software rendering is the slow part: the physics runs
once and records the state at every video frame; then the frames are rendered
in parallel and streamed to ffmpeg. Shadows and reflections are off for the
same reason.
"""

import argparse
import json
import multiprocessing as mp
import os
import subprocess
import sys

import mujoco
import numpy as np
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from coffeesim.params import TARGET_STAGES, Branch  # noqa: E402
from coffeesim.picker import Picker  # noqa: E402

RESULTS = os.path.join(os.path.dirname(__file__), "..", "results")
W, H, STRIP = 640, 400, 64
FPS, SPEEDUP = 30, 2
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"


def font(size):
    try:
        return ImageFont.truetype(FONT, size)
    except OSError:
        return ImageFont.load_default(size=size)


class Recorder:
    """on_frame callback: keeps what each video frame needs, renders nothing."""

    def __init__(self):
        self.frames = []

    def __call__(self, picker):
        stages = np.array(picker.info.stages)
        target = np.isin(stages, TARGET_STAGES)
        basket = picker.fruit_in_basket()
        knocked = (~picker.det.attached) & ~target & ~basket
        t = picker.data.time
        line = (f"Basket: {int((basket & target).sum())} ripe/overripe, {int((basket & ~target).sum())} other   "
                f"Green/half-ripe knocked off: {int(knocked.sum())}   "
                f"Suction cap {picker.cfg.suction_cap:g} N   t = {int(t // 60)}:{t % 60:04.1f}   ({SPEEDUP}x speed)")
        self.frames.append((picker.data.qpos.copy(), picker.status, line, t))


_W = {}


def _init(xml):
    m = mujoco.MjModel.from_xml_string(xml)
    _W.update(model=m, data=mujoco.MjData(m), renderer=mujoco.Renderer(m, H, W),
              big=font(20), small=font(16))


def _render(frame):
    qpos, status, line, _ = frame
    m, d, r = _W["model"], _W["data"], _W["renderer"]
    d.qpos[:] = qpos
    mujoco.mj_forward(m, d)
    views = []
    for cam in ("film_wide", "film_close"):
        r.update_scene(d, camera=cam)
        r.scene.flags[mujoco.mjtRndFlag.mjRND_SHADOW] = 0
        r.scene.flags[mujoco.mjtRndFlag.mjRND_REFLECTION] = 0
        views.append(r.render())
    img = Image.new("RGB", (2 * W, H + STRIP), (252, 252, 251))
    img.paste(Image.fromarray(np.hstack(views)), (0, 0))
    draw = ImageDraw.Draw(img)
    draw.text((14, H + 8), status, fill=(11, 11, 11), font=_W["big"])
    draw.text((14, H + 36), line, fill=(82, 81, 78), font=_W["small"])
    return img.tobytes()


def key_moments(frames):
    """Frame indices for the stills, from the first successful pick where the
    claw held the branch: claw closing, cup sealed, fruit off, next look (fruit
    in the basket)."""
    status = [f[1] for f in frames]

    def late(i, word):
        """Last frame of the phase starting at i, so the motion is mostly done."""
        while i + 1 < len(status) and word in status[i + 1]:
            i += 1
        return i

    for off, s in enumerate(status):
        if "came off" not in s or (off and "came off" in status[off - 1]):
            continue
        begin = max((i for i in range(off) if "Looking" in status[i]), default=0)
        claw = next((i for i in range(begin, off) if "claw closes" in status[i]), None)
        if claw is None:
            continue
        seal = next(i for i in range(claw, off) if "sealed" in status[i])
        drop = next((i for i in range(off, len(status)) if "Looking" in status[i]), len(status) - 1)
        return [late(claw, "claw closes"), late(seal, "sealed"),
                min(off + int(0.8 * FPS / SPEEDUP), len(status) - 1), drop]
    return [0, len(frames) // 2, len(frames) - 1]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--procs", type=int, default=os.cpu_count())
    ap.add_argument("--nodes", type=int, default=8, help="branch length in nodes (2 for a quick test)")
    ap.add_argument("--out", default="single_pick", help="file name stem in results/")
    a = ap.parse_args()
    os.makedirs(RESULTS, exist_ok=True)

    rec = Recorder()
    picker = Picker(branch=Branch(n_nodes=a.nodes, leaves=True), seed=a.seed, on_frame=rec,
                    frame_dt=SPEEDUP / FPS)
    summary = picker.run()
    print(f"physics done: {len(rec.frames)} frames", flush=True)

    stills = set(key_moments(rec.frames))
    kept = {}
    ff = subprocess.Popen(
        ["ffmpeg", "-loglevel", "error", "-y", "-f", "rawvideo", "-pix_fmt", "rgb24",
         "-s", f"{2 * W}x{H + STRIP}", "-r", str(FPS), "-i", "-",
         "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "26", "-preset", "medium",
         "-movflags", "+faststart", os.path.join(RESULTS, f"{a.out}.mp4")],
        stdin=subprocess.PIPE)
    # Spawned workers: each opens its own GL context.
    with mp.get_context("spawn").Pool(a.procs, initializer=_init, initargs=(picker.info.xml,)) as pool:
        for i, frame in enumerate(pool.imap(_render, rec.frames, chunksize=16)):
            ff.stdin.write(frame)
            if i in stills:
                kept[i] = frame
            if i % 500 == 0:
                print(f"rendered {i}/{len(rec.frames)}", flush=True)
    ff.stdin.close()
    ff.wait()

    size = (2 * W, H + STRIP)
    shots = [Image.frombytes("RGB", size, kept[i]) for i in sorted(kept)]
    sheet = Image.new("RGB", (size[0], size[1] * len(shots)), (252, 252, 251))
    for k, img in enumerate(shots):
        sheet.paste(img, (0, k * size[1]))
    sheet.resize((size[0] // 2, size[1] * len(shots) // 2), Image.LANCZOS).save(
        os.path.join(RESULTS, f"{a.out}_frames.png"))

    log = [dict(fruit=r.fruit, stage=r.stage, pull_off_N=round(r.threshold, 2), sealed=r.sealed,
                sealed_stage=picker.info.stages[r.sealed] if r.sealed >= 0 else None,
                outcome=r.outcome, branch_held=r.held, seal_peak_N=round(r.seal_peak, 2),
                t_start=round(r.t_start, 2), seconds=round(r.t_end - r.t_start, 2),
                collateral=[picker.info.stages[c] for c in r.collateral]) for r in picker.records]
    with open(os.path.join(RESULTS, f"{a.out}.json"), "w") as f:
        json.dump({"seed": a.seed, "summary": summary, "picks": log}, f, indent=1)
    print(json.dumps(summary, indent=1))


if __name__ == "__main__":
    main()
