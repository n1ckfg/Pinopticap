"""Scratch: N cameras with k of them reporting another point under an index, for multi_test."""
import json, random, struct, sys
sys.dont_write_bytecode = True
sim_dir, out = sys.argv[1], sys.argv[2]
WALL = len(sys.argv) > 3
sys.path.insert(0, sim_dir)
import camera_sim as cs
f32 = lambda v: struct.unpack(">f", struct.pack(">f", v))[0]
V = []
emit = lambda *a: V.append(" ".join(repr(float(x)) if isinstance(x, float) else str(x) for x in a))
STEPS = (1, 3, 5, 2, 7, 4, 6)

def group(n, layout, noise, frames=40):
    argv = ["-n", str(n), "--layout", layout, "--noise", str(noise), "--scene", "cloud", "--points", "12", "--seed", str(n)] + (["--size", "3"] if WALL else [])
    args = cs.parse_args(argv)
    rig_rng = random.Random(args.seed)
    scene = cs.Scene(args.scene, args.size, args.points, args.spin, rig_rng)
    cams = cs.build_rig(args, len(scene.base), rig_rng)
    if WALL:
        for cam in cams:
            a, r = rig_rng.uniform(0, 6.2832), 0.6 * rig_rng.random() ** 0.5
            cam.c = (r * cs.math.cos(a), 0.3 * r * cs.math.sin(a), -5.5 + rig_rng.uniform(-0.3, 0.3))
            cam.R = cs.look_at(cam.c, (rig_rng.uniform(-0.3, 0.3), rig_rng.uniform(-0.3, 0.3), 0.0))
            cam.focal = 0.8521
    name = "%s%02d_noise%d" % (layout, n, noise)
    json.dump(cs.rig_to_opensfm(cams, scene), open("%s/%s.json" % (out, name), "w"))
    emit("rig", name + ".json")
    npts = len(scene.base)
    rng = random.Random(1)
    for frame in range(frames):
        t = frame / args.fps
        at, seen = {}, {}
        for cam in cams:
            for _, x, y, pid in cs.detect(cam, scene.points_at(t), args, rng):
                at[(cam.host, pid)] = (f32(x), f32(y))
                seen.setdefault(pid, []).append(cam.host)
        for pid, hosts in seen.items():
            m = len(hosts)
            if m < 3: continue
            truth = scene.points_at(t)[pid]
            order = hosts[:]
            random.Random(frame * 131 + pid).shuffle(order)
            for k in range(0, 8):
                if m - k < 2 or (m - k) * 2 <= m: break
                for kind in ("different", "same"):
                    if k == 0 and kind == "same": continue
                    if k == 1 and kind == "same": continue
                    wrong = []
                    for i in range(k):
                        step = STEPS[i % len(STEPS)] if kind == "different" else 1
                        w = at.get((order[i], (pid + step) % npts))
                        if w is None: break
                        wrong.append(w)
                    if len(wrong) < k: continue
                    emit("group", "%s n=%d layout=%s noise=%d k=%d %s" % (name, m, layout, noise, k, kind if k else "none"))
                    for i, h in enumerate(order):
                        emit("ray", h, *(wrong[i] if i < k else at[(h, pid)]))
                    emit("truth", m, k, *truth)

for layout in (("ring",) if WALL else ("ring", "random")):
    for n in ((3, 4, 5, 6, 7, 8, 9, 12, 16) if WALL else (3, 4, 5, 6, 7, 8, 9, 12, 16, 24, 48)):
        for noise in (0, 1):
            group(n, layout, noise, 40 if n <= 24 else 10)
open(out + "/cases.txt", "w").write("\n".join(V) + "\n")
print(len(V), "lines")
