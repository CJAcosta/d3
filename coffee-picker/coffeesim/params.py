"""Physical parameters for coffee cherries, branches and the comb end effector.

Every number names its source. Values marked ASSUMPTION are placeholders
until PROCAFE field measurements replace them (see docs/field-protocol.md).
"""

from dataclasses import dataclass, field

import numpy as np

STAGES = ("green", "semi", "ripe", "overripe")

# Stages a selective harvest should take. Overripe/dry fruit left on the plant
# feeds the coffee berry borer, so it counts as a target, not a loss.
TARGET_STAGES = ("ripe", "overripe")

# Mean pull-off force [N] for Coffea arabica, averaged over five Brazilian
# varieties, texture analyser at 1 mm/s.
# Sampaio, C. P. (2000). M.Sc. thesis, Universidade Federal de Vicosa.
#   green 8.8, green-cherry (semi) 5.6, cherry (ripe) 3.3.
# Overripe/dry is not in Sampaio. Souza et al. (2018, Pensar Academico 16(1))
# measured dry conilon fruit at 18-34% of the green force; 0.18 * 8.8 = 1.6 N.
# ASSUMPTION: that ratio carries over to arabica.
DETACH_MEAN_N = {"green": 8.8, "semi": 5.6, "ripe": 3.3, "overripe": 1.6}

# Spread of the detachment force within a stage. No source reports it
# (Sampaio gives means and Tukey groups only). ASSUMPTION, swept in results.
DETACH_CV = 0.30

# Peduncle stiffness [N/m] along the loading ramp of Sampaio's force vs
# displacement curves (Fig. 4, Conilon): green climbs ~6 N over ~1.5 mm,
# cherry ~2 N over ~3 mm. Read off a plot, so ASSUMPTION-grade; the field
# protocol records full curves to replace it.
PEDUNCLE_STIFFNESS = {"green": 4000.0, "semi": 2500.0, "ripe": 800.0, "overripe": 800.0}


@dataclass
class Cherry:
    # Rwandan arabica: mean mass 1.53 g, length 15.40 mm, width 13.03 mm.
    # Physical and mechanical properties of arabica cherries and beans in
    # Rwanda (dspace.uac.bj handle 123456789/10981).
    mass: float = 1.53e-3
    half_length: float = 7.70e-3
    half_width: float = 6.5e-3
    # Peduncle length between the branch surface and the fruit. ASSUMPTION.
    peduncle: float = 2.0e-3
    # Bending stiffness [N m/rad] of the peduncle: a 1 N side push at the
    # fruit centre tilts it ~10 degrees. ASSUMPTION.
    bend_stiffness: float = 0.05


@dataclass
class Branch:
    # Plagiotropic branch geometry. ASSUMPTION, to be measured in PR farms.
    n_nodes: int = 8
    internode: float = 0.05
    radius_base: float = 4.0e-3
    radius_tip: float = 2.0e-3
    wood_density: float = 800.0
    # Young's modulus of green coffee wood. ASSUMPTION.
    youngs_modulus: float = 2.0e9
    # Fruit sites per node: rings around the branch x sites per ring.
    rings_per_node: int = 2
    sites_per_ring: int = 5
    ring_spacing: float = 14e-3
    site_occupancy: float = 0.9
    # Share of each stage on the branch at harvest time. ASSUMPTION; a
    # mid-season pass in a mixed-ripeness lot.
    stage_mix: dict = field(default_factory=lambda: {
        "green": 0.30, "semi": 0.15, "ripe": 0.45, "overripe": 0.10})


@dataclass
class Comb:
    """Ring of spring-preloaded breakaway tines that slides along the branch.

    Each tine is rigid until the push on it passes its breakaway force, then
    folds back and slips over the fruit. `fruit_force` is the design knob:
    the push one fruit should feel, set above the ripe pull-off force and
    below the green one. Each tine's share follows from the tine pitch
    (scene.tine_cap).
    """
    fruit_force: float = 5.0
    n_tines: int = 12
    root_radius: float = 0.040
    # Gap left between tine tips and the branch axis.
    tip_clearance: float = 1.5e-3
    tine_radius: float = 1.2e-3
    tine_mass: float = 2.0e-3
    # A long preload angle makes the spring nearly constant-force once it
    # breaks away, so the cap barely rises as the tine folds.
    preload_angle: float = 3.0
    tine_damping: float = 2e-4
    speed: float = 0.20


def lognormal_params(mean, cv):
    """Log-space (mu, sigma) of a lognormal with the given mean and CV."""
    sigma = np.sqrt(np.log1p(cv * cv))
    return np.log(mean) - 0.5 * sigma * sigma, sigma


def sample_thresholds(stages, rng, cv=DETACH_CV, means=DETACH_MEAN_N):
    """Draw one pull-off force per cherry from its stage's lognormal."""
    out = np.empty(len(stages))
    for i, s in enumerate(stages):
        mu, sigma = lognormal_params(means[s], cv)
        out[i] = rng.lognormal(mu, sigma)
    return out
