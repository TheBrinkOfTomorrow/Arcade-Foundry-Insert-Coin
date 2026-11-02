#!/usr/bin/env python3
"""Build Arcade Foundry: Insert Coin and its cores for 64-bit ARM Linux boards
(Raspberry Pi 4 and 5, Orange Pi 5, ODROID-N2) on a faster machine, and pack
them into one file to copy to the board.

  build_arm64.py --roms DIR [--sets vcop,daytona,...] [--jobs N] [--force] [--here]

By default it builds inside a container (Docker Desktop, OrbStack, Podman or
Docker on Linux) from linux/arm64/Dockerfile, a Debian 12 (Bookworm) image,
the base of Raspberry Pi OS 12, so what it builds runs on Bookworm and newer:

  - on an arm64 machine (an Apple silicon Mac) the container is arm64 and
    builds natively, at the Mac's full speed;
  - on an x86-64 Linux machine it cross-compiles (GCC's aarch64 toolchain,
    linux/arm64/aarch64-toolchain.cmake): the Model 1/2 code is generated
    by x86-64 tools, then compiled for arm64.

Everything it writes goes to build/arm64/ (the container's build/), apart
from the app's and recompiler's sources, so it never mixes with the
machine's own build/. The result is build/arm64/af-insert-coin-linux-arm64.tar.gz:
insert-coin and cores/libafcore_<set>.so. On the board:

  tar xzf af-insert-coin-linux-arm64.tar.gz
  af-insert-coin-linux-arm64/insert-coin --roms ~/ArcadeFoundry/roms

The cores are made from your ROM sets: keep the file to yourself, never
commit it. By default the Model 1 and Model 2 sets are built (the ones a Pi
runs); Model 3 sets can be named with --sets (they need desktop OpenGL 4,
which a Pi does not have yet). Lindbergh is not built for arm64.

--here builds in this machine's own environment instead of a container (an
arm64 Linux, or an x86-64 Debian/Ubuntu with the packages the Dockerfile
installs): its files then go to build/, cores in build/cores/linux (arm64)
or build/cores/linux-arm64 (x86-64).
"""

import argparse
import os
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build_cores import LINDBERGH, MODEL3, SETS  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMAGE = "insert-coin-arm64-build"
DEFAULT_SETS = [s for s in SETS if s not in MODEL3 and s not in LINDBERGH]
PACKAGE = "af-insert-coin-linux-arm64"


def run(cmd, **kw):
    print("+", " ".join(cmd), flush=True)
    subprocess.run(cmd, check=True, **kw)


def container_tool():
    for tool in ("docker", "podman"):
        if shutil.which(tool):
            return tool
    sys.exit("build_arm64: no docker or podman found: install Docker Desktop or OrbStack (Mac), "
             "or Docker (Linux), or build with --here")


