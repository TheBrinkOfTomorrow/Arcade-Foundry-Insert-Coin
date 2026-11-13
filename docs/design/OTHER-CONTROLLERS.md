# Other controllers

How controllers beyond a gamepad, keyboard, mouse and touch map to the games:
twin sticks, fight sticks, fishing rods, floor mats and other cabinet-style
controllers, each to be tried on Linux, macOS, iPhone, iPad and Apple TV.
Steering wheels have their own document, [WHEEL-AND-PEDALS.md](WHEEL-AND-PEDALS.md),
and the gamepad layout this builds on is in [CONTROL-SCHEME.md](CONTROL-SCHEME.md).

This document is being written one controller at a time. So far it covers
the GAMR Play Pad. Twin sticks (for Virtual-On), fight sticks and fishing
rods are still to come.

## GAMR Play Pad (issue #435)

Status: investigated on paper only, not tested. A maybe on the roadmap: no
code is planned until a mat has been tried with the app as it is.

### What it is

The GAMR Play Pad, sold by Apple as the "GAMR Active Game Controller", is a
roll-up floor mat (99 × 94 cm) from Gamex Studios. It has nine pressure
zones in a 3 × 3 grid:

| Zone | Reports as |
|---|---|
| Four arrow tiles | D-pad up, down, left, right |
| Four action tiles | A, B, X, Y |
| Centre G tile | "Home" (to be confirmed) |

It connects over Bluetooth 5.0 or a USB-C cable. The maker says it "shows up
as a standard Bluetooth game controller", and Apple lists iPhone, iPad,
Apple TV 4K (2nd generation and later), Mac and Apple Vision Pro, and says it
works with Apple Arcade games. It almost certainly reaches the app as a
`GCController`, but whether as an extended gamepad (with its sticks,
triggers and shoulders left at rest) or a micro gamepad is not known. It has
no sticks, triggers, shoulder buttons, Options or Menu, no motion sensor and
no SDK. Whether a zone sends how hard it is pressed or only on and off is
not stated. The maker's app remaps the zones and updates the firmware.

### How it fits today

The kit reads extended gamepads only (`Controls.swift`), so if the mat is
one, it works with no code, within these limits:

- **Fighting games** (Virtua Fighter 2 and 3, Fighting Vipers, Last Bronx):
  the d-pad and the face buttons are the joystick and the buttons, so the
  default layout works as it is.
- **Top Skater and Sega Ski Super G:** the arcade players stood on these
  cabinets, which makes them the best fit. Top Skater curves on the left
  stick, which the mat lacks, so it needs a look; Ski Super G needs a button
  map.
- **Driving games:** the gas and brake are the triggers, which the mat
  lacks. A per-game button map (right trigger to A, left trigger to B) makes
  them playable. The d-pad already steers, at full lock. A party game rather
  than a way to race.
- **Gun games:** none of the gun games in the catalog has a foot pedal, so
  the mat adds nothing.
- **Flight and twin-stick games:** not playable.

### Snags

1. **Coin, Start and the pause menu.** These are Options, Menu and both
   together. The mat has neither, so a player can't put in a coin, start or
   open the pause menu from it. What the G tile reports decides how much of
   this is left.
2. **A mat beside a gamepad.** A second controller is always player 2
   ([CONTROL-SCHEME.md](CONTROL-SCHEME.md), Two players). There is no way to
   add the mat to player 1 as their feet while they hold a gamepad.
3. **Linux.** SDL3 may not know the mat and may need a gamepad mapping
   (`SDL_GAMECONTROLLERCONFIG`).
4. **Game Director.** A mat per seat (an Apple TV cabinet, say) works as that
   seat's controller, with snags 1 and 2.

### Recognising the mat

The plan, if code is needed, is to recognise the mat as a mat rather than
treat it as one more gamepad, then give it its own layout and role:

- **Apple platforms:** `GCController` gives `vendorName` and
  `productCategory` (no Bluetooth address and no USB vendor and product IDs).
  The test logs what the mat reports there. On the Mac the IOKit HID path the
  wheel uses (`Wheel.swift`) also sees the vendor and product IDs.
- **Linux:** SDL3 gives the vendor and product IDs and the joystick GUID.
- **A mat profile**, chosen by that identity:
  - its own default layout per kind of game (gas and brake on the tiles in
    the driving games, G as Start), so no per-game button map is needed;
  - **feet for player 1**, on by default: the mat's input is added to the
    player's gamepad instead of becoming player 2, so a player can steer and
    shoot with the gamepad and use the mat at the same time. With no gamepad,
    the mat is player 1 on its own. A second mat or gamepad is player 2.
- The same profile mechanism suits the other controllers in this document
  (fight sticks, twin sticks), which also report as gamepads.

### Test plan

When a mat is on hand:

1. Pair it with an iPad, an Apple TV and a Mac. Log what `GCController`
   reports: extended or micro gamepad, its product category, and what the G
   tile sends.
2. Check whether the zones are analog, and whether several can be held at
   once.
3. Play Virtua Fighter 2, Top Skater, and Daytona USA with the gas on A.
4. On Linux, try it with SDL3.
5. Only then decide on code: the mat profile above (identity, layout, feet
   for player 1), sized by what the test shows.
