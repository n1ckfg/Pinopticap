"""Test vectors for Pinopticap/src/Triangulation.hpp, from the simulator's own reference code
and, for the real OpenSfM file, from OpenCV's projection (an implementation independent of both)."""
import json, math, random, struct, sys
sys.dont_write_bytecode = True
sim_dir, sample, out = sys.argv[1], sys.argv[2], sys.argv[3]
sys.path.insert(0, sim_dir)
import camera_sim as cs
import numpy as np, cv2

f32 = lambda v: struct.unpack(">f", struct.pack(">f", v))[0]
lines = []
emit = lambda *a: lines.append(" ".join(repr(float(x)) if isinstance(x, float) else str(x) for x in a))

def sim_group(name, argv, frames, exact_tol):
    args = cs.parse_args(argv)
    rig_rng = random.Random(args.seed)
    scene = cs.Scene(args.scene, args.size, args.points, args.spin, rig_rng)
    cams = cs.build_rig(args, len(scene.base), rig_rng)
    rig_file = name + ".json"
    json.dump(cs.rig_to_opensfm(cams, scene), open(out + "/" + rig_file, "w"))
    rig = json.load(open(out + "/" + rig_file))[0]
    views = {k.split("_")[0]: cs.View(s, rig["cameras"][s["camera"]]) for k, s in rig["shots"].items()}
    emit("rig", rig_file)
    for cam in cams:
        emit("center", cam.host, *cam.c)
    rng = random.Random(args.seed + 1)
    for frame in range(frames):
        seen = {}
        for cam in cams:
            t = frame / args.fps + cam.phase
            for _, x, y, pid in cs.detect(cam, scene.points_at(t), args, rng):
                seen.setdefault(pid, []).append((cam.host, f32(x), f32(y), t))
        for pid, obs in seen.items():
            if len(obs) < 2:
                continue
            rays = [views[h].ray(x, y, True) for h, x, y, _ in obs]
            ref = cs.triangulate(rays, 1)
            if ref is None:
                continue
            truth = scene.points_at(sum(o[3] for o in obs) / len(obs))[pid]
            # 1. no gating: must equal the Python reference
            emit("group", name + "/ungated")
            for h, x, y, _ in obs: emit("ray", h, x, y)
            emit("expect", 80.0, len(obs), exact_tol, *ref, *truth)
            # 2. the default 1 degree gate: report only
            emit("group", name + "/1deg")
            for h, x, y, _ in obs: emit("ray", h, x, y)
            emit("expect", 1.0, -1, -1.0, *ref, *truth)
            # 3. one eye reports a different point under this index (exact-input rigs only)
            if args.noise == 0 and not args.unsync and len(obs) >= 3 and len(seen) > 1:
                other = seen[(pid + 1) % len(scene.base)] if (pid + 1) % len(scene.base) in seen else None
                if other:
                    emit("group", name + "/one_eye_mislabelled")
                    emit("ray", obs[0][0], other[0][1], other[0][2])
                    for h, x, y, _ in obs[1:]: emit("ray", h, x, y)
                    clean = cs.triangulate(rays[1:], 1)
                    emit("expect", 1.0, len(obs) - 1, 1e-6, *clean, *truth)
            # 4. two eyes, two different points under one index
            if args.noise == 0 and not args.unsync and len(seen) > 1:
                other = seen.get((pid + 3) % len(scene.base))
                if other and other[-1][0] != obs[0][0]:
                    emit("group", name + "/two_eyes_different_points")
                    emit("ray", obs[0][0], obs[0][1], obs[0][2])
                    emit("ray", other[-1][0], other[-1][1], other[-1][2])
                    emit("expect", 1.0, -1, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)

sim_group("ring9", ["-n", "9"], 40, 1e-9)
sim_group("random12_distorted", ["-n", "12", "--layout", "random", "--k1", "-0.12", "--k2", "0.02"], 40, 1e-7)
sim_group("dome16_cloud_noise1", ["-n", "16", "--layout", "dome", "--scene", "cloud", "--noise", "1"], 20, 1e-9)
sim_group("random12_noise1_unsync", ["-n", "12", "--layout", "random", "--noise", "1", "--unsync"], 40, 1e-9)
sim_group("ring6_unsync_spin180", ["-n", "6", "--unsync", "--spin", "180"], 40, 1e-9)

# The repo's real OpenSfM reconstruction: project its own sparse points with OpenCV.
rec = json.load(open(sample))[0]
json.dump([rec], open(out + "/real_opensfm.json", "w"))
emit("rig", "real_opensfm.json")
emit("group", "real_opensfm_file_via_cv2")
shots = []
for key, shot in rec["shots"].items():
    m = rec["cameras"][shot["camera"]]
    w, h = m["width"], m["height"]
    f = m["focal"] * max(w, h)
    K = np.array([[f, 0, w / 2.0], [0, f, h / 2.0], [0, 0, 1.0]])
    dist = np.array([m["k1"], m["k2"], 0.0, 0.0])
    R = cv2.Rodrigues(np.array(shot["rotation"]))[0]
    t = np.array(shot["translation"])
    emit("center", key.split("_")[0], *(-R.T @ t))
    shots.append((key.split("_")[0], np.array(shot["rotation"]), t, K, dist, R, w, h))
count = 0
for pid, point in sorted(rec["points"].items(), key=lambda kv: int(kv[0])):
    X = np.array(point["coordinates"])
    obs = []
    for host, rvec, t, K, dist, R, w, h in shots:
        if (R @ X + t)[2] <= 0.1:
            continue
        uv = cv2.projectPoints(X.reshape(1, 3), rvec, t, K, dist)[0].ravel()
        if 0 <= uv[0] < w and 0 <= uv[1] < h:
            obs.append((host, f32(uv[0] / w), f32(uv[1] / h)))
    if len(obs) >= 2:
        for o in obs: emit("ray", *o)
        emit("expect", 1.0, len(obs), 2e-4 * float(np.linalg.norm(X)), *X, *X)
        count += 1
print("real file: %d shots, %d of %d points seen by 2+ shots" % (len(shots), count, len(rec["points"])))
open(out + "/vectors.txt", "w").write("\n".join(lines) + "\n")
print(len(lines), "lines")
