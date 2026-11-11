# Roadmap: from simulation to a robot in the field

Written November 2026, at [checkpoint 1](checkpoint-2026-11.md). It is for
whoever leads the project and for the engineers who join it.

## The bar the robot has to clear

Prices and pay, from public reports. PROCAFE will know the real figures
better, and should correct them.

- Growers are paid a regulated minimum of **$22 per almud** of ripe cherries
  (28 lb, 12.7 kg). The floor was $18 until October 2026.
- A skilled picker fills a little over one almud an hour. Recent job postings
  for Puerto Rico coffee pickers list $9.94 an hour. So hand picking costs
  roughly **$10 an almud** in wages, before housing, recruiting and visas.
- The island needs about 7,000 pickers (a 2021 government estimate) and
  struggles to find them. It grows about 33,000 quintals a year and drinks
  about 300,000.

So a robot has two targets. At **$10 an almud** it matches a picker. At
**$22 an almud** it costs as much as the cherries sell for, which is the
ceiling for fruit that would otherwise stay on the tree.

Suppose a robot costs $150 a day to own and run. That is an assumption:
for example, $50,000 spread over four harvests of 100 days, plus $25 a day
for power and repairs. Then:

| | Almudes per day | Cost per almud |
|---|---|---|
| Needed to match a picker's wage | 15 | $10 |
| Needed to stay under the cherry price | 7 | $22 |
| A person, 8 hours | 6.5–8 | $10–12 |
| Single-cherry robot as simulated (17 s per cherry, 20 h) | 0.5 | about $290 |
| Single-cherry robot at 3 s per cherry, 20 h | 3 | about $50 |
| One comb arm, simulation estimate, 20 h | 25 | about $6 |

Two outside facts set expectations:

- The closest working machine is a Japanese robot that picks ripe cherry
  tomatoes by suction. After three years of development it picks 31 kg a day,
  roughly a few thousand tomatoes, in a greenhouse, on rails. A coffee picker
  handles about 54,000 cherries a day.
- No coffee-picking robot turned up in field trials anywhere. Nobody has
  shown it can be done, and nobody is ahead.

## The central bet

Picking one cherry at a time gives the best quality and cannot pay for bulk
harvest. To reach even the $22 ceiling with one arm it would need about
1.3 seconds per cherry, 20 hours a day. The comb can pay, but only if it
works on real branches the way it does in simulation, and without harming
next year's buds.

So the plan is:

- Prove or kill the comb first, cheaply, on real branches.
- Keep the suction cup for what it is uniquely good at: cleaning up after
  the comb, the last pass of the season, and specialty lots.
- Decide the vehicle last. It is the most expensive part, and the one with
  the most ready-made alternatives.

Each stage below ends in a gate: a measured result that says continue,
change course, or stop.

## Stages

| Stage | What | Who | Rough cost | When |
|---|---|---|---|---|
| 0. Measure | The seven field measurements, plus photos | 1–2 technicians, an agronomist | Under $2,000 | Now, while fruit is on the trees |
| 1. Hand tools | Hand-held comb and suction cup on real branches | 1–2 mechanical engineers, a technician | $5,000–15,000 | 2–3 months; needs fruit |
| 2. One arm at one plant | Camera, arm and tool on a stand in the field; no walking | 3–4 engineers | $40,000–100,000 | 2027 harvest |
| 3. Carrier | Put the picking unit on something that moves along a real row | Same team, plus a mechanical engineer | $10,000–100,000 | 2028 |
| 4. Field prototype | One robot, one full harvest, one or two farms | 5–8 people | $0.5–1.5 million all-in | 2028–29 harvest |
| 5. Small fleet | Several robots, service and a manufacturing partner | To be planned | To be planned | After stage 4 |

The costs are my own planning ranges, not quotes. Engineers should replace
them. The pattern matters more than the figures: the first two stages
together cost under $20,000 and settle the biggest questions.

