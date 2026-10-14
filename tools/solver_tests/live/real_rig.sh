#!/bin/bash
# usage: real_rig.sh <work dir> <tag> <sed script for settings.xml> <real reconstruction.json> <camera_sim args...>
# The same as sim_rig.sh, with the four nfg-rpi-3-* cameras of a real OpenSfM file and their real host names
# (see real_rig_sim.py). Also takes screenshots: camera tiles, 3D view, 3D view zoomed out.
# DUR=seconds sets the length of the run (45). Kills any Pinopticap and Xvfb that are already running.
[ $# -ge 4 ] || { sed -n 2,5p "$0"; exit 1; }
HERE=$(cd "$(dirname "$0")" && pwd); P=$(cd "$HERE/../../.." && pwd)
S="$1"; TAG="$2"; EDIT="$3"; REC="$4"; shift 4; mkdir -p "$S"; S=$(cd "$S" && pwd)
export DISPLAY=127.0.0.1:99
pkill -x Pinopticap 2>/dev/null; pkill -x Xvfb 2>/dev/null
RUN="$S/run"; rm -rf "$RUN"; mkdir -p "$RUN/bin/data"
cp "$P/bin/Pinopticap" "$RUN/bin/"; sed "$EDIT" "$P/bin/data/settings.xml" > "$RUN/bin/data/settings.xml"
cp "$REC" "$RUN/bin/data/reconstruction.json"
Xvfb :99 -screen 0 1280x1024x24 -nolock -ac -listen tcp -nolisten unix >"$S/xvfb.log" 2>&1 &
DUR=${DUR:-45}
python3 -B "$HERE/osc_listen.py" 7114 7120 $((DUR + 4)) > "$S/listen_$TAG.log" 2>&1 &
LISTEN_PID=$!
python3 -B "$HERE/real_rig_sim.py" "$REC" "$@" --truth-port 7120 --duration $DUR 2> "$S/sim_$TAG.log" &
SIM_PID=$!
python3 -c "import time; time.sleep(1.5)"
cd "$RUN/bin"
( stdbuf -oL -eL timeout -k 5 90 ./Pinopticap 2>&1; echo "APP EXIT CODE $?" ) | python3 -u -c "
import sys,time
t0=time.time()
for line in sys.stdin:
    sys.stdout.write('%7.2f %s' % (time.time()-t0, line)); sys.stdout.flush()" > "$S/app_$TAG.log" &
python3 -c "import time; time.sleep($DUR - 25)"
import -window root "$S/shot_${TAG}_eyes.png" 2>&1 | head -3
WID=$(xwininfo -root -tree 2>/dev/null | grep -m1 "960x720" | awk '{print $1}')
if [ -n "$WID" ]; then
  xdotool windowfocus "$WID" 2>&1 | head -2; xdotool key space 2>&1 | head -2
  python3 -c "import time; time.sleep(2.5)"
  import -window root "$S/shot_${TAG}_3d.png" 2>&1 | head -3
  # zoom out with the scroll wheel to look for the scene
  xdotool mousemove 480 360 sleep 0.4 click 4 sleep 0.25 click 4 sleep 0.25 click 4 sleep 0.25 click 4 sleep 0.25 click 4 sleep 0.25 click 4 sleep 0.25 click 4 sleep 0.25 click 4 sleep 0.25 click 4 sleep 0.25 click 4 sleep 0.25 click 4 sleep 0.25 click 4 sleep 0.25 click 4 sleep 0.25 click 4 sleep 0.25 click 4 sleep 0.25 click 4 sleep 0.25 click 4 sleep 0.25 click 4 sleep 0.25 click 4 sleep 0.25 click 4 sleep 0.25 click 4 sleep 0.25 click 4 sleep 0.25 click 4 sleep 0.25 click 4 sleep 0.25 click 4 sleep 0.25 click 4 sleep 0.25 click 4 sleep 0.25 click 4 sleep 0.25 click 4 2>&1 | head -2
  python3 -c "import time; time.sleep(2.5)"
  import -window root "$S/shot_${TAG}_zoom.png" 2>&1 | head -3
fi
python3 -c "import time; time.sleep(3)"
pgrep -x Pinopticap >/dev/null && echo "app still running at the end of the stream" || echo "APP DIED during the run"
wait $LISTEN_PID
kill $SIM_PID 2>/dev/null; pkill -x Pinopticap; python3 -c "import time; time.sleep(2.5)"; pgrep -x Pinopticap >/dev/null && { echo "APP DID NOT EXIT ON SIGTERM, killing it"; pkill -9 -x Pinopticap; }; pkill -x Xvfb
head -n 2 "$S/sim_$TAG.log" | cut -c1-260
cat "$S/listen_$TAG.log"
grep -n "Calibration\|New Eye\|APP EXIT\|rror" "$S/app_$TAG.log" | cut -c1-150 | head -12
