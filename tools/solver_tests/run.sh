#!/bin/bash
# Builds the tests of src/Triangulation.hpp outside openFrameworks, makes their input with the
# camera simulator and runs them. Everything made goes into build/ next to this script.
#
# usage: run.sh           the tests with a known result, each compared with expected/
#        run.sh compare   also the old rule, the prototype and the header side by side (slow)
#        run.sh timing    also the time per group of rays
#        run.sh all       everything
#
# REAL=file sets the real OpenSfM reconstruction (default: the one in Pinopticon/experimental).
# OF_ROOT=dir sets the openFrameworks directory (default: three levels above Pinopticap).
MODE=${1:-}
HERE=$(cd "$(dirname "$0")" && pwd)
APP=$(cd "$HERE/../.." && pwd)
OF=${OF_ROOT:-$(cd "$APP/../../.." && pwd)}
SIM="$APP/tools/camera_sim"
REAL=${REAL:-$APP/../../experimental/pinopticap_ray_test/reconstruction.json}
JSONCPP="$OF/addons/ofxJSON/libs/jsoncpp"
B="$HERE/build"
FLAGS="-std=c++20 -O2 -Wall -Wextra -DGLM_FORCE_CTOR_INIT -DGLM_ENABLE_EXPERIMENTAL -I$HERE/shim -I$HERE -I$APP/src -I$OF/libs/glm/include -I$JSONCPP/include"
failures=0

for needed in "$REAL" "$JSONCPP/src/jsoncpp.cpp" "$OF/libs/glm/include/glm/glm.hpp" "$SIM/camera_sim.py"; do
    [ -f "$needed" ] || { echo "missing $needed"; exit 1; }
done
mkdir -p "$B/out"

# true if the first file exists and is newer than all the others
fresh() {
    [ -f "$1" ] && [ "$(ls -t "$@" 2> /dev/null | head -n 1)" = "$1" ]
}

build() {
    fresh "$B/jsoncpp.o" "$JSONCPP/src/jsoncpp.cpp" || g++ -std=c++20 -O2 -w -I"$JSONCPP/include" -c "$JSONCPP/src/jsoncpp.cpp" -o "$B/jsoncpp.o" || exit 1
    fresh "$B/$1" "$HERE/$1.cpp" "$HERE"/*.hpp "$HERE"/shim/*.h "$APP/src/Triangulation.hpp" && return
    echo "building $1"
    g++ $FLAGS "$HERE/$1.cpp" "$B/jsoncpp.o" -o "$B/$1" || exit 1
}

# generate <file it makes, in build/> <generator> <arguments...>
generate() {
    local made="$B/$1" script="$HERE/$2"
    shift 2
    fresh "$made" "$script" "$SIM/camera_sim.py" "$REAL" && return
    echo "making $(dirname "${made#"$B"/}")/ with $(basename "$script")"
    mkdir -p "$(dirname "$made")"
    python3 -B "$script" "$@" > "$made.log" 2>&1 || { cat "$made.log"; exit 1; }
}

# check <name> <program> <data directory>: run a test and compare what it prints with expected/<name>.txt
check() {
    "$B/$2" "$B/$3" > "$B/out/$1.txt" 2>&1
    local status=$?
    if [ $status -ne 0 ]; then
        echo "FAILED  $1 (exit $status), see build/out/$1.txt"
        tail -n 3 "$B/out/$1.txt"
        failures=$((failures + 1))
    elif ! diff -q "$HERE/expected/$1.txt" "$B/out/$1.txt" > /dev/null 2>&1; then
        echo "DIFFERS $1: diff expected/$1.txt build/out/$1.txt"
        failures=$((failures + 1))
    else
        echo "ok      $1"
    fi
}

generate data/vectors.txt gen_vectors.py "$SIM" "$REAL" "$B/data"
generate data/labels.txt gen_labels.py "$SIM" "$B/data"
generate data4/vectors.txt gen4.py "$SIM" "$B/data4"
generate real4/cases.txt gen_real4.py "$REAL" "$B/real4"
for program in tri_test labels_test labels4_test real4_test; do build $program; done

check tri_data tri_test data            # 6 to 16 cameras and the real file, against the Python reference
check tri_data4 tri_test data4          # four cameras: wrong blobs, a turned camera
check labels_data labels_test data      # 3, 9 and 24 cameras with blob indices that do not match
check labels_data4 labels4_test data4   # the same for four cameras
check real4 real4_test real4            # the four cameras of the real file

if [ "$MODE" = compare ] || [ "$MODE" = all ]; then
    generate multi/cases.txt gen_multi.py "$SIM" "$B/multi"
    generate wall/cases.txt gen_multi.py "$SIM" "$B/wall" wall
    build compare4
    build multi_test
    echo; echo "=== four cameras: header and prototype ==="
    "$B/compare4" "$B/data4"
    echo; echo "=== 3 to 48 cameras around the scene, k of them reporting another blob ==="
    "$B/multi_test" "$B/multi" 2
    echo; echo "=== 3 to 16 cameras side by side, 5.5 units from the scene ==="
    "$B/multi_test" "$B/wall" 2 0.5
fi

if [ "$MODE" = timing ] || [ "$MODE" = all ]; then
    build time_labels
    echo; echo "=== time per index group: 3, 9 and 24 cameras ==="
    "$B/time_labels" "$B/data"
    echo; echo "=== time per index group: 4 cameras ==="
    "$B/time_labels" "$B/data4"
    if pkg-config --exists opencv4 2> /dev/null; then
        fresh "$B/tri_bench" "$HERE/tri_bench.cpp" || g++ -O3 -std=c++20 -Wno-deprecated-enum-enum-conversion -DGLM_ENABLE_EXPERIMENTAL -include glm/gtc/matrix_access.hpp -I"$OF/libs/glm/include" $(pkg-config --cflags opencv4) "$HERE/tri_bench.cpp" -o "$B/tri_bench" $(pkg-config --libs opencv4) || exit 1
        echo; echo "=== triangulation methods compared (tri_bench) ==="
        "$B/tri_bench"
    else
        echo "tri_bench skipped: needs the OpenCV development files (pkg-config opencv4)"
    fi
fi

echo
[ $failures -eq 0 ] && echo "all tests ok" || echo "$failures of 5 tests failed or differ"
exit $failures
