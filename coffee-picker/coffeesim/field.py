"""The whole robot in the field: walk the contour, stop at a plant, pick, walk on.

One free-floating crab body carries everything: six legs (`walker.py`: deep
footprint, body leaning half the slope, tripod gait), the two arms and stalk
cameras of `picker.py` (claw holds the branch, suction cup picks), and the
basket. The legs stand while the arms work.

What this adds over the two parts on their own: the arms' weight and pulls
now pass through the legs into a slope, the cameras and arms tilt with the
leaning body, and the plant stands uphill of the robot.
"""

import math
from dataclasses import dataclass

import mujoco
import numpy as np

from .params import Branch
from .picker import (DARK, FILMS, GREEN, WHITE, Picker, PickerConfig, _arm_xml, _f, _xyaxes,
                     arm_actuators_xml, seal_grip_xml)
from .walker import (GAITS, LAYOUTS, LEGS, CrabDesign, Footwork, leg_actuators_xml, leg_ik, legs_xml,
                     stability_margin)

# Collision bit 16 is ground-only, so feet and the carapace meet the slope
# and nothing else (not the basket hung on the body, not the branch).
GROUND_BITS = 'contype="18" conaffinity="0"'
FOOT_BITS = 'contype="0" conaffinity="16"'


@dataclass
class FieldConfig:
    grade: float = 0.5           # the coffee belt's median slope
    mu: float = 1.2              # foot grip; ASSUMPTION: spiked feet that bite
    layout: str = "deep"
    gait: str = "tripod"
    speed: float = 0.06          # walking speed along the contour [m/s]
    stand_y: float = -0.48       # body centre's distance downhill of the plant row
    start_x: float = -0.80
    stop_x: float = 0.20         # in front of the middle of the branch
    end_x: float = 1.20
    branch_height: float = 0.80  # a low fruiting branch, above the trunk's foot
    body_mass: float = 17.0      # carapace, battery, electronics; arms and legs on top


