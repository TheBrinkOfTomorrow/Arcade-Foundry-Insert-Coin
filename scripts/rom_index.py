"""The ROM sets in a folder: at its top, or one folder down in a folder per
board (DIR/Model2/srallyc.zip, DIR/Lindbergh/hotd4a.tar). Used by
build_cores.py, pack_lindbergh.py and serve_roms.py; the Linux app does the
same (linux/main.cpp).

An entry is a <name>.zip, .7z or .tar, or a <name>/ folder holding disk0 (a
Lindbergh game). In a board folder, a <name>/ folder holding one archive
(RingEdge/initd8/Initial D Arcade Stage 8 Infinity.7z) is that archive
under the folder's name (initd8.7z). Board folders are not searched
further down. Work folders
beside them are skipped: working, temp, to-file..., and any name starting
with "_" or "." (and __MACOSX). When a name turns up twice, the copy at the
top wins, then the first by path, with a warning.
"""

import os
import sys

EXTENSIONS = (".zip", ".7z", ".tar")
DEPTH = 1  # board folders, one down

# Other names a set's file may have: a file under any of these is the app's
# set. MAME's parents holding a clone's whole set (hotd.7z with hotdo's
# files, doa.zip with doab's), and the names the app gave the Lindbergh games
# before it took MAME's (or2sp, lgj, hotd4).
OTHER_NAMES = {"hotdo": ("hotd",), "doab": ("doa",), "outr2sdx": ("or2sp",), "letsgoju": ("lgj",),
               "hotd4a": ("hotd4",)}


def names(s):
    """The app's set s and the other names its files may have."""
    return (s, *OTHER_NAMES.get(s, ()))


def work_folder(name):
    """A folder beside the board folders that is not searched."""
    n = name.lower()
    return n in ("working", "temp", "__macosx") or n.startswith(("to-file", "_", "."))


def game_archive(folder, warn):
    """The one archive in a game's own folder, or None."""
    try:
        inside = [f for f in os.listdir(folder) if not f.startswith(".") and f.lower().endswith(EXTENSIONS)]
    except OSError:
        return None
    if len(inside) > 1 and warn:
        print(f"ROM folder: {folder} holds more than one archive ({', '.join(sorted(inside))}); skipped",
              file=sys.stderr)
    return os.path.join(folder, inside[0]) if len(inside) == 1 else None


def index(roms, warn=True):
    """{file or folder name (srallyc.zip, hotd4a): its path}."""
    found = {}  # name: [(depth, path)]
    roms = os.path.abspath(roms)
    for top, dirs, files in os.walk(roms):
        depth = 0 if top == roms else os.path.relpath(top, roms).count(os.sep) + 1
        keep = []
        for d in sorted(dirs):
            if work_folder(d):
                continue
            path = os.path.join(top, d)
            if os.path.isdir(os.path.join(path, "disk0")):
                found.setdefault(d, []).append((depth, path))
            elif depth < DEPTH:
                keep.append(d)
            elif depth == DEPTH:
                inner = game_archive(path, warn)
                if inner:
                    found.setdefault(d + os.path.splitext(inner)[1].lower(), []).append((depth, inner))
        dirs[:] = keep
        for f in files:
            if not f.startswith(".") and f.lower().endswith(EXTENSIONS):
                found.setdefault(f, []).append((depth, os.path.join(top, f)))
    chosen = {}
    for name, copies in sorted(found.items()):
        copies.sort()
        chosen[name] = copies[0][1]
        if warn and len(copies) > 1:
            others = ", ".join(os.path.relpath(p, roms) for _, p in copies[1:])
            print(f"ROM folder: {name} is in more than one place; using {os.path.relpath(copies[0][1], roms)}"
                  f" (not {others})", file=sys.stderr)
    return chosen


def link_folder(roms, dst):
    """A flat folder of links to every entry in roms (dst, made again each
    time), so a game's core finds its parent and device sets beside it
    wherever they are under roms. Returns dst."""
    if os.path.isdir(dst):
        for name in os.listdir(dst):
            p = os.path.join(dst, name)
            if os.path.islink(p) or os.path.isfile(p):
                os.remove(p)
    os.makedirs(dst, exist_ok=True)
    if os.path.isdir(roms):
        for name, path in index(roms).items():
            os.symlink(path, os.path.join(dst, name))
    return dst
