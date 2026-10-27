"""Procedural MJCF for one coffee branch, its cherries and the comb.

The branch grows along +x from the trunk as a chain of capsule segments joined
by spring hinges (a discretised cantilever). Each cherry hangs off its node on
a peduncle modelled as three orthogonal slide springs plus a bending (ball)
spring. `detach.Detacher` zeroes those springs once the pull through them
passes the cherry's pull-off threshold, which leaves the fruit free.

Springs, not equality constraints, so the solver only handles contacts: about
10x faster than welds for the same scene.

Cherries are capsules 13.0 mm wide and 15.4 mm long.
"""

from dataclasses import dataclass

import numpy as np

from .params import PEDUNCLE_STIFFNESS, STAGES, Branch, Cherry, Comb

# Collision bits. Cherries hit cherries, the branch and tines. Tines also hit
# the branch, so the ring guides it rather than letting it slip out sideways.
# Branch segments never hit each other; tines never hit tines.
CHERRY = 'contype="1" conaffinity="3"'
BRANCH = 'contype="2" conaffinity="0"'
TINE = 'contype="0" conaffinity="3"'

BRANCH_HEIGHT = 1.0
STAGE_RGBA = {"green": ".35 .6 .2 1", "semi": ".85 .6 .15 1",
              "ripe": ".75 .08 .1 1", "overripe": ".3 .1 .12 1"}


@dataclass
class SceneInfo:
    xml: str
    stages: list          # stage per cherry, index = cherry id
    node_of: np.ndarray   # node index per cherry
    stiffness: np.ndarray  # peduncle stiffness per cherry [N/m]
    branch: Branch
    comb: Comb | None

    @property
    def branch_length(self):
        return self.branch.n_nodes * self.branch.internode


def _radius_at(branch, x):
    t = np.clip(x / (branch.n_nodes * branch.internode), 0.0, 1.0)
    return branch.radius_base + t * (branch.radius_tip - branch.radius_base)


def _fmt(v):
    return " ".join(f"{x:.6g}" for x in v)


def _draw_stages(branch, n, rng):
    names = list(branch.stage_mix)
    p = np.array([branch.stage_mix[s] for s in names], dtype=float)
    return [names[i] for i in rng.choice(len(names), size=n, p=p / p.sum())]


def _node_cherries(j, branch, cherry, stages, rng, out):
    """MJCF for the cherries on node j, in segment j's frame."""
    L = branch.internode
    xml = []
    for ring in range(branch.rings_per_node):
        dx = (ring - (branch.rings_per_node - 1) / 2) * branch.ring_spacing
        rb = _radius_at(branch, (j + 1) * L + dx)
        # On thin wood the ring would overlap; push it out until neighbours
        # just clear (0.3 mm) so no fruit starts preloaded.
        r_cap = max(rb + cherry.peduncle + cherry.half_width,
                    (cherry.half_width + 0.3e-3) / np.sin(np.pi / branch.sites_per_ring))
        rc = r_cap + cherry.half_length - cherry.half_width
        a = rc - rb  # cherry centre to its attachment on the bark
        offset = np.pi / branch.sites_per_ring * ring
        for site in range(branch.sites_per_ring):
            if rng.random() > branch.site_occupancy:
                continue
            k = len(out["node_of"])
            if k >= len(stages):
                raise ValueError("not enough stages for the occupied sites")
            stage = stages[k]
            phi = offset + 2 * np.pi * site / branch.sites_per_ring
            radial = np.array([0.0, np.cos(phi), np.sin(phi)])
            pos = np.array([L + dx, 0.0, 0.0]) + rc * radial
            ks = PEDUNCLE_STIFFNESS[stage]
            # Damping ~0.3 of critical keeps the peduncle from ringing.
            c = 0.6 * np.sqrt(ks * cherry.mass)
            slides = "".join(
                f'<joint name="c{k}_{ax}" type="slide" axis="{v}" stiffness="{ks:g}" damping="{c:.4g}" armature="1e-5"/>'
                for ax, v in (("x", "1 0 0"), ("y", "0 1 0"), ("z", "0 0 1")))
            xml.append(
                f'<body name="cherry{k}" pos="{_fmt(pos)}" zaxis="{_fmt(radial)}">'
                f'{slides}'
                f'<joint name="c{k}_bend" type="ball" pos="0 0 {-a:.6g}" stiffness="{cherry.bend_stiffness:g}" '
                f'damping="2e-5" armature="1e-7"/>'
                f'<geom type="capsule" size="{cherry.half_width} {cherry.half_length - cherry.half_width:.6g}" '
                f'mass="{cherry.mass}" rgba="{STAGE_RGBA[stage]}" {CHERRY}/>'
                f'</body>')
            out["node_of"].append(j)
            out["stages"].append(stage)
            out["stiffness"].append(ks)
    return "".join(xml)


def _branch_xml(branch, cherry, stages, rng, out):
    L = branch.internode
    xml = []
    for j in range(branch.n_nodes):
        r = _radius_at(branch, (j + 0.5) * L)
        k = branch.youngs_modulus * np.pi * (2 * r) ** 4 / 64 / L
        pos = f"0 0 {BRANCH_HEIGHT}" if j == 0 else f"{L} 0 0"
        hinges = "".join(
            f'<joint name="seg{j}_{ax}" type="hinge" axis="{v}" stiffness="{k:.6g}" '
            f'damping="{0.003 * k:.6g}" armature="1e-6"/>'
            for ax, v in (("y", "0 1 0"), ("z", "0 0 1")))
        xml.append(
            f'<body name="seg{j}" pos="{pos}">{hinges}'
            f'<geom type="capsule" fromto="0 0 0 {L} 0 0" size="{r:.6g}" density="{branch.wood_density}" '
            f'rgba=".45 .32 .2 1" {BRANCH}/>'
            + _node_cherries(j, branch, cherry, stages, rng, out))
    xml.append("</body>" * branch.n_nodes)
    return "".join(xml)


