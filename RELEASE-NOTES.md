# Release notes

Newest first. Each release is also on the repository's Releases page.

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
