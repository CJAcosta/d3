import os
import sys

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from coffeesim.field import FieldConfig, FieldCrab  # noqa: E402
from coffeesim.params import Branch  # noqa: E402
from coffeesim.picker import Detection, _unit  # noqa: E402

SMALL = Branch(n_nodes=2, rings_per_node=1, sites_per_ring=5, site_occupancy=1.0, leaves=False)


def test_stands_and_walks_on_the_median_slope():
    crab = FieldCrab(field=FieldConfig(start_x=0.0), branch=SMALL, stages=["green"] * 10)
    crab.phase = "settle"
    crab.hold(1.0)
    crab.phase = "walk"
    crab.walk_to(0.3)
    walk = crab.monitor["walk"]
    assert abs(crab.data.xpos[crab.root][0] - 0.3) < 0.03
    assert walk["drift"] < 0.05
    assert walk["tilt"] < 3.0
    assert np.percentile(walk["margin"], 5) > 0.05


def test_picks_while_standing_on_its_legs():
    """A ripe fruit comes off at its own pull-off force, and the arms' pull
    barely moves a body standing on a 50% slope."""
    crab = FieldCrab(field=FieldConfig(start_x=0.15), branch=SMALL, stages=["ripe"] * 10)
    crab.hold(1.0)
    d = crab.data
    y = d.xpos[crab.fruit_body][:, 1].copy()
    y[crab.info.node_of != 1] = np.inf
    k = int(np.argmin(y))  # the outer node's fruit that faces the robot most squarely
    crab.thresholds[k] = crab.det.thresholds[k] = 3.0
    eye = d.cam_xpos[crab.model.camera("eye_high").id]
    centre = d.xpos[crab.fruit_body[k]]
    axis = _unit(centre - eye)
    body0 = d.xpos[crab.root].copy()
    rec = crab.pick(Detection(surface=centre - 0.0065 * axis, axis=axis, pixels=100, camera="eye_high", truth=k), 1)
    assert rec.outcome == "picked"
    assert abs(crab.det.peak[k] - 3.0) < 0.05
    assert np.linalg.norm(d.xpos[crab.root] - body0) < 0.01
