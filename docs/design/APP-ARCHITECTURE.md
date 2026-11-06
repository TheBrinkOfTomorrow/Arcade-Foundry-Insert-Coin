# App architecture

How Arcade Foundry: Insert Coin is put together: the two front ends, how
games are listed, how a core is found, loaded and run, where files and
settings live, and why it was built this way. The ROM server and iCloud
Drive, light guns, controls and steering wheels have documents
of their own (see Related documents below); this one says where they fit.

## Related documents

- [Steering wheels and pedals](WHEEL-AND-PEDALS.md)
- ROM server and ROM sync: the ROM server URL, iCloud Drive, downloads
  and checks (to write)
- Light guns: mouse, touch, gamepad aim and real light guns (to write)
- [Control scheme](CONTROL-SCHEME.md): controllers, keyboard and mouse, touch and the
  on-screen controller, Apple TV, button maps, two players
- Other controllers: twin sticks, fight sticks, fishing rods and other
  cabinet-style controllers, on every platform (to write)
- The cores' own designs: the [index](README.md#core-designs)

## Purpose and scope

The app is the front end for the cores: it lists the games, finds the
user's ROM sets, starts a game's core and gives it a screen, sound and
controls. It does no emulation itself. Every game is a core, one library
per ROM set, behind one C interface, `afcore.h`, from
[Arcade-Foundry-Cores-Common](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Common/blob/main/include/afcore.h).

In scope:

- the Apple app (macOS on Apple silicon, iOS, iPadOS, tvOS) and the Linux
  app (x86-64 desktops, Steam Deck, Raspberry Pi 4 and 5, other arm64
  boards)
- the game list and where it comes from
- building cores from the user's ROM sets and loading them at run time
- a session: frames, timing, screen, sound, controls, the pause menu
- where ROM sets, saves, pictures and settings are kept, and what goes to
  iCloud
- cabinets and link play between devices
- versions, branches and how a release is made

Not in scope: how any core emulates its board (each core repository's
design documents), and the topics with documents of their own.

## Requirements

1. **No ROM content ships.** The app and the cores' source hold nothing
   taken from a ROM. Cores are built on the user's machine from the user's
   own ROM sets, and the built cores never leave that user's devices.
2. **One app per platform family, the same games everywhere.** A game
   that works on one platform works on the others unless the platform
   can't run it (see Platforms).
3. **The arcade's speed.** A game runs at its board's own frame rate
   (57.52 Hz for Model 2, for example) on any display, 60 Hz or 120 Hz.
4. **Controller first.** Every screen works with a game controller alone;
   Apple TV has nothing else. Keyboard, mouse and touch are extras.
5. **The app runs with any subset of cores, or none.** A missing core or
   ROM set is a reason shown on the game's row, never a crash.
6. **One interface for every core.** Adding a board means a new core
   repository with the same interface, not new code paths in the app.
7. **Saves belong to a cabinet.** A device is a cabinet with its own
   settings and records; linked cabinets must keep their own link roles.
8. **Quick to start.** The library is on screen in about a second, with
   many cores installed.

## Design

### Where it sits

```
 user's ROM sets ──► scripts/build_cores.py ──► build/cores/<platform>/afcore_<set>.*
                        │ (recompiles each set with its core repo)
                        ▼
  games.toml + cores' games.toml ──► scripts/gen_games.py ──► the app's game lists
                                                                  │
          ┌───────────────────────────────────────────────────────┴───────┐
          ▼                                                               ▼
   apple/ (SwiftUI + Metal)                                    linux/ (SDL3)
   macOS · iOS · iPadOS · tvOS                                 x86-64 · arm64
          │  dlopen afcore_<set>.framework                          │  dlopen libafcore_<set>.so
          └──────────────────────────► afcore.h ◄────────────────────┘
                     cores/: Model12 · Model3 · Classic2D · Lindbergh · Chihiro · Common
```

The core repositories are git submodules under `cores/`. The app's
`af-next` pins each one at a commit on that core's `af-next`; the
released `main` pins each at its released commit.

