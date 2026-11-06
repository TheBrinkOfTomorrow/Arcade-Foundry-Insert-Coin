// The Linux app's shared types: a game in the list, the screen settings and
// the command line's options. The app's parts: library (the game list, a
// game's page, the credits), session (a game being played), input (the
// keyboard, mouse and gamepads), render (drawing the game's screen and the
// pause menu); main.cpp starts it.
#pragma once

#include "afcore.h"
#include "game_list.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace insert_coin {

// The menu's background: the dark grey of Apple's Dark appearance (#1C1C1E).
// A game's own screen stays black.
constexpr SDL_Color BACKGROUND{28, 28, 30, 255};

struct Game {
    std::string romset, title, detail;
    const afcore_api *api = nullptr; // null: this build has no core for it
    fs::path rom;                     // empty: no ROM set found
    uint32_t outdated = 0;            // a core for another interface major: its version
    GameControls controls = GameControls::Drive; // how it is played (games.toml)
    bool flight = false;              // a flight stick and throttle (Star Wars Arcade, Wing War)
    bool twin = false;                // two sticks, each with a shot and a dash (Virtual-On)
    bool analog_gun = false;          // Model 3's light guns: no off-screen sense, the second trigger reloads
    bool stick_y = false;             // the left stick's forward and back is a second analog control
    bool gun_button = false;          // the gun has a button besides the trigger (HotD4's grenade, Line of Fire's missile)
    bool folder = false;              // a Lindbergh game (a folder holding disk0, or <romset>.tar) or a Chihiro one (a folder holding its XBE)
    std::string xbe;                  // a Chihiro game: its XBE, in its folder
    int platform = 2;                 // its board, an index into kPlatforms

    bool playable() const { return api && !rom.empty(); }
};

// The boards, in their release order: the platform filter's choices after
// All, and the Platform sort's order (Platform in the Apple app's
// Catalog.swift; the keys are its names for them).
struct PlatformName {
    const char *key, *name;
};

const PlatformName kPlatforms[] = {{"classic2d", "Classic 2D"}, {"model1", "Model 1"}, {"model2", "Model 2"}, {"model3", "Model 3"},
                                   {"lindbergh", "Lindbergh"}, {"chihiro", "Chihiro"}};

const int kPlatformCount = int(sizeof kPlatforms / sizeof kPlatforms[0]);

// The game list's order (LibrarySort in the Apple app's Settings.swift).
enum class Sort { title, year, platform, recent };

const char *const kSortKeys[] = {"title", "year", "platform", "recent"};

const char *const kSortNames[] = {"A-Z", "Year", "Platform", "Recently played"};

const int kSortCount = 4;

// x.y.z; a core from before 4.0.0 numbered its interface 1, 2, 3.
inline std::string version_string(uint32_t v) {
    if (v < (1u << 16)) return std::to_string(v);
    return std::to_string(AFCORE_VERSION_MAJOR(v)) + "." + std::to_string(AFCORE_VERSION_MINOR(v)) + "." +
           std::to_string(AFCORE_VERSION_PATCH(v));
}

// The screen settings (issue #24), as on the Apple devices: the picture's
// shape, effect, filtering, size and position. Every game's come from the
// command line; a game's own, chosen in its pause menu, are kept in
// <data>/screen/<romset> as "shape=crt,..." (the Apple app's format).
enum class Shape { original, stretch, integer };

enum class Effect { none, scanlines, crt };

struct Look {
    Shape shape = Shape::original;
    Effect effect = Effect::none;
    bool smooth = true;
    double zoom = 1, shift_x = 0, shift_y = 0; // 0.8...1.2; -0.1...0.1 of the window, right and down

    static const char *name(Shape s) { return s == Shape::stretch ? "stretch" : s == Shape::integer ? "integer" : "original"; }
    static const char *name(Effect e) { return e == Effect::crt ? "crt" : e == Effect::scanlines ? "scanlines" : "none"; }
    bool set(const std::string &key, const std::string &v) {
        if (key == "aspect" || key == "shape") {
            if (v == "original") shape = Shape::original;
            else if (v == "stretch") shape = Shape::stretch;
            else if (v == "integer") shape = Shape::integer;
            else return false;
        } else if (key == "effect") {
            if (v == "none") effect = Effect::none;
            else if (v == "scanlines") effect = Effect::scanlines;
            else if (v == "crt") effect = Effect::crt;
            else return false;
        } else if (key == "smooth") smooth = v != "0";
        else if (key == "zoom") zoom = std::clamp(std::atof(v.c_str()), 0.8, 1.2);
        else if (key == "x") shift_x = std::clamp(std::atof(v.c_str()), -0.1, 0.1);
        else if (key == "y") shift_y = std::clamp(std::atof(v.c_str()), -0.1, 0.1);
        else return false;
        return true;
    }
    std::string str() const {
        char b[160];
        std::snprintf(b, sizeof b, "aspect=%s,effect=%s,smooth=%d,zoom=%g,x=%g,y=%g", name(shape), name(effect), smooth ? 1 : 0,
                      zoom, shift_x, shift_y);
        return b;
    }
    void parse(const std::string &text) {
        size_t at = 0;
        while (at < text.size()) {
            size_t end = text.find_first_of(",\n", at);
            if (end == std::string::npos) end = text.size();
            const std::string pair = text.substr(at, end - at);
            if (const size_t eq = pair.find('='); eq != std::string::npos) set(pair.substr(0, eq), pair.substr(eq + 1));
            at = end + 1;
        }
    }
};

struct Options {
    Look look; // every game's screen settings
    fs::path roms, cores, data;
    fs::path links; // the ROM sets in roms, linked into one folder (link_roms)
    bool fullscreen = false, link_sync = false, hash = false;
    bool game_thread = true; // Model 1/2 and Chihiro run on a thread of their own (--no-game-thread: on the window's)
    bool speed = false; // --speed: the speed once a second on stdout, and the whole run's when the game ends
    bool show_speed = true; // the speed in the window's title, and in a corner in full screen (Settings)
    int link_port = 0;
    std::string link_next, play;
    long frames = 0; // > 0: quit after this many frames (tests)
};

} // namespace insert_coin
