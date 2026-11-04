"""Breakable peduncles: release a cherry once the pull on its peduncle passes its threshold."""

import mujoco
import numpy as np


class Detacher:
    """Watches each cherry's peduncle springs and cuts them at the threshold.

    Peduncle force is stiffness x stretch of the three slide springs; the
    spring itself low-passes contact spikes, as real tissue does.
    """

    def __init__(self, model, data, info, thresholds, evacuate=False, spin_damping=False):
        """`evacuate` drops released fruit out of collision at once, standing
        in for an end effector that channels picked fruit away (catch tube,
        suction). Without it, loose fruit stays in play and can be plowed
        into fruit still on the branch.

        `spin_damping` keeps a little rotational damping on loose fruit. Fruit
        left in play (rolling in a basket, say) otherwise spins up numerically
        on its tiny rotational inertia."""
        n = len(info.stages)
        if len(thresholds) != n:
            raise ValueError("one threshold per cherry")
        self.model, self.data = model, data
        self.evacuate = evacuate
        self.spin_damping = spin_damping
        self.geom = np.array([model.body_geomadr[model.body(f"cherry{k}").id] for k in range(n)])
        self.thresholds = np.asarray(thresholds, dtype=float)
        self.stiffness = np.asarray(info.stiffness, dtype=float)
        jid = lambda name: mujoco.mj_name2id(model, mujoco.mjtObj.mjOBJ_JOINT, name)
        self.slide_jnt = np.array([[jid(f"c{k}_{a}") for a in "xyz"] for k in range(n)])
        self.bend_jnt = np.array([jid(f"c{k}_bend") for k in range(n)])
        self.slide_qadr = model.jnt_qposadr[self.slide_jnt]
        self.attached = np.ones(n, dtype=bool)
        self.peak = np.zeros(n)
        self.detached_at = np.full(n, np.nan)

    def forces(self):
        """Current peduncle pull [N] per cherry (zero once detached)."""
        stretch = np.linalg.norm(self.data.qpos[self.slide_qadr], axis=1)
        return np.where(self.attached, self.stiffness * stretch, 0.0)

    def release(self, idx):
        m = self.model
        for k in np.atleast_1d(idx):
            joints = np.append(self.slide_jnt[k], self.bend_jnt[k])
            m.jnt_stiffness[joints] = 0.0
            for j in (self.slide_jnt[k] if self.spin_damping else joints):
                adr, n = m.jnt_dofadr[j], (3 if m.jnt_type[j] == mujoco.mjtJoint.mjJNT_BALL else 1)
                m.dof_damping[adr:adr + n] = 0.0
            if self.evacuate:
                m.geom_contype[self.geom[k]] = 0
                m.geom_conaffinity[self.geom[k]] = 0
            self.attached[k] = False
            self.detached_at[k] = self.data.time

    def update(self):
        """Call after each mj_step; returns indices released this step."""
        f = self.forces()
        self.peak = np.maximum(self.peak, f)
        broke = np.flatnonzero(self.attached & (f > self.thresholds))
        if broke.size:
            self.release(broke)
        return broke
