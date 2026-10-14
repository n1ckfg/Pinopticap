"""Compare the white spheres Pinopticap drew in its (default, face-on) 3D view with where the
simulator's true points should appear.  usage: check_shot.py shot.png rig.json rotated(0|1) <sim args>"""
import json, math, random, sys
sys.dont_write_bytecode = True
import numpy as np, cv2
shot, rig_path, rotated, sim_dir = sys.argv[1], sys.argv[2], sys.argv[3] == "1", sys.argv[4]
sys.path.insert(0, sim_dir)
import camera_sim as cs
from rotate_rig import RW
args = cs.parse_args(sys.argv[5:])
scene = cs.Scene(args.scene, args.size, args.points, args.spin, random.Random(args.seed))
truth = np.array(scene.points_at(0.0))
if rotated:
    truth = truth @ RW.T

rec = json.load(open(rig_path))[0]
reach = 0.0
for s in rec["shots"].values():
    R = cv2.Rodrigues(np.array(s["rotation"], dtype=float))[0]
    reach = max(reach, float(np.linalg.norm(-R.T @ np.array(s["translation"]))))
scale = 100.0 / reach                       # ofApp::setup: farthest eye halfway out on the grid
W, H = 960, 720
d = (H / 2) / math.tan(math.radians(30))    # ofEasyCam default: 60 degree fov, image plane at the origin
view = truth * scale
expected = np.stack([W / 2 + view[:, 0] * d / (d - view[:, 2]), H / 2 - view[:, 1] * d / (d - view[:, 2])], axis=1)

img = cv2.imread(shot)[:H, :W]
white = img.min(axis=2) >= 250
ys, xs = np.nonzero(white)

def score(points, name):
    """A sphere is a white disc about 5 px in radius: look for one at each expected position."""
    found, worst = 0, 0.0
    for e in points:
        near = (xs - e[0]) ** 2 + (ys - e[1]) ** 2 <= 6.0 ** 2
        if near.sum() < 45:
            continue
        offset = math.hypot(xs[near].mean() + 0.5 - e[0], ys[near].mean() + 0.5 - e[1])
        if offset <= 2.5:
            found += 1
            worst = max(worst, offset)
    print("  %-46s %2d of %d spheres found, worst offset %.2f px = %.4f scene units"
          % (name, found, len(points), worst, worst / scale))

def project(points):
    v = points * scale
    return np.stack([W / 2 + v[:, 0] * d / (d - v[:, 2]), H / 2 - v[:, 1] * d / (d - v[:, 2])], axis=1)

print("scale %.3f view units per scene unit, %d true points" % (scale, len(truth)))
score(project(truth), "true points")
score(project(truth * [1, 1, -1]), "control: depth mirrored")
score(project(truth * [-1, 1, 1]), "control: x mirrored")
score(project(truth @ RW.T), "control: world turned 90 degrees")
