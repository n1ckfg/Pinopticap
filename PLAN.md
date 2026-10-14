# Pinopticap plan

Last updated 13 October 2026. This records where the work stands, so that a new session can continue without the earlier conversation.

## Goal

Pinopticap receives 2D blob positions from camera nodes over OSC. The aim is to turn the blobs seen by any number of calibrated cameras into 3D points in real time and pass those points on over OSC. The hardware rig has four cameras.

## Where things stand

Everything requested so far is done, and no further task has been set. All results come from simulation: no camera node has been connected yet.

| Part | State | Where |
| :--- | :--- | :--- |
| Build | Works: `make -j3` here exits 0 | Needs the ofxCv fix listed below |
| Camera simulator | Done | `tools/camera_sim/` |
| Calibration loader and N-view solver | Done, with leave-one-out rejection of bad rays | `src/Triangulation.hpp` |
| 3D points in the app | Done | `ofApp::triangulateBlobs()` and `ofApp::drawWorld()`; the spacebar hides the camera tiles |
| 3D points over OSC | Done | `/blob` `ssifffi` to `osc_host`:`osc_send_port` (7114) |
| Crash in `Eye::addBlob` | Fixed | `src/Eye.cpp` |
| Settings | `calibration_file` and `max_ray_angle` added | `bin/data/settings.xml` |
| Solver tests and live-run scripts | Done; all five solver tests pass | `tools/solver_tests/` |

Four documents hold the detail:

- `ARCHITECTURE.md`: the design. Section 3.4 covers the solver.
- `REPORT.md`: measured results of the leave-one-out change, including the four-camera numbers.
- `tools/camera_sim/README.md`: the simulator, the OSC messages and the calibration file layout.
- `tools/solver_tests/README.md`: how to run the tests and what each one checks.

## Nothing is committed

Three repositories have uncommitted changes. Commit only when the user asks.

- **Pinopticap** (this directory, branch `dev`)
  - Modified: `ARCHITECTURE.md`, `bin/data/settings.xml`, `src/Eye.cpp`, `src/Eye.h`, `src/ofApp.cpp`, `src/ofApp.h`, `tools/camera_sim/README.md`, `tools/camera_sim/camera_sim.py`.
  - New: `src/Triangulation.hpp`, `REPORT.md`, `PLAN.md`, `tools/solver_tests/`.
- **ofxCv** (`../../../addons/ofxCv`, the fork `n1ckfg/ofxCv`, branch `of_0.12.1`)
  - One line in `libs/ofxCv/include/ofxCv/Tracker.h`: `Tracker<T>()` became `Tracker()`.
  - Without it Pinopticap does not compile with GCC 12 and `-std=c++20`. `setup.sh` clones this fork, so a fresh setup fails to build until the fix is pushed there.
- **Pinopticon** (`../..`, branch `master`)
  - Shows `core/Pinopticap` as modified. The submodule pointer needs updating once Pinopticap is committed.

## What the app needs in order to give 3D points

- **A calibration file.** At startup the app reads `bin/data/reconstruction.json` (the `calibration_file` setting), an OpenSfM reconstruction. Each shot must be named `<hostName>_<anything>.jpg`, with the host name that node sends in `/blob`. There is no such file in `bin/data` now, so the app prints `Calibration: 0 eyes` and gives no 3D points.
- **Blob positions from 0 to 1.** x and y must be fractions of the image width and height.
- **Matching blob indices.** Blobs are grouped by the index each camera reports. A point needs at least two cameras, and more than half of those reporting that index, to agree within `max_ray_angle` (1°).
- **A reachable `osc_host`.** The shipped value, `Example.local`, does not resolve, and then nothing is sent.

## How to test

There are three levels, and `tools/solver_tests/README.md` has the commands for the last two.

- **Simulator:** `python3 tools/camera_sim/camera_sim.py --selftest --cameras 4` checks the simulator and its Python reference solver. It does not exercise the C++ solver or the app.
- **Solver:** `tools/solver_tests/run.sh` builds the tests of `src/Triangulation.hpp` without openFrameworks, runs them and compares the results with stored ones. It takes about a minute the first time, and all five tests pass. `run.sh compare` and `run.sh timing` reproduce the comparison and timing figures in `REPORT.md`.
- **App:** the scripts in `tools/solver_tests/live/` run the built app against the simulator and compare the 3D points it sends with the true positions.

