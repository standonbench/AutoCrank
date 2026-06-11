#!/bin/sh
# ---------------------------------------------------------------------------
# Headless balance-report runner.
#
# Builds the game logic + tools/balance.c on host (clang) and runs thousands
# of instant battle sims to print the difficulty-curve and per-pet-power
# reports. No Playdate Simulator required. See tools/balance.c for details.
#
# Usage:  sh tools/balance.sh          # all reports
#         sh tools/balance.sh curve    # difficulty curve only
#         sh tools/balance.sh pets     # per-pet power (random teams) only
#         sh tools/balance.sh synergy  # archetype round-robin (support decks)
#         sh tools/balance.sh runs     # full-run Monte Carlo (policy pilot)
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
# Same set as the test harness: every TU except main.c (whose pd/eventHandler
# tools/balance.c replaces with a stub).
SRC=$(ls src/*.c | grep -v '/main\.c$')

clang -std=gnu11 -g -O2 \
  -DTARGET_EXTENSION=1 -DTARGET_SIMULATOR=1 -DDEBUG_BUILD=1 \
  -Wno-unknown-pragmas \
  -I src -I "$SDK/C_API" \
  $SRC tools/balance.c -lm -o build/balance

./build/balance "$@"