def tine_cap(comb, branch, cherry):
    """Breakaway force of one tine [N], so a fruit feels ~`comb.fruit_force`.

    A fruit spans width / pitch tines at its centre radius; each tine gets
    that share of the per-fruit rating (all of it when tines are sparser
    than fruit).
    """
    r = branch.radius_base + cherry.peduncle + cherry.half_length
    pitch = 2 * np.pi * r / comb.n_tines
    return comb.fruit_force * min(1.0, pitch / (2 * cherry.half_width))


def tine_stiffness(comb, branch, cherry):
    """Hinge stiffness that makes a tine break away at `tine_cap`.

    The cap holds for a push at a fruit's centre radius near the trunk; a
    push nearer the tine root needs proportionally more force.
    """
    lever = comb.root_radius - (branch.radius_base + cherry.peduncle + cherry.half_length)
    return tine_cap(comb, branch, cherry) * lever / comb.preload_angle


def _comb_xml(comb, branch, cherry):
    k = tine_stiffness(comb, branch, cherry)
    r_tip = branch.radius_base + comb.tip_clearance + comb.tine_radius
    tines = []
    for i in range(comb.n_tines):
        psi = 2 * np.pi * i / comb.n_tines
        c, s = np.cos(psi), np.sin(psi)
        root = comb.root_radius * np.array([0, c, s])
        tip = (r_tip - comb.root_radius) * np.array([0, c, s])
        # Positive rotation about the tangent swings the tip forward (+x);
        # the spring holds it against the 0 limit until the cap is exceeded.
        tines.append(
            f'<body name="tine{i}" pos="{_fmt(root)}">'
            f'<joint name="tine{i}" type="hinge" axis="0 {-s:.6g} {c:.6g}" range="-1.45 0" '
            f'stiffness="{k:.6g}" springref="{comb.preload_angle}" damping="{comb.tine_damping}" armature="1e-6"/>'
            f'<geom type="capsule" fromto="0 0 0 {_fmt(tip)}" size="{comb.tine_radius}" mass="{comb.tine_mass}" '
            f'friction="0.4" rgba=".8 .8 .85 1" {TINE}/>'
            f'</body>')
    return (f'<body name="comb" mocap="true" pos="-0.05 0 {BRANCH_HEIGHT}">'
            f'<geom type="cylinder" fromto="-0.004 0 0 0 0 0" size="{comb.root_radius + 0.004}" '
            f'rgba=".2 .4 .7 .25" contype="0" conaffinity="0"/>'
            + "".join(tines) + "</body>")


def build_scene(branch=None, cherry=None, comb=None, rng=None, stages=None,
                timestep=4e-4):
    """Return a SceneInfo with MJCF text and per-cherry bookkeeping.

    `stages` fixes the ripeness of each occupied fruit site in order;
    otherwise they are drawn from `branch.stage_mix`.
    """
    branch = branch or Branch()
    cherry = cherry or Cherry()
    rng = rng if rng is not None else np.random.default_rng(0)
    if stages is None:
        n_sites = branch.n_nodes * branch.rings_per_node * branch.sites_per_ring
        stages = _draw_stages(branch, n_sites, rng)
    for s in stages:
        if s not in STAGES:
            raise ValueError(f"unknown stage {s!r}")
    out = {"node_of": [], "stages": [], "stiffness": []}
    body = _branch_xml(branch, cherry, stages, rng, out)
    # MuJoCo's default 20 ms constraint time constant is far too soft for
    # gram-scale fruit pushed with newtons; tighten contacts and limits.
    tc = 5 * timestep
    xml = f"""
<mujoco model="coffee_branch">
  <compiler angle="radian" autolimits="true"/>
  <option timestep="{timestep}" integrator="implicitfast" iterations="50"/>
  <size memory="64M"/>
  <default>
    <geom solref="{tc} 1" solimp="0.95 0.99 0.0005" friction="0.5 0.005 0.0001"/>
    <joint solreflimit="{tc} 1"/>
  </default>
  <visual>
    <global offwidth="1280" offheight="720"/>
    <headlight ambient=".35 .35 .35" diffuse=".65 .65 .65" specular=".1 .1 .1"/>
  </visual>
  <asset><texture type="skybox" builtin="gradient" rgb1=".93 .94 .92" rgb2=".72 .78 .74" width="64" height="64"/></asset>
  <worldbody>
    <light pos="0.2 -0.4 1.8" dir="0 0.3 -1"/>
    <geom type="cylinder" fromto="0 0 0.6 0 0 1.4" size="0.012" rgba=".35 .25 .15 1" contype="0" conaffinity="0"/>
    {body}
    {_comb_xml(comb, branch, cherry) if comb else ""}
  </worldbody>
</mujoco>"""
    return SceneInfo(xml=xml, stages=out["stages"], node_of=np.array(out["node_of"]),
                     stiffness=np.array(out["stiffness"]), branch=branch, comb=comb)
