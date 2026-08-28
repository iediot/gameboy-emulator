#!/bin/bash
# sweeps a directory of test roms through the headless runner and tallies the verdicts.
# the suites themselves are not in this repo, grab them from
#   https://github.com/c-sp/game-boy-test-roms/releases
# and point this at one of the folders inside, for example
#   tools/run-tests.sh ~/gb-tests/mooneye-test-suite --bp
# --bp trusts the ld b,b breakpoint the mooneye and same-suite roms sign off with, which
# blargg's roms execute as ordinary code, so leave it off for those
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/build-release/gbtest"
DIR=${1:?usage: run-tests.sh <rom dir> [--bp] [--seconds n]}
shift
[[ -x "$BIN" ]] || { echo "build it first: cmake --build build-release --target gbtest"; exit 1; }

OUT=$(find "$DIR" \( -name "*.gb" -o -name "*.gbc" \) -print0 | sort -z |
      xargs -0 -P 8 -I{} "$BIN" {} "$@")
echo "$OUT" | grep -v "^PASS" | sed "s|$DIR/||"
echo
echo "$OUT" | awk '{print $1}' | sort | uniq -c
