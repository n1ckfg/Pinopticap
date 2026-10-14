#!/usr/bin/env python3
"""Simulated camera array for Pinopticap.

Projects one shared 3D scene through any number of virtual cameras and sends
each camera's detections as OSC /blob messages, the way Pinopticam and
Pinoptigrab nodes do. Standard library only.

    python3 camera_sim.py --cameras 8               # stream to 127.0.0.1:7110
    python3 camera_sim.py --cameras 8 --selftest    # no network, check geometry

See README.md for conventions and the calibration file format.
"""

import argparse
import heapq
import json
import math
import random
import socket
import struct
import sys
import time
from collections import defaultdict

NEAR = 0.05  # a point closer than this in front of a camera is not seen
ORIGIN = (0.0, 0.0, 0.0)
GOLDEN_ANGLE = math.pi * (3.0 - math.sqrt(5.0))


# --- 3-vectors and 3x3 matrices as tuples -----------------------------------

def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def scale(a, s):
    return (a[0] * s, a[1] * s, a[2] * s)


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def norm(a):
    return math.sqrt(dot(a, a))


def normalize(a):
    return scale(a, 1.0 / norm(a))


def matvec(m, v):
    return (dot(m[0], v), dot(m[1], v), dot(m[2], v))


def transpose(m):
    return tuple(zip(*m))


def rvec_to_rotation(rvec):
    """Axis-angle vector (axis * radians) to rotation matrix."""
    angle = norm(rvec)
    if angle < 1e-12:
        return ((1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0))
    x, y, z = scale(rvec, 1.0 / angle)
    c, s = math.cos(angle), math.sin(angle)
    k = 1.0 - c
    return ((c + x * x * k, x * y * k - z * s, x * z * k + y * s),
            (y * x * k + z * s, c + y * y * k, y * z * k - x * s),
            (z * x * k - y * s, z * y * k + x * s, c + z * z * k))


def rotation_to_rvec(m):
    """Rotation matrix to axis-angle vector."""
    angle = math.acos(max(-1.0, min(1.0, (m[0][0] + m[1][1] + m[2][2] - 1.0) / 2.0)))
    axis = (m[2][1] - m[1][2], m[0][2] - m[2][0], m[1][0] - m[0][1])  # 2 sin(angle) * axis
    s = norm(axis)
    if s > 1e-6:
        return scale(axis, angle / s)
    if angle < 1e-3:
        return ORIGIN
    # angle is pi, where m = 2 a a^T - I and the antisymmetric part vanishes
    i = max(range(3), key=lambda j: m[j][j])
    ai = math.sqrt((m[i][i] + 1.0) / 2.0)
    return scale(tuple(ai if j == i else m[i][j] / (2.0 * ai) for j in range(3)), angle)


# --- scene ------------------------------------------------------------------

class Scene:
    """A rigid set of points spinning about a tilted axis through the origin."""

    AXIS = normalize((0.3, 1.0, 0.1))

    def __init__(self, kind, size, count, spin_deg, rng):
        h = size / 2.0
        if kind == "cube":
            self.base = [(x, y, z) for x in (-h, h) for y in (-h, h) for z in (-h, h)]
        elif kind == "cloud":
            self.base = [tuple(rng.uniform(-h, h) for _ in range(3)) for _ in range(count)]
        else:  # point
            self.base = [(h, 0.0, 0.0)]
        self.spin = math.radians(spin_deg)
        self._cache = (None, None)

    def points_at(self, t):
        if self._cache[0] != t:
            rot = rvec_to_rotation(scale(self.AXIS, self.spin * t))
            self._cache = (t, [matvec(rot, p) for p in self.base])
        return self._cache[1]


# --- cameras ----------------------------------------------------------------

def look_at(center, target):
    """World-to-camera rotation, rows = camera x (right), y (down), z (forward)."""
    forward = normalize(sub(target, center))
    up = (0.0, 1.0, 0.0)
    if abs(dot(forward, up)) > 0.999:
        up = (0.0, 0.0, 1.0)
    right = normalize(cross(forward, up))
    return (right, cross(forward, right), forward)


