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
  out of its library until it has been tested more (`hidden` in
  `games.toml`).

macOS needs Apple silicon. iOS covers iPhone and iPad.

The tables are written from `games.toml` (this repo's and the cores') by
`scripts/gen_games.py`: change a game there, not here.

## Sega Hang-On board

Recompiled from your ROM set ([Arcade-Foundry-Cores-Classic2D](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Classic2D)).
The Classic 2D platform in the library.

<!-- GAMES:BEGIN hangon (scripts/gen_games.py writes this table from games.toml) -->
| Game | Year | Set | ROM notes | macOS | iOS | tvOS | Linux |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Hang-On | 1985 | `hangon` | Upright/ride-on. Lean with the left stick; the right trigger is the throttle, the left the brake | Yes | Yes | Yes | Yes |
| Space Harrier | 1985 | `sharrier` | Rev A, upright/sit-down. 8751 315-5163A: its program comes in MAME's set. The left stick flies; A or the right shoulder fires | Yes | Yes | Yes | Yes |
| Enduro Racer | 1986 | `enduror` | Rev A, YM2151. FD1089B 317-0013A: the key comes in MAME's set. Steer with the left stick and pull it back for a wheelie; the right trigger is the throttle, the left the brake | Yes | Yes | Yes | Yes |
<!-- GAMES:END hangon -->

## Sega Out Run board

Recompiled from your ROM set ([Arcade-Foundry-Cores-Classic2D](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Classic2D)).
The Classic 2D platform in the library.

<!-- GAMES:BEGIN outrun (scripts/gen_games.py writes this table from games.toml) -->
| Game | Year | Set | ROM notes | macOS | iOS | tvOS | Linux |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Out Run | 1986 | `outrun` | Revision B. The shoulders move the gear lever: low and high | Yes | Yes | Yes | Yes |
| Super Hang-On | 1987 | `shangon` | Sit-down/upright. Unprotected. The right shoulder or A is the turbo | Yes | Yes | Yes | Yes |
| Turbo Out Run | 1989 | `toutrun` | Out Run upgrade. FD1094 317-0118: the key comes in MAME's set. The shoulders move the gear lever: low and high; A is the turbo | Yes | Yes | Yes | Yes |
<!-- GAMES:END outrun -->

## Sega X Board

Recompiled from your ROM set ([Arcade-Foundry-Cores-Classic2D](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Classic2D)).
The Classic 2D platform in the library.

<!-- GAMES:BEGIN xboard (scripts/gen_games.py writes this table from games.toml) -->
| Game | Year | Set | ROM notes | macOS | iOS | tvOS | Linux |
| --- | --- | --- | --- | --- | --- | --- | --- |
| After Burner | 1987 | `aburner131` | VER 1.31, deluxe/upright. No throttle: After Burner II added it. The left stick flies; A or the right shoulder fires the vulcan, B the missiles | Yes | Yes | Yes | Yes |
| After Burner II | 1987 | `aburner2` | The left stick flies, the right trigger is the throttle; A or the right shoulder fires the vulcan, B the missiles | Yes | Yes | Yes | Yes |
| Thunder Blade | 1987 | `thndrbld` | Upright. FD1094 317-0056: the key comes in MAME's set. The left stick flies, the right trigger is the throttle; A or the right shoulder fires the guns, B the missiles | Yes | Yes | Yes | Yes |
| Line of Fire | 1989 | `loffire` | World. FD1094 317-0136: the key comes in MAME's set. Two gun turrets, X fires a missile. On its first start the game asks to calibrate the guns: fire, then aim at the top left corner and fire, then the bottom right and fire, for each gun | Yes | Yes | Yes | Yes |
| Racing Hero | 1989 | `rachero` | FD1094 317-0144: the key comes in MAME's set. Lean with the left stick; the right trigger is the throttle, the left the brake | Yes | Yes | Yes | Yes |
| Super Monaco GP | 1989 | `smgp` | World, upright. FD1094 317-0126a: the key comes in MAME's set. Steer with the left stick; the right trigger accelerates, the left brakes, the shoulders shift up and down | Yes | Yes | Yes | Yes |
| GP Rider | 1990 | `gprider` | World. FD1094 317-0163: the key comes in MAME's set. One cabinet of the twin set (MAME's `gpriders`, the same files). Lean with the left stick; the right trigger is the throttle, the left the brake, the shoulders shift up and down | Yes | Yes | Yes | Yes |
<!-- GAMES:END xboard -->

