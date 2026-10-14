# solver_tests

Tests for the 3D solver in `src/Triangulation.hpp` and for the 3D points the running app sends. They were written as working tools while the solver was developed, and are kept here so that later changes can be checked against the same cases.

`REPORT.md` in the Pinopticap root gives the results these tests produced for the current solver.

## Solver tests

```sh
./run.sh            # the five tests with a known result
./run.sh compare    # also the old rule, the prototype and the header side by side
./run.sh timing     # also the time per group of rays
./run.sh all
```

`run.sh` builds each test without openFrameworks, makes the test input with the camera simulator, runs the tests and compares what they print with `expected/`. Everything it makes goes into `build/`, which git ignores. The first run takes about a minute on a Raspberry Pi 4. `compare` takes several minutes more and writes about 150 MB of test input.

It needs:

- g++ with C++20.
- Python 3 with numpy and OpenCV (`cv2`), for the two generators that use the real calibration.
- openFrameworks with the ofxJSON addon, for glm and jsoncpp. `OF_ROOT=dir` overrides the default of three levels above Pinopticap.
- The Pinopticon repo around Pinopticap, for the real calibration at `experimental/pinopticap_ray_test/reconstruction.json`. `REAL=file` overrides it.

### What each test checks

| Result | Program | Input from | What it checks |
| :--- | :--- | :--- | :--- |
| `tri_data` | `tri_test` | `gen_vectors.py` | The header against the Python reference in `camera_sim.py`, on five simulated rigs of 6 to 16 cameras, and against OpenCV's projection of the real calibration's own points. |
| `tri_data4` | `tri_test` | `gen4.py` | Four cameras on ten simulated rigs. One camera reporting another blob, with four or three cameras seeing the point; two of four wrong; two unrelated rays; one camera turned 0° to 5° after calibration. |
| `labels_data` | `labels_test` | `gen_labels.py` | 3, 9 and 24 cameras whose blob indices do or do not match: how many real and false points come out. |
| `labels_data4` | `labels4_test` | `gen4.py` | The same for four cameras, with one to 32 points in view. |
| `real4` | `real4_test` | `gen_real4.py` | The four `nfg-rpi-3-*` cameras of the real calibration: right, missing and wrong points with pixel noise and with one camera reporting another blob, and the error along and across the viewing direction. The right-hand columns give the prototype. |

`tri_test` exits non-zero when a strict case fails. The strict cases are the ones without an angle limit, one of four cameras reporting another blob, and the hand-made degenerate cases at the end. The other groups and the other three programs only report numbers.

A result that differs from `expected/` is not always an error:

- **After a change to the solver**, read the diff. If the change is intended, copy `build/out/<name>.txt` to `expected/<name>.txt`.
- **On another machine**, `real4` can differ slightly, because a different numpy version may draw different noise.

### Comparison and timing

- **`compare4`**: the header against the first leave-one-out prototype, on the four-camera input, with the mean time per group.
- **`multi_test`**: 3 to 48 cameras with a chosen number reporting another blob, around the scene and side by side. It prints the share of right and wrong points and the time per group for four solvers: the old rule (`greedy.hpp`), the prototype (`loo_proto.hpp`), a scratch version of the current rule (`cand4.hpp`) and the header. `multi_test <dir> <repeats> <distance that counts as wrong>`.
- **`cand.hpp`, `cand2.hpp`, `cand3.hpp`**: variants that were tried and set aside. `cand4.hpp` uses parts of them, so `multi_test` needs all four to compile.
- **`time_labels`**: time per group, old rule against the header, on the label tests' input.
- **`tri_bench`**: the comparison of triangulation methods made when the solver was chosen. It has its own copy of the solver and needs the OpenCV development files.

`shim/` holds stand-ins for `ofMain.h` and `ofxJSONElement.h`, with just enough for the header to compile on its own.

## Live runs

The scripts in `live/` start the built app (`bin/Pinopticap`) on a virtual display, feed it from the camera simulator, record the 3D points it sends to port 7114 and compare them with the true positions.

They need Xvfb, ImageMagick (`import`), xdotool and xwininfo.

Each script:

- kills any Pinopticap and Xvfb that are already running;
- deletes and remakes `<work dir>/run`, where it puts a copy of `bin/`;
- leaves its logs and screenshots in the work dir.

```sh
E='s#<osc_host>.*</osc_host>#<osc_host>127.0.0.1</osc_host>#; s#<debug>1</debug>#<debug>0</debug>#'
W=/tmp/pinopticap_live

# four simulated cameras, still cube
live/sim_rig.sh $W static "$E" 1 --cameras 4 --spin 0

# spinning cube, noise, unsynchronised shutters, dropout
live/sim_rig.sh $W spin "$E" 1 --cameras 4 --unsync --noise 1 --dropout 0.05

# one camera turned 3 degrees after calibration
RIG_EDIT="bump cam00.sim 3" live/sim_rig.sh $W turned "$E" 1 --cameras 4 --unsync --noise 1

# the four cameras of the real calibration
live/real_rig.sh $W real "$E" ../../../../experimental/pinopticap_ray_test/reconstruction.json --unsync --noise 1 --dropout 0.05

# the 3D view: screenshots, then a check of where the points were drawn
ARGS="--cameras 9 --layout dome --scene cloud --points 8 --size 2.4 --spin 0 --seed 5"
live/view_check.sh $W view 1 $ARGS
python3 live/check_shot.py $W/shot_view_3d.png $W/rig_view.json 0 ../camera_sim $ARGS
```

The third argument of `sim_rig.sh` and `real_rig.sh` is a sed script applied to the copy of `settings.xml`. The one above points the output at this machine and turns drawing off, because software rendering otherwise holds the app to about 5 updates per second. `view_check.sh` leaves the settings as they are, since it needs the drawing.

| File | Purpose |
| :--- | :--- |
| `sim_rig.sh` | A run on a simulated rig. `DUR=seconds` sets its length; `RIG_EDIT` changes the calibration the app reads. |
| `real_rig.sh` | A run on the four `nfg-rpi-3-*` cameras of a real OpenSfM file, with screenshots. |
| `view_check.sh` | Screenshots of the camera tiles and of the 3D view, for `check_shot.py`. |
| `osc_listen.py` | The listener: counts updates and points, and measures the error against the simulator's `/truth`. |
| `real_rig_sim.py` | Drives the app from a real calibration with the simulator's projection and streaming code. |
| `rig_edit.py` | Turns one camera in a calibration file, or removes it. |
| `rotate_rig.py` | Turns the world frame of a calibration file by 90°. |
| `check_shot.py` | Compares the spheres in a 3D screenshot with where the true points should be drawn. |

The app segfaults on every exit and sometimes ignores SIGTERM. The scripts allow for both, so `APP EXIT CODE 139` in their output is expected.
