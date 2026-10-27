# Comb sweep results

240 runs: 60 settings x 4 branches (seeds). Each branch has 8 nodes and ~73 fruit. Fruit dropped before the comb arrived: 0.

Cells read **ripe+overripe taken / green knocked off / ripe+overripe share of what was taken**.
Rates pool all fruit across seeds. "Perfect cap" is the analytic best case (`coffeesim/selectivity.py`).

## Pull-off force spread CV 0.15

| Rating per fruit | 12 tines, 0.1 m/s | 12 tines, 0.3 m/s | 24 tines, 0.1 m/s | 24 tines, 0.3 m/s | Perfect cap |
|---|---|---|---|---|---|
| 2.5 N | 46% / 0% / 100% | 49% / 5% / 89% | 41% / 0% / 96% | 51% / 7% / 82% | 21% / 0% / 100% |
| 3.5 N | 83% / 4% / 90% | 81% / 5% / 89% | 81% / 1% / 93% | 85% / 9% / 87% | 74% / 0% / 100% |
| 4.5 N | 97% / 11% / 86% | 95% / 13% / 85% | 93% / 13% / 82% | 94% / 24% / 77% | 99% / 0% / 98% |
| 5.5 N | 99% / 22% / 79% | 99% / 29% / 77% | 98% / 25% / 77% | 99% / 38% / 72% | 100% / 0% / 88% |
| 7 N | 100% / 62% / 65% | 100% / 55% / 67% | 100% / 59% / 66% | 100% / 78% / 62% | 100% / 7% / 77% |

## Pull-off force spread CV 0.30

| Rating per fruit | 12 tines, 0.1 m/s | 12 tines, 0.3 m/s | 24 tines, 0.1 m/s | 24 tines, 0.3 m/s | Perfect cap |
|---|---|---|---|---|---|
| 2.5 N | 55% / 2% / 93% | 59% / 5% / 88% | 52% / 2% / 93% | 58% / 12% / 82% | 35% / 0% / 100% |
| 3.5 N | 80% / 6% / 89% | 81% / 14% / 83% | 77% / 8% / 86% | 80% / 15% / 81% | 70% / 0% / 97% |
| 4.5 N | 93% / 19% / 81% | 90% / 19% / 81% | 94% / 18% / 81% | 94% / 32% / 75% | 91% / 2% / 92% |
| 5.5 N | 96% / 36% / 73% | 96% / 39% / 74% | 96% / 31% / 74% | 97% / 42% / 72% | 98% / 7% / 84% |
| 7 N | 100% / 59% / 67% | 99% / 56% / 68% | 99% / 62% / 65% | 99% / 65% / 65% | 100% / 26% / 73% |

## Pull-off force spread CV 0.45

| Rating per fruit | 12 tines, 0.1 m/s | 12 tines, 0.3 m/s | 24 tines, 0.1 m/s | 24 tines, 0.3 m/s | Perfect cap |
|---|---|---|---|---|---|
| 2.5 N | 58% / 6% / 87% | 64% / 13% / 83% | 57% / 9% / 84% | 64% / 18% / 78% | 44% / 0% / 97% |
| 3.5 N | 82% / 13% / 84% | 77% / 19% / 80% | 78% / 15% / 81% | 78% / 35% / 72% | 70% / 3% / 91% |
| 4.5 N | 92% / 24% / 79% | 89% / 32% / 76% | 90% / 31% / 76% | 93% / 33% / 74% | 86% / 9% / 85% |
| 5.5 N | 94% / 41% / 72% | 95% / 39% / 74% | 94% / 36% / 73% | 95% / 45% / 70% | 93% / 19% / 78% |
| 7 N | 99% / 55% / 67% | 98% / 61% / 67% | 98% / 67% / 64% | 98% / 65% / 65% | 98% / 38% / 70% |

## Best setting with green loss at or under 10%

| CV | Setting | Taken | Green lost | Semi taken | Purity |
|---|---|---|---|---|---|
| 0.15 | 3.5 N, 24 tines, 0.3 m/s | 85% | 9% | 37% | 87% |
| 0.30 | 3.5 N, 12 tines, 0.1 m/s | 80% | 6% | 34% | 89% |
| 0.45 | 2.5 N, 12 tines, 0.1 m/s | 58% | 6% | 26% | 87% |

## Throughput at the CV 0.30 pick

| Picker | Fruit/s | kg cherry/h | Harvest purity |
|---|---|---|---|
| Person, selective (Colombia) | 1.87 | 10.3 | high |
| One single-fruit arm (7.53 s/fruit) | 0.13 | 0.7 | high |
| One comb arm (this sweep, 4 s reposition, 70% duty) | 2.89 | 15.9 | 89% |
| Brudden DSC-18 shaker (Puerto Rico field) | | 26.9 | 75% |
