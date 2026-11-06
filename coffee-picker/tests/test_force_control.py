import os
import sys

import numpy as np
import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from coffeesim.walker import Walker, share_force  # noqa: E402

SQUARE = [(-1, -1, 0), (1, -1, 0), (1, 1, 0), (-1, 1, 0)]


def test_share_force_balances_and_pushes_along_the_total():
    F = np.array([0.0, 150.0, 260.0])
    M = np.cross([0.3, 0.2, 0.0], F)  # centre of pressure off to one side
    forces = share_force(F, M, SQUARE, np.zeros(3))
    total = sum(forces)
    moment = sum(np.cross(r, f) for r, f in zip(np.array(SQUARE, float), forces))
    assert np.allclose(total, F)
    # The part of M across F is made; only the twist about F itself is not.
    across = M - F * np.dot(M, F) / np.dot(F, F)
    assert np.allclose(moment - F * np.dot(moment, F) / np.dot(F, F), across, atol=1e-6)
    for f in forces:  # every foot pushes along F: equal use of grip
        assert np.allclose(np.cross(f, F), 0, atol=1e-6)


def test_share_force_eases_a_foot_off():
    even = share_force(np.array([0, 0, 300.0]), np.zeros(3), SQUARE, np.zeros(3))
    eased = share_force(np.array([0, 0, 300.0]), np.zeros(3), SQUARE, np.zeros(3), weights=[1, 1, 1, 0.1])
    assert even[3][2] == pytest.approx(75.0)
    assert eased[3][2] < 30.0
    assert sum(f[2] for f in eased) == pytest.approx(300.0)


def test_force_control_walks_where_stiff_servos_slide():
    """70% grade on grip 0.8: stiff servos creep 10 cm or more downhill in a
    metre; feet that share the load evenly walk it."""
    stiff = Walker(layout="deep", gait="tripod", posture="lean", grade=0.7, mu=0.8).run()
    shared = Walker(layout="deep", gait="tripod", posture="lean", grade=0.7, mu=0.8, control="force").run()
    assert stiff.downhill_drift >= 0.1
    assert shared.outcome == "walked"
    assert shared.downhill_drift < stiff.downhill_drift / 2
