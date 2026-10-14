"""Drive Pinopticap with the four nfg-rpi-3-* cameras of a real OpenSfM file, using the
simulator's own projection, detection and streaming code. usage: real_rig_sim.py REC.json [camera_sim options]"""
import json, os, random, sys, time
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "camera_sim"))
import camera_sim as cs

rec = json.load(open(sys.argv[1]))[0]
args = cs.parse_args(sys.argv[2:])
rng = random.Random(args.seed)
cams = []
for key, shot in sorted(rec["shots"].items()):
    host = key.split("_")[0]
    if not host.startswith("nfg-rpi-3-"): continue
    m = rec["cameras"][shot["camera"]]
    R = cs.rvec_to_rotation(tuple(shot["rotation"]))
    c = cs.scale(cs.matvec(cs.transpose(R), tuple(shot["translation"])), -1.0)
    cam = cs.Camera(host, "SIM_%s%03d" % (time.strftime("%y%m%d%H%M%S"), len(cams)), c, cs.sub(c, cs.scale(R[2], -1.0)),
                    m["width"], m["height"], m["focal"], m["k1"], m["k2"])
    cam.R = R
    cam.phase = rng.random() / args.fps if args.unsync else 0.0
    cams.append(cam)

# centre the scene on the sparse points that all four cameras see
def inside(cam, p):
    uv = cam.project(p)
    return uv is not None and 0 <= uv[0] < cam.w and 0 <= uv[1] < cam.h
common = [tuple(p["coordinates"]) for p in rec["points"].values() if all(inside(cam, tuple(p["coordinates"])) for cam in cams)]
centre = tuple(sum(p[i] for p in common) / len(common) for i in range(3))

class OffsetScene(cs.Scene):
    def points_at(self, t):
        return [tuple(p[i] + centre[i] for i in range(3)) for p in cs.Scene.points_at(self, t)]

scene = OffsetScene(args.scene, args.size, args.points, args.spin, rng)
for cam in cams: cam.perm = list(range(len(scene.base)))
seen = [sum(inside(cam, p) for p in scene.points_at(t / 10.0)) for cam in cams for t in range(120)]
print("real rig: %d cameras %s; scene centre %s, %.2f from the cameras; points in view per camera frame: min %d of %d"
      % (len(cams), [c.host for c in cams], tuple(round(v, 2) for v in centre),
         sum(cs.norm(cs.sub(centre, cam.c)) for cam in cams) / len(cams), min(seen), len(scene.base)), file=sys.stderr)
sys.exit(cs.stream(cams, scene, args, random.Random(args.seed + 1)))
