# Design documents

High-level designs (HLDs) for the app's larger parts: what each part is for,
how it is built and why, the data it keeps, how it is tested and what is
still open. They are written for whoever works on the code next, so they
describe the design as it is and are updated in the same pull request as a
change that moves it. The dated history of the work is in HANDOFF.md, and
the current state of the whole app is in STATE.md.

These documents live in the app repository because the parts they describe
live here. A design that belongs to every core (the `afcore` interface, the
archive readers, x86recomp) goes in Arcade-Foundry-Cores-Common's
`docs/design/` instead. They are public: they go out with each release and
pass its scan, so they name no people, machines or private paths and hold
nothing taken from ROMs.

| Document | Covers | State |
|---|---|---|
| [WHEEL-AND-PEDALS.md](WHEEL-AND-PEDALS.md) | Steering wheels, pedals and shifters (issue #318) | Linux done; self-centring, dead zone, G29, Apple platforms open |
| [APP-ARCHITECTURE.md](APP-ARCHITECTURE.md) | The app as a whole: the game list, building and loading cores, sessions, saves and cabinets, link play, settings, the Apple and Linux front ends, releases | Current as of 1.3.0 development |
| [Game Director](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Game-Director/blob/main/docs/design/GAME-DIRECTOR.md) | Moved to Arcade-Foundry-Game-Director-devel (7 Nov 2026): the multiplayer hub, now Game Director, and its player, Game Cabinet. Part 2 is [GAME-DIRECTOR-PART2.md](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Game-Director/blob/main/docs/design/GAME-DIRECTOR-PART2.md) there | Its own repo |
| ROM server and ROM sync | The ROM server URL, iCloud Drive, downloads and checks | To write |
| Light guns | Mouse, touch, gamepad aim and real light guns | To write |
| [SAVE-STATES.md](SAVE-STATES.md) | Save states and resume (1.4.0): the pause menu's slots, the Resume slot, which boards can save (not the x86 ones, and why), afcore 5.4.0, storage, link play and replays, testing | Plan, not built |
| [CONTROL-SCHEME.md](CONTROL-SCHEME.md) | How the app and games are controlled on each platform: the default controller layout, each kind of game, keyboard and mouse, touch and the on-screen controller on iPhone and iPad, Apple TV, button maps, two players, operator mode; touch settings (#359) | Current; open items listed |
| [OTHER-CONTROLLERS.md](OTHER-CONTROLLERS.md) | Controllers beyond a gamepad, keyboard, mouse and touch, and how each maps to the games: twin sticks for Virtual-On (the Sega Saturn twin sticks), fight sticks (8BitDo Arcade Stick over Bluetooth, 2.4 GHz wireless and wired), fishing rods, the GAMR Play Pad floor mat (#435) and other cabinet-style controllers; each to try on Linux, macOS, iPhone, iPad and Apple TV | Started: GAMR Play Pad (paper only); the rest to write |

## Core designs

Designs that belong to the cores are kept with the cores, under each core
repository's `docs/design/`:

| Repository | Document | Covers |
|---|---|---|
| [Cores-Common](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Common/tree/main/docs/design) | [MTLGLES.md](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Common/blob/main/docs/design/MTLGLES.md) | The Metal path for the GLES cores |
| Cores-Common | [X86RECOMP.md](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Common/blob/main/docs/design/X86RECOMP.md) | x86recomp, shared by Lindbergh and Chihiro |
| [Cores-Model12](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Model12/tree/main/docs/design) | [MODEL2.md](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Model12/blob/main/docs/design/MODEL2.md) | The Model 2 cores (2, 2A, 2B, 2C): the i960, TGP, SHARC and TGPx4 recompilers, the geometrizer and rasterizer, the sound boards, controls, link play, enhancements and fast paths |
| Cores-Model12 | [MODEL1.md](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Model12/blob/main/docs/design/MODEL1.md) | The Model 1 cores: the V60 recompiler, the TGP's FIFO path and timing, the video, the I/O boards, the Digital Sound Board, link play |
| [Cores-Model3](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Model3/tree/main/docs/design) | [MODEL3.md](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Model3/blob/main/docs/design/MODEL3.md) | The Model 3 cores |
| Cores-Model3 | [ARM64-PERFORMANCE.md](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Model3/blob/main/docs/design/ARM64-PERFORMANCE.md) | Model 3 speed on the Pi 5 and Apple devices: wait-loop skip, PowerPC recompiler, Metal and GLES renderers, threads |
| Cores-Lindbergh | [LINDBERGH.md](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Lindbergh/blob/main/docs/design/LINDBERGH.md) | The Lindbergh cores: the runtime, graphics per platform, frames and time, the cabinet |
| Cores-Chihiro | [CHIHIRO.md](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Chihiro/blob/main/docs/design/CHIHIRO.md) | The Chihiro core |
| [Cores-Classic2D](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Classic2D/tree/main/docs/design) | [CLASSIC2D.md](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Classic2D/blob/main/docs/design/CLASSIC2D.md) | The Classic 2D cores: boards, recompilers, MAME's scheduler, video, sound, testing against MAME |
| Cores-Classic2D | [M68000.md](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Classic2D/blob/main/docs/design/M68000.md) | The 68000 recompiler and its timing (MAME's cycle-exact 68000, Musashi on the Y Board), the fallback interpreter |
| Cores-Classic2D | [TURBO.md](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Classic2D/blob/main/docs/design/TURBO.md) | The Sega Z80-3D system (Turbo, Subroc-3D, Buck Rogers): a proposal, not built |

Links to other repositories point at the development repositories
(`…-devel`, branch `af-next`), so they work while the documents are written.
The release export rewrites every such file link to the public repository on
`main` (export_release.py, step 3), so write them that way and leave them;
an issue or pull request link into a `-devel` repository is not rewritten
and the export lists it as a warning, so cite those as plain `#123` instead.

Each document follows the same outline: Purpose and scope, Requirements,
Design, Data and settings, Platforms, Testing, Open items, Decisions.
