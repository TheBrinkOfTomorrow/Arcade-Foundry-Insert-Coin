#!/usr/bin/env python3
"""Write the app's game lists from games.toml and the cores' games.toml.

  gen_games.py          rewrite them
  gen_games.py --check  only compare (CI): exit 1 when one is out of date

The cores' games.toml (cores/*/games.toml, the submodules) have the facts
about each game; the app's games.toml has the sets the app lists, in the
library's order, and the words it shows. From them this writes:

  kit/Sources/Library/GameList.swift       the catalog (GameInfo.catalog) and the set names (in the kit submodule:
                                           commit it there, then move the submodule here)
  linux/game_list.h                        the Linux app's table
  scripts/game_sets.py                     build_cores.py's and rom_index.py's lists
  GAMES.md                                 the tables between its GAMES:BEGIN/END markers

A new game: its [[game]] in the core's games.toml (in the core repo), the
submodule moved to that commit, its [[game]] here, then this script.
"""

import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CORES = os.path.join(ROOT, "cores")
sys.path.insert(0, os.path.join(CORES, "arcade-foundry-cores-common", "tools", "games"))
import afgames  # noqa: E402

# The core repos the app embeds (cores/arcade-foundry-cores-<name>)
CORE_REPOS = ("model12", "model3", "classic2d", "lindbergh", "chihiro")
# The library's platforms, in their order (Platform in Catalog.swift)
PLATFORMS = ["classic2d", "model1", "model2", "model3", "chihiro", "lindbergh", "pc"]
# The boards whose name the detail line shows (the others are the platform's name)
NAMED_BOARDS = {"hangon", "outrun", "xboard", "yboard", "system32", "model2a", "model2b", "model2c"}
# The face buttons in a hint: Apple's names (GCController) and the Linux app's (SDL's)
BUTTONS = {
    "apple": {"{W}": "X", "{S}": "A", "{E}": "B", "{N}": "Y"},
    "linux": {"{W}": "West", "{S}": "South", "{E}": "East", "{N}": "North"},
}
# What the controls class adds to the detail line
CONTROL_WORDS = {
    "apple": {"gun": "light gun: mouse or touch", "analog-gun": "light gun: mouse or touch",
              "turret": "gun turret: mouse or touch", "twin": "both sticks of the controller"},
    "linux": {"gun": "light gun: the mouse or a gamepad", "analog-gun": "light gun: the mouse or a gamepad",
              "turret": "gun turret: the mouse or a gamepad", "twin": "twin sticks: both sticks of the gamepad"},
}
SECOND_PLAYER = {"apple": "two players: a second controller", "linux": "two players: a second gamepad"}
GENERATED = "Written by scripts/gen_games.py from games.toml (the app's and the cores'): edit those, not this."


def load():
    cores = {}
    for name in CORE_REPOS:
        path = os.path.join(CORES, f"arcade-foundry-cores-{name}", "games.toml")
        if not os.path.exists(path):
            raise afgames.ManifestError(f"{path} is missing: git submodule update --init")
        cores[name] = afgames.load_core(path)
    games = afgames.load_app(os.path.join(ROOT, "games.toml"), cores)
    for g in games:
        if g["platform"] not in PLATFORMS:
            raise afgames.ManifestError(f"{g['set']}: the app has no {g['platform']} platform yet")
    return cores, games


def fill(text, device):
    for k, v in BUTTONS[device].items():
        text = text.replace(k, v)
    if "{" in text:
        raise afgames.ManifestError(f"unknown placeholder in {text!r} (the buttons are {', '.join(BUTTONS[device])})")
    return text


