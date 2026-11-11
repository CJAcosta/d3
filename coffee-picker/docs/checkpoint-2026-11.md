# Checkpoint 1: November 2026

A snapshot of the coffee-picker project after its first simulation phase.
The code and results are those of commit `fb9fa54` on branch
`ccr-74321688-r6rcfo`; this document and the roadmap were added right after.
What comes next is in [`roadmap.md`](roadmap.md).

## Where the project stands

Everything so far is simulation and analysis. Nothing has been built, and
nothing has been measured on a Puerto Rico farm. The simulations are a set of
tested computer models that answer "could this work, and what would decide
it?" They rest on Brazilian laboratory measurements and on assumptions that
are listed below.

The short version:

1. **Picking only ripe cherries by force is plausible.** Ripe cherries let go
   at about 3.3 N of pull and green ones at about 8.8 N. A tool that never
   pulls harder than about 5 N takes ripe fruit and leaves green fruit.
2. **Two tools use that fact, with opposite strengths.** A comb sweeps a whole
   branch: fast, but in simulation no more selective than today's vibrating
   harvesters. A suction cup takes one cherry at a time: it took no green
   fruit at all, but needs about 17 seconds per cherry.
3. **A six-legged "crab" can work the slopes** if it keeps its body level
   and its feet grip the soil. Half of Puerto Rico's coffee belt is steeper
   than 49% grade.
4. **Speed and cost decide whether this is a business.** At today's prices,
   and assuming a robot costs $150 a day, it has to pick roughly 7 to 15
   almudes a day to pay for itself (the arithmetic is in the roadmap). The
   one-cherry-at-a-time robot picks half an almud. The comb could reach that
   range, if the simulation holds up on real branches.

## What was built

| Piece | What it showed | Evidence |
|---|---|---|
| Branch and cherry model | Cherries release within 0.03 N of their set pull-off force | `tests/test_model.py` |
| Force-capped comb, 240 runs | 80% of ripe fruit taken, 6% of green knocked off, 89% of the harvest ripe. A perfect cap would take 97%. | `results/summary.md`, `results/tradeoff.png` |
| Two-armed single-cherry picker | 27 of 47 ripe cherries in the basket (19 of the 23 facing the robot), no green fruit taken or knocked off, 17 s per cherry | `results/single_pick.mp4`, `results/single_pick.json` |
| Slopes of the coffee belt | Median 49% grade; only 22% of the land is at or below 30% | `results/pr_slopes.json` |
| Walking crab, 288 runs | A body that tilts with the ground tips over at about 20% grade. Level or leaning, the same robot walks 66% of the belt on firm soil. | `results/walk_summary.md`, `results/walk_demo.mp4` |
| Leg control, 124 runs | Plain stiff joint motors reach 80–90% of the friction limit. Force-controlled legs add about 5% grade. | `results/grip_summary.md` |
| Whole robot on a 50% slope | Walk to the plant, pick, walk on. 11 of 14 attempts took their cherry; the other 3 were ripe fruit needing more than 5 N. Standing margin 33 cm. | `results/field_pick.mp4`, `results/field_pick.json` |
| Field protocol | Seven measurements a technician can make with hand tools | `docs/field-protocol.md` |

All 21 automated tests pass at this checkpoint. The README explains how to
re-run every result.

## What we learned

- **Hold the branch.** Thin branches bend toward whatever pulls on them. With
  a second claw holding the branch, 70% of picking attempts succeeded; with
  one arm, 39%.
- **The force cap is a second ripeness check.** When the camera aimed at a
  ripe cherry but the cup landed on a green neighbour, the cap left the green
  one on the plant.
- **Posture beats body shape.** Keeping the body level or leaning matters
  more than where the legs attach. Spreading the feet front to back gives the
  margin to carry a full basket.
- **Foot grip is the number that decides the platform.** It sets how much of
  the coffee belt a legged robot can walk: 43% of the belt at grip 0.5, 72%
  at 0.8, 95% at 1.2.
- **Picking does not strain the platform.** Standing on six legs, the arms'
  pulls barely register. Walking is where the robot is closest to its limits.
- **Working more hours does not close the speed gap.** A 20-hour day is
  worth 2.5×. The gap to a person, picking one cherry at a time, is more
  than 10×.

## What is assumed, not measured

Every one of these comes from another country, a 25-year-old plot, or a
guess. Each has a matching item in the field protocol.

| Assumption | Value used | Why it matters | Protocol item |
|---|---|---|---|
| Pull-off force by ripeness | 8.8 / 5.6 / 3.3 / 1.6 N (Brazil, 2000) | The whole idea rests on the gap between ripe and green | 1 |
| Spread of pull-off force among cherries | 30% | Changes the comb's result more than any design choice: 96%, 84% or 72% of ripe fruit at 15%, 30% or 45% spread | 1 |
| Pulling sideways equals pulling straight | Assumed equal | The comb pushes sideways; no data exists | 1 |
| A 10 mm suction cup seals on a cherry and lets go at 5 N | Assumed | The single-cherry picker depends on it | 1b |
| Cherry size and weight | 1.53 g (Rwanda) | Converts cherries to kilograms | 2 |
| Branch stiffness, cherries per branch, ripeness mix | Guesses | Sets how much the branch bends and how much fruit each pass meets | 3 |
| Foot grip on soil | 0.5 / 0.8 / 1.2 | Decides how much land the robot can walk | 4b |
| Time to move from branch to branch | 4 s | Half of the comb's speed estimate | Stage 2 trial |

## What the simulation does not cover

- Leaves, buds and flowers getting in the way or being damaged. Damage to
  next year's buds could rule out the comb.
- Real light under a canopy. The cameras here see ideal colours.
- Rough ground: roots, stones, terraces, mud.
- Rain, dust, wear, battery life and repairs.
- The far side of the plant, which needs a pass from the next row.

## Corrections made along the way

These are recorded so nobody relies on the earlier versions.

- The first platform note said a wide, low crab resists tipping. That holds
  only for a body that tilts with the ground; the simulation showed posture
  is what matters.
- The first walking study tested slopes in 20% steps and understated plain
  joint motors. The finer study replaced its numbers.
- The first combined run let the feet creep downhill 12–24 cm per metre,
  because of a soft friction setting. Real soil creeps too, so a creep test
  was added to the protocol.
- The single-cherry picker's first suction model pulled on several cherries
  at once. It was replaced by a seal that holds one cherry and breaks at 5 N.
