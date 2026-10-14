#!/bin/bash
# usage: sim_rig.sh <work dir> <tag> <sed script for settings.xml> <calibrated:0|1> <camera_sim args...>
# Runs the built Pinopticap against the camera simulator and compares the 3D blobs it sends with the truth.
# Listener on 7114 (Pinopticap out) and 7120 (truth); simulator first, then Pinopticap mid-stream under Xvfb,
# from a copy of bin/ in the work dir. DUR=seconds sets the length of the run (12).
# RIG_EDIT="bump HOST DEGREES" or "drop HOST" changes the calibration the app reads (see rig_edit.py).
# Kills any Pinopticap and Xvfb that are already running.
[ $# -ge 4 ] || { sed -n 2,7p "$0"; exit 1; }
HERE=$(cd "$(dirname "$0")" && pwd); P=$(cd "$HERE/../../.." && pwd)
S="$1"; TAG="$2"; EDIT="$3"; CAL="$4"; shift 4; mkdir -p "$S"; S=$(cd "$S" && pwd)
export DISPLAY=127.0.0.1:99
pkill -x Pinopticap 2>/dev/null; pkill -x Xvfb 2>/dev/null
RUN="$S/run"; rm -rf "$RUN"; mkdir -p "$RUN/bin/data"
cp "$P/bin/Pinopticap" "$RUN/bin/"; sed "$EDIT" "$P/bin/data/settings.xml" > "$RUN/bin/data/settings.xml"
grep -n "osc_host\|osc_send_port\|send_osc\|debug" "$RUN/bin/data/settings.xml" | tr -s ' ' | tr '\n' ' '; echo
Xvfb :99 -screen 0 1280x1024x24 -nolock -ac -listen tcp -nolisten unix >"$S/xvfb.log" 2>&1 &
DUR=${DUR:-12}
python3 -B "$HERE/osc_listen.py" 7114 7120 $((DUR + 10)) > "$S/listen_$TAG.log" 2>&1 &
LISTEN_PID=$!
RIG=(); [ "$CAL" = 1 ] && RIG=(--write-rig "$RUN/bin/data/reconstruction.json")
python3 -B "$P/tools/camera_sim/camera_sim.py" "$@" "${RIG[@]}" --truth-port 7120 --duration $DUR 2> "$S/sim_$TAG.log" &
SIM_PID=$!
python3 -c "import time; time.sleep(1.5)"
[ -n "$RIG_EDIT" ] && python3 -B "$HERE/rig_edit.py" "$RUN/bin/data/reconstruction.json" $RIG_EDIT
cd "$RUN/bin"
( stdbuf -oL -eL timeout -k 5 60 ./Pinopticap 2>&1; echo "APP EXIT CODE $?" ) | python3 -u -c "
import sys,time
t0=time.time()
for line in sys.stdin:
    sys.stdout.write('%7.2f %s' % (time.time()-t0, line)); sys.stdout.flush()" > "$S/app_$TAG.log" &
wait $LISTEN_PID
import -window root "$S/shot_${TAG}.png" 2>&1 | head -3
pgrep -x Pinopticap >/dev/null && echo "app still running after the run" || echo "APP DIED during the run"
kill $SIM_PID 2>/dev/null; pkill -x Pinopticap; python3 -c "import time; time.sleep(2.5)"; pgrep -x Pinopticap >/dev/null && { echo "APP DID NOT EXIT ON SIGTERM, killing it"; pkill -9 -x Pinopticap; }; pkill -x Xvfb
cat "$S/listen_$TAG.log"
grep -c "new blob" "$S/app_$TAG.log" | sed 's/^/new-blob lines: /'
grep -c "empty socket" "$S/app_$TAG.log" | sed 's/^/empty-socket error lines: /'
grep -n "Calibration\|New Eye\|APP EXIT\|rror" "$S/app_$TAG.log" | cut -c1-150 | head -12
tail -n 2 "$S/sim_$TAG.log" | cut -c1-120
