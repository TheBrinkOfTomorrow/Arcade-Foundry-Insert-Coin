# Control scheme

How a player controls Arcade Foundry: Insert Coin and its games on each
platform: game controllers, the keyboard and mouse, touch, the iPhone and
iPad's on-screen controller, the Siri Remote, and second players. This
document is the base the other control documents build on. Steering
wheels have their own document, [WHEEL-AND-PEDALS.md](WHEEL-AND-PEDALS.md),
light guns are only summarised here, and other controllers (twin sticks,
fight sticks, fishing rods and the like) will have their own document.

## Purpose and scope

Every game is played through one structure, the core interface's
`afcore_input`, sampled once a frame. The control scheme is how each
platform's controls fill that structure, plus how the same controls move
around the app itself (the library, a game's page, Settings, the pause
menu).

In scope:

- the default controller layout and how it changes with each game's kind
  of controls (`controls` in games.toml)
- the keyboard and mouse (Mac, iPad with a keyboard, Linux)
- touch on iPhone and iPad: the on-screen controller, the on-screen cabinet
  buttons, and touch as a light gun
- the Siri Remote and controllers on Apple TV
- the app's own navigation with each of these
- per-game button maps, operator mode, the pause menu, second players

Out of scope here:

- steering wheels, pedals and shifters ([WHEEL-AND-PEDALS.md](WHEEL-AND-PEDALS.md))
- real light guns, which are still to be written up under Light guns
- the headset view's controls (the Mac's headset play)

## Requirements

1. One controller layout works in every game: Back/Options inserts a coin,
   Start/Menu presses start, and both together open the pause menu.
2. A game is playable with whatever the device has. That means a
   controller anywhere; the keyboard on the Mac, iPad and Linux; the mouse
   for the gun games; and touch alone on iPhone and iPad.
3. A controller drives the whole app, not only the games: the library, a
   game's page and the pause menu, so a TV setup needs no keyboard or touch.
4. Each game can have its buttons laid out differently. The layout is saved
   per game on the device, and the pause chord and operator mode always
   read the buttons as they are, so a remap can't lock the player out.
5. iPad and Apple TV, which have no F2 or F3, still reach the cabinet's
   TEST and SERVICE buttons (operator mode).
6. A second controller is player 2 in the games that have one.
7. The system's own gestures on a controller (holding Share to record the
   screen, Back leaving the app on Apple TV) don't fire during a game.

## Design

### One input, filled once a frame

`Controls.sample()` (Apple, [`Input/Controls.swift`](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-IC-Kit/blob/main/Sources/Input/Controls.swift) in the kit) and
`Controls::sample()` (Linux, `linux/input.cpp`) build one `afcore_input`
per frame. It holds:

- the steering, the gas, the brake, the hand brake, the gear and `stick_y`
- the button bits (`AFCORE_COIN`, `AFCORE_START`, `AFCORE_VIEW1`,
  `AFCORE_PUNCH` and so on), with `buttons2` for player 2
- two guns' aim (`gun_x`, `gun_y`; -1 is off screen)

Every source (the controllers, the keyboard, the mouse, touch and the
on-screen buttons) is merged into that one structure, so they work side by
side and the cores never know which one was used. The two apps are kept
in step by hand: the comments at the top of `Controls.swift` and
`linux/input.h` describe the same layout, and the controls listed on a
game's page come from `ControlsHelp.swift` (Apple) and `control_lines`
(Linux).

### The default layout

Driving games (`drive`, the default):

| Control | Controller | Keyboard |
|---|---|---|
| Steer | Left stick (or d-pad) | Left, Right (ramped towards full lock) |
| Gas, brake | Right, left trigger | Up, Down |
| Shift up, down | Right, left shoulder | A, Z |
| Views 1-4 | A, B, X, Y (South, East, West, North) | 1-4 |
| Hand brake (Sega Rally) | B | 2 |
| Coin | Back / Options / View | 5 |
| Start | Start / Menu | Return |
| Test, Service | Operator mode: A, B | F2, F3 |
| Pause menu | Back + Start together | Esc |

A press of Coin or Start reaches the game a few frames late, so pressing
both to pause neither inserts a coin nor starts. Coin is a short pulse,
while Start lasts as long as it is held, because some games read codes
from a held start button. The shoulders send the shift buttons (which
Manx TT and Virtua Racing read) and also step a gear 1-4 for the other
cars.

Crazy Taxi High Roller uses the shoulders as its shifter's Drive (right)
and Reverse (left), and A or B (keys 1 or 2) as its JUMP button, which the
core sends as buttons 1 and 2.

### Each kind of game

