#!/usr/bin/env python3
"""Build the game cores Arcade Foundry: Insert Coin embeds, from your own ROM sets.

  build_cores.py --roms DIR [--sets SET,SET...] [--boards BOARD,BOARD...]
                 [--platforms macosx,iphoneos,iphonesimulator,appletvos,appletvsimulator | linux,linux-arm64]
                 [--model3 DIR] [--lindbergh DIR] [--lindbergh-renderer metal|gl] [--classic2d DIR] [--chihiro DIR]
                 [--jobs N] [--force] [--status]

--boards builds every set of those boards: a board (model2b, xboard) or a
library platform (model2 is Model 2, 2A, 2B and 2C; classic2d is every 2D
board). With --sets too, it builds both. --boards list lists the names it
takes; --sets list lists every set by board, oldest board first, each
board's games by year. Neither needs --roms.

A core that is already built from the same ROM set, the same recompiler
commit and the same version of this script is skipped (a stamp file next to
it in build/cores/<platform>/ records what it was built from); --force
rebuilds anyway. --status lists each core as up to date, out of date or
missing without building anything. With uncommitted changes in the
recompiler, nothing is skipped.

DIR may keep the sets in a folder per board, one level down
(DIR/Model2/srallyc.zip; work folders such as working/, temp/ and
to-file.../ are skipped, see scripts/rom_index.py): they are found by
name, and linked into build/roms/ so each one has its parent and device
sets (model1io2.zip, bassdx.zip) beside it, whichever board folder those
are in. If a name is in two places, the one at the top of DIR (then the
first board folder by name) is used, with a warning.

For every set with a ROM set in DIR (<set>.zip or <set>.7z), the recompiler
in cores/arcade-foundry-cores-model12 generates that game's C++ from the ROMs and
builds it as a core library (common/include/afcore.h). The result goes to
build/cores/<platform>/ (Xcode's platform names; the app's build copies
from the one it is built for): afcore_<set>.framework on Apple platforms,
libafcore_<set>.so on Linux. On a Mac the default is macosx only; the
iOS and tvOS ones are cross-built from the same generated code. On an
x86-64 Linux, --platforms linux-arm64 cross-builds arm64 Linux cores
(build/cores/linux-arm64/) with linux/arm64/aarch64-toolchain.cmake;
scripts/build_arm64.py does that, or a native arm64 build, in a container. Everything generated stays under build/ (git-ignored): never
commit it, it is derived from the ROMs.

The sets are games.toml's (scripts/game_sets.py lists them by core).

Model 3 sets come from Arcade-Foundry-Cores-Model3 (GPL-3, the submodule
cores/arcade-foundry-cores-model3, or another checkout with --model3). They
are not recompiled from the ROMs: the core is Supermodel's machine. On
Apple platforms it draws with Metal and needs nothing beyond the system
(iOS and tvOS ones are built with CMake's Xcode generator); on Linux it
uses desktop OpenGL, SDL2 and SDL2_net. Each Apple framework carries the
GPL's text (COPYING).

Classic 2D sets (the Out Run board, the X Board, System 32, the Hang-On board) come from Arcade-Foundry-Cores-Classic2D
(the submodule cores/arcade-foundry-cores-classic2d, or another checkout
with --classic2d): its scripts/recompile.py checks the set, translates the
68000 programs (System 32: the V60 program) and builds the core; iOS, tvOS and arm64 Linux are built
from the code it generates on the host.

Lindbergh sets come from Arcade-Foundry-Cores-Lindbergh (the submodule
cores/arcade-foundry-cores-lindbergh, or another checkout with --lindbergh).
Their "ROM set" is the game's folder in DIR, named after the set
(DIR/hotd4a/disk0/...), or the same packed as DIR/hotd4a.tar
(scripts/pack_games.py; unpacked under build/lindbergh-games/): its
programs are translated at build time. macOS, Linux and iOS
(iphoneos, iphonesimulator; not tvOS, which has no way to give a game its
4 GB address window). The build needs a Python with capstone and pyelftools
(the shared venv ~/venvs/afic, see translator_python); macOS and Linux need
SDL2's headers.
macOS and iOS cores draw on Metal: gl4es (desktop OpenGL on OpenGL ES,
fetched through CMake) on Cores-Common's mtlgles. --lindbergh-renderer gl
builds the old path instead (Apple's OpenGL ES on iOS, desktop OpenGL on
macOS), kept for one release. iOS builds use Ninja.

Chihiro sets come from Arcade-Foundry-Cores-Chihiro (the submodule
cores/arcade-foundry-cores-chihiro, or another checkout with --chihiro).
Their "ROM set" is the folder in DIR that holds the game's XBE, named after
the set (DIR/vcop3/vc3.xbe), or the same packed as DIR/vcop3.tar
(scripts/pack_games.py, which also turns a .7z dump into one; unpacked
under build/chihiro-games/); the XBE is translated at build time. macOS
and iOS (iphoneos, iphonesimulator; Metal) and Linux (OpenGL); not tvOS,
for the same reason as Lindbergh. The build needs a Python with
capstone and pyelftools (the shared venv ~/venvs/afic).
"""

import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys

