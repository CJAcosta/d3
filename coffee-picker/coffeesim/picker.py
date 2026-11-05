"""Single-fruit picking: a crab-bodied robot with two arms.

The right arm carries a suction cup on a thin nozzle; the left arm carries a
claw. Two stalk cameras (colour + depth) find red and dark-red fruit. For each
one the left claw closes on the bare wood just inboard of the fruit's cluster,
the cup seals on the fruit, pulls it straight away from the branch, and drops
it in the basket.

Why the claw: a thin fruiting branch bends toward whatever pulls it. Without
a hold, 12 cm of pull near the tip could not build the 2-3 N a ripe fruit
needs. Pickers steady the branch with their other hand for the same reason.

The cup can pull at most `suction_cap` newtons (vacuum x cup area), so fruit
that holds harder than that stays on the branch whatever the camera thought.
That cap is a second, physical ripeness check.

The seal is modelled explicitly: when the cup face touches a fruit, a ball
joint constraint ties that one fruit to the cup tip; it breaks when the force
through it passes the cap. (MuJoCo's built-in adhesion acts at a distance and
splits its force across every fruit within reach, which is not how a sealed
cup behaves.) The claw's grip is a constraint pinning the branch where the
claw closed on it.

Speed is not the point here; every move is deliberately slow.
"""

import math
from dataclasses import dataclass, field

import mujoco
import numpy as np

from .detach import Detacher
from .params import DETACH_CV, TARGET_STAGES, Branch, Cherry, sample_thresholds
from .scene import build_scene

ARM_JOINTS = ("j1", "j2", "j3", "j4", "j5")
WHITE, DARK, GREEN, STEEL = ".92 .92 .9 1", ".16 .17 .18 1", ".12 .5 .27 1", ".7 .7 .72 1"
FRUIT_R = Cherry().half_width


@dataclass
class PickerConfig:
    # Most the cup can pull [N]: a 10 mm bellows cup at about 64 kPa vacuum.
    # Above ripe pull-off (3.3 N mean), below green (8.8 N).
    suction_cap: float = 5.0
    pick_base: tuple = (0.20, -0.40, 0.42)   # right arm (cup)
    hold_base: tuple = (0.00, -0.40, 0.42)   # left arm (claw)
    hold_branch: bool = True
    # The claw closes this far inboard of the fruit, in the bare wood
    # between clusters [m].
    hold_inboard: float = 0.026
    # Ground grade rising toward the plant. ASSUMPTION: 30%.
    slope: float = 0.30
    standoff: float = 0.07      # where the approach starts, from the fruit [m]
    # Most the cup backs away with the fruit [m]; stops as soon as the fruit
    # lets go or the seal breaks.
    pull: float = 0.06
    approach_s: float = 1.0
    pull_s: float = 2.0
    transit_s: float = 1.5
    dwell_s: float = 0.3
    release_s: float = 0.6
    image: tuple = (480, 360)   # eye camera resolution
    fovy: float = 55.0
    min_pixels: int = 20        # smaller red blobs are ignored
    timestep: float = 5e-4


@dataclass
class Detection:
    surface: np.ndarray   # visible point on the fruit [m]
    axis: np.ndarray      # unit vector, camera to fruit: the approach direction
    pixels: int
    camera: str
    clear: bool = True    # nothing else inside the cup's footprint in front of it
    pull: np.ndarray = None    # away from the branch (or back along `axis`)
    hold: np.ndarray = None    # where the claw should close, if the branch was seen
    branch_dir: np.ndarray = None
    truth: int = -1       # nearest simulated fruit, for scoring only

    @property
    def centre(self):
        return self.surface + FRUIT_R * self.axis


@dataclass
class PickRecord:
    fruit: int            # simulated fruit id the camera was aiming at
    stage: str            # its true stage
    threshold: float      # its true pull-off force [N]
    # picked | held_on (needed more than the cap) | seal_slipped (cup lost it
    # below its pull-off force) | branch_gave (full pull, still on) | dropped
    # | wrong_fruit | no_seal | unreachable
    outcome: str
    t_start: float
    t_end: float
    sealed: int = -1      # fruit the cup actually sealed on
    seal_peak: float = 0.0  # most the seal carried [N]
    held: bool = False    # the claw held the branch
    note: str = ""
    collateral: list = field(default_factory=list)  # other fruit knocked off


def _unit(v):
    return v / np.linalg.norm(v)


def _xyaxes(pos, target):
    f = _unit(np.asarray(target, float) - pos)
    right = _unit(np.cross(f, [0, 0, 1]))
    up = np.cross(right, f)
    return " ".join(f"{v:.5g}" for v in (*right, *up))


def _f(v):
    return " ".join(f"{x:.5g}" for x in v)


# Eye cameras on stalks, placed so neither parked arm blocks them.
EYES = {"eye_high": ((0.13, -0.60, 1.22), (0.20, 0.0, 0.95)),
        "eye_low": ((-0.12, -0.60, 0.80), (0.20, 0.0, 0.95))}
# Video cameras: the whole robot, and a close-up of the branch.
FILMS = {"film_wide": ((1.25, -1.60, 1.30), (0.15, -0.22, 0.62), 47),
         "film_close": ((0.56, -0.26, 1.22), (0.19, -0.01, 0.95), 52)}
BASKET = np.array([0.55, -0.40, 0.38])
TRUNK = np.array([0.0, 0.0])  # where the plant's stem stands (x, y)


