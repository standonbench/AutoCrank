#!/bin/sh
# ---------------------------------------------------------------------------
# Headless mechanics-test runner.
#
# Builds the game logic + tools/hosttest.c on host (clang) and runs
# selftest_run() — the same suite the in-Simulator TEST screen runs — straight
# from the command line. No Playdate Simulator required.
#
# Exit 0 = every self-test passed; non-zero = at least one FAIL (or build err).
# Usage:  sh tools/runtests.sh
# ---------------------------------------------------------------------------
set -e
cd "$(dirname "$0")/.."

SDK="${PLAYDATE_SDK_PATH}"
if [ -z "$SDK" ]; then
  SDK=$(egrep '^[[:space:]]*SDKRoot' "$HOME/.Playdate/config" 2>/dev/null | head -n1 | cut -c9-)
fi
if [ -z "$SDK" ] || [ ! -d "$SDK/C_API" ]; then
  echo "ERROR: Playdate SDK not found (set PLAYDATE_SDK_PATH or ~/.Playdate/config)"
  exit 2
fi

mkdir -p build
# Every game translation unit EXCEPT main.c (whose eventHandler/pd the harness
# replaces). render/ui/images/sound/creatures compile + link fine but their
# pd-dependent code is never reached on the self-test path.
SRC=$(ls src/*.c | grep -v '/main\.c$')

clang -std=gnu11 -g -O1 \
  -DTARGET_EXTENSION=1 -DTARGET_SIMULATOR=1 -DDEBUG_BUILD=1 \
  -Wno-unknown-pragmas \
  -I src -I "$SDK/C_API" \
  $SRC tools/hosttest.c -lm -o build/hosttest

set +e
OUT=$(./build/hosttest)
printf '%s\n' "$OUT"
echo "----"
FAILS=$(printf '%s\n' "$OUT" | grep -c '^FAIL ')
if [ "$FAILS" -eq 0 ]; then
  echo "HOSTTEST: all self-tests passed"
  exit 0
else
  echo "HOSTTEST: $FAILS self-test(s) FAILED"
  exit 1
fi
