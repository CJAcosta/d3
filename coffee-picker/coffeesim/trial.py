"""One comb pass along one simulated branch, and the numbers that come out of it."""

from dataclasses import dataclass, field

import mujoco
import numpy as np

from .detach import Detacher
from .params import DETACH_CV, STAGES, TARGET_STAGES, Branch, Comb, sample_thresholds
from .scene import build_scene

SETTLE = 0.6    # s for the branch to sag under its fruit before the pass
TAIL = 0.3      # s after the pass for loosened fruit to finish moving
START_GAP = 0.02  # comb starts this far behind the first node [m]


@dataclass
class TrialResult:
    seed: int
    fruit_force: float
    n_tines: int
    speed: float
    cv: float
    total: dict
    removed: dict
    sweep_time: float
    dropped_in_settle: int = 0
    # Peak peduncle pull on fruit that stayed on, by stage: how close the
    # comb came to taking it.
    retained_peak: dict = field(default_factory=dict)
    # Per fruit, in scene order: stage and peak peduncle pull seen [N].
    stages: list = field(default_factory=list)
    peak: np.ndarray = None

    def frac(self, stages):
        n = sum(self.total.get(s, 0) for s in stages)
        return sum(self.removed.get(s, 0) for s in stages) / n if n else float("nan")

    @property
    def target_yield(self):
        """Share of ripe + overripe fruit taken."""
        return self.frac(TARGET_STAGES)

    @property
    def green_loss(self):
        """Share of green fruit knocked off (lost crop, lower cup quality)."""
        return self.frac(("green",))

    @property
    def semi_taken(self):
        return self.frac(("semi",))

    @property
    def purity(self):
        """Ripe + overripe share of everything taken."""
        n = sum(self.removed.values())
        return sum(self.removed.get(s, 0) for s in TARGET_STAGES) / n if n else float("nan")


def _quat_nlerp(q0, q1, w):
    if np.dot(q0, q1) < 0:
        q1 = -q1
    q = (1 - w) * q0 + w * q1
    return q / np.linalg.norm(q)


class BranchPath:
    """Comb pose at arc length s along the branch's settled centreline.

    Orientation blends between neighbouring segments across each node so the
    tines do not get a kick at segment joints.
    """

    def __init__(self, model, data, n_seg, internode):
        ids = [model.body(f"seg{j}").id for j in range(n_seg)]
        self.pos = data.xpos[ids].copy()
        self.mat = data.xmat[ids].reshape(-1, 3, 3).copy()
        self.quat = data.xquat[ids].copy()
        self.n, self.L = n_seg, internode

    def __call__(self, s):
        j = int(np.clip(np.floor(s / self.L), 0, self.n - 1))
        pos = self.pos[j] + self.mat[j][:, 0] * (s - j * self.L)
        u = s / self.L - 0.5
        i = int(np.clip(np.floor(u), 0, self.n - 2))
        w = float(np.clip(u - i, 0.0, 1.0))
        return pos, _quat_nlerp(self.quat[i], self.quat[i + 1], w)


def _branch_dofs(model, n_seg):
    return np.array([model.jnt_dofadr[model.joint(f"seg{j}_{a}").id]
                     for j in range(n_seg) for a in "yz"])


def run_trial(comb=None, cv=DETACH_CV, seed=0, branch=None, thresholds=None,
              evacuate=True, timestep=5e-4, on_step=None):
    """Settle a branch, sweep the comb base to tip once, report what came off.

    `thresholds` overrides the sampled pull-off forces (e.g. inf to measure
    the force the comb delivers). `evacuate` is passed to the Detacher.
    `on_step(model, data)` is called each step.
    """
    comb = comb or Comb()
    branch = branch or Branch()
    rng = np.random.default_rng(seed)
    info = build_scene(branch=branch, comb=comb, rng=rng, timestep=timestep)
    if thresholds is None:
        thresholds = sample_thresholds(info.stages, rng, cv)
    elif np.isscalar(thresholds):
        thresholds = np.full(len(info.stages), float(thresholds))
    model = mujoco.MjModel.from_xml_string(info.xml)
    data = mujoco.MjData(model)
    det = Detacher(model, data, info, thresholds, evacuate=evacuate)

    def step():
        mujoco.mj_step(model, data)
        det.update()
        if on_step:
            on_step(model, data)

    path_start = -START_GAP + branch.internode - branch.ring_spacing
    for _ in range(int(SETTLE / timestep)):
        data.mocap_pos[0] = [path_start - 0.02, 0, data.mocap_pos[0][2]]
        step()
    dropped = int((~det.attached).sum())
    path = BranchPath(model, data, branch.n_nodes, branch.internode)

    s_end = branch.n_nodes * branch.internode + branch.ring_spacing + START_GAP
    t0 = data.time
    while True:
        s = path_start + comb.speed * (data.time - t0)
        pos, quat = path(min(s, s_end))
        data.mocap_pos[0], data.mocap_quat[0] = pos, quat
        if s >= s_end:
            break
        step()
    sweep_time = data.time - t0
    for _ in range(int(TAIL / timestep)):
        step()

    stages = np.array(info.stages)
    off = ~det.attached
    total = {s: int((stages == s).sum()) for s in STAGES}
    removed = {s: int((off & (stages == s)).sum()) for s in STAGES}
    retained_peak = {s: float(det.peak[(stages == s) & det.attached].mean())
                     for s in STAGES if ((stages == s) & det.attached).any()}
    result = TrialResult(seed=seed, fruit_force=comb.fruit_force, n_tines=comb.n_tines,
                         speed=comb.speed, cv=cv,
                         total=total, removed=removed, sweep_time=sweep_time,
                         dropped_in_settle=dropped, retained_peak=retained_peak,
                         stages=info.stages, peak=det.peak.copy())
    return result
