#!/bin/bash
# Regression test: replays the reference game (whole game, invincible mode, European ROM) in the port alone and compares the trace it writes with the
# fingerprints (hashes) of the original trace kept in tests/reference_game/expected_hashes.json.
# Needs the European ROM as rom/tmnt_eu.nes (the game itself accepts any file name, this script uses that one).
# Usage : bash tools/ref_run_check.sh        (a few seconds; prints OK or the first different block, exit code 0 = identical)
#         REF=<folder> bash tools/ref_run_check.sh   to replay another reference game, EXE=<replay tool> to use another build
cd "$(dirname "${BASH_SOURCE[0]}")/.." || exit 1
PY=$(command -v python || command -v python3)
EXT=""; case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) EXT=.exe;; esac
R=${REF:-tests/reference_game}
T=build/refrun; mkdir -p $T/d
timeout 900 ${EXE:-build/tmnt_replay$EXT} rom/tmnt_eu.nes $T/trace.txt 127530 $T/d 100000 0 "" "" $R/tmnt_input.txt $R/tmnt_pokes.txt "" "" "" $R/tmnt_rewind.txt >/dev/null 2>&1 || { echo "FAILED: the replay stopped (code $?)"; exit 2; }
$PY tools/reference_hashes.py check $T/trace.txt $R/expected_hashes.json
