"""Back-of-envelope picking rates: people, single-fruit robot arms, a branch comb.

Rates are in fruit per second and kg of cherry per hour so they line up with
the field numbers growers already know.
"""

from dataclasses import dataclass

from .params import Cherry

# Selective hand picking, Colombia: 82.6 kg cherry per worker-day.
# Cenicafe, Cenicafe 55(1):45-51 (2004).
HUMAN_KG_PER_DAY = 82.6
WORKDAY_H = 8.0  # ASSUMPTION: the study reports per day, not per hour.

# Brudden DSC-18 selective shaker, Puerto Rico field evaluation (UPR
# Mayaguez, 2023): 26.91 kg/h, with 25.38% unripe fruit in what it took.
PR_SHAKER_KG_PER_H = 26.91
PR_SHAKER_UNRIPE_SHARE = 0.2538

# Dual-arm apple harvester, 2025 commercial-orchard trials: 7.53 s per fruit
# per arm (arXiv 2506.05714). The best published single-fruit cycle times.
SINGLE_PICK_CYCLE_S = 7.53


def kg_per_h(fruit_per_s, cherry=Cherry()):
    return fruit_per_s * cherry.mass * 3600


def human_fruit_per_s(cherry=Cherry()):
    return HUMAN_KG_PER_DAY / cherry.mass / (WORKDAY_H * 3600)


def single_pick_fruit_per_s(cycle_s=SINGLE_PICK_CYCLE_S, arms=1):
    return arms / cycle_s


@dataclass
class CombRate:
    """Fruit per second for an arm that combs one branch after another."""
    target_per_branch: float   # ripe + overripe fruit on a branch at this pass
    target_yield: float        # share of them the comb takes (from the sim)
    branch_length: float = 0.40
    speed: float = 0.3
    # Moving to the next branch, finding its base and closing the comb on it.
    # ASSUMPTION; nothing like this has been timed yet.
    reposition_s: float = 4.0
    # Share of the shift the arm is actually combing rather than the platform
    # walking between plants. ASSUMPTION.
    duty: float = 0.7

    @property
    def cycle_s(self):
        return self.branch_length / self.speed + self.reposition_s

    @property
    def fruit_per_s(self):
        return self.duty * self.target_per_branch * self.target_yield / self.cycle_s