def detail(g, device):
    """The line under the title: year, version, then what to know about
    playing it ("1995, Revision A (Model 2B; gun turret: mouse or touch)")."""
    notes = []
    if g["board"] in NAMED_BOARDS:
        notes.append(g["board_name"])
    if g["controls"] in CONTROL_WORDS[device]:
        notes.append(CONTROL_WORDS[device][g["controls"]])
    hint = g["hint"] if device == "apple" else g["linux_hint"]
    if hint:
        notes.append(fill(hint, device))
    if g["players"] > 1:
        notes.append(SECOND_PLAYER[device])
    if g["link"]:
        notes.append("link play")
    if g["platform"] == "lindbergh" and device == "apple":
        notes.append("the game folder that holds disk0")
    if g["platform"] == "chihiro" and device == "apple":
        notes.append(f"the game folder that holds {g['build']['xbe']}")
    text = str(g["year"]) + (f", {g['version']}" if g["version"] else "")
    return text + (f" ({'; '.join(notes)})" if notes else "")


def other_names(games):
    """file name -> the app's set: MAME parents and older dumps that may
    hold a set, and the app's earlier names for it."""
    out = {}
    for g in games:
        for n in g["other_names"] + g["renamed_from"]:
            out[n] = g["set"]
    return out


def swift(games):
    q = afgames.swift_string
    controls = {"drive": ".drive", "flight": ".flight", "twin": ".twin", "gun": ".gun", "turret": ".turret",
                "analog-gun": ".analogGun", "fighting": ".fighting", "fishing": ".fishing"}
    lines = ["// " + GENERATED, "", "extension GameInfo {",
             "    /// Every set the app knows, in the library's order (a card's versions in this order).",
             "    static let catalog: [GameInfo] = ["]
    for g in games:
        args = [f"id: {q(g['set'])}", f"title: {q(g['title'])}", f"detail: {q(detail(g, 'apple'))}",
                f"platform: .{g['platform']}", f"controls: {controls[g['controls']]}"]
        if g["link"]:
            args.append("link: true")
        if g["players"] > 1:
            args.append(f"players: {g['players']}")
        if g["stick_y"] or g["controls"] == "fishing":
            args.append("stickY: true")
        if g["gun_button"]:
            args.append("gunButton: true")
        if g["category"] != afgames.CATEGORY_OF_CONTROLS.get(g["controls"], "other"):
            args.append(f"categoryOverride: .{g['category']}")  # GameInfo.category gives the rest from the controls
        lines.append(f"        GameInfo({', '.join(args)}),")
    d = afgames.swift_dict
    lines += ["    ]", "}", "", "enum GameNames {",
              "    /// Left out of this release (games.toml's hidden): their cores, controls and saves stay.",
              "    static let hidden: Set<String> = [" + ", ".join(q(g["set"]) for g in games if g["hidden"]) + "]",
              "    /// Other names a set's files may have: MAME's parents and older dumps that hold it,",
              "    /// and the app's earlier names for it (file name -> set).",
              "    static let otherNames: [String: String] = " + d([(q(k), q(v)) for k, v in sorted(other_names(games).items())]),
              "    /// The app's earlier names for a set (saves and settings move to the new name once).",
              "    static let renamed: [String: String] = " + d([(q(n), q(g["set"])) for g in games for n in g["renamed_from"]]),
              "    /// MAME's device sets a game needs beside it (Wing War's I/O board).",
              "    static let devices: [String: [String]] = "
              + d([(q(g["set"]), "[" + ", ".join(q(x) for x in g["devices"]) + "]") for g in games if g["devices"]]),
              "    /// A Chihiro game's XBE, in its folder (set -> file).",
              "    static let chihiroXbe: [String: String] = "
              + d([(q(g["set"]), q(g["build"]["xbe"])) for g in games if g["platform"] == "chihiro"]),
              "    /// A PC-based game's program, a path in its game folder (set -> path; PCGames).",
              "    static let pcPrograms: [String: String] = "
              + d([(q(g["set"]), q(g["program"])) for g in games if g["platform"] in afgames.FOLDER_PLATFORMS]),
              "}", ""]
    return "\n".join(lines)


