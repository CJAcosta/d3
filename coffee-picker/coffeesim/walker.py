"""A six-legged crab walking sideways along a slope.

The ground is a plane rising toward +y, where the coffee row is. The crab
faces uphill and walks along x, the contour.

Control is quasi-static. Stance feet stay where they landed; the body
follows a planned path at a steady speed; swing feet lift and step to new
footholds on the slope. Joint targets come from analytic leg IK against the
planned body pose and are tracked by stiff position servos. The physics
decides the rest: whether feet slip, whether the body tips, and how much
torque each joint needs.

Three postures are compared:

- `level`: body horizontal, feet straight below their neutral spots. Downhill
  legs reach far down, uphill legs fold up.
- `lean`: body tilted by half the slope, feet still placed around the centre
  of mass. Splits the reach between uphill and downhill legs.
- `tilted`: a fixed posture relative to the body, tilted with the ground as a
  real crab or a vehicle is. Feet sit along the body's own "down" axis, so
  the centre of mass drifts downhill of the footprint as the slope steepens.
"""

POSTURES = ("level", "lean", "tilted")

import math
from dataclasses import dataclass, field

import mujoco
import numpy as np

LEGS = ("L1", "L2", "L3", "R1", "R2", "R3")  # L at -x, R at +x; 1 front (uphill), 3 back


@dataclass
class CrabDesign:
    # Whole-robot mass carried by the legs: carapace, two arms, battery,
    # empty basket. ASSUMPTION, about a mid-size research hexapod with arms.
    body_mass: float = 22.0
    body_size: tuple = (0.50, 0.34, 0.12)   # full extents [m]
    hip_x: float = 0.25
    hip_y: tuple = (0.12, 0.0, -0.12)
    coxa: float = 0.05
    femur: float = 0.25
    tibia: float = 0.40
    link_mass: tuple = (0.3, 0.6, 0.4)      # coxa, femur, tibia [kg]
    foot_radius: float = 0.02
    # Neutral foot positions (body frame, horizontal): |x| and per-row y.
    feet_x: float = 0.60
    feet_y: tuple = (0.12, 0.0, -0.12)
    height: float = 0.30                    # body centre above the ground below it
    # A full basket hung on the downhill side: the bad case for tipping.
    payload: float = 0.0
    payload_pos: tuple = (0.20, -0.22, 0.0)


# Classic crab: legs out to the sides, narrow front to back. Deep: feet spread
# downhill and uphill too (docs/platform-crab.md).
LAYOUTS = {
    "classic": dict(feet_x=0.60, feet_y=(0.12, 0.0, -0.12)),
    "deep": dict(feet_x=0.45, feet_y=(0.34, 0.0, -0.34)),
}


@dataclass
class Gait:
    name: str
    period: float      # one full cycle [s]
    duty: float        # share of the cycle each foot is down
    offsets: dict      # phase at which each leg lifts off
    lift: float = 0.06


GAITS = {
    # Three feet down at a time: L1, R2, L3 alternate with R1, L2, R3.
    "tripod": Gait("tripod", 2.4, 0.55, {"L1": 0.0, "R2": 0.0, "L3": 0.0, "R1": 0.5, "L2": 0.5, "R3": 0.5}),
    # Five feet down: one leg swings at a time.
    "wave": Gait("wave", 2.4, 0.85, {"L3": 0 / 6, "L2": 1 / 6, "L1": 2 / 6, "R3": 3 / 6, "R2": 4 / 6, "R1": 5 / 6}),
}


@dataclass
class WalkResult:
    layout: str
    gait: str
    posture: str
    grade: float
    mu: float
    payload: float
    outcome: str                 # walked | slid | tipped | out_of_reach | fell
    distance: float              # body travel along the contour [m]
    downhill_drift: float        # body slide downhill against plan [m]
    max_tilt_deg: float          # worst body tilt away from plan
    margin_p5: float             # static stability margin, 5th percentile over the walk [m]
    overreach_frac: float        # share of steps some stance leg could not reach
    peak_torque: dict = field(default_factory=dict)   # coxa/femur/tibia [N m]
    # Worst |tangential|/normal at any loaded foot. Stiff legs push against
    # each other, so this hits mu even on flat ground; drift is the slip measure.
    friction_use: float = 0.0


