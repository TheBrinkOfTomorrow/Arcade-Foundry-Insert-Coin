# Steering wheels and pedals

Issue #318. This document covers the Linux app, which is done, and the plan
for the Apple platforms, self-centring and force feedback.

## Purpose and scope

Play the driving games with a real steering wheel, pedals and shifter
instead of a gamepad. The first wheels are the Logitech Driving Force GT and
the Logitech G29, but any wheel the platform recognises works after a
one-time calibration.

In scope:

- analog steering, gas and brake
- the shifter and paddles
- the wheel's buttons, in game and in the app's menus
- per-wheel settings, and calibration for unknown wheels
- a self-centring spring
- Linux and macOS

Planned:

- iPadOS and tvOS (the G29 only)
- force feedback from the games

## Requirements

1. In every game whose controls are `drive` (the cars, bikes, jet skis and
   so on), the wheel steers, the pedals are the gas and brake, and the
   shifter and paddles shift.
2. The wheel works next to a gamepad and the keyboard. Neither replaces the
   other, and plugging a wheel in or out during a game is safe.
3. A wheel this document lists works without calibrating. Any other wheel
   needs one calibration, after which its settings are kept for that model.
4. The wheel's buttons that have a gamepad equivalent work the library, a
   game's page, Settings and the pause menu.
5. Steering feel can be tuned: how far to turn for full lock, how the turn
   builds up, and the dead zone.
6. The cores do not change. Everything a wheel needs already fits the core
   interface.

## Design

### Where it sits

```
wheel (USB HID)
  → platform input: SDL joystick on Linux; IOKit HID on the Mac
  → Wheel: raw axes and buttons, the wheel's setup (axes, ranges, buttons)
  → Controls::sample: mixes wheel, gamepad and keyboard each frame
  → afcore_input: steer, accel, brake, handbrake, gear, buttons
  → core
```

`afcore_input` (Arcade-Foundry-Cores-Common `include/afcore.h`) already
carries:

- `steer`, from -1 to +1
- `accel` and `brake`, from 0 to 1
- `handbrake`
- `gear`
- the `SHIFT_UP` and `SHIFT_DOWN` buttons

That is why no core changed. Each core maps these values to its board's
inputs. For example, Lindbergh maps them to JVS analogs 0-2 plus a gear lever.

### Finding the wheel

SDL never lists a wheel as a gamepad. It keeps a list of known wheels by USB
vendor and product (`SDL_IsJoystickWheel`), which includes all of Logitech's.
So the app opens the first joystick whose type is `SDL_JOYSTICK_TYPE_WHEEL`
as a joystick, alongside the gamepads, and reopens one when it is plugged in
(`SDL_EVENT_JOYSTICK_ADDED` and `_REMOVED`).

Logitech's wheels start in a compatibility mode (product `c294`: 200
degrees, combined pedals, few buttons). Linux's hid-logitech driver switches
the Driving Force GT and the G29 to their native mode (900 degrees, separate
pedals, every button). On the Mac the app sends the same switch command
itself (see Platforms).

### Mixing with the gamepad and keyboard

In `drive` games (not `flight` or `twin`), each frame does the following:

- **Steering:** the wheel's steering value is used whenever it is turned at
  least as far as the gamepad's stick. Arrow keys still ramp the steering as
  before.
- **Gas and brake:** each is the larger of the wheel's pedal and the
  gamepad's trigger.
- **Buttons:** the wheel's buttons are added to the gamepad's.
- **Shifting:** the shifter and the paddles both shift up and down.
- **Coin, Start and the pause menu:** these wheel buttons work in every game.

### Steering

The steering axis covers the wheel's whole turn, which is its lock. That is
900 degrees for the Driving Force GT and the G29 under hid-logitech. The value
the game sees is worked out in four steps:

1. `n` is the axis position either side of its resting centre, from -1 to +1
   across the whole lock.
2. `out = n × lock / range`, clamped to -1 to +1. `range` is how many degrees
   of turn make the game's full lock. The default is 270 degrees, about a
   cabinet's wheel.