Games.toml's `controls` picks the layout. The same names appear in the
Apple app's `GameControls` and in the Linux game list.

- **flight** (Star Wars Arcade, Wing War, Sky Target, Star Wars Trilogy):
  - The left stick is the flight stick, pushed forward to dive.
  - The right trigger is the throttle. On the keyboard, A opens it and Z
    closes it.
  - A, the right shoulder or Space fires. B and X are the stick's other
    buttons, and the left shoulder is the second trigger.
  - In Wing War, Y steps through the four views.
- **twin** (Cyber Troopers Virtual-On, Oratorio Tangram): each stick is a
  lever, each trigger that lever's shot and each shoulder its dash or
  turbo. On the keyboard, WASD with Q and E is the left lever, and IJKL
  with U and O is the right.
- **fighting** (Virtua Fighter 2 and 3, Fighting Vipers 2, and the others
  played the same way):
  - The left stick or d-pad is the joystick.
  - X, A, B and Y are buttons 1-4 (punch, kick, guard, and Virtua Fighter
    3's escape). On the keyboard they are A, S, D and F.
- **gun**, **turret**, **analogGun** (Virtua Cop, The House of the Dead,
  the Model 3 guns): the gun is aimed and fired with any of these:
  - the mouse, or a touch;
  - a controller: either stick moves the aim, the right trigger or A
    fires, B reloads, and X throws The House of the Dead 4's grenade.
    In Let's Go Jungle!, X (West on Linux, G or middle-click) presses its
    player's Start as well: the cabinet's big Start buttons are the game's
    action buttons during play (#404).
    In Virtua Cop 3, X (West) changes weapon and Y (North, or F on the
    keyboard) is the cabinet's foot pedal, on each player's own
    controller; B reloads as in the others. In The House of the Dead III,
    B reloads by pumping the gun's grip (the core pulls it twice).

  A crosshair shows the aim while a controller is connected, but not for
  the turrets, which draw their own sight. Model 3's guns can't sense a
  shot off screen, so their second trigger reloads.
- **fishing** (Sega Bass Fishing, Get Bass):
  - The left stick is the rod and the right trigger reels.
  - The left trigger is the line's tension, and the d-pad the lure stick.
  - X casts and A selects.
- `stick_y` (Sega Ski Super G, Top Skater, the fishing rod): the left
  stick's forward and back is a second analog control.

### Button maps

Each game's buttons can be laid out again (`ButtonMap` on Apple,
`PadButton` maps in `linux/input.h`). For each button of the default
layout, the map names the button that stands in for it:

- Apple: on the game's page, under Controller (choose an action, then
  press its new button).
- Linux: under Controller buttons, in the pause menu or on the game's page.

Only buttons move. The sticks and the d-pad keep their jobs, and a trigger
that stands in for a button reads as a button, while a button that stands
in for a trigger reads as fully pressed. The pause chord, operator mode and
the menus read the buttons as they are, so a bad map can always be undone.
The maps are saved per game on the device: in `UserDefaults`
(`buttons.<set>`) on Apple, and in `<data>/buttons/<set>` on Linux.

### Two players

With two controllers connected, the first connected is player 1 and the
second is player 2. Which one was used last doesn't matter, so both can't
end up as player 1. Player 2 is used in two ways:

- In the fighting games (`buttons2`), it gets the same layout as player 1.
- In the gun games, it has a gun of its own, aimed with its sticks, and a
  blue crosshair once its Start has been pressed.

Player 2's Start is `AFCORE_START2`. On the keyboard, Star Wars Arcade
takes 6.

### Operator mode

The cabinet's TEST and SERVICE buttons are F2 and F3 on a keyboard. iPad
and Apple TV have neither, so the pause menu turns on operator mode:

- A is TEST and B is SERVICE.
- On iPad, Test and Service buttons also appear on the screen.
- The game's own buttons are off, while the stick and pedals stay on for
  test mode's input checks and steering calibration.
- A and B only count once both have been let go, so the A that chose the
  menu item doesn't press TEST.

A banner says how to leave operator mode.

### The pause menu

The pause menu opens from any of these:

- Esc
- Back and Start together
- the on-screen pause button (iPhone, iPad)
- the Siri Remote's Menu/Back (Apple TV)
- leaving the app, because a game never carries on by itself in the
  background

Inside it, up and down choose, and A, Return or Space picks. The same
control that opened it, or B, closes it.

### Touch on iPhone and iPad

Touch is the only control an iPhone, or an iPad without a keyboard or
controller, has. It is made of three layers.

