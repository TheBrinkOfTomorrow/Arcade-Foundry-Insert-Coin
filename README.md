# Arcade Foundry: Insert Coin

An arcade front end for Sega's classic 2D boards (Out Run, X Board), Model 1,
Model 2, Model 3 and Lindbergh (and beyond?) games: 

The games are cores, one library per game with a C interface (`afcore.h`), 
built from your own ROM sets: the classic 2D, Model 1/2 and Lindbergh games are
statically recompiled, and Model 3 runs Supermodel's machine.

| Front end | Platforms | Where |
| --- | --- | --- |
| SwiftUI, Metal | macOS (Apple silicon), iOS, iPadOS, tvOS | `apple/` |
| SDL3 | Linux desktop, Steam Deck and Raspberry Pi 4/5 | `linux/` |

- [GAMES.md](GAMES.md): the supported games, their set names and ROM
  notes, the platforms each runs on, and their controls.
- [BUILD.md](BUILD.md): building the cores and the apps, and [updating](BUILD.md#updating) them (`git pull && git submodule update --init`).
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
| Linux | `~/.local/share/arcade-foundry/` (`roms/`, `games/`, `saves/`, `pictures/`, `buttons/`) |

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
lists each game's controls. 
A game controller's Back/Options inserts a coin,
Start presses start, and both together open the pause menu.

On a Mac, iPhone, iPad or Apple TV a each game has it's own settings within which 
the controller buttons can be (re)assigned.
(Controller: choose an action, then press its new button).
On Linux the setting is located in the pause menu : choose a button, then press the one to use in its
place (a trigger works too); Reset goes back to the default. Saved per
game in `~/.local/share/arcade-foundry/buttons/<romset>`.

### Library
Library: one card per game and platform, with filters: 
#### Platform:
(All, Classic 2D, Model 1, Model 2, Model 3, Lindbergh) next to
Settings, and 
#### All | Playable:
Playable shows only the games with a core in this build and a ROM set on
the device or in iCloud Drive (remembered); C
#### View type - Cards | List:
toggles between individual tiles or compact rows with a small picture

### Game Cards
A card opens the game's page: its versions (e.g. Daytona USA and
Daytona USA Revision A), each played, downloaded or added there, and the
options of the version chosen under "Options for" (link play, picture,
controller, ROM set). With a controller on a Mac or iPad the d-pad moves
between cards and versions, A opens or plays, B goes back. Below the
versions, "About this game" tells the game's history, with links to read
more (not on Apple TV).

### Appearance
Appearance: the app is dark by default. Settings > Appearance switches it
to Light or to Match the system (macOS, iOS and tvOS). On Linux the menu is
always dark.

### Credits
Credits: everyone whose work the app and cores use, with their licences,
are in [CREDITS.md](CREDITS.md). The app shows them under Settings > About
> Credits (iPhone, iPad, Apple TV), in the About window from the app menu
(Mac), and from the game list with C or the gamepad's West button (Linux,
or `insert-coin --credits`).

### Screen size and effects
There are golboal settings, and also per-game settings 
#### Shape:
- Original (4:3 for the classic 2D boards and Model 1 through 3, wider for Lindbergh/other)
- Stretch to fill
- Whole-number scale (every line of the game as tall, best with scanlines)

#### Effect: 
- None
- Scanlines
- CRT (a curved tube, the shadow mask and a glow; on Linux without the curve and glow)
- Smooth pixels, Size (80-120%) and Move across/down, for TVs that cut off the picture's edges

### Library pictures: 
each game's card shows the game's screen, taken by itself
after half a minute of its first play, or with "Use this screen as the
picture" in the pause menu. On a Mac, iPhone or iPad the game's page
can use an image of your own instead.

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
The screen options below (`--shape`, `--effect`, `--sharp`, `--size`,
`--move`) are for every game; Shape and Effect in a game's pause menu
change that game's own.

Esc opens the pause menu: Resume, Operator mode, Shape, Effect, Controller
buttons, Use this screen as the picture, Quit to library (back to the
game's page). C (or the
gamepad's West button) in the game list shows the credits.

### Command-line options

`insert-coin --help` (or any unknown option) prints them all.

| Option | What it does |
| --- | --- |
| `--roms DIR` | Read the ROM sets from DIR (the folder that holds the board folders, or the sets themselves) instead of `~/.local/share/arcade-foundry/roms`. |
| `--cores DIR` | Load the game cores from DIR instead of the ones built next to the app. |
| `--fullscreen` | Start in full screen (F11 switches while it runs). |
| `--play ROMSET` | Go straight into a game, by its MAME set name (`daytona`, `hotd4a`). |
| `--speed` | Print the speed once a second (screen fps, game fps and its share of the arcade's rate) and, when the game ends, the whole run's average and its slowest second. For speed reports: `insert-coin --play daytona --speed \| tee speed.txt`. |
| `--no-game-thread` | Run Model 1 and 2 games on the window's thread, as before. By default they run on a thread of their own, so drawing to the screen never delays the next game frame (it smooths slow machines such as a Raspberry Pi 4). |
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
speed in the window title.

## License

BSD-3-Clause, see `LICENSE`. The game cores in `cores/arcade-foundry-cores-model12`
keep their own BSD-3-Clause licence and third-party notices
(`THIRD_PARTY.md` there). Cores built from your ROMs are derived from them
and are not covered by this licence; don't redistribute them. Model 3
cores (the `cores/arcade-foundry-cores-model3` submodule) are GPL-3.0-or-later,
as Supermodel is.
