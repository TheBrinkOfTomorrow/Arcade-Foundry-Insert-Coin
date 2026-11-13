# Roadmap

Things I'd love to add to Arcade Foundry: Insert Coin. It's one developer's
wish list, not a plan:

- **No order.** Nothing here is ranked; I work on things as time, hardware
  and interest allow.
- **No promises.** Some of these may never ship, change shape, or be
  replaced by something better.
- **No dates.** There is no timescale for starting or finishing any of them.

Arcade Foundry: Insert Coin is not affiliated with or endorsed by Sega, and
nothing on this list involves ROM data: every game still runs from your own ROM sets.

## Arcade controls

Support, tested on real hardware, for the controls the cabinets had, not
only game controllers.

- **Arcade sticks** and button panels.
- **Steering wheels** and pedals: more wheels than the Logitech Driving
  Force GT (which works on Linux and the Mac now), the Logitech G29 next, and
  force feedback where the game had it.
- **Light guns**, such as the Sinden Lightgun, and others.
- **Tilt to steer** on iPhone and iPad, as a setting.
- **Unique controllers**: the fishing rod, Magical Truck Adventure's lever,
  skis and the like.
- **Motion seat rigs.**: maybe, but a long way off

## VR headsets

Play the games in full, immersive 3D.

- **Apple Vision Pro** - hoping for access to one soon
- **Steam Frame** - (pending hardware aviailability)
- **Meta Quest** maybe (and pending hardware aviailability)
- Two ways to play: **full view**, where you sit in the car or cockpit and
  look around, and **cabinet**, where the game's screen floats in front of
  you in 3D.
- Hand and controller tracking, for gun aim

## Other platforms

- **Linux beyond x86-64**:
-   ARM64 SBCs (Pi 4/5) - planned for next major release
-   Radxa Dragon and/or Orion, RISC-V SBCs (pending hardware aviailability)
- **Windows.** The desktop front end and the Model 1, Model 2 and Classic 2D
  cores already compile with Clang on Windows, so this is mostly packaging and
  testing, plus a Windows OpenGL context for Model 3. Lindbergh games need a
  different graphics path first (their NVIDIA shader extension has no
  Windows driver support outside NVIDIA's own), so they would come later.
- **Lindbergh games on Apple TV**, on the Apple TV 4K third generation and
  later, once their Metal graphics are done.
- **Android.** maybe, as its own app. Android devices are planned to join
  Game Director's races through a web page in Chrome.

## More games

- **The full catalog of Lindbergh games.**
- **More Sega Chihiro games**, after Virtua Cop 3, The House of the Dead III
  and Crazy Taxi High Roller.
- **Other Sega arcade boards.**
- **Sega's early-1980s Turbo hardware**: Turbo, Subroc-3D and Buck Rogers:
  Planet of Zoom.
- **The last four Model 2 games**: Power Sled, Air Walkers, Royal Ascot II
  and Hanguk Pro Yagu 98. MAME can't run any of them yet, so each needs
  extra work first.
- **Arcade boards from other makers.**
- The remaining games on the boards already supported.

## Speed and polish

- Saves backed up and synced on Linux, as iCloud does on Apple devices:
  Dropbox, Google Drive or your own home server (to be investigated).
- **Metal graphics for the Lindbergh games** on Mac, iPhone and iPad,
  replacing the OpenGL path they use today.
- **Game art in the library**: flyers and title screens downloaded on your
  device the first time a game is shown, instead of only screenshots.
- A recompiler for the Model 3's PowerPC, so Model 3 games run faster,
  above all on iPhone, iPad and Apple TV.
- **Sturdier link play**: linked cabinets check that they belong to the same
  game, and a cabinet that stops answering drops the link instead of
  freezing the others.
- **Arcade Foundry: Game Director**, a companion app for cabinet versus
  cabinet (VS): one Mac with your games sets up your other devices (Mac,
  iPad, iPhone, Apple TV, Linux) for a linked race, putting a small player
  app, Arcade Foundry: Game Cabinet, on each for the session and tidying up
  afterwards. Later: seeing the other players' screens, a
  spectator screen, two players on two devices for Virtua Cop, and joining
  from an Android phone or tablet (or any device with Chrome) through a web
  page that plays the game streamed from the Game Director.
- **Save states for the Classic 2D games**, as the Model 1, 2 and 3 games
  have them.
- ideas and bug reports are welcome as GitHub issues.

## Helping

Sponsorship pays for my time and the hardware (headsets, wheels, guns,
controllers) much of this needs. Sponsors get my thanks and a say in what
comes next, never early builds or locked features.
