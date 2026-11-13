# Arcade Foundry: Insert Coin

An arcade front end for Sega's classic 2D boards (Hang-On, Out Run, X Board, System 32), Model 1,
Model 2, Model 3 and Lindbergh (and beyond?) games: 

The games are cores, one library per game with a C interface (`afcore.h`), 
built from your own ROM sets: the classic 2D, Model 1/2 and Lindbergh games are
statically recompiled, and Model 3 runs Supermodel's machine.

| Front end | Platforms | Where |
| --- | --- | --- |
| SwiftUI, Metal | macOS (Apple silicon), iOS, iPadOS, tvOS | `apple/` |
| SDL3 | Linux desktop, Steam Deck and Raspberry Pi 4/5; also macOS (Apple silicon) | `linux/` |

The SDL3 app on macOS has the Linux app's features, not the SwiftUI app's
(no iCloud saves, ROM sync, ROM server, touch or gameplay videos). It suits a
Mac set up as a cabinet or kiosk: full screen, game controllers and a wheel,
no mouse or keyboard needed. See [The SDL3 app on a Mac](BUILD.md#the-sdl3-app-on-a-mac).

- [GAMES.md](GAMES.md): the supported games, their set names and ROM
  notes, the platforms each runs on, and their controls.
- [BUILD.md](BUILD.md): building the cores and the apps, and [updating](BUILD.md#updating) them (`git pull && git submodule update --init --recursive`).
- [FAQ.md](FAQ.md): where the app keeps files, ROM sets, saves and
  cabinets.
- [LINK_PLAY.md](LINK_PLAY.md): cabinet versus cabinet (VS): which games
  link, how many cabinets each takes, and how to set it up.
- [STEAM_DECK.md](STEAM_DECK.md): a step-by-step guide to setting up and
  playing on a Steam Deck.
- [ROADMAP.md](ROADMAP.md): things we hope to add, in no order and with
  no promises or dates.

Arcade Foundry: Insert Coin is not affiliated with or endorsed by Sega.

## ROM sets

> **Apple TV needs a ROM server.** Apple TV has no iCloud Drive, so its
> only way to get ROM sets is a [ROM server](#rom-server) on your network,
> for example a Mac running
> `python3 scripts/serve_roms.py --roms /path/to/roms`. The server must be
> running whenever Apple TV downloads: the first time, and again whenever
> tvOS clears the app's cache, which it may do at any time. If the server is
> off, the games stay "No ROM set yet".

Nothing here contains ROM data or code generated from it. Each game's core
is generated from your own MAME ROM set on your machine (see
[BUILD.md](BUILD.md)), and the app loads your ROM set at run time, from one
of:

- **Your ROM server** (every platform, the only one on Apple TV): any web
  server with the sets (see [ROM server](#rom-server)); set its URL in
  Settings and the app downloads `<url>/<romset>.zip` (or `.7z`) once per
  device ("Download all" fetches every missing one; Apple TV does that by
  itself). The URL is shared between your devices through iCloud.
- **iCloud Drive** (Mac, iPhone, iPad): the app's folder, iCloud Drive >
  Arcade Foundry. Add ROM puts sets there (a setting), or drop
  `<romset>.zip` into it from the Finder or Files; every device signed in
  to the same Apple Account sees them and downloads one when it is played.
- **This device only**: Add ROM with the iCloud Drive setting off.

Each device is a cabinet with its own saves (settings, link role, records).
They are backed up to your iCloud per cabinet (retried when the device is
back online) and restored if the device loses them (Apple TV's cache being
cleared). A replacement device can take over an old cabinet with Transfer;
devices sharing a cabinet then take turns on one save history, until "Make
this a new cabinet" splits them. Settings can also copy one game's save from
another cabinet. See the [FAQ](FAQ.md#cabinets-and-saves).

### File locations

| Platform | Folder (holds `ROMs/`, `Saves/<romset>/`, `Pictures/`) |
| --- | --- |
| macOS | `~/Library/Containers/com.<your-developer-url>.arcadefoundry.insertcoin/Data/Library/Application Support/Arcade Foundry/` (the app is sandboxed; ⇧⌘G in the Finder opens it) |
| iPhone, iPad | the app's Application Support, `Arcade Foundry/` |
| Apple TV | the app's Caches, `Arcade Foundry/` (the system may clear it) |
| Linux | `~/.local/share/arcade-foundry/` (`roms/`, `games/`, `saves/`, `states/`, `pictures/`, `art/`, `buttons/`) |

Downloaded game art is in `Art/` beside them on the Mac (`art/` on Linux), and in the app's Caches (`Arcade Foundry/Art/`) on iPhone, iPad and Apple TV.

ROM sets in iCloud Drive are in the app's folder, iCloud Drive > Arcade
Foundry.

### ROM server

Any web server that serves the sets will do. The simplest is Python's, on a
Mac or Linux machine on the same network as your devices:

1. Put the sets in one folder, named as MAME names them: `<romset>.zip` or
   `<romset>.7z`. A Lindbergh game is `<set>.tar` (see
   [BUILD.md](BUILD.md#lindbergh-games)). They can be directly in it or
   in a folder per board (`Model2/srallyc.zip`, see
   [BUILD.md](BUILD.md#the-rom-folder)).
2. Serve that folder with the repository's script, which finds each set
   wherever it is in the folder:

       python3 scripts/serve_roms.py --roms /path/to/roms

   Any other web server works too, but only for a flat folder (every set
   directly in it): the app asks for `<URL>/<romset>.zip`. Python's
   `python3 -m http.server 8000 --directory /path/to/roms` is one.

3. Find the machine's address on your network: on a Mac
   `ipconfig getifaddr en0` (or `en1`), on Linux `hostname -I`.
4. In the app's Settings, set the ROM server to `http://<address>:8000`.
   Allow local network access when asked (iPhone, iPad, Apple TV).

The server only has to run while a device downloads: each device fetches a
set once and keeps it (Apple TV fetches again if the system clears its
cache). For a server that is always there, run the same command on a
machine that stays on (a NAS, a home server). Plain http on your own
network is fine; don't expose it to the internet.

## Using the app

### Controllers

Controllers, keyboards, mice and touch all work; [GAMES.md](GAMES.md#controls)
lists each game's controls. A game controller's Back/Options inserts a coin,
Start presses start, and both together open the pause menu.

On an iPhone or iPad without a controller, an on-screen controller is drawn
over the game, with Coin, Start and Pause along the top; the gun games aim
by touch instead. Settings > On-screen controller chooses when it shows
(Automatic: while no controller is connected; Always; Off), and each game's
settings can choose differently, and let a gun game aim with the on-screen
controller instead of touch. In the driving games the stick steers and the
buttons on the right are the pedals: A gas, B brake, X and Y shift down and
up, the right shoulder changes the view.

On a Mac, iPhone, iPad or Apple TV each game has its own settings, where the
controller buttons can be reassigned (Controller: choose an action, then
press its new button). On Linux the setting is in the pause menu: choose a
button, then press the one to use in its place (a trigger works too); Reset
goes back to the default. Saved per game in
`~/.local/share/arcade-foundry/buttons/<romset>`.

### Library

One card per game and platform, with three controls next to Settings:

- **Platform**: All, Classic 2D, Model 1, Model 2, Model 3, Lindbergh or Chihiro.
- **All | Playable**: Playable shows only the games with a core in this build
  and a ROM set on the device or in iCloud Drive. The choice is remembered.
- **Cards | List**: tiles with the game's picture, or compact rows with a
  small picture.

### Game pages

A card opens the game's page: its versions (for example Daytona USA and
Daytona USA Revision A), each played, downloaded or added there, and the
options of the version chosen under "Options for" (link play, picture,
controller, ROM set). With a controller on a Mac or iPad the d-pad moves
between cards and versions, A opens or plays, B goes back. Below the
versions, "About this game" tells the game's history, with links to read
more (not on Apple TV).

### Appearance

The app is dark by default. Settings > Appearance switches it to Light or
to Match the system (macOS, iOS and tvOS). On Linux the menu is always dark.

### Credits

Everyone whose work the app and cores use, with their licences, is in
[CREDITS.md](CREDITS.md). The app shows them under Settings > About >
Credits (iPhone, iPad, Apple TV), in the About window from the app menu
(Mac), and on Linux under Settings > About > Credits, from the game list
with C or the gamepad's West button, or with `insert-coin --credits`.

### Screen size and effects

These are global settings in Settings, and each game can have its own on its
page or in the pause menu.

- **Shape**: Original (4:3 for the classic 2D boards and Model 1 through 3,
  wider for Lindbergh), Stretch to fill, or Whole-number scale (every line
  of the game as tall, best with scanlines).
- **Effect**: None, Scanlines, or CRT (a curved tube, the shadow mask and a
  glow; on Linux without the curve and glow).
- **Smooth pixels**, **Size** (80-120%) and **Move** across and down, for
  TVs that cut off the picture's edges.

### Library pictures and game art

Each game's card, and the top of its page, shows one picture, the first of:

1. **Your own**: the screen you keep with "Use this screen as the picture"
   in the pause menu, or (Mac, iPhone, iPad) an image you choose on the
   game's page. On Linux, a PNG of your own at
   `pictures/own/<romset>.png` works too.
2. **The game's title screen**, downloaded (or its in-game screen when there
   is no title screen).
3. **A screen from the game**, taken by itself after half a minute of its
   first play (two and a half minutes on Lindbergh and Chihiro, which are
   still loading at half a minute). On a Mac, iPhone or iPad, the game's
   settings can remove it (Picture > Remove the screen from the game), and
   the next play takes another.
4. **The game's flyer**, downloaded.

For the Lindbergh and Chihiro games no title or in-game screen is
downloaded: the ones available come from MAME, which can't run those
boards yet, so they show its "Screenless System" card or noise. Their cards
show your own picture, else the flyer; the screen from the game is used
only for a game with no flyer.

The app ships no art: the art is Sega's, so your device downloads it for
itself as you browse, one picture at a time and never while a game plays,
from the [Arcade Database](https://adb.arcadeitalia.net) and, when that has
none, [libretro-thumbnails](https://github.com/libretro-thumbnails/libretro-thumbnails).
Only the game's MAME set name is sent. The game's page also shows its flyer,
marquee, cabinet and in-game screen under About this game; each opens full
size (on Linux, I or the gamepad's North button). The art is kept for good
and never checked again. Settings > Game art turns the downloads off and
removes the downloaded art (your own pictures stay). iPhone, iPad and Apple
TV keep it where the system may clear it, and it downloads again.

The Linux app downloads art only when built with libcurl
(`libcurl4-openssl-dev` or `libcurl-devel`); without it, the cards show the
screens from the games.

## Gameplay videos (Apple platforms only)

For trailers and gameplay videos (YouTube, say), the Mac makes MP4s
straight from the game's own picture and sound, with no window: 4K or
1080p at 60 frames a second, the picture sharp at its cabinet shape, the
game's name, system and year in the lower left, and the logo with
arcadefoundryinsertcoin.com in the upper right. Each video ends with its
last second fading to black and four seconds of the logo, the website and
the YouTube channel (youtube.com/@ArcadeFoundryInsertCoin). Nothing is filmed off the screen, so there are no menus, no dropped
frames and no lag, and it runs faster than the game would.

- Attract mode: `scripts/make_videos.sh daytona` records the game's own
  demo loop (no controls) for two minutes; `all` does every game with a
  core and ROM set.
- Replays: turn on Record replays in Settings for every game, or give a
  game its own setting on its page or in the pause menu, as with the
  screen settings (Mac, iPhone, iPad; not yet on Apple TV, see issue #145). Every frame's
  controls are kept from the moment the game starts, so it can go on
  partway through. When you leave the game, a small `.afreplay` file is
  saved to the Replays folder in iCloud Drive (Arcade Foundry), and a red
  dot shows while it records. On the Mac, `scripts/make_videos.sh replays`
  turns each new one into a video of the game exactly as it was played.
  A replay plays back only with the same version of the game's core, and
  not for link play.

Videos are stored in `~/Movies/Arcade Foundry`. Options: `--seconds N`, `--skip N`
(seconds of the boot left out), `--lines 1080`, `--hevc`, `--no-caption`,
`--no-logo`, `--no-end-card`, `--no-sound` (no sound track, for music
added in editing), `--music FILE` (a track under the game's sound, which
`--game-volume 0.35` sets), `--intro FILE` (a screen recording of the app
opening the game, from an iPad or the Mac, played first and dipping to
black into the game; `%s` in FILE stands for the game's set),
`--fresh-saves` (the game's factory settings, not yours).
The games and their music belong to Sega: say in a video's description
that Arcade Foundry: Insert Coin isn't affiliated with Sega, and never show or link
where to get ROM sets.

## Linux

The Linux app, `insert-coin` (see [BUILD.md](BUILD.md#linux)), reads the
ROM sets from `~/.local/share/arcade-foundry/roms` unless `--roms` names
another folder; either folder can keep them in a folder per board (see
[BUILD.md](BUILD.md#the-rom-folder)). 
A Lindbergh game (`hotd4a/disk0/...`) or `hotd4a.tar`,
unpacked into `~/.local/share/arcade-foundry/games/` the first time it is
played (and again when the .tar changes).
In the game list, as in the Mac's library: P (or a gamepad's North button) 
shows all games or only the playable ones; Left/Right (L1/R1) step through the platforms (All, Classic 2D,
Model 1, Model 2, Model 3, Lindbergh); S and Shift+S (R2/L2) through the orders
(A-Z, Year, Platform, Recently played). All three are remembered. The
list has a row per game; Return (South) opens its page, as on the Mac: its
versions, each with why it can't be played if it can't (Return or South
plays it), the chosen version's Shape, Effect and Controller buttons, and
About this game; Up/Down move through it, Esc (East) goes back to the list.
Tab (or the gamepad's Start button) in the game list opens Settings:
every game's Shape, Effect, Smooth pixels, Size, Move across and Move down;
Full screen; Show speed (the speed in the title bar, and in a corner in
full screen); Game on its own thread; and About, with the version and
Credits. Up/Down choose, Left/Right or Return (South) change, Esc (East)
goes back. They are remembered. Shape and Effect in a game's pause menu or
on its page change that game's own, which win over Settings. The screen
options below (`--shape`, `--effect`, `--sharp`, `--size`, `--move`,
`--no-game-thread`) win over Settings for that run and are not saved.

Esc opens the pause menu: Resume, Operator mode, Shape, Effect, Controller
buttons, Use this screen as the picture, Quit to library (back to the
game's page). C (or the
gamepad's West button) in the game list shows the credits.

Everything in this section also applies to the SDL3 app built on a Mac
([BUILD.md](BUILD.md#the-sdl3-app-on-a-mac)), with the same folder in your
home folder.

### Command-line options

`insert-coin --help` (or any unknown option) prints them all.

| Option | What it does |
| --- | --- |
| `--roms DIR` | Read the ROM sets from DIR (the folder that holds the board folders, or the sets themselves) instead of `~/.local/share/arcade-foundry/roms`. |
| `--cores DIR` | Load the game cores from DIR instead of the ones built next to the app. |
| `--fullscreen` | Start in full screen (F11 switches while it runs). |
| `--play ROMSET` | Go straight into a game, by its MAME set name (`daytona`, `hotd4a`). |
| `--resume` | With `--play`: start the game from its Resume slot (Settings > Resume where I left off). |
| `--speed` | Print the speed once a second (screen fps, game fps and its share of the arcade's rate) and, when the game ends, the whole run's average and its slowest second. For speed reports: `insert-coin --play daytona --speed \| tee speed.txt`. |
| `--no-game-thread` | Run Model 1, Model 2 and Chihiro games on the window's thread, as before. By default they run on a thread of their own, so drawing to the screen never delays the next game frame (it smooths slow machines such as a Raspberry Pi 4). |
| `--frames N` | Quit after N game frames, run as fast as the machine goes (for tests and benchmarks; with `--speed` the total is the unthrottled speed). |
| `--screenshot-hash` | With `--frames`: print a hash of the last frame (tests). |
| `--link-port PORT` | Link play: listen on PORT for the cabinet before this one. |
| `--link-next HOST:PORT` | Link play: the next cabinet in the ring. |
| `--link-sync` | Link play: keep the cabinets in lockstep. |
| `--shape original\|stretch\|integer` | The screen's shape, for every game. |
| `--effect none\|scanlines\|crt` | The screen effect, for every game. |
| `--sharp` | No smoothing. |
| `--size PERCENT` | The picture's size, 80 to 120. |
| `--move X,Y` | Move the picture, in percent of the window, -10 to 10. |
| `--version` | Print the app's version and core interface, then quit. |
| `--credits` | Print the credits, then quit. |

The Mac app takes options only for making videos (`--video`, see
[Gameplay videos](#gameplay-videos-apple-platforms-only)); it shows its
speed in the window title, and, with Settings > Show speed on, in a corner
over the game (for full screen).

## Design documents

How the larger parts are built and why: [docs/design/](docs/design/README.md).
It has the app's designs (its architecture, steering wheels and pedals, and save states) and links to the
cores' designs in their own repositories (the Metal path for the GLES cores,
the x86 recompiler, the Model 1, Model 2 and Model 3 cores, Model 3 speed on ARM64,
Lindbergh, Chihiro, the Classic 2D cores and their 68000 recompiler).

## License

BSD-3-Clause, see `LICENSE`. The game cores in `cores/arcade-foundry-cores-model12`
keep their own BSD-3-Clause licence and third-party notices
(`THIRD_PARTY.md` there). Cores built from your ROMs are derived from them
and are not covered by this licence; don't redistribute them. Model 3
cores (the `cores/arcade-foundry-cores-model3` submodule) are GPL-3.0-or-later,
as Supermodel is.

## Version numbers

The app and each game core are versioned separately. The release number on this
repository (for example v1.0.0) is a snapshot label: it means "as shipped with
Arcade Foundry: Insert Coin 1.0.0". Every game core also has its own x.y.z
version (shown in the app), which can be higher or lower than the release number.
A core's major number only changes when its saves stop carrying over.