def _arm_xml(prefix, base, tool):
    """Five-joint arm: yaw, three pitches, a side yaw. `tool` is cup or claw."""
    bx, by, bz = base
    col = 'contype="0" conaffinity="3" density="600"'
    vis = 'contype="0" conaffinity="0" density="600"'
    if tool == "cup":
        end = f"""
      <geom type="cylinder" fromto="0 0 0 0 0 0.14" size="0.006" rgba="{STEEL}" {col}/>
      <body name="cup" pos="0 0 0.144" gravcomp="1">
       <geom name="cup" type="cylinder" size="0.005 0.004" rgba=".08 .08 .08 1"
             friction="1.2 0.01 0.001" contype="0" conaffinity="3" mass="0.005"/>
       <site name="{prefix}_tip" pos="0 0 0.004" size="0.002"/>
      </body>"""
    else:
        end = f"""
      <geom type="cylinder" fromto="0 0 0 0 0 0.09" size="0.01" rgba="{STEEL}" {col}/>
      <body name="{prefix}_tool" pos="0 0 0.09" gravcomp="1">
       <geom type="box" pos="0 0 0.006" size="0.022 0.008 0.006" rgba="{DARK}" {vis}/>
       <geom type="capsule" fromto="0.018 0 0.008 0.011 0 0.040" size="0.0045" rgba="{GREEN}" {vis}/>
       <geom type="capsule" fromto="-0.018 0 0.008 -0.011 0 0.040" size="0.0045" rgba="{GREEN}" {vis}/>
       <site name="{prefix}_tip" pos="0 0 0.030" size="0.002"/>
      </body>"""
    return f"""
<geom type="cylinder" fromto="{bx} {by} {bz - 0.09} {bx} {by} {bz}" size="0.06" rgba="{DARK}" {vis}/>
<body name="{prefix}_base" pos="{bx} {by} {bz}">
 <body name="{prefix}_l1" gravcomp="1">
  <joint name="{prefix}_j1" axis="0 0 1" range="-3.1 3.1" damping="3" armature="0.02"/>
  <geom type="cylinder" fromto="0 0 0 0 0 0.12" size="0.05" rgba="{WHITE}" {vis}/>
  <body name="{prefix}_l2" pos="0 0 0.12" gravcomp="1">
   <joint name="{prefix}_j2" axis="1 0 0" range="-2.2 2.2" damping="3" armature="0.02"/>
   <geom type="cylinder" fromto="-0.05 0 0 0.05 0 0" size="0.045" rgba="{DARK}" {vis}/>
   <geom type="capsule" fromto="0 0 0 0 0 0.38" size="0.03" rgba="{WHITE}" {col}/>
   <body name="{prefix}_l3" pos="0 0 0.38" gravcomp="1">
    <joint name="{prefix}_j3" axis="1 0 0" range="-2.7 2.7" damping="2" armature="0.01"/>
    <geom type="cylinder" fromto="-0.042 0 0 0.042 0 0" size="0.038" rgba="{DARK}" {vis}/>
    <geom type="capsule" fromto="0 0 0 0 0 0.36" size="0.025" rgba="{WHITE}" {col}/>
    <body name="{prefix}_l4" pos="0 0 0.36" gravcomp="1">
     <joint name="{prefix}_j4" axis="1 0 0" range="-2.4 2.4" damping="1" armature="0.005"/>
     <geom type="cylinder" fromto="-0.032 0 0 0.032 0 0" size="0.03" rgba="{DARK}" {vis}/>
     <geom type="capsule" fromto="0 0 0 0 0 0.05" size="0.02" rgba="{GREEN}" {col}/>
     <body name="{prefix}_l5" pos="0 0 0.06" gravcomp="1">
      <joint name="{prefix}_j5" axis="0 1 0" range="-1.7 1.7" damping="1" armature="0.005"/>
      <geom type="cylinder" fromto="0 -0.026 0 0 0.026 0" size="0.024" rgba="{DARK}" {vis}/>{end}
     </body>
    </body>
   </body>
  </body>
 </body>
</body>"""


ARM_KP = {"j1": 1500, "j2": 3000, "j3": 2000, "j4": 600, "j5": 400}


def arm_actuators_xml():
    return "".join(f'<position name="{a}_{j}" joint="{a}_{j}" kp="{ARM_KP[j]}" dampratio="1" ctrllimited="false"/>'
                   for a in ("p", "h") for j in ARM_JOINTS)


def seal_grip_xml(cfg, n_fruit, n_seg):
    """Dormant constraints: a cup seal per fruit, a claw grip per branch segment."""
    tc = 5 * cfg.timestep
    soft = f'active="false" solref="{tc} 1" solimp="0.95 0.99 0.0005"'
    eq = "".join(f'<connect name="seal{k}" body1="cup" body2="cherry{k}" anchor="0 0 0.004" {soft}/>'
                 for k in range(n_fruit))
    eq += "".join(f'<connect name="grip{j}" body1="h_tool" body2="seg{j}" anchor="0 0 0.03" {soft}/>'
                  for j in range(n_seg))
    return eq