1. **The on-screen controller** (`GCVirtualController`). Apple draws it,
   and it shows up as one more extended gamepad, so every layout and
   button map above works on it unchanged. When it shows is a setting
   (below); by default while no controller is connected, and not in the gun
   games, which aim by touch. Its buttons depend on the game:

   | Game's controls | On-screen controller |
   |---|---|
   | drive (#362) | left stick to steer; on the right A gas, B brake, X shift down, Y shift up, right shoulder the next view |
   | flight and the rest | left stick, both triggers, both shoulders, A, B |
   | twin | both sticks, both triggers, both shoulders, A, B |
   | fighting | left stick, X, A, B, Y |
   | fishing | left stick, both triggers, X, A, B |
   | gun, aimed by the on-screen controller | left stick, A, B; X with a gun button (HotD4) |

   Apple decides where each element goes, and the app can't move them:
   the triggers and the left shoulder sit beside the stick, under the
   steering thumb. So the driving layout (#362) puts the pedals and shifts
   on A, B, X and Y, read that way only from the on-screen controller; a
   real controller and button maps are unchanged.

   Every layout has A and B, because operator mode needs them as TEST and
   SERVICE.

2. **The on-screen cabinet buttons**, along the top in SwiftUI:
   - Pause, and the speed when Show speed is on.
   - Coin and Start, held while touched (`Controls.touchButtons`).
   - Test and Service, while operator mode is on.

   These are always there, with or without a controller, because the
   on-screen controller has no Options or Menu button to coin and start
   with.

3. **Touch on the picture.** In the gun games, while they aim by touch,
   the screen is the gun (`Controls.touchAims`; elsewhere a touch on the
   picture does nothing):
   - one finger aims where it touches and fires;
   - a second finger reloads;
   - a third throws The House of the Dead 4's grenade.

   The touch is mapped through the picture's place on the screen
   (`Controls.picture`, from the renderer), so it lines up whatever the
   screen's shape and size.

A keyboard on an iPad works as on the Mac, and a controller works as
anywhere else.

### Touch settings (#359)

Which touch controls a game gets is the player's choice, globally and per
game (`Controls.startTouchControls`, from the game view).

- **On-screen controller** (Settings, iPhone and iPad):
  - *Automatic* (the default): shown while no controller is connected,
    hidden when one connects and shown again when it disconnects, during
    the game as well as at its start.
  - *Always*: shown even with a controller connected. The controllers
    count as players in the order they connected, with the on-screen
    controller last, so beside one controller it is player 2.
  - *Off*: never shown; the cabinet buttons stay.
- **Per game**, in the game's options: *Use the global setting* (the
  default), or *Automatic*, *Always* or *Off* for that game alone.
- **Per gun game**, *Aim with*:
  - *Touch* (the default): the screen is the gun, as above, and the
    on-screen controller isn't shown.
  - *On-screen controller*: the left stick moves the crosshair (shown),
    A fires, B reloads and X throws the grenade, as a controller does.

Automatic watches `GCControllerDidConnect` and `GCControllerDidDisconnect`,
leaving out the on-screen controller's own (it names itself "Apple Touch
Controller").

### Apple TV

A controller plays the games. The Siri Remote works the app through tvOS's
focus: the library's cards and a game's page. During a game, its Menu/Back
opens the pause menu. Swiping and clicking work the menu, and Menu closes
it again.

While a game plays, `GameView` keeps the controller's buttons from tvOS
(through a `GCEventViewController`), or B and Menu would act as Back and
leave the app. The pause menu and alerts hand them back. The crosshair is
always shown on Apple TV, because there is no mouse or touch to show the
aim. tvOS has no keyboard input for the games.

### The app with a controller or keyboard

The app itself can be worked without touch or a mouse:

| Where | Controller | Keyboard (Mac, iPad) | Linux |
|---|---|---|---|
| Library | d-pad or stick moves, A opens, X cards or list, Y playable only, L1/R1 platform, L2/R2 sort, Menu Settings (iOS) | the system's own (Tab, Space); no keys of the app's | Up/Down, Return; Left/Right or L1/R1 platform; S, Shift+S or R2/L2 sort; P or North playable; Tab or Start Settings; C or West credits |
| Game page | up/down picks a version, A plays or downloads, B back | the system's own | Up/Down, Right/Left folds a version's settings, Return plays or changes, I or North shows the art, Esc or East back |
| Pause menu | up/down, A picks, B closes | arrows, Return, Esc | arrows, Return, Esc; South, East |

The library on Apple TV uses tvOS's focus instead. The library ignores a
controller until every button has been let go, so the A that ended a game
doesn't open another. On the Mac, Help › Controls (⌘?) shows the playing
game's controls over the pause menu. A game's page lists its controls on
every platform.

### The system's gestures

`claimButtons` turns off the system's gestures on Options, Menu and Home
for every controller, so holding a PlayStation controller's Share doesn't
offer to record the screen. On Linux there are none to turn off. The Steam
Deck's own Steam button stays with Steam.

## Data and settings

| What | Apple | Linux |
|---|---|---|
| A game's button map | `UserDefaults` `buttons.<set>` | `<data>/buttons/<set>` (`a=b,rightTrigger=a`) |
| A gun game's crosshair | `crosshair.<set>` (on by default, off for Let's Go Jungle) | `<data>/crosshair/<set>` when off |
| On-screen controller (#359) | `touchControls` (automatic, always, off) | not used |
| A game's on-screen controller (#359) | `touchControls.<set>` (unset: the global setting) | not used |
| A gun game's aim (#359) | `aimByTouch.<set>` (on by default) | not used |
| Operator mode | per session, not saved | per session, not saved |
| Wheel settings | not yet | `<data>/wheels/` ([WHEEL-AND-PEDALS.md](WHEEL-AND-PEDALS.md)) |

## Platforms

| | Controller | Keyboard | Mouse | Touch | Second player | Wheel |
|---|---|---|---|---|---|---|
| macOS | Yes | Yes | Gun games | No | Second controller | Planned |
| iPad | Yes | Yes (a keyboard attached) | Through the pointer, as touch | On-screen controller, cabinet buttons, gun | Second controller | Planned |
| iPhone | Yes | No | No | As on iPad | Second controller | No |
| Apple TV | Yes (needed to play) | No | No | No (the remote works menus) | Second controller | No |
| Linux | Yes (SDL gamepads) | Yes | Gun games | No | Second gamepad | Yes |

## Testing

- Each kind of controls has been played with a controller, and the
  keyboard where there is one, on a full game to game over. The Linux
  app's Xvfb runs inject keys (XTest) for the menus.
- Things to check by hand after a change to `Controls`:
  - the pause chord inserts no coin;
  - operator mode's A doesn't press TEST;
  - a remap of every button, then Reset;
  - two controllers in a fighting game and in a gun game;
  - an iPad with no controller in a driving, fighting, fishing and gun game;
  - Apple TV's Back during a game.

## Open items

- **The twin layout on a phone.** Two sticks, four trigger buttons and
  A and B is the most Apple's on-screen controller draws; how it fits an
  iPhone's screen is to be checked on a device (#359).
- **The on-screen controller and button maps.** A map applies to the
  on-screen controller too. The Controller screen should say so, or the
  on-screen layout should follow the map.
- **The driving layout has no hand brake** (Sega Rally's is B on a
  controller, the brake on the on-screen controller) and its pedals are on
  or off. Tilt to steer (#364) is a future item that may or may not be
  built, behind a setting.
- **Apple TV with only the Siri Remote** can't play. The remote could
  steer (tilt) or work as a simple pad for some games.
- **Wheels on the Apple platforms**, fight sticks, flight sticks, real
  light guns and other controllers (twin sticks, fight sticks, fishing
  rods and the like): planned in their own documents.
- **The library has no keys of its own** on the Mac and iPad: a keyboard
  moves only through the system's focus, while a controller has the full
  set. Arrows and Return, as on Linux, would match.
- **One description of the controls.** The layout is written three times
  (`Controls.swift`, `linux/input.h`, `ControlsHelp.swift` and
  `control_lines`) and kept in step by hand. A table generated from
  games.toml, as the game list is, would keep them from drifting.

## Decisions

- **One input structure for every source.** Controllers, keyboard, mouse,
  touch and wheels all merge into `afcore_input`, so the cores never change
  for a new kind of controller.
- **The same default layout in every game,** with the game's kind of
  controls choosing the rest, rather than a layout per game. A player
  learns it once.
- **Coin and Start on Back and Start, pause on both together,** because
  every controller has those two buttons and no game needs both at once.
- **Apple's on-screen controller rather than a custom one,** so touch is
  just another controller to the rest of the code and to button maps.
  Coin, Start and Pause are separate SwiftUI buttons, because the on-screen
  controller can't show Options or Menu.
- **Settings, not guesses, for touch (#359).** Automatic suits most
  players, Always and Off cover the rest, and a game's own choice beats
  the global one, as the screen settings do.
- **Touch is the gun in the gun games,** with no on-screen controller, so
  aiming is direct.
- **Operator mode instead of extra buttons,** so TEST and SERVICE work on
  devices without F2 and F3 and on any controller.
- **The pause chord and the menus ignore button maps,** so a map can never
  lock the player in a game.