class Camera:

    def __init__(self, host, session, center, target, width, height, focal, k1, k2):
        self.host = host
        self.c = center
        self.R = look_at(center, target)
        self.w, self.h = width, height
        self.focal, self.k1, self.k2 = focal, k1, k2
        self.phase = 0.0  # shutter offset within the frame period, seconds
        self.perm = None  # point id -> blob index, for --labels shuffled
        self.prefix = datagram_prefix("/blob", "ssiffi", 16, host, session)

    def project(self, point):
        """Pixel position of a world point, or None if it is behind the camera."""
        p = sub(point, self.c)
        z = dot(self.R[2], p)
        if z < NEAR:
            return None
        x, y = dot(self.R[0], p) / z, dot(self.R[1], p) / z
        r2 = x * x + y * y
        s = self.focal * max(self.w, self.h) * (1.0 + r2 * (self.k1 + r2 * self.k2))
        return (self.w / 2.0 + s * x, self.h / 2.0 + s * y)


def camera_placements(layout, n, radius, height, aim_jitter, rng):
    """(center, target) for each of n cameras."""
    if layout == "ring":
        angles = [2.0 * math.pi * i / n for i in range(n)]
        return [((radius * math.cos(a), height, radius * math.sin(a)), ORIGIN) for a in angles]
    if layout == "dome":
        out = []
        for i in range(n):
            y = 0.15 + 0.8 * (i + 0.5) / n  # stays clear of the floor and the pole
            r, a = math.sqrt(1.0 - y * y), i * GOLDEN_ANGLE
            out.append(((radius * r * math.cos(a), radius * y, radius * r * math.sin(a)), ORIGIN))
        return out
    # random: mixed distances and aim points, so the views only partly overlap
    out = []
    for _ in range(n):
        dist = radius * rng.uniform(0.6, 1.6)
        az, el = rng.uniform(0.0, 2.0 * math.pi), math.radians(rng.uniform(-20.0, 70.0))
        center = (dist * math.cos(el) * math.cos(az), dist * math.sin(el), dist * math.cos(el) * math.sin(az))
        out.append((center, tuple(rng.uniform(-aim_jitter, aim_jitter) for _ in range(3))))
    return out


def build_rig(args, point_count, rng):
    width, height = args.resolution
    focal = 0.5 / math.tan(math.radians(args.fov) / 2.0) * width / max(width, height)
    stamp = time.strftime("%y%m%d%H%M%S")
    cams = []
    placements = camera_placements(args.layout, args.cameras, args.radius, args.height, 0.2 * args.size, rng)
    for i, (center, target) in enumerate(placements):
        cam = Camera(args.hostname.format(i), "SIM_%s%03d" % (stamp, i), center, target,
                     width, height, focal, args.k1, args.k2)
        if args.unsync:
            cam.phase = rng.random() / args.fps
        cam.perm = rng.sample(range(point_count), point_count)
        cams.append(cam)
    return cams


def detect(cam, points, args, rng):
    """One camera frame as [(blob index, x, y, point id)] in wire coordinates."""
    hits = []
    for pid, point in enumerate(points):
        uv = cam.project(point)
        if uv is None:
            continue
        u, v = uv
        if args.noise:
            u, v = u + rng.gauss(0.0, args.noise), v + rng.gauss(0.0, args.noise)
        if not (0.0 <= u < cam.w and 0.0 <= v < cam.h):
            continue
        if args.dropout and rng.random() < args.dropout:
            continue
        hits.append((pid, u, v))
    if args.labels == "raster":
        # index = position in the frame's contour list, bottom of the image first
        hits.sort(key=lambda hit: (-hit[2], hit[1]))
        labelled = [(i, u, v, pid) for i, (pid, u, v) in enumerate(hits)]
    elif args.labels == "shuffled":
        labelled = sorted((cam.perm[pid], u, v, pid) for pid, u, v in hits)
    else:
        labelled = [(pid, u, v, pid) for pid, u, v in hits]
    if args.coords == "normalized":
        return [(i, u / cam.w, v / cam.h, pid) for i, u, v, pid in labelled]
    return labelled


# --- OSC --------------------------------------------------------------------

def osc_string(s):
    data = s.encode("utf-8") + b"\0"
    return data + b"\0" * (-len(data) % 4)


def datagram_prefix(address, typetags, payload_size, *strings):
    """Constant leading bytes of a datagram: one message whose leading arguments
    are strings, wrapped in a bundle (ofxOscReceiver takes it bare or wrapped)."""
    head = osc_string(address) + osc_string("," + typetags) + b"".join(map(osc_string, strings))
    return b"#bundle\0" + struct.pack(">QI", 1, len(head) + payload_size) + head