## Sega Y Board

Recompiled from your ROM set ([Arcade-Foundry-Cores-Classic2D](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Classic2D)): the three 68000 programs.
The Classic 2D platform in the library.

<!-- GAMES:BEGIN yboard (scripts/gen_games.py writes this table from games.toml) -->
| Game | Year | Set | ROM notes | macOS | iOS | tvOS | Linux |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Galaxy Force 2 | 1988 | `gforce2` | World, upright. The left stick flies, the right trigger is the throttle; A or the right shoulder fires, B the missiles | Yes | Yes | Yes | Yes |
| Power Drift | 1988 | `pdrift` | World, Rev A. Steer with the left stick; the right trigger accelerates, the left brakes; the shoulders move the gear lever: low and high | Yes | Yes | Yes | Yes |
| G-LOC Air Battle | 1990 | `gloc` | World. The left stick flies, the right trigger is the throttle; A or the right shoulder fires the vulcan, B the missiles, X the after burner | Yes | Yes | Yes | Yes |
| Rail Chase | 1991 | `rchase` | World. Two guns on the rail cars; the game draws each gun's sight | Yes | Yes | Yes | Yes |
| Strike Fighter | 1991 | `strkfgtr` | World. The left stick flies, the right trigger is the throttle; A or the right shoulder fires the vulcan, B the missiles, X the after burner | Yes | Yes | Yes | Yes |
<!-- GAMES:END yboard -->

## Sega System 32

Recompiled from your ROM set ([Arcade-Foundry-Cores-Classic2D](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Classic2D)): the V60 program, as on Model 1.
The Classic 2D platform in the library.

<!-- GAMES:BEGIN system32 (scripts/gen_games.py writes this table from games.toml) -->
| Game | Year | Set | ROM notes | macOS | iOS | tvOS | Linux |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Rad Mobile | 1990 | `radm` | World. A the lights, B the wiper | Yes | Yes | Yes | Yes |
| F1 Exhaust Note | 1991 | `f1en` | World, Rev A. Steer to pick automatic or manual and press the gas; with manual the shoulders shift up and down | Yes | Yes | Yes | Yes |
| Rad Rally | 1991 | `radr` | World. The shoulders move the gear lever: low and high | Yes | Yes | Yes | Yes |
| Golden Axe: The Revenge of Death Adder | 1992 | `ga2` | World, Rev B. X A B the three buttons; B casts magic | Yes | Yes | Yes | Yes |
| OutRunners | 1992 | `orunners` | World. System Multi 32: the left screen. The shoulders shift; A, B and X choose the music | Yes | Yes | Yes | Yes |
| SegaSonic the Hedgehog | 1992 | `sonic` | Japan, Rev C. The stick or d-pad rolls the trackball; X or A spin jumps | Yes | Yes | Yes | Yes |
| Jurassic Park | 1993 | `jpark` | World, Rev A. The game draws each gun's sight | Yes | Yes | Yes | Yes |
<!-- GAMES:END system32 -->

## Sega Model 1

Recompiled from your ROM set ([Arcade-Foundry-Cores-Model12](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Model12)).

<!-- GAMES:BEGIN model1 (scripts/gen_games.py writes this table from games.toml) -->
| Game | Year | Set | ROM notes | macOS | iOS | tvOS | Linux |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Virtua Racing | 1992 | `vr` | Link play | Yes | Yes | Yes | Yes |
| Star Wars Arcade | 1993 | `swa` | The left stick flies | Yes | Yes | Yes | Yes |
| Virtua Fighter | 1993 | `vf` | | Yes | Yes | Yes | Yes |
| Wing War | 1994 | `wingwar` | MAME's I/O board set `model1io2.zip` (or `.7z`) must sit beside it. The left stick flies | Yes | Yes | Yes | Yes |
<!-- GAMES:END model1 -->

## Sega Model 2

Recompiled from your ROM set ([Arcade-Foundry-Cores-Model12](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Model12)).

### Model 2