def linux(games):
    c = afgames.c_string
    controls = {"drive": "Drive", "flight": "Flight", "twin": "Twin", "gun": "Gun", "turret": "Turret",
                "analog-gun": "AnalogGun", "fighting": "Fighting", "fishing": "Fishing"}
    lines = ["// " + GENERATED, "#pragma once", "", "#include <cstdint>", "",
             "// How a game is played: picks the controller layout and the controls list.",
             "enum class GameControls : uint8_t { " + ", ".join(dict.fromkeys(controls.values())) + " };", "",
             "struct GameListEntry {",
             "    const char *set, *title, *detail;",
             "    int platform; // an index into kPlatforms",
             "    GameControls controls;",
             "    int players;",
             "    bool link, stick_y, gun_button, hidden;",
             "};", "",
             "// Every set the app knows, in the library's order (a card's versions in this order).",
             "inline constexpr GameListEntry kGameList[] = {"]
    for g in games:
        flags = ", ".join("true" if g[k] else "false" for k in ("link",))
        stick = "true" if g["stick_y"] or g["controls"] == "fishing" else "false"
        lines.append(f"    {{{c(g['set'])}, {c(g['title'])}, {c(detail(g, 'linux'))}, {PLATFORMS.index(g['platform'])}, "
                     f"GameControls::{controls[g['controls']]}, {g['players']}, {flags}, {stick}, "
                     f"{'true' if g['gun_button'] else 'false'}, {'true' if g['hidden'] else 'false'}}},")
    lines += ["};", "",
              "// Other names a set's files may have (file name, set): MAME's parents and older",
              "// dumps that hold it, and the app's earlier names for it.",
              "inline constexpr const char *kOtherNames[][2] = {"]
    lines += [f"    {{{c(k)}, {c(v)}}}," for k, v in sorted(other_names(games).items())]
    lines += ["};", "", "// The app's earlier names for a set (old, new): saves and settings move once.",
              "inline constexpr const char *kRenamedSets[][2] = {"]
    lines += [f"    {{{c(n)}, {c(g['set'])}}}," for g in games for n in g["renamed_from"]]
    lines += ["};", "", "// A Chihiro game's XBE, in its folder (set, file).",
              "inline constexpr const char *kChihiroXbe[][2] = {"]
    lines += [f"    {{{c(g['set'])}, {c(g['build']['xbe'])}}}," for g in games if g["platform"] == "chihiro"]
    lines += ["};", "", "// A PC-based game's program, a path in its game folder (set, path); ends with nulls.",
              "inline constexpr const char *kPcPrograms[][2] = {"]
    lines += [f"    {{{c(g['set'])}, {c(g['program'])}}}," for g in games if g["platform"] in afgames.FOLDER_PLATFORMS]
    lines += ["    {nullptr, nullptr},", "};", ""]
    return "\n".join(lines)


def boards_by_age(games):
    """The boards the listed sets are on, oldest first: by their first game's
    year, then in afgames.BOARDS's order."""
    first = {}
    for g in games:
        first[g["board"]] = min(first.get(g["board"], g["year"]), g["year"])
    order = list(afgames.BOARDS)
    return sorted(first, key=lambda b: (first[b], order.index(b)))


