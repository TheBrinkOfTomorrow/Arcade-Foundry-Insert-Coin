#!/bin/sh
# Gameplay videos for YouTube from the Mac app (issue #145): runs the built
# app with --video, so it makes the videos with no window and quits.
#
#   scripts/make_videos.sh daytona srallyc          attract mode, 2 minutes each
#   scripts/make_videos.sh all --seconds 90         every game with a core and ROM set
#   scripts/make_videos.sh replays                  every replay in iCloud Drive's Replays folder without a video yet
#   scripts/make_videos.sh ~/Downloads/race.afreplay --lines 1080
#
# Options (after the games): --seconds N, --skip N, --lines 1080|2160,
# --hevc, --no-caption, --no-logo, --no-end-card, --no-sound,
# --music FILE, --game-volume N, --intro FILE, --fresh-saves, --out DIR (inside
# ~/Movies: the app is sandboxed). Videos go to ~/Movies/Arcade Foundry.
#
# The app: $APP, else /Applications, else the newest Debug or Release build
# in Xcode's DerivedData. It must be built with the cores (and signed, for
# iCloud Drive's replays and ROM sets).
set -e
name="Arcade Foundry Insert Coin"
if [ -z "$APP" ]; then
    if [ -d "/Applications/$name.app" ]; then
        APP="/Applications/$name.app"
    else
        APP=$(ls -td "$HOME"/Library/Developer/Xcode/DerivedData/InsertCoin-*/Build/Products/*/"$name.app" 2>/dev/null | head -n 1)
    fi
fi
if [ -z "$APP" ] || [ ! -x "$APP/Contents/MacOS/$name" ]; then
    echo "No $name.app found: build InsertCoin-macOS in Xcode, or set APP=/path/to/$name.app" >&2
    exit 1
fi
[ $# -gt 0 ] || { sed -n '2,15p' "$0" | sed 's/^# \{0,1\}//' >&2; exit 1; }
exec "$APP/Contents/MacOS/$name" --video "$@"
