# Cabinet versus cabinet (VS)

Sega built many of its racing games so that several cabinets could be wired
together, each driver on their own seat and screen, all in the same race.
Arcade Foundry: Insert Coin does the same over your home network: each
device is one cabinet, and the devices race each other. In the app this is
called **Link play**.

This page explains how many cabinets each game links, which games the app
can link today, and how to set it up.

## Which games link

"Arcade" is how many cabinets (drivers) the original machines could link.
One device in the app is one cabinet seat, so a twin cabinet counts as two.
"In the app" is what works today:

- **Yes, tested**: I've raced linked cabinets in the app. "In the core"
  means more cabinets linked on one Mac with the core's own test runner,
  the same game code the app runs.
- **Yes, not tested yet**: the app offers link play, but nobody has
  tried it yet. Tell me how it goes.
- **Not yet**: the arcade game links, but its core doesn't link yet.
- **No**: the arcade game never linked cabinets.

### Sega Model 1

| Game | Set | Arcade | In the app |
| --- | --- | --- | --- |
| Virtua Racing | `vr` | 8 | Yes, tested with 2 in the app and 4 in the core |
| Virtua Formula | `vformula` | 8 | Yes, tested with 3 in the core |
| Star Wars Arcade | `swa` | No | No |
| Virtua Fighter | `vf` | No (two players on one cabinet) | No |
| Wing War | `wingwar` | Linked cabinets for versus dogfights | Not yet |

### Sega Model 2

| Game | Set | Arcade | In the app |
| --- | --- | --- | --- |
| Daytona USA (Revision A) | `daytona` | 8 | Yes, tested with 2 in the app and 4 in the core |
| Daytona USA (1993) | `daytona93` | 8 | No: its link works differently from Revision A's, which is the one the app speaks. Use `daytona` to link |
| Sega Rally Championship | `srallyc` | 4 (two twin cabinets) | Yes, tested with 3 (Game Director, 8 Nov 2026) |
| INDY 500 | `indy500` | 8 | Yes, tested with 8 in the core and 3 across devices (Game Director, 8 Nov 2026) |
| Manx TT Superbike | `manxttc` | 8 | Yes, tested with 2. Even numbers only: this is the Twin program, which pairs cabinets, so with 3 the odd one becomes a relay and the race never links |
| Sega Touring Car Championship | `stcc` | Linked (LINK TYPE CAR 1 and up) | Yes, tested with 3 in the core |
| Over Rev | `overrevb` | 4 | Yes, tested with 2 in the core |
| Cyber Troopers Virtual-On | `von` | 2 (versus) | Yes, tested with 2 in the core |
| Super GT 24h | `sgt24h` | 4 | Not yet: the cabinets link, but all of them stay on the course select |
| Motor Raid | `motoraid` | 4 | Not yet |

The other Model 2 games don't link cabinets. The fighting games, Virtua
Cop 2 and the others marked "two players" in [GAMES.md](GAMES.md) take a
second controller on the same device instead.

### Sega Model 3

| Game | Set | Arcade | In the app |
| --- | --- | --- | --- |
| Scud Race | `scud` | 8 | Yes, tested with 2 in the core |
| Scud Race Plus | `scudplus` | 8 | Yes, not tested yet |
| Daytona USA 2 | `daytona2` | 16 | Yes, tested with 3 in the core |
| Daytona USA 2 Power Edition | `dayto2pe` | 16 | Yes, not tested yet |
| Sega Rally 2 | `srally2` | 4 | Yes, tested with 2 in the core |
| Le Mans 24 | `lemans24` | 4 | Yes, tested with 3 in the core |
| Dirt Devils | `dirtdvls` | 4 | Yes, tested with 4 in the core |
| Harley-Davidson & L.A. Riders | `harley` | 4 | Yes, tested with 4 in the core |

The Model 3 cores link the way Supermodel does, over the same network
protocol, so a cabinet in the app can even race one running Supermodel. The
cabinets always wait for each other every frame, so **Frame sync** makes no
difference for these games, and a Wi-Fi ring runs at the speed of its
slowest link. Spikeout and Virtual-On Oratorio Tangram also had a network
board in the arcade. Their cores can link too, but the app doesn't offer it until each
one has been tested. Scud Race and Daytona USA 2 are set to a single cabinet
the first time they start without a link. Le Mans 24 starts in Export
(English, no warning screen) the first time it starts without a link. Its
country isn't in GAME ASSIGNMENTS: at the top of the test menu press Start,
Start, Service, Service, Start, Test, then Service picks Japan, USA, Export
or Australia.

