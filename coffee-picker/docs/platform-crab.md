# A crab body plan for Puerto Rico slopes

Status: a hypothesis to test in simulation (milestone 3), not a decision.

## What the biology does and does not say

The crab body plan (flattened carapace, abdomen tucked under) evolved
independently at least five times among decapod crustaceans. It was also lost
again ("decarcinization") at least seven times. Biologists do not agree on why
it keeps appearing; protection and compactness are leading guesses.

So repeated convergence shows that the shape is a strong attractor for
decapods living on sea floors and shores. It does not show the shape is the
best way to move in general. Animals that live on steep mountain slopes, such
as goats and ibex, converged on four legs under a tall, narrow body instead.

The useful question is narrower: which crab traits fit this particular job?
Several do.

## Crab traits that fit coffee picking

1. **Walking sideways along the row.** Where rows follow the contour, as is
   usual on steep ground, a sideways walker keeps its claws facing the plants
   the whole time. It never has to turn between trees, and its arms stay in
   the canopy while it moves. A crab leg also swings mostly in one plane, so a
   leg built for sideways walking needs fewer motors than a general-purpose leg.
2. **Two claws doing two different jobs.** The comb simulation shows a fruiting
   branch whips out of the way when combed near its thin tip. Human pickers hold
   the branch with one hand and strip with the other. A crab's two chelae map
   directly onto that: one holds the branch, the other combs it.
3. **Many legs.** With 6–8 legs, at least 4 can stay planted while the others
   step, so the robot is always statically stable. It can also lose footing on
   one leg in mud and keep standing.
4. **A low, wide body,** which keeps the centre of gravity low.

## The catch: which way is "wide"

A crab's long axis runs side to side, across its leg span. When it walks
sideways along a contour row, that long axis lies along the contour, and its
short front-to-back axis points downhill. Downhill is exactly the direction it
can tip over.

The steepest slope a robot can stand on without tipping is:

    max grade = d / h

Here d is the distance from the centre of gravity to the downhill edge of the
footprint, and h is the height of the centre of gravity, both measured relative
to the slope.

| Footprint depth, downslope | CoG height 0.30 m | CoG height 0.45 m |
|---|---|---|
| 0.5 m | 83% | 56% |
| 0.7 m | 117% | 78% |
| 0.9 m | 150% | 100% |

These are static limits. Walking, an arm reaching downhill with a full bin,
and soft ground all eat into them, and a robot should normally run well inside
them. The design rule that follows is that the crab must be **at least as
deep, front to back, as it is wide**. Its downhill legs also need to reach
farther than its uphill legs, holding the body level. That is closer to a
round spider-like stance than to a typical side-to-side crab, and it is still
compatible with sideways walking.

## Cost

| Platform | Leg motors | Arm motors | Total |
|---|---|---|---|
| Crab: 8 legs × 3 + 2 arms × 6 | 24 | 12 | 36 |
| Crab-hexapod: 6 legs × 3 + 2 arms × 6 | 18 | 12 | 30 |
| Quadruped + 1 arm | 12 | 6 | 18 |
| Tracked carrier + 1 arm | 2 | 6 | 8 |

Each motor is a cost, a weight and a failure point. That is the main argument
against eight legs. It is also why the six-leg version in the concept image is
the likelier compromise.

## How milestone 3 will test it

Compare three platforms in simulation on slope profiles taken from Puerto Rico
lidar for actual PROCAFE farms:

- a crab-hexapod with two arms;
- a quadruped with one arm, using DEEP Robotics' published X30/Lite3 models
  from Hugging Face (`deeproboticslab/simready-assets`);
- a tracked carrier.

The scores:

- tip-over margin with the arm loaded;
- slip on wet clay;
- energy per metre of row;
- share of time the arms are in the canopy;
- motor count as a cost proxy.

## What we need from PROCAFE first

Field protocol section 4: farm boundaries for the slope maps, whether rows
follow contours, row and path widths, and ground conditions in the wet season.