### The core interface

Each core library exports one function, `afcore_api_<romset>()`, which
returns a table (`afcore_api`) of function pointers: `create` (from a
ROM path and a save folder), `run_frame` (with that frame's
`afcore_input`), `screen`, `audio_rate` and `audio_read`, `save`,
`destroy`, `error`, and the optional ones: `link_start` and `link_state`,
the stereo calls (`render_eyes`, `eye_screen`, `hud_screen`),
`screen_surface` (a GPU surface instead of a pixel buffer), `check_roms`
and `set_option`.

The interface has a version x.y.z, packed as major<<16 | minor<<8 |
patch. The app loads a core only when the major matches its own. Minor
versions only append: a newer core adds fields at the end of its tables
and says how big they are (`size`), so an older app reads what it knows
and a newer app checks a field is there before using it. A core that hits
something it can't run stops and reports through `error` rather than
crashing; the app shows the message and goes back to the library.

Each game's core also has its own version, separate from the app's and
the interface's (each core repository's `games.toml`).

### The game list

There is one list of games, written once. Each core repository's
`games.toml` holds the facts about its games (title, year, board,
controls, players, link play, other file names, ROM notes). The app's
`games.toml` holds the sets the app shows, in the library's order, and
the words it shows (hints, notes). `scripts/gen_games.py` writes from
both: the Apple catalog (`GameList.swift`), the Linux table
(`linux/game_list.h`), `scripts/game_sets.py` for the build and ROM
scripts, and the tables in GAMES.md. CI runs it with `--check` and fails
on any difference.

A game not in a core's `games.toml` does not exist to the app. `hidden`
in `games.toml` is the one switch that keeps a listed game out of a
release; there are no other code flags for it.

The library groups versions of one game (Daytona USA and its Revision A)
into one card per title and board. It can show all games or only the
playable ones, filter by platform (Classic 2D, Model 1, Model 2, Model
3, Lindbergh, Chihiro), and sort A-Z, by year, by platform or by
recently played. Each game has a page with its versions, each version's
own settings, its controls and a short history (`history/<set>.md`,
shown as About this game).

### Building cores

`scripts/build_cores.py --roms <folder>` builds a core for every set it
finds a ROM set for. Each core repository has its own way to make a core
(static recompilation for Model 1, 2 and Classic 2D; Supermodel's machine
for Model 3; x86 translation for Lindbergh and Chihiro); the script
calls each one the same way and puts the result in
`build/cores/<platform>/`. It can be limited with `--sets` or
`--boards`, and builds iOS, tvOS and arm64 Linux cores from the code it
generates on the host.

A stamp file beside each core records the ROM set, the core repository's
commit and the script's version it was built from, so an unchanged core
is skipped. Everything under `build/` is derived from ROMs and is never
committed.

The Apple build copies whatever is in `build/cores/<platform>/` into the
app and signs it with the app. The Linux app looks for cores beside its
binary or in `build/cores/linux/`.

### Loading a core

The app opens a core only when a game starts. To draw the library it
only checks that the core is there and was built for the same interface
major, from a small `afcore-interface` file the build writes into each
core, so starting the app does not open dozens of libraries (#193; the
library used to take seconds to appear on iPad).

A core is opened with `dlopen` and found by its one exported symbol. It is
closed when the game ends.

### A session

A session is one game being played: the core, its sound, its controls
and its screen.

- **Timing.** Frames follow the board's rate, not the display's. Each
  display refresh reads the controls and runs the frames that are due, so
  a 57.52 Hz game on a 60 Hz display runs 57.52 frames a second.
- **Game on its own thread.** On by default on every platform. The core's
  frames run on a thread of their own, so a slow frame never holds up the
  display, which always draws the newest screen. It can be turned off per
  device (Apple Settings and pause menu; Linux Settings or
  `--no-game-thread`). On Linux, Lindbergh games keep to the window's
  thread; Chihiro uses the game thread.
