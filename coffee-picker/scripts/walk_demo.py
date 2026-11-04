"""Two crabs on the same slope, side by side: results/walk_demo.mp4 and a still.

    MUJOCO_GL=osmesa python scripts/walk_demo.py            # 50% grade
    MUJOCO_GL=osmesa python scripts/walk_demo.py --grade 0.6

Left: the classic crab holding a fixed posture, tilted with the ground.
Right: the deep crab with its body leaning half the slope. Same mass, legs,
gait and foot grip.
"""

import argparse
import os
import subprocess
import sys

import mujoco
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from coffeesim.walker import Walker  # noqa: E402

RESULTS = os.path.join(os.path.dirname(__file__), "..", "results")
W, H, STRIP, FPS = 640, 420, 56, 30
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"


def font(size):
    try:
        return ImageFont.truetype(FONT, size)
    except OSError:
        return ImageFont.load_default(size=size)


def film(walker, distance):
    """Run the walk, recording the body state each frame; return frames and result."""
    frames = []
    result = walker.run(distance=distance, on_frame=lambda w: frames.append(w.data.qpos.copy()),
                        frame_dt=1 / FPS, linger=2.5)
    return frames, result


def render(walker, frames):
    m, d = walker.model, walker.data
    r = mujoco.Renderer(m, H, W)
    cam = mujoco.MjvCamera()
    out = []
    for q in frames:
        d.qpos[:] = q
        mujoco.mj_forward(m, d)
        body = d.xpos[walker.root]
        cam.lookat[:] = [body[0], body[1], body[2] - 0.08]
        cam.distance, cam.azimuth, cam.elevation = 1.9, -35, -14
        r.update_scene(d, camera=cam)
        r.scene.flags[mujoco.mjtRndFlag.mjRND_REFLECTION] = 0
        out.append(r.render().copy())
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--grade", type=float, default=0.5)
    ap.add_argument("--mu", type=float, default=1.2)
    ap.add_argument("--distance", type=float, default=0.6)
    a = ap.parse_args()
    runs = [("Classic crab, body tilted with the ground", Walker("classic", "wave", "tilted", a.grade, a.mu)),
            ("Deep crab, body leaning half the slope", Walker("deep", "wave", "lean", a.grade, a.mu))]
    clips, results = [], []
    for _, w in runs:
        frames, res = film(w, a.distance)
        clips.append(render(w, frames))
        results.append(res)
    n = max(len(c) for c in clips)
    big, small = font(18), font(15)
    path = os.path.join(RESULTS, "walk_demo.mp4")
    ff = subprocess.Popen(["ffmpeg", "-loglevel", "error", "-y", "-f", "rawvideo", "-pix_fmt", "rgb24",
                           "-s", f"{2 * W}x{H + STRIP}", "-r", str(FPS), "-i", "-", "-c:v", "libx264",
                           "-pix_fmt", "yuv420p", "-crf", "26", "-movflags", "+faststart", path],
                          stdin=subprocess.PIPE)
    still_at = min(n - 1, int(0.6 * n))
    for i in range(n):
        img = Image.new("RGB", (2 * W, H + STRIP), (252, 252, 251))
        draw = ImageDraw.Draw(img)
        for k, ((title, _), clip, res) in enumerate(zip(runs, clips, results)):
            # A clip that ended early (the crab fell) holds its last frame.
            img.paste(Image.fromarray(clip[min(i, len(clip) - 1)]), (k * W, 0))
            draw.text((k * W + 12, H + 6), title, fill=(11, 11, 11), font=big)
            verdict = {"walked": "walks", "tipped": "tips over", "slid": "slides downhill",
                       "out_of_reach": "legs out of reach", "fell": "falls"}[res.outcome]
            done = i >= len(clip) - 1
            draw.text((k * W + 12, H + 31),
                      f"{a.grade:.0%} grade, foot grip {a.mu:g}" + (f": {verdict}" if done else ""),
                      fill=(82, 81, 78), font=small)
        ff.stdin.write(img.tobytes())
        if i == still_at:
            img.save(os.path.join(RESULTS, "walk_demo.png"))
    ff.stdin.close()
    ff.wait()
    for (title, _), res in zip(runs, results):
        print(f"{title}: {res.outcome}, {res.distance} m, drift {res.downhill_drift} m")


if __name__ == "__main__":
    main()
