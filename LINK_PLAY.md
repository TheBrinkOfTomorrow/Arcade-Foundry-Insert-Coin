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
| Star Wars Arcade | `swa` | No | No |
| Virtua Fighter | `vf` | No (two players on one cabinet) | No |
| Wing War | `wingwar` | Linked cabinets for versus dogfights | Not yet |

### Sega Model 2

| Game | Set | Arcade | In the app |
| --- | --- | --- | --- |
| Daytona USA (Revision A) | `daytona` | 8 | Yes, tested with 2 in the app and 4 in the core |
| Daytona USA (1993) | `daytona93` | 8 | No: its link works differently from Revision A's, which is the one the app speaks. Use `daytona` to link |
| Sega Rally Championship | `srallyc` | 4 (two twin cabinets) | Yes, tested with 2 |
| INDY 500 | `indy500` | 8 | Yes, tested with 8 in the core, not yet in the app |
| Manx TT Superbike | `manxttc` | 8 | Yes, not tested yet |
| Motor Raid | `motoraid` | 4 | Not yet |

The other Model 2 games don't link cabinets. The fighting games, Virtua
Cop 2 and the others marked "two players" in [GAMES.md](GAMES.md) take a
second controller on the same device instead.

### Sega Model 3

| Game | Set | Arcade | In the app |
| --- | --- | --- | --- |
| Scud Race, Scud Race Plus | `scud`, `scudplus` | 8 | Not yet |
| Daytona USA 2, Power Edition | `daytona2`, `dayto2pe` | 16 | Not yet |
| Sega Rally 2 | `srally2` | 4 | Not yet |

The Model 3 cores already have Supermodel's network board, but it isn't
connected to the app's link settings yet (the core turns link play down).
Le Mans 24, Dirt Devils, Harley-Davidson & L.A. Riders, Spikeout and
Oratorio Tangram also had a network board in the arcade, and they'll come
with it. Until then every Model 3 game plays as a
single cabinet. Scud Race and Daytona USA 2 are already set to a single
cabinet the first time they start.

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
| INDY 500 | GAME ASSIGNMENTS > NETWORK TYPE and CABINET ID | MASTER on cabinet 1, SLAVE on the others, and a different CABINET ID (1 to 8) on each. Two cabinets with the same ID keep showing "Checking network now" |
| Manx TT Superbike | The game's link setting in test mode | One master, and a different bike on each of the others |

Leave test mode, and the game restarts with its new role. Each cabinet
keeps the setting in its own save, so you only do this once. When the link
is up, the cabinets show it in attract mode. Daytona USA shows 通信システム
("communication system"), and Virtua Racing and Sega Rally finish their
network check. Each driver then puts in a coin and starts, and the cabinets
that have started race together.

To play alone again, turn **Link to other cabinets** off and set the game
back to a single cabinet in test mode (Virtua Racing: LINK ID to NO LINK;
Daytona USA: the link off and a single cabinet; Sega Rally: LINK TYPE to
NOTLINK). If you leave the
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
