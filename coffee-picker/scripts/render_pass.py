"""Render three moments of one comb pass to results/comb_pass.png.

    MUJOCO_GL=osmesa python scripts/render_pass.py   # headless (apt: libosmesa6)
    python scripts/render_pass.py                     # with a display
"""

import os
import sys

import mujoco
import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from coffeesim.params import Comb  # noqa: E402
from coffeesim.trial import SETTLE, run_trial  # noqa: E402

OUT = os.path.join(os.path.dirname(__file__), "..", "results", "comb_pass.png")
W, H = 640, 400


def main():
    comb = Comb(fruit_force=4.5, n_tines=24, speed=0.3)
    sweep = 0.42 / comb.speed  # path length of the default 8-node branch
    shots = [SETTLE + 0.05, SETTLE + 0.45 * sweep, SETTLE + sweep + 0.2]
    frames, state = [], {}

    def on_step(model, data):
        if "r" not in state:
            state["r"] = mujoco.Renderer(model, H, W)
            cam = mujoco.MjvCamera()
            cam.lookat[:] = (0.22, 0.0, 0.96)
            cam.distance, cam.azimuth, cam.elevation = 0.42, -118, -18
            state["cam"] = cam
        if shots and data.time >= shots[0]:
            shots.pop(0)
            state["r"].update_scene(data, state["cam"])
            frames.append(state["r"].render().copy())

    run_trial(comb, seed=1, on_step=on_step)
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    fig, axes = plt.subplots(1, 3, figsize=(15, 3.5), dpi=110, facecolor="#fcfcfb")
    titles = ["Before: ripe (red), overripe (dark), half-ripe (amber), green",
              "Mid-pass: tines fold over fruit that holds on", "After: most green fruit stays; a few ripe hold on"]
    for ax, img, t in zip(axes, frames, titles):
        ax.imshow(img)
        ax.set_title(t, fontsize=10, color="#0b0b0b", loc="left")
        ax.axis("off")
    fig.tight_layout()
    fig.savefig(OUT, facecolor="#fcfcfb")
    print("wrote", OUT)


if __name__ == "__main__":
    main()
