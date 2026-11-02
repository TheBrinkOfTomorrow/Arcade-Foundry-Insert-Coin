# FAQ

Questions about using Arcade Foundry: Insert Coin. For building it, see
[BUILD.md](BUILD.md).

Arcade Foundry: Insert Coin is not affiliated with or endorsed by Sega. It
contains no ROM data, and it never downloads ROM sets from anywhere except your own
iCloud Drive or your own ROM server.

## Files

### Where does the app keep its files (ROM sets, saves, pictures)?

| Platform | Folder |
| --- | --- |
| macOS | `~/Library/Containers/com.thebrinkoftomorrow.arcadefoundry.insertcoin/Data/Library/Application Support/Arcade Foundry/` |
| iPhone, iPad | The app's Application Support folder, `Arcade Foundry/` (not shown in Files) |
| Apple TV | The app's Caches folder, `Arcade Foundry/` (the system may clear it) |
| Linux | `~/.local/share/arcade-foundry/` (or `$XDG_DATA_HOME/arcade-foundry/`) |

The Mac app is sandboxed, so its Application Support folder is inside its
container, not `~/Library/Application Support`. To open it in the Finder,
press ⇧⌘G and paste the path.

Inside that folder:

- `ROMs/`: ROM sets (`<romset>.zip` or `.7z`) and unpacked Lindbergh games
  (`ROMs/hotd4a/disk0/...`). If an unpack fails, it can leave a
  `<set>.unpacking` folder behind, and the next try clears it.
- `Saves/<romset>/`: each game's saves (its settings, link role and
  records).
- `Pictures/<romset>.png`: the library pictures.

On Linux, `roms/` holds the ROM sets, `games/` the unpacked Lindbergh games,
`saves/<romset>/` the saves, `pictures/` the library pictures and
`buttons/<romset>` each game's controller buttons.

Settings (the ROM server, screen options, link play) are kept in the app's
preferences. The ROM server address is shared with your other devices
through iCloud.

### Where are the game saves?

Each game's saves (its settings, link role and records) are in
`Saves/<romset>/` inside the folder above:

| Platform | Saves | Backed up |
| --- | --- | --- |
| macOS | `.../Arcade Foundry/Saves/<romset>/` in the app's container | To your iCloud, per cabinet |
| iPhone, iPad | `Arcade Foundry/Saves/<romset>/` in the app's Application Support | To your iCloud, per cabinet |
| Apple TV | `Arcade Foundry/Saves/<romset>/` in the app's Caches | To your iCloud, per cabinet (and restored if the system clears Caches) |
| Linux | `~/.local/share/arcade-foundry/saves/<romset>/` | No: back up the folder yourself |

The iCloud backups are kept in the app's private iCloud database, not in
iCloud Drive, so they don't show up in the Finder or Files. Settings >
Other cabinets in iCloud lists them.

### Where is the iCloud Drive ROM folder?

The app has its own iCloud Drive folder, **Arcade Foundry**:

| Platform | Where to find it |
| --- | --- |
| macOS | The Finder: iCloud Drive > Arcade Foundry (`~/Library/Mobile Documents/iCloud~com~thebrinkoftomorrow~arcadefoundry~insertcoin/Documents/`) |
| iPhone, iPad | The Files app: iCloud Drive > Arcade Foundry |
| Apple TV | None: Apple TV has no iCloud Drive, so it gets ROM sets from your ROM server |
| Linux | None: put ROM sets in `~/.local/share/arcade-foundry/roms/`, or pass `--roms <folder>` (or a folder per board in it) |

Every Mac, iPhone and iPad signed in to the same Apple Account sees the
sets there. A device downloads a set the first time it plays it, and then
keeps a copy in its own `ROMs/` folder.

### Can I delete a game from a device?

Yes. Delete its ROM set (or Lindbergh game folder) from `ROMs/` in the
folder above. A copy in iCloud Drive or on your ROM server stays there, and
the device can get it again.

## ROM sets

### Where do I get ROM sets?

From your own arcade boards, dumped with MAME's naming (`<romset>.zip` or
`.7z`). [GAMES.md](GAMES.md) gives each game's set name.
The app and its cores never include ROM data.

### How do I get ROM sets onto my devices?

You have three options:

- **iCloud Drive** (Mac, iPhone, iPad): use Add ROM with the iCloud Drive
  setting on, or drop the sets into iCloud Drive > Arcade Foundry.
