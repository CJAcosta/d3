"""The best any force-capped picker could do: every fruit feels exactly F.

A fruit comes off iff its pull-off force is below F, so the take rate per
stage is the lognormal CDF at F. The simulated comb can only do worse; the
gap is what contact mechanics (uneven tine loading, fruit pushing fruit,
peduncle compliance) costs.
"""

from math import erf, log, sqrt

from .params import DETACH_CV, DETACH_MEAN_N, STAGES, TARGET_STAGES, lognormal_params


def take_rate(stage, force, cv=DETACH_CV, means=DETACH_MEAN_N):
    mu, sigma = lognormal_params(means[stage], cv)
    return 0.5 * (1 + erf((log(force) - mu) / (sigma * sqrt(2))))


def ideal(force, stage_mix, cv=DETACH_CV, means=DETACH_MEAN_N):
    """Target yield, green loss and harvest purity for a perfect cap at `force`."""
    rate = {s: take_rate(s, force, cv, means) for s in STAGES}
    w = {s: stage_mix.get(s, 0.0) for s in STAGES}
    taken = {s: w[s] * rate[s] for s in STAGES}
    target_w = sum(w[s] for s in TARGET_STAGES)
    total_taken = sum(taken.values())
    return {
        "target_yield": sum(taken[s] for s in TARGET_STAGES) / target_w,
        "green_loss": rate["green"],
        "semi_taken": rate["semi"],
        "purity": sum(taken[s] for s in TARGET_STAGES) / total_taken if total_taken else float("nan"),
    }
