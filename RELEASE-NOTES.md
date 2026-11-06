# Release notes

Newest first. Each release is also on the repository's Releases page.

## 1.2.0 (2026-11-06)

**Arcade Foundry: Insert Coin 1.2.0**

1.2.0 brings two planned versions out together: 1.1.0 (21 more Classic 2D
games, steering wheels, touch controls, and a sturdier base) and 1.2.0 (two
more Lindbergh games, Metal graphics for Lindbergh on Apple devices, region
settings and game art). There was no separate 1.1.0 release. The library now
has 89 ROM sets.

### What came with 1.1.0

- **21 more Classic 2D games, 25 in all** on five Sega boards, each recompiled
  from your ROM set and checked frame by frame against MAME in attract mode,
  test mode and a full game: Hang-On, Space Harrier and Enduro Racer (Hang-On
  board); Turbo Out Run (Out Run board); After Burner (VER 1.31), Line of Fire,
  Racing Hero, Super Monaco GP and GP Rider (X Board); Galaxy Force 2, Power
  Drift, G-LOC Air Battle, Rail Chase and Strike Fighter (Y Board); Rad Mobile,
  F1 Exhaust Note, Rad Rally, Golden Axe: The Revenge of Death Adder, SegaSonic
  the Hedgehog and Jurassic Park (System 32); OutRunners (System Multi 32).
  They run on macOS, iPhone and iPad, Apple TV and Linux. The list is in
  [GAMES.md](GAMES.md).
