# camera_sim

A simulated camera array for testing Pinopticap without hardware. It projects one shared 3D scene through any number of virtual cameras and sends each camera's detections as OSC `/blob` messages, the way Pinopticam and Pinoptigrab nodes do. Because every camera looks at the same scene and the camera poses are known, the output can be triangulated back to 3D and compared with the truth.

Python 3, standard library only.

## Quick start

```sh
python3 camera_sim.py --cameras 9                  # 9 cameras around a spinning cube, to 127.0.0.1:7110
python3 camera_sim.py --cameras 9 --host 10.0.0.5  # Pinopticap on another machine
python3 camera_sim.py --help                       # every option, with defaults
```

Each camera appears in Pinopticap as its own node (`cam00.sim`, `cam01.sim`, ...) showing the cube's eight vertices from its own viewpoint.

To see the cube rebuilt in 3D, give Pinopticap the cameras' calibration. It reads `bin/data/reconstruction.json` when it starts, so write the file first:

```sh
python3 camera_sim.py --cameras 9 --write-rig ../../bin/data/reconstruction.json
```

Then start Pinopticap. The overlay reports `8 blobs in 3D`; the spacebar hides the camera tiles so the 3D view behind them shows, with each calibrated camera drawn as the corners of its image.

## What is sent

One datagram per blob, each a single message wrapped in a bundle. (`ofxOscSender` in openFrameworks 0.12.1 sends the bare message instead; `ofxOscReceiver` takes either.)

| Address | Type tag | Arguments |
| :--- | :--- | :--- |
| `/blob` | `ssiffi` | hostName, sessionId, blobIndex, x, y, timestamp |
| `/truth` | `ssifffi` | `"truth"`, `"SIM_truth"`, pointId, x, y, z, timestamp |

`/truth` carries the true 3D position of every scene point once per frame. It is only sent with `--truth-port`, and goes to that port rather than to Pinopticap. The timestamp is epoch milliseconds cut to 32 bits, one clock for every simulated camera. Real nodes send `(int) ofGetSystemTimeMillis()`, which on 64-bit Linux counts from boot, so their timestamps cannot be compared from one node to the next.

A point is only sent by cameras that can see it: it must be in front of the camera and inside the image.

## Options that change what a receiver has to cope with

| Option | Effect |
| :--- | :--- |
| `--layout ring\|dome\|random` | Cameras on a circle, on a hemisphere, or scattered at mixed distances with partly overlapping views. |
| `--scene cube\|cloud\|point` | Eight cube vertices, `--points N` random points, or a single point. The scene spins rigidly at `--spin` degrees per second. |
| `--noise PX` | Gaussian noise on every detection, in pixels. |
| `--dropout P` | Chance that a visible point is missed in a frame. |
| `--unsync` | Every camera gets its own shutter phase, so no two cameras see the scene at the same instant. |
| `--labels consistent` | blobIndex is the scene point id, the same in every camera. |
| `--labels shuffled` | Each camera has its own fixed numbering of the points. |
| `--labels raster` | blobIndex is the blob's position in that frame's list, bottom of the image first. This is what the real nodes send (the loop counter over the contour finder), so an index means nothing across cameras or from one frame to the next. |
| `--coords normalized\|pixels` | x and y as fractions of the image (what Pinopticap draws) or as pixels (what the real nodes currently send). |
| `--k1`, `--k2` | Radial lens distortion. |

The rig depends only on the rig options and `--seed`, so the same command line always gives the same cameras.

## Camera model and calibration file

`--write-rig FILE` writes the camera calibration in the layout of an OpenSfM `reconstruction.json`, the same layout `experimental/pinopticap_ray_test` reads:

- `cameras[model]`: `width`, `height`, `focal` (in units of the larger image side), `k1`, `k2`.
- `shots["<hostName>_0.jpg"]`: `rotation` (axis-angle vector, world to camera) and `translation`. The camera centre is `-Rᵀ·t`.
- `points`: the scene points at time 0.

The world is right-handed with Y up and the scene centred on the origin. Camera axes are x right, y down, z forward. A world point `X` lands at pixel `(u, v)`:

```
p = R·X + t
x = p.x / p.z        y = p.y / p.z        r² = x² + y²
s = focal · max(width, height) · (1 + k1·r² + k2·r⁴)
u = width / 2 + s·x  v = height / 2 + s·y
```

With `--coords normalized` the wire values are `u / width` and `v / height`.

## Self test

```sh
python3 camera_sim.py --selftest
python3 camera_sim.py --selftest --cameras 12 --layout random --noise 1
```

Sends nothing. It rebuilds each camera from the calibration file, turns the wire values back into rays, triangulates every point seen by at least two cameras and reports the 3D error against the true scene. With exact input it must reproduce the scene and exits non-zero if it does not; with noise, dropout or unsynchronised shutters it reports how large the error becomes. Detections are paired across cameras by their true point id, so this measures triangulation, not matching.

`View.ray()` and `triangulate()` in `camera_sim.py` are the reference for a receiver: calibration plus wire values to rays, and rays to a 3D point.

## Capacity

On a Raspberry Pi 4 the simulator keeps pace up to roughly 30,000 blobs per second, for example 128 cameras of 8 points at 30 fps. The status line prints `lag`, how far behind schedule the slowest frame of the last second was; a lag that keeps growing means it is overloaded.

## Driving Pinopticap as it stands (October 2026)

- **Host names.** Pinopticap opens a WebSocket to `<hostName>.local:7112` for every new node and waits for the name to resolve. A made-up single-label name such as `simcam00` stalls it for the mDNS timeout, 5 seconds per camera here. The default names (`cam00.sim`) have two labels, which the resolver rejects at once. If registration is still slow on your system, map the names to `127.0.0.1` in `/etc/hosts`.
- **3D needs matching blob indices.** Pinopticap triangulates the blobs that calibrated cameras report under the same index, so only `--labels consistent` rebuilds the scene. With `raster` or `shuffled` labels a point appears only where more than half of the cameras happen to agree, which is rare.
- **3D output.** Pinopticap sends every triangulated point to `osc_host`:`osc_send_port` (7114) as `/blob` `ssifffi`, under its own hostName, whenever a camera has reported something new. `/truth` has the same arguments, so one listener can compare the two. The shipped `osc_host`, `Example.local`, does not resolve, and then nothing is sent; set it to the receiver's address in `bin/data/settings.xml`.
- **Pixels.** With `--coords pixels` the dots land far outside the tiles, because Pinopticap multiplies x and y by the tile size, and nothing is triangulated.
