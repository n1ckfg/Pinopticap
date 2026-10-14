import json, random, struct, sys
sys.dont_write_bytecode = True
sim_dir, out = sys.argv[1], sys.argv[2]
sys.path.insert(0, sim_dir)
import camera_sim as cs
f32 = lambda v: struct.unpack(">f", struct.pack(">f", v))[0]
lines = []
def config(name, argv, frames=60):
    args = cs.parse_args(argv)
    rig_rng = random.Random(args.seed)
    scene = cs.Scene(args.scene, args.size, args.points, args.spin, rig_rng)
    cams = cs.build_rig(args, len(scene.base), rig_rng)
    json.dump(cs.rig_to_opensfm(cams, scene), open(out + "/" + name + ".json", "w"))
    lines.append("rig %s.json" % name); lines.append("config " + name)
    rng = random.Random(args.seed + 1)
    for frame in range(frames):
        t = frame / args.fps
        lines.append("frame")
        for p in scene.points_at(t): lines.append("truth %r %r %r" % tuple(p))
        by_index = {}
        for cam in cams:
            for index, x, y, _ in cs.detect(cam, scene.points_at(t + cam.phase), args, rng):
                by_index.setdefault(index, []).append((cam.host, f32(x), f32(y)))
        for index, obs in sorted(by_index.items()):
            for o in obs: lines.append("ray %s %r %r" % o)
            lines.append("index")
for n in (3, 9, 24):
    for labels in ("consistent", "raster", "shuffled"):
        config("cube_%dcams_%s" % (n, labels), ["-n", str(n), "--labels", labels, "--noise", "1"])
config("cube_9cams_consistent_dropout30", ["-n", "9", "--noise", "1", "--dropout", "0.3"])
config("cloud32_9cams_raster", ["-n", "9", "--labels", "raster", "--scene", "cloud", "--points", "32", "--noise", "1"])
open(out + "/labels.txt", "w").write("\n".join(lines) + "\n")