- **Screen.** Apple: each frame's pixels go into a Metal texture drawn as
  one quad, placed by the screen settings (shape, size, position) and
  through an effect (none, scanlines, CRT). A core with `screen_surface`
  hands over a GPU surface instead, which saves a copy. Linux: an SDL
  texture, the same settings and effects.
- **Sound.** The core's streams are read each frame into a ring buffer per
  stream and played by the platform's audio (AVAudioEngine; SDL audio).
  A full buffer drops the oldest samples rather than slowing the game.
- **Controls.** Read once a frame into `afcore_input`: steering, gas and
  brake, buttons for each player, guns' aim and triggers. Each game's
  `controls` (drive, fight, gun, flight, twin and so on) picks the layout.
  The controller documents cover the details.
- **The pause menu.** Opened with Options and Menu together, Escape, or on
  Apple TV the remote's Menu. Resume, operator mode (Test and Service),
  the game's own screen settings, controller buttons, full screen, use
  this screen as the game's picture, quit.
- **Speed readout.** Screen and game frames a second: the Mac's window
  title; on iPhone, iPad and Apple TV over the game when Show speed is
  on; on Linux the window title, and a corner box in full screen.

### Cabinets and saves

Each device is a cabinet: an ID of its own and a name the user picks.
A game's saves (the board's EEPROM and backup RAM, under
`Saves/<set>/`) belong to that cabinet. They hold the cabinet's settings
and records, and also its link role (car number, master or slave), so
they are never shared between cabinets: sharing would break link play.

