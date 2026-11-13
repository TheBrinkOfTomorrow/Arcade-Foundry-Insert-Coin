# Building Arcade Foundry: Insert Coin

How to build the game cores and the apps. For an overview of the app and how to use it, see the [README](README.md).
For the supported games and their set names, see [GAMES.md](GAMES.md).

The cores are built from your own ROM sets on your own machine. Nothing in
these repositories contains ROM data or code generated from it. Cores built
from your ROMs are derived from them, so don't share them.

## macOS, iPhone, iPad and Apple TV

You need Xcode, CMake, Ninja, Python 3 and
[XcodeGen](https://github.com/yonaskolb/XcodeGen):

    brew install cmake ninja xcodegen python gh sdl2 sdl3 sevenzip ccache

ccache is optional but recommended: when it is installed, build_cores.py
runs every core compile through it, so rebuilding a core that hasn't
changed takes seconds (`--no-ccache` turns it off).

Then:

    git clone --recursive https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Insert-Coin
    cd Arcade-Foundry-Insert-Coin
    git pull && git submodule update --init --recursive
    cores/arcade-foundry-cores-common/tools/afic-venv/setup.sh    
    python3 scripts/build_cores.py --roms ~/path/to/roms   # each <set>.zip/.7z found there
    export AF_ROMS=~/path/to/roms                          # then --roms can be left out
    cd apple && xcodegen && open InsertCoin.xcodeproj

`build_cores.py` recompiles each game it finds a ROM set for (the recompiler
is in the cores submodule) and builds its core as a framework into
`build/cores/macosx/`. The Xcode project embeds whatever is there.

For iPhone, iPad and Apple TV, add
`--platforms macosx,iphoneos,iphonesimulator,appletvos,appletvsimulator`
and build the InsertCoin-iOS or InsertCoin-tvOS target.
`--sets vf3,scud` builds only the sets you name;
`--boards model3` builds every set of a board
(`model2` is all of Model 2, 2A, 2B and 2C; `classic2d` every 2D board).
`--boards list` lists the boards, and `--sets list` every set by board, oldest board first.

## Updating

When there's a new version, in the app's folder (on any machine):

    git pull && git submodule update --init --recursive

`git pull` alone updates the app but leaves the cores folders
(`cores/...`) on their old versions, so always run both. Then build again
as you did the first time: `python3 scripts/build_cores.py --roms <your ROM
folder>` rebuilds only the cores that changed, and on Linux
`cmake --build build/linux` (or Xcode on a Mac) rebuilds the app. Your
saves, settings and pictures are kept outside the app folder, so they carry
over.

### The ROM folder

The ROM folder (`--roms`) holds one file per set, named after the set
(e.g. `srallyc.zip`, `scud.7z`, `hotd4a.tar`), either directly in it or in a
folder per board, one level down and flat (no folders inside):

    roms/
      OutRun/     outrun.zip  shangon.zip  toutrun.zip
      System32/   orunners.zip  radr.zip  radm.zip  f1en.zip  jpark.zip  sonic.zip  ga2.zip
      HangOn/     hangon.zip  sharrier.zip  enduror.zip
      XBoard/     aburner131.zip  aburner2.zip  thndrbld.zip  rachero.zip  smgp.zip  loffire.zip  gprider.zip
      YBoard/     gforce2.zip  pdrift.zip  gloc.zip  strkfgtr.zip  rchase.zip
      Model1/     vr.7z (vr and vformula)  swa.7z  vf.7z  wingwar.7z  model1io2.zip
      Model2/     daytona.zip  srallyc.zip  hotd.7z ...
      Model3/     scud.zip  vf3.zip  bassdx.zip ...
      Lindbergh/  hotd4a.tar  outr2sdx.tar ...
      Chihiro/    vcop3/  hotd3/
      working/    (work folders: skipped)

A board folder may also hold a folder per game, named after the set, with
one archive in it under any name (`RingEdge/initd8/Initial D Arcade Stage
8 Infinity.7z` is `initd8.7z`): the folder's name is what counts. Every set
goes by its MAME name (`outr2sdx`, `letsgoju`, `hotd4a` for the Lindbergh
games). MAME's parents `hotd` and `doa` also work for `hotdo` and `doab`,
as do the app's older Lindbergh names (`or2sp`, `lgj`, `hotd4`).

A game's parent and device sets (`hotd.7z`, `model1io2.zip`, `bassdx.zip`)
can be in any board folder. `build_cores.py` and the Linux app link every
set they find into one flat folder (`build/roms/`, and `rom-links/` in the
Linux app's data folder), so each core finds the others beside it.
Folders named `working`, `temp` or `to-file…`, and any starting with `_` or
`.`, are work folders and are skipped, as is anything deeper than a board
folder. If a set is in two places, the copy at the top (then the first
board folder by name) is used and the other is named in a warning.
`scripts/serve_roms.py` and `pack_games.py` look the same way, on macOS and Linux alike.

### Signing

Use your own Apple developer account. Make `apple/Local.xcconfig`
(git-ignored) with your team ID and a bundle ID of your own, then run
`xcodegen`:

    DEVELOPMENT_TEAM = ABCDE12345
    APP_BUNDLE_ID = com.example.arcadefoundry.insertcoin

Apple registers a bundle ID and its iCloud container to one team, so the
project's own (`APP_BUNDLE_ID` in `apple/Signing.xcconfig`) only signs with
its team. The iCloud container follows the bundle ID:
`iCloud.<APP_BUNDLE_ID>` in the entitlements, the iCloud Drive folder and
the save backups. Xcode's automatic signing registers both the first time
you build.

iCloud (iCloud Drive ROM sets, the shared ROM server address and save
backups) needs a paid developer membership.

The entitlements point at the Production CloudKit environment. For the
first run on a new iCloud container:

1. Remove `com.apple.developer.icloud-container-environment` from the
   entitlements.
2. Run a debug build signed in to iCloud. The app creates its record types.
3. Put the key back.
4. Deploy the schema to Production in the CloudKit Console, so release
   builds can back up saves.

#### The shared Python venv (required for Lindbergh and Chihiro games)

Every Python module the app and the cores use (the Lindbergh and Chihiro
translator's `capstone` and `pyelftools`, and the rest) goes in one virtual
environment per machine, `~/venvs/afic`, never into the system's Python and
never into a venv inside a core checkout. Make it once, and again when
Cores-Common's `tools/afic-venv/requirements.txt` changes:

    cd <local-repo-folder-location>
    git pull && git submodule update --init --recursive
    cores/arcade-foundry-cores-common/tools/afic-venv/setup.sh

`AFIC_VENV` puts it somewhere else. `build_cores.py` runs the translator
with the venv it runs under, else `$AFIC_VENV/bin/python3` (default
`~/venvs/afic`), else its own Python. Without the modules, a Lindbergh or
Chihiro build stops with `ModuleNotFoundError: No module named 'capstone'`.
The old `cores/arcade-foundry-cores-lindbergh/venv` and
`cores/arcade

### Lindbergh games

Lindbergh games are the game's disk, not ROM sets. Put the game folder (the
one that holds `disk0`) in the ROMs folder, named after its set
(`roms/hotd4a/disk0/...`), for `build_cores.py`, and add that folder in the
app with Add ROM.

Their cores build for macOS, Linux, iPhone and iPad
(`--platforms macosx,iphoneos,iphonesimulator`), not Apple TV. They need:

- the shared Python venv, `~/venvs/afic`, for the translator (below)
- on macOS, `brew install sdl2` (headers only)
- for iPhone and iPad, Ninja. The build fetches gl4es, which runs the
  games' OpenGL on OpenGL ES.

On an iPhone or iPad a Lindbergh core needs the
`com.apple.developer.kernel.extended-virtual-addressing` entitlement (the
game's 4 GB address window), which the iOS target has. Apple grants it only
to a paid developer team, so a free (personal) team can't install the app
with it.

To go through iCloud Drive or a ROM server, a Lindbergh game travels as one
file, `<set>.tar` (the game folder with the set's name as its top folder).
In the app, Add ROM with iCloud Drive on packs it into iCloud Drive by
itself, and the game stays on the Mac too. "Copy to iCloud Drive" on the
game's page does the same later. For a ROM server,

    python3 scripts/pack_games.py --roms ~/path/to/roms

writes one beside each game folder. A device unpacks it into its ROMs
folder before the first play. A ROM server download is unpacked as it
arrives, so the game is never stored twice. Add ROM also takes a
`<set>.tar`, and `build_cores.py` and the Linux app read it too.

-foundry-cores-chihiro/venv` folders are no longer used and can
be deleted.

### Chihiro games

The Chihiro games and their core are not in the 1.2.0 release; they are
planned for 1.3.0. This section is for when they are.

A Chihiro game is a folder named after its set that holds the game's XBE:
`roms/vcop3/vc3.xbe` (Virtua Cop 3), `roms/hotd3/hod3xb.xbe` (The House of
the Dead III), with the rest of the game's files beside it. Add that folder
in the app with Add ROM. The cores build for macOS and Linux only for now
(`build_cores.py --boards chihiro`), and the translator needs the same
Python packages as Lindbergh's (`capstone`, `pyelftools`), from the same
shared venv (above).

Like a Lindbergh game, it also travels as one file, `<set>.tar`, which
`build_cores.py`, the Linux app, Add ROM, iCloud Drive and the ROM server
all take. A game dumped as a `.7z` of its disc files (`Chihiro/Virtua Cop
3.7z`, the XBE at its top) becomes one with

    python3 scripts/pack_games.py --roms ~/path/to/roms

which finds each dump by the XBE inside it, unpacks it with `7zz`, `7z`,
`7za` or `bsdtar` (`brew install sevenzip`; on Linux the `7zip` or `p7zip-full` package) and
writes `vcop3.tar`, `hotd3.tar` beside it. The `.7z` is left as it is.
A decrypted GD-ROM image (a `.bin` that starts with `FATX`, such as
`Crazy Taxi High Roller (Rev B) (GDX-0002B).bin`) is read the same way,
with no other tool. It wins over a `.7z` of the same game when both hold
the same XBE: the Crazy Taxi `.7z` dumps going round hold filler where its
attract movies should be (they play as black), and the Rev B image has
them. An image with another XBE (Virtua Cop 3's GDX-0003B is a later
revision) loses to the `.7z`, as the cores are made and tested from the
dumps; the packer says so. Likewise an image never replaces an existing
`<set>.tar` that holds another XBE. Add `--force --sets crtaxihr` to replace a
`crtaxihr.tar` packed earlier from a `.7z`. `scripts/fatx.py list IMAGE`
shows what an image holds.

The baseboard's EEPROMs (its operator settings and board ID) come from
MAME's Chihiro BIOS set. Leave `chihiro.zip` zipped, anywhere in the ROM
folder (`Chihiro/chihiro.zip` beside the games is fine): the core reads it
straight from the zip, and the Linux app points the core at it even for a
game unpacked from its `.tar`. On Apple devices, add `chihiro.zip` with Add
ROM like a ROM set. Without it the games still play: the first start uses
factory operator settings and a blank board ID. Either way the core keeps
what the game writes to its EEPROMs (`baseboard-eeprom.bin` in the game's
save folder), so test-menu changes survive a restart.

### Model 3 cores

Model 3 games come from Arcade-Foundry-Cores-Model3, Supermodel's machine
as a core (GPL-3.0-or-later, the submodule
`cores/arcade-foundry-cores-model3`). They are not recompiled from your
ROMs. On Apple platforms they draw with Metal and need nothing extra. Build
them like the others:

    python3 scripts/build_cores.py --roms ~/path/to/roms \
        --boards model3 \
        --platforms macosx,iphoneos,appletvos

The framework carries the GPL's text, and the app embeds it like any other
core.

### Game histories

The "About this game" section on a game's page comes from
`history/<romset>.md`, one file per game, named after any of its versions,
and built into the app. If you want to make changes to the game histories,
then write it in your own words: other sites' text is theirs, so link to
them under "Read more" instead of copying. The layout:

    # Title

    Developer · Year · Board

    A few short paragraphs: who made it, what made it stand out, its
    sequels and home versions.

    ## Read more

    - [Title on Wikipedia](https://en.wikipedia.org/wiki/...)

Apple TV shows the text without the links (it has no browser).

### Credits

`CREDITS.md` is built into the Apple app (Settings > About > Credits, and
the Mac's About window) and into the Linux app (CMake reads it when it
configures; `insert-coin --credits` prints it). Keep its layout: `## `
sections of paragraphs and `- ` items, with `**bold**` and `[links](...)`.
When a core takes in someone else's code, add them there as well as to
the core's own notices. Supporters can go in a section of their own.

### Icons

`scripts/make_icons.py` redraws the app icons from `art/`.

## Linux

You need CMake, Ninja, a C++20 compiler and Python 3, plus SDL's usual
development packages for X11/Wayland and audio. Model 3 cores also need
`libsdl2-dev libsdl2-net-dev libglu1-mesa-dev libegl-dev`. Game art
downloads need libcurl (`libcurl4-openssl-dev`, or `libcurl-devel` on
Fedora); without it CMake warns and the app builds without them. Save
states need zlib (`zlib1g-dev`, or `zlib-devel` on Fedora).
Install `ccache` too (optional, recommended): build_cores.py uses it when
it is installed, which makes core rebuilds much faster.

1. The cores, from your ROM sets:

       python3 scripts/build_cores.py --roms ~/path/to/roms

2. Set up the app's build (once):

       cmake -S linux -B build/linux -G Ninja

3. Build the app (this is the step that makes `build/linux/insert-coin`;
   on a board with 4 GB or less, add `-j 2`):

       cmake --build build/linux

4. Play:

       build/linux/insert-coin --roms ~/path/to/roms

See the [README](README.md#linux) for the Linux app's options.

### The SDL3 app on a Mac

The same SDL3 app also builds on a Mac with Apple silicon. It has the
Linux app's features, not the SwiftUI app's: no iCloud saves or ROM sync,
no ROM server, no touch controls or gameplay videos. It suits a Mac kept
as a cabinet or kiosk, running full screen with game controllers or a
wheel and no mouse or keyboard. It uses the Mac's cores (step 1 of
[macOS](#macos-iphone-ipad-and-apple-tv), which builds them into
`build/cores/macosx`) and draws with Metal; the Model 3
screen goes to the window straight from the GPU, as in the SwiftUI app.
You need CMake and Ninja (`brew install cmake ninja`); SDL3 comes with the
cores' sources.

    cmake -S linux -B build/sdl-mac -G Ninja
    cmake --build build/sdl-mac
    build/sdl-mac/insert-coin --roms ~/path/to/roms --fullscreen

It keeps its files in `~/.local/share/arcade-foundry/`, apart from the
SwiftUI app's, and takes the options in the [README](README.md#linux).

### arm64 boards (Raspberry Pi 4 and 5, Orange Pi 5, ODROID-N2)

Building on the board itself takes a long time. `scripts/build_arm64.py`
builds the app and the cores on a faster machine instead, in a container
(Debian 12 Bookworm, the base of Raspberry Pi OS 12, so the result runs on
Bookworm and newer), and packs them into one file:

    python3 scripts/build_arm64.py --roms ~/path/to/roms

- On an Apple silicon Mac the container is arm64 and builds natively.
  Install [OrbStack](https://orbstack.dev) once to run it
  (`brew install orbstack`, then `open -a OrbStack`); Docker Desktop works
  too. On OrbStack's first start, pick **Docker** when it asks what you
  want to use. Its window can then be closed: OrbStack keeps running from
  the menu bar (Quit there stops it; "Start at login" in its settings
  starts it with the Mac). `docker info` printing a "Server" section means
  it is ready. If a compile runs out of memory, give the container more
  (OrbStack: Settings, System) or lower `-j`.
- On an x86-64 Linux machine (with Docker or Podman) it cross-compiles:
  the Model 1/2 code is generated by x86-64 tools, then compiled for arm64
  with GCC's aarch64 toolchain (`linux/arm64/aarch64-toolchain.cmake`).

Everything goes to `build/arm64/`, apart from your own `build/`. The result
is `build/arm64/af-insert-coin-linux-arm64.tar.gz` (the app and `cores/`); copy it to
the board and:

    tar xzf af-insert-coin-linux-arm64.tar.gz
    af-insert-coin-linux-arm64/insert-coin --roms ~/ArcadeFoundry/roms

It runs on any 64-bit ARM Linux with glibc 2.36 or newer: Raspberry Pi OS
12 (Bookworm) or later, Debian 12+, Ubuntu 24.04+, Armbian, and other
distributions of that age, Debian-based or not. It does not run on a 32-bit
OS, Ubuntu 22.04 or older, or musl-based systems (Alpine). The board needs
only its usual desktop libraries (SDL 2 and SDL2_net as well for Model 3). By default the Model 1
and Model 2 sets are built; name others with `--sets` (Model 3 cores need
OpenGL 4, which a Pi does not have yet; Lindbergh is not built for arm64).
The cores come from your ROMs: keep the file to yourself. One build serves
every board (generic armv8-a). `--here` skips the container (an arm64
Linux, or an x86-64 Debian/Ubuntu with the Dockerfile's packages).

## Adding a game

The games are listed once, in `games.toml` files: each core repo's lists
every set it builds (title, year, board, version, core version, controls),
and this repo's lists the sets the app shows, in the library's order, with
the hint its detail line gives. `scripts/gen_games.py` (Python 3.11 or
later) writes the rest from them: the Apple catalog
(`kit/Sources/Library/GameList.swift`, in the kit submodule: commit it
there, then move the submodule here), the Linux app's table
(`linux/game_list.h`), `build_cores.py`'s lists (`scripts/game_sets.py`)
and the tables in [GAMES.md](GAMES.md). For a new game:

1. Add its `[[game]]` to the core repo's `games.toml`, run that repo's
   `gen_games.py`, and commit it there with the core's code.
2. Move the core's submodule here to that commit.
3. Add its `[[game]]` to `games.toml` here, in the place it goes in the
   library, with its `hint`.
4. Run `python3 scripts/gen_games.py` and commit what it wrote, with a
   `history/<set>.md` for a new card.

CI runs `scripts/gen_games.py --check`, which fails when a generated file
does not match the `games.toml` files.

## Versions

- **The app**: `MARKETING_VERSION` in `apple/project.yml` and the version
  in `linux/CMakeLists.txt`, kept the same (x.y.z). Each release is tagged
  `vX.Y.Z` in this repo.
- **The core interface**, `afcore.h` (`AFCORE_API_*`): one file shared by
  every core repo. Arcade-Foundry-Cores-Model12 has it, and the others carry
  an identical copy (`build_cores.py` warns when a copy differs). A major
  version breaks old cores: the app refuses them until they are rebuilt. A
  minor one only adds, so mixed minors work together.
- **Each game's core**: `core_version`, from `core_versions.h` (Model 1/2,
  Model 3) or the game's `GAME_CORE_VERSION` (Lindbergh). Major when its
  saves no longer carry over, minor for a change players see, patch for
  anything else. The app shows it on the game's page.
