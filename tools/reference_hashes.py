#!/usr/bin/env python3
"""Fingerprints of the trace written by the port while it replays the reference game (see tools/ref_run_check.sh).

The reference keeps only hashes of the trace, not the trace itself: the trace holds every sound register write of the game, which is the whole soundtrack.
Two streams are hashed, in blocks of BLOCK lines: the events (screen loads, palettes, sound starts, TODO lines) and the APU writes (with the sound requests).

  python tools/reference_hashes.py make  <trace.txt>                  prints the fingerprints (JSON) of a trace
  python tools/reference_hashes.py check <trace.txt> <expected.json>  compares a trace with the fingerprints; exit code 0 = identical
"""
import hashlib
import json
import re
import sys

BLOCK = 2000


def streams(path):
    events, apu = [], []
    for line in open(path, encoding="utf-8", errors="ignore"):
        line = line.rstrip("\r\n")
        if re.match(r"^f=[1-9]\d* (LoadScreen|LoadPalettePreset|SoundInit|TODO)", line):  # f=0: initialisation of the replay before the first frame
            events.append(line)
        elif re.match(r"^frame=([1-9]\d*) (APU|SoundPlay)", line):  # the first frame (frame=0) only exists in the replay
            apu.append(line)
    return {"events": events, "apu": apu}


def fingerprint(lines):
    hashes = [hashlib.sha256("\n".join(lines[i:i + BLOCK]).encode()).hexdigest() for i in range(0, len(lines), BLOCK)]
    return {"count": len(lines), "block": BLOCK, "hashes": hashes}


def make(path):
    return {name: fingerprint(lines) for name, lines in streams(path).items()}


def check(path, expected_path):
    expected = json.load(open(expected_path))
    got = streams(path)
    ok = True
    for name in ("events", "apu"):
        want, lines = expected[name], got[name]
        mine = fingerprint(lines)
        if mine == want:
            print("OK   %-8s %d identical lines" % (name, want["count"]))
            continue
        ok = False
        print("DIFF %-8s expected %d lines, replay %d" % (name, want["count"], len(lines)))
        for i, (a, b) in enumerate(zip(want["hashes"], mine["hashes"])):
            if a != b:
                first = i * want["block"]
                print("   first different block: lines %d to %d of the replay, for instance:" % (first, first + want["block"] - 1))
                for l in lines[first:first + 3]:
                    print("     ", l)
                break
    print("TODO lines in the replay:", len([l for l in got["events"] if "TODO" in l]))
    return ok


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "make":
        print(json.dumps(make(sys.argv[2]), indent=1))
    elif len(sys.argv) == 4 and sys.argv[1] == "check":
        sys.exit(0 if check(sys.argv[2], sys.argv[3]) else 1)
    else:
        sys.exit(__doc__)
