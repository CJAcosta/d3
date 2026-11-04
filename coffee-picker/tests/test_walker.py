import os
import sys

import mujoco
import numpy as np
import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from coffeesim.walker import LEGS, Walker, leg_ik, stability_margin  # noqa: E402


def test_leg_ik_matches_mujoco():
    w = Walker(layout="deep")
    m, d = w.model, w.data
    rng = np.random.default_rng(0)
    for n in LEGS:
        side = -1 if n[0] == "L" else 1
        row = int(n[1]) - 1
        for _ in range(10):
            target = np.array([side * w.d.feet_x, w.d.feet_y[row], -0.3]) + rng.uniform(-0.08, 0.08, 3)
            q, ok = leg_ik(w.d, n, target)
            assert ok
            d.qpos[:] = 0
            d.qpos[3] = 1
            d.qpos[w.qadr[n]] = q
            mujoco.mj_kinematics(m, d)
            foot = d.xmat[w.root].reshape(3, 3).T @ (d.site_xpos[w.tip[n]] - d.xpos[w.root])
            assert np.linalg.norm(foot - target) < 1e-5


def test_stability_margin():
    square = [(-1, -1), (1, -1), (1, 1), (-1, 1)]
    assert stability_margin(np.array([0.0, 0.0]), square) == pytest.approx(1.0)
    assert stability_margin(np.array([0.5, 0.0]), square) == pytest.approx(0.5)
    assert stability_margin(np.array([1.5, 0.0]), square) == pytest.approx(-0.5)


def test_walks_on_flat_ground():
    r = Walker(layout="deep", gait="tripod", posture="level", grade=0.0, mu=0.8).run(distance=0.3)
    assert r.outcome == "walked"
    assert r.distance > 0.27
    assert r.downhill_drift < 0.02
    assert r.margin_p5 > 0.1


def test_tilted_classic_crab_tips_where_statics_says():
    """Feet 0.12 m downhill of the centre of mass, body 0.30 m up: a body that
    tilts with the ground tips at 0.12 / 0.30 = 40% grade. Level, it walks."""
    assert Walker(layout="classic", gait="wave", posture="tilted", grade=0.45, mu=1.2).run(
        distance=0.3).outcome == "tipped"
    assert Walker(layout="classic", gait="wave", posture="level", grade=0.45, mu=1.2).run(
        distance=0.3).outcome == "walked"


def test_slides_when_grade_beats_grip():
    r = Walker(layout="deep", gait="tripod", posture="lean", grade=0.6, mu=0.4).run(distance=0.3)
    assert r.outcome in ("slid", "tipped")
    assert r.downhill_drift > 0.1