from rom_index import link_folder, names

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RECOMP = os.path.join(ROOT, "cores", "arcade-foundry-cores-model12")
COMMON = os.path.join(ROOT, "cores", "arcade-foundry-cores-common")  # afcore.h, shared code
# The sets, and which core repo builds each: scripts/game_sets.py, written by
# scripts/gen_games.py from games.toml (the app's and the cores'). SETS in the
# app's order; MODEL1 (the recompiler's Model 1 build, CMake M1_CORE), MODEL3
# (CMake M3CORE_ROMSET), CLASSIC2D (scripts/recompile.py, CMake C2D_ROMSET),
# LINDBERGH (CMake LB_CORE: the game's program and, when it is a program of its
# own, the test menu's, under the game folder); the rest are Model 2.
# CHIHIRO (CMake CH_CORE: the game's XBE in the game folder).
from game_sets import BOARDS, CHIHIRO, CLASSIC2D, GAMES, HIDDEN, LINDBERGH, MODEL1, MODEL3, SETS  # noqa: E402

# The library's platforms (model2, classic2d, ...): --boards takes these too,
# each meaning all of its boards.
PLATFORM_NAMES = {"classic2d": "Classic 2D", "model1": "Model 1", "model2": "Model 2",
                  "model3": "Model 3", "lindbergh": "Lindbergh", "chihiro": "Chihiro"}


def board_sets(name):
    """The sets of a board, or of every board of a platform; None for an unknown name."""
    boards = {b for b, (plat, _) in BOARDS.items() if b == name or plat == name}
    if not boards:
        return None
    return [s for s in SETS if GAMES[s][2] in boards]


def list_boards():
    """--boards list: the names --boards takes."""
    print("Platforms (all of their boards):")
    for plat, title in PLATFORM_NAMES.items():
        boards = [b for b, (p, _) in BOARDS.items() if p == plat]
        if boards:
            print(f"  {plat:10} {title:12} {len(board_sets(plat)):3} sets  ({', '.join(boards)})")
    print("Boards, oldest first:")
    for b, (_, title) in BOARDS.items():
        if b not in PLATFORM_NAMES:
            print(f"  {b:10} {title:12} {len(board_sets(b)):3} sets")


def list_sets():
    """--sets list: every set by board, oldest board first, each board's games by year."""
    for b, (plat, title) in BOARDS.items():
        sets = sorted((s for s in SETS if GAMES[s][2] == b), key=lambda s: (GAMES[s][0], GAMES[s][1].removeprefix("The ")))
        print(f"{title} ({b}, {len(sets)} sets)")
        for s in sets:
            year, name, _ = GAMES[s]
            print(f"  {year}  {s:11} {name}")


def run(cmd, **kw):
    print("+", " ".join(cmd), flush=True)
    subprocess.run(cmd, check=True, **kw)


# Linux platforms cross-built from an x86-64 Linux: the CMake toolchain file.
LINUX_CROSS = {"linux-arm64": os.path.join(ROOT, "linux", "arm64", "aarch64-toolchain.cmake")}


def is_linux(plat):
    return plat == "linux" or plat in LINUX_CROSS


# Apple platforms cross-built from a Mac: CMake system, SDK, minimum OS.
CROSS = {
    "iphoneos": ("iOS", "iphoneos", "16.0"),
    "iphonesimulator": ("iOS", "iphonesimulator", "16.0"),
    "appletvos": ("tvOS", "appletvos", "16.0"),
    "appletvsimulator": ("tvOS", "appletvsimulator", "16.0"),
}


def interface_version(header):
    """The core interface x.y.z an afcore.h declares, or None."""
    try:
        with open(header) as f:
            text = f.read()
    except OSError:
        return None
    parts = [re.search(rf"#define AFCORE_API_{k}\s+(\d+)", text) for k in ("MAJOR", "MINOR", "PATCH")]
    return ".".join(m.group(1) for m in parts) if all(parts) else None


def publish(build, s, plat, header=None):
    """Copy a built core into build/cores/<plat>/. An Apple framework gets
    afcore-interface, the interface version it was built for (from `header`,
    the core repo's afcore.h): the app reads it to tell an out-of-date core
    without opening it."""
    name = f"libafcore_{s}.so" if is_linux(plat) else f"afcore_{s}.framework"
    out = os.path.join(ROOT, "build", "cores", plat)
    os.makedirs(out, exist_ok=True)
    dst = os.path.join(out, name)
    if os.path.isdir(dst):
        shutil.rmtree(dst)
    elif os.path.exists(dst):
        os.remove(dst)
    src = os.path.join(build, name)
    if os.path.isdir(src):
        shutil.copytree(src, dst, symlinks=True)
    else:
        shutil.copy2(src, dst)
    version = interface_version(header or os.path.join(RECOMP, "common", "include", "afcore.h"))
    if not is_linux(plat) and version:
        # The Mac's framework keeps its files in Versions/A/Resources; iOS and tvOS ones are flat.
        if os.path.isdir(os.path.join(dst, "Versions", "A")):
            resources = os.path.join(dst, "Versions", "A", "Resources")
            os.makedirs(resources, exist_ok=True)
            if not os.path.lexists(os.path.join(dst, "Resources")):
                os.symlink(os.path.join("Versions", "Current", "Resources"), os.path.join(dst, "Resources"))
        else:
            resources = dst
        with open(os.path.join(resources, "afcore-interface"), "w") as f:
            f.write(version + "\n")