def robot_xml(cfg, n_fruit, n_seg):
    """(asset, worldbody, sections) MJCF strings for ground, robot and cameras.

    Dormant constraints are declared for every fruit (cup seal) and every
    branch segment (claw grip), so any of them can be switched on later.
    """
    theta = math.atan(cfg.slope)
    ground_z = lambda y: y * cfg.slope
    c = 0.5 * (np.array(cfg.pick_base) + np.array(cfg.hold_base)) + np.array([0, -0.07, -0.12])
    w = []
    w.append(f'<geom name="ground" type="plane" size="3 3 0.1" pos="0.2 0 0" euler="{theta:.5g} 0 0" '
             f'material="soil" contype="2" conaffinity="0"/>')
    w.append('<geom type="cylinder" fromto="0 0 -0.05 0 0 0.6" size="0.02" rgba=".35 .25 .15 1" '
             'contype="0" conaffinity="0"/>')
    for p, s in (((-0.8, 0.9, 0.55), (0.45, 0.35, 0.6)), ((1.1, 0.8, 0.6), (0.5, 0.35, 0.65)),
                 ((0.2, 1.4, 0.8), (0.7, 0.3, 0.6))):
        w.append(f'<geom type="ellipsoid" pos="{_f(p)}" size="{_f(s)}" rgba=".2 .42 .2 1" '
                 f'contype="0" conaffinity="0"/>')

    # Crab body: carapace, six legs splayed front-to-back as well as sideways
    # (docs/platform-crab.md: deep footprint downhill), eye stalks, basket.
    r = []
    r.append(f'<geom type="ellipsoid" pos="{_f(c)}" size="0.27 0.17 0.06" rgba="{WHITE}" contype="0" conaffinity="0"/>')
    r.append(f'<geom type="ellipsoid" pos="{_f(c + [0, 0, 0.035])}" size="0.15 0.09 0.035" rgba="{GREEN}" '
             f'contype="0" conaffinity="0"/>')
    for side in (1, -1):
        for dy in (0.09, 0.0, -0.09):
            hip = c + np.array([side * 0.23, dy, -0.01])
            foot = hip + np.array([side * 0.26, 1.6 * dy, 0])
            foot[2] = ground_z(foot[1]) + 0.012
            knee = 0.5 * (hip + foot) + np.array([side * 0.05, 0.3 * dy, 0.16])
            r.append(f'<geom type="capsule" fromto="{_f(hip)} {_f(knee)}" size="0.016" rgba="{WHITE}" '
                     f'contype="0" conaffinity="0"/>')
            r.append(f'<geom type="capsule" fromto="{_f(knee)} {_f(foot)}" size="0.012" rgba="{DARK}" '
                     f'contype="0" conaffinity="0"/>')
            r.append(f'<geom type="sphere" pos="{_f(knee)}" size="0.022" rgba="{DARK}" contype="0" conaffinity="0"/>')
    for name, (pos, target) in EYES.items():
        pos = np.array(pos)
        stalk_base = np.array([pos[0], c[1] - 0.05, c[2] + 0.05])
        r.append(f'<geom type="capsule" fromto="{_f(stalk_base)} {_f(pos - [0, 0, 0.03])}" size="0.009" '
                 f'rgba="{DARK}" contype="0" conaffinity="0"/>')
        r.append(f'<geom type="sphere" pos="{_f(pos - [0, 0.03, 0])}" size="0.026" rgba="{WHITE}" contype="0" conaffinity="0"/>')
        r.append(f'<camera name="{name}" pos="{_f(pos)}" xyaxes="{_xyaxes(pos, target)}" fovy="{cfg.fovy}"/>')
    # Basket on a side tray. Walls catch fruit.
    b = BASKET
    r.append(f'<geom type="capsule" fromto="{_f(c + [0.25, 0.02, 0.02])} {_f(b - [0.06, 0, 0.03])}" size="0.012" '
             f'rgba="{DARK}" contype="0" conaffinity="0"/>')
    walls = [((0, 0, -0.035), (0.07, 0.07, 0.005)), ((0.07, 0, 0), (0.005, 0.07, 0.035)),
             ((-0.07, 0, 0), (0.005, 0.07, 0.035)), ((0, 0.07, 0), (0.07, 0.005, 0.035)),
             ((0, -0.07, 0), (0.07, 0.005, 0.035))]
    for off, size in walls:
        r.append(f'<geom type="box" pos="{_f(b + off)}" size="{_f(size)}" rgba=".55 .4 .25 1" contype="2" conaffinity="0"/>')
    r.append(f'<site name="basket" pos="{_f(b)}" size="0.005" rgba="0 0 0 0"/>')
    r.append(_arm_xml("p", cfg.pick_base, "cup"))
    r.append(_arm_xml("h", cfg.hold_base, "claw"))
    for name, (pos, target, fovy) in FILMS.items():
        pos = np.array(pos)
        r.append(f'<camera name="{name}" pos="{_f(pos)}" xyaxes="{_xyaxes(pos, target)}" fovy="{fovy}"/>')
    r.append('<light pos="1.5 -1.5 2.5" dir="-0.5 0.5 -1" diffuse=".5 .5 .5"/>')

    act = arm_actuators_xml()
    eq = seal_grip_xml(cfg, n_fruit, n_seg)
    asset = ('<texture name="soil" type="2d" builtin="flat" rgb1=".38 .28 .18" rgb2=".3 .22 .14" '
             'mark="random" random="0.15" markrgb=".25 .2 .12" width="256" height="256"/>'
             '<material name="soil" texture="soil" texrepeat="12 12"/>')
    return asset, "".join(w) + "".join(r), f"<actuator>{act}</actuator><equality>{eq}</equality>"


def bark_mask(rgb):
    """Brown pixels: branch, trunk and soil. Depth separates them later."""
    x = rgb.astype(np.float32) / 255.0 + 1e-6
    r, g, b = x[..., 0], x[..., 1], x[..., 2]
    return (r > 0.12) & (g / r > 0.6) & (g / r < 0.82) & (b / r > 0.3) & (b / r < 0.6)


