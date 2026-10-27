import os
import sys

import mujoco
import numpy as np
import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from coffeesim import selectivity, throughput  # noqa: E402
from coffeesim.detach import Detacher  # noqa: E402
from coffeesim.params import (DETACH_MEAN_N, Branch, Comb, lognormal_params,  # noqa: E402
                              sample_thresholds)
from coffeesim.scene import build_scene  # noqa: E402
from coffeesim.trial import run_trial  # noqa: E402


def test_pull_test_releases_at_threshold():
    """Ramp a radial pull on each fruit; it must come off at its threshold."""
    b = Branch(n_nodes=1, rings_per_node=1, sites_per_ring=5, site_occupancy=1.0)
    stages = ["green", "semi", "ripe", "overripe", "ripe"]
    thresholds = np.array([8.8, 5.6, 3.3, 1.6, 4.0])
    info = build_scene(branch=b, stages=stages, timestep=5e-4)
    m = mujoco.MjModel.from_xml_string(info.xml)
    d = mujoco.MjData(m)
    det = Detacher(m, d, info, thresholds)
    for _ in range(400):
        mujoco.mj_step(m, d)
        det.update()
    assert det.attached.all()
    for k, thr in enumerate(thresholds):
        bid = m.body(f"cherry{k}").id
        t0 = d.time
        while det.attached[k]:
            assert d.time - t0 < 3, "fruit never came off"
            pull = 5.0 * (d.time - t0)
            d.xfrc_applied[bid, :3] = pull * d.xmat[bid].reshape(3, 3)[:, 2]
            mujoco.mj_step(m, d)
            det.update()
        d.xfrc_applied[bid] = 0
        # Gravity adds up to one fruit weight (0.015 N) either way.
        assert pull == pytest.approx(thr, abs=0.03)


def test_default_branch_starts_unloaded():
    """No fruit may start overlapping a neighbour (it would be preloaded)."""
    info = build_scene(rng=np.random.default_rng(1))
    m = mujoco.MjModel.from_xml_string(info.xml)
    d = mujoco.MjData(m)
    mujoco.mj_forward(m, d)
    assert d.ncon == 0
    det = Detacher(m, d, info, np.full(len(info.stages), np.inf))
    for _ in range(1000):
        mujoco.mj_step(m, d)
        det.update()
    # Sagging under its own fruit should load a peduncle by a few weights at most.
    assert det.peak.max() < 0.1


def test_thresholds_match_mean_and_cv():
    rng = np.random.default_rng(0)
    x = sample_thresholds(["ripe"] * 20000, rng, cv=0.3)
    assert x.mean() == pytest.approx(DETACH_MEAN_N["ripe"], rel=0.02)
    assert x.std() / x.mean() == pytest.approx(0.3, rel=0.05)
    mu, sigma = lognormal_params(2.0, 0.0)
    assert sigma == 0 and np.exp(mu) == pytest.approx(2.0)


def test_ideal_selectivity():
    mix = Branch().stage_mix
    lo, hi = selectivity.ideal(3.0, mix), selectivity.ideal(6.0, mix)
    assert lo["target_yield"] < hi["target_yield"]
    assert lo["green_loss"] < hi["green_loss"]
    # Cap at 5 N, CV 0.3: ~94% of ripe and ~4% of green come off.
    assert selectivity.take_rate("ripe", 5.0, 0.3) == pytest.approx(0.94, abs=0.01)
    assert selectivity.take_rate("green", 5.0, 0.3) == pytest.approx(0.038, abs=0.005)


def test_throughput_baselines():
    # 82.6 kg/day of 1.53 g fruit over 8 h is ~1.9 fruit/s.
    assert throughput.human_fruit_per_s() == pytest.approx(1.87, abs=0.01)
    arms = throughput.human_fruit_per_s() / throughput.single_pick_fruit_per_s()
    assert 13 < arms < 15
    rate = throughput.CombRate(target_per_branch=40, target_yield=0.9)
    assert rate.cycle_s == pytest.approx(0.4 / 0.3 + 4.0)
    # 0.7 duty x 36 fruit per 5.33 s cycle.
    assert rate.fruit_per_s == pytest.approx(4.725)
    assert throughput.kg_per_h(rate.fruit_per_s) == pytest.approx(26.0, abs=0.1)


@pytest.mark.parametrize("fruit_force,expect", [(0.8, "none"), (25.0, "all")])
def test_comb_extremes(fruit_force, expect):
    """A feather-light comb takes no green fruit; a brutal one strips the branch."""
    b = Branch(n_nodes=2)
    r = run_trial(Comb(fruit_force=fruit_force, speed=0.3), seed=5, branch=b)
    assert r.dropped_in_settle == 0
    if expect == "none":
        assert r.removed["green"] == 0 and r.removed["semi"] == 0
    else:
        assert sum(r.removed.values()) >= 0.9 * sum(r.total.values())
