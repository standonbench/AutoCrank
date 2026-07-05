#!/bin/zsh
# Build AutoCrank and open it in the Playdate Simulator.
# Double-click me, or run: ./RUN_AUTOCRANK.command
cd "$(dirname "$0")"
echo "=== building AutoCrank ==="
if make run; then
    echo "=== build OK — Simulator launching ==="
else
    echo ""
    echo "*** BUILD FAILED — see errors above ***"
fi