On Apple devices each cabinet's saves are backed up to iCloud
(CloudKit, the user's private database), per cabinet. A game starts from
the cabinet's newest backup; an upload never replaces a later one; a
failed upload is tried again when the network returns or the app comes
to the front. A new device can continue as an existing cabinet (Continue
as), and a device can split off as a new cabinet. On iOS and tvOS the
cabinet ID is kept in the Keychain, which outlives a cleared cache; on
macOS in the app's preferences. Linux keeps saves on the machine only.

### Link play

Linked cabinets (cabinet versus cabinet) run one game on several devices
on the same network, as the arcade did. The cores link over their own
sockets, in a ring: each cabinet listens on a port and connects to the
next one (`link_start`), optionally in lockstep. The app's part is
finding the others: every running app advertises itself over Bonjour
(`_arcadefoundry._tcp`) with its cabinet ID, name and each linkable
game's port, so the next cabinet is picked by name. On Linux the ring is
given on the command line. Which games link, and how many cabinets each
takes, is in [LINK_PLAY.md](../../LINK_PLAY.md).

### ROM sets and where files live

| Platform | App folder (holds `ROMs/`, `Saves/<set>/`, `Pictures/`) |
| --- | --- |
| macOS | Application Support in the app's sandbox container, `Arcade Foundry/` |
| iPhone, iPad | the app's Application Support, `Arcade Foundry/` |
| Apple TV | the app's Caches, `Arcade Foundry/` (the system may clear it) |
| Linux | `~/.local/share/arcade-foundry/` |

ROM sets are MAME's: `<set>.zip` or `.7z`, with parent and device sets
found by name. A Lindbergh or Chihiro game is its game folder, or the
same folder as `<set>.tar` so it can travel like a ROM set; it is
unpacked on first play. The user's ROM folder can hold the sets
directly or one folder per board; the app and the scripts find each set
wherever it is and link them into one flat folder, so each core finds
its parent and device sets beside it.

On Apple devices sets come from three places: Add ROM, the app's
iCloud Drive folder (shared by the user's Macs, iPhones and iPads), and
a ROM server on the home network (any web server; the repository's
`serve_roms.py` finds sets in board folders). Apple TV has no iCloud
Drive, so it uses the ROM server. A downloaded set is checked with the
core's `check_roms` before it is used. The ROM server document covers
the rest.

### Library pictures and game art

Each game has one picture, on its card and at the top of its page. Which
one depends on whether MAME runs the game's board, because MAME is where
the downloaded title and in-game screens come from:

- **Boards MAME runs** (Model 1, 2 and 3, the 2D boards): the player's own
  picture (the pause menu's "Use this screen as the picture", or an image
  they chose), else the downloaded title screen (or in-game shot), else
  the screen the game took of itself after half a minute, else the
  downloaded flyer.
- **Boards MAME can't run** (Lindbergh, Chihiro): the player's own
  picture, else the downloaded flyer, else the screen the game took of
  itself after two and a half minutes (these games are still loading at
  half a minute). The automatic screen never replaces a flyer; only a
  picture the player saves or chooses does (#377).

A flyer is shown whole, not cropped. On Apple, when the card shows the
automatic screen, the game's settings can remove it (`Pictures.card`,
`Pictures.cardIsScreen`; Linux `card_picture` in `art.cpp`). Its other art (flyer, marquee,
cabinet, in-game) is a row under About this game, each opening full size;
on Apple TV the row is focusable.

The title and in-game pictures in both sources are taken in MAME. For a
board MAME can't run yet (Lindbergh, Chihiro) they are MAME's own screen,
its "Screenless System" card or noise, so those two kinds are never
downloaded or shown for these sets (`GameArt.noMAMEScreens`,
`art_wanted`), and any downloaded before are removed when the app starts.

The art is Sega's, so the app ships none: each device downloads it for
itself (`GameArt` on Apple, `art.h` on Linux), by MAME set name, from the
Arcade Database's scraper service (adb.arcadeitalia.net), and from
libretro-thumbnails (by the MAME description the Arcade Database gives)
when that has none of a kind. The title screen downloads when a card is
first drawn, the rest when the page opens; one download at a time on one
connection, none while a game plays. Art is kept for good and never
checked again. The Arcade Database never answers 404: a missing picture
comes back as an all-black frame at the game's resolution, a 140 x 105
"no image" picture, or its home page, so anything that isn't an image,
is under 160 x 120 or is one colour counts as missing. A kind neither
source has is remembered as a `.miss` file,
and a failed connection is tried again on the next start. Only the set
name leaves the device.

| Platform | Pictures | Downloaded art |
| --- | --- | --- |
| macOS | `Pictures/<set>.png`, own in `Pictures/Own/` | Application Support, `Arcade Foundry/Art/<set>/` |
| iPhone, iPad | as on macOS | Caches, `Arcade Foundry/Art/<set>/` (may be cleared; downloads again) |
| Apple TV | as on macOS (in Caches) | as on iPhone |
| Linux | `pictures/<set>.bmp`, own in `pictures/own/` (`.png` or `.bmp`) | `art/<set>/` |

Settings has "Download game art" (on by default) and "Remove downloaded
art", which never touches the player's own pictures. The Linux app
downloads only when built with libcurl, and reads PNG (and BMP) only, so
art served as JPEG counts as missing there. `AF_ART_URL` points both
sources at a test server.

### Settings

Apple settings are `UserDefaults` keys (`SettingKey` in Settings.swift):
every game's screen settings and each game's own (`screenLook` and
`screenLook.<set>`), smooth pixels, volume, library filter and order,
appearance (dark by default), game thread, show speed, ROM server,
add-to-iCloud, replays, download game art. The ROM server address is shared between the
user's devices through iCloud key-value storage; everything else is per
device.

Linux keeps the same choices in small `key=value` files in its data
folder: `settings` (every game's screen settings, show speed, game
thread, download game art), `library`, `fullscreen`, and per game `screen/<set>`,
`buttons/<set>` and the gun and region settings. Command-line options win
over them for that run and are not saved.

A game's own screen settings win over every game's.

### The two front ends

**Apple** (`apple/`): one SwiftUI app, three targets (macOS, iOS and
iPadOS, tvOS) over the same sources, generated by XcodeGen from
`apple/project.yml`. The source folders follow the parts above:
`Library/` (catalog, library, game pages, settings, cabinets, ROM sync,
packages, pictures, credits), `Session/` (the session and link
discovery), `Core/` (loading cores), `Render/` (the screen and its
settings), `Audio/`, `Input/`, `Video/` (gameplay videos), `VR/`
(headset play). Signing and the bundle ID come from `Local.xcconfig`,
which each builder keeps for themselves.

**Linux** (`linux/`): one SDL3 program, `insert-coin`, drawn with SDL's
renderer and its built-in text, in parts: `library` (the game list, a
game's page, Settings, credits), `session` (a game), `input` (keyboard,
mouse, gamepads, wheels), `render` (the screen and the pause menu), and
`main.cpp` (the command line and the menus' loop). It reads the same
`games.toml`-generated list, the same histories and credits, and builds
from the same scripts. It is a functional twin of the Apple app, not a
port of its code.

The SDL3 program builds for two systems:

| | Linux | macOS |
| --- | --- | --- |
| Who it is for | Linux PC, Raspberry Pi, Steam Deck | a Mac kept as a cabinet or kiosk (full screen, controllers or a wheel, no mouse or keyboard); checking interface changes without a Linux machine |
| Build | `cmake -S linux -B build/linux` | `cmake -S linux -B build/sdl-mac` |
| Cores | `build/cores/linux` (`libafcore_<set>.so`) | the Apple app's, `build/cores/macosx` (`afcore_<set>.framework`) |
| Renderer | SDL's default (OpenGL) | SDL's Metal renderer |
| Screen | the core's pixels, uploaded each frame | a core's GPU buffer (`screen_surface`: Model 3) wrapped as a texture with no copy, as the Apple app shows it; the others' pixels |
| Files | `~/.local/share/arcade-foundry/` | the same folder, apart from the Apple app's: no iCloud saves, no ROM server |

On a Mac it has the Linux app's features, not the SwiftUI app's: no iCloud
saves, ROM sync, ROM server, touch or gameplay videos, and the SwiftUI app
has no switch to it (#387). It is in the public build instructions. As a
check of the Linux interface it can't cover what is Linux's own (desktop
OpenGL, Mesa, the Linux sound, gamepad and wheel drivers), so changes are
still checked on Linux.

## Platforms

| | macOS | iOS / iPadOS | tvOS | Linux |
| --- | --- | --- | --- | --- |
| Minimum | macOS 26 (Apple silicon) | iOS 16 | tvOS 16 | glibc 2.36, 64-bit |
| Input | controller, keyboard, mouse, wheel | controller, touch, keyboard | controller, remote | controller, keyboard, mouse, wheel |
| ROM sets from | Add ROM, iCloud Drive, server | Add ROM, iCloud Drive, server | server | ROM folder |
| Saves backed up | iCloud, per cabinet | iCloud, per cabinet | iCloud, per cabinet | no |
| Lindbergh | yes | paid developer team | no | yes |
| Chihiro | yes | no | no | yes |

- **Apple builds are personal sideloads.** Nothing goes to the App Store
  or TestFlight: the cores are built from the user's ROMs, and the
  Model 3 cores are GPL. Each user builds with their own developer team.
- **Lindbergh on iOS** needs the extended virtual addressing entitlement,
  which only a paid developer team can sign. tvOS cannot give a game its
  4 GB address window, so Lindbergh is not on Apple TV yet (planned for
  Apple TV 4K third generation and later with the Metal renderer).
- **No code is loaded from outside the app** on iOS and tvOS, and there
  is no JIT: every core is compiled ahead of time and signed with the app.
- **macOS 26** is the minimum because the Mac can drive an Apple Vision
  Pro for headset play.
- **Linux arm64** builds come from a cross-build (`scripts/build_arm64.py`)
  or natively on the board.
- **Windows** is a roadmap item only (#296).

## Testing

- **CI on every pull request:** the release scan (no ROM content, no
  private names or paths) and a ROM-free Linux build, plus
  `gen_games.py --check`.
- **Nightly, with ROMs,** on a self-hosted Mac: macOS cores and app,
  Linux cores and app with a screen hash per set after a fixed number of
  frames (`insert-coin --play <set> --frames N --screenshot-hash`), then
  each core through its own repository's tool against stored baselines.
  Each run starts from fresh saves.
- **By hand, on devices:** every game must pass its test mode and a full
  game to game over before it is listed. Apple TV, iPad and Raspberry Pi
  speed are measured on the devices, from the Home Screen, never under
  the debugger.
- The Apple sources are type-checked for all three platforms before a
  change merges.

## Versions and releases

- **App version** x.y.z, the same in `apple/project.yml` and
  `linux/CMakeLists.txt`; `scripts/check_release.py` refuses a mismatch.
  x.y.1 is fixes, x.1.0 features.
- **Branches.** `af-next` is where work happens and the default branch;
  `main` is released code. Feature branches are squash-merged into
  `af-next`. Fixes for a released version go to `main`, are released, and
  `main` is merged back into `af-next`.
- **Development and public repositories.** Work happens in private
  `-devel` repositories. A release exports each one into its public
  repository as a single commit with a tag, cores first, then the app with
  its submodules pinned to the public cores' release commits. The export
  scans for ROM-derived files and secrets, and leaves out developer-only
  files (`*-DEVELOPER.md`, STATE.md, HANDOFF.md and its monthly logs).
- **State and history.** STATE.md says where the app stands in one page;
  HANDOFF.md is the dated log, newest first.

## Open items

- Model 3 link play (the cores' NetBoard wired to `link_start`).
- Lindbergh on Apple TV and the Metal renderer as the default for
  Lindbergh and Chihiro on Apple devices.
- Chihiro on iOS and iPadOS.
- Link play token and non-blocking sends, and one `AF_DEBUG_*` set of
  debug switches across the cores.
- Saves backed up or synced on Linux: a roadmap item, to be
  investigated and then built. Candidates are Dropbox, Google Drive and a
  home server; each cabinet's saves stay its own, as on Apple devices.
- Model 3 on the Raspberry Pi (GLES renderer).
- Headset play: GPU stereo at 90 Hz, gestures, a standalone visionOS app,
  and other headsets.
- Windows (#296).
- Game art: the Arcade Database's terms ask for contact with its author
  before release (#352).

## Decisions

- **The front end and the cores are separate repositories,** and each
  game is its own core library behind one C interface. A new board is a
  new core repository, not new app code.
- **Cores are loaded at run time (`dlopen`)** from whatever the build put
  in the app, so the app builds and runs with any subset of cores.
- **Cores are built locally from the user's ROM sets.** Nothing derived
  from a ROM is ever committed, released or downloaded.
- **SwiftUI and Metal on Apple; SDL3 on Linux.** Native on each, rather
  than one cross-platform UI, so tvOS and touch feel right and Linux stays
  light enough for a Raspberry Pi.
- **The SDL3 program also builds on macOS** (#387), with the Linux app's
  features: for a Mac kept as a cabinet or kiosk, and for checking the
  Linux interface. The SwiftUI app stays the Mac's full app (iCloud saves,
  ROM sync) and has no switch to the SDL3 one.
- **The interface's minors only append,** and the app loads only cores of
  its own major. Old cores keep working with a newer app within a major.
- **One game list, generated** from the cores' and the app's
  `games.toml`; `hidden` is the only way to keep a game out.
- **The library never opens a core** to draw itself; a small version
  file in each core says what it was built for.
- **Frames follow the board's rate,** and the game runs on its own thread
  by default on every platform.
- **Saves are per cabinet and never shared,** because they hold the
  cabinet's link role. iCloud backup is per cabinet, private, and newest
  wins.
- **Apple TV gets ROM sets from a ROM server,** since it has no iCloud
  Drive. The server is any plain web server on the home network.
- **No App Store or TestFlight.** Apple builds are personal sideloads,
  which also lets them include the GPL Model 3 cores.
- **Game art is downloaded on the player's device, never shipped,**
  because it is Sega's (#352). The player's own picture always wins over
  it, and the art is never updated once kept.
- **Dark appearance by default,** with Light and Match the system as
  choices; the Linux menus are always dark.
- **Private `-devel` repositories and public release repositories,** one
  commit per release in the public ones, so the public history holds only
  released code.