3. Dead zone: below `deadzone` percent the steering is 0. Above it, the rest
   is rescaled back to 0 to 1.
4. Curve: `steer = (1 − k)·x + k·x³`, with `k = curve / 100`.

At the default curve of 50%, the centre is half as quick as linear and the
last part of the turn twice as quick:

| Wheel turned (of `range`) | Game sees |
|---|---|
| 10% | 5% |
| 25% | 13% |
| 50% | 31% |
| 75% | 59% |
| 100% | 100% |

A curve of 0% is linear, and 100% is a pure cubic. The first drive (Scud
Race Plus) was too quick at the centre when linear, and felt right with the
50% curve.

### Pedals

Each pedal has an axis, a resting value and a fully pressed value, learned
by calibration. A pedal can run either way: the Driving Force GT's rest at
the top and fall when pressed. The app maps rest to fully pressed onto 0 to
1, ignoring the first 3% so a resting pedal never reads as pressed.

### Buttons and actions

A wheel button can be set to any of these cabinet actions:

- Shift up and Shift down (the shifter)
- Paddle up and Paddle down (both shift too)
- View 1 to View 4
- Coin
- Start
- Hand brake (Sega Rally)
- Pause menu

Each action takes one button. Buttons are numbered as SDL numbers them on
Linux, which for these wheels is the HID button number − 1. The Mac
numbers them the same way, so a wheel's setup reads the same on both.

### The wheel in the menus

The function `wheel_as_pad` turns the wheel's d-pad and its buttons with a
gamepad equivalent into gamepad events. It puts them in the event queue, so
every screen that takes a gamepad takes the wheel too. Logitech's layout
(HID numbers):

| Wheel | Gamepad |
|---|---|
| D-pad | D-pad |
| 1 Cross, 2 Square, 3 Circle, 4 Triangle | South, West, East, North |
| 5 R1, 6 L1 (the paddles) | Right, left shoulder |
| 7 R2, 8 L2 | Right, left trigger (a full pull, then let go) |
| 9 Select, 10 Start | Back, Start |
| 11 R3, 12 L3 | Right, left stick click |
| 21 PS | Guide |

These events carry the wheel's id. The wheel's own screen ignores them, so
choosing a button for an action does not also press "pick".

On the Mac the library, a game's page and the pause menu read a gamepad
through `MenuPad` (`MenuPad.swift`), which adds the wheel's d-pad and
buttons by the same table (Cross is A, Square X, Circle B, Triangle Y,
Select Options, Start Menu). The wheel's pause button opens and closes the
pause menu.

### Calibration

Pause menu > Wheel and pedals > Calibrate:

1. Centre the wheel, let go of the pedals, and press Return or a wheel
   button. The app records every axis at rest.
2. Turn the wheel. The axis that moves past a quarter of its travel is the
   steering, and its resting value is the centre.
3. Press the gas all the way, then let go. The axis that moves past half its
   travel, other than the steering, is the gas. Its furthest value is fully
   pressed.
4. Press the brake the same way. Any axis other than the steering and gas
   counts.

Esc stops calibration and keeps the old setup. Reset forgets the wheel's
setup and goes back to the defaults for its model.

## Data and settings

The settings are kept per wheel model, not per game. Linux keeps them in
`<data>/wheels/<vendor>-<product>` (hex, for example `046d-c29a`), as
`key=value` lines:

```
steer_axis, steer_center
gas_axis, gas_rest, gas_full        (gas_axis -1: not calibrated)
brake_axis, brake_rest, brake_full
lock      wheel's turn lock to lock, degrees (90-1080)
range     degrees of that for the game's full lock (30-lock, default 270)
deadzone  percent (0-20, default 0)
curve     percent (0-100, default 50)
centring  self-centring strength, percent (0-100, 0 off, default 50)
shift_up, shift_down, paddle_up, paddle_down, view1-view4,
coin, start, handbrake, pause      button numbers (from 0)
```

The Mac keeps the same lines in its user defaults, under the key
`wheel.<vendor>-<product>`.