def wire_timestamp(epoch_seconds):
    """Milliseconds since the epoch cut to int32, like (int) ofGetSystemTimeMillis()."""
    ms = int(epoch_seconds * 1000.0) & 0xFFFFFFFF
    return ms - 0x100000000 if ms >= 0x80000000 else ms


# --- calibration file -------------------------------------------------------

def rig_to_opensfm(cams, scene):
    """Calibration in the layout of an OpenSfM reconstruction.json."""
    cam = cams[0]
    model = "sim perspective %d %d" % (cam.w, cam.h)
    shots = {}
    for cam in cams:
        shots[cam.host + "_0.jpg"] = {
            "rotation": list(rotation_to_rvec(cam.R)),
            "translation": list(scale(matvec(cam.R, cam.c), -1.0)),
            "camera": model,
        }
    points = {str(i): {"coordinates": list(p), "color": [255, 255, 255]}
              for i, p in enumerate(scene.points_at(0.0))}
    return [{
        "cameras": {model: {"projection_type": "perspective", "width": cam.w, "height": cam.h,
                            "focal": cam.focal, "k1": cam.k1, "k2": cam.k2}},
        "shots": shots,
        "points": points,
    }]


# --- reference consumer: calibration file + wire values -> 3D ---------------

class View:
    """One camera as a receiver rebuilds it from the calibration file."""

    def __init__(self, shot, model):
        self.Rt = transpose(rvec_to_rotation(shot["rotation"]))
        self.c = scale(matvec(self.Rt, shot["translation"]), -1.0)
        self.w, self.h = model["width"], model["height"]
        self.s = model["focal"] * max(self.w, self.h)
        self.k1, self.k2 = model["k1"], model["k2"]

    def ray(self, x, y, normalized):
        """World-space ray (origin, unit direction) through a wire coordinate."""
        if normalized:
            x, y = x * self.w, y * self.h
        xd, yd = (x - self.w / 2.0) / self.s, (y - self.h / 2.0) / self.s
        xu, yu = xd, yd
        if self.k1 or self.k2:
            for _ in range(8):  # invert the radial distortion by fixed-point iteration
                r2 = xu * xu + yu * yu
                f = 1.0 + r2 * (self.k1 + r2 * self.k2)
                xu, yu = xd / f, yd / f
        return self.c, normalize(matvec(self.Rt, (xu, yu, 1.0)))


def triangulate(rays, reweight=1):
    """Least-squares intersection of any number of rays.

    rays holds (origin, unit direction) pairs. The point X minimising
    sum_i w_i |(I - d_i d_i^T)(X - c_i)|^2, the squared distance to each ray,
    solves a 3x3 symmetric system that is accumulated in one pass over the
    rays. Each reweight pass sets w_i = 1 / depth_i^2, which turns distance
    into angle so that far cameras are not trusted more than near ones.
    Returns None when the rays are too close to parallel.
    """
    weights = [1.0] * len(rays)
    x = None
    for _ in range(reweight + 1):
        a00 = a01 = a02 = a11 = a12 = a22 = b0 = b1 = b2 = 0.0
        for (c, d), w in zip(rays, weights):
            p00, p01, p02 = w * (1.0 - d[0] * d[0]), -w * d[0] * d[1], -w * d[0] * d[2]
            p11, p12, p22 = w * (1.0 - d[1] * d[1]), -w * d[1] * d[2], w * (1.0 - d[2] * d[2])
            a00 += p00; a01 += p01; a02 += p02; a11 += p11; a12 += p12; a22 += p22
            b0 += p00 * c[0] + p01 * c[1] + p02 * c[2]
            b1 += p01 * c[0] + p11 * c[1] + p12 * c[2]
            b2 += p02 * c[0] + p12 * c[1] + p22 * c[2]
        c00, c01, c02 = a11 * a22 - a12 * a12, a02 * a12 - a01 * a22, a01 * a12 - a02 * a11
        det = a00 * c00 + a01 * c01 + a02 * c02
        # for two rays at angle t, det / total^3 = sin(t)^2 / 4; this cuts off near 0.4 degrees
        if det <= 1e-5 * sum(weights) ** 3:
            return None
        c11, c12, c22 = a00 * a22 - a02 * a02, a01 * a02 - a00 * a12, a00 * a11 - a01 * a01
        x = ((c00 * b0 + c01 * b1 + c02 * b2) / det,
             (c01 * b0 + c11 * b1 + c12 * b2) / det,
             (c02 * b0 + c12 * b1 + c22 * b2) / det)
        depths = [dot(sub(x, c), d) for c, d in rays]
        weights = [1.0 / (depth * depth) if depth > 1e-6 else 0.0 for depth in depths]
    return x


