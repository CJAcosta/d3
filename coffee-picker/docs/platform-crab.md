# A crab body plan for Puerto Rico slopes

Status: first simulation pass done (`coffeesim/walker.py`, results in the
README and `results/walk_summary.md`). Still a hypothesis, not a decision.

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
   directly onto that: one holds the branch, the other combs or picks.

   The single-fruit picker (`coffeesim/picker.py`) tested this on one
   simulated branch. With one arm, the thin outer branch often just bent
   toward the robot: 12 of 61 attempts ended with 12 cm of pull and the fruit
   still on. With the left claw closed on the wood just inboard of the
   cluster, every held ripe fruit that came off did so at its own pull-off
   force, and 70% of attempts succeeded against 39% with one arm. The final
   count was almost the same (27 against 26 of 47 ripe fruit), because the
   one-armed robot kept retrying as the branch shifted. So the second claw
   buys reliability and fewer wasted attempts more than extra fruit.
3. **Many legs.** With 6–8 legs, at least 4 can stay planted while the others
   step, so the robot is always statically stable. It can also lose footing on
   one leg in mud and keep standing.
4. **A low, wide body,** which keeps the centre of gravity low.

## The slopes it has to walk

USGS 10 m elevation data for the coffee belt (250–1000 m, 1,705 km² from 67 W
eastward) gives a median grade of 49% (`results/pr_slopes.json`):

| Grade | Share of the belt no steeper |
|---|---|
| 30% (limit of Brazilian harvesting machines) | 22% |
| 45% | 43% |
| 60% | 66% |
| 80% | 88% |

The demos so far used 30%, easier than three quarters of the belt.

## What tips a crab, and what doesn't

A body that **tilts with the ground**, as a real crab's does or as a
vehicle's must, shifts its centre of mass downhill of its feet as the slope
steepens. It tips once that shift reaches the downhill edge of the footprint:

    max grade = d / h

Here d is the distance from the centre of mass to the downhill feet, and h is
the body's height off the ground, both measured relative to the slope. For a
classic crab, with feet 0.12 m downhill and the body 0.30 m up, that is 40%
standing on all six legs. Walking, a foot is always in the air, so the margin
roughly halves and it tips at about 20%. The simulation shows exactly this.

A body held **level** (or leaning part way) keeps its centre of mass over the
middle of its feet on any slope; the downhill legs just reach further down.
Then tipping stops being the limit, and two others take over:

- **Grip.** Whatever the legs do, the feet must hold the whole weight's
  downhill pull, so a robot slides once the grade passes its foot grip
  (friction coefficient). This simple controller gives out a step earlier,
  because stiff legs share sideways load unevenly.
- **Reach.** On steep ground the uphill legs fold toward their hips and the
  downhill legs stretch. The wider the footprint front to back, the sooner a
  leg runs out.

## What the walking simulation found

288 runs: classic or deep footprint; body tilted, level or leaning half the
slope; tripod or wave gait; grades 0–100%; foot grip 0.5, 0.8 or 1.2
(assumed: slick wet soil, firm soil, spiked feet that bite).

1. **Keeping the body level matters more than the footprint.** A tilted
   classic crab walks less than 11% of the coffee belt at any grip. Level or
   leaning, the same crab reaches 66% of the belt at grip 0.8.
2. **Grip sets the ceiling.**

   | Grip | Best design | Share of belt |
   |---|---|---|
   | 0.5 | 40% grade | 36% |
   | 0.8 | 60% grade | 66% |
   | 1.2 | 80–100% grade | 88–96% |

   Feet that bite into soil, which is what a crab's pointed tips do, are worth
   more than any leg layout. The real grip of rubber pads and spikes on
   Puerto Rico soils, wet and dry, is unmeasured: field protocol item 4b.
3. **Deep beats classic once the robot carries anything.** The classic
   footprint reaches steeper ground empty (100% at grip 1.2, body level), but
   with 2–4 cm of margin. Hang a full 6 kg basket on its downhill side
   (body leaning) and it tips by 40%. The deep footprint with a leaning body
   keeps 13 cm of margin empty and about 9 cm with the basket, and walks 80%
   grade at grip 1.2 either way.
4. **Joints peak at 20–35 N m** across the walkable cases, for a 22 kg body
   plus legs. That is roughly the class of mid-size quadruped robot joints.

So the revised design rule: hold the body level or leaning, never tilted
with the ground; spread the feet front to back as well as sideways (deep), so
there is margin for arms and a basket; and give the feet tips that bite.

Limits of this first pass:

- a smooth plane, with no roots, terraces, mud or loose stones;
- a 10 m elevation grid, which smooths the banks a foot actually meets;
- a position-controlled gait with no force control or terrain sensing;
- assumed grip values;
- walking only, with the arms parked.

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

## Still to test

- Rough ground: steps, roots, terraces and soft soil, from 1 m lidar profiles
  of actual PROCAFE farms.
- A controller that shares foot forces within each foot's grip, which should
  close the gap to the ideal (grade = grip).
- Arms working while standing on the slope, and walking with a load shifting
  as the basket fills.
- Against the same terrain: a quadruped with one arm (DEEP Robotics' X30 or
  Lite3 models on Hugging Face, `deeproboticslab/simready-assets`) and a
  tracked carrier. Score them on tip-over margin with the arm loaded, slip,
  energy per metre of row, share of time the arms are in the canopy, and
  motor count as a cost proxy.

## What we need from PROCAFE first

Field protocol section 4: farm boundaries for the slope maps, whether rows
follow contours, row and path widths, and ground conditions in the wet season.
Section 4b: foot grip on those soils, the number this study turned on.
