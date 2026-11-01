#!/bin/sh
# Builds the simulated board and runs every scenario, each in a fresh process
# with its own NVS file (persistence uses one file across three processes).
# Exit status is non-zero if any check fails.
#   ./run.sh                 normal build (-O2)
#   ./run.sh --asan          also rebuild with AddressSanitizer + UBSan and rerun everything
#   ./run.sh --fw DIR [...]  build against the firmware tree DIR (build dir DIR/simbuild),
#                            e.g. a mutated copy; "--only a,b" runs just those scenarios
set -u
cd "$(dirname "$0")"
FW=../..
BUILD=build
ASAN=0
ONLY=
while [ $# -gt 0 ]; do
  case "$1" in
    --asan) ASAN=1 ;;
    --fw) FW=$2; BUILD=$2/simbuild; shift ;;
    --only) ONLY=$2; shift ;;
    *) echo "usage: $0 [--asan] [--fw DIR] [--only s1,s2]" >&2; exit 2 ;;
  esac
  shift
done
OUT=out
[ "$FW" = ../.. ] || OUT=$BUILD/out
mkdir -p "$OUT"
LOG="$OUT/results.txt"
: > "$LOG"

SIMDIR=${SIMDIR:-../../../simulator/dist}  # the repo's simulator; override with SIMDIR=...
if command -v node >/dev/null 2>&1; then
  node ref/gen_reference.mjs > ref/reference.h.new && mv ref/reference.h.new ref/reference.h
  node ref/gen_font.mjs > ref/font.h.new && { cmp -s ref/font.h.new ref/font.h || mv ref/font.h.new ref/font.h; }
  rm -f ref/font.h.new
fi
# ref/*.js copies must be the simulator's current files (the references are generated from them)
if [ -d "$SIMDIR" ]; then
  for f in app.js:app.js dsp.js:dsp.mjs model.js:model.mjs; do
    src=${f%%:*}; dst=${f##*:}
    if cmp -s "$SIMDIR/$src" "ref/$dst"; then echo "CHECK PASS refs/$dst: same as simulator/dist/$src" >> "$LOG"
    else echo "CHECK FAIL refs/$dst: differs from simulator/dist/$src (copy it and rerun)" >> "$LOG"; fi
  done
fi

ALL="selftest boot pad priority owner harmonics console panel flash sweep nvs robust display pulse ledlatch uptime pulse_uptime loopgap txbuf i2s_stall dsp round2 round2_panel"
want() { [ -z "$ONLY" ] || case ",$ONLY," in *",$1,"*) return 0 ;; *) return 1 ;; esac; }

run_one() {  # $1 = binary, $2 = label, $3 = scenario, $4 = nvs file, $5 = timeout
  CP100_NVS="$4" CP100_EXPORT="$OUT/export.json" timeout "$5" "$1" "$3" > "$OUT/$3.log" 2>&1
  rc=$?
  cat "$OUT/$3.log" >> "$LOG"
  [ $rc -le 1 ] || echo "CHECK FAIL $3/process: exit code $rc ($2)" >> "$LOG"
}

run_suite() {  # $1 = binary, $2 = label
  BIN=$1
  pids=
  for s in $ALL; do
    want "$s" || continue
    rm -f "$OUT/$s.nvs"
    ( CP100_NVS="$OUT/$s.nvs" CP100_EXPORT="$OUT/export-$s.json" timeout 150 "$BIN" "$s" > "$OUT/$s.log" 2>&1
      echo $? > "$OUT/$s.rc" ) &
    pids="$pids $!"
  done
  [ -z "$pids" ] || wait $pids
  for s in $ALL; do
    want "$s" || continue
    cat "$OUT/$s.log" >> "$LOG"
    rc=$(cat "$OUT/$s.rc" 2>/dev/null || echo 99)
    [ "$rc" -le 1 ] || echo "CHECK FAIL $s/process: exit code $rc ($2)" >> "$LOG"
    if [ "$s" = console ] && command -v node >/dev/null 2>&1; then node ref/check_export.mjs "$OUT/export-console.json" >> "$LOG"; fi
  done
  if want persist1; then
    rm -f "$OUT/persist.nvs"
    for s in persist1 persist2; do run_one "$BIN" "$2" "$s" "$OUT/persist.nvs" 60; done
    # third boot with a blob from another layout version (first byte changed)
    sed 's/ 01/ 02/' "$OUT/persist.nvs" > "$OUT/persist3.nvs"
    run_one "$BIN" "$2" persist3 "$OUT/persist3.nvs" 60
  fi
  if want fingerprint; then
    # determinism: two runs must emit bit-identical audio, LED frames and serial output
    f1=$(CP100_NVS="$OUT/fp1.nvs" "$BIN" fingerprint 2>&1 | grep FINGERPRINT)
    f2=$(CP100_NVS="$OUT/fp2.nvs" "$BIN" fingerprint 2>&1 | grep FINGERPRINT)
    rm -f "$OUT/fp1.nvs" "$OUT/fp2.nvs"
    if [ -n "$f1" ] && [ "$f1" = "$f2" ]; then echo "CHECK PASS fingerprint/deterministic: two runs ${f1#FINGERPRINT }" >> "$LOG"
    else echo "CHECK FAIL fingerprint/deterministic: '$f1' vs '$f2'" >> "$LOG"; fi
  fi
}

make -s -j8 -f sim.mk FW="$FW" BUILD="$BUILD" || exit 2
start=$(date +%s)
run_suite "$BUILD/cp100sim" "-O2"
if [ "$ASAN" = 1 ]; then
  make -s -j8 -f sim.mk FW="$FW" BUILD="$BUILD-asan" SANITIZE="-fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer" \
    CXXFLAGS="-std=c++17 -O1 -g -pthread -Wall -Wno-maybe-uninitialized" || exit 2
  echo "== AddressSanitizer + UBSan ==" >> "$LOG"
  mark=$(wc -l < "$LOG")
  ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1 run_suite "$BUILD-asan/cp100sim" "asan"
  grep -h "runtime error\|AddressSanitizer\|ThreadSanitizer" "$OUT"/*.log | sed 's/^/CHECK FAIL sanitizer\/report: /' >> "$LOG"
fi
end=$(date +%s)

summary() {  # $1 = first line, $2 = last line, $3 = label
  sed -n "$1,${2}p" "$LOG" | grep "^CHECK" > "$OUT/.part"
  p=$(grep -c "^CHECK PASS" "$OUT/.part"); f=$(grep -c "^CHECK FAIL" "$OUT/.part")
  echo "$3: $p passed, $f failed"
}
grep "^CHECK" "$LOG" | sed 's/^CHECK //' | awk '{ s=$1; $1=""; printf "%-5s%s\n", s, $0 }'
echo "----"
if [ "$ASAN" = 1 ]; then
  summary 1 "$mark" "normal (-O2)"
  summary "$((mark + 1))" '$' "ASan + UBSan"
fi
pass=$(grep -c "^CHECK PASS" "$LOG")
fail=$(grep -c "^CHECK FAIL" "$LOG")
echo "$pass passed, $fail failed in $((end - start)) s (full log: $LOG)"
[ "$fail" -eq 0 ]
