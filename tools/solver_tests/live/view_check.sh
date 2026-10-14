#!/bin/bash
# usage: view_check.sh <work dir> <tag> <calibrated:0|1> <camera_sim args...>
# Starts the simulator FIRST, then Pinopticap mid-stream under Xvfb from a scratch copy of bin/,
# screenshots with the eyes shown, presses space, screenshots the 3D view, presses space again.
# Run check_shot.py on the 3D screenshot afterwards to compare it with where the true points should be drawn.
# ROTATE_RIG=1 turns the world frame of the calibration by 90 degrees first (see rotate_rig.py).
# Kills any Pinopticap and Xvfb that are already running.
[ $# -ge 3 ] || { sed -n 2,7p "$0"; exit 1; }
HERE=$(cd "$(dirname "$0")" && pwd); P=$(cd "$HERE/../../.." && pwd)
S="$1"; TAG="$2"; CAL="$3"; shift 3; mkdir -p "$S"; S=$(cd "$S" && pwd)
export DISPLAY=127.0.0.1:99
pkill -x Pinopticap 2>/dev/null; pkill -x Xvfb 2>/dev/null
RUN="$S/run"; rm -rf "$RUN"; mkdir -p "$RUN/bin/data"
cp "$P/bin/Pinopticap" "$RUN/bin/"; cp "$P/bin/data/settings.xml" "$RUN/bin/data/"
Xvfb :99 -screen 0 1280x1024x24 -nolock -ac -listen tcp -nolisten unix >"$S/xvfb.log" 2>&1 &
RIG=(); [ "$CAL" = 1 ] && RIG=(--write-rig "$RUN/bin/data/reconstruction.json")
python3 -B "$P/tools/camera_sim/camera_sim.py" "$@" "${RIG[@]}" --duration 60 2> "$S/sim_$TAG.log" &
SIM_PID=$!
python3 -c "import time; time.sleep(1.5)"
[ -n "$ROTATE_RIG" ] && python3 -B "$HERE/rotate_rig.py" "$RUN/bin/data/reconstruction.json"
cp "$RUN/bin/data/reconstruction.json" "$S/rig_$TAG.json" 2>/dev/null
cd "$RUN/bin"
( stdbuf -oL -eL timeout -k 5 80 ./Pinopticap 2>&1; echo "APP EXIT CODE $?" ) | python3 -u -c "
import sys,time
t0=time.time()
for line in sys.stdin:
    sys.stdout.write('%7.2f %s' % (time.time()-t0, line)); sys.stdout.flush()" > "$S/app_$TAG.log" &
python3 - <<'PY'
import subprocess, time
for _ in range(160):
    if ":7110 " in subprocess.run(["ss", "-lun"], capture_output=True, text=True).stdout: print("app is listening on udp 7110"); break
    time.sleep(0.25)
else: print("app never opened udp 7110")
time.sleep(8)
PY
import -window root "$S/shot_${TAG}_eyes.png" 2>&1 | head -3
xwininfo -root -tree 2>&1 | grep -v "^$" | sed -n 1,12p
WID=$(xwininfo -root -tree 2>/dev/null | grep -m1 "960x720" | awk '{print $1}')
echo "window id: ${WID:-none}"
xdotool windowfocus "$WID" 2>&1 | head -2; xdotool key space 2>&1 | head -2
python3 -c "import time; time.sleep(2.5)"
import -window root "$S/shot_${TAG}_3d.png" 2>&1 | head -3
xdotool mousemove 480 360 sleep 0.4 mousedown 1 sleep 0.4 mousemove 470 350 sleep 0.4 mousemove 440 310 sleep 0.4 mousemove 400 270 sleep 0.4 mousemove 380 250 sleep 0.4 mouseup 1 sleep 0.3 click 4 sleep 0.3 click 4 sleep 0.3 click 4 2>&1 | head -2
python3 -c "import time; time.sleep(2.5)"
import -window root "$S/shot_${TAG}_orbit.png" 2>&1 | head -3
xdotool key space 2>&1 | head -2
python3 -c "import time; time.sleep(2.5)"
import -window root "$S/shot_${TAG}_eyes2.png" 2>&1 | head -3
pgrep -x Pinopticap >/dev/null && echo "app still running after the run" || echo "APP DIED during the run"
kill $SIM_PID 2>/dev/null; pkill -x Pinopticap; python3 -c "import time; time.sleep(2.5)"; pgrep -x Pinopticap >/dev/null && { echo "APP DID NOT EXIT ON SIGTERM, killing it"; pkill -9 -x Pinopticap; }; pkill -x Xvfb
grep -c "new blob" "$S/app_$TAG.log" | sed 's/^/new-blob lines: /'
grep -n "Calibration\|New Eye\|APP EXIT\|Exception: \|rror" "$S/app_$TAG.log" | cut -c1-150 | head -24
