# Coffee picker: selective harvesting on Puerto Rico slopes

A simulation-first study of whether a robot can pick ripe coffee selectively,
fast enough to matter, on steep mountain farms. It was started with PROCAFE.
It lives in this repository for now and can move to its own repository with
its history.

## The question this milestone answers

Picking one fruit at a time cannot pay. A selective picker in Colombia takes
82.6 kg of cherry a day (Cenicafé). That is about 1.9 fruit per second, at
1.53 g per fruit. The best published fruit-picking arm takes 7.5 s per fruit,
so it would take **14 arms to match one person**.

The opening is a property of the fruit itself. Ripe cherries come off at about
3.3 N; green ones hold until about 8.8 N (Sampaio 2000, five arabica
varieties). A tool that sweeps a whole branch while capping the push each
fruit feels between those two forces could take ripe fruit by the dozen and
leave green fruit on the plant.

**Milestone 1:** does that hold up once contact mechanics are real? Real
contact means tines landing unevenly, fruit pushing fruit, peduncles bending,
and the branch flexing.

## What the simulation says (240 runs)

![Trade-off between ripe fruit taken and green fruit lost](results/tradeoff.png)

Best setting at the middle assumption (a 3.5 N per-fruit rating, 12 tines,
0.1 m/s, pull-off spread CV 0.30), per pass:

- takes **80%** of ripe and overripe fruit;
- knocks off **6%** of green fruit and 34% of half-ripe fruit;
- **89%** of what it collects is ripe or overripe.

For scale, the selective shaker tested in Puerto Rico (Brudden DSC-18) worked
faster, at 26.9 kg/h, but 25% of what it took was unripe. That comparison sets
a field result against a simulation, which flatters the simulation.

1. **The principle holds; the comb is not yet good at it.** A perfect force cap
   would take 97% of ripe fruit at the same 6% green loss. The comb takes 80%,
   and its curve runs right past the 2025 Brazilian field result for selective
   vibration (89.5% ripe, 15.9% green). The gap is uneven force. At a 4 N
   rating, the push fruit actually felt ranged from 3 to 7.5 N (10th to 90th
   percentile), because a fruit gets one to three tines and fruit shove fruit.
   Green fruit felt more push than ripe (median about 6 N against 4 N),
   because its stiffer peduncle doesn't give. **Making every fruit feel the
   same push is the engineering target.**
2. **Fruit-to-fruit spread decides more than any comb setting.** Holding green
   loss at 10%, the comb takes 96%, 84% or 72% of ripe fruit, depending on
   whether pull-off forces vary by 15%, 30% or 45% around their mean. No
   source reports that spread. Measuring it is item 1 of the field protocol.
3. **Slow and sparse beats fast and dense.** At 3.5 N with 12 tines, moving
   at 0.3 m/s instead of 0.1 m/s raises green loss from 6% to 14%. Going from
   12 to 24 tines made no consistent difference.
4. **Loose fruit must leave the comb at once.** A closed ring traps fruit it
   has detached and plows it into the next cluster. In early runs that raised
   green loss from 18% to 27%. The real tool needs a catch path, such as an
   open side, a tube or suction.
5. **Throughput looks viable but rests on guesses.** One comb arm works out to
   about 16 kg/h, roughly 1.5 pickers; two arms make about 3 people. A
   single-fruit arm manages 0.7 kg/h. The comb figure assumes 41 ripe fruit per
   branch, 4 s to move to the next branch and 70% of time spent combing; none
   of those has been measured.

Full tables: [`results/summary.md`](results/summary.md). Raw runs:
[`results/sweep.csv`](results/sweep.csv).

**Not modelled yet:**

- leaves, buds and flowers, so the damage a comb does to next year's crop is
  unknown;
- pulling off sideways, which the model treats like pulling straight away;
- fatigue, where repeated light pushes eventually break a peduncle;
- wind and rain;
- the branch being held by a second hand.

## What is in the model

![Three moments of a comb pass](results/comb_pass.png)

- **Branch:** an 8-node cantilever of spring-hinged capsules, tapering from
  8 to 4 mm. Wood stiffness E = 2 GPa (assumed).
- **Fruit:** 10 sites per node in two rings, 90% occupied, giving about 73
  fruit per branch. Each fruit is a 13 × 15.4 mm, 1.53 g capsule.
- **Ripeness mix:** green 30%, half-ripe 15%, ripe 45%, overripe 10%. Assumed
  to represent a mid-season pass.