def drop_old_cores(plat):
    """Remove cores built before the interface was renamed afcore (5.0.0):
    m2core_<set>.framework / libm2core_<set>.so, which no app loads now."""
    out = os.path.join(ROOT, "build", "cores", plat)
    if not os.path.isdir(out):
        return
    for name in os.listdir(out):
        if name.startswith(("m2core_", "libm2core_", ".m2core_")):
            path = os.path.join(out, name)
            print(f"build_cores: removing {path} (from before afcore 5.0.0)")
            shutil.rmtree(path) if os.path.isdir(path) and not os.path.islink(path) else os.remove(path)


def check_headers(*repos):
    """Each core repo has Arcade-Foundry-Cores-Common as its submodule common/,
    and the app has it as cores/arcade-foundry-cores-common: an afcore.h that
    differs from the app's means that core's common/ is pinned elsewhere."""
    ref = os.path.join(COMMON, "include", "afcore.h")
    for repo in repos:
        copy = os.path.join(repo, "common", "include", "afcore.h")
        if os.path.exists(ref) and os.path.exists(copy) and file_sha1(ref) != file_sha1(copy):
            print(f"build_cores: warning: {copy} differs from {ref}; move {repo}'s common/ to the app's commit")


def stamp_path(s, plat):
    return os.path.join(ROOT, "build", "cores", plat, f".afcore_{s}.stamp")


def core_path(s, plat):
    name = f"libafcore_{s}.so" if is_linux(plat) else f"afcore_{s}.framework"
    return os.path.join(ROOT, "build", "cores", plat, name)


