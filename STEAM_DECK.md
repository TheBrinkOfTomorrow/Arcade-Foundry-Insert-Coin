# Arcade Foundry: Insert Coin on the Steam Deck

This guide takes you from a fresh Steam Deck to playing in Game Mode with
the Deck's own controls. It assumes you already have your ROM sets (and,
for Lindbergh, your game folders), and that they are the versions listed
in [GAMES.md](GAMES.md). You'll need about an hour the first time, most of
it waiting for the build.

Arcade Foundry: Insert Coin runs on the Deck as the Linux app. There's no
download to install: the game cores are made on your Deck from your own
ROM sets, so every step below happens on the Deck itself.

Arcade Foundry: Insert Coin is not affiliated with or endorsed by Sega. It
contains no ROM data, and this guide won't tell you where to find any.

## Contents

1. [What you need](#what-you-need)
2. [Switch to Desktop Mode](#1-switch-to-desktop-mode)
3. [Make a build box](#2-make-a-build-box)
4. [Get the app](#3-get-the-app)
5. [Put your ROM sets on the Deck](#4-put-your-rom-sets-on-the-deck)
6. [Build the game cores](#5-build-the-game-cores)
7. [Build the app and try it](#6-build-the-app-and-try-it)
8. [Add it to Steam for Game Mode](#7-add-it-to-steam-for-game-mode)
9. [Controls](#controls)
10. [Adding games and updating](#adding-games-and-updating)
11. [Where things are kept](#where-things-are-kept)
12. [Problems and fixes](#problems-and-fixes)

## What you need

- A Steam Deck (LCD or OLED) on SteamOS 3.5 or later (Settings > System
  shows the version). 3.5 is the first with distrobox built in.
- An internet connection for the setup.
- Your ROM sets, named as MAME names them (see
  [Folder layout](#folder-layout)).
- A way to copy them over: a USB stick or drive (with the Deck's USB-C hub
  or an adapter), a microSD card, or a shared folder on your network.
- Some free space for the build: the tools, the app and a core per game.
  Lindbergh games also need room for their unpacked folder (see
  [Lindbergh games](#lindbergh-games)).
- A keyboard and mouse help a lot in Desktop Mode, but the touchscreen and
  the on-screen keyboard (Steam + X) work too.
- Keep the Deck plugged in while it builds.

Which games play on Linux, and so on the Deck: every classic 2D (Out Run
board, X Board), Model 1, Model 2 and Model 3 game in [GAMES.md](GAMES.md),
and from Lindbergh, **The House of
the Dead 4** (`hotd4a`) and **OutRun 2 SP SDX** (`outr2sdx`). Let's Go
Jungle! and Sega Race TV aren't in the 1.0.0 release yet.

## 1. Switch to Desktop Mode

Press the Steam button, then Power > Switch to Desktop. Everything up to
[step 7](#7-add-it-to-steam-for-game-mode) happens here.

Open **Konsole** (the terminal) from the app launcher in the lower left,
under System. You'll type (or paste) the commands below into it. In
Konsole, paste is Ctrl+Shift+V.

## 2. Make a build box

SteamOS keeps its system files read-only, and a SteamOS update would undo
any packages you added to it. So the build tools go into a **distrobox**:
a small Ubuntu inside your home folder that SteamOS updates leave alone.
It shares your home folder, your screen, your sound and your controllers
with the Deck, so the app runs from it just as well.

Make it (one time):

    distrobox create --name insertcoin --image ubuntu:24.04

Then step into it:

    distrobox enter insertcoin

The first time, this takes a few minutes to set up. Your prompt changes to
`deck@insertcoin` while you're inside. Type `exit` to leave it, and
`distrobox enter insertcoin` to go back in.

Inside the box, install the build tools (one long command):

    sudo apt update && sudo apt install -y \
        build-essential git cmake ninja-build pkg-config python3 python3-venv \
        libasound2-dev libpulse-dev libpipewire-0.3-dev \
        libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev \
        libxss-dev libxtst-dev libxkbcommon-dev libwayland-dev libdecor-0-dev \
        libdrm-dev libgbm-dev libegl-dev libgl1-mesa-dev libglu1-mesa-dev \
        libdbus-1-dev libudev-dev libsdl2-dev libsdl2-net-dev

`sudo` inside the box doesn't ask for a password, and it only changes the
box, not SteamOS.

## 3. Get the app

Still inside the box:

    cd ~
    git clone --recursive https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Insert-Coin
    cd Arcade-Foundry-Insert-Coin

`--recursive` also fetches the game cores (Model 1/2, Model 3 and
Lindbergh), which live in their own repositories.

If you'll play Lindbergh games, the Lindbergh build needs two Python
packages. Install them into the venv the build looks for:

    python3 -m venv cores/arcade-foundry-cores-lindbergh/venv
    cores/arcade-foundry-cores-lindbergh/venv/bin/pip install capstone pyelftools

## 4. Put your ROM sets on the Deck

### Where they go

I suggest one folder in your home folder, `/home/deck/ArcadeFoundry`
(it shows up as **ArcadeFoundry** under Home in Dolphin, the file
manager). The build reads your ROM sets from there, and so does the app,
so this is the only place they need to be.

You can use a microSD card instead. SteamOS mounts it at
`/run/media/deck/<card name>` (Dolphin shows the exact path in its address
bar when you open the card), so the folder would be
`/run/media/deck/<card name>/ArcadeFoundry`. Use that path wherever this
guide says `~/ArcadeFoundry`. The internal drive is faster, which matters
most for Lindbergh games.

### Folder layout

Inside it, make a folder per arcade board, and put each set straight into
its board's folder, one file per set:

    ArcadeFoundry/
      OutRun/      outrun.zip  shangon.zip
      XBoard/      aburner2.zip  thndrbld.zip
      Model1/      vr.zip  swa.zip  vf.zip  wingwar.zip  model1io2.zip
      Model2/      daytona.zip  srallyc.zip  vcop.zip  hotdo.zip ...
      Model3/      vf3.zip  scud.zip  daytona2.zip  srally2.zip ...
      Lindbergh/   hotd4a.tar  outr2sdx.tar

The rules that matter:

- **Name each set as MAME does**, and as [GAMES.md](GAMES.md) lists it:
  `srallyc.zip`, not `Sega Rally (Rev C).zip`. `.zip` and `.7z` both
  work. The app goes by the file name, so a renamed file is a missing game.
- **Board folders are flat.** No folders inside `Model2/` (except
  Lindbergh's game folders, below). The folder names above are what I
  use, but any name works, and so does putting every set straight into
  `ArcadeFoundry/` with no board folders at all.
- **Helper sets go in too.** Some games need a second set beside them:
  Wing War needs MAME's I/O board set `model1io2.zip`. Put it in any
  board folder (`Model1/` is the natural one).
- **Parents work as well.** `hotd.zip` (or `.7z`) holding the original
  House of the Dead works for `hotdo`, and `doa.zip` for `doab`. Over Rev
  needs the whole set, such as MAME's `overrevb.7z`: the split `.zip`
  lacks its parent's files.
- **Folders named `working`, `temp` or `to-file…`, or starting with `_`
  or `.`, are skipped**, so you can keep other things there without
  confusing the app.
- If a set is in two places, the app uses one and names the other in a
  warning in the terminal.

### Lindbergh games

A Lindbergh game is the game's disk, not a ROM set. The Deck takes it
either way:

- **As one file, `<set>.tar`** (easiest to copy): `Lindbergh/hotd4a.tar`
  and `Lindbergh/outr2sdx.tar`. It's a plain, uncompressed tar of the
  game folder (the one that holds `disk0`). If you made one for the Mac or
  iPad app, it's the same file. To make one on another Linux or Mac
  computer, from the folder that holds the game folder:

      tar -cf hotd4a.tar hotd4a

  The app's `scripts/pack_lindbergh.py --roms <folder>` does the same for
  every Lindbergh game in a ROM folder.
- **As the game folder**, named after the set:
  `Lindbergh/hotd4a/disk0/...`.

The first time you play a `.tar`, the app unpacks it into
`~/.local/share/arcade-foundry/games/`, which takes a while and needs room
for the whole game. It unpacks again only when the `.tar` changes. The
older names `hotd4` and `or2sp` also work.

### Copying them over

Any of these work. In Dolphin, drag the board folders into
`ArcadeFoundry`:

- **USB stick or drive, or microSD**: plug it in, and Dolphin lists it on
  the left under Devices. Copy the folders across.
- **A shared folder on your network**: in Dolphin's address bar, type
  `smb://<computer name or address>/<share>` (Windows or Mac file sharing,
  most NAS boxes), sign in, and copy.
- **From another Linux or Mac computer**, over the network: turn on SSH on
  the Deck (set a password with `passwd`, then
  `sudo systemctl enable --now sshd` in Konsole, outside the box) and
  copy with `scp` or `rsync`.

## 5. Build the game cores

Each game's core is made from your ROM set, on the Deck. Inside the box
(`distrobox enter insertcoin`), in the app folder:

    cd ~/Arcade-Foundry-Insert-Coin
    python3 scripts/build_cores.py --roms ~/ArcadeFoundry

It finds every set in your folder, recompiles the Model 1, Model 2 and
Lindbergh games and builds the Model 3 ones. This is the long step: a full
library can take a good while on the Deck, so leave it plugged in. When
it's done, each game has a core in `build/cores/linux/`.

If the build stops because it ran out of memory, run it again with fewer
jobs at once: add `--jobs 4` (or `--jobs 2`). It carries on from where it
stopped. To build only some games, name them: `--sets daytona,srallyc,vf3`.
`--status` lists which cores are built and which are missing, without
building anything.

## 6. Build the app and try it

Still inside the box, in the app folder:

    cmake -S linux -B build/linux -G Ninja
    cmake --build build/linux

Then start it:

    build/linux/insert-coin --roms ~/ArcadeFoundry

A window opens with your games. Check that a game starts, then close the
window. If a game
is missing or can't be played, its page says why (see
[Problems and fixes](#problems-and-fixes)).

## 7. Add it to Steam for Game Mode

### A launcher

Steam needs one thing to start. Make a small script that steps into the
box and starts the app full screen. In Konsole, **outside** the box (type
`exit` first if your prompt says `insertcoin`):

    cat > ~/insert-coin.sh <<'EOF'
    #!/bin/bash
    exec distrobox enter insertcoin -- \
        "$HOME/Arcade-Foundry-Insert-Coin/build/linux/insert-coin" \
        --fullscreen --roms "$HOME/ArcadeFoundry"
    EOF
    chmod +x ~/insert-coin.sh

If your ROM folder is on a microSD card, change the `--roms` path in the
script to match. You can add any of the app's other options to that line
too (see [Picture options](#picture-options)).

### Add it to your library

1. Open Steam (the desktop icon or the launcher).
2. Games > Add a Non-Steam Game to My Library.
3. Click Browse, set the file type at the bottom to **All Files**, go to
   `/home/deck`, choose `insert-coin.sh` and click Add Selected Programs.
4. In your library, right-click `insert-coin.sh` > Properties, and rename
   it **Arcade Foundry: Insert Coin**. You can also give it artwork here.

Go back to Game Mode with the **Return to Gaming Mode** icon on the
desktop. You'll find the app in your Library under Non-Steam.

### The controller layout

In Game Mode, open the app's page, press the controller icon, and choose a
template:

- **Gamepad with Mouse Trackpad**: the Deck's buttons are a gamepad, and
  the right trackpad moves the mouse (a click on the trackpad is the left
  mouse button). This is the one I'd pick: it covers every game, and the
  trackpad makes a good light gun.
- **Gamepad**: the buttons and sticks only. Gun games then aim with the
  right stick (see [Gun games](#gun-games)).

## Controls

The app sees the Deck as a standard gamepad. Its buttons map like this:

| Deck | In the app |
| --- | --- |
| A | South: choose, play; in games as listed per game |
| B | East: back |
| X | West |
| Y | North |
| View (two squares, left of the screen) | Back: **insert a coin** |
| Menu (three lines, right of the screen) | **Start** |
| View and Menu together | **Pause menu** |
| L1 / R1 (bumpers) | Shoulders |
| L2 / R2 (triggers) | Triggers, read as analog (accelerate, brake) |
| Left stick, d-pad | Move, steer |
| Right stick | Second stick; aims in gun games |

### In the game list and on a game's page

- Up/Down move through the list. A opens a game's page; B goes back.
- L1/R1 step through the platforms (All, Classic 2D, Model 1, Model 2,
  Model 3, Lindbergh), L2/R2 through the orders (A-Z, Year, Platform, Recently
  played), and Y shows all games or only the ones you can play. The app
  remembers all three.
- X shows the credits.
- On a game's page, A on a version plays it. Right on a version folds out
  that version's settings (Shape, Effect, Controller buttons, and the
  crosshair in gun games); Left or B folds them back. The page also lists
  the game's controls and its history.

### The pause menu

View and Menu together (or Esc on a keyboard) pause the game: Resume,
Operator mode, Shape, Effect, Controller buttons, Full screen, Use this
screen as the picture, and Quit to library.

- **Operator mode** is the cabinet's test switch, for the game's own test
  menu (difficulty, free play, link role). While it's on, A is the
  cabinet's Test button and B its Service button. Opening the pause menu
  again turns it off.
- **Controller buttons** changes which button does what in this game:
  choose a button, then press the one to use in its place (a trigger
  works too). Reset goes back to the default.

### Driving games

The left stick steers, R2 accelerates and L2 brakes, R1 and L1 shift up
and down, and A, B, X and Y are the view buttons. OutRun 2 SP SDX plays
the same way.

### Fighting and other games

Each game's page lists its controls, and [GAMES.md](GAMES.md#controls) has
the full list. Virtual-On uses both sticks, with the triggers as the shots
and the bumpers as the turbos.

### Gun games

Virtua Cop, The House of the Dead, The House of the Dead 4, The Lost World
and the others aim in two ways:

- **The right trackpad** (with the Gamepad with Mouse Trackpad template):
  it moves the aim like a mouse. A click on the trackpad fires. For
  reloading, set a back grip button to Right Mouse Click in the controller
  layout.
- **The right stick** (or the left): moves the aim from where it is, R2 or
  A fires, B reloads, and a red crosshair shows where you're aiming. You
  can turn the crosshair off per game, on the game's page.

In The House of the Dead 4, X throws a grenade. Gunblade NY, Behind Enemy
Lines and Rail Chase 2 draw their own sight, so they have no crosshair.

### Two players

Pair a second controller (Steam button > Settings > Bluetooth, or plug one
into USB). It's player 2. In gun games, its blue crosshair shows once
player 2 presses Start.

### Keyboard (Desktop Mode, or a keyboard on the Deck)

Arrows steer, accelerate and brake; A/Z shift; 1-4 are the views; 5
inserts a coin; Return is Start; F2 is Test and F3 Service; Esc is the
pause menu; F11 or Alt+Enter switches full screen. The mouse aims in gun
games (left button fires, right button reloads).

### Picture options

Each game's page and its pause menu set its Shape (Original, Stretch,
Whole-number scale) and Effect (None, Scanlines, CRT). The Deck's screen
is 16:10, so the Model 1 to 3 games show with bars at the sides unless you
choose Stretch. To set them for every game at once, add options to the
launcher script's `insert-coin` line: `--shape original|stretch|integer`,
`--effect none|scanlines|crt`, `--sharp` (no smoothing).

## Adding games and updating

**A new game**: copy its set into the right board folder, then in Konsole:

    distrobox enter insertcoin
    cd ~/Arcade-Foundry-Insert-Coin
    python3 scripts/build_cores.py --roms ~/ArcadeFoundry

Only the cores that are missing or out of date are built.

**A new version of the app**: in the same place,

    git pull --recurse-submodules
    git submodule update --init --recursive
    python3 scripts/build_cores.py --roms ~/ArcadeFoundry
    cmake --build build/linux

Your saves, settings and pictures are kept outside the app folder, so they
carry over. SteamOS updates leave the box, the app and your games alone.

## Where things are kept

Everything the app writes is in `~/.local/share/arcade-foundry/` (Dolphin
hides it: press Ctrl+H to show hidden folders):

| Folder | What's in it |
| --- | --- |
| `saves/<set>/` | Each game's saves: its settings, link role and records |
| `pictures/` | The library pictures |
| `buttons/<set>` | Each game's controller buttons |
| `crosshair/<set>` | A game's crosshair, when you turned it off |
| `games/` | Unpacked Lindbergh games |
| `rom-links/` | Links to your ROM sets, made by the app |

The Linux app doesn't back up your saves, so copy `saves/` somewhere safe
now and then.

## Problems and fixes

**`distrobox: command not found`.** SteamOS is older than 3.5. Update it
(Settings > System > Software updates), or install distrobox into your
home folder as its own guide describes.

**A game is missing from the list, or its page says it can't be played.**
Check, in this order:

1. The file's name is the set name in [GAMES.md](GAMES.md), with `.zip`,
   `.7z` or (Lindbergh) `.tar`.
2. It's in `ArcadeFoundry/` or directly in one of its board folders, not
   deeper, and not in a skipped folder (`working`, `temp`, `_…`).
3. Its core is built: `python3 scripts/build_cores.py --roms
   ~/ArcadeFoundry --status` inside the box. If it isn't, run the build
   again and read the last lines it prints.
4. Its helper set is there too (Wing War's `model1io2.zip`).
5. In Game Mode, the launcher points `--roms` at the same folder you built
   from.

**Let's Go Jungle! or Sega Race TV doesn't show up.** They aren't in the
1.0.0 release yet.

**The build stops with `No module named 'capstone'`.** The Lindbergh venv
is missing; make it as in [step 3](#3-get-the-app).

**The build runs out of memory or the Deck freezes while building.** Run
it again with `--jobs 2`.

**The app starts in Desktop Mode but not from Game Mode.** Start
`~/insert-coin.sh` from Konsole to see its messages. Most often the path
to the app or the ROM folder in the script is wrong.

**The picture isn't full screen.** Press F11 on a keyboard, or choose Full
screen in the pause menu. The app remembers it.

**A Lindbergh game takes a long time to start the first time.** It's
unpacking the `.tar`; later starts are quick. If the Deck runs out of
space while unpacking, free some up and start the game again.

**No sound.** Check the Deck's volume and output first, and that no other app
is holding the sound. Then start `~/insert-coin.sh` from Konsole and look
for audio messages.

For anything else, see the [FAQ](FAQ.md), or open an issue with what you
did, what happened and the last lines Konsole printed.
