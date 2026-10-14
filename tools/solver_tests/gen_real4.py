"""Scratch: the four nfg-rpi-3-* cameras of the repo's real OpenSfM file, looking at its own sparse points.
Projection by OpenCV (independent of the code under test), pixel noise added, wire values as float32."""
import json, struct, sys, numpy as np, cv2
sample, out = sys.argv[1], sys.argv[2]
f32 = lambda v: struct.unpack(">f", struct.pack(">f", v))[0]
rec = json.load(open(sample))[0]
json.dump([rec], open(out + "/real.json", "w"))
shots = {}
for key, shot in rec["shots"].items():
    host = key.split("_")[0]
    if not host.startswith("nfg-rpi-3-"): continue
    m = rec["cameras"][shot["camera"]]; w, h = m["width"], m["height"]; f = m["focal"] * max(w, h)
    shots[host] = (np.array(shot["rotation"]), np.array(shot["translation"]), np.array([[f, 0, w / 2.0], [0, f, h / 2.0], [0, 0, 1.0]]),
                   np.array([m["k1"], m["k2"], 0.0, 0.0]), cv2.Rodrigues(np.array(shot["rotation"]))[0], w, h)
hosts = sorted(shots)
axis = np.mean([shots[h][4][2] for h in hosts], 0); axis /= np.linalg.norm(axis)
X = np.array([p["coordinates"] for _, p in sorted(rec["points"].items(), key=lambda kv: int(kv[0]))])
uv = {}
for host in hosts:
    rvec, t, K, dist, R, w, h = shots[host]
    p = cv2.projectPoints(X.reshape(-1, 1, 3), rvec, t, K, dist)[0].reshape(-1, 2)
    z = (R @ X.T).T[:, 2] + t[2]
    uv[host] = (p, (z > 0.1))
rng = np.random.default_rng(4)
lines = ["rig real.json", "axis %r %r %r" % tuple(float(a) for a in axis)]
def obs_of(i, noise, only=None):
    out = []
    for host in (only or hosts):
        p, front = uv[host]; w, h = shots[host][5], shots[host][6]
        u, v = p[i] + (rng.normal(0, noise, 2) if noise else 0)
        if front[i] and 0 <= u < w and 0 <= v < h: out.append((host, f32(u / w), f32(v / h)))
    return out
def case(group, obs, truth):
    lines.append("group " + group)
    for o in obs: lines.append("ray %s %r %r" % o)
    lines.append("truth %r %r %r" % tuple(float(a) for a in truth))
for noise in (0, 0.5, 1, 2, 4):
    for i in range(len(X)):
        obs = obs_of(i, noise)
        if len(obs) >= 2: case("all_four/noise_%gpx" % noise, obs, X[i])
for noise in (0, 1):
    for i in range(len(X)):
        for pair in (hosts[:2], hosts[1:3], [hosts[0], hosts[2]]):
            obs = obs_of(i, noise, pair)
            if len(obs) == 2: case("two_cameras/noise_%gpx" % noise, obs, X[i])
        obs = obs_of(i, noise, hosts[:3])
        if len(obs) == 3: case("three_cameras/noise_%gpx" % noise, obs, X[i])
# one camera reports another point under this index (exact and 1 px)
for noise in (0, 1):
    for i in range(len(X)):
        j = int(rng.integers(len(X)))
        good, other = obs_of(i, noise), obs_of(j, noise)
        if len(good) == 4 and len(other) == 4 and np.linalg.norm(X[i] - X[j]) > 0.5:
            k = int(rng.integers(4))
            case("1_of_4_mislabelled/noise_%gpx" % noise, [other[n] if n == k else good[n] for n in range(4)], X[i])
            keep = [n for n in range(4) if n != (k + 1) % 4]
            case("1_of_3_mislabelled/noise_%gpx" % noise, [other[n] if n == k else good[n] for n in keep], X[i])
            case("2_cameras_2_points/noise_%gpx" % noise, [good[k], other[(k + 1) % 4]], X[i])
open(out + "/cases.txt", "w").write("\n".join(lines) + "\n")
print(len(lines), "lines; hosts", hosts, "; mean viewing axis", np.round(axis, 3))
