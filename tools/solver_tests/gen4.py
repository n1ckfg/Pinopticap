"""Scratch: four-camera test data for Pinopticap/src/Triangulation.hpp.
vectors.txt for tri_test (solver against the Python reference, mislabelled and bumped cameras),
labels.txt for labels4_test (what index-grouped triangulation yields per labelling)."""
import json, math, random, struct, sys
sys.dont_write_bytecode = True
sim_dir, out = sys.argv[1], sys.argv[2]
sys.path.insert(0, sim_dir)
import camera_sim as cs

f32 = lambda v: struct.unpack(">f", struct.pack(">f", v))[0]
V, L = [], []
emit = lambda *a: V.append(" ".join(repr(float(x)) if isinstance(x, float) else str(x) for x in a))
mm = lambda a, b: tuple(tuple(sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)) for i in range(3))

def make(argv):
    args = cs.parse_args(argv)
    rig_rng = random.Random(args.seed)
    scene = cs.Scene(args.scene, args.size, args.points, args.spin, rig_rng)
    cams = cs.build_rig(args, len(scene.base), rig_rng)
    return args, scene, cams

def write_rig(name, rig):
    json.dump(rig, open(out + "/" + name + ".json", "w"))
    return name + ".json"

def bumped(rig, host, deg, axis=(0.0, 1.0, 0.0)):
    """The calibration as it would read if camera `host` had been turned about its own centre."""
    rig = json.loads(json.dumps(rig))
    key = next(k for k in rig[0]["shots"] if k.split("_")[0] == host)
    shot = rig[0]["shots"][key]
    D = cs.rvec_to_rotation(cs.scale(axis, math.radians(deg)))
    shot["rotation"] = list(cs.rotation_to_rvec(mm(D, cs.rvec_to_rotation(tuple(shot["rotation"])))))
    shot["translation"] = list(cs.matvec(D, tuple(shot["translation"])))
    return rig

def frames_of(args, scene, cams, frames, rng):
    """Per frame: {pid: [(host, x, y, t)]} from consistent labels."""
    for frame in range(frames):
        seen = {}
        for cam in cams:
            t = frame / args.fps + cam.phase
            for _, x, y, pid in cs.detect(cam, scene.points_at(t), args, rng):
                seen.setdefault(pid, []).append((cam.host, f32(x), f32(y), t))
        yield seen

def sim_group(name, argv, frames, exact_tol):
    args, scene, cams = make(argv)
    rig = cs.rig_to_opensfm(cams, scene)
    rig_file = write_rig(name, rig)
    views = {k.split("_")[0]: cs.View(s, rig[0]["cameras"][s["camera"]]) for k, s in rig[0]["shots"].items()}
    emit("rig", rig_file)
    for cam in cams: emit("center", cam.host, *cam.c)
    exact = args.noise == 0 and not args.unsync and not args.dropout
    n = len(scene.base)
    for seen in frames_of(args, scene, cams, frames, random.Random(args.seed + 1)):
        at = {(h, pid): (x, y) for pid, obs in seen.items() for h, x, y, _ in obs}
        for pid, obs in seen.items():
            if len(obs) < 2: continue
            rays = [views[h].ray(x, y, True) for h, x, y, _ in obs]
            ref = cs.triangulate(rays, 1)
            if ref is None: continue
            truth = scene.points_at(sum(o[3] for o in obs) / len(obs))[pid]
            emit("group", name + "/ungated")
            for h, x, y, _ in obs: emit("ray", h, x, y)
            emit("expect", 80.0, len(obs), exact_tol, *ref, *truth)
            emit("group", name + "/1deg")
            for h, x, y, _ in obs: emit("ray", h, x, y)
            emit("expect", 1.0, -1, -1.0, *ref, *truth)
            if not exact or len(obs) != 4: continue
            wrong = lambda i, step: at.get((obs[i][0], (pid + step) % n))   # camera i's sighting of another point
            w0, w1 = wrong(0, 1), wrong(1, 3)
            if w0:
                # one camera of four reports another point under this index
                emit("group", name + "/1_of_4_mislabelled")
                emit("ray", obs[0][0], *w0)
                for h, x, y, _ in obs[1:]: emit("ray", h, x, y)
                emit("expect", 1.0, 3, 1e-6, *cs.triangulate(rays[1:], 1), *truth)
                # the same with one of the other three cameras not seeing the point
                emit("group", name + "/1_of_3_mislabelled")
                emit("ray", obs[0][0], *w0)
                for h, x, y, _ in obs[2:]: emit("ray", h, x, y)
                emit("expect", 1.0, -1, -1.0, *truth, *truth)
            if w0 and w1:
                emit("group", name + "/2_of_4_mislabelled")
                emit("ray", obs[0][0], *w0); emit("ray", obs[1][0], *w1)
                for h, x, y, _ in obs[2:]: emit("ray", h, x, y)
                emit("expect", 1.0, -1, -1.0, *truth, *truth)
            if w1:
                emit("group", name + "/2_cameras_2_points")
                emit("ray", obs[0][0], obs[0][1], obs[0][2]); emit("ray", obs[1][0], *w1)
                emit("expect", 1.0, -1, -1.0, *truth, *truth)

