# Compatibility

Which games I've played on which kinds of hardware for each release of
Arcade Foundry: Insert Coin, and how they ran.

[GAMES.md](GAMES.md) shows where each game *builds*. This page shows where
I've actually *played* it. If a game isn't listed for a chip, I haven't tried
it there yet, and it may well run fine. If you've played a game on hardware
that isn't listed, open an issue with the game, the device, the chip, the
release and what you saw.

Every Model 1, Model 2 and Model 3 game is also checked frame for frame
against MAME or Supermodel (attract mode, test mode and a full game) before
it's added.

- ✅ **Plays**: full speed, picture and sound right.
- ⚠️ **Plays with issues**: playable, see the note.
- ❌ **Doesn't play well yet**: see the note.

## Hardware

| Device | Chip |
| --- | --- |
| Mac | Apple M4 Pro |
| Mac | Apple M2 Max |
| iPad Pro | Apple M4 |
| Apple TV HD | Apple A8 |
| Apple TV 4K | Apple A12 Bionic |
| Apple TV 4K | Apple A15 Bionic |
| Linux PC | x86-64, AMD Radeon graphics |
| Raspberry Pi 5 | Broadcom BCM2712 |
| Raspberry Pi 4 | Broadcom BCM2711 |

## Controllers

| Controller | Connection | macOS | iPhone and iPad | Apple TV | Linux |
| --- | --- | --- | --- | --- | --- |
| Sony DUALSHOCK 4 Wireless Controller (PlayStation 4) | Bluetooth | ✅ | ✅ | ✅ | |
| Sony DualSense Wireless Controller (PlayStation 5) | Bluetooth | ✅ | ✅ | ✅ | |
| Microsoft Xbox Wireless Controller (Xbox One, Bluetooth model) | Bluetooth | ✅ | ✅ | ✅ | |
| Logitech G F310 Gamepad | Wired USB | ✅ | | | ✅ |
| Logitech G F710 Wireless Gamepad | 2.4 GHz USB receiver | | | | ✅ |
| Logitech Driving Force GT (wheel and pedals) | Wired USB | ✅ | | | ✅ |

## 1.0.0

### Sega Model 1

| Game | Set | Device | Chip | Result | Notes |
| --- | --- | --- | --- | --- | --- |
| Virtua Racing | `vr` | Mac | Apple M4 Pro | ✅ | |
| Virtua Racing | `vr` | Linux PC | x86-64, AMD Radeon | ✅ | |
| Virtua Racing | `vr` | Apple TV HD | Apple A8 | ✅ | |

### Sega Model 2

| Game | Set | Device | Chip | Result | Notes |
| --- | --- | --- | --- | --- | --- |
| Daytona USA | `daytona` | Linux PC | x86-64, AMD Radeon | ✅ | |
| Daytona USA | `daytona` | Apple TV 4K | Apple A12 Bionic | ✅ | |
| Daytona USA | `daytona` | Raspberry Pi 5 | BCM2712 | ✅ | Full speed. The screen rate can dip for a second now and then; the game doesn't slow |
| Daytona USA | `daytona` | Raspberry Pi 4 | BCM2711 | ❌ | About half speed in races |
| Sega Rally Championship | `srallyc` | Linux PC | x86-64, AMD Radeon | ✅ | |
| Manx TT Superbike | `manxttc` | Mac | Apple M4 Pro | ✅ | Engine and road sounds need `manxtt` beside it |
| Rail Chase 2 | `rchase2` | Linux PC | x86-64, AMD Radeon | ✅ | |

### Sega Model 3

| Game | Set | Device | Chip | Result | Notes |
| --- | --- | --- | --- | --- | --- |
| Virtua Fighter 3 | `vf3` | Mac | Apple M4 Pro | ✅ | |
| Scud Race | `scud` | Mac | Apple M4 Pro | ✅ | |
| Scud Race Plus | `scudplus` | Apple TV 4K | Apple A12 Bionic | ⚠️ | About 47 fps in races (full speed is 57.5 fps) |
| Daytona USA 2: Battle on the Edge | `daytona2` | Mac | Apple M4 Pro | ✅ | |
| Daytona USA 2: Battle on the Edge | `daytona2` | Apple TV 4K | Apple A15 Bionic | ✅ | |
| Daytona USA 2: Battle on the Edge | `daytona2` | Apple TV 4K | Apple A12 Bionic | ⚠️ | About 40 fps in races (full speed is 57.5 fps) |

### Sega Out Run board (Classic 2D)

| Game | Set | Device | Chip | Result | Notes |
| --- | --- | --- | --- | --- | --- |
| Out Run | `outrun` | Mac | Apple M4 Pro | ✅ | |
| Out Run | `outrun` | Apple TV 4K | Apple A12 Bionic | ✅ | |

### Sega Lindbergh

| Game | Set | Device | Chip | Result | Notes |
| --- | --- | --- | --- | --- | --- |
| The House of the Dead 4 | `hotd4a` | Mac | Apple M4 Pro | ✅ | |
| The House of the Dead 4 | `hotd4a` | iPad Pro | Apple M4 | ✅ | |
| The House of the Dead 4 | `hotd4a` | Linux PC | x86-64, AMD Radeon | ⚠️ | Title screen 57-59 fps on a 120 Hz display |
| OutRun 2 SP SDX | `outr2sdx` | Mac | Apple M4 Pro | ✅ | |
| OutRun 2 SP SDX | `outr2sdx` | Linux PC | x86-64, AMD Radeon | ✅ | |
