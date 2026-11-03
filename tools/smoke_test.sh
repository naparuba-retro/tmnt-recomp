#!/bin/bash
# Tests that need no ROM (they run in the continuous integration, where the ROMs are not available):
#   - tmnt_replay without arguments prints its usage;
#   - a file that is not a known ROM is refused with a clear message, and so is a missing file;
#   - tmnt without any ROM opens the help screen and creates the "rom" folder next to the executable.
# Usage : bash tools/smoke_test.sh [folder with tmnt and tmnt_replay]     (default : build)
cd "$(dirname "${BASH_SOURCE[0]}")/.." || exit 1
BIN=$(cd "${1:-build}" && pwd) || exit 1
EXT=""; case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) EXT=.exe;; esac
fail=0
step() { printf '%-58s' "$1"; }
ok() { echo "OK"; }
ko() { echo "FAILED"; fail=1; }
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

step "S1. tmnt_replay without arguments: usage, exit code 2"
out=$("$BIN/tmnt_replay$EXT" 2>&1); rc=$?
if [ $rc -eq 2 ] && echo "$out" | grep -qi "usage"; then ok; else ko; echo "   rc=$rc: $out" | head -3; fi

# a file that looks like an iNES image (header, 8 x 16 KB PRG, 16 x 8 KB CHR, mapper 1) but is not the game
{ printf 'NES\x1a\x08\x10\x10\x00'; head -c 8 /dev/zero; head -c $((8 * 16384 + 16 * 8192)) /dev/zero; } > "$WORK/fake.nes"

step "S2. tmnt_replay refuses a ROM that is not TMNT"
out=$("$BIN/tmnt_replay$EXT" "$WORK/fake.nes" "$WORK/trace.txt" 10 2>&1); rc=$?
if [ $rc -ne 0 ] && echo "$out" | grep -qi "not the European or American release"; then ok; else ko; echo "   rc=$rc: $out" | head -3; fi

step "S3. tmnt_replay refuses a missing file"
out=$("$BIN/tmnt_replay$EXT" "$WORK/missing.nes" "$WORK/trace.txt" 10 2>&1); rc=$?
if [ $rc -ne 0 ] && echo "$out" | grep -qi "cannot open"; then ok; else ko; echo "   rc=$rc: $out" | head -3; fi

# the game itself: no ROM -> help screen (needs a display: xvfb-run on a Linux machine without one)
mkdir -p "$WORK/game" && cp "$BIN/tmnt$EXT" "$WORK/game/"
RUN=""
if [ "$(uname -s)" = "Linux" ] && [ -z "$DISPLAY" ] && command -v xvfb-run >/dev/null; then RUN="xvfb-run -a"; fi
if [ "$(uname -s)" != "Linux" ] || [ -n "$DISPLAY" ] || [ -n "$RUN" ]; then
    step "S4. tmnt without ROM: help screen, creates ./rom"
    (cd "$WORK/game" && SDL_AUDIODRIVER=dummy timeout 20 $RUN ./tmnt$EXT >/dev/null 2>&1) # long enough for a slow virtual display to start
    rc=$?
    if [ -d "$WORK/game/rom" ] && [ $rc -eq 124 ]; then ok; else ko; echo "   rc=$rc (124 = still waiting for a key, as expected), rom folder: $([ -d "$WORK/game/rom" ] && echo yes || echo no)"; fi
else
    echo "S4. tmnt without ROM                                     skipped (no display)"
fi

echo
[ $fail -eq 0 ] && echo "smoke_test: ALL GREEN" || echo "smoke_test: FAILED"
exit $fail
