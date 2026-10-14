# Leave-one-out rejection in Triangulation.hpp

13 October 2026

`intersectRays` in `src/Triangulation.hpp` now uses leave-one-out rejection, and Pinopticap builds cleanly with it. With four cameras, one camera reporting the wrong blob no longer loses the point or moves it. Everything below was measured in simulation; no camera nodes were used.

## What it does

Each camera that reports a blob under an index gives one ray. When the rays under one index do not meet within the angle limit (`max_ray_angle`, 1° by default), one ray is left out and the rest are fitted again.

- **Old rule:** leave out the ray that misses the fitted point by most.
- **New rule:** try leaving out each ray in turn, refit the others, and leave out the one whose removal gives the smallest total angle error.
- **Size limit:** the new rule runs while 16 or fewer rays are in play; above that the old rule applies. The limit is a judgement, set because the cost of each rejection grows with the square of the ray count.

As before, a point needs at least two rays, and more than half of those reported, to agree.

## Difference from the first prototype

The prototype tried during the four-camera tests left out the ray whose removal gave the smallest *worst* angle error. It was not used, because it did worse than the old rule on rigs of five or more cameras. Share of cases that gave the right point with two bad rays among eight:

| Rig | Old rule | Prototype | New rule |
| :--- | :--- | :--- | :--- |
| 8 cameras around the scene | 100% | 69% | 100% |
| 8 cameras side by side | 34% | 92% | 97% |

From 3 to 48 cameras the new rule matched or beat the old one in every combination tested but two: 16 cameras around the scene with 5 or 6 bad rays (99.7% against 99.9%, and 98.1% against 98.5%).

## Four cameras, before and after

| Case | Before | After |
| :--- | :--- | :--- |
| One of four cameras reports another blob, six simulated rigs | right in 55–100% of cases | right in 100% on all six |
| Same, real calibration, 1 px noise | 309 of 813 right, 504 no point | 813 of 813 right |
| Only three cameras see the point and one reports another blob, real calibration, 1 px noise | 221 right, 517 no point, 75 wrong | 806 right, 7 wrong |
| One camera turned 5° after calibration | 361 of 480 right | 480 of 480 right |
| Live app, one camera turned 3° | 23% of messages more than 0.1 units off; fewer than 8 points on most updates | all 8 points on every update; at most 9 of about 3,000 messages more than 0.1 units off per run |

The real calibration is the four `nfg-rpi-3-*` cameras in `experimental/pinopticap_ray_test/reconstruction.json` of the Pinopticon repo. Its scene is about 5.5 units from the cameras, and a result more than 0.5 units from the true position counts as wrong.

- **Input with no bad rays:** results are unchanged with up to 2 px of noise. At 4 px, 1,070 of 1,138 points were right on the real calibration, against 1,072 before. The live run on the real calibration matches the earlier one (median error 0.038 units).
- **Small turns:** a camera turned by 1° or less after calibration is never left out, so its error still reaches the points. At 1.5° it is left out for a quarter of the points, at 2° for 60% and at 5° for all of them.

## Costs and downsides

- **Time:** on the Raspberry Pi 4, a four-ray group that needs a ray left out takes about 1.6 µs instead of 0.4 µs; groups that need none cost the same as before. A group of meaningless indices takes about 18 µs at 9 cameras and 61 µs at 24.
- **Three cameras with meaningless indices:** false points rose from 50 to 85 of 480 groups, because any two of three count as a majority and all three pairs are now tried.
- **Two unrelated rays:** slightly more pass as a point (45 instead of 37 of 480 on one simulated rig), because the angle limit is now checked only on the final weighted fit.

## Files changed

- `src/Triangulation.hpp`: `fitRays` is new and `intersectRays` is rewritten around it. Its signature is unchanged, so `ofApp.cpp` needed no change.
- `ARCHITECTURE.md`, section 3.4: the description of the solver.

The tests behind these numbers are in `tools/solver_tests/`.