### Stage 0. Measure (start now)

The harvest sets the calendar. Without these numbers before the fruit is
gone, most of the project waits until the next harvest.

- Follow [`field-protocol.md`](field-protocol.md). In priority order:
  pull-off force with its spread, pulled straight and sideways (item 1);
  foot grip and creep (4b); suction-cup seal (1b); branch census (3);
  rows and slopes (4); pickers' real pace and pay (5); cherry size (2).
- Take at least 2,000 photos of fruiting branches with the colour card in
  frame: sun, shade and wet, every variety grown.
- Send farm boundaries, so slope maps cover real farms.

I turn the data into updated simulation results within days of receiving it.

**Gate 0.**

- *Force.* The measured forces must let a perfect tool take at least 90% of
  ripe fruit while losing no more than 10% of green. With Brazil's gap
  (green holds 2.7 times harder than ripe), that needs a spread under 40%.
  If green only holds twice as hard, it needs a spread under 28%. If the gate
  fails, force alone cannot select. The robot must then rely on cameras and
  pick singly, or the answer lies in varieties.
- *Grip.* A spiked foot on wet soil should reach 0.8 or better, which opens
  about 70% of the belt. Below 0.5, legs and tracks are both limited to the
  gentler farms, and cables or rails become the option.
- *Seal.* The cup should seal on at least 90% of ripe cherries. Otherwise
  switch to soft fingers.

### Stage 1. Hand tools on real branches

Build three hand-held tools and test them on at least 200 branches:

- A comb whose tines fold at a set force, with swappable springs and a catch
  chute. The simulation says to start near 3.5 N per cherry, 12 tines, slow.
- A suction cup on a wand with a vacuum gauge, and a two-finger soft gripper
  as the fallback.
- A clamp to hold the branch, since holding it is what made picking reliable.

Measure ripe fruit taken, green fruit knocked off, kilograms per hour in a
person's hands, and damage to leaves and buds. Tag the branches and inspect
them again at the next flowering.

The comb is also a possible product in itself. A picker with a selective
comb might fill more almudes an hour. If it works, it could reach farms
years before a robot does.

**Gate 1.** The comb takes at least 80% of ripe fruit, knocks off no more
than 10% of green, and harms buds no more than hand picking does. If it
fails on damage, the comb route ends and the project becomes about quality
picking only.

### Stage 2. One arm at one plant

Mount a camera, an arm, the branch clamp and the best tool from stage 1 on a
stand or cart beside real plants. It does not walk. The work is everything
simulation cannot do: finding ripe fruit and branches in real light,
reaching through leaves, and running for hours.

**Gate 2.** One arm picks at least half an almud an hour, at least 85% of it
ripe, sustained for a full day. Two such arms working 20 hours give about 20
almudes a day, inside the range that pays.

### Stage 3. Carrier

The simulation says what any carrier must do: keep the body level, work
sideways along the contour, hold the branch while picking, and grip the
soil. It does not yet say the carrier must have legs. Buy or borrow before
building:

- A small tracked carrier. Remote-controlled slope mowers are sold with
  dealer ratings of 40° to 60°; a 100% grade is 45°. The open questions are
  whether one fits between coffee rows and what it does to roots and soil.
- A commercial four-legged robot as a mule. Published models exist, so it
  can be tried in simulation first.
- A cable or rail along the contour, for the steepest farms.
- The custom six-legged crab. It has about 30 motors, which makes it the
  most expensive option. It earns its place if the others fail on the
  steep, tight rows where its sideways gait and level body matter most.

Before any purchase I compare these in simulation on the real terrain of
PROCAFE farms, using 1 m elevation data.

**Gate 3.** The carrier takes the picking unit 100 m along a real contour
row at 50% grade or steeper, on wet ground, with a full basket. It must not
fall or damage plants, and must stay steady enough to pick.

### Stage 4. Field prototype

One robot works a full harvest. Measure almudes per day, cost per almud,
hours between breakdowns, and the share of plants it can reach.