<!-- GAMES:BEGIN model2 (scripts/gen_games.py writes this table from games.toml) -->
| Game | Year | Set | ROM notes | macOS | iOS | tvOS | Linux |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Daytona USA | 1993 | `daytona93` | | Yes | Yes | Yes | Yes |
| Daytona USA (Revision A) | 1994 | `daytona` | Revision A, link play | Yes | Yes | Yes | Yes |
| Desert Tank | 1994 | `desert` | Right shoulder machine gun, left shoulder cannon, A/B/X VR1-3, Y shift | Yes | Yes | Yes | Yes |
| Virtua Cop | 1994 | `vcop` | Revision B | Yes | Yes | Yes | Yes |
<!-- GAMES:END model2 -->

### Model 2A

<!-- GAMES:BEGIN model2a (scripts/gen_games.py writes this table from games.toml) -->
| Game | Year | Set | ROM notes | macOS | iOS | tvOS | Linux |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Virtua Fighter 2 | 1994 | `vf2` | Version 2.1 | Yes | Yes | Yes | Yes |
| Manx TT Superbike | 1995 | `manxttc` | Revision C, Twin, link play | Yes | Yes | Yes | Yes |
| Sega Rally Championship | 1995 | `srallyc` | Revision C, link play | Yes | Yes | Yes | Yes |
| Sky Target | 1995 | `skytargt` | The left stick flies, A machine gun, B missile, X view change | Yes | Yes | Yes | Yes |
| Virtua Cop 2 | 1995 | `vcop2` | | Yes | Yes | Yes | Yes |
| Motor Raid | 1997 | `motoraid` | Twin. X punch, A kick | Yes | Yes | Yes | Yes |
| Dynamite Cop | 1998 | `dynamcop` | Export. X punch, A kick, B jump | Yes | Yes | Yes | Yes |
<!-- GAMES:END model2a -->

### Model 2B

<!-- GAMES:BEGIN model2b (scripts/gen_games.py writes this table from games.toml) -->
| Game | Year | Set | ROM notes | macOS | iOS | tvOS | Linux |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Rail Chase 2 | 1994 | `rchase2` | Revision A | Yes | Yes | Yes | Yes |
| Virtua Striker | 1994 | `vstriker` | Revision A. X long pass, A shoot, B short pass | Yes | Yes | Yes | Yes |
| Cyber Troopers Virtual-On | 1995 | `von` | Twin, Export | Yes | Yes | Yes | Yes |
| Fighting Vipers | 1995 | `fvipers` | Revision D | Yes | Yes | Yes | Yes |
| Gunblade NY | 1995 | `gunblade` | Revision A | Yes | Yes | Yes | Yes |
| INDY 500 | 1995 | `indy500` | Twin, Revision A, link play | Yes | Yes | Yes | Yes |
| Dead or Alive | 1996 | `doab` | Revision B. Tecmo. Older romsets name it `doa.zip`; their earlier dump of mpr-19324 is accepted. X punch, A kick, B hold | Yes | Yes | Yes | Yes |
| Last Bronx | 1996 | `lastbrnx` | Export, Revision A. X punch, A kick, B guard | Yes | Yes | Yes | Yes |
| Sonic Championship | 1996 | `schamp` | USA. X punch, A kick, B barrier | Yes | Yes | Yes | Yes |
| Super GT 24h | 1996 | `sgt24h` | Jaleco | Yes | Yes | Yes | Yes |
| Dynamite Baseball 97 | 1997 | `dynabb97` | Revision A. X and A the buttons, B swings the bat | Yes | Yes | Yes | Yes |
| Over Rev | 1997 | `overrevb` | Jaleco. Use the whole (non-merged) set, such as MAME's `overrevb.7z`: the split `.zip` lacks its parent's files | Yes | Yes | Yes | Yes |
| Zero Gunner | 1997 | `zerogun` | Export. X shot, A special | Yes | Yes | Yes | Yes |
| Pilot Kids | 1998 | `pltkids` | Revision A. X shot, A special | Yes | Yes | Yes | Yes |
<!-- GAMES:END model2b -->

### Model 2C