def _f(v):
    return " ".join(f"{x:.5g}" for x in v)


def crab_xml(d, grade, mu, timestep=1e-3):
    theta = math.atan(grade)
    hx, hy = d.hip_x, d.hip_y
    legs = []
    for name in LEGS:
        side = -1 if name[0] == "L" else 1
        row = int(name[1]) - 1
        hip = np.array([side * hx, hy[row], 0.0])
        foot = np.array([side * d.feet_x, d.feet_y[row], 0.0])
        yaw = math.atan2(foot[1] - hip[1], foot[0] - hip[0])
        legs.append(f"""
   <body name="{name}_base" pos="{_f(hip)}" euler="0 0 {yaw:.6g}">
    <body name="{name}_coxa">
     <joint name="{name}_c" axis="0 0 1" range="-1.3 1.3" armature="0.01" damping="1"/>
     <geom type="capsule" fromto="0 0 0 {d.coxa} 0 0" size="0.025" mass="{d.link_mass[0]}" rgba=".16 .17 .18 1" contype="0" conaffinity="0"/>
     <body name="{name}_femur" pos="{d.coxa} 0 0">
      <joint name="{name}_f" axis="0 1 0" range="-1.7 1.7" armature="0.01" damping="1"/>
      <geom type="capsule" fromto="0 0 0 {d.femur} 0 0" size="0.022" mass="{d.link_mass[1]}" rgba=".92 .92 .9 1" contype="0" conaffinity="0"/>
      <body name="{name}_tibia" pos="{d.femur} 0 0">
       <joint name="{name}_t" axis="0 1 0" range="0 2.9" armature="0.01" damping="1"/>
       <geom type="capsule" fromto="0 0 0 {d.tibia - d.foot_radius} 0 0" size="0.016" mass="{d.link_mass[2]}" rgba=".16 .17 .18 1" contype="0" conaffinity="0"/>
       <geom name="{name}_foot" type="sphere" pos="{d.tibia - d.foot_radius} 0 0" size="{d.foot_radius}" mass="0.05"
             friction="{mu} 0.01 0.001" rgba=".12 .5 .27 1"/>
       <site name="{name}_tip" pos="{d.tibia} 0 0" size="0.005"/>
      </body>
     </body>
    </body>
   </body>""")
    bx, by, bz = (s / 2 for s in d.body_size)
    payload = (f'<geom type="box" pos="{_f(d.payload_pos)}" size="0.07 0.07 0.04" mass="{d.payload}" '
               f'rgba=".55 .4 .25 1" contype="0" conaffinity="0"/>' if d.payload > 0 else "")
    kp = {"c": 800, "f": 2000, "t": 2000}
    act = "".join(f'<position name="{n}_{j}" joint="{n}_{j}" kp="{kp[j]}" dampratio="1"/>'
                  for n in LEGS for j in "cft")
    return f"""
<mujoco model="crab_walker">
  <compiler angle="radian" autolimits="true"/>
  <option timestep="{timestep}" integrator="implicitfast" cone="elliptic" impratio="10"/>
  <default><geom solref="0.004 1" solimp="0.95 0.99 0.001"/></default>
  <visual><global offwidth="1280" offheight="720"/><headlight ambient=".35 .35 .35" diffuse=".65 .65 .65"/></visual>
  <asset>
    <texture type="skybox" builtin="gradient" rgb1=".93 .94 .92" rgb2=".72 .78 .74" width="64" height="64"/>
    <texture name="soil" type="2d" builtin="checker" rgb1=".38 .28 .18" rgb2=".33 .24 .15" width="256" height="256"/>
    <material name="soil" texture="soil" texrepeat="20 20"/>
  </asset>
  <worldbody>
    <light pos="0 -1 3" dir="0 0.3 -1"/>
    <geom name="ground" type="plane" size="6 6 0.1" euler="{theta:.6g} 0 0" material="soil" friction="{mu} 0.01 0.001"/>
    <body name="crab" pos="0 0 1">
     <freejoint/>
     <geom name="carapace" type="ellipsoid" size="{bx} {by} {bz}" mass="{d.body_mass}" rgba=".92 .92 .9 1"/>
     {payload}
     {"".join(legs)}
    </body>
  </worldbody>
  <actuator>{act}</actuator>
</mujoco>"""