def in_container(args):
    """Build again inside the container, with the checkout at /src and
    build/arm64 over its build/."""
    tool = container_tool()
    roms = os.path.abspath(os.path.expanduser(args.roms))
    if not os.path.isdir(roms):
        sys.exit(f"build_arm64: {roms} is not a folder")
    out = os.path.join(ROOT, "build", "arm64")
    os.makedirs(out, exist_ok=True)
    if subprocess.run([tool, "info"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode:
        sys.exit(f"build_arm64: {tool} is installed but not running: start OrbStack (open -a OrbStack) or "
                 f"Docker Desktop, wait until it is up ({tool} info works), then run this again")
    run([tool, "build", "-t", IMAGE, os.path.join(ROOT, "linux", "arm64")])
    cmd = [tool, "run", "--rm", "-v", f"{ROOT}:/src", "-v", f"{out}:/src/build", "-v", f"{roms}:/roms:ro",
           "-w", "/src", "-e", "HOME=/tmp", "-e", "BUILD_ARM64_HOST_OUT=build/arm64"]
    if tool == "docker" and hasattr(os, "getuid"):
        cmd += ["--user", f"{os.getuid()}:{os.getgid()}"]  # files in build/arm64 stay yours
    cmd += [IMAGE, "python3", "scripts/build_arm64.py", "--here", "--roms", "/roms", "--sets", args.sets,
            "--jobs", str(args.jobs)] + (["--force"] if args.force else [])
    run(cmd)


def here(args):
    machine = os.uname().machine
    native = machine in ("aarch64", "arm64")
    if not native and not shutil.which("aarch64-linux-gnu-g++"):
        sys.exit("build_arm64: this is not an arm64 machine and has no aarch64-linux-gnu-g++ "
                 "(install crossbuild-essential-arm64, or leave out --here to build in a container)")
    build = os.path.join(ROOT, "build")
    plat = "linux" if native else "linux-arm64"

    # The cores
    cmd = [sys.executable, os.path.join(ROOT, "scripts", "build_cores.py"), "--roms", args.roms,
           "--sets", args.sets, "--jobs", str(args.jobs)]
    if not native:
        cmd += ["--platforms", plat]
    run(cmd + (["--force"] if args.force else []))

    # The app (SDL 3 comes from the recompiler's extern/, fetched by build_cores.py)
    app = os.path.join(build, "linux" if native else "linux-arm64")
    toolchain = [] if native else [
        "-DCMAKE_TOOLCHAIN_FILE=" + os.path.join(ROOT, "linux", "arm64", "aarch64-toolchain.cmake")]
    run(["cmake", "-S", os.path.join(ROOT, "linux"), "-B", app, "-G", "Ninja", *toolchain])
    run(["cmake", "--build", app, "--parallel", str(args.jobs)])

    # The package: the app with cores/ beside it, where it looks first
    pkg = os.path.join(build, PACKAGE)
    shutil.rmtree(pkg, ignore_errors=True)
    os.makedirs(os.path.join(pkg, "cores"))
    shutil.copy2(os.path.join(app, "insert-coin"), pkg)
    cores = os.path.join(build, "cores", plat)
    wanted = {f"libafcore_{s}.so" for s in args.sets.split(",") if s}
    packed = sorted(n for n in os.listdir(cores) if n in wanted) if os.path.isdir(cores) else []
    for name in packed:
        shutil.copy2(os.path.join(cores, name), os.path.join(pkg, "cores"))
    tarball = os.path.join(build, PACKAGE + ".tar.gz")
    run(["tar", "czf", tarball, "-C", build, PACKAGE])

    # A quick check that the app starts (under qemu when cross-built, if it is there)
    qemu = [] if native else ([shutil.which("qemu-aarch64"), "-L", "/usr/aarch64-linux-gnu"]
                              if shutil.which("qemu-aarch64") else None)
    if qemu is not None:
        run(qemu + [os.path.join(pkg, "insert-coin"), "--version"])
    shown = os.environ.get("BUILD_ARM64_HOST_OUT", "build")
    print(f"build_arm64: {shown}/{PACKAGE}.tar.gz holds insert-coin and {len(packed)} cores "
          f"({', '.join(n[len('libafcore_'):-3] for n in packed) or 'none'})")
    print("build_arm64: on the board: tar xzf af-insert-coin-linux-arm64.tar.gz && "
          "af-insert-coin-linux-arm64/insert-coin --roms ~/ArcadeFoundry/roms")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--roms", required=True, help="folder with your MAME ROM sets (or its board folders)")
    ap.add_argument("--sets", default=",".join(DEFAULT_SETS), help="comma-separated (default: the Model 1 and 2 sets)")
    ap.add_argument("--jobs", "-j", type=int, default=os.cpu_count() or 1)
    ap.add_argument("--force", action="store_true", help="rebuild cores that are up to date")
    ap.add_argument("--here", action="store_true", help="build in this machine's environment, not a container")
    args = ap.parse_args()
    lindbergh = [s for s in args.sets.split(",") if s in LINDBERGH]
    if lindbergh:
        sys.exit(f"build_arm64: Lindbergh games are not built for arm64 ({', '.join(lindbergh)})")
    if not sys.platform.startswith("linux") and args.here:
        sys.exit("build_arm64: --here is for Linux; on a Mac leave it out to build in a container")
    here(args) if args.here else in_container(args)


if __name__ == "__main__":
    main()
