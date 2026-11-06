# Leg control against the grip limit

Deep footprint, body leaning half the slope, 1 m along the contour at 4 cm/s, 5%-grade steps. "Walked": covered the metre, slid under 10 cm, tilted under 25 degrees. Best of tripod and wave gait. Ideal: slides only once the grade passes the grip.

| Foot grip | Stiff joint servos | Force-shared feet | Ideal |
|---|---|---|---|
| 0.5 | 45% (90% of grip) → 43% of belt, tripod | 45% (90% of grip) → 43% of belt, tripod | 50% → 51% of belt |
| 0.8 | 65% (81% of grip) → 72% of belt, tripod | 70% (87% of grip) → 78% of belt, tripod | 80% → 88% of belt |
| 1.2 | 95% (79% of grip) → 95% of belt, tripod | 100% (83% of grip) → 96% of belt, wave | 120% → 98% of belt |

## At the steepest grade each walked

| Grip | Control | Grade | Downhill drift | Margin | Peak joint torque (coxa / femur / tibia) |
|---|---|---|---|---|---|
| 0.5 | position | 45% | 10 cm | 14 cm | 7 / 23 / 22 N m |
| 0.5 | force | 45% | 3 cm | 14 cm | 15 / 34 / 32 N m |
| 0.8 | position | 65% | 6 cm | 13 cm | 10 / 21 / 24 N m |
| 0.8 | force | 70% | 4 cm | 14 cm | 35 / 57 / 50 N m |
| 1.2 | position | 95% | 9 cm | 13 cm | 10 / 19 / 25 N m |
| 1.2 | force | 100% | 7 cm | 2 cm | 41 / 43 / 49 N m |
