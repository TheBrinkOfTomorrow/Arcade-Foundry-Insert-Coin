#!/usr/bin/env python3
"""Pack Lindbergh game folders as one file each, for iCloud Drive and the ROM server.

  pack_lindbergh.py --roms DIR [--out DIR] [--sets hotd4a,letsgoju,segartv,outr2sdx]

A Lindbergh game is a folder (DIR/hotd4a/, the one that holds disk0). The
app keeps it in iCloud Drive and fetches it from the ROM server as one
file, <set>.tar: a plain tar with the set's name as its top folder. This
writes <set>.tar beside every game folder in DIR or its board folders (or
OUT/<set>.tar); put
the files next to your .zip ROM sets on the ROM server, or in iCloud Drive >
Arcade Foundry. The Mac app packs one itself when you add a game folder
with iCloud Drive on; the Linux app plays a <set>.tar as it is.

`tar -chf hotd4a.tar -C DIR hotd4a` makes the same file (`h` follows
symbolic links).
"""

import argparse
import os
import shutil
import sys
import tarfile

from rom_index import index, names

SETS = ["hotd4a", "letsgoju", "segartv", "outr2sdx"]


def pack(folder, set_name, out):
    """Write the game folder `folder` as the tar `out`, `set_name` its top folder."""
    # symbolic links are followed: a game folder is often a link to where the
    # game was unpacked, and the tar has to carry the files themselves
    # and a link to nothing (the game's own absolute ones, /cf/X11R6/bin in
    # HotD4's driver folder) is left out
    tmp = out + ".part"
    root = os.path.realpath(folder)
    with tarfile.open(tmp, "w", format=tarfile.PAX_FORMAT, dereference=True) as tar:
        tar.add(root, arcname=set_name, recursive=False)
        for here, dirs, files in os.walk(root, followlinks=True):
            dirs.sort()
            for f in sorted(dirs + files):
                path = os.path.join(here, f)
                if os.path.islink(path) and not os.path.exists(path):
                    continue
                arc = os.path.join(set_name, os.path.relpath(path, root))
                tar.add(path, arcname=arc, recursive=False)
    os.replace(tmp, out)


def unpack(tar_path, set_names, dst):
    """Unpack <set>.tar into `dst` (the folder that holds disk0); the set's
    name (any of set_names) as the top folder is optional. Refuses anything
    outside it."""
    tmp = dst + ".unpacking"
    shutil.rmtree(tmp, ignore_errors=True)
    os.makedirs(tmp)
    try:
        extract(tar_path, set_names, tmp)
    except BaseException:
        shutil.rmtree(tmp, ignore_errors=True)
        raise
    shutil.rmtree(dst, ignore_errors=True)
    os.replace(tmp, dst)


def outside(name, target):
    """Whether the symbolic link `name` -> `target` leads out of the game folder."""
    if target.startswith("/"):
        return True
    return os.path.normpath(os.path.join(os.path.dirname(name), target)).split(os.sep)[0] == ".."


def extract(tar_path, set_names, tmp):
    with tarfile.open(tar_path) as tar:
        members = []
        for m in tar.getmembers():
            parts = [p for p in m.name.split("/") if p not in ("", ".")]
            if parts and parts[0].lower() in set_names:
                parts = parts[1:]
            if not parts:
                continue
            m.name = "/".join(parts)
            if m.issym() and outside(m.name, m.linkname):
                continue
            if m.islnk():
                link = [p for p in m.linkname.split("/") if p not in ("", ".")]
                if link and link[0].lower() in set_names:
                    link = link[1:]
                m.linkname = "/".join(link)
            members.append(m)
        if hasattr(tarfile, "data_filter"):
            tar.extractall(tmp, members=members, filter="data")
        else:
            root = os.path.realpath(tmp)
            for m in members:
                if m.name.startswith("/") or ".." in m.name.split("/"):
                    sys.exit(f"{tar_path}: a file outside the game folder: {m.name}")
                if not os.path.realpath(os.path.join(tmp, os.path.dirname(m.name))).startswith(root):
                    sys.exit(f"{tar_path}: a file outside the game folder: {m.name}")
                tar.extract(m, tmp)
    if not os.path.isdir(os.path.join(tmp, "disk0")):
        sys.exit(f"{tar_path}: no disk0 folder: pack the game folder that holds disk0")


def main():
    ap = argparse.ArgumentParser(description="Pack Lindbergh game folders as <set>.tar")
    ap.add_argument("--roms", required=True, help="folder with the game folders (hotd4a/, letsgoju/, ...)")
    ap.add_argument("--out", help="where the .tar files go (default: beside each game folder)")
    ap.add_argument("--sets", default=",".join(SETS), help="comma-separated (default: all)")
    args = ap.parse_args()
    if args.out:
        os.makedirs(args.out, exist_ok=True)
    found = index(args.roms)  # the game folders, in board folders too (scripts/rom_index.py)
    packed = []
    for s in [x for x in args.sets.split(",") if x]:
        if s not in SETS:
            sys.exit(f"pack_lindbergh: unknown set {s} (known: {', '.join(SETS)})")
        # the folder under its name or an older one (or2sp/ for outr2sdx); the .tar keeps it
        name = next((n for n in names(s) if n in found and os.path.isdir(os.path.join(found[n], "disk0"))), None)
        if not name:
            continue
        folder = found[name]
        dst = os.path.join(args.out or os.path.dirname(folder), f"{name}.tar")
        print(f"packing {folder} -> {dst}", flush=True)
        pack(folder, name, dst)
        packed.append(s)
    print(f"packed: {', '.join(packed) or 'nothing (no game folder holding disk0)'}")


if __name__ == "__main__":
    main()