def selftest(cams, scene, args, rng):
    """Rebuild the scene from the calibration file and the wire values alone,
    using the true point ids to pair detections across cameras."""
    rig = json.loads(json.dumps(rig_to_opensfm(cams, scene)))[0]
    views = {key.split("_")[0]: View(shot, rig["cameras"][shot["camera"]])
             for key, shot in rig["shots"].items()}
    normalized = args.coords == "normalized"
    solved = unseen = parallel = view_total = 0
    sq_sum = worst = 0.0
    for frame in range(args.frames):
        rays, times = defaultdict(list), defaultdict(list)
        for cam in cams:
            t = frame / args.fps + cam.phase
            for _, x, y, pid in detect(cam, scene.points_at(t), args, rng):
                x, y = struct.unpack(">ff", struct.pack(">ff", x, y))  # float32 on the wire
                rays[pid].append(views[cam.host].ray(x, y, normalized))
                times[pid].append(t)
        for pid in range(len(scene.base)):
            if len(rays[pid]) < 2:
                unseen += 1
                continue
            estimate = triangulate(rays[pid], args.reweight)
            if estimate is None:
                parallel += 1
                continue
            truth = scene.points_at(sum(times[pid]) / len(times[pid]))[pid]
            error = norm(sub(estimate, truth))
            solved += 1
            view_total += len(rays[pid])
            sq_sum += error * error
            worst = max(worst, error)

    total = args.frames * len(scene.base)
    print("selftest: %d cameras (%s), scene %s (%d points), %d frames"
          % (len(cams), args.layout, args.scene, len(scene.base), args.frames))
    print("  noise %g px, dropout %g, labels %s, coords %s, %s shutters, reweight %d"
          % (args.noise, args.dropout, args.labels, args.coords,
             "unsynchronised" if args.unsync else "synchronised", args.reweight))
    print("  triangulated %d of %d points (%d seen by fewer than 2 cameras, %d near-parallel)"
          % (solved, total, unseen, parallel))
    if not solved:
        print("  FAIL: nothing could be triangulated")
        return 1
    rms = math.sqrt(sq_sum / solved)
    print("  views per point %.1f, 3D error rms %.3g max %.3g (camera distance %g)"
          % (view_total / solved, rms, worst, args.radius))
    if args.noise == 0 and not (args.unsync and args.spin):
        ok = worst < 1e-4 * args.radius
        print("  %s: exact input must reproduce the scene" % ("PASS" if ok else "FAIL"))
        return 0 if ok else 1
    return 0


# --- live streaming ---------------------------------------------------------

def stream(cams, scene, args, rng):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    blob_dest = (args.host, args.port)
    truth_dest = (args.host, args.truth_port) if args.truth_port else None
    truth_prefix = datagram_prefix("/truth", "ssifffi", 20, "truth", "SIM_truth")
    period = 1.0 / args.fps

    # (time the frame is due, camera index, frame number); index -1 is the ground truth
    phases = {i: cam.phase for i, cam in enumerate(cams)}
    if truth_dest:
        phases[-1] = 0.0
    schedule = [(phase, i, 0) for i, phase in phases.items()]
    heapq.heapify(schedule)

    print("%d cameras (%s ... %s) -> %s:%d at %g fps, scene %s (%d points)"
          % (len(cams), cams[0].host, cams[-1].host, args.host, args.port, args.fps,
             args.scene, len(scene.base)), file=sys.stderr)
    start, wall_start = time.monotonic(), time.time()
    sent = frames = 0
    report_at, lag = 1.0, 0.0
    try:
        while True:
            due, i, frame = schedule[0]
            if args.duration and due >= args.duration:
                break
            if due >= report_at:
                if not args.quiet:
                    print("%7.1fs  %6d blobs/s  %5.2f blobs per camera frame  lag %4.0f ms"
                          % (report_at, sent, sent / max(frames, 1), lag * 1000.0), file=sys.stderr)
                sent = frames = 0
                lag = 0.0
                report_at += 1.0
            now = time.monotonic() - start
            if due > now:
                time.sleep(due - now)
            lag = max(lag, now - due)
            heapq.heapreplace(schedule, (phases[i] + (frame + 1) * period, i, frame + 1))

            points = scene.points_at(due)
            stamp = wire_timestamp(wall_start + due)
            if i < 0:
                for pid, p in enumerate(points):
                    sock.sendto(truth_prefix + struct.pack(">ifffi", pid, p[0], p[1], p[2], stamp), truth_dest)
            else:
                cam = cams[i]
                for index, x, y, _ in detect(cam, points, args, rng):
                    sock.sendto(cam.prefix + struct.pack(">iffi", index, x, y, stamp), blob_dest)
                    sent += 1
                frames += 1
    except KeyboardInterrupt:
        pass
    return 0


