# Field measurements for PROCAFE

The simulator runs on Brazilian lab numbers and guesses. This protocol is what
it takes to replace them with Puerto Rico numbers. It is ordered by how much
each item changes the answer.

Everything here can be done by one technician with hand tools over a few
harvest days. Record into `field-data-template.csv` (one row per fruit) and a
second sheet per branch (section 3).

## 1. Pull-off force by ripeness: the number that decides everything

**Why:** the comb works only if ripe fruit lets go at a force green fruit
resists. The published data (Sampaio 2000, Brazil) gives only averages: green
8.8 N, half-ripe 5.6 N, ripe 3.3 N. It doesn't give the spread around those
averages, and the spread is what limits selectivity. In the sweep, going from
a tight spread (CV 0.15) to a loose one (CV 0.45) changes the result more than
any comb setting does.

**Kit:** digital push-pull force gauge, 0–50 N, 0.01 N resolution, with
peak-hold. Roughly US$150–300. A small wire loop or soft-jaw clip to hold the
fruit. A printed ripeness colour card, so stages are called the same way by
everyone.

**Stages:** green, pintón (half-ripe), ripe (cherry red), and overripe/dry.

**Sample:** for each variety your growers actually plant, 10 plants, with fruit
from the lower, middle and upper canopy. Measure at least 30 fruit per stage
per variety per day.

**Two pulls, because the comb pushes sideways:**

1. **Axial:** pull straight away from the branch along the peduncle. This is
   the textbook number.
2. **Lateral:** hook the fruit and pull parallel to the branch, toward the tip.
   This is how the comb actually loads fruit, and no published data covers it.

Note for each fruit whether it came off with or without its peduncle. Sampaio
found ripe fruit separates at the fruit end and green fruit at the branch end.

**Repeat:** at the start, peak and end of harvest, and in the morning versus
the afternoon. Brazilian studies show the force drifts with season, sun
exposure and recent rain.

**Better, if a test stand is available:** mount the gauge on a manual stand
and log force against displacement. The simulator's peduncle stiffness (green
about 4000 N/m, ripe about 800 N/m) was read off a 25-year-old plot.

### 1b. Does a small suction cup hold a cherry? (if a vacuum pump is at hand)

The single-fruit picker relies on a 10 mm cup letting go at about 5 N. That
cap is the vacuum level times the cup's area, and it acts as the physical
ripeness check. Whether a cup seals on a real cherry, and holds it, is the
first thing to check.

**Kit:** a small vacuum pump or venturi with a gauge, and a 10–12 mm bellows
cup on a hand-held tube. Clip the force gauge from item 1 behind the cup.

**Test:** on 20 ripe and 20 green fruit, in dry and wet conditions:

- record whether the cup seals at all;
- record the pull at which the seal fails or the fruit comes off;
- record which of the two happened.

## 2. Fruit size and mass

Weigh (to 0.01 g) and caliper 20 fruit per stage per variety. The model uses
1.53 g, 15.4 × 13.0 mm, from Rwanda.

## 3. Branch census: what the comb actually meets

For about 30 fruiting branches per variety, record:

- Length, diameter at the base and at the tip, and the number of nodes.
- For each node, the count of fruit at each stage. This sets the stage mix and
  how tightly clusters pack, both of which the model guesses.
- **Stiffness:** hang a 200 g weight 5 cm from the tip and measure how far the
  tip drops. That gives the wood stiffness, which the model guesses at 2 GPa.
- **Photos:** two per branch from about 30 cm, with the colour card in frame.
  These also start the ripeness-detection image set, which we will need anyway.

## 4. The slopes and the rows

The platform cannot be chosen without these.

- **Slope:** USGS lidar covers Puerto Rico at 1 m resolution (2015–2017
  survey). Send farm boundaries or GPS points and the slope maps can be
  computed here. Spot-check a few rows with a phone clinometer.
- **Layout:** do rows follow the contour? Record row spacing, plant spacing,
  plant height after pruning, path width, terraces or none, and ground cover
  (leaf litter, roots, mud after rain).
- **Access:** how fruit leaves the field today (sacks carried uphill or
  downhill, cable lines, vehicles) and how far.

## 5. Today's picking, as the baseline

Video three or four pickers for 15 minutes each, recording branches per
minute, kg per hour, passes per season, and how often they reach versus walk.
Add the current pay rate per kg or per day. The economics compare against
this, not against Colombia.

## What comes back

Each item maps to a parameter in `coffeesim/params.py`:

| Item | Replaces |
|---|---|
| 1 | `DETACH_MEAN_N`, `DETACH_CV`, `PEDUNCLE_STIFFNESS` |
| 2 | `Cherry` |
| 3 | `Branch`: geometry, `stage_mix`, `youngs_modulus` |
| 4 | Platform study inputs (`docs/platform-crab.md`) |
| 5 | `throughput.py` baselines |
