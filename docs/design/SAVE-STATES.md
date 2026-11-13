# Save states and resume

Status: 1.4.0, being built. Done: afcore 5.4 and `afcommon/state.h`
(Cores-Common #43), Model 3 (Cores-Model3 #58, exact screen and sound
after a load), Model 1 and 2 (Cores-Model12 #55, exact screen and sound
after a load on all nine Model 2 sets tried; the Model 1 sets not yet run
on real ROMs), the Linux app; the Apple app written (IC-Kit #13), being
built on an M4 Pro Mac (64 GB), with the iCloud Resume record (schema deploy pending).
Next: Classic 2D.

A save state is the whole board at one frame: CPUs, memory, video, sound
and the settings EEPROM and backup RAM, written to a file and loaded back
later so the game carries on from that frame. Resume is the same thing done
for the player: the app saves the game's state when it is left, and offers
to carry on from there the next time.

## What the player sees

- **Pause menu: Save state and Load state.** Save state writes the frame
  under the menu to a slot; Load state lists the slots with a picture of
  the screen, the date and the play time, and loads one. Three slots per
  game per cabinet, plus the Resume slot.
- **Resume.** When a game is left (Quit to library, the app sent to the
  background on iPhone, iPad and Apple TV, the app quit on Mac and Linux),
  the app writes the Resume slot. The game's page and its library card get
  a **Resume** button beside Play, with the Resume slot's picture and date;
  Play starts the game from power-on as today. A setting, **Resume where I
  left off**, off by default, turns the Resume slot on.
- **Main menu.** Apple: the game's page and the library card's context
  menu. Linux and the SDL front end: the library menu gets Resume above
  Play for a game with a Resume slot, and the pause menu gets the two state
  items.
- **Games without save states** (Lindbergh, Chihiro, ALLS, see below) show
  none of these items. The game's page says "Save states: not on this
  board".

## Which boards can have them

What decides it is whether the whole running game lives in data the core
owns, or partly on the host's own call stacks and threads.

| Board | Cores | How the game runs | Save states | Work |
|---|---|---|---|---|
| Model 3 | Supermodel's `CModel3` | Interpreted PowerPC, all state in objects | **Yes.** Supermodel already saves and loads the whole board (`SaveState`/`LoadState` on every device, `CBlockFile`); the core does not use it yet | Small |
| Model 1 and 2 | Static recompilers (V60, i960, TGP, SHARC, TGPx4) | Generated code returns to `GameLoop::run_frame` at the end of each frame with the CPU in its register structs | **Yes.** Every piece of state is in structs: CPUs, RAMs, geometrizer, video, sound boards | Medium: many board variants |
| Classic 2D | Static recompilers (68000, V60, Z80) under MAME's scheduler | The same, plus the scheduler's timers | **Yes**, once the timers can be named in a file | Medium |
| Lindbergh | x86recomp | The game's own threads, each a host thread; a guest call is a host C call, so the game's call stack is the host's stack | **No.** The state at a frame includes host stacks full of return addresses into the core library, live pthread objects and the game's OpenGL objects; none of it can be written out and rebuilt in a new process | Research only |
| Chihiro | x86recomp | As Lindbergh, with Direct3D 8 state | **No**, for the same reasons | Research only |
| ALLS | recomp64 | As Lindbergh, Windows threads, Direct3D 11, Unreal Engine 4 | **No** | Research only |

On iPhone, iPad and Apple TV every core already carries on after the app
comes back from the background, as long as the system kept the app in
memory: the game pauses under the menu and Resume continues it. That is
the only resume the x86 boards get in 1.4.0.

Making the x86 boards save would mean restoring a process: the 4 GB guest
window, every guest thread's host stack at the same address with return
addresses moved by the library's new load address, the threads' locks, and
every texture, buffer and shader the game made. That is a research project
with no promise of working on iOS, not a 1.4.0 item.

## The core interface: afcore 5.4.0

Three functions appended to `afcore_api` (a minor version: front ends and
cores of other minors still work together; NULL in a core without save
states):

```c
/* 5.4; NULL in a core without save states. The whole board at this frame,
 * written into buf (state_size bytes at most), between run_frame calls,
 * on the thread that calls run_frame. Returns the bytes written, or 0
 * with the reason in err. */
size_t (*state_size)(afcore *core);
size_t (*save_state)(afcore *core, void *buf, size_t size, char *err, size_t err_size);
/* Puts the board back to a saved frame; the next run_frame carries on from
 * it. Returns 0, or -1 with the reason in err, leaving the game as it was
 * (a state from another set or core version, a damaged file). */
int (*load_state)(afcore *core, const void *buf, size_t size, char *err, size_t err_size);
```

The bytes follow one layout in every core, so the front end and the tools
can read the header without the core:

- `AFST`, a format version, the romset, `core_version`, the frame count;
- then tagged chunks (`I960`, `RAM `, `GEO `, `SCSP`, `M3  ` for
  Supermodel's own block file, ...), each with its length, so a core can
  skip a chunk it doesn't know.

Cores-Common gets a small C writer and reader for this
(`include/afcommon/state.h`), used by every core.

**Compatibility.** A state loads only into the same romset and the same
`core_version` (a state is not a save: a fix inside the board can move
what any byte means). A core update leaves older states in the list,
marked "made with an older version", and they don't load; the Resume slot
of an older version is dropped and the game starts from power-on. Each
core's `core_version` gets a minor bump when save states arrive (a change
players see).

**Saves inside the state.** The state holds the board's EEPROM and backup
RAM as they were at that frame, as Supermodel's does: the game in RAM and
its settings must agree. Loading an older state therefore puts the
records and settings back to that frame too, and the next `save` writes
them out. The Load state list says so ("records set after this state are
replaced"). Resume does not hit this in normal play, because the Resume
slot is always the newest; when the player has played from power-on since
the Resume slot was written, the Resume button is dropped (the slot is
older than the saves).

## What each core saves

**Model 3.** Supermodel's `CModel3::SaveState` into one chunk: the
PowerPC, 68000s and Z80s, 8 MB of RAM, backup RAM, the Real3D's memory, the
tile generator, SCSP, DSB, the drive board, the 93C46. On load the Real3D
already sends all texture memory to the renderer again
(`UploadTextures(0, 0, 0, 2048, 2048)`); the Metal and GLES renderers are
checked for anything else they cache. Our own additions (the wait-loop
skip, the link's state, the input mapping) are checked to carry nothing
that matters across a frame, or added as chunks. The PowerPC recompiler,
once merged, keeps its state in the same `ppc` registers, so states carry
between it and the interpreter.

**Model 1 and 2.** Per board: the CPU structs (V60 or i960 with its
register cache, the sound 68000, the Z80s), the main and buffer RAMs, the
geometry processor (TGP, SHARC or TGPx4: registers, FIFOs, its RAM; the
uploaded program is rebuilt from RAM), the geometrizer and video state,
the sound boards (SCSP, MultiPCM, the DSB and DSB2, MPEG decoder
position), the I/O boards, the lockstep counters and pending interrupts.
The deferred sound packet is finished before a save. Caches that can be
rebuilt (scenery budget measurements, enhancement state, decoded
textures) are not saved; they are marked stale on load.

**Classic 2D.** As Model 1 and 2, per board (Hang-On, OutRun, X Board,
Y Board, System 32, Turbo): CPUs, RAMs, video, sound chips (YM2151,
SegaPCM, MultiPCM, ...). The transplanted MAME scheduler keeps its
timers as callbacks; each timer gets a name, and the state holds each
timer's name, expiry and parameter, rebuilt into the same callbacks on
load. The FD1089 and FD1094 decryption caches are rebuilt, not saved.

## Where states are kept

| | Apple | Linux |
|---|---|---|
| Folder | `States/<set>/`, beside `Saves/` (kept out of the save folder, so the save backup, which reads every file there, stays as it is) | the same, under `~/.local/share/arcade-foundry/` |
| Files | `resume.afstate`, `1.afstate` to `3.afstate`, each with a `.png` of its screen and a small `.json` (date, play time, core version, app version) | the same, with a `.bmp` (as the app's other pictures); the folder is `states/` |
| Compressed | raw deflate, as replays are (NSData `.zlib`) | the same (zlib, window bits -15), so either reads the other's |

States are per cabinet, like the saves they contain: a state carries the
cabinet's EEPROM, and so its link role, and is never shared between
cabinets.

**iCloud: the Resume slot is synced** (on Apple devices, when Resume where
I left off is on). It goes up to the cabinet's CloudKit zone as a new
record type, `Resume` (cabinet and romset strings, core version, saved
date, the state and its picture as CKAssets, since a state is larger than
a record's 1 MB), next to the cabinet's `Save` record, and follows the
same rules: written when the game is left, uploaded with the save, pending
until it reaches iCloud, never replacing one saved later, and fetched
before the game page shows Resume. So a game left on the iPad resumes on
the Apple TV when both are the same cabinet (Continue as). A Resume slot
from another core version is not offered. The three manual slots stay on
the device. The new record type needs a Development run and a schema
deploy to Production, as `Save` did. A Model 3 state holds 8 MB of main RAM
plus the Real3D's memory before compression, Model 1 and 2 a few MB (both
measured in steps 2 and 4). On Apple TV the app folder is in Caches, so the
system can remove the local slots; the Resume slot comes back from iCloud.
Linux keeps states on the machine, as it does saves.

## Link play, Game Director and replays

- **Link play.** No save states and no Resume while a link is running or
  waiting: the other cabinets can't load the same frame, so the race would
  split. The pause menu hides the items, and leaving a linked game writes
  no Resume slot. Game Director sessions are linked games, so the same.
  Game Cabinet gets the code through IC-Kit and has the items hidden.
- **Replays.** A replay is the inputs from power-on. Loading a state ends
  the recording (the replay so far is kept, ending at that frame). A later
  replay format could start from a state instead of power-on.
- **Operator mode.** States save and load in test mode too; nothing
  special.

## Testing

- **Same game after a load.** A headless run plays a fixed input script,
  saves at frame N, carries on to frame N + M and records each frame's
  screen hash; a second run loads the state at N and plays the same inputs.
  The hashes must match from N to N + M. Model 1 and 2 and Classic 2D are
  deterministic, so this is exact; Model 3 is run single-threaded for it.
- **Each game:** save and load on the title, in a race or stage, in test
  mode, and with sound playing; load a state, play to game over (the
  project's rule for every game).
- **Across launches:** quit, start the app again, Resume.
- **Wrong state:** another set's state, an older core version's, a cut-off
  file: refused with a message, the game carries on.
- The weekend nightly runs the hash check for every set that has states.

## Order of work and size

1. Cores-Common: `afcore.h` 5.4.0 and `afcommon/state.h` (half a day).
2. Model 3: wrap Supermodel's save and load, check the renderers and our
   additions, the hash check (one to two days).
3. App, Apple (IC-Kit) and Linux: the pause menu items, the slots screen,
   Resume on the game page, library and Linux menu, the Resume setting,
   link play rules (three to four days). Model 3 is the first board it
   works with.
4. Model 1 and 2, board by board, Model 2 first (four to six days).
5. Classic 2D, board by board, with the scheduler's named timers (three
   to five days).
6. Release notes, the game pages' "Save states" line, COMPATIBILITY.md.
7. The Resume slot in iCloud (CloudKit `Resume` record, schema deploy)
   (one to two days).

About three weeks in all. Each core's PR bumps its games' core versions
and updates its design document.

## Decided

- Three slots per game per cabinet, plus the Resume slot (10 Nov 2026).
- Resume where I left off is off by default (10 Nov 2026).
- Each board ships as it is ready: Model 3 first, then Model 1 and 2, then
  Classic 2D; a board not ready by the 1.4.0 freeze goes in a later
  release (10 Nov 2026).

- The Resume slot is synced through iCloud, per cabinet (10 Nov 2026).

## Linux app (built 10 Nov 2026)

`linux/states.h` and `.cpp`: slots in `<data>/states/<set>/`, the state
deflated, written beside and moved over (never half written), with its
screen (`.bmp`) and `.json`. The pause menu gets Save state and Load state
(only for a core with save states, not while linked, not in Game Cabinet);
each opens the three slots with date and play time and the chosen slot's
screen. Settings > GAMES > Resume where I left off (off at first) saves
the Resume slot when a game is left (Quit, or the window closed); the
game's page then has a Resume row above the version's Play row, and the
page opens on it. Play from the start removes the Resume slot. A slot from
another core version is marked and not offered. `--play SET --resume`
starts from the Resume slot (tests).

Tested in the cloud container (Xvfb, llvmpipe, scud): a run of 900 frames
leaving a Resume slot, then 300 frames from it, ends on the same screen
hash as one run of 1200 frames (3a46e8d71f36037c). Through the menus with
xdotool: Save state to slot 2, Load state from it, Quit saving Resume, the
page's Resume row, Resume into the game.

## Apple app (written 10 Nov 2026, IC-Kit #13)

The same slots in `States/<set>/` (the `.json` reads the same as Linux's).
Save state and Load state in the pause menu open a slots screen with the
chosen slot's picture; Settings > Save states > Resume where I left off
(off). Leaving a game, or the app going to the background, saves the
Resume slot; the game's page then shows Resume (with its date and play
time) beside Play from the start (on Apple TV a Resume row above the
version's row). The library cards have no context menu, so Resume is on the
page only. Loading a state ends the replay recording; a resumed game records
none.