- **Peduncle:** three slide springs plus a bending spring. It is cut when the
  pull through it passes that fruit's own threshold. Thresholds are lognormal
  around Sampaio's stage means. Overripe is 1.6 N, scaled from conilon data.
  Stiffness depends on stage, read off Sampaio's force-displacement curves.
  The pull test in `tests/` checks that fruit releases within 0.03 N of its
  threshold.
- **Comb:** a ring of spring-preloaded tines that slides from the base to the
  tip. Each tine stays rigid until its share of the per-fruit rating is
  exceeded, then folds back over the fruit. Tines also bear on the bark, so
  the ring guides the branch. Detached fruit is removed from play at once,
  standing in for a catch tube (see the first finding).

Every number in `coffeesim/params.py` names its source or is marked
`ASSUMPTION`.

## Run it

```bash
pip install -r requirements.txt
python -m pytest -q tests                  # 7 tests, ~5 s
python scripts/run_sweep.py                # 240 runs, ~7 min on 4 cores
python scripts/summarize.py                # results/summary.md + tradeoff.png
MUJOCO_GL=osmesa python scripts/render_pass.py   # results/comb_pass.png (headless needs libosmesa6)
```

## Layout

| Path | What |
|---|---|
| `coffeesim/params.py` | All physical numbers, with sources |
| `coffeesim/scene.py` | Procedural MJCF: branch, fruit, comb |
| `coffeesim/detach.py` | Breakable peduncles |
| `coffeesim/trial.py` | One comb pass and its metrics |
| `coffeesim/selectivity.py` | Perfect-cap baseline (analytic) |
| `coffeesim/throughput.py` | Fruit/s and kg/h against people, arms, the Puerto Rico shaker |
| `docs/field-protocol.md` | What PROCAFE can measure to replace the assumptions |
| `docs/platform-crab.md` | The crab body plan on Puerto Rico slopes |

## Milestones

1. **Selective comb (this).** Simulation done; the field numbers it needs are
   in `docs/field-protocol.md`.
2. **Bench test.** A hand-held comb with swappable tine springs, used on real
   branches during the Puerto Rico harvest. It measures what the simulation
   cannot: leaf and bud damage, and lateral pull-off. A dial-a-force comb
   costs little and answers the main question directly.
3. **Platform.** Crab-hexapod against quadruped against tracked carrier, on
   slope profiles from Puerto Rico lidar (`docs/platform-crab.md`).
4. **Perception.** Branch and ripeness detection under canopy shade, trained
   on the field photos from the protocol.

## Sources

- Sampaio, C. P. (2000). *Determinação da força requerida para o desprendimento de frutos de café em diferentes estádios de maturação.* M.Sc. thesis, UFV. [locus.ufv.br](https://locus.ufv.br/items/ce45fb2f-a85a-4322-be05-84e63bd8b3a4/full)
- Souza, G. S. et al. (2018). Força de desprendimento de frutos de café conilon. *Pensar Acadêmico* 16(1):103–108. [incaper](https://biblioteca.incaper.es.gov.br/digital/bitstream/123456789/3176/1/BRT-forcadodesprendimentodefrutosdocafeconilon-soaressouza.pdf)
- Physical and mechanical properties of arabica cherries and beans in Rwanda. [dspace.uac.bj](https://dspace.uac.bj/handle/123456789/10981)
- Cenicafé (2004), harvest labour productivity, *Cenicafé* 55(1):45–51. [pdf](https://www.cenicafe.org/es/publications/arc055%2801%29045-051.pdf)
- Selective vibration harvesting (2025), *Agriculture* 15(10):1036. [ideas.repec.org](https://ideas.repec.org/a/gam/jagris/v15y2025i10p1036-d1653266.html)
- Brudden DSC-18 evaluation in Puerto Rico (UPR Mayagüez, 2023). [scholar.uprm.edu](https://scholar.uprm.edu/entities/publication/5eb4945e-126d-401e-b887-dd25eadd8d8c)
- Dual-arm apple harvester field evaluation (2025). [arXiv 2506.05714](https://arxiv.org/abs/2506.05714)
- USGS Puerto Rico lidar 2015–2017 (1 m DEMs). [NOAA InPort 54852](https://www.fisheries.noaa.gov/inport/item/54852)
- NVIDIA SimReady assets on Hugging Face. [nvidia/simready-assets](https://huggingface.co/datasets/nvidia/simready-assets), [deeproboticslab/simready-assets](https://huggingface.co/datasets/deeproboticslab/simready-assets)