### Defaults by model

| Wheel | Lock | Axes (steer, gas, brake) | Buttons (HID numbers) |
|---|---|---|---|
| Driving Force GT, native (`046d:c29a`) | 900 | X, Y, Z; pedals rest at the top | Shifter 13/14, paddles 5/6, views 1-4, Select coin, Start start, PS pause |
| G29 (`046d:c24f`, `c260`) | 900 | Calibrate (to be filled in from evtest) | To be filled in |
| Momo (`c295`, `ca03`) | 240 | Calibrate | Calibrate |
| Logitech in compatibility mode (`c294`) | 200 | Calibrate | Calibrate |
| Any other wheel | 900 | Calibrate | Calibrate |

The Driving Force GT's layout comes from evtest on Linux. The direction of
the shifter and the paddle numbers still need to be confirmed on the wheel.

## Platforms

- **Linux:** done. See `linux/wheel.h`, `linux/wheel.cpp`, `Controls` in
  `linux/input.*` and the event loops in `main.cpp` and `session.cpp`.
- **macOS (built, to test on the wheels):** `apple/InsertCoin/Input/Wheel.swift`
  reads every Logitech wheel through IOKit HID (`IOHIDManager`), the G29
  included, so setup, calibration and buttons work the same everywhere.
  Apple's `GCRacingWheel` only knows the G29, G920 and G923, so it is not
  used. Axes are numbered in HID usage order (X, Y, Z, Rx, Ry, Rz, slider,
  dial, wheel, then the Simulation Controls page, then any other, a
  vendor's own pedals for one) and scaled to -32768 to 32767, the same as
  SDL on Linux; buttons by their HID number - 1, as on Linux. The d-pad is
  the HID hat switch. A Logitech wheel found in compatibility mode
  (`046d:c294`) is told apart by its version number, as Linux's hid-lg4ff
  does: Driving Force GT `0x13xx` (the test wheel reports `0x1322`), G29
  `0x1350`-`0x1357`. The app then sends `f8 0a 00 00 00 00 00` and the
  model's `f8 09 …`, and the wheel reconnects as itself. It is sent once per
  USB location in 10 seconds, so a wheel that ignores it is used as it is.
  On connect the log lists the wheel's axes (page, usage, range), for
  checking a new wheel. The Driving Force GT's pedals default to the axes
  named Y and Z (or Rz); if its descriptor names them otherwise, Calibrate
  finds them. Self-centring is Logitech's autocentre command, worked out as
  hid-lg4ff's `lg4ff_set_autocenter_default` (`fe 0d …` then `14 …`; `f5 …`
  turns it off, and quitting the app turns it off). The wheel's own turn
  lock to lock is Logitech's range command, `f8 81 lo hi 00 00 00`
  (hid-lg4ff's `lg4ff_set_range_g25`, for the G25, G27, Driving Force GT and
  G29), sent with Wheel turns (40 to 900) on connect and on every change.
  Linux's driver sends 900 itself; without it the wheel stays at its own
  200 degrees and the game reaches full lock far too soon. The settings are
  in Settings > Wheel and pedals (a form, for the mouse) and in the pause
  menu's Wheel and pedals (`WheelSettings.swift`: `WheelPanelModel` and
  `WheelPanel`), which works like Linux's pause-menu screen: up and down
  choose a row, left and right change it, A or Return chooses (Calibrate,
  Wheel buttons, Reset, Back), B or Escape goes back, and X or Delete
  clears a wheel button, from a wheel's d-pad and buttons, a gamepad or
  the keyboard (the mouse works too). The settings are kept per model in the user
  defaults (`wheel.<vendor>-<product>`) as the same key=value lines. The
  library, a game's page and the pause menu read a wheel's d-pad and buttons
  as a gamepad's (`MenuPad.swift`). The app sandbox gets
  `com.apple.security.device.usb`.
- **iPadOS and tvOS (planned):** only `GCRacingWheel` is available there (no
  IOKit), so only the wheels Apple supports (G29, G920, G923) will work. The
  Driving Force GT will not. This needs confirming on the devices.