def file_sha1(path):
    h = hashlib.sha1()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def recompiler_state(src=RECOMP):
    """The checkout's commit, or None when it has uncommitted changes (never skip then)."""
    try:
        commit = subprocess.run(["git", "rev-parse", "HEAD"], cwd=src, check=True, capture_output=True,
                                text=True).stdout.strip()
        dirty = subprocess.run(["git", "status", "--porcelain", "--untracked-files=no"], cwd=src, check=True,
                               capture_output=True, text=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return None
    return None if dirty else commit


def wanted_stamp(rom, commit, plat):
    """What a core built now would be built from: ROM set, recompiler, this script, platform."""
    if commit is None:
        return None
    return "\n".join([f"rom {file_sha1(rom)}", f"recompiler {commit}",
                      f"script {file_sha1(os.path.abspath(__file__))}", f"platform {plat}"]) + "\n"


def up_to_date(s, plat, stamp):
    if stamp is None or not os.path.exists(core_path(s, plat)):
        return False
    try:
        with open(stamp_path(s, plat)) as f:
            return f.read() == stamp
    except OSError:
        return False


def write_stamp(s, plat, stamp):
    if stamp is not None:
        with open(stamp_path(s, plat), "w") as f:
            f.write(stamp)


def bundle_libraries(fw):
    """Copy the non-system libraries a Mac framework links (Homebrew's SDL2
    and SDL2_net) into it, point it at the copies and sign them ad hoc, so
    the framework runs inside the app (and its sandbox) on its own."""
    binary = os.path.join(fw, "Versions", "A", os.path.basename(fw)[:-len(".framework")])
    libs = os.path.join(fw, "Versions", "A", "Libraries")

    def deps(path):
        out = subprocess.run(["otool", "-L", path], check=True, capture_output=True, text=True).stdout
        return [line.split(" (")[0].strip() for line in out.splitlines()[1:]]

    todo, done = [binary], set()
    while todo:
        path = todo.pop()
        for dep in deps(path):
            if not dep.startswith(("/opt/homebrew/", "/usr/local/")):
                continue
            name = os.path.basename(dep)
            copy = os.path.join(libs, name)
            if name not in done:
                os.makedirs(libs, exist_ok=True)
                shutil.copy2(os.path.realpath(dep), copy)
                os.chmod(copy, 0o755)
                run(["install_name_tool", "-id", f"@loader_path/{name}", copy])
                done.add(name)
                todo.append(copy)
            ref = f"@loader_path/{name}" if path != binary else f"@loader_path/Libraries/{name}"
            run(["install_name_tool", "-change", dep, ref, path])
    for name in sorted(done):
        run(["codesign", "--force", "--sign", "-", os.path.join(libs, name)])
    run(["codesign", "--force", "--sign", "-", fw])


def build_model3(src, s, rom, plat, jobs, env):
    """Build a Model 3 core from Arcade-Foundry-Cores-Model3 and publish it."""
    build = os.path.join(ROOT, "build", "cores-src", s if plat in ("macosx", "linux") else f"{s}-{plat}")
    # Build from scratch: a cache from an older Arcade-Foundry-Cores-Model3 kept
    # Homebrew's SDL2_net linked after the core stopped using SDL on Apple,
    # and its renderer choice (OpenGL or Metal) is cached the same way.
    shutil.rmtree(build, ignore_errors=True)
    if plat in CROSS:
        # iOS and tvOS: Metal only, no SDL or OpenGL; zlib from the SDK. Built
        # with Xcode's generator, which puts the framework in Release-<sdk>/.
        system, sdk, target = CROSS[plat]
        run(["cmake", "-S", src, "-B", build, "-G", "Xcode", f"-DCMAKE_SYSTEM_NAME={system}",
             f"-DCMAKE_OSX_DEPLOYMENT_TARGET={target}", "-DCMAKE_OSX_ARCHITECTURES=arm64",
             f"-DM3CORE_ROMSET={s}", "-DM3CORE_FRAMEWORK=ON"], env=env)
        run(["cmake", "--build", build, "--target", f"afcore_{s}", "--config", "Release", "--",
             "-sdk", sdk, "-jobs", str(jobs), "CODE_SIGNING_ALLOWED=NO"], env=env)
        shutil.copytree(os.path.join(build, f"Release-{sdk}", f"afcore_{s}.framework"),
                        os.path.join(build, f"afcore_{s}.framework"), symlinks=True)
    else:
        toolchain = [f"-DCMAKE_TOOLCHAIN_FILE={LINUX_CROSS[plat]}"] if plat in LINUX_CROSS else []
        run(["cmake", "-S", src, "-B", build, *toolchain, "-DCMAKE_BUILD_TYPE=Release", f"-DM3CORE_ROMSET={s}",
             "-DM3CORE_FRAMEWORK=" + ("ON" if plat == "macosx" else "OFF")], env=env)
        run(["cmake", "--build", build, "--parallel", str(jobs), "--target", f"afcore_{s}"], env=env)
    if not is_linux(plat):
        fw = os.path.join(build, f"afcore_{s}.framework")
        # iOS and tvOS frameworks are flat; the Mac's has Versions/A.
        resources = fw if plat in CROSS else os.path.join(fw, "Versions", "A", "Resources")
        os.makedirs(resources, exist_ok=True)
        # The GPL's text travels with the core.
        shutil.copy2(os.path.join(src, "Docs", "LICENSE.txt"), os.path.join(resources, "COPYING"))
        if plat == "macosx":
            # Libraries an earlier build bundled (relinking leaves them behind).
            shutil.rmtree(os.path.join(fw, "Versions", "A", "Libraries"), ignore_errors=True)
            bundle_libraries(fw)
    publish(build, s, plat, os.path.join(src, "common", "include", "afcore.h"))


# Lindbergh cores build for these cross platforms (tvOS has no extended
# virtual addressing, which the 4 GB guest window needs)
LINDBERGH_CROSS = {"iphoneos", "iphonesimulator"}


def translator_python():
    """The Python that runs the x86 translator (Lindbergh, Chihiro): the venv
    this script runs under, else the one shared venv (AFIC_VENV, default
    ~/venvs/afic; Cores-Common's tools/afic-venv/setup.sh makes it), else this
    script's own Python. Never a venv inside a core checkout."""
    if sys.prefix != sys.base_prefix:
        return sys.executable
    shared = os.path.join(os.environ.get("AFIC_VENV") or os.path.expanduser("~/venvs/afic"), "bin", "python3")
    return shared if os.path.exists(shared) else sys.executable


def build_lindbergh(src, s, folder, plat, jobs, env, renderer="metal"):
    """Build a Lindbergh core from Arcade-Foundry-Cores-Lindbergh and publish it.
    renderer: on Apple, "metal" (gl4es on Cores-Common's mtlgles) or "gl" (the
    old path: Apple's OpenGL ES on iOS, desktop OpenGL on macOS)."""
    build = os.path.join(ROOT, "build", "cores-src", s if plat in ("macosx", "linux") else f"{s}-{plat}")
    drop_foreign_cache(build, src)
    gl_xml = os.path.join(src, "extern", "gl.xml")
    if not os.path.exists(gl_xml):
        os.makedirs(os.path.dirname(gl_xml), exist_ok=True)
        run(["curl", "-fsSL", "-o", gl_xml,
             "https://raw.githubusercontent.com/KhronosGroup/OpenGL-Registry/main/xml/gl.xml"])
    python = translator_python()
    game, test = LINDBERGH[s]
    # iOS: the core through gl4es (the core's CMake fetches it), with Ninja
    cross = []
    if plat in CROSS:
        system, sysroot, target = CROSS[plat]
        cross = ["-G", "Ninja", f"-DCMAKE_SYSTEM_NAME={system}", f"-DCMAKE_OSX_SYSROOT={sysroot}",
                 "-DCMAKE_OSX_ARCHITECTURES=arm64", f"-DCMAKE_OSX_DEPLOYMENT_TARGET={target}"]
    run(["cmake", "-S", src, "-B", build, *cross, "-DCMAKE_BUILD_TYPE=Release", f"-DGAME={s}", f"-DCORE_ROMSET={s}",
         f"-DGAME_ELF={os.path.join(folder, game)}", f"-DGAME_TEST_ELF={os.path.join(folder, test) if test else ''}",
         "-DLB_CORE=ON", "-DLB_RUN=OFF", f"-DPython3_EXECUTABLE={python}",
         # explicit, so a build folder configured before Metal became the default follows
         *([f"-DLB_MTLGLES={'ON' if renderer == 'metal' else 'OFF'}"] if plat != "linux" else [])], env=env)
    run(["cmake", "--build", build, "--parallel", str(jobs), "--target", f"afcore_{s}"], env=env)
    if plat == "macosx":
        bundle_libraries(os.path.join(build, f"afcore_{s}.framework"))
    publish(build, s, plat, os.path.join(src, "common", "include", "afcore.h"))


# Chihiro cores cross-build for iOS (iPadOS) only: like Lindbergh's, the game
# needs a 4 GB address window, which tvOS cannot give an app
CHIHIRO_CROSS = {"iphoneos", "iphonesimulator"}


def build_chihiro(src, s, folder, plat, jobs, env):
    """Build a Chihiro core from Arcade-Foundry-Cores-Chihiro and publish it."""
    build = os.path.join(ROOT, "build", "cores-src", s if plat in ("macosx", "linux") else f"{s}-{plat}")
    drop_foreign_cache(build, src)
    python = translator_python()
    # iOS: the same Metal renderer, the core only (its CMake turns chihiro-run off)
    cross = []
    if plat in CROSS:
        system, sysroot, target = CROSS[plat]
        cross = ["-G", "Ninja", f"-DCMAKE_SYSTEM_NAME={system}", f"-DCMAKE_OSX_SYSROOT={sysroot}",
                 "-DCMAKE_OSX_ARCHITECTURES=arm64", f"-DCMAKE_OSX_DEPLOYMENT_TARGET={target}"]
    run(["cmake", "-S", src, "-B", build, *cross, "-DCMAKE_BUILD_TYPE=Release", f"-DGAME={s}", f"-DCORE_ROMSET={s}",
         f"-DGAME_XBE={os.path.join(folder, CHIHIRO[s])}", "-DCH_CORE=ON", "-DCH_RUN=OFF",
         f"-DPython3_EXECUTABLE={python}"], env=env)
    run(["cmake", "--build", build, "--parallel", str(jobs), "--target", f"afcore_{s}"], env=env)
    publish(build, s, plat, os.path.join(src, "common", "include", "afcore.h"))


def build_classic2d(src, s, rom, plat, jobs, env):
    """Build a Classic 2D core from Arcade-Foundry-Cores-Classic2D and publish
    it. The host build (macOS, Linux) generates the code; the others are
    cross-built from it."""
    host_build = os.path.join(ROOT, "build", "cores-src", s)
    drop_foreign_cache(host_build, src)
    recompile = [sys.executable, os.path.join(src, "scripts", "recompile.py"), "--set", s, "--rom", rom,
                 "--build-dir", host_build, "--jobs", str(jobs)]
    if plat in ("macosx", "linux"):
        run(recompile + ["--core"], env=env)
        publish(host_build, s, plat, os.path.join(src, "common", "include", "afcore.h"))
        return
    if not any(os.path.exists(os.path.join(host_build, "gen", s, f)) for f in ("gen_main.cpp", "table.cpp")):
        run(recompile + ["--gen-only"], env=env)
    build = os.path.join(ROOT, "build", "cores-src", f"{s}-{plat}")
    drop_foreign_cache(build, src)
    if plat in LINUX_CROSS:
        system = [f"-DCMAKE_TOOLCHAIN_FILE={LINUX_CROSS[plat]}"]
    else:
        name, sysroot, target = CROSS[plat]
        system = [f"-DCMAKE_SYSTEM_NAME={name}", f"-DCMAKE_OSX_SYSROOT={sysroot}",
                  "-DCMAKE_OSX_ARCHITECTURES=arm64", f"-DCMAKE_OSX_DEPLOYMENT_TARGET={target}"]
    run(["cmake", "-S", src, "-B", build, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", *system, f"-DC2D_ROMSET={s}",
         "-DC2D_TOOLS=OFF", "-DC2D_GEN_ROOT=" + os.path.join(host_build, "gen")], env=env)
    run(["cmake", "--build", build, "--parallel", str(jobs), "--target", f"afcore_{s}"], env=env)
    publish(build, s, plat, os.path.join(src, "common", "include", "afcore.h"))


# Sets whose split .zip lacks the parent's files: the .7z (the whole set) first
SEVEN_Z_FIRST = {"overrevb"}


def find_rom(roms, s):
    """The ROM set for s in roms, under its own name or another
    (rom_index.OTHER_NAMES: hotd.7z for hotdo, the older or2sp.tar for outr2sdx)."""
    if s in LINDBERGH:
        for name in names(s):
            folder = os.path.join(roms, name)
            if os.path.exists(os.path.join(folder, LINDBERGH[s][0])):
                return folder
        for name in names(s):
            if os.path.exists(os.path.join(roms, name + ".tar")):
                return unpacked_game(os.path.join(roms, name + ".tar"), s)
        return None
    if s in CHIHIRO:
        for name in names(s):
            folder = os.path.join(roms, name)
            if os.path.exists(os.path.join(folder, CHIHIRO[s])):
                return folder
        for name in names(s):
            if os.path.exists(os.path.join(roms, name + ".tar")):
                return unpacked_game(os.path.join(roms, name + ".tar"), s)
        return None
    for name in names(s):
        for ext in ("7z", "zip") if s in SEVEN_Z_FIRST else ("zip", "7z"):
            p = os.path.join(roms, name + "." + ext)
            if os.path.exists(p):
                return p
    return None


def unpacked_game(tar, s):
    """A Lindbergh or Chihiro game packed as <set>.tar: unpacked once under
    build/ (again when the .tar changes); its folder, or None."""
    if not os.path.exists(tar):
        return None
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from pack_games import unpack
    required = CHIHIRO[s] if s in CHIHIRO else LINDBERGH[s][0]
    dst = os.path.join(ROOT, "build", "chihiro-games" if s in CHIHIRO else "lindbergh-games", s)
    st = os.stat(tar)
    mark, want = dst + ".from", f"{os.path.abspath(tar)} {st.st_size} {st.st_mtime_ns}"
    have = open(mark).read() if os.path.exists(mark) else ""
    if have != want or not os.path.exists(os.path.join(dst, required)):
        print(f"build_cores: unpacking {tar}", flush=True)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        unpack(tar, names(s), dst, required)
        with open(mark, "w") as f:
            f.write(want)
    return dst if os.path.exists(os.path.join(dst, required)) else None


def drop_foreign_cache(build, source):
    """Remove a build folder whose CMake cache was made from another source tree
    (for example the cores submodule's old path), which CMake refuses to reuse."""
    cache = os.path.join(build, "CMakeCache.txt")
    if not os.path.exists(cache):
        return
    with open(cache, errors="replace") as f:
        for line in f:
            if line.startswith("CMAKE_HOME_DIRECTORY:"):
                if os.path.realpath(line.split("=", 1)[1].strip()) != os.path.realpath(source):
                    print(f"build_cores: {build} was configured from another source, starting it fresh")
                    shutil.rmtree(build, ignore_errors=True)
                return


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--roms", help="folder with your MAME ROM sets (or its board folders)")
    ap.add_argument("--sets", help="comma-separated (default: all, or --boards's); list: list every set by board")
    ap.add_argument("--boards", help="comma-separated boards or platforms (model2, model3, xboard, ...): "
                    "build all their sets; list: list the names")
    ap.add_argument("--platforms", help="comma-separated: on a Mac, Apple platforms (default: macosx); "
                    "on Linux, linux and/or linux-arm64 (default: linux)")
    ap.add_argument("--model3", default=os.path.join(ROOT, "cores", "arcade-foundry-cores-model3"),
                    help="Arcade-Foundry-Cores-Model3 checkout, for Model 3 sets (default: cores/arcade-foundry-cores-model3)")
    ap.add_argument("--lindbergh", default=os.path.join(ROOT, "cores", "arcade-foundry-cores-lindbergh"),
                    help="Arcade-Foundry-Cores-Lindbergh checkout, for Lindbergh sets (default: cores/arcade-foundry-cores-lindbergh)")
    ap.add_argument("--classic2d", default=os.path.join(ROOT, "cores", "arcade-foundry-cores-classic2d"),
                    help="Arcade-Foundry-Cores-Classic2D checkout, for Classic 2D sets (default: cores/arcade-foundry-cores-classic2d)")
    ap.add_argument("--chihiro", default=os.path.join(ROOT, "cores", "arcade-foundry-cores-chihiro"),
                    help="Arcade-Foundry-Cores-Chihiro checkout, for Chihiro sets (default: cores/arcade-foundry-cores-chihiro)")
    ap.add_argument("--lindbergh-renderer", choices=("metal", "gl"), default="metal",
                    help="Lindbergh on Apple: metal (default, gl4es on mtlgles) or gl (Apple's OpenGL ES on iOS, "
                         "desktop OpenGL on macOS; kept for one release)")
    ap.add_argument("--jobs", "-j", type=int, default=os.cpu_count() or 1)
    ap.add_argument("--force", action="store_true", help="rebuild cores that are up to date")
    ap.add_argument("--status", action="store_true", help="list which cores are up to date, build nothing")
    args = ap.parse_args()
    if args.boards == "list" or args.sets == "list":
        if args.boards == "list":
            list_boards()
        if args.sets == "list":
            list_sets()
        return
    if not args.roms:
        ap.error("--roms is required")
    wanted = [x for x in (args.sets or "").split(",") if x]
    for b in [x for x in (args.boards or "").split(",") if x]:
        sets = board_sets(b)
        if sets is None:
            sys.exit(f"build_cores: unknown board {b} (build_cores.py --boards list lists them)")
        wanted += sets
    named = set(x for x in (args.sets or "").split(",") if x)
    if not wanted:
        wanted = list(SETS)
    wanted = list(dict.fromkeys(wanted))
    # A set left out of this release (games.toml's hidden) isn't listed by the
    # app, so it is built only when --sets names it.
    left_out = [s for s in wanted if s in HIDDEN and s not in named]
    if left_out:
        print(f"build_cores: not in this release, not built (name them with --sets to build them): {', '.join(left_out)}")
        wanted = [s for s in wanted if s not in left_out]

    if not os.path.exists(os.path.join(RECOMP, "CMakeLists.txt")):
        sys.exit("build_cores: the recompiler is missing; run: git submodule update --init --recursive")
    # The recompiler's pinned dependencies (SoftFloat, SDL, ymfm, the LZMA SDK,
    # Dear ImGui), fetched when any is missing: an extern/ fetched by an older
    # setup.py lacks the ones added since (ImGui, for one).
    needed = ("softfloat", "sdl3/CMakeLists.txt", "ymfm", "lzma", "imgui/imgui.cpp")
    if not all(os.path.exists(os.path.join(RECOMP, "extern", n)) for n in needed):
        run([sys.executable, os.path.join(RECOMP, "scripts", "setup.py"), "--no-build"], cwd=RECOMP)

    host = "macosx" if sys.platform == "darwin" else "linux" if sys.platform.startswith("linux") else None
    if not host:
        sys.exit(f"build_cores: {sys.platform} is not supported yet")
    # The platforms to build for: on a Mac always macosx (it generates the
    # code the others build from); on Linux the ones asked for.
    known = CROSS if host == "macosx" else LINUX_CROSS
    asked = [x for x in (args.platforms or host).split(",") if x]
    for p in asked:
        if p != host and p not in known:
            sys.exit(f"build_cores: unknown platform {p} (known: {', '.join([host, *known])})")
    targets = [host] if host == "macosx" or host in asked else []
    targets += [p for p in dict.fromkeys(asked) if p != host]
    cross = [p for p in targets if p != host]
    if "linux-arm64" in cross and os.uname().machine in ("aarch64", "arm64"):
        sys.exit("build_cores: this machine is arm64 already: build for linux")
    env = dict(os.environ)
    if host == "macosx":
        env.setdefault("MACOSX_DEPLOYMENT_TARGET", "13.0")  # the app's

    # The sets anywhere under --roms, linked into one flat folder, where each
    # core also finds its parent and device sets beside it (scripts/rom_index.py).
    roms = link_folder(args.roms, os.path.join(ROOT, "build", "roms"))
    check_headers(RECOMP, args.model3, args.lindbergh, args.classic2d, args.chihiro)
    if not args.status:
        for p in targets:
            drop_old_cores(p)

    commit = recompiler_state()
    model3_commit = recompiler_state(args.model3) if os.path.isdir(args.model3) else None
    lindbergh_commit = recompiler_state(args.lindbergh) if os.path.isdir(args.lindbergh) else None
    classic2d_commit = recompiler_state(args.classic2d) if os.path.isdir(args.classic2d) else None
    chihiro_commit = recompiler_state(args.chihiro) if os.path.isdir(args.chihiro) else None
    if commit is None and not args.status:
        print("build_cores: the recompiler has uncommitted changes (or is not a git checkout): building everything")
    built, skipped, missing = [], [], []
    for s in wanted:
        if s not in SETS:
            sys.exit(f"build_cores: unknown set {s} (build_cores.py --sets list lists them)")
        rom = find_rom(roms, s)
        if not rom:
            missing.append(s)
            continue
        if s in LINDBERGH:
            plats = [p for p in targets if p == host or p in LINDBERGH_CROSS]
            # The stamp records the game's main program (a folder has no single file to hash).
            stamps = {p: wanted_stamp(os.path.join(rom, LINDBERGH[s][0]), lindbergh_commit, p) for p in plats}
            if args.lindbergh_renderer != "metal":
                stamps = {p: v and v + f"renderer {args.lindbergh_renderer}\n" for p, v in stamps.items()}
            if args.status:
                for p in plats:
                    state = ("up to date" if up_to_date(s, p, stamps[p]) else
                             "out of date" if os.path.exists(core_path(s, p)) else "not built")
                    print(f"{s:10} {p:18} {state}")
                continue
            if not os.path.exists(os.path.join(args.lindbergh, "CMakeLists.txt")):
                sys.exit(f"build_cores: {s} needs Arcade-Foundry-Cores-Lindbergh in {args.lindbergh}; run: git submodule update --init --recursive")
            other = [p for p in cross if p not in LINDBERGH_CROSS]
            if other:
                print(f"build_cores: {s} is not built for {', '.join(other)} "
                      "(tvOS cannot give the game its 4 GB address window; no arm64 Linux build yet)")
            if not plats:
                continue
            stale = [p for p in plats if args.force or not up_to_date(s, p, stamps[p])]
            if not stale:
                skipped.append(s)
                continue
            for p in stale:
                build_lindbergh(args.lindbergh, s, rom, p, args.jobs, env, args.lindbergh_renderer)
                write_stamp(s, p, stamps[p])
            built.append(s)
            continue
        if s in CHIHIRO:
            plats = [p for p in targets if p == host or p in CHIHIRO_CROSS]
            stamps = {p: wanted_stamp(os.path.join(rom, CHIHIRO[s]), chihiro_commit, p) for p in plats}
            if args.status:
                for p in plats:
                    state = ("up to date" if up_to_date(s, p, stamps[p]) else
                             "out of date" if os.path.exists(core_path(s, p)) else "not built")
                    print(f"{s:10} {p:18} {state}")
                continue
            if not os.path.exists(os.path.join(args.chihiro, "CMakeLists.txt")):
                sys.exit(f"build_cores: {s} needs Arcade-Foundry-Cores-Chihiro in {args.chihiro}; run: git submodule update --init --recursive")
            other = [p for p in cross if p not in CHIHIRO_CROSS]
            if other:
                print(f"build_cores: {s} is not built for {', '.join(other)} "
                      "(tvOS cannot give the game its 4 GB address window; no arm64 Linux build yet)")
            if not plats:
                continue
            stale = [p for p in plats if args.force or not up_to_date(s, p, stamps[p])]
            if not stale:
                skipped.append(s)
                continue
            for p in stale:
                build_chihiro(args.chihiro, s, rom, p, args.jobs, env)
                write_stamp(s, p, stamps[p])
            built.append(s)
            continue
        if s in CLASSIC2D:
            stamps = {p: wanted_stamp(rom, classic2d_commit, p) for p in targets}
            if args.status:
                for p in targets:
                    state = ("up to date" if up_to_date(s, p, stamps[p]) else
                             "out of date" if os.path.exists(core_path(s, p)) else "not built")
                    print(f"{s:10} {p:18} {state}")
                continue
            if not os.path.exists(os.path.join(args.classic2d, "CMakeLists.txt")):
                sys.exit(f"build_cores: {s} needs Arcade-Foundry-Cores-Classic2D in {args.classic2d}; run: git submodule update --init --recursive")
            stale = [p for p in targets if args.force or not up_to_date(s, p, stamps[p])]
            if not stale:
                skipped.append(s)
                continue
            # the host first: its build generates the code the others build from
            for p in sorted(stale, key=lambda p: p not in ("macosx", "linux")):
                build_classic2d(args.classic2d, s, rom, p, args.jobs, env)
                write_stamp(s, p, stamps[p])
            built.append(s)
            continue
        if s in MODEL3:
            # Not built from the ROMs (the stamp still records the set the app will run).
            stamps = {p: wanted_stamp(rom, model3_commit, p) for p in targets}
            if args.status:
                for p in targets:
                    state = ("up to date" if up_to_date(s, p, stamps[p]) else
                             "out of date" if os.path.exists(core_path(s, p)) else "not built")
                    print(f"{s:10} {p:18} {state}")
                continue
            if not os.path.exists(os.path.join(args.model3, "CMakeLists.txt")):
                sys.exit(f"build_cores: {s} needs Arcade-Foundry-Cores-Model3 in {args.model3}; run: git submodule update --init --recursive")
            stale = [p for p in targets if args.force or not up_to_date(s, p, stamps[p])]
            if not stale:
                skipped.append(s)
                continue
            for p in stale:
                build_model3(args.model3, s, rom, p, args.jobs, env)
                write_stamp(s, p, stamps[p])
            built.append(s)
            continue
        stamps = {p: wanted_stamp(rom, commit, p) for p in targets}
        stale = [p for p in targets if args.force or not up_to_date(s, p, stamps[p])]
        if args.status:
            for p in targets:
                state = ("up to date" if up_to_date(s, p, stamps[p]) else
                         "out of date" if os.path.exists(core_path(s, p)) else "not built")
                print(f"{s:10} {p:18} {state}")
            continue
        if not stale:
            skipped.append(s)
            continue
        # The host build generates the code (and its core is the macOS/Linux one).
        build = os.path.join(ROOT, "build", "cores-src", s)
        drop_foreign_cache(build, RECOMP)
        if host in stale:
            run([sys.executable, os.path.join(RECOMP, "scripts", "recompile.py"), "--set", s, "--build-dir", build,
                 "--rom", rom, "--core", "--jobs", str(args.jobs)], cwd=RECOMP, env=env)
            publish(build, s, host)
            write_stamp(s, host, stamps[host])
        elif host not in targets or not os.path.isdir(os.path.join(build, "gen")):
            # Only the code, for the cross builds (a recompiler from before
            # --gen-only builds the host's core as well, unused).
            gen_only = "--gen-only" if "--gen-only" in open(os.path.join(RECOMP, "scripts", "recompile.py")).read() else "--core"
            run([sys.executable, os.path.join(RECOMP, "scripts", "recompile.py"), "--set", s, "--build-dir", build,
                 "--rom", rom, gen_only, "--jobs", str(args.jobs)], cwd=RECOMP, env=env)
        for p in [x for x in cross if x in stale]:
            xbuild = os.path.join(ROOT, "build", "cores-src", f"{s}-{p}")
            drop_foreign_cache(xbuild, RECOMP)
            game = ["-DM1_CORE=ON", f"-DM1_ROMSET={s}"] if s in MODEL1 else ["-DM2_CORE=ON", f"-DM2_ROMSET={s}"]
            if p in LINUX_CROSS:
                system = [f"-DCMAKE_TOOLCHAIN_FILE={LINUX_CROSS[p]}"]
            else:
                name, sysroot, target = CROSS[p]
                system = [f"-DCMAKE_SYSTEM_NAME={name}", f"-DCMAKE_OSX_SYSROOT={sysroot}",
                          "-DCMAKE_OSX_ARCHITECTURES=arm64", f"-DCMAKE_OSX_DEPLOYMENT_TARGET={target}"]
            run(["cmake", "-S", RECOMP, "-B", xbuild, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
                 *system, *game, "-DM2_APP=OFF", "-DM2_GEN_ROOT=" + os.path.join(build, "gen")])
            run(["cmake", "--build", xbuild, "--parallel", str(args.jobs), "--target", "m1core" if s in MODEL1 else "m2core"])
            publish(xbuild, s, p)
            write_stamp(s, p, stamps[p])
        built.append(s)

    if missing:
        print(f"build_cores: no ROM set in {args.roms} for {', '.join(missing)}")
    if args.status:
        return
    print(f"build_cores: built {', '.join(built) or 'nothing'} for {', '.join(targets)} into build/cores/")
    if skipped:
        print(f"build_cores: already up to date: {', '.join(skipped)} (--force rebuilds)")
    if not built and not skipped:
        sys.exit(1)


if __name__ == "__main__":
    main()
