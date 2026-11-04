import os
import sys

import numpy as np
import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from coffeesim.params import Branch  # noqa: E402
from coffeesim.picker import EYES, Detection, Picker, _unit, fruit_mask  # noqa: E402

SMALL = Branch(n_nodes=2, rings_per_node=1, sites_per_ring=5, site_occupancy=1.0, leaves=False)


def test_colour_check():
    px = np.array([[[190, 20, 25], [77, 25, 30], [90, 150, 50], [215, 150, 40], [115, 82, 51], [240, 240, 235]]],
                  dtype=np.uint8)
    # ripe, overripe | green, half-ripe, bark, robot white
    assert fruit_mask(px).tolist() == [[True, True, False, False, False, False]]


def _aimed_at(p, k):
    """A perfect detection of fruit k, seen from the high eye."""
    centre = p.data.xpos[p.fruit_body[k]]
    axis = _unit(centre - np.array(EYES["eye_high"][0]))
    return Detection(surface=centre - 0.0065 * axis, axis=axis, pixels=100, camera="eye_high", truth=k)


def _front_fruit(p):
    """The fruit facing the robot most squarely (lowest y) on the outer node."""
    y = p.data.xpos[p.fruit_body][:, 1].copy()
    y[p.info.node_of != 1] = np.inf
    return int(np.argmin(y))


@pytest.mark.parametrize("pull_off,expect", [(3.0, "picked"), (8.8, "held_on")])
def test_suction_cap_is_a_ripeness_check(pull_off, expect):
    """The cup takes fruit that lets go below 5 N and leaves fruit that holds on."""
    p = Picker(branch=SMALL, stages=["ripe"] * 10, seed=0)
    p.hold(0.5)
    k = _front_fruit(p)
    p.thresholds[k] = p.det.thresholds[k] = pull_off
    rec = p.pick(_aimed_at(p, k), 1)
    assert rec.sealed == k
    assert rec.outcome == expect
    if expect == "picked":
        p.hold(0.5)
        assert p.fruit_in_basket()[k]
    else:
        assert p.det.attached[k]
        assert p.det.peak[k] < 5.5  # the seal broke near 5 N, well before 8.8 N


def test_run_takes_only_red_fruit():
    """Fruit 2, 3 and 7 face the robot (ripe/overripe); 8 faces it too (green)."""
    stages = ["green", "ripe", "overripe", "ripe", "semi", "green", "ripe", "ripe", "green", "semi"]
    p = Picker(branch=SMALL, stages=stages, seed=2)
    s = p.run()
    st = np.array(p.info.stages)
    assert all(st[r.fruit] in ("ripe", "overripe") for r in p.records)
    assert s["front_target_total"] == 3
    assert s["front_target_in_basket"] == 3
    assert s["nontarget_in_basket"] == 0
    assert s["nontarget_knocked_off"] == 0