<!-- GAMES:BEGIN model2c (scripts/gen_games.py writes this table from games.toml) -->
| Game | Year | Set | ROM notes | macOS | iOS | tvOS | Linux |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Sega Ski Super G | 1996 | `skisuprg` | A, B and X select, the shoulders zoom, the stick swings | Yes | Yes | Yes | Yes |
| Sega Touring Car Championship | 1996 | `stcc` | | Yes | Yes | Yes | Yes |
| Wave Runner | 1996 | `waverunr` | Japan, Revision A. The right trigger is the throttle lever | Yes | Yes | Yes | Yes |
| Behind Enemy Lines | 1997 | `bel` | X the missile | Yes | Yes | Yes | Yes |
| The House of the Dead | 1997 | `hotdo` | Original set. MAME's `hotd.7z` holding it also works | Yes | Yes | Yes | Yes |
| Sega Water Ski | 1997 | `segawski` | Japan, Revision A. Start is set, X and B the pitch buttons, the d-pad selects | Yes | Yes | Yes | Yes |
| Top Skater | 1997 | `topskatr` | Export, Revision A. Its 3D draws whole here, though not in MAME. The left stick curves and slides, A and B jump, the shoulders select | Yes | Yes | Yes | Yes |
<!-- GAMES:END model2c -->

## Sega Model 3

Supermodel's machine as a core ([Arcade-Foundry-Cores-Model3](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Model3), GPL-3.0-or-later). These cores are not recompiled from your ROMs, and they draw with Metal on Apple platforms.

<!-- GAMES:BEGIN model3 (scripts/gen_games.py writes this table from games.toml) -->
| Game | Year | Set | ROM notes | macOS | iOS | tvOS | Linux |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Scud Race | 1996 | `scud` | | Yes | Yes | Yes | Yes |
| Virtua Fighter 3 | 1996 | `vf3` | | Yes | Yes | Yes | Yes |
| Harley-Davidson & L.A. Riders | 1997 | `harley` | A view change, B music select; two coins per credit | Yes | Yes | Yes | Yes |
| Le Mans 24 | 1997 | `lemans24` | | Yes | Yes | Yes | Yes |
| The Lost World | 1997 | `lostwsga` | Right button reloads | Yes | Yes | Yes | Yes |
| Scud Race Plus | 1997 | `scudplus` | Scud Race Plus | Yes | Yes | Yes | Yes |
| Cyber Troopers Virtual-On Oratorio Tangram | 1998 | `von2` | Two coins per credit | Yes | Yes | Yes | Yes |
| Daytona USA 2 Power Edition | 1998 | `dayto2pe` | Power Edition | Yes | Yes | Yes | Yes |
| Daytona USA 2: Battle on the Edge | 1998 | `daytona2` | Battle on the Edge | Yes | Yes | Yes | Yes |
| Dirt Devils | 1998 | `dirtdvls` | A view change | Yes | Yes | Yes | Yes |
| Fighting Vipers 2 | 1998 | `fvipers2` | X punch, A kick, B guard | Yes | Yes | Yes | Yes |
| Get Bass | 1998 | `getbass` | Japan, Standard. The left stick is the rod, the right trigger reels, X casts | Yes | Yes | Yes | Yes |
| Get Bass | 1998 | `getbassur` | Japan, Upright. The left stick is the rod, the right trigger reels, X casts | Yes | Yes | Yes | Yes |
| Get Bass | 1998 | `getbassdx` | Japan, Deluxe. Needs `bassdx.zip` beside it. Needs bassdx.zip beside it; the left stick is the rod, the right trigger reels, X casts; two coins per credit | Yes | Yes | Yes | Yes |
| L.A. Machineguns | 1998 | `lamachin` | | Yes | Yes | Yes | Yes |
| Magical Truck Adventure | 1998 | `magtruck` | Japan. Pump the lever: the stick up and down; X the pedal | Yes | Yes | Yes | Yes |
| The Ocean Hunter | 1998 | `oceanhuna` | Two coins per credit | Yes | Yes | Yes | Yes |
| Sega Bass Fishing | 1998 | `bassdx` | Deluxe, USA. The left stick is the rod, the right trigger reels, the d-pad the lure stick, X casts, A selects | Yes | Yes | Yes | Yes |
| Sega Rally 2 | 1998 | `srally2` | A view change, B hand brake | Yes | Yes | Yes | Yes |
| Spikeout | 1998 | `spikeout` | X beat, A charge, B shift, Y jump | Yes | Yes | Yes | Yes |
| Star Wars Trilogy Arcade | 1998 | `swtrilgy` | The left stick flies, left shoulder second trigger, B/X event buttons | Yes | Yes | Yes | Yes |
| Emergency Call Ambulance | 1999 | `ecau` | A view change | Yes | Yes | Yes | Yes |
| Spikeout Final Edition | 1999 | `spikeofe` | Final Edition. X beat, A charge, B shift, Y jump | Yes | Yes | Yes | Yes |
| Virtua Striker 2 '99 | 1999 | `vs299a` | X short pass, A long pass, B shoot | Yes | Yes | Yes | Yes |
<!-- GAMES:END model3 -->

