#!/usr/bin/env python3
"""Builds the port: MSVC (Visual Studio Build Tools) on Windows, gcc/cc + SDL2 on Linux.
  build/tmnt_replay[.exe] : windowless replay tool (traces, picture dumps), used by the regression tests
  build/tmnt[.exe]        : the game (window + keyboard + sound); Windows: GDI + waveOut, Linux: SDL2 (package libsdl2-dev)
Usage : python build.py [--rebuild] [--replay-only]
        python build.py --clean      removes build/ (objects, .map files, executables) and stops
  --rebuild     : removes build/ then builds everything
  --replay-only : builds tmnt_replay only (no SDL2 needed on Linux)"""
import glob
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
WINDOWS = os.name == "nt"
out = os.path.join(ROOT, "build")
if "--clean" in sys.argv or "--rebuild" in sys.argv:
    shutil.rmtree(out, ignore_errors=True)
    print("build removed")
    if "--clean" in sys.argv:
        sys.exit(0)
os.makedirs(out, exist_ok=True)
src = os.path.join(ROOT, "src")
# the entry points and platform layers are listed per executable below
NOT_COMMON = ("replay_main.c", "frontend.c", "help_screen.c", "platform_win.c", "platform_sdl.c")
common = [p for p in sorted(glob.glob(os.path.join(src, "*.c")) + glob.glob(os.path.join(src, "*", "*.c")))
          if os.path.basename(p) not in NOT_COMMON]
targets = [("tmnt_replay", ["replay_main.c"])]
if "--replay-only" not in sys.argv:
    targets.append(("tmnt", ["frontend.c", "help_screen.c", "platform_win.c" if WINDOWS else "platform_sdl.c"]))

if WINDOWS:
    VC = glob.glob(r"C:\Program Files*\Microsoft Visual Studio\*\*\VC\Auxiliary\Build\vcvars64.bat")
    if not VC:
        sys.exit("MSVC not found (vcvars64.bat): install the Visual Studio Build Tools")
    q = lambda files: " ".join('"%s"' % p for p in files)
    flags = "/nologo /std:c11 /W3 /O2 /MT /D_CRT_SECURE_NO_WARNINGS /I\"%s\"" % src
    for exe, mains in targets:
        objdir = os.path.join(out, exe)
        os.makedirs(objdir, exist_ok=True)
        libs = "user32.lib gdi32.lib winmm.lib xinput.lib" if exe == "tmnt" else ""
        cmd = 'call "%s" >nul && cl %s /Fo%s\\ /Fe"%s" %s %s %s' % (
            VC[0], flags, objdir, os.path.join(out, exe + ".exe"), q(common + [os.path.join(src, m) for m in mains]), "/link /MAP", libs)
        rc = subprocess.run(cmd, shell=True).returncode
        if rc:
            sys.exit(rc)
else:
    cc = os.environ.get("CC", "cc")
    flags = ["-std=c11", "-O2", "-Wall", "-Wextra", "-Wno-unused-parameter", "-I" + src]
    for exe, mains in targets:
        extra = []
        if exe == "tmnt":
            sdl = subprocess.run(["pkg-config", "--cflags", "--libs", "sdl2"], capture_output=True, text=True)
            if sdl.returncode:
                sys.exit("SDL2 not found (pkg-config sdl2): install libsdl2-dev, or use --replay-only")
            extra = sdl.stdout.split()
        cmd = [cc] + flags + common + [os.path.join(src, m) for m in mains] + ["-o", os.path.join(out, exe)] + extra + ["-lm"]
        rc = subprocess.run(cmd).returncode
        if rc:
            sys.exit(rc)