def leg_ik(d, name, foot_body):
    """Joint angles (coxa, femur, tibia) placing `name`'s foot at a body-frame
    point. Returns (q, reached): out-of-reach targets are clamped to the
    nearest reachable point along the same line."""
    side = -1 if name[0] == "L" else 1
    row = int(name[1]) - 1
    hip = np.array([side * d.hip_x, d.hip_y[row], 0.0])
    neutral = np.array([side * d.feet_x, d.feet_y[row]])
    yaw = math.atan2(neutral[1] - hip[1], neutral[0] - hip[0])
    v = foot_body - hip
    c, s = math.cos(-yaw), math.sin(-yaw)
    lx, ly, lz = c * v[0] - s * v[1], s * v[0] + c * v[1], v[2]
    q1 = math.atan2(ly, lx)
    u = math.hypot(lx, ly) - d.coxa
    w = -lz
    dist = math.hypot(u, w)
    lo, hi = abs(d.femur - d.tibia) + 1e-3, d.femur + d.tibia - 1e-3
    reached = lo <= dist <= hi
    if not reached:
        k = min(max(dist, lo), hi) / max(dist, 1e-9)
        u, w, dist = u * k, w * k, min(max(dist, lo), hi)
    c3 = (dist ** 2 - d.femur ** 2 - d.tibia ** 2) / (2 * d.femur * d.tibia)
    q3 = math.acos(max(-1.0, min(1.0, c3)))
    q2 = math.atan2(w, u) - math.atan2(d.tibia * math.sin(q3), d.femur + d.tibia * math.cos(q3))
    return np.array([q1, q2, q3]), reached


def _quat_x(angle):
    return np.array([math.cos(angle / 2), math.sin(angle / 2), 0.0, 0.0])


def _rot_x(angle):
    c, s = math.cos(angle), math.sin(angle)
    return np.array([[1, 0, 0], [0, c, -s], [0, s, c]])


def _hull(points):
    pts = sorted(map(tuple, points))
    if len(pts) < 3:
        return pts

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lower, upper = [], []
    for p in pts:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0:
            lower.pop()
        lower.append(p)
    for p in reversed(pts):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0:
            upper.pop()
        upper.append(p)
    return lower[:-1] + upper[:-1]


def stability_margin(com_xy, feet_xy):
    """Signed distance [m] from the centre of mass (projected along gravity)
    to the edge of the support polygon; negative means it would tip."""
    hull = _hull(feet_xy)
    if len(hull) < 3:
        return -1.0
    best = np.inf
    for a, b in zip(hull, hull[1:] + hull[:1]):
        a, b = np.array(a), np.array(b)
        e = b - a
        n = np.array([e[1], -e[0]]) / np.linalg.norm(e)  # outward for a CCW hull
        best = min(best, -np.dot(com_xy - a, n))
    return float(best)


