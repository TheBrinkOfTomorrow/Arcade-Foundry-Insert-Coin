#!/usr/bin/env python3
"""Pack Lindbergh and Chihiro game folders as one file each, for iCloud Drive and the ROM server.

  pack_games.py --roms DIR [--out DIR] [--sets hotd4a,vcop3,...]

A Lindbergh game is a folder (DIR/hotd4a/, the one that holds disk0); a
Chihiro game is the folder that holds its XBE (DIR/vcop3/vc3.xbe). The app
keeps either in iCloud Drive and fetches it from the ROM server as one
file, <set>.tar: a plain tar with the set's name as its top folder. This
writes <set>.tar beside every game folder in DIR or its board folders (or
OUT/<set>.tar); put the files next to your .zip ROM sets on the ROM
server, or in iCloud Drive > Arcade Foundry. The Mac app packs one itself
when you add a game folder with iCloud Drive on; build_cores.py and the
Linux app read a <set>.tar as it is.

A Chihiro game dumped as a .7z of its disc files (Chihiro/Virtua Cop 3.7z,
the XBE at the archive's top) is packed too: it is found by the XBE inside,
unpacked into a work folder beside it with 7zz, 7z, 7za or bsdtar, written
as <set>.tar and the work folder removed. The .7z is left as it is.

`tar -chf hotd4a.tar -C DIR hotd4a` makes the same file from a folder
(`h` follows symbolic links).
"""

import argparse
import os
import shutil
import subprocess
import sys
import tarfile

from game_sets import CHIHIRO, LINDBERGH
from rom_index import index, names, work_folder

SETS = list(LINDBERGH) + list(CHIHIRO)  # games.toml's Lindbergh and Chihiro games


def mark(s):
    """What a game folder of the set s holds at its top: disk0, or the Chihiro game's XBE."""
    return CHIHIRO.get(s, "disk0")


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


def unpack(tar_path, set_names, dst, required="disk0"):
    """Unpack <set>.tar into `dst` (the folder that holds `required`: disk0,
    or a Chihiro game's XBE); the set's name (any of set_names) as the top
    folder is optional. Refuses anything outside it."""
    tmp = dst + ".unpacking"
    shutil.rmtree(tmp, ignore_errors=True)
    os.makedirs(tmp)
    try:
        extract(tar_path, set_names, tmp, required)
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


def extract(tar_path, set_names, tmp, required="disk0"):
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
    if not os.path.exists(os.path.join(tmp, required)):
        sys.exit(f"{tar_path}: no {required}: pack the game folder that holds {required}")


SEVEN_ZIP_TOOLS = ("7zz", "7z", "7za")


def archive_names(archive):
    """The file names in a .7z (paths inside it), or None when no tool can list it."""
    for tool in SEVEN_ZIP_TOOLS:
        if shutil.which(tool):
            r = subprocess.run([tool, "l", "-slt", "-ba", archive], capture_output=True, text=True)
            if r.returncode == 0:
                return [l[7:] for l in r.stdout.splitlines() if l.startswith("Path = ")]
    if shutil.which("bsdtar"):
        r = subprocess.run(["bsdtar", "-tf", archive], capture_output=True, text=True)
        if r.returncode == 0:
            return r.stdout.splitlines()
    return None


def unpack_7z(archive, dst):
    """Unpack the .7z into the new folder dst with the first tool there is."""
    os.makedirs(dst)
    for tool in SEVEN_ZIP_TOOLS:
        if shutil.which(tool):
            subprocess.run([tool, "x", "-y", "-bd", f"-o{dst}", archive], check=True, stdout=subprocess.DEVNULL)
            return
    subprocess.run(["bsdtar", "-xf", archive, "-C", dst], check=True)


def chihiro_dumps(roms, wanted):
    """The Chihiro .7z dumps in roms or its board folders, by set: an archive
    whose top holds one of the games' XBEs ("Virtua Cop 3.7z" holds vc3.xbe)."""
    xbes = {CHIHIRO[s].lower(): s for s in wanted if s in CHIHIRO}
    found, listed = {}, False
    for here in [roms] + [os.path.join(roms, d) for d in sorted(os.listdir(roms))
                         if os.path.isdir(os.path.join(roms, d)) and not work_folder(d)]:
        for f in sorted(os.listdir(here)):
            if not f.lower().endswith(".7z") or f.startswith("."):
                continue
            path = os.path.join(here, f)
            inside = archive_names(path)
            if inside is None:
                if not listed:
                    print("pack_games: no 7zz, 7z, 7za or bsdtar to read .7z dumps with; skipped them. Install one:\n"
                          "  macOS: brew install sevenzip\n"
                          "  Debian, Ubuntu: sudo apt install 7zip (older: p7zip-full)\n"
                          "  Fedora: sudo dnf install 7zip    Arch, SteamOS: sudo pacman -S 7zip", file=sys.stderr)
                listed = True
                return found
            for name in inside:
                s = xbes.get(name.replace("\\", "/").lower())  # the XBE at the archive's top
                if s and s not in found:
                    found[s] = path
    return found


def main():
    ap = argparse.ArgumentParser(description="Pack Lindbergh and Chihiro game folders as <set>.tar")
    ap.add_argument("--roms", required=True, help="folder with the game folders (hotd4a/, vcop3/, ...) or Chihiro .7z dumps")
    ap.add_argument("--out", help="where the .tar files go (default: beside each game folder or dump)")
    ap.add_argument("--sets", default=",".join(SETS), help="comma-separated (default: all)")
    args = ap.parse_args()
    if args.out:
        os.makedirs(args.out, exist_ok=True)
    found = index(args.roms)  # the game folders, in board folders too (scripts/rom_index.py)
    wanted = [x for x in args.sets.split(",") if x]
    for s in wanted:
        if s not in SETS:
            sys.exit(f"pack_games: unknown set {s} (known: {', '.join(SETS)})")
    packed, from_folder = [], set()
    for s in wanted:
        # the folder under its name or an older one (or2sp/ for outr2sdx); the .tar keeps it
        name = next((n for n in names(s) if n in found and os.path.exists(os.path.join(found[n], mark(s)))), None)
        if not name:
            continue
        folder = found[name]
        dst = os.path.join(args.out or os.path.dirname(folder), f"{name}.tar")
        print(f"packing {folder} -> {dst}", flush=True)
        pack(folder, name, dst)
        packed.append(s)
        from_folder.add(s)
    rest = [s for s in wanted if s in CHIHIRO and s not in from_folder]
    for s, archive in chihiro_dumps(args.roms, rest).items():
        dst = os.path.join(args.out or os.path.dirname(archive), f"{s}.tar")
        if os.path.exists(dst) and os.path.getmtime(dst) >= os.path.getmtime(archive):
            print(f"{dst} is newer than {archive}: kept", flush=True)
            packed.append(s)
            continue
        work = os.path.join(os.path.dirname(dst), f"_unpacking-{s}")
        shutil.rmtree(work, ignore_errors=True)
        print(f"unpacking {archive}", flush=True)
        try:
            unpack_7z(archive, work)
            if not os.path.exists(os.path.join(work, mark(s))):
                sys.exit(f"pack_games: {archive} has no {mark(s)} at its top")
            print(f"packing it -> {dst}", flush=True)
            pack(work, s, dst)
        finally:
            shutil.rmtree(work, ignore_errors=True)
        packed.append(s)
    print(f"packed: {', '.join(packed) or 'nothing (no game folder holding disk0 or an XBE, and no Chihiro .7z)'}")


if __name__ == "__main__":
    main()