### What a live run involves on this Pi

`tools/solver_tests/live/sim_rig.sh` does all of this. The steps are listed because each one works around something:

1. Copy `bin/Pinopticap` and `bin/data/settings.xml` into a scratch `bin/` and `bin/data/`. openFrameworks reads `data/` next to the real executable, so a symlink does not work.
2. In the copied settings, set `osc_host` to `127.0.0.1` and `debug` to `0`. With drawing on, software rendering holds the app to about 5 updates per second.
3. Start a display: `Xvfb :99 -screen 0 1280x1024x24 -nolock -ac -listen tcp -nolisten unix &`. Plain `Xvfb :99` and `xvfb-run` fail inside the Claude Code sandbox.
4. Start a listener on UDP 7114 (the app's output) and on a second port for the true positions.
5. Start the simulator first, because it writes the calibration the app reads at startup.
6. About 1.5 s later, start the app from the scratch `bin/` with `DISPLAY=127.0.0.1:99`.
7. Stop it with `pkill -x Pinopticap` and fall back to `pkill -9 -x Pinopticap`. The app sometimes ignores SIGTERM, and it segfaults on every exit (code 139).

Expected with four simulated cameras (cube edge 1, cameras 3 units away):

- **Still cube:** all 8 points on every update, each within 1e-7 of the truth.
- **Spinning cube, 1 px noise, unsynchronised shutters, 5% dropout:** all 8 points on every update, median error about 0.008, maximum about 0.03.
- **Update rate:** the app sends only on updates where a camera reported something new, so 30 fps cameras give 30 to 60 output updates per second.

## Next steps

None of these has been agreed with the user. They are proposed in the order that stands between the simulation and the real four-camera rig.

1. **Commit the work**, in the three repositories above, when the user says so.
2. **Get a calibration file for the real rig** into `bin/data/reconstruction.json`. The Pinopticon repo holds two reconstructions of four `nfg-rpi-3-*` cameras, and whether either is the current rig is an open question:
   - `experimental/pinopticap_ray_test/reconstruction.json`, which also has a fifth camera, `nfg-rpi-0-1`. Every result on real calibration so far used this file.
   - `tools/calibration_samples/opensfm.json`, a different reconstruction that has not been tested.
3. **Convert blob positions to 0 to 1.** The real nodes (Pinopticam, Pinoptigrab) send pixels. With pixels nothing is triangulated and the 2D dots land outside the tiles. Either the nodes divide by the image size or Pinopticap does.
4. **Match blobs across cameras.** The real nodes send the loop counter over the contour finder as the index, which means nothing from one camera to the next. In simulation, with four cameras and indices in image order, one or two blobs in view all came through, three gave 62 to 87% of the points, four gave 56 to 76%, and the eight corners of the cube gave 23%. More than two blobs need a matching step that does not rely on the index; none is designed yet.
5. **Run on the hardware** and compare with the simulated numbers in `REPORT.md`.

## Known problems, not addressed

- **Exit:** the app segfaults on every exit and sometimes hangs on SIGTERM.
- **New nodes:** the app opens a WebSocket to `<hostName>.local:7112` for each new node, and stalls for about 5 s per node whose name does not resolve.
- **Own host name:** the 3D points go out under the host name `unknown` on this Pi. `ofApp::setup()` reads the name only when `TARGET_LINUX_ARM` is defined, which the 64-bit build does not do.
- **3D view scale:** the view is scaled by how far the cameras are from the origin. On a rig whose cameras sit close together and look outward, like the sample calibration, the points fall outside the view until it is zoomed out.
- **Solver limits:** a camera turned by 1° or less after calibration is never left out, so its error reaches the points. With three cameras and meaningless indices, false points are more frequent than before the leave-one-out change. `REPORT.md` has the numbers.

## Open questions for the user

1. Is either reconstruction in the Pinopticon repo (see next step 2) the calibration of the current four-camera rig? If not, the accuracy numbers on real calibration need rerunning on the current one, with `REAL=file tools/solver_tests/run.sh`.
2. Should pixel positions be converted in the nodes or in Pinopticap?
