# Games

Every game Arcade Foundry: Insert Coin supports, grouped by arcade board.
The set name is MAME's, and it is the file name the app looks for:
`<set>.zip` or `<set>.7z`. A Lindbergh game is a folder or `<set>.tar`
instead (see [Sega Lindbergh](#sega-lindbergh)). No ROM data comes with the app. See
the [FAQ](FAQ.md) for how to get your ROM sets onto your devices.

Deployment targets:

- **Yes**: the core builds for that platform, and the app lists the game.
- **Paid developer account**: it builds, but installing it needs a paid
  Apple Developer Program membership; a free personal team can't (see
  [Sega Lindbergh](#sega-lindbergh)).
- **No**: not available on that platform.
- **Not in this release**: the core is there, but the app leaves the game
  out of its library until it has been tested more (see `ThisRelease` in
  `apple/InsertCoin/Library/Catalog.swift` and `kNotThisRelease` in
  `linux/main.cpp`).

macOS needs Apple silicon. iOS covers iPhone and iPad.

## Sega Out Run board

Recompiled from your ROM set ([Arcade-Foundry-Cores-Classic2D](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Classic2D)).
The Classic 2D platform in the library.

| Game | Year | Set | ROM notes | macOS | iOS | tvOS | Linux |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Out Run | 1986 | `outrun` | Revision B (sitdown/upright). The shift buttons move the gear lever (low and high) | Yes | Yes | Yes | Yes |
| Super Hang-On | 1987 | `shangon` | Sit-down/upright (unprotected). The right shoulder is the turbo | Yes | Yes | Yes | Yes |

## Sega X Board

Recompiled from your ROM set ([Arcade-Foundry-Cores-Classic2D](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Classic2D)).
The Classic 2D platform in the library.

| Game | Year | Set | ROM notes | macOS | iOS | tvOS | Linux |
| --- | --- | --- | --- | --- | --- | --- | --- |
| After Burner II | 1987 | `aburner2` | The left stick flies, the right trigger is the throttle; A (or the right shoulder) fires the vulcan, B the missiles | Yes | Yes | Yes | Yes |
| Thunder Blade | 1987 | `thndrbld` | Upright (FD1094 317-0056; the key comes in MAME's set). The left stick flies, the right trigger is the throttle; A (or the right shoulder) fires the guns, B the missiles | Yes | Yes | Yes | Yes |

## Sega Model 1

Recompiled from your ROM set ([Arcade-Foundry-Cores-Model12](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Model12)).

| Game | Year | Set | ROM notes | macOS | iOS | tvOS | Linux |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Virtua Racing | 1992 | `vr` | Link play | Yes | Yes | Yes | Yes |
| Star Wars Arcade | 1993 | `swa` | | Yes | Yes | Yes | Yes |
| Virtua Fighter | 1993 | `vf` | | Yes | Yes | Yes | Yes |
| Wing War | 1994 | `wingwar` | MAME's I/O board set `model1io2.zip` (or `.7z`) must sit beside it | Yes | Yes | Yes | Yes |

## Sega Model 2

Recompiled from your ROM set ([Arcade-Foundry-Cores-Model12](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Model12)).

### Model 2

| Game | Year | Set | ROM notes | macOS | iOS | tvOS | Linux |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Daytona USA | 1993 | `daytona93` | | Yes | Yes | Yes | Yes |
| Daytona USA (Revision A) | 1994 | `daytona` | Link play | Yes | Yes | Yes | Yes |
| Virtua Cop | 1994 | `vcop` | Revision B | Yes | Yes | Yes | Yes |
| Virtua Fighter 2 | 1994 | `vf2` | Version 2.1 | Yes | Yes | Yes | Yes |
| Desert Tank | 1994 | `desert` | | Yes | Yes | Yes | Yes |

### Model 2A

| Game | Year | Set | ROM notes | macOS | iOS | tvOS | Linux |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Sega Rally Championship | 1995 | `srallyc` | Revision C, link play | Yes | Yes | Yes | Yes |
| Manx TT Superbike | 1995 | `manxttc` | Revision C (Twin) | Yes | Yes | Yes | Yes |
| Virtua Cop 2 | 1995 | `vcop2` | | Yes | Yes | Yes | Yes |
| Sky Target | 1995 | `skytargt` | | Yes | Yes | Yes | Yes |
| Motor Raid | 1997 | `motoraid` | Twin | Yes | Yes | Yes | Yes |
| Dynamite Cop | 1998 | `dynamcop` | Export | Yes | Yes | Yes | Yes |

### Model 2B

| Game | Year | Set | ROM notes | macOS | iOS | tvOS | Linux |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Virtua Striker | 1994 | `vstriker` | Revision A | Yes | Yes | Yes | Yes |
| Rail Chase 2 | 1994 | `rchase2` | Revision A | Yes | Yes | Yes | Yes |
| INDY 500 | 1995 | `indy500` | Twin, Revision A | Yes | Yes | Yes | Yes |
| Fighting Vipers | 1995 | `fvipers` | Revision D | Yes | Yes | Yes | Yes |
| Gunblade NY | 1995 | `gunblade` | Revision A | Yes | Yes | Yes | Yes |
| Cyber Troopers Virtual-On | 1995 | `von` | Twin, Export | Yes | Yes | Yes | Yes |
| Last Bronx | 1996 | `lastbrnx` | Export, Revision A | Yes | Yes | Yes | Yes |
| Sonic Championship | 1996 | `schamp` | USA | Yes | Yes | Yes | Yes |
| Super GT 24h | 1996 | `sgt24h` | Jaleco | Yes | Yes | Yes | Yes |
| Dead or Alive | 1996 | `doab` | Tecmo, Revision B. Older romsets name it `doa.zip`; their earlier dump of mpr-19324 is accepted | Yes | Yes | Yes | Yes |
| Dynamite Baseball 97 | 1997 | `dynabb97` | Revision A | Yes | Yes | Yes | Yes |
| Over Rev | 1997 | `overrevb` | Jaleco. Use the whole (non-merged) set, such as MAME's `overrevb.7z`: the split `.zip` lacks its parent's files | Yes | Yes | Yes | Yes |
| Zero Gunner | 1997 | `zerogun` | Export | Yes | Yes | Yes | Yes |
| Pilot Kids | 1998 | `pltkids` | Revision A | Yes | Yes | Yes | Yes |

### Model 2C

| Game | Year | Set | ROM notes | macOS | iOS | tvOS | Linux |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Sega Touring Car Championship | 1996 | `stcc` | | Yes | Yes | Yes | Yes |
| Wave Runner | 1996 | `waverunr` | Japan, Revision A | Yes | Yes | Yes | Yes |
| Sega Ski Super G | 1996 | `skisuprg` | | Yes | Yes | Yes | Yes |
| The House of the Dead | 1997 | `hotdo` | Original set. MAME's `hotd.7z` holding it also works | Yes | Yes | Yes | Yes |
| Sega Water Ski | 1997 | `segawski` | Japan, Revision A | Yes | Yes | Yes | Yes |
| Behind Enemy Lines | 1997 | `bel` | | Yes | Yes | Yes | Yes |
| Top Skater | 1997 | `topskatr` | Export, Revision A. Its 3D draws whole here, though not in MAME | Yes | Yes | Yes | Yes |

## Sega Model 3

Supermodel's machine as a core ([Arcade-Foundry-Cores-Model3](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Model3), GPL-3.0-or-later). These cores are not recompiled from your ROMs, and they draw with Metal on Apple platforms.

| Game | Year | Set | ROM notes | macOS | iOS | tvOS | Linux |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Virtua Fighter 3 | 1996 | `vf3` | | Yes | Yes | Yes | Yes |
| Scud Race | 1996 | `scud` | | Yes | Yes | Yes | Yes |
| Scud Race Plus | 1997 | `scudplus` | | Yes | Yes | Yes | Yes |
| Le Mans 24 | 1997 | `lemans24` | | Yes | Yes | Yes | Yes |
| Harley-Davidson & L.A. Riders | 1997 | `harley` | | Yes | Yes | Yes | Yes |
| The Lost World | 1997 | `lostwsga` | | Yes | Yes | Yes | Yes |
| Daytona USA 2: Battle on the Edge | 1998 | `daytona2` | | Yes | Yes | Yes | Yes |
| Daytona USA 2 Power Edition | 1998 | `dayto2pe` | | Yes | Yes | Yes | Yes |
| Sega Rally 2 | 1998 | `srally2` | | Yes | Yes | Yes | Yes |
| Dirt Devils | 1998 | `dirtdvls` | | Yes | Yes | Yes | Yes |
| Fighting Vipers 2 | 1998 | `fvipers2` | | Yes | Yes | Yes | Yes |
| Cyber Troopers Virtual-On Oratorio Tangram | 1998 | `von2` | | Yes | Yes | Yes | Yes |
| Spikeout | 1998 | `spikeout` | | Yes | Yes | Yes | Yes |
| Star Wars Trilogy Arcade | 1998 | `swtrilgy` | | Yes | Yes | Yes | Yes |
| L.A. Machineguns | 1998 | `lamachin` | | Yes | Yes | Yes | Yes |
| The Ocean Hunter | 1998 | `oceanhuna` | | Yes | Yes | Yes | Yes |
| Magical Truck Adventure | 1998 | `magtruck` | Japan | Yes | Yes | Yes | Yes |
| Sega Bass Fishing | 1998 | `bassdx` | Deluxe, USA | Yes | Yes | Yes | Yes |
| Get Bass | 1998 | `getbass` | Japan, Standard | Yes | Yes | Yes | Yes |
| Get Bass | 1998 | `getbassur` | Japan, Upright | Yes | Yes | Yes | Yes |
| Get Bass | 1998 | `getbassdx` | Japan, Deluxe; needs `bassdx.zip` beside it | Yes | Yes | Yes | Yes |
| Emergency Call Ambulance | 1999 | `ecau` | | Yes | Yes | Yes | Yes |
| Spikeout Final Edition | 1999 | `spikeofe` | | Yes | Yes | Yes | Yes |
| Virtua Striker 2 '99 | 1999 | `vs299a` | | Yes | Yes | Yes | Yes |

## Sega Lindbergh

Recompiled from the game's disk ([Arcade-Foundry-Cores-Lindbergh](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Lindbergh)).

| Game | Year | Set | ROM notes | macOS | iOS | tvOS | Linux |
| --- | --- | --- | --- | --- | --- | --- | --- |
| The House of the Dead 4 | 2005 | `hotd4a` | Revision A. The game folder that holds `disk0` (the older name `hotd4` also works) | Yes | Paid developer account | No | Yes |
| Let's Go Jungle! Lost on the Island of Spice | 2006 | `letsgoju` | The game folder that holds `disk0` (the older name `lgj` also works) | Not in this release | Not in this release | No | Not in this release |
| OutRun 2 SP SDX | 2006 | `outr2sdx` | The game folder that holds `disk0` (the older name `or2sp` also works) | Yes | Not in this release | No | Yes |
| Sega Race TV | 2008 | `segartv` | The game folder that holds `disk0` | Not in this release | Not in this release | No | Not in this release |

A Lindbergh game is the game's disk, not a ROM set. Put the folder that
holds `disk0` in the ROMs folder, named after its set
(`<roms>/hotd4a/disk0/...`). To keep it in iCloud Drive or on a ROM server,
it travels as one file, `<set>.tar` (see [BUILD.md](BUILD.md#lindbergh-games)).

On iPhone and iPad a Lindbergh core needs the extended virtual addressing
entitlement (the game's 4 GB address window). Apple grants it only to paid
developer teams, so a free (personal) team can't install the app with
these games. tvOS doesn't give apps that entitlement, so Apple TV doesn't
list them.

## Controls

A game controller works the same in every game: Back/Options inserts a
coin, Start presses start, and both together open the pause menu. A
game's page can change which button does what in that game (Controller).
Games marked "two players" take a second controller as player 2.

Driving games (Daytona USA, Sega Rally, INDY 500 and the others): the left
stick steers, R2/L2 accelerate and brake, R1/L1 shift, and A/B/X/Y are the
view buttons. On a keyboard: the arrows steer, accelerate and brake, A/Z
shift up and down, 1-4 are the views, 5 inserts a coin, Return is start,
F2 is test and F3 is service.

Gun games (Virtua Cop, The House of the Dead, The Lost World and the
others): aim with the mouse, a touch, or a controller's sticks. Gunblade NY,
Behind Enemy Lines and Rail Chase 2 are turrets that draw their own sight.

Some games need more:

- **Wing War**: flies with the left stick, throttle on RT. A or RS fires
  the machine gun, B the missile, X the smoke, and Y changes the view.
- **Sky Target**, **Star Wars Arcade**: the left stick flies.
- **Cyber Troopers Virtual-On**, **Oratorio Tangram**: both sticks of the
  controller; the triggers are the shots and the shoulders the turbos.
  Oratorio Tangram takes two coins per credit.
- **Dynamite Baseball 97**: X and A are the buttons, and B swings the bat.
- **Sega Ski Super G**, **Top Skater**: the left stick's left and right
  inclines the skis or curves the board, its forward and back swings or
  slides. Top Skater: A and B jump (front and tail), the shoulders select.
- **Virtua Fighter 3**, **Fighting Vipers 2**: X punch, A kick, B guard,
  Y escape (keyboard A, S, D, F). Virtua Fighter 3 takes two coins per
  credit.
- **Scud Race**, **Daytona USA 2**: play like Daytona USA; the four view
  buttons are their VR buttons.
- **Sega Rally 2**, **Le Mans 24**, **Dirt Devils**, **Harley-Davidson &
  L.A. Riders**, **Emergency Call Ambulance**: A (keyboard 1) is the view
  change. B (keyboard 2) is Sega Rally 2's hand brake and Harley-Davidson's
  music select. Harley-Davidson takes two coins per credit.
- **Spikeout**: X beat, A charge, B shift, Y jump.
- **Virtua Striker 2 '99**: X short pass, A long pass, B shoot.
- **Star Wars Trilogy Arcade**: flies like Star Wars Arcade. A or the right
  shoulder is the trigger, the left shoulder (keyboard V) the second
  trigger, and B and X the event buttons.
- **The Lost World**, **L.A. Machineguns**, **The Ocean Hunter**: aim like
  Virtua Cop 2. The guns have no off-screen sense, so The Lost World
  reloads with its second trigger (the right mouse button, a second
  finger, or B). The Ocean Hunter takes two coins per credit.
- **Magical Truck Adventure**: pump the lever by moving the stick up and
  down; X is the pedal.
- **Sega Race TV**, **OutRun 2 SP SDX**: wheel and pedals.
- **Let's Go Jungle!**: hold to fire.

Test mode without F2/F3 (iPad, Apple TV, a controller): Operator mode in
the pause menu makes the controller's A the cabinet's Test button and B its
Service button (on iPad there are also two buttons on screen). That is
where a cabinet's link role is set. Opening the pause menu again leaves
Operator mode.
