#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
BUILD=$(mktemp -d "${TMPDIR:-/tmp}/cp100-host.XXXXXX")
trap 'rm -rf "$BUILD"' EXIT
JSON=.pio/libdeps/cp100/ArduinoJson/src
if [ ! -f "$JSON/ArduinoJson.h" ]; then echo 'Run pio pkg install -d firmware from the repo root first.' >&2; exit 2; fi
CXX=${CXX:-c++}
FLAGS='-std=c++17 -Wall -Wextra -O1 -g'
if [ "${1:-}" = --asan ]; then FLAGS="$FLAGS -fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer"; fi
$CXX $FLAGS -Isrc test/host_test.cpp src/dsp.cpp src/model.cpp -o "$BUILD/dsp-test"
"$BUILD/dsp-test"
$CXX $FLAGS -Isrc -Itest/fakes -I"$JSON" test/remote_test.cpp src/controller.cpp src/model.cpp src/dsp.cpp src/protocol.cpp src/storage.cpp -o "$BUILD/remote-test"
"$BUILD/remote-test"