**Gate 4.** Under $22 an almud all-in, with a believable path to $10.

## Work that runs alongside

- **Robot-ready rows.** This is PROCAFE's home ground, and may matter as much
  as the robot. Picking robots work where the crop is shaped for them; the
  tomato robot runs in a greenhouse built around it. Trials to start in 2027:
  pruning that keeps fruiting branches between knee and shoulder height and
  facing the path; contour paths of a fixed width; varieties that ripen
  evenly and have a wide gap between ripe and green pull-off force. Plants
  take two to three years to respond.
- **Seeing ripe fruit.** A computer-vision engineer trains a detector on the
  stage 0 photos during the off-season. Synthetic images from simulation can
  add to them.
- **Simulation.** Fold in the measurements; add a branch clamp to the comb;
  make leaves solid; try the comb followed by the cup, and several arms at
  once; walk rough ground from real elevation data; compare carriers.
- **Partners and money.** UPR Mayagüez evaluated a selective shaker in
  Puerto Rico in 2023 and is a natural partner for field trials. Funding
  leads to check: USDA NIFA's small-business and specialty-crop research
  programmes, and the Puerto Rico Science, Technology and Research Trust.
  I have not verified eligibility for any of them.

## People

- **Now:** a field lead, agronomist or senior technician, who owns the
  measurements and photos.
- **Soon:** a technical lead, a robotics or mechatronics engineer who owns
  the design decisions and the hardware. This is the most important seat.
- **Stage 1:** a mechanical designer for the tools.
- **Stage 2:** a computer-vision engineer, a robot-software engineer and an
  electronics engineer.
- **Stages 3–4:** a field-robotics mechanical engineer and a test lead.
- **Claude:** simulation, analysis of field data, software and documents.
  It cannot touch hardware or visit a farm.

## Main risks, and where each is settled

| Risk | Settled at |
|---|---|
| Ripe and green pull-off forces overlap in Puerto Rico varieties | Stage 0 |
| The comb harms next year's buds | Stage 1, confirmed at the next flowering |
| Cameras cannot find enough ripe fruit through the leaves | Photos, then stage 2 |
| Too slow to pay | Gate 2 |
| Soil too slick, or rows too tight, for any carrier | Stage 0 grip test, then stage 3 |
| A short season, rain and repairs push the cost per almud too high | Stage 4 |
| A robot idle for eight months a year | Off-season jobs such as weeding or pest scouting; not studied yet |

## Decisions needed now

1. **Who measures, on which farms, before the harvest ends?** Stage 0 needs
   under $2,000 of equipment and can start this week.
2. **Who is the technical lead?**
3. **Open or private?** The code and results sit in a public GitHub
   repository today. Decide before engineers join, along with a licence. The
   project can move to its own repository, private or public, with its
   history.
4. **Hand tool as well as robot?** I recommend both. The hand tool is the
   robot's first test, and could help pickers sooner.

## Sources

- Cherry price: [Rio Times, DACO Order 2026-007](https://www.riotimesonline.com/puerto-rico-coffee-prices-spike-2026/) and [Metro PR](https://www.metro.pr/noticias/2026/08/16/secretario-de-agricultura-avala-revision-de-precio-del-cafe/) (farms, pickers needed, production).
- Picker pay and pace: US Department of Labor seasonal job postings, for example [H-300-25118-912723](https://seasonaljobs.dol.gov/jobs/H-300-25118-912723).
- Tomato-picking robot: [FreshPlaza on Tokuiten](https://www.freshplaza.com/north-america/article/9842754/japanese-agri-tech-startup-puts-cherry-tomato-harvesting-robot-into-routine-production-use/).
- Slope mower ratings: dealer listings for [Green Climber](https://brimtractor.com/?p=7183) and [Spider](https://www.agrability.org/new-toolbox/718/) machines.
- Everything else: the README and its sources.