def fruit_mask(rgb):
    """Pixels the colour check calls ripe or overripe: red-dominant.

    Green, half-ripe amber, bark and soil all have red/green below 1.6.
    """
    x = rgb.astype(np.float32) / 255.0
    r, g, b = x[..., 0], x[..., 1], x[..., 2]
    return (r > 0.10) & (r > 1.9 * g) & (r > 1.6 * b)


class Arm:
    """Joint, actuator and site indices for one arm, plus its commanded pose."""

    def __init__(self, model, prefix):
        jid = [model.joint(f"{prefix}_{j}").id for j in ARM_JOINTS]
        self.prefix = prefix
        self.q = model.jnt_qposadr[jid]
        self.d = model.jnt_dofadr[jid]
        self.lo, self.hi = model.jnt_range[jid, 0], model.jnt_range[jid, 1]
        self.act = np.array([model.actuator(f"{prefix}_{j}").id for j in ARM_JOINTS])
        self.tip = model.site(f"{prefix}_tip").id
        self.shoulder = model.body(f"{prefix}_l2").id
        self.q_cmd = np.zeros(len(ARM_JOINTS))
        self.q_home = self.q_ready = None


class Picker:
    def __init__(self, cfg=None, branch=None, seed=0, cv=DETACH_CV, on_frame=None, frame_dt=1 / 30,
                 stages=None):
        self.cfg = cfg = cfg or PickerConfig()
        branch = branch or Branch(leaves=True)
        # Same seed twice: once to count the fruit, once for real with a seal
        # constraint per fruit.
        n = len(build_scene(branch=branch, rng=np.random.default_rng(seed), stages=stages).stages)
        rng = np.random.default_rng(seed)
        asset, world, sections = self._robot_xml(cfg, n, branch.n_nodes)
        self.info = build_scene(branch=branch, rng=rng, timestep=cfg.timestep, extra_asset=asset,
                                extra_world=world, extra_sections=sections, stages=stages,
                                **self._scene_kwargs())
        self.thresholds = sample_thresholds(self.info.stages, rng, cv)
        m = self.model = mujoco.MjModel.from_xml_string(self.info.xml)
        self.data = mujoco.MjData(m)
        self.ik_data = mujoco.MjData(m)
        # Picked fruit stays solid to ride the cup and land in the basket.
        self.det = Detacher(m, self.data, self.info, self.thresholds, evacuate=False, spin_damping=True)
        self.cup, self.claw = Arm(m, "p"), Arm(m, "h")
        self.cup_geom = m.geom("cup").id
        self.fruit_body = np.array([m.body(f"cherry{k}").id for k in range(n)])
        self.fruit_geom = m.body_geomadr[self.fruit_body]
        self.geom_to_fruit = {g: k for k, g in enumerate(self.fruit_geom)}
        self.seal_eq = np.array([m.equality(f"seal{k}").id for k in range(n)])
        self.seg_body = np.array([m.body(f"seg{j}").id for j in range(branch.n_nodes)])
        self.grip_eq = np.array([m.equality(f"grip{j}").id for j in range(branch.n_nodes)])
        self.internode = branch.internode
        self.suction_on, self.sealed, self.seal_broke, self.seal_force = False, None, False, 0.0
        self.seal_peak = 0.0
        self.gripping = None
        self.on_frame, self.frame_dt, self._next_frame = on_frame, frame_dt, 0.0
        self.status = ""
        self.records = []
        self.blocked = 0
        self.renderer = None
        self.basket = m.site("basket").id
        self._initial_state()
        mujoco.mj_forward(m, self.data)
        self._solve_poses()
        for arm in (self.cup, self.claw):
            arm.q_cmd = arm.q_home.copy()
            self.data.qpos[arm.q] = arm.q_home
            self.data.ctrl[arm.act] = arm.q_home
        mujoco.mj_forward(m, self.data)
        self.front0 = self.data.xpos[self.fruit_body][:, 1] < self.data.xpos[self.seg_body[0]][1]

    # -- hooks a robot on a moving body overrides ----------------------------
    def _robot_xml(self, cfg, n_fruit, n_seg):
        return robot_xml(cfg, n_fruit, n_seg)

    def _scene_kwargs(self):
        return {}

    def _initial_state(self):
        pass

    def _pre_step(self):
        pass

    def _solve_poses(self):
        """Parked poses out of the cameras' view, and reaching poses as IK seeds."""
        self.cup.q_home = self._solve(self.cup, self.data.site_xpos[self.basket] + [0, 0, 0.12], [0, 0, -1],
                                      [-1.2, -0.6, -1.8, -0.6, 0])
        self.claw.q_home = self._solve(self.claw, [-0.36, -0.32, 0.36], [0, 0, -1], [1.4, -0.9, -1.2, -0.6, 0])
        self.cup.q_ready = self._solve(self.cup, [0.2, -0.12, 0.98], [0, 0.9, -0.44], [0, -0.4, -1.0, -0.5, 0])
        self.claw.q_ready = self._solve(self.claw, [0.12, -0.08, 0.95], [0, 1, 0], [0, -0.4, -1.0, -0.5, 0])

    # -- simulation -------------------------------------------------------
    def step(self):
        self._pre_step()
        mujoco.mj_step(self.model, self.data)
        self.det.update()
        self._suction_update()
        if self.on_frame and self.data.time >= self._next_frame:
            self._next_frame += self.frame_dt
            self.on_frame(self)

    def hold(self, seconds):
        for _ in range(int(round(seconds / self.model.opt.timestep))):
            self.step()

    def set_suction(self, on):
        self.suction_on = on
        self.seal_broke = False
        if not on:
            self._unseal()

    # -- suction cup ------------------------------------------------------
    def _touching(self):
        """Fruit pressed against the cup face, if any (the one in deepest)."""
        best, best_dist = None, 1e-4
        d = self.data
        for c in d.contact[:d.ncon]:
            if self.cup_geom in (c.geom1, c.geom2) and c.dist < best_dist:
                other = c.geom2 if c.geom1 == self.cup_geom else c.geom1
                if other in self.geom_to_fruit:
                    best, best_dist = self.geom_to_fruit[other], c.dist
        return best

    def _pin(self, eq, tip, body):
        """Switch on a dormant connect so `body` stays where it is relative to
        the site `tip`."""
        m, d = self.model, self.data
        mujoco.mj_kinematics(m, d)  # positions after this step's integration
        m.eq_data[eq, 0:3] = m.site_pos[tip]
        m.eq_data[eq, 3:6] = d.xmat[body].reshape(3, 3).T @ (d.site_xpos[tip] - d.xpos[body])
        d.eq_active[eq] = 1

    def _seal(self, k):
        self._pin(self.seal_eq[k], self.cup.tip, self.fruit_body[k])
        self.sealed, self.seal_force, self.seal_peak = k, 0.0, 0.0

    def _unseal(self):
        if self.sealed is not None:
            self.data.eq_active[self.seal_eq[self.sealed]] = 0
        self.sealed = None

    def _seal_load(self):
        d = self.data
        rows = (d.efc_type[:d.nefc] == mujoco.mjtConstraint.mjCNSTR_EQUALITY) & \
               (d.efc_id[:d.nefc] == self.seal_eq[self.sealed])
        return float(np.linalg.norm(d.efc_force[:d.nefc][rows]))

    def _suction_update(self):
        # Once the seal breaks the attempt is over: a vacuum sensor sees the
        # loss and the cup does not grab the fruit again.
        if not self.suction_on or self.seal_broke:
            return
        if self.sealed is None:
            k = self._touching()
            if k is not None:
                self._seal(k)
            return
        # Low-pass over ~3 ms so a single contact spike does not break the seal.
        self.seal_force += min(1.0, self.model.opt.timestep / 3e-3) * (self._seal_load() - self.seal_force)
        self.seal_peak = max(self.seal_peak, self.seal_force)
        if self.seal_force > self.cfg.suction_cap:
            self._unseal()
            self.seal_broke = True

    # -- claw -------------------------------------------------------------
    def _segment_at(self, point):
        """Branch segment whose wood passes nearest `point`."""
        d, best, best_dist = self.data, None, np.inf
        for j, b in enumerate(self.seg_body):
            ax = d.xmat[b].reshape(3, 3)[:, 0]
            t = float(np.clip(np.dot(point - d.xpos[b], ax), 0, self.internode))
            dist = np.linalg.norm(d.xpos[b] + t * ax - point)
            if dist < best_dist:
                best, best_dist = j, dist
        return best

    def grip_branch(self, det, tag):
        """Close the claw on the wood inboard of the fruit. False if it can't."""
        if not self.cfg.hold_branch or det.hold is None:
            return False
        shoulder = self.data.xpos[self.claw.shoulder].copy()
        along = det.branch_dir
        axis = det.hold - shoulder
        axis = _unit(axis - np.dot(axis, along) * along)  # square onto the branch
        pre = det.hold - 0.07 * axis
        q_pre = self.reach(self.claw, pre, axis)
        line = self.plan_line(self.claw, pre, det.hold, axis, q_pre) if q_pre is not None else None
        if line is None:
            return False
        self.status = f"{tag}: left claw closes on the branch"
        self.move_joints(self.claw, q_pre, self.cfg.transit_s)
        self.follow(self.claw, line, self.cfg.approach_s)
        j = self._segment_at(self.data.site_xpos[self.claw.tip])
        self._pin(self.grip_eq[j], self.claw.tip, self.seg_body[j])
        self.gripping = (j, axis)
        self.hold(0.2)
        return True

    def release_branch(self):
        if self.gripping is None:
            return
        j, axis = self.gripping
        self.data.eq_active[self.grip_eq[j]] = 0
        self.gripping = None
        self.retreat(self.claw, axis, 0.07, 0.6)
        self.move_joints(self.claw, self.claw.q_home, self.cfg.transit_s)

    # -- kinematics -------------------------------------------------------
    def ik(self, arm, pos, axis, seed, iters=400):
        """Joint angles putting `arm`'s tip at `pos`, pointing along `axis`.

        `axis=None` constrains position only.
        """
        m, d = self.model, self.ik_data
        d.qpos[:] = self.data.qpos
        q = np.array(seed, float)
        jp, jr = np.zeros((3, m.nv)), np.zeros((3, m.nv))
        w = 0.2  # metres per radian of pointing error
        axis = None if axis is None else _unit(np.asarray(axis, float))
        for _ in range(iters):
            d.qpos[arm.q] = q
            mujoco.mj_kinematics(m, d)
            mujoco.mj_comPos(m, d)
            ep = pos - d.site_xpos[arm.tip]
            er = np.zeros(3) if axis is None else np.cross(d.site_xmat[arm.tip].reshape(3, 3)[:, 2], axis)
            if np.linalg.norm(ep) < 3e-4 and np.linalg.norm(er) < 3e-3:
                return q, True
            mujoco.mj_jacSite(m, d, jp, jr, arm.tip)
            if axis is None:
                J, e = jp[:, arm.d], ep
            else:
                J = np.vstack([jp[:, arm.d], w * jr[:, arm.d]])
                e = np.concatenate([ep, w * er])
            dq = J.T @ np.linalg.solve(J @ J.T + 2e-4 * np.eye(len(e)), e)
            dq *= min(1.0, 0.2 / max(1e-9, np.abs(dq).max()))
            q = np.clip(q + dq, arm.lo, arm.hi)
        z = d.site_xmat[arm.tip].reshape(3, 3)[:, 2]
        ok = np.linalg.norm(pos - d.site_xpos[arm.tip]) < 2e-3 and (axis is None or np.dot(z, axis) > 0.995)
        return q, ok

    def _solve(self, arm, pos, axis, seed):
        q, ok = self.ik(arm, np.asarray(pos, float), axis, seed=seed)
        if not ok:
            raise RuntimeError(f"pose unreachable for arm {arm.prefix}: {pos}")
        return q

    def reach(self, arm, pos, axis):
        """IK from several seeds; the solution closest to the current command.

        One seed is enough for nearby moves, but a fresh target can sit across
        a joint limit from wherever the arm is.
        """
        seeds = [arm.q_cmd, arm.q_ready, arm.q_home]
        rng = np.random.default_rng(0)
        seeds += [arm.q_ready + rng.normal(0, 0.5, len(ARM_JOINTS)) for _ in range(6)]
        best = None
        for seed in seeds:
            q, ok = self.ik(arm, pos, axis, seed=np.clip(seed, arm.lo, arm.hi))
            if ok and (best is None or np.abs(q - arm.q_cmd).sum() < np.abs(best - arm.q_cmd).sum()):
                best = q
        return best

    def move_joints(self, arm, q_goal, seconds):
        q0 = arm.q_cmd.copy()
        n = max(1, int(round(seconds / self.model.opt.timestep)))
        for i in range(n):
            s = 0.5 - 0.5 * math.cos(math.pi * (i + 1) / n)
            arm.q_cmd = q0 + s * (q_goal - q0)
            self.data.ctrl[arm.act] = arm.q_cmd
            self.step()

    def plan_line(self, arm, p0, p1, axis, seed, n_way=16):
        qs, q = [], seed
        for k in range(n_way + 1):
            q, ok = self.ik(arm, p0 + (p1 - p0) * k / n_way, axis, seed=q)
            if not ok:
                return None
            qs.append(q)
        return np.array(qs)

    def follow(self, arm, qs, seconds, until=None):
        """Track a planned joint path; stop early once `until()` is true."""
        n = max(1, int(round(seconds / self.model.opt.timestep)))
        for i in range(n):
            if until is not None and until():
                return True
            s = (0.5 - 0.5 * math.cos(math.pi * (i + 1) / n)) * (len(qs) - 1)
            k = min(int(s), len(qs) - 2)
            arm.q_cmd = qs[k] + (s - k) * (qs[k + 1] - qs[k])
            self.data.ctrl[arm.act] = arm.q_cmd
            self.step()
        return until is not None and until()

    def retreat(self, arm, axis, distance=0.08, seconds=0.8):
        """Back straight out of the canopy along `axis`, so the swing home
        does not sweep through fruit."""
        tip = self.data.site_xpos[arm.tip].copy()
        line = self.plan_line(arm, tip, tip - distance * axis, None, arm.q_cmd)
        if line is not None:
            self.follow(arm, line, seconds)

    # -- perception -------------------------------------------------------
    def _render(self, camera):
        W, H = self.cfg.image
        if self.renderer is None:
            self.renderer = mujoco.Renderer(self.model, H, W)
        r = self.renderer
        r.update_scene(self.data, camera=camera)
        rgb = r.render().copy()
        r.enable_depth_rendering()
        r.update_scene(self.data, camera=camera)
        depth = r.render().copy()
        r.disable_depth_rendering()
        return rgb, depth

    def look(self):
        """Red fruit seen by either eye, merged; each with an approach, a pull
        direction and a place for the claw."""
        m, d = self.model, self.data
        W, H = self.cfg.image
        f = 0.5 * H / math.tan(math.radians(self.cfg.fovy) / 2)
        found = []
        for cam in EYES:
            rgb, depth = self._render(cam)
            cid = m.camera(cam).id

            def to_world(rows, cols):
                z = depth[rows, cols]
                local = np.stack([(cols + 0.5 - W / 2) / f * z, (H / 2 - rows - 0.5) / f * z, -z], axis=1)
                return d.cam_xpos[cid] + local @ d.cam_xmat[cid].reshape(3, 3).T

            rows, cols = np.nonzero(fruit_mask(rgb) & (depth < 2.0))
            if rows.size == 0:
                continue
            pts, z = to_world(rows, cols), depth[rows, cols]
            bark = to_world(*np.nonzero(bark_mask(rgb) & (depth < 2.0)))
            order = np.argsort(z)
            pts, z, rows, cols = pts[order], z[order], rows[order], cols[order]
            free = np.ones(len(pts), bool)
            label = np.full(depth.shape, -1)
            # Greedy split into fruit-sized clusters, nearest surface first.
            clusters = []
            while free.any():
                i = np.flatnonzero(free)[0]
                near = free & (np.linalg.norm(pts - pts[i], axis=1) < 0.011)
                free &= ~near
                label[rows[near], cols[near]] = len(clusters)
                clusters.append((i, near))
            rr, cc_grid = np.ogrid[:H, :W]
            for c, (i, near) in enumerate(clusters):
                if near.sum() < self.cfg.min_pixels:
                    continue
                front = near & (z < z[i] + 0.003)
                surface = pts[front].mean(axis=0)
                axis = _unit(surface - d.cam_xpos[cid])
                # Clear path: within the cup's footprint (6 mm), nothing but
                # this fruit may sit closer than 4 mm behind its front.
                rc, cc = rows[front].mean(), cols[front].mean()
                rad = f * 0.006 / z[i]
                foot = (rr - rc) ** 2 + (cc_grid - cc) ** 2 <= rad ** 2
                other = foot & (depth < z[i] + 0.004) & (label != c)
                det = Detection(surface=surface, axis=axis, pixels=int(near.sum()), camera=cam,
                                clear=int(other.sum()) <= 3)
                self._read_branch(det, bark)
                found.append(det)
        # The two eyes often see the same fruit: keep the bigger view.
        found.sort(key=lambda x: -x.pixels)
        merged = []
        for det in found:
            if all(np.linalg.norm(det.centre - o.centre) > 0.009 for o in merged):
                merged.append(det)
        for det in merged:
            det.truth = self._nearest_fruit(det.centre)
        return merged

    def _read_branch(self, det, bark):
        """Fit a line to bark seen within 2.5 cm of the fruit. From it: the
        pull direction (straight out from the wood, loading the peduncle in
        tension, as a picker's fingers do) and where the claw should close
        (on the wood, toward the trunk, clear of the cluster). Without enough
        bark in view, the cup pulls straight back and the claw sits out."""
        back = -det.axis
        det.pull = back
        near = bark[np.linalg.norm(bark - det.centre, axis=1) < 0.025]
        if len(near) < 15:
            return
        mean = near.mean(axis=0)
        u = np.linalg.svd(near - mean, full_matrices=False)[2][0]
        on_line = mean + np.dot(det.centre - mean, u) * u
        out = det.centre - on_line
        if np.linalg.norm(out) > 1e-3:
            out = _unit(out)
            # Never pull into the nozzle's own path.
            det.pull = out if np.dot(out, back) > 0.3 else _unit(out + back)
        to_trunk = np.append(TRUNK - on_line[:2], 0.0)
        u = u if np.dot(u, to_trunk) > 0 else -u
        hold = on_line + self.cfg.hold_inboard * u
        if np.linalg.norm(hold[:2] - TRUNK) > 0.03:  # next to the stem it is stiff anyway
            det.hold, det.branch_dir = hold, u

    def _nearest_fruit(self, point):
        dist = np.linalg.norm(self.data.xpos[self.fruit_body] - point, axis=1)
        dist[~self.det.attached] = np.inf
        return int(np.argmin(dist))

    # -- picking ----------------------------------------------------------
    def pick(self, det, index):
        """One fruit: steady the branch, approach until the cup seals, pull
        straight off the wood, bin it."""
        cfg = self.cfg
        k = det.truth
        rec = PickRecord(fruit=k, stage=self.info.stages[k], threshold=float(self.thresholds[k]),
                         outcome="", t_start=self.data.time, t_end=0.0)
        before = self.det.attached.copy()
        a, s = det.axis, det.surface
        pre = s - cfg.standoff * a
        q_pre = self.reach(self.cup, pre, a)
        # Aim 10 mm past the seen surface; the approach stops at first touch.
        line_in = self.plan_line(self.cup, pre, s + 0.010 * a, a, q_pre) if q_pre is not None else None
        if line_in is None:
            rec.outcome, rec.t_end = "unreachable", self.data.time
            self.records.append(rec)
            return rec
        tag = f"Fruit {index}"
        rec.held = self.grip_branch(det, tag)
        if rec.held:
            # The claw can nudge the branch on its way in; look again before
            # the cup goes for the fruit.
            self.status = f"{tag}: branch held, looking again"
            self.hold(0.3)
            again = [x for x in self.look() if np.linalg.norm(x.centre - det.centre) < 0.02]
            if again:
                new = min(again, key=lambda x: np.linalg.norm(x.centre - det.centre))
                if new.clear:
                    if np.allclose(new.pull, -new.axis):
                        new.pull = det.pull  # the claw hides the bark now; keep the first reading
                    det = new
                    a, s = det.axis, det.surface
                    pre = s - cfg.standoff * a
                    q_pre = self.reach(self.cup, pre, a)
                    line_in = self.plan_line(self.cup, pre, s + 0.010 * a, a, q_pre) if q_pre is not None else None
                    if line_in is None:
                        self.release_branch()
                        rec.outcome, rec.t_end = "unreachable", self.data.time
                        self.records.append(rec)
                        return rec
        self.status = f"{tag}: cup moving in"
        self.move_joints(self.cup, q_pre, cfg.transit_s)
        self.status = f"{tag}: approach, suction on"
        self.set_suction(True)
        self.follow(self.cup, line_in, cfg.approach_s, until=lambda: self.sealed is not None)
        if self.sealed is None:
            self.set_suction(False)
            rec.outcome = "no_seal"
            self.status = f"{tag}: cup found no fruit"
            self.retreat(self.cup, a, 0.05, 0.5)
            self.release_branch()
        else:
            rec.sealed = self.sealed
            self.status = f"{tag}: sealed, pulling (cup lets go at {cfg.suction_cap:g} N)"
            self.hold(cfg.dwell_s)
            tip = self.data.site_xpos[self.cup.tip].copy()
            # Position only: through the ball-jointed seal the cup's angle
            # does not matter while pulling, and fixing it can make the path
            # unreachable.
            pull = det.pull if det.pull is not None else -a
            line_out = self.plan_line(self.cup, tip, tip + cfg.pull * pull, None, self.cup.q_cmd, n_way=24)
            if line_out is None:
                line_out = self.plan_line(self.cup, tip, tip - cfg.pull * a, None, self.cup.q_cmd, n_way=24)
            if line_out is not None:
                k_sealed = self.sealed
                self.follow(self.cup, line_out, cfg.pull_s,
                            until=lambda: self.sealed is None or not self.det.attached[k_sealed])
            else:
                rec.note = "pull path unreachable"
            self.hold(0.2)
            held = self.sealed
            rec.seal_peak = self.seal_peak
            if held is not None and not self.det.attached[held]:
                rec.outcome = "picked" if held == k else "wrong_fruit"
                self.status = f"{tag}: came off at {self.det.peak[held]:.1f} N, to the basket"
                self.retreat(self.cup, a)
                self.release_branch()
                self.move_joints(self.cup, self.cup.q_home, cfg.transit_s)
                self.set_suction(False)
                self.hold(cfg.release_s)
            else:
                stuck = rec.sealed
                self.set_suction(False)
                if self.det.attached[stuck] and held is not None:
                    # Seal intact, fruit still on: the pull ran out of reach.
                    rec.outcome = "branch_gave"
                    self.status = f"{tag}: full pull, still on"
                elif self.det.attached[stuck]:
                    # Did it hold because it needed more than the cap (the
                    # ripeness check working), or did the seal slip first?
                    strong = self.thresholds[stuck] > cfg.suction_cap
                    rec.outcome = "held_on" if strong else "seal_slipped"
                    self.status = (f"{tag}: held on past {cfg.suction_cap:g} N, left on the branch" if strong
                                   else f"{tag}: seal slipped before the fruit let go")
                else:
                    rec.outcome = "dropped"
                    self.status = f"{tag}: came off but slipped from the cup"
                self.retreat(self.cup, a)
                self.release_branch()
                self.hold(0.3)
        rec.collateral = [int(j) for j in np.flatnonzero(before & ~self.det.attached)
                          if j not in (k, rec.sealed)]
        rec.t_end = self.data.time
        self.records.append(rec)
        return rec

    def run(self, settle=0.6, max_picks=80):
        self.status = "Settling"
        self.hold(settle)
        self.pick_all(max_picks)
        self.status = "Done"
        self.move_joints(self.cup, self.cup.q_home, 1.0)
        self.hold(0.5)
        return self.summary()

    def pick_all(self, max_picks=80):
        """Look, take the best-seen red fruit with a clear path, repeat.

        Both arms park out of the cameras' view before every look: each pick
        changes the branch (it springs up as it loses fruit), so an old
        picture would be stale.
        """
        # Estimated centres of fruit already tried and failed; not tried twice.
        # Centres, not surfaces: the two eyes see opposite faces of a fruit.
        failed = []
        n = 0
        while n < max_picks:
            if np.abs(self.cup.q_cmd - self.cup.q_home).max() > 1e-3:
                self.move_joints(self.cup, self.cup.q_home, 1.0)
            self.status = "Looking for red fruit"
            self.hold(0.8)  # let the branch stop bouncing first
            seen = [x for x in self.look() if all(np.linalg.norm(x.centre - f) > 0.010 for f in failed)]
            ready = [x for x in seen if x.clear]
            self.blocked = len(seen) - len(ready)
            if not ready:
                self.status = ("No red fruit with a clear path left in view"
                               + (f" ({self.blocked} blocked)" if self.blocked else ""))
                self.hold(1.5)
                break
            n += 1
            det = max(ready, key=lambda x: x.pixels)
            self.status = f"{len(ready)} red fruit in clear view; taking the best-seen"
            self.hold(0.6)
            rec = self.pick(det, n)
            if rec.outcome not in ("picked", "wrong_fruit"):
                failed.append(det.centre)

    def summary(self):
        stages = np.array(self.info.stages)
        target = np.isin(stages, TARGET_STAGES)
        in_basket = self.fruit_in_basket()
        outcomes = {}
        for r in self.records:
            outcomes[r.outcome] = outcomes.get(r.outcome, 0) + 1
        off = ~self.det.attached
        picked = [r for r in self.records if r.outcome == "picked"]
        return {
            "sim_time_s": round(self.data.time, 1),
            "attempts": len(self.records),
            "outcomes": outcomes,
            "branch_held": sum(r.held for r in self.records),
            "target_total": int(target.sum()),
            "target_in_basket": int((in_basket & target).sum()),
            "target_left": int((target & self.det.attached).sum()),
            # Ripe fruit that came off but missed the basket (knocked off, or
            # bounced out).
            "target_off_not_in_basket": int((target & off & ~in_basket).sum()),
            "nontarget_in_basket": int((in_basket & ~target).sum()),
            "nontarget_knocked_off": int((off & ~target).sum()),
            "blocked_at_end": self.blocked,
            "front_target_total": int((target & self.front0).sum()),
            "front_target_in_basket": int((in_basket & target & self.front0).sum()),
            "mean_cycle_s": round(float(np.mean([r.t_end - r.t_start for r in picked])), 2) if picked else None,
        }

    def fruit_in_basket(self):
        d = self.data
        local = (d.xpos[self.fruit_body] - d.site_xpos[self.basket]) @ d.site_xmat[self.basket].reshape(3, 3)
        return (np.abs(local[:, 0]) < 0.075) & (np.abs(local[:, 1]) < 0.075) & (local[:, 2] < 0.04)