### Classic 2D (coming after 1.0.0)

| Game | Board | Arcade | In the app |
| --- | --- | --- | --- |
| Out Run | Out Run board | No | No |
| Super Hang-On | Out Run board | No | No |
| Turbo Out Run | Out Run board | No | No |
| After Burner II | X Board | No | No |
| Thunder Blade | X Board | No | No |
| OutRunners | System Multi 32 | 8 (four twin cabinets, two drivers each) | Not yet |
| Rad Rally | System 32 | 2 | Not yet |

OutRunners' twin cabinet is one board with two screens. The core shows the
first driver's screen for now. The second screen comes next, then link play.
Rad Rally's link board is connected back to itself for now, which is how
the game runs as a single cabinet.

### Sega Lindbergh (1.0.0)

| Game | Set | Arcade | In the app |
| --- | --- | --- | --- |
| The House of the Dead 4 | `hotd4a` | No (two players on one cabinet) | No. Player 2 is a second controller, mouse or touch on the same device |
| OutRun 2 SP SDX | `outr2sdx` | 4 | Not yet. 1.0.0 plays it as a single cabinet |

## What you need

- One device for each cabinet: a Mac, iPhone, iPad, Apple TV or Linux PC.
  They can be different kinds, for example a Mac racing an Apple TV.
- The same game and the same version of the app on every device.
- All of them on the same network. Wired is best, but Wi-Fi works too.
  The cabinets swap a little data every frame, so a busy or weak Wi-Fi
  connection can slow the race down.