# --- command line -----------------------------------------------------------

def resolution(text):
    width, height = text.lower().split("x")
    return int(width), int(height)


def parse_args(argv):
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0],
                                formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    rig = p.add_argument_group("camera rig")
    rig.add_argument("-n", "--cameras", type=int, default=6, help="number of simulated cameras")
    rig.add_argument("--layout", choices=("ring", "dome", "random"), default="ring")
    rig.add_argument("--radius", type=float, default=3.0, help="camera distance from the scene origin")
    rig.add_argument("--height", type=float, default=1.0, help="camera height, ring layout only")
    rig.add_argument("--fov", type=float, default=61.0, help="horizontal field of view in degrees")
    rig.add_argument("--resolution", type=resolution, default=(640, 480), metavar="WxH")
    rig.add_argument("--k1", type=float, default=0.0, help="radial distortion, r^2 term")
    rig.add_argument("--k2", type=float, default=0.0, help="radial distortion, r^4 term")
    rig.add_argument("--hostname", default="cam{:02d}.sim", metavar="FORMAT",
                     help="hostName of camera i; see README before using single-label names")
    rig.add_argument("--seed", type=int, default=1, help="same arguments and seed give the same rig")

    scene = p.add_argument_group("scene")
    scene.add_argument("--scene", choices=("cube", "cloud", "point"), default="cube")
    scene.add_argument("--size", type=float, default=1.0, help="cube edge, or extent of the cloud")
    scene.add_argument("--points", type=int, default=32, help="number of points in the cloud scene")
    scene.add_argument("--spin", type=float, default=30.0, help="rotation speed in degrees per second")

    det = p.add_argument_group("detection")
    det.add_argument("--fps", type=float, default=30.0, help="frame rate of every camera")
    det.add_argument("--noise", type=float, default=0.0, help="Gaussian noise on each detection, pixels")
    det.add_argument("--dropout", type=float, default=0.0, help="chance that a visible point is missed")
    det.add_argument("--unsync", action="store_true", help="give every camera its own shutter phase")
    det.add_argument("--labels", choices=("consistent", "shuffled", "raster"), default="consistent",
                     help="how blob indices relate to scene points")
    det.add_argument("--coords", choices=("normalized", "pixels"), default="normalized",
                     help="units of the x and y that are sent")

    out = p.add_argument_group("output")
    out.add_argument("--host", default="127.0.0.1", help="where Pinopticap runs")
    out.add_argument("--port", type=int, default=7110, help="Pinopticap osc_receive_port")
    out.add_argument("--truth-port", type=int, default=0, metavar="PORT",
                     help="also send the true 3D points as /truth to this port")
    out.add_argument("--write-rig", metavar="FILE", help="write the camera calibration as JSON")
    out.add_argument("--duration", type=float, default=0.0, help="stop after this many seconds (0 = run on)")
    out.add_argument("--quiet", action="store_true", help="no status line every second")

    test = p.add_argument_group("self test")
    test.add_argument("--selftest", action="store_true",
                      help="send nothing; triangulate the simulated detections and report the error")
    test.add_argument("--frames", type=int, default=120, help="frames to check in the self test")
    test.add_argument("--reweight", type=int, default=1, help="reweighting passes of the triangulation")

    args = p.parse_args(argv)
    if args.cameras < 1 or args.fps <= 0 or args.points < 1:
        p.error("--cameras, --fps and --points must be positive")
    return args


def main(argv=None):
    args = parse_args(argv)
    rig_rng = random.Random(args.seed)
    scene = Scene(args.scene, args.size, args.points, args.spin, rig_rng)
    cams = build_rig(args, len(scene.base), rig_rng)
    if args.write_rig:
        with open(args.write_rig, "w") as f:
            json.dump(rig_to_opensfm(cams, scene), f, indent=2)
    detection_rng = random.Random(args.seed + 1)
    if args.selftest:
        return selftest(cams, scene, args, detection_rng)
    return stream(cams, scene, args, detection_rng)


if __name__ == "__main__":
    sys.exit(main())