sim_group("ring4", ["-n", "4"], 60, 1e-9)
sim_group("dome4", ["-n", "4", "--layout", "dome"], 60, 1e-9)
for seed in (1, 2, 3):
    sim_group("random4_seed%d" % seed, ["-n", "4", "--layout", "random", "--seed", str(seed)], 60, 1e-9)
sim_group("ring4_distorted", ["-n", "4", "--k1", "-0.12", "--k2", "0.02"], 60, 1e-7)
sim_group("ring4_noise1", ["-n", "4", "--noise", "1"], 60, 1e-9)
sim_group("ring4_noise1_drop30", ["-n", "4", "--noise", "1", "--dropout", "0.3"], 60, 1e-9)
sim_group("ring4_unsync_noise1", ["-n", "4", "--unsync", "--noise", "1"], 60, 1e-9)
sim_group("ring4_unsync_spin180", ["-n", "4", "--unsync", "--spin", "180"], 60, 1e-9)

# one camera of four turned after calibration: true detections, stale calibration
for noise in (0, 1):
    args, scene, cams = make(["-n", "4", "--noise", str(noise)])
    rig = cs.rig_to_opensfm(cams, scene)
    for deg in (0.0, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 5.0):
        emit("rig", write_rig("ring4_bump%03d" % round(deg * 100), bumped(rig, cams[0].host, deg)))
        emit("group", "one_camera_turned_%.2fdeg/noise%d" % (deg, noise))
        for seen in frames_of(args, scene, cams, 60, random.Random(7)):
            for pid, obs in seen.items():
                if len(obs) < 2: continue
                truth = scene.points_at(obs[0][3])[pid]
                for h, x, y, _ in obs: emit("ray", h, x, y)
                emit("expect", 1.0, -1, -1.0, *truth, *truth)

# --- labels: rays grouped by the blob index each camera sent ---
def config(name, argv, frames=120):
    args, scene, cams = make(argv)
    L.append("rig " + write_rig("labels_" + name, cs.rig_to_opensfm(cams, scene))); L.append("config " + name)
    rng = random.Random(args.seed + 1)
    for frame in range(frames):
        t = frame / args.fps
        L.append("frame")
        for p in scene.points_at(t): L.append("truth %r %r %r" % tuple(p))
        by_index = {}
        for cam in cams:
            for index, x, y, _ in cs.detect(cam, scene.points_at(t + cam.phase), args, rng):
                by_index.setdefault(index, []).append((cam.host, f32(x), f32(y)))
        for index, obs in sorted(by_index.items()):
            for o in obs: L.append("ray %s %r %r" % o)
            L.append("index")

base = ["-n", "4", "--noise", "1"]
for labels in ("consistent", "raster", "shuffled"):
    config("cube8_ring_%s" % labels, base + ["--labels", labels])
config("cube8_ring_consistent_drop05", base + ["--dropout", "0.05"])
config("cube8_ring_consistent_drop30", base + ["--dropout", "0.3"])
config("cube8_ring_consistent_drop50", base + ["--dropout", "0.5"])
config("cube8_ring_raster_drop05", base + ["--labels", "raster", "--dropout", "0.05"])
for layout in ("ring", "dome", "random"):
    config("1point_%s_raster" % layout, base + ["--labels", "raster", "--scene", "point", "--layout", layout])
    config("1point_%s_raster_drop30" % layout, base + ["--labels", "raster", "--scene", "point", "--layout", layout, "--dropout", "0.3"])
    for count in (2, 3, 4):
        config("%dpoints_%s_raster" % (count, layout), base + ["--labels", "raster", "--scene", "cloud", "--points", str(count), "--layout", layout])
config("cloud32_ring_raster", base + ["--labels", "raster", "--scene", "cloud", "--points", "32"])
config("cloud32_ring_consistent", base + ["--scene", "cloud", "--points", "32"])

open(out + "/vectors.txt", "w").write("\n".join(V) + "\n")
open(out + "/labels.txt", "w").write("\n".join(L) + "\n")
print(len(V), "vector lines,", len(L), "label lines")