class FieldCrab(Picker):
    def __init__(self, field=None, cfg=None, seed=1, on_frame=None, frame_dt=1 / 30, stages=None, branch=None):
        self.field = f = field or FieldConfig()
        self.theta = math.atan(f.grade)
        self.lean = 0.5 * self.theta
        self.design = CrabDesign(body_mass=f.body_mass)
        for k, v in LAYOUTS[f.layout].items():
            setattr(self.design, k, v)
        self.segments = []   # planned walks: (t_start, x_start, t_end, x_end)
        self.phase = "settle"
        self.monitor = {}
        super().__init__(cfg=cfg or PickerConfig(), branch=branch or Branch(leaves=True), seed=seed,
                         on_frame=on_frame, frame_dt=frame_dt, stages=stages)

    # -- the planned body path ---------------------------------------------
    def ground_z(self, x, y):
        return self.field.grade * y

    def plan_x(self, t):
        x = self.field.start_x
        for t0, x0, t1, x1 in self.segments:
            if t >= t1:
                x = x1
            elif t >= t0:
                x = x0 + (x1 - x0) * (t - t0) / (t1 - t0)
        return x

    def plan(self, t):
        x, y = self.plan_x(t), self.field.stand_y
        p = np.array([x, y, self.ground_z(x, y) + self.design.height])
        c, s = math.cos(self.lean), math.sin(self.lean)
        return p, np.array([[1, 0, 0], [0, c, -s], [0, s, c]]), self.lean

    def neutral(self, name, p, R):
        side = -1 if name[0] == "L" else 1
        row = int(name[1]) - 1
        w = p + np.array([side * self.design.feet_x, self.design.feet_y[row], 0.0])
        w[2] = self.ground_z(w[0], w[1]) + self.design.foot_radius
        return w

    def _b(self, world_rel):
        """Body-frame point for an offset given level, relative to the body
        centre: the picker's tested layout, rotated into the leaning body."""
        _, R, _ = self.plan(0.0)
        return R.T @ np.asarray(world_rel, float)

    # -- model ----------------------------------------------------------------
    def _scene_kwargs(self):
        return {"height": self.field.branch_height}

    def _robot_xml(self, cfg, n_fruit, n_seg):
        f, d = self.field, self.design
        theta = self.theta
        w = [f'<geom name="ground" type="plane" size="4 4 0.1" pos="0.2 0 0" euler="{theta:.5g} 0 0" '
             f'material="soil" friction="{f.mu} 0.01 0.001" {GROUND_BITS}/>',
             '<geom type="cylinder" fromto="0 0 -0.05 0 0 0.6" size="0.02" rgba=".35 .25 .15 1" '
             'contype="0" conaffinity="0"/>']
        # Neighbouring plants along the row and the row above, for scale (visual).
        for x, y in ((-2.3, 0.0), (2.7, 0.0), (-0.8, 1.4), (1.4, 1.4)):
            z = self.ground_z(x, y)
            w.append(f'<geom type="cylinder" fromto="{x} {y} {z - 0.05} {x} {y} {z + 0.9}" size="0.025" '
                     f'rgba=".35 .25 .15 1" contype="0" conaffinity="0"/>')
            w.append(f'<geom type="ellipsoid" pos="{x} {y} {z + 1.15}" size="0.55 0.45 0.6" rgba=".2 .42 .2 1" '
                     f'contype="0" conaffinity="0"/>')
        film_pos, film_target, fovy = FILMS["film_close"]
        dz = f.branch_height - 1.0
        fp, ft = np.array(film_pos) + [0, 0, dz], np.array(film_target) + [0, 0, dz]
        w.append(f'<camera name="film_close" pos="{_f(fp)}" xyaxes="{_xyaxes(fp, ft)}" fovy="{fovy}"/>')
        w.append('<light pos="1.5 -1.5 2.5" dir="-0.5 0.5 -1" diffuse=".5 .5 .5"/>')

        # The crab. Offsets below are the fixed picker's layout relative to its
        # carapace, so the arms, eyes and basket keep their tested geometry.
        b = []
        b.append(f'<geom name="carapace" type="ellipsoid" size="0.27 0.17 0.06" mass="{f.body_mass}" '
                 f'rgba="{WHITE}" {FOOT_BITS}/>')
        b.append(f'<geom type="ellipsoid" pos="0 0 0.035" size="0.15 0.09 0.035" rgba="{GREEN}" '
                 f'contype="0" conaffinity="0" mass="0.01"/>')
        b.append(legs_xml(d, f.mu, FOOT_BITS))
        for prefix, x, tool in (("p", 0.10, "cup"), ("h", -0.10, "claw")):
            arm = _arm_xml(prefix, self._b([x, 0.07, 0.12]), tool)
            # Gravity compensation through the joint motors, not as a free
            # upward force: the arms' weight still loads the legs.
            b.append(arm.replace('<joint name="', '<joint actuatorgravcomp="true" name="'))
        eyes = {"eye_high": ([0.03, -0.13, 0.92], [0.10, 0.47, 0.65]),
                "eye_low": ([-0.22, -0.13, 0.50], [0.10, 0.47, 0.65])}
        for name, (pos, target) in eyes.items():
            pb, tb = self._b(pos), self._b(target)
            base = self._b([pos[0], -0.12, 0.05])
            b.append(f'<geom type="capsule" fromto="{_f(base)} {_f(pb - [0, 0, 0.03])}" size="0.009" '
                     f'rgba="{DARK}" contype="0" conaffinity="0" mass="0.05"/>')
            b.append(f'<geom type="sphere" pos="{_f(pb - [0, 0.03, 0])}" size="0.026" rgba="{WHITE}" '
                     f'contype="0" conaffinity="0" mass="0.05"/>')
            b.append(f'<camera name="{name}" pos="{_f(pb)}" xyaxes="{_xyaxes(pb, tb)}" fovy="{cfg.fovy}"/>')
        # Basket on a tray off the right side. Walls catch fruit only.
        bk = self._b([0.45, 0.07, 0.08])
        b.append(f'<geom type="capsule" fromto="{_f(self._b([0.2, 0.05, 0.03]))} {_f(bk - [0.06, 0, 0.03])}" '
                 f'size="0.012" rgba="{DARK}" contype="0" conaffinity="0" mass="0.1"/>')
        walls = [((0, 0, -0.035), (0.07, 0.07, 0.005)), ((0.07, 0, 0), (0.005, 0.07, 0.035)),
                 ((-0.07, 0, 0), (0.005, 0.07, 0.035)), ((0, 0.07, 0), (0.07, 0.005, 0.035)),
                 ((0, -0.07, 0), (0.07, 0.005, 0.035))]
        for off, size in walls:
            b.append(f'<geom type="box" pos="{_f(bk + off)}" size="{_f(size)}" rgba=".55 .4 .25 1" '
                     f'contype="2" conaffinity="0" mass="0.08"/>')
        b.append(f'<site name="basket" pos="{_f(bk)}" size="0.005" rgba="0 0 0 0"/>')
        p0, _, _ = self.plan(0.0)
        q0 = [math.cos(self.lean / 2), math.sin(self.lean / 2), 0, 0]
        w.append(f'<body name="crab" pos="{_f(p0)}" quat="{_f(q0)}"><freejoint name="crab_root"/>'
                 + "".join(b) + "</body>")
        asset = ('<texture name="soil" type="2d" builtin="flat" rgb1=".38 .28 .18" rgb2=".3 .22 .14" '
                 'mark="random" random="0.15" markrgb=".25 .2 .12" width="256" height="256"/>'
                 '<material name="soil" texture="soil" texrepeat="16 16"/>')
        sections = (f"<actuator>{leg_actuators_xml()}{arm_actuators_xml()}</actuator>"
                    f"<equality>{seal_grip_xml(cfg, n_fruit, n_seg)}</equality>")
        return asset, "".join(w), sections

    # -- legs -----------------------------------------------------------------
    def _initial_state(self):
        m, d = self.model, self.data
        # Same friction model as the walking study: elliptic cones with a high
        # impedance ratio. The branch scene's defaults let a stance foot creep
        # under a steady sideways load, and the robot slid 12-24 cm per metre.
        m.opt.cone = mujoco.mjtCone.mjCONE_ELLIPTIC
        m.opt.impratio = 10.0
        self.root = m.body("crab").id
        self.root_q = m.jnt_qposadr[m.joint("crab_root").id]
        self.ground_geom = m.geom("ground").id
        self.leg_q = {n: [m.jnt_qposadr[m.joint(f"{n}_{j}").id] for j in "cft"] for n in LEGS}
        self.leg_act = {n: [m.actuator(f"{n}_{j}").id for j in "cft"] for n in LEGS}
        jid = {n: [m.joint(f"{n}_{j}").id for j in "cft"] for n in LEGS}
        self.leg_lo = {n: m.jnt_range[jid[n], 0] for n in LEGS}
        self.leg_hi = {n: m.jnt_range[jid[n], 1] for n in LEGS}
        p, R, _ = self.plan(0.0)
        feet = {n: self.neutral(n, p, R) for n in LEGS}
        self.footwork = Footwork(GAITS[self.field.gait], self.neutral, feet)
        for n in LEGS:
            q, _ = leg_ik(self.design, n, R.T @ (feet[n] - p))
            q = np.clip(q, self.leg_lo[n], self.leg_hi[n])
            d.qpos[self.leg_q[n]] = q
            d.ctrl[self.leg_act[n]] = q

    def _pre_step(self):
        t = self.data.time
        p, R, _ = self.plan(t)
        feet = self.footwork.update(t, self.plan)
        for n in LEGS:
            q, _ = leg_ik(self.design, n, R.T @ (feet[n] - p))
            self.data.ctrl[self.leg_act[n]] = np.clip(q, self.leg_lo[n], self.leg_hi[n])

    def step(self):
        super().step()
        d = self.data
        if int(round(d.time / self.model.opt.timestep)) % 20:
            return
        mon = self.monitor.setdefault(self.phase, {"margin": [], "drift": 0.0, "tilt": 0.0, "torque": 0.0,
                                                   "seconds": 0.0})
        mon["seconds"] += 20 * self.model.opt.timestep
        loaded = []
        for c in range(d.ncon):
            con = d.contact[c]
            if self.ground_geom not in (con.geom1, con.geom2):
                continue
            other = con.geom2 if con.geom1 == self.ground_geom else con.geom1
            if other in self.geom_to_fruit:
                continue  # dropped fruit lying on the slope
            f6 = np.zeros(6)
            mujoco.mj_contactForce(self.model, d, c, f6)
            if f6[0] > 1.0:
                loaded.append(con.pos[:2].copy())
        if len(loaded) >= 3:
            mon["margin"].append(stability_margin(d.subtree_com[self.root][:2], loaded))
        p, R, _ = self.plan(d.time)
        mon["drift"] = max(mon["drift"], float(p[1] - d.xpos[self.root][1]))
        cos = float(np.dot(d.xmat[self.root].reshape(3, 3)[:, 2], R[:, 2]))
        mon["tilt"] = max(mon["tilt"], math.degrees(math.acos(max(-1.0, min(1.0, cos)))))
        legs = [a for n in LEGS for a in self.leg_act[n]]
        mon["torque"] = max(mon["torque"], float(np.abs(d.actuator_force[legs]).max()))

    def _solve_poses(self):
        """Arm poses relative to the body, so they hold wherever it walks."""
        d = self.data
        R = d.xmat[self.root].reshape(3, 3)
        to_world = lambda rel: d.xpos[self.root] + R @ self._b(rel)
        self.cup.q_home = self._solve(self.cup, d.site_xpos[self.basket] + [0, 0, 0.12], [0, 0, -1],
                                      [-1.2, -0.6, -1.8, -0.6, 0])
        self.claw.q_home = self._solve(self.claw, to_world([-0.46, 0.15, 0.06]), [0, 0, -1],
                                       [1.4, -0.9, -1.2, -0.6, 0])
        self.cup.q_ready = self._solve(self.cup, to_world([0.10, 0.35, 0.68]), [0, 0.9, -0.44],
                                       [0, -0.4, -1.0, -0.5, 0])
        self.claw.q_ready = self._solve(self.claw, to_world([0.02, 0.39, 0.65]), [0, 1, 0],
                                        [0, -0.4, -1.0, -0.5, 0])

    # -- the day's work ---------------------------------------------------------
    def walk_to(self, x):
        """Walk along the contour to `x`, then stand with every foot down."""
        t0, x0 = self.data.time, self.plan_x(self.data.time)
        t1 = t0 + abs(x - x0) / self.field.speed
        self.segments.append((t0, x0, t1, x))
        self.footwork.start(t0)
        while self.data.time < t1:
            self.step()
        self.footwork.stop()
        while self.footwork.stance != set(LEGS):
            self.step()
        self.hold(0.5)

    def episode(self, max_picks=40):
        self.phase, self.status = "settle", "Settling on the slope"
        self.hold(1.0)
        self.phase, self.status = "walk", "Walking along the row to the plant"
        self.walk_to(self.field.stop_x)
        self.phase = "pick"
        self.pick_all(max_picks)
        self.status = "Done here"
        self.move_joints(self.cup, self.cup.q_home, 1.0)
        self.hold(0.5)
        self.phase, self.status = "walk", "Walking on to the next plant"
        self.walk_to(self.field.end_x)
        self.status = "Done"
        return self.field_summary()

    def field_summary(self):
        out = self.summary()
        out["grade"], out["mu"] = self.field.grade, self.field.mu
        out["robot_mass_kg"] = round(float(self.model.body_subtreemass[self.root]), 1)
        out["phases"] = {
            k: {"seconds": round(v["seconds"], 1),
                "margin_p5_cm": round(100 * float(np.percentile(v["margin"], 5)), 1) if v["margin"] else None,
                "margin_min_cm": round(100 * float(min(v["margin"])), 1) if v["margin"] else None,
                "downhill_drift_cm": round(100 * v["drift"], 1),
                "max_tilt_deg": round(v["tilt"], 1),
                "peak_leg_torque_Nm": round(v["torque"], 1)}
            for k, v in self.monitor.items()}
        out["travel_m"] = round(float(self.data.xpos[self.root][0] - self.field.start_x), 2)
        return out


