# Crab walking along the contour

288 runs. Each walks 1 m sideways along a slope at 4 cm/s. "Walked" means it covered the metre, slid less than 10 cm downhill and tilted less than 25 degrees.

Coffee belt: 1705 km2 between 250 and 1000 m, median grade 49% (`results/pr_slopes.json`).

## Steepest grade walked, and share of the coffee belt that is no steeper

Best of tripod and wave gait, on a 20%-step grid. In brackets: what stopped it at the next grade up (a slide often ends in a tip, and is then logged as one).

| Robot | grip 0.5 | grip 0.8 | grip 1.2 |
|---|---|---|---|
| Classic crab, body tilts with ground | under 20% → under 11% of belt (tipped at the next step) | under 20% → under 11% of belt (tipped at the next step) | under 20% → under 11% of belt (tipped at the next step) |
| Deep crab, body tilts with ground | 40% → 36% of belt (slid at the next step) | 40% → 36% of belt (slid at the next step) | 40% → 36% of belt (slid at the next step) |
| Classic crab, body level | 40% → 36% of belt (tipped at the next step) | 60% → 66% of belt (tipped at the next step) | 100% → 96% of belt |
| Deep crab, body level | 40% → 36% of belt (tipped at the next step) | 60% → 66% of belt (tipped at the next step) | 60% → 66% of belt (slid at the next step) |
| Classic crab, body leans half | 20% → 11% of belt (slid at the next step) | 60% → 66% of belt (tipped at the next step) | 80% → 88% of belt (slid at the next step) |
| Deep crab, body leans half | 40% → 36% of belt (tipped at the next step) | 60% → 66% of belt (slid at the next step) | 80% → 88% of belt (slid at the next step) |
| Ideal: slides only when grade > grip | 50% → 51% of belt | 80% → 88% of belt | 120% → 98% of belt |

## With a full 6 kg basket on the downhill side (body leaning)

| Robot | grip 0.5 | grip 0.8 | grip 1.2 |
|---|---|---|---|
| Classic crab, body leans half | 20% (tipped at the next step) | 20% (tipped at the next step) | 20% (tipped at the next step) |
| Deep crab, body leans half | 40% (tipped at the next step) | 60% (slid at the next step) | 80% (slid at the next step) |

## Margin and joint torque at the steepest grade walked (grip 1.2, no basket)

Margin: how far the centre of mass stays inside the feet (5th percentile over the walk). Torques are peaks across all legs.

| Robot | Grade | Gait | Margin | Coxa | Femur | Tibia |
|---|---|---|---|---|---|---|
| Classic crab, body tilts with ground | 0% | tripod | 6 cm | 0 N m | 33 N m | 23 N m |
| Deep crab, body tilts with ground | 40% | tripod | 4 cm | 14 N m | 23 N m | 20 N m |
| Classic crab, body level | 100% | tripod | 4 cm | 2 N m | 33 N m | 22 N m |
| Deep crab, body level | 60% | tripod | 14 cm | 6 N m | 22 N m | 23 N m |
| Classic crab, body leans half | 80% | wave | 2 cm | 20 N m | 22 N m | 21 N m |
| Deep crab, body leans half | 80% | tripod | 13 cm | 9 N m | 21 N m | 25 N m |