## Self-centring and force feedback

Self-centring does not need the games' force feedback. It is a constant
spring effect that the wheel's motor runs on its own:

- **Linux (built):** hid-logitech supports `FF_AUTOCENTER`. The app opens
  the wheel's haptic device beside its joystick (`SDL_OpenHapticFromJoystick`,
  with `SDL_INIT_HAPTIC`) and sets the strength with
  `SDL_SetHapticAutocenter`: when the wheel is opened, when the setting
  changes and on reset. At exit it sets it back to 0, which is how the
  driver starts the wheel. The wheel's event device has to be writable;
  udev's `uaccess` rule gives the seat's user that for joysticks. A wheel
  with no autocentre (or no write access) shows "Self-centring: not on this
  wheel", and the log says "no self-centring".
- **macOS (built):** Logitech's HID autocentre command, sent the same way
  as the mode switch (see Platforms).

One setting, its strength, on the Wheel and pedals screen in 10% steps: 0 is
off, and the default is 50% until the dead-zone test (see Open items) settles
both defaults. It is kept per wheel like the rest. With the spring on, the
wheel comes back to a true centre by itself. That leaves only sensor noise
for the dead zone to hide, so the default dead zone is chosen after
self-centring is in.

Real force feedback comes later and needs the cores. The drive boards' FFB
commands (Lindbergh's serial drive board, the Model 2 and 3 drive board
CPUs) have to be turned into the wheel's effects. It is not part of the first
release.

## Testing

- **Unit test:** a test on SDL's virtual joystick, made to look like
  `046d:c29a` and typed as a wheel. It covers:
  - the defaults and the steering scale and curve
  - the pedals in either direction
  - calibration with swapped axes
  - learning and clearing a button
  - saving, loading and reset, the self-centring setting included
  - the dead zone at 2%
  - a wheel with no motor: self-centring cannot be changed
  - unplugging and closing
  - `wheel_as_pad`: one press per d-pad push, triggers going full and back,
    and the wheel's screen ignoring its own events
- **Screenshots:** of the Wheel and pedals screens, under Xvfb.
- **On the wheel:** the test mode's input check (full steering and pedal
  travel), then a full race in Daytona USA, Scud Race, Sega Rally, OutRun 2
  SP SDX and Sega Race TV. Then the library and menus with only the wheel.

## Open items

Tracked in issue #318.

1. Self-centring spring: built on Linux; to test on the Driving Force GT.
2. Test dead zones (0, 2 and 4%) with the spring on, then choose the
   defaults for both.
3. Sega Race TV's name entry scrolls with the wheel connected. It is
   suspected to be the Lindbergh core's gear lever, which stays up or down
   after a shift. If confirmed, the fix is in the Lindbergh core.
4. Confirm the Driving Force GT's shifter direction and paddle buttons.
5. G29: its axes and buttons on Linux (evtest), and defaults like the
   Driving Force GT's.
6. macOS: built (IOKit HID, the native-mode switch, self-centring, Settings >
   Wheel and pedals, the wheel in the menus); to test with both wheels on
   the Mac mini.
7. iPadOS and tvOS: `GCRacingWheel` for the G29.
8. Force feedback from the games (later; needs the cores).

## Decisions

- **The cores are unchanged.** `afcore_input` already has what a wheel
  needs.
- **One input path per platform for every wheel** (SDL on Linux, IOKit on
  the Mac), not Apple's `GCRacingWheel` on the Mac, because it lacks the
  Driving Force GT.
- **Settings per wheel model, not per game.** A feel set in one game carries
  to the others.
- **Default range 270 degrees and a 50% curve.** These came from the first
  drive. The dead zone stays at 0% until self-centring is tested.
- **Self-centring is the driver's autocentre**, not a spring effect the app
  plays: the driver keeps it running with no effect to upload or restart,
  and it is one call on Linux and one HID command on the Mac.
- **The wheel works the menus through gamepad events** rather than its own
  code in each screen.
