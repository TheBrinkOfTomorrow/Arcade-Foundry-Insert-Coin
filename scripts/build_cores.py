#!/usr/bin/env python3
"""Build the game cores Arcade Foundry: Insert Coin embeds, from your own ROM sets.

  build_cores.py --roms DIR [--sets daytona93,daytona,srallyc,manxttc,vcop2,vcop,vf2,indy500,fvipers,sgt24h,overrevb,stcc,von,hotdo,gunblade,vstriker,zerogun,pltkids,motoraid,dynamcop,skytargt,desert,lastbrnx,schamp,dynabb97,waverunr,segawski,skisuprg,bel,topskatr,rchase2,doab,vr,swa,vf,wingwar,vf3,scud,scudplus,daytona2,dayto2pe,srally2,lemans24,dirtdvls,harley,ecau,fvipers2,von2,spikeout,spikeofe,vs299a,swtrilgy,lostwsga,lamachin,oceanhuna,magtruck,bassdx,getbass,getbassur,getbassdx,outrun,shangon,aburner2,thndrbld]
                 [--platforms macosx,iphoneos,iphonesimulator,appletvos,appletvsimulator | linux,linux-arm64]
                 [--model3 DIR] [--lindbergh DIR] [--classic2d DIR] [--jobs N] [--force] [--status]

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
builds it as a core library (its include/afcore.h). The result goes to
build/cores/<platform>/ (Xcode's platform names; the app's build copies
from the one it is built for): afcore_<set>.framework on Apple platforms,
libafcore_<set>.so on Linux. On a Mac the default is macosx only; the
iOS and tvOS ones are cross-built from the same generated code. On an
x86-64 Linux, --platforms linux-arm64 cross-builds arm64 Linux cores
(build/cores/linux-arm64/) with linux/arm64/aarch64-toolchain.cmake;
scripts/build_arm64.py does that, or a native arm64 build, in a container. Everything generated stays under build/ (git-ignored): never
commit it, it is derived from the ROMs.

Model 3 sets (vf3, scud, scudplus, daytona2, dayto2pe, srally2, lemans24,
dirtdvls, harley, ecau, fvipers2, von2, spikeout, spikeofe, vs299a,
swtrilgy, lostwsga, lamachin, oceanhuna, magtruck, bassdx, getbass, getbassur,
getbassdx) come from Arcade-Foundry-Cores-Model3 (GPL-3, the submodule
cores/arcade-foundry-cores-model3, or another checkout with --model3). They
are not recompiled from the ROMs: the core is Supermodel's machine. On
Apple platforms it draws with Metal and needs nothing beyond the system
(iOS and tvOS ones are built with CMake's Xcode generator); on Linux it
uses desktop OpenGL, SDL2 and SDL2_net. Each Apple framework carries the
GPL's text (COPYING).

Classic 2D sets (outrun, shangon: the Out Run board; aburner2, thndrbld: the X Board) come from Arcade-Foundry-Cores-Classic2D
(the submodule cores/arcade-foundry-cores-classic2d, or another checkout
with --classic2d): its scripts/recompile.py checks the set, translates the
68000 programs and builds the core; iOS, tvOS and arm64 Linux are built
from the code it generates on the host.

Lindbergh sets (hotd4a, letsgoju, segartv, outr2sdx) come from Arcade-Foundry-Cores-Lindbergh (the submodule
cores/arcade-foundry-cores-lindbergh, or another checkout with --lindbergh).
Their "ROM set" is the game's folder in DIR, named after the set
(DIR/hotd4a/disk0/...), or the same packed as DIR/hotd4a.tar
(scripts/pack_lindbergh.py; unpacked under build/lindbergh-games/): its
programs are translated at build time. macOS, Linux and iOS
(iphoneos, iphonesimulator; not tvOS, which has no way to give a game its
4 GB address window). The build needs a Python with capstone and pyelftools
(the checkout's venv/ if it has one); macOS and Linux need SDL2's headers,
and iOS builds fetch gl4es (desktop OpenGL on OpenGL ES) through CMake and
use Ninja.
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
SETS = ["daytona93", "daytona", "srallyc", "manxttc", "vcop2", "vcop", "vf2", "indy500", "fvipers", "sgt24h", "overrevb", "stcc", "von", "hotdo", "gunblade", "vstriker", "zerogun", "pltkids", "motoraid", "dynamcop", "skytargt", "desert", "lastbrnx", "schamp", "dynabb97", "waverunr", "segawski", "skisuprg", "bel", "topskatr", "rchase2", "doab", "vr", "swa", "vf", "wingwar",
        "vf3", "scud", "scudplus", "daytona2", "dayto2pe", "srally2", "lemans24", "dirtdvls", "harley", "ecau",
        "fvipers2", "von2", "spikeout", "spikeofe", "vs299a", "swtrilgy", "lostwsga", "lamachin", "oceanhuna", "magtruck", "bassdx", "getbass", "getbassur", "getbassdx", "hotd4a", "letsgoju", "segartv", "outr2sdx", "outrun", "shangon", "aburner2", "thndrbld"]
# Model 1 sets: the recompiler's Model 1 build (CMake M1_CORE, target m1core)
MODEL1 = {"vr", "swa", "vf", "wingwar"}
# Model 3 sets: Arcade-Foundry-Cores-Model3 (CMake M3CORE_ROMSET)
MODEL3 = {"vf3", "scud", "scudplus", "daytona2", "dayto2pe", "srally2", "lemans24", "dirtdvls", "harley", "ecau",
          "fvipers2", "von2", "spikeout", "spikeofe", "vs299a", "swtrilgy", "lostwsga", "lamachin", "oceanhuna", "magtruck", "bassdx", "getbass", "getbassur", "getbassdx"}
# Classic 2D sets: Arcade-Foundry-Cores-Classic2D (scripts/recompile.py, CMake C2D_ROMSET)
CLASSIC2D = {"outrun", "shangon", "aburner2", "thndrbld"}
# Lindbergh sets: Arcade-Foundry-Cores-Lindbergh (CMake LB_CORE); the game's
# program and, when it is a program of its own, the test menu's, under the
# game folder (the shared libraries a game ships sit next to its program)
LINDBERGH = {"hotd4a": ("disk0/elf/hod4M.elf", "disk0/elf/hod4testM.elf"),
             "letsgoju": ("disk0/lgj_final", None),
             "segartv": ("disk0/drive.elf", None),
             "outr2sdx": ("disk0/Jennifer/Jennifer", "disk0/JenTest/JenTest")}


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
    version = interface_version(header or os.path.join(RECOMP, "include", "afcore.h"))
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
    """Each core repo carries a copy of the Model 1/2 repo's include/afcore.h;
    a copy that differs means that repo needs the header copied over."""
    ref = os.path.join(RECOMP, "include", "afcore.h")
    for repo, rel in repos:
        copy = os.path.join(repo, rel)
        if os.path.exists(ref) and os.path.exists(copy) and file_sha1(ref) != file_sha1(copy):
            print(f"build_cores: warning: {copy} differs from {ref}; copy the Model 1/2 repo's header over it")


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
    publish(build, s, plat, os.path.join(src, "Core", "include", "afcore.h"))


# Lindbergh cores build for these cross platforms (tvOS has no extended
# virtual addressing, which the 4 GB guest window needs)
LINDBERGH_CROSS = {"iphoneos", "iphonesimulator"}


def build_lindbergh(src, s, folder, plat, jobs, env):
    """Build a Lindbergh core from Arcade-Foundry-Cores-Lindbergh and publish it."""
    build = os.path.join(ROOT, "build", "cores-src", s if plat in ("macosx", "linux") else f"{s}-{plat}")
    drop_foreign_cache(build, src)
    gl_xml = os.path.join(src, "extern", "gl.xml")
    if not os.path.exists(gl_xml):
        os.makedirs(os.path.dirname(gl_xml), exist_ok=True)
        run(["curl", "-fsSL", "-o", gl_xml,
             "https://raw.githubusercontent.com/KhronosGroup/OpenGL-Registry/main/xml/gl.xml"])
    venv = os.path.join(src, "venv", "bin", "python")
    python = venv if os.path.exists(venv) else sys.executable
    game, test = LINDBERGH[s]
    # iOS: the core on OpenGL ES through gl4es (the core's CMake fetches it), with Ninja
    cross = []
    if plat in CROSS:
        system, sysroot, target = CROSS[plat]
        cross = ["-G", "Ninja", f"-DCMAKE_SYSTEM_NAME={system}", f"-DCMAKE_OSX_SYSROOT={sysroot}",
                 "-DCMAKE_OSX_ARCHITECTURES=arm64", f"-DCMAKE_OSX_DEPLOYMENT_TARGET={target}"]
    run(["cmake", "-S", src, "-B", build, *cross, "-DCMAKE_BUILD_TYPE=Release", f"-DGAME={s}", f"-DCORE_ROMSET={s}",
         f"-DGAME_ELF={os.path.join(folder, game)}", f"-DGAME_TEST_ELF={os.path.join(folder, test) if test else ''}",
         "-DLB_CORE=ON", "-DLB_RUN=OFF", f"-DPython3_EXECUTABLE={python}"], env=env)
    run(["cmake", "--build", build, "--parallel", str(jobs), "--target", f"afcore_{s}"], env=env)
    if plat == "macosx":
        bundle_libraries(os.path.join(build, f"afcore_{s}.framework"))
    publish(build, s, plat, os.path.join(src, "include", "afcore.h"))


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
        publish(host_build, s, plat, os.path.join(src, "include", "afcore.h"))
        return
    if not os.path.exists(os.path.join(host_build, "gen", s, "gen_main.cpp")):
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
    publish(build, s, plat, os.path.join(src, "include", "afcore.h"))


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
    for name in names(s):
        for ext in ("7z", "zip") if s in SEVEN_Z_FIRST else ("zip", "7z"):
            p = os.path.join(roms, name + "." + ext)
            if os.path.exists(p):
                return p
    return None


def unpacked_game(tar, s):
    """A Lindbergh game packed as <set>.tar: unpacked once under build/
    (again when the .tar changes); its folder, or None."""
    if not os.path.exists(tar):
        return None
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from pack_lindbergh import unpack
    dst = os.path.join(ROOT, "build", "lindbergh-games", s)
    st = os.stat(tar)
    mark, want = dst + ".from", f"{os.path.abspath(tar)} {st.st_size} {st.st_mtime_ns}"
    have = open(mark).read() if os.path.exists(mark) else ""
    if have != want or not os.path.exists(os.path.join(dst, LINDBERGH[s][0])):
        print(f"build_cores: unpacking {tar}", flush=True)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        unpack(tar, names(s), dst)
        with open(mark, "w") as f:
            f.write(want)
    return dst if os.path.exists(os.path.join(dst, LINDBERGH[s][0])) else None


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
    ap.add_argument("--roms", required=True, help="folder with your MAME ROM sets (or its board folders)")
    ap.add_argument("--sets", default=",".join(SETS), help="comma-separated (default: all)")
    ap.add_argument("--platforms", help="comma-separated: on a Mac, Apple platforms (default: macosx); "
                    "on Linux, linux and/or linux-arm64 (default: linux)")
    ap.add_argument("--model3", default=os.path.join(ROOT, "cores", "arcade-foundry-cores-model3"),
                    help="Arcade-Foundry-Cores-Model3 checkout, for Model 3 sets (default: cores/arcade-foundry-cores-model3)")
    ap.add_argument("--lindbergh", default=os.path.join(ROOT, "cores", "arcade-foundry-cores-lindbergh"),
                    help="Arcade-Foundry-Cores-Lindbergh checkout, for Lindbergh sets (default: cores/arcade-foundry-cores-lindbergh)")
    ap.add_argument("--classic2d", default=os.path.join(ROOT, "cores", "arcade-foundry-cores-classic2d"),
                    help="Arcade-Foundry-Cores-Classic2D checkout, for Classic 2D sets (default: cores/arcade-foundry-cores-classic2d)")
    ap.add_argument("--jobs", "-j", type=int, default=os.cpu_count() or 1)
    ap.add_argument("--force", action="store_true", help="rebuild cores that are up to date")
    ap.add_argument("--status", action="store_true", help="list which cores are up to date, build nothing")
    args = ap.parse_args()

    if not os.path.exists(os.path.join(RECOMP, "CMakeLists.txt")):
        sys.exit("build_cores: the recompiler is missing; run: git submodule update --init")
    # The recompiler's pinned dependencies (SoftFloat, ymfm, the LZMA SDK, ...), fetched once.
    if not os.path.exists(os.path.join(RECOMP, "extern", "softfloat")):
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
    check_headers((args.model3, os.path.join("Core", "include", "afcore.h")),
                  (args.lindbergh, os.path.join("include", "afcore.h")),
                  (args.classic2d, os.path.join("include", "afcore.h")))
    if not args.status:
        for p in targets:
            drop_old_cores(p)

    commit = recompiler_state()
    model3_commit = recompiler_state(args.model3) if os.path.isdir(args.model3) else None
    lindbergh_commit = recompiler_state(args.lindbergh) if os.path.isdir(args.lindbergh) else None
    classic2d_commit = recompiler_state(args.classic2d) if os.path.isdir(args.classic2d) else None
    if commit is None and not args.status:
        print("build_cores: the recompiler has uncommitted changes (or is not a git checkout): building everything")
    built, skipped, missing = [], [], []
    for s in [x for x in args.sets.split(",") if x]:
        if s not in SETS:
            sys.exit(f"build_cores: unknown set {s} (known: {', '.join(SETS)})")
        rom = find_rom(roms, s)
        if not rom:
            missing.append(s)
            continue
        if s in LINDBERGH:
            plats = [p for p in targets if p == host or p in LINDBERGH_CROSS]
            # The stamp records the game's main program (a folder has no single file to hash).
            stamps = {p: wanted_stamp(os.path.join(rom, LINDBERGH[s][0]), lindbergh_commit, p) for p in plats}
            if args.status:
                for p in plats:
                    state = ("up to date" if up_to_date(s, p, stamps[p]) else
                             "out of date" if os.path.exists(core_path(s, p)) else "not built")
                    print(f"{s:10} {p:18} {state}")
                continue
            if not os.path.exists(os.path.join(args.lindbergh, "CMakeLists.txt")):
                sys.exit(f"build_cores: {s} needs Arcade-Foundry-Cores-Lindbergh in {args.lindbergh}; run: git submodule update --init")
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
                build_lindbergh(args.lindbergh, s, rom, p, args.jobs, env)
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
                sys.exit(f"build_cores: {s} needs Arcade-Foundry-Cores-Classic2D in {args.classic2d}; run: git submodule update --init")
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
                sys.exit(f"build_cores: {s} needs Arcade-Foundry-Cores-Model3 in {args.model3}; run: git submodule update --init")
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