- **Your ROM server** (every platform, and the only option on Apple TV):
  any web server on your network that serves the sets. Set its address in
  Settings. The [README](README.md#rom-server) shows how to run one with a
  single command.
- **This device only**: use Add ROM with the iCloud Drive setting off.

### Why doesn't Apple TV get my ROM sets from iCloud Drive?

Apple TV has no iCloud Drive, so it can only download sets from your ROM
server ([README](README.md#rom-server)). Keep the server running whenever
Apple TV needs a set: the first time, and again after tvOS clears the app's
cache (it may do this at any time, and Apple TV then fetches the sets
again). If downloads fail with the server running, check that the Apple TV
is on the same network and the address in Settings is right.

### Should "Put in iCloud Drive" move or copy the set?

It copies. The set stays on this device too, so the device keeps playing it
without downloading it again.

### A download from my ROM server never finishes.

Check that the device is on the same network as the server: a phone on
another Wi-Fi network or on mobile data can't reach a server at home. Then
check the address in Settings (`http://<address>:<port>`) and that the
server is still running. A download gives up after 30 seconds with no data,
and says why.

### How do Lindbergh games travel?

A Lindbergh game is a folder (the one that holds `disk0`), not a ROM set. To
go through iCloud Drive or a ROM server, it is packed as one file,
`<set>.tar`. Add ROM with iCloud Drive on makes that file for you, and
`scripts/pack_lindbergh.py` makes one for a ROM server. A device unpacks the
file into `ROMs/<set>/` before the first play.

### Why can't I install a build with Lindbergh games on my iPhone or iPad?

Lindbergh cores on iPhone and iPad need the extended virtual addressing
entitlement. Apple grants it only to paid developer teams, so a free
(personal) team can't install the app with it. Apple TV doesn't list
Lindbergh games.

## Cabinets and saves

### What is a cabinet?

Each device is a cabinet, like a machine in an arcade. It has a name you
choose and an ID of its own. A game's saves belong to the cabinet: its
settings, its link role (car 1 or 2, master or slave) and its records.
Cabinets are part of the Apple apps (macOS, iOS, tvOS). On Linux, saves
stay on the machine.

### What is the cabinet ID?

The cabinet ID is a random identifier the app makes the first time it
runs on a device. It names the cabinet in iCloud: the backups of its saves
are filed under it. Settings > This cabinet shows the ID under the name,
and Settings > Other cabinets in iCloud shows each cabinet's ID, so two
devices with the same ID are easy to spot.

The name is only a label. The ID is what ties a device to its cabinet.

Where the ID is kept:

- **macOS**: in the app's preferences. Deleting the app's container gives
  the Mac a new ID.
- **iPhone, iPad, Apple TV**: in the device's Keychain (this device only,
  not shared through iCloud Keychain). It usually survives deleting and
  reinstalling the app.

When a device starts with a new ID and you already have cabinets in
iCloud, the app offers to continue as one of them.

### Are my saves backed up?

Yes, to your iCloud, each time you leave a game. If the device is offline
at that moment, the backup is retried when the network comes back or when
you open the app again. The app doesn't poll in the background. A device
that loses its saves (a reinstall, or Apple TV clearing its cache) gets
them back from the backup the next time it starts the game.

### I have a new device. How do I keep my old cabinet's saves?

On the new device, go to Settings > Other cabinets in iCloud, choose the old
cabinet, and pick **Transfer its name and game saves to this device**. To
take only one game's save, use **Copy its <game> save to this cabinet**
instead.

### After a transfer, are the two devices tied together?

Yes, if both are still in use. A transfer gives the new device the old
cabinet's ID, and the ID is what ties them. Renaming one of them doesn't
separate them. Devices that share a cabinet take turns on one save history:
before a game starts, each device picks up the newest backup, so you carry
on from where you played last on any of them.

A few things to know:

- Saves are never merged. If two tied devices play the same game at once,
  or both play while offline, the session played later is kept.
- Session times come from each device's clock, so keep the clocks set
  automatically.
- Tied devices share the link role in each game's save, so they can't link
  with each other in that game.

### How do I separate two tied devices?

On one of them, go to Settings > This cabinet and choose **Make this a new
cabinet**. That device gets a new ID, keeps its name and saves, and backs
them up under the new cabinet. The other device keeps the old cabinet and
its backups. Give the new cabinet a name of its own too.

### Can I delete an old cabinet from iCloud?

Yes. Settings > Other cabinets in iCloud lists each one with a Delete
button. That removes the cabinet and its backups from iCloud, not the saves
on any device.

## Link play

### How do I link cabinets?

Open a game's page and turn on **Link to other cabinets**. Cabinets on the
same network find each other, or you can type the next cabinet's address.
Then set each cabinet's link role in the game's test mode. On a keyboard
that's F2. Without a keyboard, use Operator mode in the pause menu, where
the controller's A is the Test button and B is Service.
[LINK_PLAY.md](LINK_PLAY.md) lists which games link and how many cabinets
each takes, with the test mode setting for each game.