## Sega Lindbergh

Recompiled from the game's disk ([Arcade-Foundry-Cores-Lindbergh](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Lindbergh)).

<!-- GAMES:BEGIN lindbergh (scripts/gen_games.py writes this table from games.toml) -->
| Game | Year | Set | ROM notes | macOS | iOS | tvOS | Linux |
| --- | --- | --- | --- | --- | --- | --- | --- |
| The House of the Dead 4 | 2005 | `hotd4a` | Revision A. The game folder that holds `disk0` (the older name `hotd4` also works). Middle-click, G, X or three fingers throw a grenade | Yes | Paid developer account | No | Yes |
| Let's Go Jungle! Lost on the Island of Spice | 2006 | `letsgoju` | Lost on the Island of Spice. The game folder that holds `disk0` (the older name `lgj` also works). Hold to fire | Yes | Paid developer account | No | Yes |
| OutRun 2 SP SDX | 2006 | `outr2sdx` | The game folder that holds `disk0` (the older name `or2sp` also works). Wheel and pedals | Yes | Paid developer account | No | Yes |
| Sega Race TV | 2008 | `segartv` | The game folder that holds `disk0`. Wheel and pedals | Yes | Paid developer account | No | Yes |
<!-- GAMES:END lindbergh -->

A Lindbergh game is the game's disk, not a ROM set. Put the folder that
holds `disk0` in the ROMs folder, named after its set
(`<roms>/hotd4a/disk0/...`). To keep it in iCloud Drive or on a ROM server,
it travels as one file, `<set>.tar` (see [BUILD.md](BUILD.md#lindbergh-games)).

On iPhone and iPad a Lindbergh core needs the extended virtual addressing
entitlement (the game's 4 GB address window). Apple grants it only to paid
developer teams, so a free (personal) team can't install the app with
these games. tvOS doesn't give apps that entitlement, so Apple TV doesn't
list them.

## Sega Chihiro

Recompiled from the game's Xbox program, the XBE ([Arcade-Foundry-Cores-Chihiro](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Cores-Chihiro)).

<!-- GAMES:BEGIN chihiro (scripts/gen_games.py writes this table from games.toml) -->
| Game | Year | Set | ROM notes | macOS | iOS | tvOS | Linux |
| --- | --- | --- | --- | --- | --- | --- | --- |
| The House of the Dead III | 2002 | `hotd3` | The game folder that holds `hod3xb.xbe` | Not in this release | Not in this release | No | Not in this release |
| Crazy Taxi High Roller | 2003 | `crtaxihr` | The game folder that holds `ctx_ac[r].xbe`. Steer with the left stick; the right trigger accelerates, the left brakes; the right shoulder is Drive, the left Reverse | Not in this release | Not in this release | No | Not in this release |
| Virtua Cop 3 | 2003 | `vcop3` | The game folder that holds `vc3.xbe`. Middle-click, G, X or three fingers changes weapon | Not in this release | Not in this release | No | Not in this release |
<!-- GAMES:END chihiro -->

A Chihiro game is the game's files, not a ROM set. Put the folder that
holds its XBE in the ROMs folder, named after its set
(`<roms>/vcop3/vc3.xbe`, `<roms>/hotd3/hod3xb.xbe`). The baseboard's
EEPROMs come from MAME's `chihiro` BIOS set: leave `chihiro.zip` zipped in
the ROMs folder (or beside the games in `Chihiro/`; Add ROM on Apple
devices). Without it the game still plays: the first start uses its
factory settings and the board ID is blank. Settings changed in the test
menu are kept from then on, with or without the zip. Like a
Lindbergh game, a Chihiro game needs a 4 GB address window, so Apple TV
doesn't list these games.

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
others): aim with the mouse, a touch, or a controller's sticks (on an
iPhone or iPad, the game's settings can aim with the on-screen controller
instead of touch). Gunblade NY,
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