class Walker:
    def __init__(self, layout="deep", gait="wave", posture="level", grade=0.0, mu=0.8, payload=0.0,
                 speed=0.04, design=None):
        if posture not in POSTURES:
            raise ValueError(f"posture must be one of {POSTURES}")
        d = design or CrabDesign()
        for k, v in LAYOUTS[layout].items():
            setattr(d, k, v)
        d.payload = payload
        self.d, self.gait, self.posture, self.grade, self.mu = d, GAITS[gait], posture, grade, mu
        self.layout_name, self.speed = layout, speed
        self.theta = math.atan(grade)
        self.model = mujoco.MjModel.from_xml_string(crab_xml(d, grade, mu))
        self.data = mujoco.MjData(self.model)
        m = self.model
        self.root = m.body("crab").id
        self.foot_geom = {n: m.geom(f"{n}_foot").id for n in LEGS}
        self.tip = {n: m.site(f"{n}_tip").id for n in LEGS}
        self.qadr = {n: [m.jnt_qposadr[m.joint(f"{n}_{j}").id] for j in "cft"] for n in LEGS}
        self.act = {n: [m.actuator(f"{n}_{j}").id for j in "cft"] for n in LEGS}
        jid = {n: [m.joint(f"{n}_{j}").id for j in "cft"] for n in LEGS}
        self.lo = {n: m.jnt_range[jid[n], 0] for n in LEGS}
        self.hi = {n: m.jnt_range[jid[n], 1] for n in LEGS}
        self.ground_geom = m.geom("ground").id

    # -- plan -------------------------------------------------------------
    def ground_z(self, x, y):
        return self.grade * y

    @property
    def normal(self):
        return np.array([0.0, -math.sin(self.theta), math.cos(self.theta)])

    def body_pose(self, t, t_walk):
        """Planned body position and rotation at time t (walking starts at t_walk)."""
        x = self.speed * max(0.0, t - t_walk)
        if self.posture != "tilted":
            p = np.array([x, 0.0, self.ground_z(x, 0.0) + self.d.height])
            tilt = 0.5 * self.theta if self.posture == "lean" else 0.0
            return p, _rot_x(tilt), tilt
        # Body parallel to the ground, `height` off it along the normal.
        p = np.array([x, 0.0, 0.0]) + self.d.height * self.normal
        return p, _rot_x(self.theta), self.theta

    def neutral_world(self, name, p, R):
        side = -1 if name[0] == "L" else 1
        row = int(name[1]) - 1
        local = np.array([side * self.d.feet_x, self.d.feet_y[row], 0.0])
        if self.posture != "tilted":
            # Neutral spot laid out horizontally around the body centre, then
            # straight down onto the ground along gravity.
            w = p + local
            w[2] = self.ground_z(w[0], w[1]) + self.d.foot_radius
            return w
        w = p + R @ local
        # Along the body's own down axis, which meets the ground `height` away.
        return w - (self.d.height - self.d.foot_radius) * self.normal

    # -- run --------------------------------------------------------------
    def set_joints(self, p, R, feet):
        """Command every leg; returns the legs whose targets it could not meet
        (out of reach, or past a joint stop)."""
        missed = set()
        for n in LEGS:
            q, ok = leg_ik(self.d, n, R.T @ (feet[n] - p))
            lo, hi = self.lo[n], self.hi[n]
            clipped = np.clip(q, lo, hi)
            if not ok or np.abs(clipped - q).max() > 1e-3:
                missed.add(n)
            # Never command past a stop: the servo would only fight it.
            self.data.ctrl[self.act[n]] = clipped
        return missed

    def run(self, distance=1.0, settle=1.0, on_frame=None, frame_dt=1 / 30, linger=0.0):
        """Walk `distance` along the contour. `linger` keeps simulating (servos
        frozen) that long after a fall, so a film shows it."""
        m, data, d, g = self.model, self.data, self.d, self.gait
        dt = m.opt.timestep
        p, R, tilt = self.body_pose(0.0, settle)
        feet = {n: self.neutral_world(n, p, R) for n in LEGS}
        data.qpos[0:3] = p
        data.qpos[3:7] = _quat_x(tilt)
        for n in LEGS:
            q, _ = leg_ik(d, n, R.T @ (feet[n] - p))
            data.qpos[self.qadr[n]] = np.clip(q, self.lo[n], self.hi[n])
        self.set_joints(p, R, feet)
        mujoco.mj_forward(m, data)

        T, duty = g.period, g.duty
        t_end = settle + distance / self.speed
        lifted = {n: None for n in LEGS}   # (t0, start, target, t_land) while swinging
        last_cycle = {n: -1 for n in LEGS}  # one swing per leg per cycle
        drift = tilt_err = 0.0
        margins, over, peak, fric = [], 0, {"coxa": 0.0, "femur": 0.0, "tibia": 0.0}, 0.0
        steps = int(round(t_end / dt))
        next_frame = 0.0
        outcome = None
        for i in range(steps):
            t = i * dt
            p, R, tilt = self.body_pose(t, settle)
            if t >= settle:
                phase_t = (t - settle) / T
                for n in LEGS:
                    ph = (phase_t - g.offsets[n]) % 1.0
                    cycle = math.floor(phase_t - g.offsets[n])
                    swinging = ph < (1 - duty) and cycle >= 0 and cycle > last_cycle[n]
                    if swinging and lifted[n] is None:
                        last_cycle[n] = cycle
                        # Land where this foot's neutral spot will be at
                        # mid-stance, so stance straddles neutral.
                        t_land = t + (1 - duty - ph) * T
                        p_land, R_land, _ = self.body_pose(t_land + 0.5 * duty * T, settle)
                        lifted[n] = (t, feet[n].copy(), self.neutral_world(n, p_land, R_land), t_land)
                    if lifted[n] is not None:
                        t0, a, b, t_land = lifted[n]
                        tau = min(1.0, (t - t0) / max(1e-6, t_land - t0))
                        s = 0.5 - 0.5 * math.cos(math.pi * tau)
                        feet_n = a + s * (b - a)
                        feet_n[2] += g.lift * math.sin(math.pi * tau)
                        feet[n] = feet_n
                        if tau >= 1.0:
                            feet[n] = b.copy()
                            lifted[n] = None
            stance = {n for n in LEGS if lifted[n] is None}
            # Only count it when a supporting leg cannot reach its foothold.
            if self.set_joints(p, R, feet) & stance:
                over += 1
            mujoco.mj_step(m, data)
            if on_frame and data.time >= next_frame:
                next_frame += frame_dt
                on_frame(self)
            if not np.all(np.isfinite(data.qpos)):
                outcome = "fell"
                break
            # How far the real body has slid downhill from the plan.
            body = data.xpos[self.root]
            drift = max(drift, float(p[1] - body[1]))
            zb = data.xmat[self.root].reshape(3, 3)[:, 2]
            zp = R[:, 2]
            tilt_err = max(tilt_err, math.degrees(math.acos(max(-1.0, min(1.0, float(np.dot(zb, zp)))))))
            if tilt_err > 25 or np.dot(body - [0, 0, self.ground_z(body[0], body[1])], self.normal) < 0.08:
                outcome = "tipped"
                for _ in range(int(round(linger / dt))):
                    mujoco.mj_step(m, data)
                    if on_frame and data.time >= next_frame:
                        next_frame += frame_dt
                        on_frame(self)
                break
            if i % 20 == 0 and t >= settle:
                loaded = []
                for c in range(data.ncon):
                    con = data.contact[c]
                    pair = {con.geom1, con.geom2}
                    if self.ground_geom not in pair:
                        continue
                    f6 = np.zeros(6)
                    mujoco.mj_contactForce(m, data, c, f6)
                    if f6[0] > 1.0:
                        loaded.append(con.pos[:2].copy())
                        fric = max(fric, float(np.hypot(f6[1], f6[2]) / f6[0]))
                if len(loaded) >= 3:
                    margins.append(stability_margin(data.subtree_com[self.root][:2], loaded))
                for n in LEGS:
                    for j, key in zip(self.act[n], ("coxa", "femur", "tibia")):
                        peak[key] = max(peak[key], abs(float(data.actuator_force[j])))
        travel = float(data.xpos[self.root][0])
        overreach = over / max(1, steps)
        if outcome is None:
            if drift > 0.10:
                outcome = "slid"
            elif overreach > 0.02:
                outcome = "out_of_reach"
            elif travel < 0.85 * distance:
                outcome = "slid"
            else:
                outcome = "walked"
        return WalkResult(layout=self.layout_name, gait=g.name, posture=self.posture, grade=self.grade, mu=self.mu,
                          payload=self.d.payload, outcome=outcome, distance=round(travel, 3),
                          downhill_drift=round(drift, 3), max_tilt_deg=round(tilt_err, 1),
                          margin_p5=round(float(np.percentile(margins, 5)), 3) if margins else float("nan"),
                          overreach_frac=round(overreach, 3), peak_torque={k: round(v, 1) for k, v in peak.items()},
                          friction_use=round(fric, 2))