def python(cores, games):
    listed = [g["set"] for g in games]

    def sets(core, pred=lambda g: True):
        return [g["set"] for g in cores[core] if g["set"] in listed and pred(g)]

    lb = {g["set"]: (g["build"]["program"], g["build"].get("test_program")) for g in cores["lindbergh"] if g["set"] in listed}
    ch = {g["set"]: g["build"]["xbe"] for g in cores["chihiro"] if g["set"] in listed}
    lines = ['"""The app\'s set lists. ' + GENERATED + '"""', "",
             "# Every set the app lists, in its order (build_cores.py --sets)",
             *afgames.py_list("SETS = ", listed),
             "# Sets left out of this release (games.toml's hidden): built only when named",
             # an empty "{}" would be a dict, so nothing hidden writes set()
             *(afgames.py_list("HIDDEN = ", hidden, brackets="{}") if (hidden := [g["set"] for g in games if g["hidden"]])
               else ["HIDDEN = set()"]),
             "# Model 1 sets: the recompiler's Model 1 build (CMake M1_CORE, target m1core)",
             *afgames.py_list("MODEL1 = ", sets("model12", lambda g: g["board"] == "model1"), brackets="{}"),
             "# Model 3 sets: Arcade-Foundry-Cores-Model3 (CMake M3CORE_ROMSET)",
             *afgames.py_list("MODEL3 = ", sets("model3"), brackets="{}"),
             "# Classic 2D sets: Arcade-Foundry-Cores-Classic2D (scripts/recompile.py, CMake C2D_ROMSET)",
             *afgames.py_list("CLASSIC2D = ", sets("classic2d"), brackets="{}"),
             "# Lindbergh sets: Arcade-Foundry-Cores-Lindbergh (CMake LB_CORE); the game's program and,",
             "# when it is a program of its own, the test menu's, under the game folder",
             "LINDBERGH = {",
             *[f"    {k!r}: {v!r}," for k, v in lb.items()],
             "}",
             "# Chihiro sets: Arcade-Foundry-Cores-Chihiro (CMake CH_CORE); the game's XBE in the game folder",
             "CHIHIRO = {",
             *[f"    {k!r}: {v!r}," for k, v in ch.items()],
             "}",
             "# Each set's year, title and board (build_cores.py --sets list, --boards)",
             "GAMES = {",
             *[f"    {g['set']!r}: ({g['year']!r}, {g['doc_title']!r}, {g['board']!r})," for g in games],
             "}",
             "# The boards with a listed set, oldest first (its first game's year): the",
             "# app's platform (Platform in Catalog.swift) and the board's name",
             "BOARDS = {",
             *[f"    {b!r}: {afgames.BOARDS[b]!r}," for b in boards_by_age(games)],
             "}",
             "# The other names a set's files may have: MAME's parents and older dumps that",
             "# hold it, and the app's earlier names for it",
             f"OTHER_NAMES = {dict((g['set'], tuple(g['other_names'] + g['renamed_from'])) for g in games if g['other_names'] or g['renamed_from'])!r}",
             ""]
    return "\n".join(lines)


def games_md(games):
    """GAMES.md's tables, one per board, by year."""
    tables = {}
    for g in games:
        tables.setdefault(g["board"], []).append(g)
    out = {}
    for board, rows in tables.items():
        lines = ["| Game | Year | Set | ROM notes | macOS | iOS | tvOS | Linux |", "| --- | --- | --- | --- | --- | --- | --- | --- |"]
        for g in sorted(rows, key=lambda g: (g["year"], g["doc_title"].removeprefix("The "))):
            first = g["version"]
            if g["link"]:
                first = f"{first}, link play" if first else "Link play"
            notes = [first, g["rom_notes"]]
            if g["platform"] == "lindbergh":
                names = " or ".join(f"`{n}`" for n in g["renamed_from"])
                notes.append("The game folder that holds `disk0`" + (f" (the older name {names} also works)" if names else ""))
            if g["platform"] == "chihiro":
                notes.append(f"The game folder that holds `{g['build']['xbe']}`")
            hint = fill(g["hint"], "apple")
            notes.append(hint[:1].upper() + hint[1:])
            notes.append(g["doc_note"])
            note = ". ".join(n for n in notes if n)
            status = {"macos": "Yes", "ios": "Yes", "tvos": "Yes", "linux": "Yes"}
            if g["platform"] == "lindbergh":
                status.update(ios="Paid developer account", tvos="No")
            if g["platform"] == "chihiro":
                status.update(ios="Not yet", tvos="No")
            if g["hidden"]:
                status = {k: (v if v == "No" else "Not in this release") for k, v in status.items()}
            status.update(g["doc_status"])
            cells = [g["doc_title"], str(g["year"]), f"`{g['set']}`", note] + [status[k] for k in ("macos", "ios", "tvos", "linux")]
            lines.append(("| " + " | ".join(cells) + " |").replace("|  |", "| |"))
        out[board] = lines
    return out


def generate(out):
    cores, games = load()
    out.write("kit/Sources/Library/GameList.swift", swift(games))
    out.write("linux/game_list.h", linux(games))
    out.write("scripts/game_sets.py", python(cores, games))
    for board, lines in games_md(games).items():
        out.region("GAMES.md", board, lines, "<!--")


if __name__ == "__main__":
    afgames.main(generate, ROOT, __doc__)