- Devices that are separate cabinets, not tied together by a transfer (see
  the [FAQ](FAQ.md#after-a-transfer-are-the-two-devices-tied-together)).
  Each cabinet keeps its link setting in its own save, so two tied devices
  can't link with each other.

## How the cabinets connect

The cabinets form a ring. Each one listens on a port for the cabinet before
it and connects to the cabinet after it, and the last one connects back to
the first. With two cabinets, each one's next cabinet is the other. With
four, it's 1 → 2 → 3 → 4 → 1.

The game itself decides who is who, just as in the arcade: one cabinet is
the master (car 1) and the others are slaves, each with its own car
number. You set that in the game's test mode.

## Setting it up on a Mac, iPhone, iPad or Apple TV

On each device:

1. Open the game's page (on Apple TV, a long press on its card) and find
   **Link play**.
2. Turn on **Link to other cabinets**.
3. Leave **This cabinet's port** as it is, or pick any free port.
4. Pick the next cabinet. Devices on your network running the app show up
   as **Next cabinet: \<name\>** buttons, and choosing one fills in its
   address. You can also type it as `host:port`, for example
   `192.168.1.20:15112`.
5. **Frame sync** makes every cabinet wait for the master each frame. It
   keeps the race tight on a good network, but it slows every cabinet down
   to the slowest one. Try without it first.

Then start the game on each device and set its role in test mode (next
section). On a keyboard, F2 opens test mode. Without one, open the pause
menu and choose **Operator mode**: the controller's A becomes the Test
button and B the Service button. On iPad there are also two buttons on
screen.

## Setting it up on Linux

The Linux app links from the command line. Two cabinets on two PCs:

    insert-coin --play daytona --link-port 15112 --link-next 192.168.1.21:15112
    insert-coin --play daytona --link-port 15112 --link-next 192.168.1.20:15112

Add `--link-sync` for frame sync. On one PC, give each copy its own port
and point them at each other on `127.0.0.1`. The app prints
`link up, cabinet 1 of 2` when the ring is up.

## Each game's test mode setting

| Game | Where | Set it to |
| --- | --- | --- |
| Virtua Racing | GAME SYSTEM > LINK ID | MASTER on one cabinet, SLAVE on the others. The VR buttons drive the menu: red moves, blue changes, green enters. Choose EXIT, then YES to save |
| Daytona USA | GAME SYSTEM > LINK ID and CAR NUMBER | MASTER on one cabinet, SLAVE on the others, and a different CAR NUMBER on each |
| Sega Rally Championship | GAME ASSIGNMENTS > LINK TYPE | CAR 1 on one cabinet (the master), then CAR 2, CAR 3 and CAR 4 |
| Sega Touring Car Championship | GAME ASSIGNMENTS > LINK TYPE | CAR 1 on one cabinet (the master), then CAR 2, CAR 3 and so on |
| Over Rev | GAME ASSIGNMENTS > FOUNDATIONAL SETTING > LINK MAX and LINK TYPE | LINK MAX to the number of cabinets on all, then LINK TYPE MASTER CarNo.1 on one cabinet and SLAVE CarNo.2, 3 and 4 on the others |
| Cyber Troopers Virtual-On | GAME ASSIGNMENTS > NETWORK LINK ATTRIBUTE | MASTER on one cabinet, SLAVE on the other. The boot screen says THIS IS MASTER SITE or SLAVE SITE. A cabinet that starts while the other is playing gets ENGAGE OTHER PLAYER, and the two fight |
| INDY 500 | GAME ASSIGNMENTS > NETWORK TYPE and CABINET ID | MASTER on cabinet 1, SLAVE on the others, and a different CABINET ID (1 to 8) on each. Two cabinets with the same ID keep showing "Checking network now" |
| Manx TT Superbike | GAME ASSIGNMENT > CABINET, LINK ID and BIKE COLOR/No. | CABINET TWIN on all (DELUXE hangs: that set isn't supported), MASTER on one cabinet, SLAVE on the others, and a different bike number on each. Use an even number of cabinets |
| Scud Race, Daytona USA 2 | GAME ASSIGNMENTS > LINK ID and CAR NUMBER | MASTER on one cabinet, SLAVE on the others, and a different CAR NUMBER on each. The car number is the car you drive, whatever the cabinet's place in the ring. The boot screen then says THIS IS MASTER CONTROLLER or SLAVE MACHINE |
| Le Mans 24 | GAME ASSIGNMENTS > COMMUNICATION MODE and Cabinet Number | MASTER on one cabinet, SLAVE on the others, and a different Cabinet Number (1 to 4) on each. A cabinet set to MASTER or SLAVE stays on a black screen until the link is up. The 24-hour race is the course for linked play |
| Dirt Devils | GAME ASSIGNMENTS > COMMUNICATION MODE and MACHINE ID | MASTER on one cabinet, SLAVE on the others, and a different MACHINE ID (1 to 4) on each. The game starts out as SLAVE, ID 1 |
| Harley-Davidson & L.A. Riders | NETWORK ASSIGNMENTS > COMMUNICATION, PRIVILEGE MODE and CABINET ID NUMBER | COMMUNICATION to NETWORK on all, PRIVILEGE MODE MASTER on one cabinet and SLAVE on the others, and a different CABINET ID NUMBER (1 to 4) on each. The cabinet ID picks the bike. A NETWORK cabinet waits on CHECKING NETWORK until the link is up |
| Sega Rally 2 | GAME ASSIGNMENTS > LINK TYPE | MASTER (CAR 1) on one cabinet, then SLAVE (CAR 2), (CAR 3) and (CAR 4). LINK TYPE only shows with **Link to other cabinets** on, so turn that on first. The game then checks the network and shows TOTAL NODES and each cabinet's NODE ID |

Leave test mode, and the game restarts with its new role. Each cabinet
keeps the setting in its own save, so you only do this once. When the link
is up, the cabinets show it in attract mode. Daytona USA shows 通信システム
("communication system"), and Virtua Racing and Sega Rally finish their
network check. Each driver then puts in a coin and starts, and the cabinets
that have started race together.

To play alone again, turn **Link to other cabinets** off and set the game
back to a single cabinet in test mode (Virtua Racing: LINK ID to NO LINK;
Daytona USA: the link off and a single cabinet; Sega Rally: LINK TYPE to
NOTLINK; STCC: LINK TYPE to STAND ALONE; Over Rev: LINK MAX to NOT LINK;
Virtual-On: NETWORK LINK ATTRIBUTE to NOLINK; Scud Race and Daytona USA 2: LINK ID to SINGLE; Sega Rally 2:
LINK TYPE to STAND ALONE, set while the link is still on; Le Mans 24 and
Dirt Devils: COMMUNICATION MODE to NO LINK; Harley-Davidson: COMMUNICATION
to STAND-ALONE). If you leave the
link role on with linking off, the game waits for the other cabinets and
doesn't play.

## Troubleshooting

- **The game sits on a network check or a settings screen.** It's waiting
  for the other cabinets. Check that every cabinet is running, that each
  one's next cabinet is right and the ring closes back to the first, and
  that exactly one cabinet is the master.
- **No "Next cabinet" buttons.** The app finds other cabinets only while
  it's open on them and they're on the same network. Some networks (guest
  Wi-Fi, mesh systems with isolation turned on) block this. Type the
  address instead.
- **The link is lost.** Quit the game to the library on every cabinet and
  start them again.
- **A firewall asks about incoming connections.** Allow them: each cabinet
  listens for the one before it. On iPhone and iPad, allow **Local
  Network** access for the app in Settings.
