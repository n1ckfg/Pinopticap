#!/usr/bin/env python3
"""Record what Pinopticap sends (OUT port) and the simulator's /truth (TRUTH port),
then compare the 3D blobs with the truth. usage: osc_listen.py OUT_PORT TRUTH_PORT SECONDS [static]"""
import bisect, select, socket, struct, sys, time, statistics, collections

def osc_string(buf, pos):
    end = buf.index(b"\0", pos)
    return buf[pos:end].decode(), (end + 4) & ~3

def messages(buf):
    if buf.startswith(b"#bundle\0"):
        pos = 16
        while pos < len(buf):
            (size,) = struct.unpack_from(">i", buf, pos)
            yield from messages(buf[pos + 4:pos + 4 + size])
            pos += 4 + size
        return
    address, pos = osc_string(buf, 0)
    tags, pos = osc_string(buf, pos)
    args = []
    for tag in tags[1:]:
        if tag == "s":
            value, pos = osc_string(buf, pos)
        elif tag == "i":
            (value,) = struct.unpack_from(">i", buf, pos); pos += 4
        elif tag == "f":
            (value,) = struct.unpack_from(">f", buf, pos); pos += 4
        else:
            raise ValueError("unhandled tag " + tag)
        args.append(value)
    if pos != len(buf): raise ValueError("%d trailing bytes" % (len(buf) - pos))
    yield address, tags[1:], args

out_port, truth_port, seconds = int(sys.argv[1]), int(sys.argv[2]), float(sys.argv[3])
socks = {}
for port in (out_port, truth_port):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1 << 22)
    s.bind(("127.0.0.1", port)); socks[s] = port
out, truth, datagrams, shapes = [], collections.defaultdict(list), 0, collections.Counter()
t0 = time.monotonic()
while time.monotonic() - t0 < seconds:
    for s in select.select(list(socks), [], [], 0.2)[0]:
        buf = s.recv(65536); t = time.monotonic() - t0
        for address, tags, args in messages(buf):
            if socks[s] == truth_port:
                truth[args[2]].append((t, args[3:6]))
            else:
                shapes[(address, tags, len(buf), buf.startswith(b"#bundle"))] += 1
                out.append((t, args))
        if socks[s] == out_port: datagrams += 1

print("truth: %d points, %d messages; out: %d messages in %d datagrams" % (len(truth), sum(map(len, truth.values())), len(out), datagrams))
for shape, n in shapes.items(): print("  out shape %s x %d" % (shape, n))
if not out: sys.exit(0)
print("  hostName/sessionId seen:", sorted({(a[0], a[1]) for _, a in out}))
print("  indices seen:", dict(sorted(collections.Counter(a[2] for _, a in out).items())))
first, last = out[0][0], out[-1][0]
stamps = collections.Counter(a[6] for _, a in out)
print("  out from %.2fs to %.2fs: %.0f messages/s, %d updates (%.1f/s), blobs per update min %d max %d"
      % (first, last, len(out) / max(last - first, 1e-9), len(stamps), len(stamps) / max(last - first, 1e-9), min(stamps.values()), max(stamps.values())))
# stamps must not decrease, and must advance about as fast as the wall clock
ts = [a[6] for _, a in out]
print("  blobs per update: " + ", ".join("%d x %d" % (k, n) for k, n in sorted(collections.Counter(stamps.values()).items())))
print("  timestamps monotonic: %s; advanced %d ms over %.0f ms of wall time" % (all(b >= a for a, b in zip(ts, ts[1:])), ts[-1] - ts[0], 1000 * (last - first)))
# time between one update and the next, by the app's own timestamps
gaps = sorted(set(ts)); gaps = sorted(b - a for a, b in zip(gaps, gaps[1:]))
print("  ms between updates that sent something: 10%% %d, median %d, 90%% %d, max %d" % (gaps[len(gaps) // 10], gaps[len(gaps) // 2], gaps[len(gaps) * 9 // 10], gaps[-1]))
if truth:
    tl = max(v[-1][0] for v in truth.values()); tf = min(v[0][0] for v in truth.values())
    print("  truth from %.2fs to %.2fs; out stopped %.0f ms after the last truth" % (tf, tl, 1000 * (last - tl)))
    errs, lagbest = [], collections.defaultdict(list)
    times = {pid: [e[0] for e in seq] for pid, seq in truth.items()}
    anyerrs = []
    for t, a in out:
        seq = truth.get(a[2])
        if not seq: errs.append(float("inf")); continue
        def err(at, seq=seq, a=a):
            i = bisect.bisect_left(times[a[2]], at)
            i = min((j for j in (i - 1, i) if 0 <= j < len(seq)), key=lambda j: abs(seq[j][0] - at))
            return sum((seq[i][1][k] - a[3 + k]) ** 2 for k in range(3)) ** 0.5
        errs.append(err(t))
        # nearest true point of any index, at that time (for labels that are not point ids)
        best = float("inf")
        for pid, q in truth.items():
            i = min(bisect.bisect_left(times[pid], t), len(q) - 1)
            best = min(best, sum((q[i][1][k] - a[3 + k]) ** 2 for k in range(3)) ** 0.5)
        anyerrs.append(best)
        for lag in (0.0, 0.033, 0.066, 0.1, 0.15, 0.2, 0.3): lagbest[lag].append(err(t - lag))
    errs.sort()
    print("  3D error against the truth nearest in arrival time: median %.2e, 95%% %.2e, max %.2e (scene units); over 0.1: %d" % (errs[len(errs) // 2], errs[int(len(errs) * 0.95)], errs[-1], sum(e > 0.1 for e in errs)))
    anyerrs.sort()
    print("  distance to the nearest true point of any index: median %.2e, 95%% %.2e; over 0.1: %d of %d" % (anyerrs[len(anyerrs) // 2], anyerrs[int(len(anyerrs) * 0.95)], sum(e > 0.1 for e in anyerrs), len(anyerrs)))
    print("  median error if the output is taken to be this late: " + ", ".join("%d ms: %.5f" % (1000 * lag, statistics.median(v)) for lag, v in lagbest.items()))
# exact repeats of the previous position of the same index
prev, repeats = {}, 0
for t, a in out:
    if prev.get(a[2]) == tuple(a[3:6]): repeats += 1
    prev[a[2]] = tuple(a[3:6])
print("  messages repeating the previous position of the same index exactly: %d of %d" % (repeats, len(out)))