- **Steering wheels and pedals on Linux and the Mac** (issue #318). A wheel
  steers the driving games, its pedals are the gas and brake, and its shifter
  and buttons shift, change view, insert a coin, start and open the pause
  menu. Wheel and pedals (in the pause menu, and in Settings on the Mac)
  shows the live steering and pedals and sets the steering range (270 degrees
  by default, like a cabinet), a steering curve (50% by default: a gentler
  centre), the wheel's full turn, a dead zone, calibration, self-centring
  (50% by default, 0 turns it off) and the wheel's buttons. The wheel's d-pad
  and buttons work the library and the menus, and the paddles shift as well
  as the shifter. Tested with the Logitech Driving Force GT, which works
  without calibrating; on the Mac the app switches it to its full 900-degree
  mode. Other Logitech wheels, the G29 included, should work after
  calibrating but haven't been tested yet. iPad and Apple TV don't support
  wheels.
- **The on-screen controller on iPhone and iPad is a setting** (issue #359).
  Settings > On-screen controller: Automatic (the default) shows it while no
  controller is connected and follows a controller connected or disconnected
  during a game; Always shows it beside a controller too, as player 2; Off
  never shows it. Each game's settings can choose differently, and a gun game
  can aim with the on-screen controller (stick, A fires, B reloads, X grenade)
  instead of by touch. Virtual-On and Oratorio Tangram get both sticks on
  screen, and touching the picture no longer fires a gun outside the gun
  games. In the driving games the pedals are on the right, where the steering
  thumb isn't (issue #362): A is the gas, B the brake, X and Y shift down and
  up, and the right shoulder changes the view.
- **Downloads are checked.** After a download the app checks the ROM set
  against the files the game needs (each core's own list, and for Lindbergh
  games a list made from verified dumps) and shows the result in Downloads.
- **Model 2 runs faster on the Mac and on x86 Linux**: the fast drawing paths
  the iPad, Apple TV and the Pi already used are on everywhere, with the same
  pictures and sound.
- **Linux:** a Settings screen.
- **Sturdier underneath:** archive readers refuse sizes a damaged or hostile
  file could use to exhaust memory; Lindbergh games can't reach files outside
  their game and save folders; quitting a game frees its core at once.
- **Build:** the core interface header and the archive readers come from the
  shared Arcade-Foundry-Cores-Common repo (`cores/arcade-foundry-cores-common`),
  which every core repo uses too. Clone with `--recursive`, or run
  `git submodule update --init --recursive`. The cores' build tools take their
  Python from one shared environment per machine, `~/venvs/afic` (see
  [BUILD.md](BUILD.md)), so nothing is installed into the system Python.

### What came with 1.2.0

- **Let's Go Jungle! Lost on the Island of Spice and Sega Race TV** join The
  House of the Dead 4 and OutRun 2 SP SDX (Lindbergh), on macOS, Linux and
  iPad. Let's Go Jungle's crosshair is off by default.
- **Lindbergh games draw with Metal on the Mac and iPad**, through Cores-Common's
  mtlgles layer, instead of Apple's deprecated OpenGL. The old renderer stays
  in the code for this release as a fallback.
- **Region** for each Lindbergh game (Japan or Export) in its settings; the
  game and its test menu follow it.
- **Game art** (issue #352): each game's card and the top of its page show its
  title screen, downloaded on your device from the Arcade Database
  (libretro-thumbnails when it has none), and the page shows the flyer,
  marquee, cabinet and in-game screen under About this game, each opening full
  size. Your own picture (the pause menu's "Use this screen as the picture", or
  an image you chose) always comes first; the screen a game takes of itself
  after half a minute is the last choice. Settings > Game art turns the
  downloads off and removes the downloaded art. For the Lindbergh games, which
  MAME can't run yet, no title or in-game screen is downloaded (the available
  ones are MAME's "Screenless System" card or noise); their cards show the
  flyer (issue #377) unless you saved your own picture, and the screen the game
  takes of itself only when there is no flyer. That screen is taken after two
  and a half minutes on Lindbergh (half a minute elsewhere), past the loading
  screens, and a game's settings can remove it (issue #367). Only the set name
  is sent. The Linux app needs libcurl at build time for it.
- **For developers:** the cores' debug environment variables now all start
  with `AF_DEBUG_` (the old names still work in this release); the list is in
  Cores-Common's docs/DEBUG-SWITCHES.md. The SDL3 app (the Linux front end)
  also builds on a Mac, for testing.

### Known issues

- Race TV's first start is slow on the iPad (several minutes on an M1 iPad).
- The very first start of a Lindbergh game on Metal builds its shaders and
  runs slowly, without sound, until they are built.

### Not in this release

- The Sega Chihiro games (Virtua Cop 3, The House of the Dead III, Crazy Taxi
  3: High Roller) are in the code but left out of the library until their
  last problems are fixed; they are planned for 1.3.0.
- Lindbergh games aren't on Apple TV yet, and on iPhone and iPad they still
  need a paid Apple Developer account.

### Included cores (all at v1.2.0)

[Classic 2D](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Classic2D/releases/tag/v1.2.0), [Model 1 & 2](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Model12/releases/tag/v1.2.0), [Model 3](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Model3/releases/tag/v1.2.0) and [Lindbergh](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Lindbergh/releases/tag/v1.2.0), with the shared [Cores-Common](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Common/releases/tag/v1.2.0). They come in as submodules, so clone with `git clone --recursive`.

## 1.0.1 (2026-11-02)

Fixes only; the game cores are unchanged and stay at their 1.0.0 tags.

- The app's About said "1.0 (1)" on every Apple platform (the generated Info.plist did not take the project's version) and the Linux app said 0.2.0. Both now say 1.0.1, and `scripts/check_release.py` refuses a release whose version numbers disagree.
- App Transport Security now allows plain http only on the local network (`NSAllowsLocalNetworking`), which is all a ROM server on your LAN needs; every other connection keeps TLS.
- README: the "Using the app" section rewritten.
- iPad: the on-screen controller shown when no controller is connected now has a B button on every layout (view 2 in play), so operator mode has its SERVICE button and the test menus can be used.
- iPad: on a game's page, Download no longer opens the Add ROM file picker as well (a tap on the row pressed both buttons).

## 1.0.0 (2026-11-02)

**Arcade Foundry: Insert Coin 1.0.0**

Arcade Foundry: Insert Coin 1.0.0 is the first public release. It's a front end for classic Sega arcade games, built from your own ROM sets: you pick a game from the library and it runs as a native app on macOS, iOS, iPadOS, tvOS and Linux.

### What's in 1.0.0

- **Library & front end.** One card per game with the game's history, picture, filters for platform and playable games, and a list view. Controllers, keyboards, mice and touch all work, and buttons can be reassigned per game.
- **Games.** Out Run and X Board (4 games), Model 1 (4), Model 2 (32 sets), Model 3 (24 sets), and Lindbergh (The House of the Dead 4 & OutRun 2 SP SDX). The full list, with set names and ROM notes, is in [GAMES.md](GAMES.md).
- **Platforms.** macOS (Apple silicon), iPhone & iPad, Apple TV, and Linux (x86-64 desktop, Steam Deck, Raspberry Pi 4 & 5). Which games run on which platform is in [GAMES.md](GAMES.md).
- **Cabinet versus cabinet (VS).** Games with linked cabinets can be played across devices. See [LINK_PLAY.md](LINK_PLAY.md) for which games link and how to set it up.
- **Screen options.** Original shape, stretch, or whole-number scale, with none, scanlines or CRT effects, per game or for all.
- **Saves & cabinets.** Each device is a cabinet with its own saves, backed up to your iCloud and restored if a device loses them.
- **Gameplay videos (Apple platforms).** The Mac can record attract mode or your replays straight from the game, with no window and no dropped frames.

### What's not in it yet

- Let's Go Jungle! and Sega Race TV (Lindbergh) are in the code but left out of this release's library until they've been tested more.
- Lindbergh games on iPhone & iPad need a paid Apple Developer account, and Apple TV doesn't list them.
- See [ROADMAP.md](ROADMAP.md) for what I hope to add. It's in no order and has no dates.

### Getting started

No ROMs come with the app. You supply your own MAME ROM sets (Lindbergh games use the game's disk). Start with the [README](README.md), then [BUILD.md](BUILD.md) to build the cores & app, [FAQ.md](FAQ.md) for where files go, and [STEAM_DECK.md](STEAM_DECK.md) for the Steam Deck. Apple TV needs a ROM server on your network, as it has no iCloud Drive.

### About version numbers

The app and each game core are versioned separately. This release's number (1.0.0) is the app's version, and the matching tag on the cores repos is a snapshot label meaning "the cores as shipped with Insert Coin 1.0.0". Each game core has its own x.y.z version (shown in the app), which may be higher or lower than the release number. A core's major number only changes when its saves stop carrying over.

### Included cores (all at v1.0.0)

[Classic 2D](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Classic2D/releases/tag/v1.0.0), [Model 1 & 2](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Model12/releases/tag/v1.0.0), [Model 3](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Model3/releases/tag/v1.0.0) and [Lindbergh](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Lindbergh/releases/tag/v1.0.0). They come in as submodules, so clone with `git clone --recursive`.

### Licence

BSD-3-Clause. The Model 3 cores are GPL-3.0-or-later, as Supermodel is. See the README.

Arcade Foundry: Insert Coin is not affiliated with or endorsed by Sega. This is source only: there are no builds to download and no game data.
