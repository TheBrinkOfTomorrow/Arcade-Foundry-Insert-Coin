// Arcade Foundry: Insert Coin for Linux: the same games as the Apple app, through SDL3. A
// list of the games this build has cores for (build/cores/linux), then the
// game in the window with its sound, keyboard and gamepad. The cores are
// opened at run time (dlopen), one shared library per game with the C
// interface in cores/arcade-foundry-cores-model12/include/afcore.h.
//
//   insert-coin [--roms DIR] [--cores DIR] [--fullscreen]
//                [--shape original|stretch|integer] [--effect none|scanlines|crt]
//                [--sharp] [--size PERCENT] [--move X,Y]
//                [--link-port PORT --link-next HOST:PORT [--link-sync]]
//                [--play ROMSET] [--frames N --screenshot-hash] [--speed] [--no-game-thread] [--version] [--credits]
//
// ROM sets: DIR/<romset>.zip or .7z (default ~/.local/share/arcade-foundry/roms),
// at its top or in a folder per board (DIR/Model2/srallyc.zip);
// a Lindbergh game: DIR/<romset>/ (the folder that holds disk0) or
// DIR/<romset>.tar, unpacked into ~/.local/share/arcade-foundry/games/ when first played.
// Saved settings: ~/.local/share/arcade-foundry/saves/<romset>/.
#include "afcore.h"
#include "credits.h"
#include "history.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <dlfcn.h>
#include <sys/stat.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <clocale>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

// The menu's background: the dark grey of Apple's Dark appearance (#1C1C1E).
// A game's own screen stays black.
constexpr SDL_Color BACKGROUND{28, 28, 30, 255};

struct Game {
    std::string romset, title, detail;
    const afcore_api *api = nullptr; // null: this build has no core for it
    fs::path rom;                     // empty: no ROM set found
    uint32_t outdated = 0;            // a core for another interface major: its version
    bool flight = false;              // a flight stick and throttle (Star Wars Arcade, Wing War)
    bool twin = false;                // two sticks, each with a shot and a dash (Virtual-On)
    bool analog_gun = false;          // Model 3's light guns: no off-screen sense, the second trigger reloads
    bool folder = false;              // a Lindbergh game: a folder holding disk0, or <romset>.tar
    int platform = 2;                 // its board, an index into kPlatforms (set from the detail)

    bool playable() const { return api && !rom.empty(); }
};

// The boards, in their release order: the platform filter's choices after
// All, and the Platform sort's order (Platform in the Apple app's
// Catalog.swift; the keys are its names for them).
struct PlatformName {
    const char *key, *name;
};
const PlatformName kPlatforms[] = {{"classic2d", "Classic 2D"}, {"model1", "Model 1"}, {"model2", "Model 2"}, {"model3", "Model 3"},
                                   {"lindbergh", "Lindbergh"}};
const int kPlatformCount = 5;

// The game list's order (LibrarySort in the Apple app's Settings.swift).
enum class Sort { title, year, platform, recent };
const char *const kSortKeys[] = {"title", "year", "platform", "recent"};
const char *const kSortNames[] = {"A-Z", "Year", "Platform", "Recently played"};
const int kSortCount = 4;

// x.y.z; a core from before 4.0.0 numbered its interface 1, 2, 3.
std::string version_string(uint32_t v) {
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

// Where the picture went in the last draw_screen, in render coordinates
// (the mouse aims across it).
SDL_FRect g_picture{0, 0, 0, 0};

// The picture's place in an ow x oh output for a w x h screen: 4:3 for
// 496 x 384, a widened screen keeping that pixel shape, and a Lindbergh
// screen (square_pixels) at its own shape; then the look's shape, size and
// position.
SDL_FRect screen_rect(int ow, int oh, int w, int h, const Look &look, bool square_pixels = false) {
    // the classic 2D boards' narrower screens (Out Run's 320 x 224): 4:3
    const double shape = square_pixels ? double(w) / h : w < 496 ? 4.0 / 3.0 : 4.0 / 3.0 * (w / 496.0);
    double dw = ow, dh = ow / shape;
    if (dh > oh) { dh = oh; dw = oh * shape; }
    if (look.shape == Shape::stretch) { dw = ow; dh = oh; }
    if (look.shape == Shape::integer) {
        const double n = std::floor(std::min(double(oh) / h, ow / (h * shape)));
        if (n >= 1) { dh = n * h; dw = dh * shape; }
    }
    dw *= look.zoom;
    dh *= look.zoom;
    const double cx = ow * (0.5 + look.shift_x), cy = oh * (0.5 + look.shift_y);
    return SDL_FRect{float(cx - dw / 2), float(cy - dh / 2), float(dw), float(dh)};
}

struct Options {
    Look look; // every game's screen settings
    fs::path roms, cores, data;
    fs::path links; // the ROM sets in roms, linked into one folder (link_roms)
    bool fullscreen = false, link_sync = false, hash = false;
    bool game_thread = true; // Model 1/2 run on a thread of their own (--no-game-thread: on the window's)
    bool speed = false; // --speed: the speed once a second on stdout, and the whole run's when the game ends
    int link_port = 0;
    std::string link_next, play;
    long frames = 0; // > 0: quit after this many frames (tests)
};

fs::path data_dir() {
    fs::path base;
    if (const char *x = std::getenv("XDG_DATA_HOME"); x && *x) base = fs::path(x);
    else if (const char *h = std::getenv("HOME"); h && *h) base = fs::path(h) / ".local/share";
    else return fs::current_path() / "arcade-foundry-data";
    // The folder was "attract-mode" before the app was renamed: carry its
    // ROM sets and saves over once.
    std::error_code ec;
    if (fs::exists(base / "attract-mode", ec) && !fs::exists(base / "arcade-foundry", ec))
        fs::rename(base / "attract-mode", base / "arcade-foundry", ec);
    return base / "arcade-foundry";
}

// The app's set and the other names its files may have (as
// scripts/rom_index.py's OTHER_NAMES): MAME's parents holding a clone's whole
// set (hotd.7z for hotdo, doa.zip for doab), and the app's names for the Lindbergh games before it took MAME's (or2sp for
// outr2sdx, lgj for letsgoju, hotd4 for hotd4a).
std::vector<std::string> set_names(const std::string &set) {
    static const std::map<std::string, std::vector<std::string>> other = {
        {"hotdo", {"hotd"}}, {"doab", {"doa"}}, {"outr2sdx", {"or2sp"}}, {"letsgoju", {"lgj"}}, {"hotd4a", {"hotd4"}}};
    std::vector<std::string> all{set};
    if (auto it = other.find(set); it != other.end()) all.insert(all.end(), it->second.begin(), it->second.end());
    return all;
}

// The ROM sets in --roms, at its top or one folder down in a folder per
// board (DIR/Model2/srallyc.zip, DIR/Lindbergh/hotd4a.tar), found as
// scripts/rom_index.py finds them: <name>.zip, .7z or .tar, or a <name>/
// folder holding disk0; work folders skipped (working, temp, to-file...,
// names starting with _ or .); a name in two places: the copy at the top,
// then the first by path, with a warning. They are linked into one flat
// folder, dst (made again at each start), where each game's core also finds
// its parent and device sets beside it. In a board folder, a <name>/ folder
// holding one archive (RingEdge/initd8/<anything>.7z) is that archive under
// the folder's name (initd8.7z).
fs::path link_roms(const fs::path &roms, const fs::path &dst) {
    std::error_code ec;
    fs::create_directories(dst, ec);
    for (fs::directory_iterator it(dst, ec), end; !ec && it != end; it.increment(ec))
        if (it->is_symlink(ec)) fs::remove(it->path(), ec);
    std::map<std::string, std::vector<std::pair<int, fs::path>>> found;
    std::function<void(const fs::path &, int)> walk = [&](const fs::path &dir, int depth) {
        std::vector<fs::path> entries;
        std::error_code e;
        for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, e), end; !e && it != end; it.increment(e))
            entries.push_back(it->path());
        std::sort(entries.begin(), entries.end());
        for (const fs::path &p : entries) {
            const std::string name = p.filename().string();
            if (name.empty() || name[0] == '.') continue;
            if (fs::is_directory(p, e)) {
                std::string n = name;
                for (char &c : n) c = char(std::tolower((unsigned char)c));
                const bool work = n == "working" || n == "temp" || n == "__macosx" || n.rfind("to-file", 0) == 0 || n[0] == '_';
                if (work) continue;
                if (fs::is_directory(p / "disk0", e)) found[name].push_back({depth, p});
                else if (depth < 1 && !fs::is_symlink(p, e)) walk(p, depth + 1);
                else if (depth == 1) { // a game's own folder: its one archive
                    std::vector<fs::path> inside;
                    for (fs::directory_iterator it(p, e), end; !e && it != end; it.increment(e)) {
                        const std::string f = it->path().filename().string();
                        std::string x = it->path().extension().string();
                        for (char &c : x) c = char(std::tolower((unsigned char)c));
                        if (!f.empty() && f[0] != '.' && (x == ".zip" || x == ".7z" || x == ".tar")) inside.push_back(it->path());
                    }
                    if (inside.size() == 1) {
                        std::string x = inside[0].extension().string();
                        for (char &c : x) c = char(std::tolower((unsigned char)c));
                        found[name + x].push_back({depth, inside[0]});
                    } else if (inside.size() > 1) {
                        std::fprintf(stderr, "insert-coin: %s holds more than one archive; skipped\n", p.c_str());
                    }
                }
                continue;
            }
            std::string ext = p.extension().string();
            for (char &c : ext) c = char(std::tolower((unsigned char)c));
            if (ext == ".zip" || ext == ".7z" || ext == ".tar") found[name].push_back({depth, p});
        }
    };
    if (fs::is_directory(roms, ec)) walk(roms, 0);
    for (auto &[name, copies] : found) {
        std::sort(copies.begin(), copies.end());
        fs::create_symlink(copies[0].second, dst / name, ec);
        if (copies.size() > 1) {
            std::string others;
            for (size_t i = 1; i < copies.size(); ++i)
                others += (i > 1 ? ", " : "") + fs::relative(copies[i].second, roms, ec).string();
            std::fprintf(stderr, "insert-coin: %s is in more than one place in %s; using %s (not %s)\n", name.c_str(),
                         roms.c_str(), fs::relative(copies[0].second, roms, ec).c_str(), others.c_str());
        }
    }
    return dst;
}

// The Lindbergh games took MAME's set names (2026-10-28): their saves, game
// folders, screen settings, buttons and pictures under the old names move to
// the new ones, once. ROM files under the old names are still found
// (set_names).
void migrate_renamed_sets(const fs::path &data) {
    static const char *const kRenamed[][2] = {{"hotd4", "hotd4a"}, {"lgj", "letsgoju"}, {"or2sp", "outr2sdx"}};
    std::error_code ec;
    for (const auto &r : kRenamed) {
        const std::string old_set = r[0], new_set = r[1];
        for (const char *dir : {"saves", "games", "buttons", "screen"})
            if (fs::exists(data / dir / old_set, ec) && !fs::exists(data / dir / new_set, ec))
                fs::rename(data / dir / old_set, data / dir / new_set, ec);
        for (const auto &[dir, ext] : {std::pair{"games", ".from"}, std::pair{"pictures", ".bmp"}})
            if (fs::exists(data / dir / (old_set + ext), ec) && !fs::exists(data / dir / (new_set + ext), ec))
                fs::rename(data / dir / (old_set + ext), data / dir / (new_set + ext), ec);
    }
}

fs::path exe_dir() {
    std::error_code ec;
    fs::path p = fs::read_symlink("/proc/self/exe", ec);
    return ec ? fs::current_path() : p.parent_path();
}

// A core of this interface's major version, or null (*outdated: the version
// of one for another major).
const afcore_api *load_core(const fs::path &dir, const std::string &romset, uint32_t *outdated) {
    const fs::path lib = dir / ("libafcore_" + romset + ".so");
    void *h = dlopen(lib.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!h) return nullptr;
    using Entry = const afcore_api *(*)();
    auto entry = reinterpret_cast<Entry>(dlsym(h, ("afcore_api_" + romset).c_str()));
    const afcore_api *api = entry ? entry() : nullptr;
    if (!api || AFCORE_VERSION_MAJOR(api->version) != AFCORE_API_MAJOR) {
        if (api) *outdated = api->version; // before 4.0.0 only this first field is there
        dlclose(h);
        return nullptr;
    }
    return api; // kept open for the program's life
}

std::vector<Game> find_games(const Options &o) {
    std::vector<Game> games = {
        {"daytona93", "Daytona USA", "1993", nullptr, {}},
        {"daytona", "Daytona USA", "1994, Revision A (link play)", nullptr, {}},
        {"srallyc", "Sega Rally Championship", "1995, Revision C (link play)", nullptr, {}},
        {"manxttc", "Manx TT Superbike", "1995, Revision C (Twin)", nullptr, {}},
        {"motoraid", "Motor Raid", "1997, Twin (Model 2A; West punch, South kick)", nullptr, {}},
        {"doab", "Dead or Alive", "1996, Revision B (Model 2B; West punch, South kick, East hold; two players: a second gamepad)", nullptr, {}},
        {"lastbrnx", "Last Bronx", "1996, Export, Revision A (Model 2B; West punch, South kick, East guard; two players: a second gamepad)", nullptr, {}},
        {"schamp", "Sonic Championship", "1996, USA (Model 2B; West punch, South kick, East barrier; two players: a second gamepad)", nullptr, {}},
        {"dynabb97", "Dynamite Baseball 97", "1997, Revision A (Model 2B; West and South the buttons, East swings the bat; two players: a second gamepad)", nullptr, {}},
        {"waverunr", "Wave Runner", "1996, Japan, Revision A (Model 2C; the right trigger is the throttle lever)", nullptr, {}},
        {"bel", "Behind Enemy Lines", "1997 (Model 2C; gun turret: the mouse or a gamepad, West the missile)", nullptr, {}},
        {"topskatr", "Top Skater", "1997, Export, Revision A (Model 2C; the left stick curves and slides, South and East jump, the shoulders select)", nullptr, {}},
        {"skisuprg", "Sega Ski Super G", "1996 (Model 2C; South, East and West select, the shoulders zoom, the stick swings)", nullptr, {}},
        {"segawski", "Sega Water Ski", "1997, Japan, Revision A (Model 2C; Start is set, West and East the pitch buttons, the d-pad selects)", nullptr, {}},
        {"desert", "Desert Tank", "1994 (Model 2; right shoulder machine gun, left shoulder cannon, South/East/West VR1-3, North shift)", nullptr, {}},
        {"skytargt", "Sky Target", "1995 (Model 2A; the left stick flies, South machine gun, East missile, West view change)", nullptr, {}, 0, true},
        {"dynamcop", "Dynamite Cop", "1998, Export (Model 2A; West punch, South kick, East jump; two players: a second gamepad)", nullptr, {}},
        {"vcop2", "Virtua Cop 2", "1995 (light guns: the mouse or a gamepad)", nullptr, {}},
        {"vcop", "Virtua Cop", "1994, Revision B (light guns: the mouse or a gamepad)", nullptr, {}},
        {"vf2", "Virtua Fighter 2", "1994, Version 2.1 (two players: a second gamepad)", nullptr, {}},
        {"indy500", "INDY 500", "1995, Twin, Revision A (Model 2B)", nullptr, {}},
        {"fvipers", "Fighting Vipers", "1995, Revision D (two players: a second gamepad)", nullptr, {}},
        {"sgt24h", "Super GT 24h", "1996, Jaleco (Model 2B)", nullptr, {}},
        {"overrevb", "Over Rev", "1997, Jaleco (Model 2B)", nullptr, {}},
        {"stcc", "Sega Touring Car Championship", "1996, Model 2C", nullptr, {}},
        {"vstriker", "Virtua Striker", "1994, Revision A (Model 2B; West long pass, South shoot, East short pass; two players: a second gamepad)", nullptr, {}},
        {"zerogun", "Zero Gunner", "1997, Export (Model 2B; West shot, South special; two players: a second gamepad)", nullptr, {}},
        {"pltkids", "Pilot Kids", "1998, Revision A (Model 2B; West shot, South special; two players: a second gamepad)", nullptr, {}},
        {"rchase2", "Rail Chase 2", "1994, Revision A (Model 2B; gun turrets: the mouse or a gamepad; two players: a second gamepad)", nullptr, {}},
        {"gunblade", "Gunblade NY", "1995, Revision A (Model 2B; gun turret: the mouse or a gamepad)", nullptr, {}},
        {"hotdo", "The House of the Dead", "1997, Model 2C (light guns: the mouse or a gamepad)", nullptr, {}},
        {"von", "Cyber Troopers Virtual-On", "1995, Twin, Export (Model 2B; twin sticks: both sticks of the gamepad)", nullptr, {}, 0, false, true},
        {"vr", "Virtua Racing", "1992, Model 1 (link play)", nullptr, {}},
        {"swa", "Star Wars Arcade", "1993, Model 1", nullptr, {}, 0, true},
        {"vf", "Virtua Fighter", "1993, Model 1 (two players: a second gamepad)", nullptr, {}},
        {"wingwar", "Wing War", "1994, Model 1 (a flight stick: the left stick)", nullptr, {}, 0, true},
        {"vf3", "Virtua Fighter 3", "1996, Model 3 (two players: a second gamepad)", nullptr, {}},
        {"scud", "Scud Race", "1996, Model 3", nullptr, {}},
        {"scudplus", "Scud Race", "1997, Scud Race Plus, Model 3", nullptr, {}},
        {"daytona2", "Daytona USA 2", "1998, Battle on the Edge, Model 3", nullptr, {}},
        {"dayto2pe", "Daytona USA 2", "1998, Power Edition, Model 3", nullptr, {}},
        {"srally2", "Sega Rally 2", "1998, Model 3 (South view change, East hand brake)", nullptr, {}},
        {"lemans24", "Le Mans 24", "1997, Model 3", nullptr, {}},
        {"dirtdvls", "Dirt Devils", "1998, Model 3 (South view change)", nullptr, {}},
        {"harley", "Harley-Davidson & L.A. Riders", "1997, Model 3 (South view change, East music select; two coins per credit)", nullptr, {}},
        {"ecau", "Emergency Call Ambulance", "1999, Model 3 (South view change)", nullptr, {}},
        {"fvipers2", "Fighting Vipers 2", "1998, Model 3 (West punch, South kick, East guard; two players: a second gamepad)", nullptr, {}},
        {"von2", "Cyber Troopers Virtual-On Oratorio Tangram", "1998, Model 3 (twin sticks: both sticks of the gamepad; two coins per credit)", nullptr, {}, 0, false, true},
        {"spikeout", "Spikeout", "1998, Model 3 (West beat, South charge, East shift, North jump)", nullptr, {}},
        {"spikeofe", "Spikeout", "1999, Final Edition, Model 3 (West beat, South charge, East shift, North jump)", nullptr, {}},
        {"vs299a", "Virtua Striker 2 '99", "1999, Model 3 (West short pass, South long pass, East shoot; two players: a second gamepad)", nullptr, {}},
        {"swtrilgy", "Star Wars Trilogy Arcade", "1998, Model 3 (the left stick flies, left shoulder second trigger, East/West event buttons)", nullptr, {}, 0, true},
        {"lostwsga", "The Lost World", "1997, Model 3 (light guns: the mouse or a gamepad; right button reloads)", nullptr, {}, 0, false, false, true},
        {"lamachin", "L.A. Machineguns", "1998, Model 3 (light guns: the mouse or a gamepad)", nullptr, {}, 0, false, false, true},
        {"oceanhuna", "The Ocean Hunter", "1998, Model 3 (light guns: the mouse or a gamepad; two coins per credit)", nullptr, {}, 0, false, false, true},
        {"hotd4a", "The House of the Dead 4", "2005, Revision A, Lindbergh (light gun: the mouse or a gamepad; middle-click, G or West throws a grenade)", nullptr, {}, 0, false, false, false, true},
        {"letsgoju", "Let's Go Jungle!", "2006, Lindbergh (light gun: the mouse or a gamepad, hold to fire)", nullptr, {}, 0, false, false, false, true},
        {"segartv", "Sega Race TV", "2008, Lindbergh", nullptr, {}, 0, false, false, false, true},
        {"outr2sdx", "OutRun 2 SP SDX", "2006, Lindbergh", nullptr, {}, 0, false, false, false, true},
        {"outrun", "Out Run", "1986, Revision B, Out Run board (the shoulders move the gear lever: low and high)", nullptr, {}},
        {"shangon", "Super Hang-On", "1987, sit-down/upright, Out Run board (the right shoulder is the turbo)", nullptr, {}},
        {"aburner2", "After Burner II", "1987, X Board (a flight stick: the left stick; South fires the vulcan, East the missiles)", nullptr, {}, 0, true},
        {"thndrbld", "Thunder Blade", "1987, upright, X Board (a flight stick: the left stick; South fires the guns, East the missiles)", nullptr, {}, 0, true},
        {"bassdx", "Sega Bass Fishing", "1998, Deluxe, USA, Model 3 (the left stick is the rod, the right trigger reels, the d-pad the lure stick, West casts, South selects)", nullptr, {}},
        {"getbass", "Get Bass", "1998, Japan, Standard, Model 3 (the left stick is the rod, the right trigger reels, West casts)", nullptr, {}},
        {"getbassur", "Get Bass", "1998, Japan, Upright, Model 3 (the left stick is the rod, the right trigger reels, West casts)", nullptr, {}},
        {"getbassdx", "Get Bass", "1998, Japan, Deluxe, Model 3 (needs bassdx.zip beside it; the left stick is the rod, the right trigger reels, West casts; two coins per credit)", nullptr, {}},
        {"magtruck", "Magical Truck Adventure", "1998, Japan, Model 3 (pump the lever: the stick up and down; West the pedal; two players: a second gamepad)", nullptr, {}},
    };
    // Left out of this release while untested (the cores stay): take a set out to bring it back.
    static const char *const kNotThisRelease[] = {"letsgoju", "segartv"};
    std::erase_if(games, [](const Game &g) {
        return std::any_of(std::begin(kNotThisRelease), std::end(kNotThisRelease), [&](const char *s) { return g.romset == s; });
    });
    for (Game &g : games) {
        // Its board, as its detail names it; Model 2 when it names none.
        g.platform = g.detail.find("Model 1") != std::string::npos     ? 1
                     : g.detail.find("Model 3") != std::string::npos   ? 3
                     : g.detail.find("Lindbergh") != std::string::npos ? 4
                     : g.detail.find("Out Run board") != std::string::npos || g.detail.find("X Board") != std::string::npos ? 0
                                                                       : 2;
        g.api = load_core(o.cores, g.romset, &g.outdated);
        if (g.folder) { // the game folder, or its package
            std::error_code ec;
            for (const std::string &name : set_names(g.romset))
                if (g.rom.empty() && fs::exists(o.links / name / "disk0", ec)) g.rom = o.links / name;
            for (const std::string &name : set_names(g.romset))
                if (g.rom.empty() && fs::exists(o.links / (name + ".tar"), ec)) g.rom = o.links / (name + ".tar");
            continue;
        }
        // hotd.7z (MAME's parent) may hold The House of the Dead's original set, doa.zip Dead or Alive's Revision B;
        // Over Rev's split .zip lacks the parent's files, so its .7z (the whole set) first.
        static const char *const kZipFirst[] = {".zip", ".7z"}, *const k7zFirst[] = {".7z", ".zip"};
        const auto &exts = g.romset == "overrevb" ? k7zFirst : kZipFirst;
        for (const std::string &name : set_names(g.romset))
            for (const char *ext : exts)
                if (g.rom.empty() && fs::exists(o.links / (name + ext))) g.rom = o.links / (name + ext);
    }
    return games;
}

// When each game was last started, for the Recently played order: lines of
// "<romset> <seconds since 1970>" in <data>/last-played.
std::map<std::string, double> last_played(const fs::path &data) {
    std::map<std::string, double> all;
    if (std::FILE *f = std::fopen((data / "last-played").c_str(), "r")) {
        char set[64];
        double t = 0;
        while (std::fscanf(f, "%63s %lf", set, &t) == 2) all[set] = t;
        std::fclose(f);
    }
    return all;
}

void mark_played(const fs::path &data, const std::string &romset) {
    auto all = last_played(data);
    all[romset] = double(std::time(nullptr));
    std::error_code ec;
    fs::create_directories(data, ec);
    if (std::FILE *f = std::fopen((data / "last-played").c_str(), "w")) {
        for (const auto &[set, t] : all) std::fprintf(f, "%s %.0f\n", set.c_str(), t);
        std::fclose(f);
    }
}

// One row of the game list: a game on one board with its versions (Daytona
// USA on Model 2: daytona93 and daytona), as the Apple app's ArcadeGame.
struct ArcadeGame {
    std::string title;
    int platform = 2;
    std::vector<const Game *> versions;

    std::string key() const { return std::string(kPlatforms[platform].key) + "/" + title; }
    bool playable() const {
        return std::any_of(versions.begin(), versions.end(), [](const Game *g) { return g->playable(); });
    }
    // The release year of its earliest version (each detail starts with it).
    int year() const {
        int y = 9999;
        for (const Game *g : versions) y = std::min(y, std::atoi(g->detail.substr(0, 4).c_str()));
        return y;
    }
};

// `games` grouped by title and board, in catalog order.
std::vector<ArcadeGame> group_games(const std::vector<Game> &games) {
    std::vector<ArcadeGame> all;
    for (const Game &g : games) {
        auto it = std::find_if(all.begin(), all.end(), [&](const ArcadeGame &a) { return a.title == g.title && a.platform == g.platform; });
        if (it == all.end()) it = all.insert(all.end(), ArcadeGame{g.title, g.platform, {}});
        it->versions.push_back(&g);
    }
    return all;
}

// `games` in the list's order, as the Apple app's ArcadeGame.sorted: A-Z
// with "The House of the Dead" under H; Year, the same year A-Z; Platform,
// Model 1 to Lindbergh, each by year; Recently played (any of a game's
// versions), the games never played after the others, A-Z.
void sort_games(std::vector<ArcadeGame> &games, Sort sort, const std::map<std::string, double> &played) {
    const auto title = [](const ArcadeGame &g) {
        std::string t = g.title.rfind("The ", 0) == 0 ? g.title.substr(4) : g.title;
        for (char &c : t) c = char(std::tolower((unsigned char)c));
        return t;
    };
    const auto by_title = [&](const ArcadeGame &a, const ArcadeGame &b) { return title(a) < title(b); };
    const auto by_year = [&](const ArcadeGame &a, const ArcadeGame &b) { return a.year() != b.year() ? a.year() < b.year() : by_title(a, b); };
    const auto last = [&](const ArcadeGame &g) {
        double t = -1;
        for (const Game *v : g.versions)
            if (const auto it = played.find(v->romset); it != played.end()) t = std::max(t, it->second);
        return t;
    };
    std::stable_sort(games.begin(), games.end(), [&](const ArcadeGame &a, const ArcadeGame &b) {
        switch (sort) {
        case Sort::title: return by_title(a, b);
        case Sort::year: return by_year(a, b);
        case Sort::platform: return a.platform != b.platform ? a.platform < b.platform : by_year(a, b);
        case Sort::recent: return last(a) != last(b) ? last(a) > last(b) : by_title(a, b);
        }
        return false;
    });
}

// The game list's choices, remembered in <data>/library as "key=value"
// lines: all games or only the playable ones (P or North), the platform
// (-1 all; Left/Right or L1/R1) and the order (S or L2/R2).
struct LibrarySettings {
    bool playable_only = false;
    int platform = -1;
    Sort sort = Sort::title;

    void load(const fs::path &file) {
        std::FILE *f = std::fopen(file.c_str(), "r");
        if (!f) return;
        char line[64];
        while (std::fgets(line, sizeof line, f)) {
            std::string l = line;
            while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
            const size_t eq = l.find('=');
            if (eq == std::string::npos) continue;
            const std::string key = l.substr(0, eq), v = l.substr(eq + 1);
            if (key == "playable") playable_only = v == "1";
            for (int i = 0; i < kPlatformCount; ++i)
                if (key == "platform" && v == kPlatforms[i].key) platform = i;
            for (int i = 0; i < kSortCount; ++i)
                if (key == "sort" && v == kSortKeys[i]) sort = Sort(i);
        }
        std::fclose(f);
    }
    void save(const fs::path &file) const {
        std::error_code ec;
        fs::create_directories(file.parent_path(), ec);
        if (std::FILE *f = std::fopen(file.c_str(), "w")) {
            std::fprintf(f, "playable=%d\nplatform=%s\nsort=%s\n", playable_only ? 1 : 0,
                         platform < 0 ? "all" : kPlatforms[platform].key, kSortKeys[int(sort)]);
            std::fclose(f);
        }
    }
};

// Full screen (issue #226): F11 or Alt+Enter anywhere, and Full screen in
// the pause menu. The choice is kept in <data>/fullscreen ("1" or "0") for
// the next start; --fullscreen starts full screen whatever it says.
bool fullscreen_saved(const fs::path &data) {
    std::FILE *f = std::fopen((data / "fullscreen").c_str(), "r");
    if (!f) return false;
    const int c = std::fgetc(f);
    std::fclose(f);
    return c == '1';
}

// Full screen as last asked for: SDL3 switches asynchronously, so the
// window's flags can lag behind a quick second press.
bool g_fullscreen = false;

void set_fullscreen(SDL_Window *w, bool on, const fs::path &data) {
    g_fullscreen = on;
    SDL_SetWindowFullscreen(w, on);
    std::error_code ec;
    fs::create_directories(data, ec);
    if (std::FILE *f = std::fopen((data / "fullscreen").c_str(), "w")) {
        std::fputs(on ? "1\n" : "0\n", f);
        std::fclose(f);
    }
}

// F11 or Alt+Enter: switches full screen on or off; true when the event was one
bool fullscreen_key(const SDL_Event &e, SDL_Window *w, const fs::path &data) {
    if (e.type != SDL_EVENT_KEY_DOWN || e.key.repeat) return false;
    if (e.key.scancode != SDL_SCANCODE_F11 && !(e.key.scancode == SDL_SCANCODE_RETURN && (e.key.mod & SDL_KMOD_ALT))) return false;
    set_fullscreen(w, !g_fullscreen, data);
    return true;
}

// The cabinet's controls from the keyboard and the first gamepad.
// Keyboard: arrows steer, accelerate and brake; A/Z gear up/down; 1-4
// views; 5 coin; Return start; F2 test; F3 service; Esc pause menu.
// Gamepad: Back (Select) inserts a coin, Start presses start, both together
// pause; in operator mode (pause menu) South is TEST and East SERVICE. Virtua Cop 2:
// the mouse aims player 1's gun, the left button (or Space) fires, the right
// button reloads. Virtua Fighter 2: arrows move, A punch, S kick, D guard;
// on a gamepad the left stick or d-pad, West punch, South kick, East guard,
// and a second gamepad is player 2. Star Wars Arcade (`flight`): the left
// stick (keyboard: arrows) is the flight stick, pushed forward to dive; the
// right trigger is the throttle (keyboard: A opens it, Z closes it); South,
// the right shoulder or Space fires; East and West (keyboard: X and C) are
// the stick's other buttons; 6 is player 2's start. Wing War flies the
// same way: South fires the machine gun, East a missile, West smoke; North
// (keyboard: 1-4) changes the view. Virtual-On (`twin`): the
// left stick (or d-pad) is the left lever, the left trigger its shot and the
// left shoulder its dash; the right stick, trigger and shoulder the right
// lever's. Keyboard: WASD, Q shot, E dash (left); IJKL, U shot, O dash (right).
// Oratorio Tangram (Model 3) takes the same controls as its shot triggers and
// turbo buttons. Star Wars Trilogy flies like Star Wars Arcade; its second
// trigger is the left shoulder (keyboard V). Model 3's light guns
// (`analog_gun`) aim with the mouse; the right button is the gun's second
// trigger (The Lost World's reload), not a shot off screen. In the light
// gun games (`gun`) a gamepad aims too, as on the Apple devices: the right
// stick (or the left) moves the aim from where the mouse left it, the right
// trigger or South fires, East reloads, a crosshair shows the aim (off on
// the game's page; none where the game draws its own sight); a second
// gamepad is player 2's gun. The House of the Dead 4's gun button throws a
// grenade: the middle button, G or West (each gamepad its player's).
// Controller remapping (issue #22), as on the Apple devices: for each
// button of the default layout, the button that stands in for it in one
// game. Saved in <data>/buttons/<romset> as "a=b,rightTrigger=a" (the Apple
// app's names); chosen under Controller buttons in the pause menu. The
// sticks and the d-pad keep their jobs, and the pause chord and operator
// mode read the buttons as they are.
enum PadButton { kA, kB, kX, kY, kL1, kR1, kL2, kR2, kBack, kStart, kPadButtons };
const char *const kPadKeys[kPadButtons] = {"a", "b", "x", "y", "leftShoulder", "rightShoulder",
                                           "leftTrigger", "rightTrigger", "options", "menu"};
const char *const kPadNames[kPadButtons] = {"South (A, Cross)", "East (B, Circle)", "West (X, Square)", "North (Y, Triangle)",
                                            "L1 / LB", "R1 / RB", "L2 / LT", "R2 / RT", "Back (Share, View)", "Start (Options, Menu)"};

// The default layout's button for an SDL button, or -1 (the d-pad, the sticks' clicks).
int pad_button(SDL_GamepadButton b) {
    switch (b) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return kA;
    case SDL_GAMEPAD_BUTTON_EAST: return kB;
    case SDL_GAMEPAD_BUTTON_WEST: return kX;
    case SDL_GAMEPAD_BUTTON_NORTH: return kY;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return kL1;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return kR1;
    case SDL_GAMEPAD_BUTTON_BACK: return kBack;
    case SDL_GAMEPAD_BUTTON_START: return kStart;
    default: return -1;
    }
}

struct ButtonMap {
    int to[kPadButtons];
    ButtonMap() { reset(); }
    void reset() {
        for (int i = 0; i < kPadButtons; ++i) to[i] = i;
    }
    bool is_default() const {
        for (int i = 0; i < kPadButtons; ++i)
            if (to[i] != i) return false;
        return true;
    }

    // How far a button of the gamepad is pressed, 0...1 (the triggers are analog).
    static float raw(SDL_Gamepad *p, int b) {
        static const SDL_GamepadButton buttons[kPadButtons] = {
            SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH,
            SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, SDL_GAMEPAD_BUTTON_INVALID,
            SDL_GAMEPAD_BUTTON_INVALID, SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_START};
        if (!p) return 0;
        if (b == kL2 || b == kR2)
            return std::max(0.f, SDL_GetGamepadAxis(p, b == kL2 ? SDL_GAMEPAD_AXIS_LEFT_TRIGGER : SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.0f);
        return SDL_GetGamepadButton(p, buttons[b]) ? 1.f : 0.f;
    }
    // The default layout's button `b`, through this game's map.
    float value(SDL_Gamepad *p, int b) const { return raw(p, to[b]); }
    bool pressed(SDL_Gamepad *p, int b) const { return value(p, b) > 0.5f; }

    static ButtonMap load(const fs::path &file) {
        ButtonMap m;
        std::FILE *f = std::fopen(file.c_str(), "r");
        if (!f) return m;
        char buf[512] = "";
        const std::string text(buf, std::fread(buf, 1, sizeof buf - 1, f));
        std::fclose(f);
        const auto index = [](const std::string &key) {
            for (int i = 0; i < kPadButtons; ++i)
                if (key == kPadKeys[i]) return i;
            return -1;
        };
        size_t at = 0;
        while (at < text.size()) {
            size_t end = text.find_first_of(",\n", at);
            if (end == std::string::npos) end = text.size();
            const std::string pair = text.substr(at, end - at);
            if (const size_t eq = pair.find('='); eq != std::string::npos) {
                const int from = index(pair.substr(0, eq)), to_ = index(pair.substr(eq + 1));
                if (from >= 0 && to_ >= 0) m.to[from] = to_;
            }
            at = end + 1;
        }
        return m;
    }
    void save(const fs::path &file) const {
        std::error_code ec;
        if (is_default()) {
            fs::remove(file, ec);
            return;
        }
        fs::create_directories(file.parent_path(), ec);
        std::string text;
        for (int i = 0; i < kPadButtons; ++i)
            if (to[i] != i) text += std::string(text.empty() ? "" : ",") + kPadKeys[i] + "=" + kPadKeys[to[i]];
        if (std::FILE *f = std::fopen(file.c_str(), "w")) {
            std::fprintf(f, "%s\n", text.c_str());
            std::fclose(f);
        }
    }
};

struct Controls {
    ButtonMap map; // the game being played's (ButtonMap)
    SDL_Gamepad *pad = nullptr, *pad2 = nullptr;
    int gear = 1;
    float steer = 0;
    bool flight = false;    // set for the game being played (Game::flight)
    bool twin = false;      // (Game::twin)
    bool analog_gun = false; // (Game::analog_gun)
    bool gun = false;        // a light gun game (is_gun_game): the gamepads aim
    bool gun_button = false; // the gun's button, BUTTON3: HotD4's grenade (GameInfo.gunButton)
    bool crosshair = false;  // and draw_crosshairs shows where (shows_crosshair, and the game's setting)
    bool second_joined = false; // player 2 pressed Start on the second gamepad: its crosshair shows
    // Where the guns aim, 0..1 across and down the picture: player 1's from
    // the mouse (when it moves) or the first gamepad's stick, player 2's from
    // the second gamepad's.
    float aim_x = 0.5f, aim_y = 0.5f, aim2_x = 0.5f, aim2_y = 0.5f;
    float mouse_x = -1, mouse_y = -1; // the mouse where it was last sampled
    bool stick_y = false;    // the left stick's forward and back is stick_y (Sega Ski Super G's swing, Top Skater's slide)
    bool fishing = false;    // Sega Bass Fishing: the left stick is the rod, only the d-pad moves the lure stick
    // Operator mode (pause menu): the gamepad's South is TEST and East
    // SERVICE, the game's own buttons are off; they count once both were up.
    bool operator_mode = false, operator_armed = false;
    float key_throttle = 0; // where the keys left the throttle lever
    int view = 0, view_frames = 0; // Wing War: the view North last picked, and how long its button stays down
    bool view_held = false;
    bool up_held = false, down_held = false, coin_held = false, start_held = false;
    // Frames since Back (coin) or Start was pressed, or -1. The game sees a
    // press a few frames late, so pressing both to pause neither inserts a
    // coin nor starts.
    int coin_wait = -1, start_wait = -1;
    bool chord = false; // Back and Start pressed together: neither counts until both are up
    static constexpr int grace = 4, pulse = 6;

    // Whether Back and Start are both down (the pause chord); from then on
    // until both are up neither reaches the game.
    bool chord_down() {
        if (!pad || !SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_BACK) || !SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_START)) return false;
        chord = true;
        coin_wait = start_wait = -1;
        return true;
    }

    // A gamepad's button or trigger as the game's map has it (ButtonMap).
    bool pad_btn(SDL_Gamepad *p, SDL_GamepadButton x) const {
        const int b = pad_button(x);
        return b < 0 ? SDL_GetGamepadButton(p, x) : map.pressed(p, b);
    }
    float pad_axis(SDL_Gamepad *p, SDL_GamepadAxis a) const {
        if (a == SDL_GAMEPAD_AXIS_LEFT_TRIGGER) return map.value(p, kL2);
        if (a == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) return map.value(p, kR2);
        return SDL_GetGamepadAxis(p, a) / 32767.0f;
    }

    // A gun's aim from a gamepad: the right stick moves it, or the left
    // (whichever is pushed further), as on the Apple devices.
    void move_aim(SDL_Gamepad *p, float &x, float &y) const {
        float sx = pad_axis(p, SDL_GAMEPAD_AXIS_RIGHTX), sy = pad_axis(p, SDL_GAMEPAD_AXIS_RIGHTY);
        const float lx = pad_axis(p, SDL_GAMEPAD_AXIS_LEFTX), ly = pad_axis(p, SDL_GAMEPAD_AXIS_LEFTY);
        if (lx * lx + ly * ly > sx * sx + sy * sy) sx = lx, sy = ly;
        if (std::fabs(sx) > 0.15f || std::fabs(sy) > 0.15f) {
            x = std::clamp(x + sx * 0.025f, 0.f, 1.f);
            y = std::clamp(y + sy * 0.025f, 0.f, 1.f);
        }
    }
    // A gamepad's trigger (the right trigger or South) and reload (East).
    bool pad_fire(SDL_Gamepad *p) const { return pad_axis(p, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 0.5f || pad_btn(p, SDL_GAMEPAD_BUTTON_SOUTH); }
    bool pad_reload(SDL_Gamepad *p) const { return pad_btn(p, SDL_GAMEPAD_BUTTON_EAST); }

    void open_pad() {
        if (pad) return;
        int n = 0;
        SDL_JoystickID *ids = SDL_GetGamepads(&n);
        if (ids && n > 0) pad = SDL_OpenGamepad(ids[0]);
        if (ids && n > 1 && !pad2) pad2 = SDL_OpenGamepad(ids[1]);
        SDL_free(ids);
    }

    // The fighting games' buttons and joystick from one gamepad (North is
    // Virtua Fighter 3's Escape).
    uint32_t fight(SDL_Gamepad *p) const {
        if (!p) return 0;
        const auto btn = [&](SDL_GamepadButton x) { return pad_btn(p, x); };
        float x = SDL_GetGamepadAxis(p, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f, y = SDL_GetGamepadAxis(p, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
        if (fishing) x = y = 0;
        uint32_t b = 0;
        if (btn(SDL_GAMEPAD_BUTTON_WEST)) b |= AFCORE_PUNCH;
        if (btn(SDL_GAMEPAD_BUTTON_SOUTH)) b |= AFCORE_KICK;
        if (btn(SDL_GAMEPAD_BUTTON_EAST)) b |= AFCORE_GUARD;
        if (btn(SDL_GAMEPAD_BUTTON_NORTH)) b |= AFCORE_BUTTON2; // Virtua Fighter 3's Escape
        if (btn(SDL_GAMEPAD_BUTTON_DPAD_UP) || y < -0.5f) b |= AFCORE_UP;
        if (btn(SDL_GAMEPAD_BUTTON_DPAD_DOWN) || y > 0.5f) b |= AFCORE_DOWN;
        if (btn(SDL_GAMEPAD_BUTTON_DPAD_LEFT) || x < -0.5f) b |= AFCORE_LEFT;
        if (btn(SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || x > 0.5f) b |= AFCORE_RIGHT;
        return b;
    }

    afcore_input sample(SDL_Renderer *ren) {
        afcore_input in{};
        in.size = sizeof in;
        const bool *k = SDL_GetKeyboardState(nullptr);
        float target = 0, accel = 0, brake = 0;
        bool up = k[SDL_SCANCODE_A], down = k[SDL_SCANCODE_Z];
        uint32_t b = 0;
        if (pad) {
            const auto axis = [&](SDL_GamepadAxis a) { return pad_axis(pad, a); };
            target = axis(SDL_GAMEPAD_AXIS_LEFTX);
            if (std::fabs(target) < 0.08f) target = 0;
            accel = std::max(0.f, axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
            brake = std::max(0.f, axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
            const auto btn = [&](SDL_GamepadButton x) { return pad_btn(pad, x); };
            up = up || btn(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
            down = down || btn(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
            if (btn(SDL_GAMEPAD_BUTTON_SOUTH)) b |= AFCORE_VIEW1;
            if (btn(SDL_GAMEPAD_BUTTON_EAST)) { b |= AFCORE_VIEW2; in.handbrake = 1; }
            if (btn(SDL_GAMEPAD_BUTTON_WEST)) b |= AFCORE_VIEW3;
            if (btn(SDL_GAMEPAD_BUTTON_NORTH)) b |= AFCORE_VIEW4;
            const bool coin = btn(SDL_GAMEPAD_BUTTON_BACK), start = btn(SDL_GAMEPAD_BUTTON_START);
            if (!coin && !start) chord = false;
            if (!chord) {
                if (coin && !coin_held) coin_wait = 0;
                if (start && !start_held) start_wait = 0;
            }
            coin_held = coin;
            start_held = start;
        }
        // A coin is a short pulse; start lasts as long as Start is held (codes hold it).
        if (coin_wait >= 0) {
            if (coin_wait >= grace) b |= AFCORE_COIN;
            if (++coin_wait >= grace + pulse) coin_wait = -1;
        }
        if (start_wait >= 0) {
            if (start_wait >= grace) b |= AFCORE_START;
            if (++start_wait >= grace + pulse && !start_held) start_wait = -1;
        }
        const bool l = k[SDL_SCANCODE_LEFT], r = k[SDL_SCANCODE_RIGHT];
        if (l || r) steer += std::clamp((r ? 1.f : 0.f) - (l ? 1.f : 0.f) - steer, -0.12f, 0.12f); // keys ramp, as in the recomp's app
        else steer = target;
        if (k[SDL_SCANCODE_UP]) accel = 1;
        if (k[SDL_SCANCODE_DOWN]) brake = 1;
        if (k[SDL_SCANCODE_1]) b |= AFCORE_VIEW1;
        if (k[SDL_SCANCODE_2]) { b |= AFCORE_VIEW2; in.handbrake = 1; }
        if (k[SDL_SCANCODE_3]) b |= AFCORE_VIEW3;
        if (k[SDL_SCANCODE_4]) b |= AFCORE_VIEW4;
        if (k[SDL_SCANCODE_5]) b |= AFCORE_COIN;
        if (k[SDL_SCANCODE_RETURN] && !(SDL_GetModState() & SDL_KMOD_ALT)) b |= AFCORE_START; // Alt+Enter: full screen
        if (k[SDL_SCANCODE_F2]) b |= AFCORE_TEST;
        if (k[SDL_SCANCODE_F3]) b |= AFCORE_SERVICE;
        b |= fight(pad); // Virtua Fighter 2 (the other games ignore these)
        if (k[SDL_SCANCODE_A]) b |= AFCORE_PUNCH;
        if (k[SDL_SCANCODE_S]) b |= AFCORE_KICK;
        if (k[SDL_SCANCODE_D]) b |= AFCORE_GUARD;
        if (k[SDL_SCANCODE_F]) b |= AFCORE_BUTTON2;
        if (k[SDL_SCANCODE_UP]) b |= AFCORE_UP;
        if (k[SDL_SCANCODE_DOWN]) b |= AFCORE_DOWN;
        if (l) b |= AFCORE_LEFT;
        if (r) b |= AFCORE_RIGHT;
        in.buttons2 = fight(pad2);
        if (pad2 && pad_btn(pad2, SDL_GAMEPAD_BUTTON_START)) b |= AFCORE_START2, second_joined = true;
        if (up) b |= AFCORE_SHIFT_UP; // Manx TT, INDY 500 and Virtua Racing read the shift buttons themselves
        if (down) b |= AFCORE_SHIFT_DOWN;
        if (up && !up_held) gear = std::min(gear + 1, 4);
        if (down && !down_held) gear = std::max(gear - 1, 1);
        up_held = up;
        down_held = down;
        if (operator_mode) {
            b &= AFCORE_COIN | AFCORE_START | AFCORE_START2 | AFCORE_TEST | AFCORE_SERVICE;
            in.buttons2 = 0;
            const bool test = pad && SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_SOUTH);
            const bool service = pad && SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_EAST);
            if (!test && !service) operator_armed = true;
            if (operator_armed && test) b |= AFCORE_TEST;
            if (operator_armed && service) b |= AFCORE_SERVICE;
            in.steer = std::clamp(steer, -1.f, 1.f); // the stick and pedals stay: test mode's input checks
            in.accel = accel;
            in.brake = brake;
            in.gear = gear;
            in.gun_x[0] = in.gun_x[1] = in.gun_y[0] = in.gun_y[1] = -1.f;
            in.buttons = b;
            return in;
        }
        if (flight) return fly(k, b & (AFCORE_COIN | AFCORE_START | AFCORE_START2 | AFCORE_TEST | AFCORE_SERVICE));
        if (twin) return twin_sticks(k, b & (AFCORE_COIN | AFCORE_START | AFCORE_TEST | AFCORE_SERVICE));
        in.steer = std::clamp(steer, -1.f, 1.f);
        in.accel = accel;
        in.brake = brake;
        in.gear = gear;
        if (stick_y && pad) in.stick_y = std::clamp(pad_axis(pad, SDL_GAMEPAD_AXIS_LEFTY), -1.f, 1.f); // -1 pushed forward
        // Virtua Cop 2's gun: the mouse over the picture (where draw_screen put it)
        float mx = 0, my = 0, rx = 0, ry = 0;
        const SDL_MouseButtonFlags mb = SDL_GetMouseState(&mx, &my);
        SDL_RenderCoordinatesFromWindow(ren, mx, my, &rx, &ry);
        int ow = 0, oh = 0;
        SDL_GetCurrentRenderOutputSize(ren, &ow, &oh);
        SDL_FRect pic = g_picture;
        if (pic.w <= 0 || pic.h <= 0) pic = screen_rect(ow, oh, 496, 384, Look{});
        const double dw = pic.w, dh = pic.h, left = pic.x, top = pic.y;
        // the mouse aims when it moves, the gamepad's stick from there
        if (mx != mouse_x || my != mouse_y || !gun || !pad) {
            aim_x = float((rx - left) / dw);
            aim_y = float((ry - top) / dh);
        }
        mouse_x = mx, mouse_y = my;
        bool fire = (mb & SDL_BUTTON_LMASK) || k[SDL_SCANCODE_SPACE], reload = mb & SDL_BUTTON_RMASK;
        bool grenade = (mb & SDL_BUTTON_MMASK) || k[SDL_SCANCODE_G];
        bool fire2 = false, reload2 = false, grenade2 = false;
        const bool second = gun && pad2;
        if (gun && pad) {
            move_aim(pad, aim_x, aim_y);
            fire = fire || pad_fire(pad);
            reload = reload || pad_reload(pad);
            grenade = grenade || pad_btn(pad, SDL_GAMEPAD_BUTTON_WEST);
        }
        if (second) {
            move_aim(pad2, aim2_x, aim2_y);
            fire2 = pad_fire(pad2);
            reload2 = pad_reload(pad2);
            grenade2 = pad_btn(pad2, SDL_GAMEPAD_BUTTON_WEST);
        }
        if (analog_gun) { // the aim stays on screen; the second trigger reloads (The Lost World)
            in.gun_x[0] = aim_x;
            in.gun_y[0] = aim_y;
            in.gun_x[1] = second ? aim2_x : -1.f; // player 2 without a gamepad: its edge, never fired
            in.gun_y[1] = second ? aim2_y : -1.f;
            if (fire && !reload) b |= AFCORE_TRIGGER1;
            if (reload) b |= AFCORE_TRIGGER2;
            in.buttons = b;
            // player 2's triggers
            in.buttons2 = (fire2 && !reload2 ? AFCORE_TRIGGER1 : 0) | (reload2 ? AFCORE_TRIGGER2 : 0);
            return in;
        }
        // TRIGGER1 is player 1's trigger, TRIGGER2 player 2's; a shot off screen reloads
        in.gun_x[0] = reload ? -1.f : aim_x;
        in.gun_y[0] = reload ? -1.f : aim_y;
        in.gun_x[1] = second && !reload2 ? aim2_x : -1.f; // player 2 without a gamepad: off screen
        in.gun_y[1] = second && !reload2 ? aim2_y : -1.f;
        if (fire || reload) b |= AFCORE_TRIGGER1;
        if (fire2 || reload2) b |= AFCORE_TRIGGER2;
        if (gun_button) { // player 1's in buttons, player 2's in buttons2
            if (grenade) b |= AFCORE_BUTTON3;
            if (grenade2) in.buttons2 |= AFCORE_BUTTON3;
        }
        in.buttons = b;
        return in;
    }

    // Virtual-On's two levers (see above): the left one as player 1's
    // fighting-game buttons (punch = shot, kick = dash), the right one as
    // buttons2 (the core puts them on IN1 and IN2, MAME's von ports).
    // Oratorio Tangram reads the levers the same way, and its shots and
    // turbos as TRIGGER1/BUTTON2 (left) and TRIGGER2/BUTTON3 (right), in b.
    afcore_input twin_sticks(const bool *k, uint32_t b) {
        afcore_input in{};
        in.size = sizeof in;
        uint32_t r = 0;
        const auto lever = [](float x, float y, uint32_t &to) {
            if (y < -0.5f) to |= AFCORE_UP;
            if (y > 0.5f) to |= AFCORE_DOWN;
            if (x < -0.5f) to |= AFCORE_LEFT;
            if (x > 0.5f) to |= AFCORE_RIGHT;
        };
        if (pad) {
            const auto axis = [&](SDL_GamepadAxis a) { return pad_axis(pad, a); };
            const auto btn = [&](SDL_GamepadButton x) { return pad_btn(pad, x); };
            float lx = axis(SDL_GAMEPAD_AXIS_LEFTX), ly = axis(SDL_GAMEPAD_AXIS_LEFTY);
            if (btn(SDL_GAMEPAD_BUTTON_DPAD_LEFT)) lx = -1;
            if (btn(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) lx = 1;
            if (btn(SDL_GAMEPAD_BUTTON_DPAD_UP)) ly = -1;
            if (btn(SDL_GAMEPAD_BUTTON_DPAD_DOWN)) ly = 1;
            lever(lx, ly, b);
            lever(axis(SDL_GAMEPAD_AXIS_RIGHTX), axis(SDL_GAMEPAD_AXIS_RIGHTY), r);
            if (axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 0.5f) b |= AFCORE_PUNCH | AFCORE_TRIGGER1;
            if (btn(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) b |= AFCORE_KICK | AFCORE_BUTTON2;
            if (axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 0.5f) r |= AFCORE_PUNCH, b |= AFCORE_TRIGGER2;
            if (btn(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) r |= AFCORE_KICK, b |= AFCORE_BUTTON3;
        }
        lever(float(k[SDL_SCANCODE_D]) - float(k[SDL_SCANCODE_A]), float(k[SDL_SCANCODE_S]) - float(k[SDL_SCANCODE_W]), b);
        lever(float(k[SDL_SCANCODE_L]) - float(k[SDL_SCANCODE_J]), float(k[SDL_SCANCODE_K]) - float(k[SDL_SCANCODE_I]), r);
        if (k[SDL_SCANCODE_Q]) b |= AFCORE_PUNCH | AFCORE_TRIGGER1;
        if (k[SDL_SCANCODE_E]) b |= AFCORE_KICK | AFCORE_BUTTON2;
        if (k[SDL_SCANCODE_U]) r |= AFCORE_PUNCH, b |= AFCORE_TRIGGER2;
        if (k[SDL_SCANCODE_O]) r |= AFCORE_KICK, b |= AFCORE_BUTTON3;
        in.buttons = b;
        in.buttons2 = r;
        in.gun_x[0] = in.gun_y[0] = in.gun_x[1] = in.gun_y[1] = -1.f;
        return in;
    }

    // Star Wars Arcade's stick, throttle and buttons (see above); `b` holds
    // the shared mapping's coin, starts, test and service.
    afcore_input fly(const bool *k, uint32_t b) {
        afcore_input in{};
        in.size = sizeof in;
        float y = 0, throttle = 0;
        bool fire = k[SDL_SCANCODE_SPACE];
        if (pad) {
            const auto axis = [&](SDL_GamepadAxis a) { return pad_axis(pad, a); };
            const auto btn = [&](SDL_GamepadButton x) { return pad_btn(pad, x); };
            y = axis(SDL_GAMEPAD_AXIS_LEFTY); // SDL: down is positive, as pulling back is
            if (std::fabs(y) < 0.08f) y = 0;
            throttle = std::max(0.f, axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
            fire = fire || btn(SDL_GAMEPAD_BUTTON_SOUTH) || btn(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
            if (btn(SDL_GAMEPAD_BUTTON_EAST)) b |= AFCORE_BUTTON2;
            if (btn(SDL_GAMEPAD_BUTTON_WEST)) b |= AFCORE_BUTTON3;
            if (btn(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) b |= AFCORE_TRIGGER2; // Star Wars Trilogy's second trigger
            // Wing War's four view buttons: North steps to the next view
            const bool north = btn(SDL_GAMEPAD_BUTTON_NORTH);
            if (north && !view_held) view = view % 4 + 1, view_frames = 6;
            view_held = north;
        }
        if (view_frames > 0) {
            --view_frames;
            b |= view == 1 ? AFCORE_VIEW1 : view == 2 ? AFCORE_VIEW2 : view == 3 ? AFCORE_VIEW3 : AFCORE_VIEW4;
        }
        if (k[SDL_SCANCODE_1]) b |= AFCORE_VIEW1;
        if (k[SDL_SCANCODE_2]) b |= AFCORE_VIEW2;
        if (k[SDL_SCANCODE_3]) b |= AFCORE_VIEW3;
        if (k[SDL_SCANCODE_4]) b |= AFCORE_VIEW4;
        if (k[SDL_SCANCODE_UP]) y = -1;
        if (k[SDL_SCANCODE_DOWN]) y = 1;
        if (k[SDL_SCANCODE_A]) key_throttle = std::min(1.f, key_throttle + 0.03f);
        if (k[SDL_SCANCODE_Z]) key_throttle = std::max(0.f, key_throttle - 0.03f);
        if (k[SDL_SCANCODE_X]) b |= AFCORE_BUTTON2;
        if (k[SDL_SCANCODE_C]) b |= AFCORE_BUTTON3;
        if (k[SDL_SCANCODE_V]) b |= AFCORE_TRIGGER2;
        if (k[SDL_SCANCODE_6]) b |= AFCORE_START2;
        if (fire) b |= AFCORE_TRIGGER1;
        in.steer = std::clamp(steer, -1.f, 1.f); // the left stick, or the arrows ramping as for a wheel
        in.stick_y = std::clamp(y, -1.f, 1.f);
        in.accel = std::max(throttle, key_throttle);
        in.buttons = b;
        in.gun_x[0] = in.gun_y[0] = in.gun_x[1] = in.gun_y[1] = -1.f;
        return in;
    }
};

// The game's sound streams (the sound board's two, Star Wars Arcade's
// Digital Sound Board), each at its own rate, into SDL audio
// streams on one device (SDL resamples and mixes them).
struct Audio {
    SDL_AudioDeviceID dev = 0;
    SDL_AudioStream *streams[3] = {};

    void open(const afcore_api *api, afcore *core) {
        dev = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
        if (!dev) return;
        for (int s = 0; s < 3; ++s) {
            const double rate = api->audio_rate(core, s);
            if (rate <= 0) continue;
            const SDL_AudioSpec spec{SDL_AUDIO_F32, 2, int(rate + 0.5)};
            streams[s] = SDL_CreateAudioStream(&spec, nullptr);
            if (streams[s]) SDL_BindAudioStream(dev, streams[s]);
        }
    }
    void push(const afcore_api *api, afcore *core) {
        static float buf[2 * 4096];
        for (int s = 0; s < 3; ++s) {
            size_t n;
            while ((n = api->audio_read(core, s, buf, 4096)) > 0)
                if (streams[s]) SDL_PutAudioStreamData(streams[s], buf, int(n * 2 * sizeof(float)));
            // A stall (window drag): drop the backlog rather than lag behind.
            if (streams[s] && SDL_GetAudioStreamQueued(streams[s]) > int(api->audio_rate(core, s) * 8 * 0.25))
                SDL_ClearAudioStream(streams[s]);
        }
    }
    void close() {
        for (SDL_AudioStream *&s : streams)
            if (s) { SDL_DestroyAudioStream(s); s = nullptr; }
        if (dev) SDL_CloseAudioDevice(dev);
        dev = 0;
    }
};

// Scanlines and the CRT look, drawn over the picture with SDL_BLENDMODE_MOD
// (each pixel's colour multiplies the picture's): dark gaps between the
// game's lines and, for the CRT, the shadow mask's stripes and darker
// edges. SDL's renderer has no shaders, so there is no curve or glow here,
// as there is on the Apple devices. Built again when the size changes.
struct Overlay {
    SDL_Texture *tex = nullptr;
    int w = 0, h = 0, lines = 0;
    Effect effect = Effect::none;

    SDL_Texture *get(SDL_Renderer *r, int ow, int oh, int screen_lines, Effect e) {
        if (e == Effect::none || ow < 1 || oh < 1) return nullptr;
        if (tex && ow == w && oh == h && lines == screen_lines && e == effect) return tex;
        drop();
        w = ow, h = oh, lines = screen_lines, effect = e;
        tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
        if (!tex) return nullptr;
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_MOD);
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
        std::vector<uint32_t> px(size_t(w) * h);
        const double per_line = double(h) / std::max(lines, 1);
        const double fade = std::clamp(per_line - 1.0, 0.0, 1.0); // none under two pixels a line
        const bool mask = e == Effect::crt && double(w) / (lines * 4.0 / 3.0) >= 2.5;
        for (int y = 0; y < h; ++y) {
            const double f = std::fmod((y + 0.5) / per_line, 1.0);
            const double scan = 1 - fade * (1 - (0.45 + 0.55 * std::sin(f * SDL_PI_D)));
            for (int x = 0; x < w; ++x) {
                double rgb[3] = {scan, scan, scan};
                if (mask)
                    for (int c = 0; c < 3; ++c) rgb[c] *= c == x % 3 ? 1.0 : 0.78;
                if (e == Effect::crt) {
                    const double ex = std::min(x + 0.5, w - x - 0.5), ey = std::min(y + 0.5, h - y - 0.5);
                    const double edge = std::clamp(std::min(ex, ey) / 10.0, 0.0, 1.0);
                    const double cx = 2.0 * x / w - 1, cy = 2.0 * y / h - 1;
                    for (double &v : rgb) v *= edge * (1 - 0.12 * (cx * cx + cy * cy));
                }
                px[size_t(y) * w + x] = 0xff000000u | uint32_t(std::lround(rgb[0] * 255)) << 16 |
                                        uint32_t(std::lround(rgb[1] * 255)) << 8 | uint32_t(std::lround(rgb[2] * 255));
            }
        }
        SDL_UpdateTexture(tex, nullptr, px.data(), w * 4);
        return tex;
    }
    void drop() {
        if (tex) SDL_DestroyTexture(tex);
        tex = nullptr;
    }
};

void draw_screen(SDL_Renderer *r, SDL_Texture *tex, int w, int h, const Look &look, Overlay &overlay, bool square_pixels) {
    int ow = 0, oh = 0;
    SDL_GetCurrentRenderOutputSize(r, &ow, &oh);
    const SDL_FRect dst = screen_rect(ow, oh, w, h, look, square_pixels);
    g_picture = dst;
    SDL_SetTextureScaleMode(tex, look.smooth ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
    SDL_RenderTexture(r, tex, nullptr, &dst);
    if (SDL_Texture *o = overlay.get(r, int(std::lround(dst.w)), int(std::lround(dst.h)), h, look.effect))
        SDL_RenderTexture(r, o, nullptr, &dst);
}

uint64_t fnv(const uint32_t *px, size_t n) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i)
        for (int b = 0; b < 4; ++b) {
            h ^= (px[i] >> (8 * b)) & 0xff;
            h *= 0x100000001b3ULL;
        }
    return h;
}

// Each game's picture in the game list: <data>/pictures/<romset>.bmp, the
// game's screen at 4:3 (taken after half a minute of its first play, or
// with "Use this screen as the picture" in the pause menu).
fs::path picture_file(const Options &o, const std::string &romset) { return o.data / "pictures" / (romset + ".bmp"); }

void save_picture(const fs::path &file, const uint32_t *px, int w, int h) {
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    // XRGB: the screen's alpha isn't coverage (Model 1's polygons come with 0), so it is ignored.
    SDL_Surface *screen = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_XRGB8888, const_cast<uint32_t *>(px), w * 4);
    SDL_Surface *pic = SDL_CreateSurface(h * 4 / 3, h, SDL_PIXELFORMAT_ARGB8888); // square pixels (the board's are not)
    if (screen && pic && SDL_BlitSurfaceScaled(screen, nullptr, pic, nullptr, SDL_SCALEMODE_LINEAR) && !SDL_SaveBMP(pic, file.c_str()))
        std::fprintf(stderr, "insert-coin: %s: %s\n", file.c_str(), SDL_GetError());
    SDL_DestroySurface(pic);
    SDL_DestroySurface(screen);
}

// The selected game's picture, loaded again when its file changes.
struct Picture {
    std::string romset;
    fs::file_time_type time{};
    SDL_Texture *tex = nullptr;

    SDL_Texture *get(SDL_Renderer *r, const fs::path &file, const std::string &set) {
        std::error_code ec;
        const auto t = fs::last_write_time(file, ec);
        if (ec) {
            drop();
            return nullptr;
        }
        if (tex && set == romset && t == time) return tex;
        drop();
        romset = set;
        time = t;
        if (SDL_Surface *s = SDL_LoadBMP(file.c_str())) {
            tex = SDL_CreateTextureFromSurface(r, s);
            SDL_DestroySurface(s);
            if (tex) SDL_SetTextureAlphaMod(tex, 110); // behind the list's text
        }
        return tex;
    }
    void drop() {
        if (tex) SDL_DestroyTexture(tex);
        tex = nullptr;
    }
};

// The pause menu over the stopped game.
// Item 1 turns operator mode on or off (Controls::operator_mode), items 2
// and 3 step through the screen's shape and effect (kept as this game's
// own), item 4 opens the controller buttons (draw_buttons), item 5 keeps
// the screen as the game's picture, item 6 switches full screen.
const int kMenuItems = 8;

// The pause menu's frame: the game dimmed, text at twice the size; returns
// where the text starts.
SDL_FPoint menu_frame(SDL_Renderer *r) {
    int ow = 0, oh = 0;
    SDL_GetCurrentRenderOutputSize(r, &ow, &oh);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 170);
    const SDL_FRect all{0, 0, float(ow), float(oh)};
    SDL_RenderFillRect(r, &all);
    SDL_SetRenderScale(r, 2, 2);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    return SDL_FPoint{ow / 4.f - 60, oh / 4.f - 30};
}

// The controller buttons: each button of the default layout and the one
// that stands in for it in this game; rows kPadButtons and kPadButtons + 1
// are Reset and Back. `waiting`: the next button pressed takes the row.
const int kButtonRows = kPadButtons + 2;

void draw_buttons(SDL_Renderer *r, const Game &g, const ButtonMap &map, int row, bool waiting) {
    float sx = 0, sy = 0;
    SDL_GetRenderScale(r, &sx, &sy);
    const SDL_FPoint at = menu_frame(r);
    const float x = at.x - 40, y = at.y - 40;
    SDL_RenderDebugText(r, x, y, "CONTROLLER BUTTONS");
    SDL_RenderDebugText(r, x, y + 12, g.title.c_str());
    for (int i = 0; i < kButtonRows; ++i) {
        std::string line = i == row ? "> " : "  ";
        if (i < kPadButtons) {
            char b[96];
            std::snprintf(b, sizeof b, "%-22s %s", kPadNames[i],
                          waiting && i == row ? "press a button..." : map.to[i] == i ? "-" : kPadNames[map.to[i]]);
            line += b;
        } else line += i == kPadButtons ? "Reset to the default" : "Back";
        SDL_RenderDebugText(r, x, y + 34 + 14 * i, line.c_str());
    }
    SDL_RenderDebugText(r, x, y + 44 + 14 * kButtonRows,
                        waiting ? "Press its new button (Esc cancels)."
                                : "Choose a row, then press its new button.");
    SDL_SetRenderScale(r, sx, sy);
}

// The controller buttons' keys and gamepad buttons: Up/Down choose a row,
// Return or South waits for the button to put on it (Esc cancels), or
// resets, or goes back; so do Esc, Backspace and East (`row` -1).
void buttons_event(const SDL_Event &e, ButtonMap &map, const fs::path &file, int &row, bool &waiting) {
    const bool key = e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat, padb = e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    const auto is = [&](SDL_Scancode s, SDL_GamepadButton b) {
        return (key && e.key.scancode == s) || (padb && e.gbutton.button == b);
    };
    if (waiting) {
        int pressed = padb ? pad_button(SDL_GamepadButton(e.gbutton.button)) : -1;
        if (e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION && e.gaxis.value > 24000) {
            if (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER) pressed = kL2;
            if (e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) pressed = kR2;
        }
        if (pressed >= 0) {
            map.to[row] = pressed;
            map.save(file);
            waiting = false;
        } else if (key && (e.key.scancode == SDL_SCANCODE_ESCAPE || e.key.scancode == SDL_SCANCODE_BACKSPACE)) waiting = false;
        return;
    }
    if (is(SDL_SCANCODE_UP, SDL_GAMEPAD_BUTTON_DPAD_UP)) row = std::max(0, row - 1);
    if (is(SDL_SCANCODE_DOWN, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) row = std::min(kButtonRows - 1, row + 1);
    const bool pick = is(SDL_SCANCODE_RETURN, SDL_GAMEPAD_BUTTON_SOUTH);
    if (pick && row < kPadButtons) waiting = true;
    else if (pick && row == kPadButtons) {
        map.reset();
        map.save(file);
    } else if (pick || is(SDL_SCANCODE_BACKSPACE, SDL_GAMEPAD_BUTTON_EAST) || (key && e.key.scancode == SDL_SCANCODE_ESCAPE))
        row = -1; // back
}

void draw_pause(SDL_Renderer *r, const Game &g, int item, bool operator_mode, const Look &look) {
    const std::string shape = std::string("Shape: ") + Look::name(look.shape), effect = std::string("Effect: ") + Look::name(look.effect);
    const char *const full = g_fullscreen ? "Full screen: on (F11)" : "Full screen: off (F11)";
    const char *const menu[kMenuItems] = {"Resume", operator_mode ? "Leave operator mode" : "Operator mode (Test, Service)",
                                          shape.c_str(), effect.c_str(), "Controller buttons", "Use this screen as the picture",
                                          full, "Quit to library"};
    float sx = 0, sy = 0;
    SDL_GetRenderScale(r, &sx, &sy);
    const SDL_FPoint at = menu_frame(r);
    const float x = at.x, y = at.y;
    SDL_RenderDebugText(r, x, y, "PAUSED");
    SDL_RenderDebugText(r, x, y + 12, g.title.c_str());
    for (int i = 0; i < kMenuItems; ++i) {
        const std::string line = std::string(i == item ? "> " : "  ") + menu[i];
        SDL_RenderDebugText(r, x, y + 34 + 14 * i, line.c_str());
    }
    SDL_SetRenderScale(r, sx, sy);
}

// A game's screen settings: its own (chosen in its pause menu), else every game's.
Look game_look(const Options &o, const std::string &romset) {
    Look look = o.look;
    if (std::FILE *f = std::fopen((o.data / "screen" / romset).c_str(), "r")) {
        char buf[256] = "";
        const size_t n = std::fread(buf, 1, sizeof buf - 1, f);
        std::fclose(f);
        look.parse(std::string(buf, n));
    }
    return look;
}

void save_look(const Options &o, const std::string &romset, const Look &look) {
    std::error_code ec;
    fs::create_directories(o.data / "screen", ec);
    if (std::FILE *f = std::fopen((o.data / "screen" / romset).c_str(), "w")) {
        std::fprintf(f, "%s\n", look.str().c_str());
        std::fclose(f);
    }
}

// A Lindbergh game packed as one file (<romset>.tar, as the Apple app
// keeps it in iCloud Drive and on the ROM server): a plain tar of the
// folder that holds disk0, the set's name as its top folder optional.
// Unpacked into `dst` through <dst>.unpacking; anything that would land
// outside the game folder is refused. Returns an error, or "".
std::string unpack_tar(const fs::path &tar, const std::string &set, const fs::path &dst,
                       const std::function<void(double)> &progress) {
    std::FILE *in = std::fopen(tar.c_str(), "rb");
    if (!in) return tar.string() + ": " + std::strerror(errno);
    std::error_code ec;
    const uintmax_t total = fs::file_size(tar, ec);
    const fs::path staging = dst.string() + ".unpacking";
    fs::remove_all(staging, ec);
    fs::create_directories(staging, ec);
    const std::string base = fs::weakly_canonical(staging, ec).string();
    const auto octal = [](const unsigned char *b, int n) {
        uint64_t v = 0;
        if (b[0] & 0x80) { // GNU base-256, for files over 8 GB
            v = b[0] & 0x7f;
            for (int i = 1; i < n; ++i) v = v << 8 | b[i];
            return v;
        }
        for (int i = 0; i < n; ++i) {
            if (b[i] == 0 || b[i] == ' ') { if (v) break; continue; }
            if (b[i] < '0' || b[i] > '7') break;
            v = v * 8 + (b[i] - '0');
        }
        return v;
    };
    const auto field = [](const unsigned char *b, size_t n) { return std::string(reinterpret_cast<const char *>(b), strnlen(reinterpret_cast<const char *>(b), n)); };
    // The game folder's own path: "./" and the set's name in front dropped; empty when it leaves the folder.
    const auto inside = [&](const std::string &name, bool &top) -> fs::path {
        std::vector<std::string> parts;
        size_t at = 0;
        while (at <= name.size()) {
            size_t slash = name.find('/', at);
            if (slash == std::string::npos) slash = name.size();
            std::string p = name.substr(at, slash - at);
            if (!p.empty() && p != ".") parts.push_back(p);
            at = slash + 1;
        }
        top = false;
        if (!parts.empty()) {
            std::string first = parts[0];
            std::transform(first.begin(), first.end(), first.begin(), ::tolower);
            const auto names = set_names(set);
            if (std::find(names.begin(), names.end(), first) != names.end()) parts.erase(parts.begin());
        }
        if (!name.empty() && name[0] == '/') return {};
        if (std::find(parts.begin(), parts.end(), "..") != parts.end()) return {};
        if (parts.empty()) { top = true; return staging; }
        fs::path p = staging;
        for (const std::string &part : parts) p /= part;
        return p;
    };
    std::string error, long_name, long_link;
    uint64_t pax_size = UINT64_MAX, done = 0;
    double shown = -1;
    std::vector<char> buf(1 << 20);
    unsigned char h[512];
    const auto skip = [&](uint64_t n) { return std::fseek(in, long(n), SEEK_CUR) == 0; };
    const auto read_text = [&](uint64_t n, std::string &out) {
        out.resize(n);
        return n == 0 || std::fread(out.data(), 1, n, in) == n;
    };
    bool ended = false;
    while (error.empty()) {
        if (std::fread(h, 1, 512, in) != 512) { error = "ended early"; break; }
        if (std::all_of(h, h + 512, [](unsigned char c) { return c == 0; })) { ended = true; break; }
        unsigned sum = 0;
        for (int i = 0; i < 512; ++i) sum += (i >= 148 && i < 156) ? ' ' : h[i];
        if (sum != octal(h + 148, 8)) { error = "not a tar file"; break; }
        const char type = char(h[156]);
        uint64_t size = octal(h + 124, 12);
        if (pax_size != UINT64_MAX && type != 'x' && type != 'g') size = pax_size;
        const uint64_t pad = (512 - size % 512) % 512;
        if (type == 'L' || type == 'K' || type == 'x' || type == 'g') {
            std::string text;
            if (!read_text(size, text) || !skip(pad)) { error = "ended early"; break; }
            const std::string trimmed = text.substr(0, strnlen(text.c_str(), text.size()));
            if (type == 'L') long_name = trimmed;
            if (type == 'K') long_link = trimmed;
            if (type == 'x') { // "<length> <key>=<value>\n" records
                size_t at = 0;
                while (at < text.size()) {
                    const size_t nl = text.find('\n', at), space = text.find(' ', at);
                    if (nl == std::string::npos || space == std::string::npos || space > nl) break;
                    const std::string rec = text.substr(space + 1, nl - space - 1);
                    const size_t eq = rec.find('=');
                    if (eq != std::string::npos) {
                        const std::string key = rec.substr(0, eq), value = rec.substr(eq + 1);
                        if (key == "path") long_name = value;
                        if (key == "linkpath") long_link = value;
                        if (key == "size") pax_size = std::strtoull(value.c_str(), nullptr, 10);
                    }
                    at = nl + 1;
                }
            }
            continue;
        }
        std::string name = long_name;
        if (name.empty()) {
            name = field(h, 100);
            const std::string prefix = std::memcmp(h + 257, "ustar", 5) == 0 ? field(h + 345, 155) : "";
            if (!prefix.empty()) name = prefix + "/" + name;
        }
        const std::string link = long_link.empty() ? field(h + 157, 100) : long_link;
        long_name.clear();
        long_link.clear();
        pax_size = UINT64_MAX;
        bool top = false;
        const fs::path out = inside(name, top);
        if (out.empty()) { error = "a file outside the game folder: " + name; break; }
        if (!top) {
            fs::create_directories(out.parent_path(), ec);
            // A symbolic link earlier in the tar must not lead a file out of the game folder.
            if (fs::weakly_canonical(out.parent_path(), ec).string().rfind(base, 0) != 0) {
                error = "a file outside the game folder: " + name;
                break;
            }
        }
        const unsigned mode = unsigned(octal(h + 100, 8)) & 0777;
        if (top || type == '5') {
            fs::create_directories(out, ec);
        } else if (type == '2') {
            fs::remove(out, ec);
            fs::create_symlink(link, out, ec);
        } else if (type == '1') { // a hard link: a copy of the file it names
            bool t = false;
            const fs::path target = inside(link, t);
            if (target.empty() || t) { error = "a bad link: " + link; break; }
            fs::remove(out, ec);
            fs::copy_file(target, out, ec);
        } else if (type == '0' || type == '\0' || type == '7') {
            std::FILE *f = std::fopen(out.c_str(), "wb");
            if (!f) { error = out.string() + ": " + std::strerror(errno); break; }
            for (uint64_t left = size; left > 0;) {
                const size_t n = size_t(std::min<uint64_t>(left, buf.size()));
                if (std::fread(buf.data(), 1, n, in) != n) { error = "ended early"; break; }
                if (std::fwrite(buf.data(), 1, n, f) != n) { error = out.string() + ": could not write (disk full?)"; break; }
                left -= n;
                done += n;
                if (total && progress && double(done) / double(total) - shown >= 0.01) {
                    shown = double(done) / double(total);
                    progress(shown);
                }
            }
            std::fclose(f);
            chmod(out.c_str(), mode ? mode : 0644);
            if (!error.empty()) break;
            if (!skip(pad)) { error = "ended early"; break; }
            continue;
        }
        if (!skip(size + pad)) { error = "ended early"; break; } // devices and FIFOs: not part of a game
    }
    std::fclose(in);
    if (error.empty() && !ended) error = "ended early";
    if (error.empty() && !fs::exists(staging / "disk0", ec)) error = "no disk0 folder: pack the game folder that holds disk0";
    if (!error.empty()) {
        fs::remove_all(staging, ec);
        return tar.filename().string() + ": " + error;
    }
    fs::remove_all(dst, ec);
    fs::rename(staging, dst, ec);
    return ec ? dst.string() + ": " + ec.message() : "";
}

// The folder a Lindbergh game plays from: its own, or its .tar unpacked
// into <data>/games/<romset> (again when the .tar changes). Shows
// "Unpacking…" meanwhile. Returns an empty path after telling the user why.
fs::path game_folder(SDL_Renderer *r, const Game &g, const Options &o) {
    if (g.rom.extension() != ".tar") return g.rom;
    std::error_code ec;
    const fs::path dst = o.data / "games" / g.romset, stamp = o.data / "games" / (g.romset + ".from");
    const std::string from = g.rom.string() + " " + std::to_string(fs::file_size(g.rom, ec)) + " " +
                             std::to_string(fs::last_write_time(g.rom, ec).time_since_epoch().count());
    std::string had;
    if (std::FILE *f = std::fopen(stamp.c_str(), "r")) {
        char line[4096] = "";
        if (std::fgets(line, sizeof line, f)) had = line;
        std::fclose(f);
    }
    if (had == from && fs::exists(dst / "disk0", ec)) return dst;
    fs::create_directories(dst.parent_path(), ec);
    const auto show = [&](double f) {
        SDL_PumpEvents();
        SDL_SetRenderDrawColor(r, BACKGROUND.r, BACKGROUND.g, BACKGROUND.b, 255);
        SDL_RenderClear(r);
        SDL_SetRenderScale(r, 2, 2);
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        const std::string text = "Unpacking " + g.title + "... " + std::to_string(int(f * 100)) + "%";
        SDL_RenderDebugText(r, 24, 20, text.c_str());
        SDL_SetRenderScale(r, 1, 1);
        SDL_RenderPresent(r);
    };
    show(0);
    const std::string err = unpack_tar(g.rom, g.romset, dst, show);
    if (!err.empty()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Arcade Foundry: Insert Coin", err.c_str(), nullptr);
        return {};
    }
    if (std::FILE *f = std::fopen(stamp.c_str(), "w")) {
        std::fputs(from.c_str(), f);
        std::fclose(f);
    }
    return dst;
}

// SDL's renderer draws with OpenGL (and so does a software renderer's window
// surface), and a core with a GL context of its own (Model 3's EGL one) can't
// make that current while SDL's is: GLVND allows a thread one current context,
// and the core's GL then went into SDL's, so the window stayed blank. SDL's is
// let go around each call into a core and taken back after.
struct RendererGL {
    SDL_Window *win;
    SDL_GLContext ctx;
    explicit RendererGL(SDL_Renderer *r) : win(SDL_GetRenderWindow(r)), ctx(SDL_GL_GetCurrentContext()) {}
    void release() const { if (ctx) SDL_GL_MakeCurrent(nullptr, nullptr); }
    void take() const { if (ctx) SDL_GL_MakeCurrent(win, ctx); }
};

// The light gun games (GameInfo.aimsGun in the Apple app's Catalog.swift).
bool is_gun_game(const Game &g) {
    static const char *const ids[] = {"vcop", "vcop2", "hotdo", "gunblade", "bel", "rchase2", "hotd4a", "letsgoju"};
    for (const char *id : ids)
        if (g.romset == id) return true;
    return g.analog_gun;
}

// The gun games that get a crosshair: not Gunblade NY, Behind Enemy Lines
// or Rail Chase 2, whose turrets draw their own sight (as on the Apple devices).
bool shows_crosshair(const Game &g) {
    return is_gun_game(g) && g.romset != "gunblade" && g.romset != "bel" && g.romset != "rchase2";
}

// A game's crosshair setting, on unless turned off on its page:
// <data>/crosshair/<romset> holds "0" when off.
bool crosshair_on(const Options &o, const std::string &romset) {
    std::FILE *f = std::fopen((o.data / "crosshair" / romset).c_str(), "r");
    if (!f) return true;
    const int c = std::fgetc(f);
    std::fclose(f);
    return c != '0';
}

void save_crosshair(const Options &o, const std::string &romset, bool on) {
    std::error_code ec;
    fs::create_directories(o.data / "crosshair", ec);
    if (std::FILE *f = std::fopen((o.data / "crosshair" / romset).c_str(), "w")) {
        std::fputs(on ? "1\n" : "0\n", f);
        std::fclose(f);
    }
}

// A gun's crosshair: a ring with four ticks, in the colour given, with a
// dark outline so it shows on any picture.
void draw_crosshair(SDL_Renderer *r, float x, float y, float size, SDL_Color c) {
    const auto shape = [&](float ox, float oy) {
        const int n = 24;
        SDL_FPoint ring[n + 1];
        for (int i = 0; i <= n; i++) {
            const float a = float(i) * 6.2831853f / n;
            ring[i] = {x + ox + std::cos(a) * size * 0.5f, y + oy + std::sin(a) * size * 0.5f};
        }
        SDL_RenderLines(r, ring, n + 1);
        const float in = size * 0.2f, out = size * 0.75f;
        SDL_RenderLine(r, x + ox - out, y + oy, x + ox - in, y + oy);
        SDL_RenderLine(r, x + ox + in, y + oy, x + ox + out, y + oy);
        SDL_RenderLine(r, x + ox, y + oy - out, x + ox, y + oy - in);
        SDL_RenderLine(r, x + ox, y + oy + in, x + ox, y + oy + out);
    };
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    for (const float d : {-1.f, 1.f}) shape(d, 0), shape(0, d);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
    shape(0, 0);
}

// Where the gamepads aim, while one is connected (the mouse shows where it
// is): player 1's in red, player 2's in blue once player 2 pressed Start
// on the second gamepad (the game's own player state is out of sight), as
// on the Apple devices.
void draw_crosshairs(SDL_Renderer *r, const Controls &ctl) {
    if (!ctl.crosshair || !ctl.pad || g_picture.w <= 0) return;
    const SDL_FRect &p = g_picture;
    const float size = std::max(24.f, p.w / 22);
    if (ctl.pad2 && ctl.second_joined) draw_crosshair(r, p.x + ctl.aim2_x * p.w, p.y + ctl.aim2_y * p.h, size, {60, 140, 255, 255});
    draw_crosshair(r, p.x + ctl.aim_x * p.w, p.y + ctl.aim_y * p.h, size, {255, 40, 40, 255});
}

// Runs one game until Quit in its pause menu (or the window closes: returns false).
bool play(SDL_Renderer *r, const Game &g, const Options &o, Controls &ctl) {
    char err[512] = "";
    const fs::path rom = g.folder ? game_folder(r, g, o) : g.rom;
    if (rom.empty()) return true;
    const fs::path saves = o.data / "saves" / g.romset;
    std::error_code ec;
    fs::create_directories(saves, ec);
    const RendererGL gl(r);
    gl.release();
    afcore *core = g.api->create(rom.c_str(), saves.c_str(), err, sizeof err);
    gl.take();
    if (!core) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Arcade Foundry: Insert Coin", err, nullptr);
        return true;
    }
    mark_played(o.data, g.romset);
    if (o.link_port > 0 && g.api->link_start(core, o.link_port, o.link_next.c_str(), o.link_sync, err, sizeof err) != 0)
        std::fprintf(stderr, "insert-coin: %s\n", err);
    ctl.flight = g.flight;
    ctl.twin = g.twin;
    ctl.analog_gun = g.analog_gun;
    ctl.gun = is_gun_game(g);
    ctl.gun_button = g.romset == "hotd4a";
    ctl.crosshair = shows_crosshair(g) && crosshair_on(o, g.romset);
    ctl.aim_x = ctl.aim_y = ctl.aim2_x = ctl.aim2_y = 0.5f;
    ctl.second_joined = false;
    ctl.fishing = g.romset == "bassdx" || g.romset == "getbass" || g.romset == "getbassur" || g.romset == "getbassdx";
    ctl.stick_y = g.romset == "skisuprg" || g.romset == "topskatr" || ctl.fishing;
    const fs::path buttons_file = o.data / "buttons" / g.romset;
    ctl.map = ButtonMap::load(buttons_file);
    ctl.operator_mode = false;
    Audio audio;
    audio.open(g.api, core);
    SDL_Texture *tex = nullptr;
    int tex_w = 0, tex_h = 0;
    Look look = game_look(o, g.romset);
    Overlay overlay;
    g_picture = SDL_FRect{0, 0, 0, 0};
    const double hz = g.api->frame_hz;
    uint64_t last = SDL_GetPerformanceCounter();
    const double freq = double(SDL_GetPerformanceFrequency());
    double owed = 0;
    std::atomic<long> frames{0}, played{0}; // the game thread counts them too
    const fs::path picture = picture_file(o, g.romset);
    const auto keep_screen = [&] {
        int w = 0, h = 0;
        if (const uint32_t *px = played > 0 ? g.api->screen(core, &w, &h) : nullptr) save_picture(picture, px, w, h);
    };
    bool running = true, quit = false;
    bool paused = false; // the pause menu is open
    int item = 0;
    int button_row = -1;   // the controller buttons are open (draw_buttons): the row picked
    bool waiting = false;  // for the button to put on that row
    int last_link = -1;
    // The window title's speed, once a second: loop presents and game
    // frames a second, the game's as a share of the board's rate.
    SDL_Window *win = SDL_GetRenderWindow(r);
    uint64_t stats_start = SDL_GetPerformanceCounter();
    long stats_presents = 0, stats_frames = 0;
    const uint64_t run_start = stats_start; // --speed
    double slowest = 0;                     // --speed: the slowest second's game fps
    std::string corner; // the same in a corner in full screen, where there is no title bar (as on iPad)
    long counted = 0; // played, as far as count_speed has seen it
    // The game on its own thread (Model 1/2, which have no GL of their own):
    // it runs the frames at the board's rate and leaves each finished screen
    // for this thread to show, so a slow present never holds up the next
    // frame (a Raspberry Pi 4 near its limit ran two or three frames a
    // present instead). It is stopped while the pause menu is open, so the
    // menu's calls into the core never meet it. Input is sampled here and
    // handed over; the frames, sound, the 30-second picture and the link's
    // state are the thread's.
    const bool threaded = o.game_thread && !g.folder && g.platform <= 1;
    std::thread worker;
    std::mutex shared_m;              // guards the rest of the shared_ things
    afcore_input shared_in = ctl.sample(r);
    std::vector<uint32_t> shared_px;  // the newest finished screen
    int shared_w = 0, shared_h = 0;
    uint64_t shared_seq = 0, shown_seq = 0;
    std::string worker_error;         // the core stopped: why
    std::atomic<bool> stop_worker{false}, worker_done{false};
    const auto worker_loop = [&] {
        uint64_t wlast = SDL_GetPerformanceCounter();
        double wowed = 0;
        while (!stop_worker) {
            const uint64_t now = SDL_GetPerformanceCounter();
            wowed += (now - wlast) / freq * hz;
            wlast = now;
            if (o.frames > 0) wowed = 1; // tests: as fast as it goes
            if (wowed > 4) wowed = 1;    // a stall: skip ahead rather than race
            if (wowed < 1) {
                SDL_DelayPrecise(uint64_t((1 - wowed) / hz * 1e9));
                continue;
            }
            wowed -= 1;
            afcore_input in;
            {
                std::lock_guard<std::mutex> lock(shared_m);
                in = shared_in;
            }
            g.api->run_frame(core, &in);
            if (const char *why = g.api->error(core)) {
                std::lock_guard<std::mutex> lock(shared_m);
                worker_error = why;
                worker_done = true;
                return;
            }
            audio.push(g.api, core);
            if (++played == long(hz * 30) && o.frames == 0 && !fs::exists(picture)) keep_screen();
            int w = 0, h = 0;
            if (const uint32_t *px = g.api->screen(core, &w, &h)) {
                std::lock_guard<std::mutex> lock(shared_m);
                shared_px.assign(px, px + size_t(w) * size_t(h));
                shared_w = w, shared_h = h;
                ++shared_seq;
            }
            int id = 0, count = 0;
            const int link = o.link_port > 0 ? g.api->link_state(core, &id, &count) : AFCORE_LINK_OFF;
            if (link != last_link && o.link_port > 0) {
                static const char *names[] = {"off (set it in the game's test mode)", "waiting", "up", "lost"};
                if (link == AFCORE_LINK_UP) std::printf("insert-coin: link up, cabinet %d of %d\n", id, count);
                else std::printf("insert-coin: link %s\n", names[link]);
                last_link = link;
            }
            if (o.frames > 0 && ++frames >= o.frames) {
                worker_done = true;
                return;
            }
        }
    };
    const auto end_worker = [&] {
        if (!worker.joinable()) return;
        stop_worker = true;
        worker.join();
        stop_worker = false;
    };
    const auto count_speed = [&](long ran_frames) {
        ++stats_presents;
        stats_frames += ran_frames;
        const double span = (SDL_GetPerformanceCounter() - stats_start) / freq;
        if (span < 1) return;
        const double screen = stats_presents / span, game = stats_frames / span;
        char title[256];
        if (paused) std::snprintf(title, sizeof title, "%s \u2014 %.0f fps \u00b7 paused", g.title.c_str(), screen);
        else std::snprintf(title, sizeof title, "%s \u2014 %.0f fps \u00b7 game %.1f fps (%.0f%%)", g.title.c_str(), screen, game, 100 * game / hz);
        SDL_SetWindowTitle(win, title);
        char text[128]; // the debug font is ASCII only
        if (paused) std::snprintf(text, sizeof text, "%.0f fps - paused", screen);
        else std::snprintf(text, sizeof text, "%.0f fps - game %.1f fps (%.0f%%)", screen, game, 100 * game / hz);
        corner = text;
        if (o.speed && !paused) {
            const double at = (SDL_GetPerformanceCounter() - run_start) / freq;
            std::printf("speed: %s %6.1f s: screen %.0f fps, game %.1f fps (%.0f%%)\n", g.romset.c_str(), at, screen, game, 100 * game / hz);
            std::fflush(stdout);
            if (slowest == 0 || game < slowest) slowest = game;
        }
        stats_start = SDL_GetPerformanceCounter();
        stats_presents = stats_frames = 0;
    };
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) { running = false; quit = true; }
            if (e.type == SDL_EVENT_GAMEPAD_ADDED) ctl.open_pad();
            if (fullscreen_key(e, win, o.data)) continue;
            const bool key = e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat, padb = e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
            const auto is = [&](SDL_Scancode s, SDL_GamepadButton b) {
                return (key && e.key.scancode == s) || (padb && e.gbutton.button == b);
            };
            if (paused && button_row >= 0) { // the controller buttons
                buttons_event(e, ctl.map, buttons_file, button_row, waiting);
                continue;
            }
            const bool chord = padb && (e.gbutton.button == SDL_GAMEPAD_BUTTON_START || e.gbutton.button == SDL_GAMEPAD_BUTTON_BACK) && ctl.chord_down();
            if (chord || (key && e.key.scancode == SDL_SCANCODE_ESCAPE)) {
                paused = !paused;
                item = 0;
                button_row = -1;
                if (paused) end_worker(), g.api->save(core);
                if (audio.dev) { if (paused) SDL_PauseAudioDevice(audio.dev); else SDL_ResumeAudioDevice(audio.dev); }
            } else if (paused) {
                if (is(SDL_SCANCODE_UP, SDL_GAMEPAD_BUTTON_DPAD_UP)) item = std::max(0, item - 1);
                if (is(SDL_SCANCODE_DOWN, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) item = std::min(kMenuItems - 1, item + 1);
                const bool back = is(SDL_SCANCODE_BACKSPACE, SDL_GAMEPAD_BUTTON_EAST);
                const bool pick = is(SDL_SCANCODE_RETURN, SDL_GAMEPAD_BUTTON_SOUTH);
                if (pick && item == 4) {
                    button_row = 0;
                    waiting = false;
                } else if (pick && (item == 2 || item == 3)) { // the menu stays open to see the change
                    if (item == 2) look.shape = Shape((int(look.shape) + 1) % 3);
                    else look.effect = Effect((int(look.effect) + 1) % 3);
                    save_look(o, g.romset, look);
                } else if (pick && item == 6) {
                    set_fullscreen(win, !g_fullscreen, o.data);
                } else if (back || pick) {
                    if (!back && item == 1) {
                        ctl.operator_mode = !ctl.operator_mode;
                        ctl.operator_armed = false;
                    }
                    if (!back && item == 5) keep_screen();
                    if (!back && item == 7) running = false;
                    paused = false;
                    if (audio.dev) SDL_ResumeAudioDevice(audio.dev);
                }
            }
        }
        if (paused) {
            last = SDL_GetPerformanceCounter(); // no catching up on the time spent in the menu
            SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
            SDL_RenderClear(r);
            if (tex) draw_screen(r, tex, tex_w, tex_h, look, overlay, g.folder);
            if (button_row >= 0) draw_buttons(r, g, ctl.map, button_row, waiting);
            else draw_pause(r, g, item, ctl.operator_mode, look);
            SDL_RenderPresent(r);
            count_speed(0);
            continue;
        }
        if (threaded) {
            if (!worker.joinable() && !worker_done) worker = std::thread(worker_loop);
            const afcore_input in = ctl.sample(r);
            bool fresh = false;
            {
                std::lock_guard<std::mutex> lock(shared_m);
                shared_in = in;
                if (shared_seq != shown_seq && shared_w > 0) {
                    if (!tex || tex_w != shared_w || tex_h != shared_h) {
                        if (tex) SDL_DestroyTexture(tex);
                        tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, shared_w, shared_h);
                        tex_w = shared_w;
                        tex_h = shared_h;
                    }
                    SDL_UpdateTexture(tex, nullptr, shared_px.data(), shared_w * 4);
                    shown_seq = shared_seq;
                    fresh = true;
                }
            }
            if (worker_done) {
                end_worker();
                running = false;
                if (!worker_error.empty()) {
                    const std::string text = g.title + " stopped: its core can't run this yet.\n" + worker_error;
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Arcade Foundry: Insert Coin", text.c_str(), nullptr);
                }
                if (o.hash && !shared_px.empty())
                    std::printf("%s: %ld frames, last screen hash %016" PRIx64 "\n", g.romset.c_str(), frames.load(),
                                fnv(shared_px.data(), shared_px.size()));
            } else if (!fresh) {
                SDL_DelayNS(500000); // nothing new to show yet
                continue;
            }
        }
        const uint64_t now = SDL_GetPerformanceCounter();
        owed += (now - last) / freq * hz;
        last = now;
        if (o.frames > 0) owed = 1; // tests: one frame per loop, as fast as it goes
        if (owed > 4) owed = 1;     // a stall: skip ahead rather than race
        if (threaded) owed = 0;     // the game thread runs them
        bool ran = false;
        if (owed >= 1) gl.release();
        while (owed >= 1) {
            owed -= 1;
            const afcore_input in = ctl.sample(r);
            g.api->run_frame(core, &in);
            if (const char *why = g.api->error(core)) {
                const std::string text = g.title + " stopped: its core can't run this yet.\n" + why;
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Arcade Foundry: Insert Coin", text.c_str(), nullptr);
                running = false;
                break;
            }
            audio.push(g.api, core);
            ran = true;
            // A game without a picture takes one after half a minute (not in tests).
            if (++played == long(hz * 30) && o.frames == 0 && !fs::exists(picture)) keep_screen();
            if (o.frames > 0 && ++frames >= o.frames) running = false;
        }
        gl.take();
        if (ran) {
            int w = 0, h = 0;
            const uint32_t *px = g.api->screen(core, &w, &h);
            if (!tex || tex_w != w || tex_h != h) {
                if (tex) SDL_DestroyTexture(tex);
                // XRGB, so the screen is opaque whatever a core leaves in the alpha byte
                // (Model 1's polygons have 0 there, which blended them to black).
                tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
                tex_w = w;
                tex_h = h;
            }
            SDL_UpdateTexture(tex, nullptr, px, w * 4);
            if (!running && o.hash)
                std::printf("%s: %ld frames, last screen hash %016" PRIx64 "\n", g.romset.c_str(), frames.load(), fnv(px, size_t(w) * h));
        }
        int id = 0, count = 0;
        const int link = o.link_port > 0 && !threaded ? g.api->link_state(core, &id, &count) : AFCORE_LINK_OFF;
        if (link != last_link && o.link_port > 0 && !threaded) {
            static const char *names[] = {"off (set it in the game's test mode)", "waiting", "up", "lost"};
            if (link == AFCORE_LINK_UP) std::printf("insert-coin: link up, cabinet %d of %d\n", id, count);
            else std::printf("insert-coin: link %s\n", names[link]);
            last_link = link;
        }
        SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
        SDL_RenderClear(r);
        if (tex) draw_screen(r, tex, tex_w, tex_h, look, overlay, g.folder);
        if (tex && ctl.gun) draw_crosshairs(r, ctl);
        if (ctl.operator_mode) {
            SDL_SetRenderDrawColor(r, 255, 170, 0, 255);
            SDL_RenderDebugText(r, 8, 8, "OPERATOR MODE: gamepad A = Test, B = Service (or F2, F3). Pause to leave.");
        }
        if (!corner.empty() && (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN)) {
            int ow = 0, oh = 0;
            SDL_GetCurrentRenderOutputSize(r, &ow, &oh);
            const float tw = 8.f * float(corner.size());
            const SDL_FRect box{float(ow) - tw - 20, 8, tw + 12, 20};
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, 0, 0, 0, 128);
            SDL_RenderFillRect(r, &box);
            SDL_SetRenderDrawColor(r, 255, 255, 255, 204);
            SDL_RenderDebugText(r, box.x + 6, box.y + 6, corner.c_str());
        }
        SDL_RenderPresent(r);
        const long now_played = played;
        count_speed(now_played - counted);
        counted = now_played;
    }
    end_worker();
    SDL_SetWindowTitle(win, "Arcade Foundry: Insert Coin");
    if (o.speed && played > 0) {
        const double span = (SDL_GetPerformanceCounter() - run_start) / freq;
        std::printf("speed: %s total: %ld frames in %.1f s, game %.1f fps (%.0f%% of %.2f), slowest second %.1f fps\n",
                    g.romset.c_str(), played.load(), span, played / span, 100 * played / span / hz, hz, slowest);
        std::fflush(stdout);
    }
    if (tex) SDL_DestroyTexture(tex);
    overlay.drop();
    audio.close();
    gl.release();
    g.api->destroy(core); // saves the settings
    gl.take();
    return !quit;
}

// Why a version can't be played (no core, an out-of-date one, no ROM set),
// or its set and core when it can.
std::string status(const Game &g, const Options &o) {
    return g.outdated ? "core for interface " + version_string(g.outdated) + ": rebuild it with scripts/build_cores.py"
           : !g.api ? "not in this build: scripts/build_cores.py with its ROM set"
           : g.rom.empty() && g.folder ? "no game: put its folder (" + g.romset + "/, holding disk0) or " + g.romset + ".tar in " + o.roms.string()
           : g.rom.empty() ? "no ROM set: put " + g.romset + ".zip in " + o.roms.string()
                           : g.romset + "  core " + version_string(g.api->core_version);
}

// The game list: a header with the platform and order chosen, a row per
// game (scrolled to keep the chosen one in sight, at any window size, with
// how many more there are above and below), the buttons' hints at the bottom.
void draw_menu(SDL_Renderer *r, const std::vector<ArcadeGame> &games, size_t sel, const Options &o, Picture &pic,
               const LibrarySettings &lib) {
    SDL_SetRenderDrawColor(r, BACKGROUND.r, BACKGROUND.g, BACKGROUND.b, 255);
    SDL_RenderClear(r);
    int ow = 0, oh = 0;
    SDL_GetCurrentRenderOutputSize(r, &ow, &oh);
    if (!games.empty()) { // the first of its versions' pictures
        std::error_code ec;
        const Game *with = games[sel].versions[0];
        for (const Game *v : games[sel].versions)
            if (fs::exists(picture_file(o, v->romset), ec)) { with = v; break; }
        if (SDL_Texture *t = pic.get(r, picture_file(o, with->romset), with->romset)) {
            const float w = ow * 0.5f, h = w * 3 / 4; // the right half, 4:3
            const SDL_FRect dst{ow - w - 24, (oh - h) / 2, w, h};
            SDL_RenderTexture(r, t, nullptr, &dst);
        }
    }
    SDL_SetRenderScale(r, 2, 2);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_RenderDebugText(r, 24, 16, lib.playable_only ? "ARCADE FOUNDRY: INSERT COIN " INSERT_COIN_VERSION "   [PLAYABLE]"
                                                     : "ARCADE FOUNDRY: INSERT COIN " INSERT_COIN_VERSION "   [ALL]");
    const std::string view = std::string("Platform: ") + (lib.platform < 0 ? "All" : kPlatforms[lib.platform].name) +
                             "   Sort: " + kSortNames[int(lib.sort)];
    SDL_SetRenderDrawColor(r, 255, 255, 80, 255);
    SDL_RenderDebugText(r, 24, 30, view.c_str());
    if (games.empty()) {
        SDL_SetRenderDrawColor(r, 140, 140, 150, 255);
        const std::string none = lib.platform < 0 ? "No games to play yet: put ROM sets in " + o.roms.string()
                                                  : std::string("No ") + kPlatforms[lib.platform].name + " games to play yet";
        SDL_RenderDebugText(r, 24, 52, none.c_str());
    }
    const size_t rows = size_t(std::max(1, (oh / 2 - 52 - 24) / 30));
    const size_t top = games.size() <= rows ? 0 : std::min(sel > rows / 2 ? sel - rows / 2 : 0, games.size() - rows);
    for (size_t i = top; i < games.size() && i < top + rows; ++i) {
        const ArcadeGame &g = games[i];
        const float y = 52 + 30 * float(i - top);
        const bool ok = g.playable();
        const size_t n = g.versions.size();
        SDL_SetRenderDrawColor(r, ok ? 255 : 120, ok ? 255 : 120, ok ? (i == sel ? 80 : 255) : 120, 255);
        const std::string line = std::string(i == sel ? "> " : "  ") + g.title + "  (" +
                                 (n > 1 ? std::to_string(n) + " versions" : g.versions[0]->detail) + ")";
        SDL_RenderDebugText(r, 24, y, line.c_str());
        std::string why = "    ";
        if (n == 1) why += status(*g.versions[0], o);
        else { // "daytona93, daytona: 1 of 2 playable"
            size_t playable = 0;
            for (size_t v = 0; v < n; ++v) {
                why += (v ? ", " : "") + g.versions[v]->romset;
                playable += g.versions[v]->playable();
            }
            why += ": " + std::to_string(playable) + " of " + std::to_string(n) + " playable";
        }
        SDL_SetRenderDrawColor(r, 140, 140, 150, 255);
        SDL_RenderDebugText(r, 24, y + 11, why.c_str());
    }
    // More games above or below the ones in sight.
    SDL_SetRenderDrawColor(r, 140, 140, 150, 255);
    if (top > 0) SDL_RenderDebugText(r, 24, 41, ("^ " + std::to_string(top) + " more above").c_str());
    if (top + rows < games.size())
        SDL_RenderDebugText(r, 24, 52 + 30 * float(rows) - 6, ("v " + std::to_string(games.size() - top - rows) + " more below").c_str());
    // The hints at the window's own size, naming the gamepad's buttons as on the Mac.
    SDL_SetRenderScale(r, 1, 1);
    SDL_SetRenderDrawColor(r, 140, 140, 150, 255);
    SDL_RenderDebugText(r, 48, oh - 36.f, lib.playable_only
                                              ? "Up/Down choose   Return (South) open   P (North) show all   C (West) credits   Esc quit"
                                              : "Up/Down choose   Return (South) open   P (North) only playable   C (West) credits   Esc quit");
    SDL_RenderDebugText(r, 48, oh - 22.f, "Left/Right (L1 R1) platform   S, Shift+S (R2 L2) sort   F11 full screen");
    SDL_RenderPresent(r);
}

// Text for SDL's debug font, which has ASCII only: the middle dot as a
// dash, accented letters without their accents, anything else as '?'.
std::string ascii(const std::string &utf8) {
    static const std::map<std::string, std::string> kAs = {{"·", "-"}, {"–", "-"}, {"—", "-"}, {"’", "'"},
                                                           {"“", "\""}, {"”", "\""}, {"é", "e"}, {"ü", "u"}};
    std::string out;
    for (size_t i = 0; i < utf8.size();) {
        const unsigned char c = utf8[i];
        const size_t n = c < 0x80 ? 1 : c >= 0xf0 ? 4 : c >= 0xe0 ? 3 : 2;
        const auto it = kAs.find(utf8.substr(i, n));
        out += n == 1 ? std::string(1, char(c)) : it != kAs.end() ? it->second : "?";
        i += n;
    }
    return out;
}

// `text` in lines of at most `cols` characters, broken between words; the
// next lines indented as the first.
std::vector<std::string> wrap(std::string text, size_t cols) {
    std::vector<std::string> out;
    const std::string indent(std::min(text.find_first_not_of(' '), cols / 2), ' ');
    while (text.size() > cols) {
        size_t cut = text.rfind(' ', cols);
        if (cut == std::string::npos || cut <= indent.size()) cut = cols;
        out.push_back(text.substr(0, cut));
        text = indent + text.substr(text[cut] == ' ' ? cut + 1 : cut);
    }
    out.push_back(text);
    return out;
}

// A game's history for its page, from history/<romset>.md built in (one
// file per game, named after any of its versions), as the Apple app's
// GameHistory: "# Title", a facts line (developer, year, board), paragraphs,
// and under "## Read more" links ("- [Label](url)"), kept as "Label: url".
struct History {
    std::string facts;
    std::vector<std::string> paragraphs, links;

    static History load(const ArcadeGame &game) {
        History h;
        for (const Game *v : game.versions)
            for (const HistoryFile &f : HISTORY_MD)
                if (h.facts.empty() && v->romset == f.romset) h.parse(f.text);
        return h;
    }
    void parse(const std::string &text) {
        std::vector<std::string> blocks;
        std::string current;
        bool in_links = false;
        size_t at = 0;
        while (at <= text.size()) {
            size_t end = text.find('\n', at);
            if (end == std::string::npos) end = text.size();
            std::string line = text.substr(at, end - at);
            at = end + 1;
            line.erase(0, line.find_first_not_of(" \t\r"));
            line.erase(line.find_last_not_of(" \t\r") + 1);
            if (line.empty() || line.rfind("## ", 0) == 0) {
                if (!current.empty()) blocks.push_back(current);
                current.clear();
                in_links = in_links || !line.empty();
            } else if (line.rfind("# ", 0) == 0) {
                continue;
            } else if (in_links) {
                const size_t open = line.find('['), mid = line.find("](");
                if (open != std::string::npos && mid != std::string::npos && open < mid && line.back() == ')')
                    links.push_back(line.substr(open + 1, mid - open - 1) + ": " + line.substr(mid + 2, line.size() - mid - 3));
            } else {
                current += (current.empty() ? "" : " ") + line;
            }
        }
        if (!current.empty()) blocks.push_back(current);
        if (blocks.empty()) return;
        facts = blocks[0];
        paragraphs.assign(blocks.begin() + 1, blocks.end());
    }
};

// A game's page (Return or South on a game in the list), as the Mac's
// GameDetailView: the title and board; Game Versions, a row per version
// with why it can't be played, if it can't (Return or South plays it); the
// version's options as in its pause menu (Shape, Effect, Controller
// buttons, and in the gun games the crosshair), folded out under it with
// Right and back with Left, Esc or East (issue #231; a game with one version
// has them out); its controls; About this game, its history. Up/Down move
// through the rows, then on down the history half a window at a time; Esc
// or East goes back to the list.
const int kPageOptions = 3;

struct GamePage {
    ArcadeGame game;
    History history;
    int item = 0;    // the row chosen (rows())
    int version = 0; // the version that row belongs to: the options and controls shown are its
    int open = -1;   // the version whose options are folded out, or -1
    int extra = 0;   // lines scrolled on past the last row, into the history
    int max_extra = 0; // as far as that goes, and
    int step = 1;      // half the lines in sight: how far Up or Down scrolls (draw_page)
    Look look;       // the version's screen settings
    bool crosshair = true; // its crosshair (shows_crosshair)
    ButtonMap map;   // and buttons
    int button_row = -1; // the controller buttons are open (draw_buttons): the row picked
    bool waiting = false;

    struct Row {
        int version, option; // option -1: the version's own row
    };
    const Game &chosen() const { return *game.versions[version]; }
    bool folds() const { return game.versions.size() > 1; }
    int options(int v) const {
        const Game &g = *game.versions[v];
        return g.api ? kPageOptions + (shows_crosshair(g) ? 1 : 0) : 0;
    }
    // each version's row, its options under it when folded out
    std::vector<Row> rows() const {
        std::vector<Row> out;
        for (int v = 0; v < int(game.versions.size()); ++v) {
            out.push_back({v, -1});
            if (open == v)
                for (int k = 0; k < options(v); ++k) out.push_back({v, k});
        }
        return out;
    }
    int items() const { return int(rows().size()); }
    Row row() const { return rows()[size_t(item)]; }
    int index_of(int v, int option) const {
        const std::vector<Row> all = rows();
        for (size_t i = 0; i < all.size(); ++i)
            if (all[i].version == v && all[i].option == option) return int(i);
        return 0;
    }
    fs::path buttons_file(const Options &o) const { return o.data / "buttons" / chosen().romset; }
    void reload(const Options &o) {
        look = game_look(o, chosen().romset);
        crosshair = crosshair_on(o, chosen().romset);
        map = ButtonMap::load(buttons_file(o));
    }
};

// A game's controls on the keyboard and a gamepad, as Controls reads them
// (keep the two in step; ControlsHelp.swift is the Apple app's list).
struct ControlLine {
    const char *control, *keyboard, *gamepad;
};

std::vector<ControlLine> control_lines(const Game &g) {
    static const std::vector<ControlLine> driving = {
        {"Steer", "Left, Right", "Left stick"},
        {"Accelerate, brake", "Up, Down", "Right, left trigger"},
        {"Gear up, down", "A, Z", "Right, left shoulder"},
        {"Views 1-4", "1-4", "South, East, West, North (A, B, X, Y)"},
    };
    static const std::vector<ControlLine> gun = {
        {"Aim", "Mouse", "Right or left stick"},
        {"Fire", "Click or Space", "Right trigger or South (A)"},
        {"Reload", "Right-click", "East (B)"},
        {"Player 2", "", "A second gamepad (its Start joins)"},
    };
    static const std::vector<ControlLine> fighting = {
        {"Move", "Arrows", "Left stick or d-pad"},
        {"Button 1 (punch)", "A", "West (X)"},
        {"Button 2 (kick)", "S", "South (A)"},
        {"Button 3 (guard)", "D", "East (B)"},
        {"Button 4", "F", "North (Y)"},
        {"Player 2", "", "A second gamepad"},
    };
    static const std::vector<ControlLine> flight = {
        {"Stick", "Arrows", "Left stick"},
        {"Throttle", "A opens, Z closes", "Right trigger"},
        {"Fire", "Space", "South (A) or right shoulder"},
        {"Other buttons", "X, C", "East, West (B, X)"},
        {"Second trigger", "V", "Left shoulder"},
        {"Views", "1-4", "North (Y)"},
        {"Player 2 start", "6", ""},
    };
    static const std::vector<ControlLine> twin = {
        {"Left lever", "W A S D", "Left stick or d-pad"},
        {"Left shot, dash", "Q, E", "Left trigger, shoulder"},
        {"Right lever", "I J K L", "Right stick"},
        {"Right shot, dash", "U, O", "Right trigger, shoulder"},
    };
    static const std::vector<ControlLine> fishing = {
        {"Rod", "", "Left stick"},
        {"Reel", "", "Right trigger"},
        {"Line tension", "", "Left trigger"},
        {"Lure", "", "D-pad"},
        {"Cast, select", "", "West, South (X, A)"},
    };
    static const std::vector<ControlLine> cabinet = {
        {"Coin", "5", "Back (Select)"},
        {"Start", "Return", "Start"},
        {"Pause", "Esc", "Back and Start together"},
        {"Test, Service", "F2, F3", "South, East in operator mode"},
        {"Full screen", "F11 or Alt+Enter", "Full screen in the pause menu"},
    };
    static const char *const fighters[] = {"doab", "vf2", "fvipers", "vf", "vstriker", "zerogun", "pltkids", "dynamcop", "lastbrnx",
                                           "schamp", "dynabb97", "vf3", "fvipers2", "spikeout", "spikeofe", "vs299a", "magtruck"};
    const bool fishes = g.romset == "bassdx" || g.romset == "getbass" || g.romset == "getbassur" || g.romset == "getbassdx";
    const bool fights = std::any_of(std::begin(fighters), std::end(fighters), [&](const char *id) { return g.romset == id; });
    std::vector<ControlLine> lines = g.flight ? flight : g.twin ? twin : is_gun_game(g) ? gun : fishes ? fishing : fights ? fighting : driving;
    if (g.romset == "hotd4a") lines.insert(lines.end() - 1, {"Grenade", "Middle-click or G", "West (X)"}); // before Player 2
    lines.insert(lines.end(), cabinet.begin(), cabinet.end());
    return lines;
}

// The page as lines of text at twice the size, scrolled to keep the row
// chosen in sight (and on by `extra`).
void draw_page(SDL_Renderer *r, GamePage &page, const Options &o) {
    struct Line {
        std::string text;
        SDL_Color color;
        int item = -1; // the row it belongs to
    };
    const SDL_Color white{255, 255, 255, 255}, grey{140, 140, 150, 255}, dim{120, 120, 120, 255}, chosen{255, 255, 80, 255};
    SDL_SetRenderDrawColor(r, BACKGROUND.r, BACKGROUND.g, BACKGROUND.b, 255);
    SDL_RenderClear(r);
    int ow = 0, oh = 0;
    SDL_GetCurrentRenderOutputSize(r, &ow, &oh);
    const size_t cols = size_t(std::max(20, (ow / 2 - 48) / SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE));
    std::vector<Line> lines;
    const auto add = [&](const std::string &text, SDL_Color color, int item = -1) {
        for (const std::string &l : wrap(ascii(text), cols)) lines.push_back({l, color, item});
    };
    add(page.game.title, white);
    add(kPlatforms[page.game.platform].name, grey);
    add("", grey);
    add("GAME VERSIONS", white);
    const std::vector<GamePage::Row> all = page.rows();
    for (int i = 0; i < int(all.size()); ++i) {
        const GamePage::Row row = all[size_t(i)];
        const Game &v = *page.game.versions[size_t(row.version)];
        const bool on = page.item == i;
        if (row.option < 0) {
            std::string hint = v.playable() ? "   [Play]" : "";
            if (page.folds() && page.options(row.version)) hint += page.open == row.version ? "   [< settings]" : "   [settings >]";
            add(std::string(on ? "> " : "  ") + v.detail + hint, on ? chosen : v.playable() ? white : dim, i);
            add("    " + (v.playable() ? "" : v.romset + ": ") + status(v, o), grey, i);
            continue;
        }
        // the open version's options: the page holds the chosen version's, and the cursor is on one of them
        const std::string text[kPageOptions + 1] = {std::string("Shape: ") + Look::name(page.look.shape),
                                                    std::string("Effect: ") + Look::name(page.look.effect),
                                                    std::string("Controller buttons: ") + (page.map.is_default() ? "default" : "changed"),
                                                    std::string("Crosshair (gamepad aim): ") + (page.crosshair ? "on" : "off")};
        add(std::string(on ? "    > " : "      ") + text[row.option], on ? chosen : white, i);
    }
    add("", grey);
    add("CONTROLS", white);
    for (const ControlLine &c : control_lines(page.chosen())) {
        std::string text = std::string("  ") + c.control + ":";
        if (*c.keyboard) text += std::string("  keys ") + c.keyboard;
        if (*c.keyboard && *c.gamepad) text += ";";
        if (*c.gamepad) text += std::string("  gamepad ") + c.gamepad;
        add(text, grey);
    }
    if (!page.map.is_default()) add("  (Controller buttons changed for this version: see its settings)", grey);
    if (!page.history.facts.empty()) {
        add("", grey);
        add("ABOUT THIS GAME", white);
        add(page.history.facts, grey);
        for (const std::string &p : page.history.paragraphs) {
            add("", grey);
            add(p, white);
        }
        if (!page.history.links.empty()) {
            add("", grey);
            add("Read more:", grey);
            for (const std::string &l : page.history.links) add("  " + l, grey);
        }
    }
    // The first line shown: the chosen row a third of the way down, then `extra` more.
    const int rows = std::max(1, (oh / 2 - 20 - 24) / 12), total = int(lines.size());
    const int last_top = std::max(0, total - rows);
    int at = 0;
    while (at < total && lines[at].item != page.item) ++at;
    const int base = std::clamp(at - rows / 3, 0, last_top);
    page.max_extra = last_top - base;
    page.step = std::max(1, rows / 2);
    page.extra = std::min(page.extra, page.max_extra);
    const int top = base + page.extra;
    if (page.button_row >= 0) { // the controller buttons, on their own
        draw_buttons(r, page.chosen(), page.map, page.button_row, page.waiting);
        SDL_RenderPresent(r);
        return;
    }
    SDL_SetRenderScale(r, 2, 2);
    for (int i = 0; i < rows && top + i < total; ++i) {
        const Line &l = lines[top + i];
        SDL_SetRenderDrawColor(r, l.color.r, l.color.g, l.color.b, 255);
        SDL_RenderDebugText(r, 24, 20 + 12 * float(i), l.text.c_str());
    }
    SDL_SetRenderScale(r, 1, 1);
    SDL_SetRenderDrawColor(r, 140, 140, 150, 255);
    SDL_RenderDebugText(r, 48, oh - 22.f, page.folds() ? "Up/Down choose   Right/Left settings   Return (South) play or change   Esc (East) back"
                                                  : "Up/Down choose (and read on)   Return (South) play or change   Esc (East) back to the list");
    SDL_RenderPresent(r);
}

// CREDITS.md as plain text: headings in capitals, bold marks dropped,
// [label](url) as "label (url)". Lines are wrapped at `cols` (0: not
// wrapped), a list item's next lines indented under its text.
struct CreditLine {
    std::string text;
    bool heading;
};

std::vector<CreditLine> credits_lines(size_t cols) {
    std::vector<CreditLine> out;
    std::string md = CREDITS_MD;
    size_t start = 0;
    while (start <= md.size()) {
        size_t end = md.find('\n', start);
        if (end == std::string::npos) end = md.size();
        std::string line = md.substr(start, end - start);
        start = end + 1;
        bool heading = false;
        if (line.rfind("## ", 0) == 0) line = line.substr(3), heading = true;
        else if (line.rfind("# ", 0) == 0) line = line.substr(2), heading = true;
        std::string text;
        for (size_t i = 0; i < line.size(); ++i) {
            if (line.compare(i, 2, "**") == 0) { ++i; continue; }
            if (line[i] == '[') {
                const size_t mid = line.find("](", i), close = mid == std::string::npos ? mid : line.find(')', mid);
                if (close != std::string::npos) {
                    text += line.substr(i + 1, mid - i - 1) + " (" + line.substr(mid + 2, close - mid - 2) + ")";
                    i = close;
                    continue;
                }
            }
            text += line[i];
        }
        if (heading) for (char &c : text) c = char(std::toupper((unsigned char)c));
        const std::string indent = text.rfind("- ", 0) == 0 ? "  " : "";
        while (cols && text.size() > cols) {
            size_t cut = text.rfind(' ', cols);
            if (cut == std::string::npos || cut <= indent.size()) cut = cols;
            out.push_back({text.substr(0, cut), heading});
            text = indent + text.substr(text[cut] == ' ' ? cut + 1 : cut);
        }
        out.push_back({text, heading});
    }
    while (!out.empty() && out.back().text.empty()) out.pop_back();
    return out;
}

// The credits screen (C, or the gamepad's West button, from the list; Esc,
// East or Back to leave):
// `top` is the first line shown; returns the most it can be.
size_t draw_credits(SDL_Renderer *r, size_t &top) {
    SDL_SetRenderDrawColor(r, BACKGROUND.r, BACKGROUND.g, BACKGROUND.b, 255);
    SDL_RenderClear(r);
    int ow = 0, oh = 0;
    SDL_GetCurrentRenderOutputSize(r, &ow, &oh);
    SDL_SetRenderScale(r, 2, 2);
    const auto lines = credits_lines(std::max(20, (ow / 2 - 48) / SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE));
    const size_t rows = size_t(std::max(1, (oh / 2 - 60) / 12));
    const size_t last = lines.size() > rows ? lines.size() - rows : 0;
    top = std::min(top, last);
    for (size_t i = 0; i < rows && top + i < lines.size(); ++i) {
        const CreditLine &line = lines[top + i];
        if (line.heading) SDL_SetRenderDrawColor(r, 255, 255, 80, 255);
        else SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        SDL_RenderDebugText(r, 24, 20 + 12 * float(i), line.text.c_str());
    }
    SDL_SetRenderDrawColor(r, 140, 140, 150, 255);
    SDL_RenderDebugText(r, 24, oh / 2.f - 24, "Up/Down scroll, Esc (or East) back");
    SDL_SetRenderScale(r, 1, 1);
    SDL_RenderPresent(r);
    return last;
}

} // namespace

int main(int argc, char **argv) {
    // The window title has an em dash and a middle dot ("Daytona USA — 60 fps
    // · game 57.5 fps"): SDL's X11 title needs a UTF-8 character type, or the
    // title bar keeps the first title. Only LC_CTYPE, so numbers stay "57.5".
    const char *ctype = std::setlocale(LC_CTYPE, "");
    if (!ctype || (!std::strstr(ctype, "UTF-8") && !std::strstr(ctype, "utf8"))) std::setlocale(LC_CTYPE, "C.UTF-8");
    Options o;
    o.data = data_dir();
    o.roms = o.data / "roms";
    o.cores = exe_dir() / "cores";
    if (!fs::exists(o.cores)) o.cores = exe_dir() / "../build/cores/linux";
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const bool more = i + 1 < argc;
        if (a == "--roms" && more) o.roms = argv[++i];
        else if (a == "--cores" && more) o.cores = argv[++i];
        else if (a == "--fullscreen") o.fullscreen = true;
        else if (a == "--link-port" && more) o.link_port = std::atoi(argv[++i]);
        else if (a == "--link-next" && more) o.link_next = argv[++i];
        else if (a == "--link-sync") o.link_sync = true;
        else if (a == "--play" && more) o.play = argv[++i];
        else if (a == "--frames" && more) o.frames = std::atol(argv[++i]);
        else if (a == "--screenshot-hash") o.hash = true;
        else if (a == "--speed") o.speed = true;
        else if (a == "--no-game-thread") o.game_thread = false;
        else if ((a == "--shape" || a == "--effect") && more) {
            if (!o.look.set(a.substr(2), argv[++i])) {
                std::fprintf(stderr, "insert-coin: %s: no such %s\n", argv[i], a.c_str() + 2);
                return 2;
            }
        } else if (a == "--sharp") o.look.smooth = false;
        else if (a == "--size" && more) o.look.zoom = std::clamp(std::atof(argv[++i]) / 100, 0.8, 1.2);
        else if (a == "--move" && more) {
            double x = 0, y = 0;
            std::sscanf(argv[++i], "%lf,%lf", &x, &y);
            o.look.shift_x = std::clamp(x / 100, -0.1, 0.1);
            o.look.shift_y = std::clamp(y / 100, -0.1, 0.1);
        }
        else if (a == "--credits") {
            for (const CreditLine &line : credits_lines(0)) std::printf("%s\n", line.text.c_str());
            return 0;
        } else if (a == "--version") {
            std::printf("insert-coin %s (core interface %d.%d.%d)\n", INSERT_COIN_VERSION, AFCORE_API_MAJOR, AFCORE_API_MINOR,
                        AFCORE_API_PATCH);
            return 0;
        } else {
            std::fprintf(stderr, "usage: insert-coin [--roms DIR] [--cores DIR] [--fullscreen] [--shape original|stretch|integer] [--effect none|scanlines|crt] [--sharp] [--size PERCENT] [--move X,Y] [--link-port PORT --link-next HOST:PORT [--link-sync]] [--play ROMSET] [--frames N --screenshot-hash] [--speed] [--no-game-thread] [--version] [--credits]\n");
            return 2;
        }
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        std::fprintf(stderr, "insert-coin: %s\n", SDL_GetError());
        return 1;
    }
    g_fullscreen = o.fullscreen || fullscreen_saved(o.data);
    SDL_Window *win = SDL_CreateWindow("Arcade Foundry: Insert Coin", 992, 768, SDL_WINDOW_RESIZABLE | (g_fullscreen ? SDL_WINDOW_FULLSCREEN : 0));
    SDL_Renderer *r = win ? SDL_CreateRenderer(win, nullptr) : nullptr;
    if (!r) {
        std::fprintf(stderr, "insert-coin: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetRenderVSync(r, 1);
    Controls ctl;
    ctl.open_pad();
    migrate_renamed_sets(o.data);
    o.links = link_roms(o.roms, o.data / "rom-links");
    std::vector<Game> games = find_games(o);

    if (!o.play.empty()) { // straight into one game
        auto it = std::find_if(games.begin(), games.end(), [&](const Game &g) { return g.romset == o.play; });
        if (it == games.end() || !it->api || it->rom.empty()) {
            std::fprintf(stderr, "insert-coin: %s: no core or no ROM set (cores %s, ROMs %s)\n", o.play.c_str(),
                         o.cores.c_str(), o.roms.c_str());
            return 1;
        }
        play(r, *it, o, ctl);
    } else {
        size_t sel = 0;
        Picture pic;
        bool running = true;
        // All the games, or only those with a core and a ROM set; one
        // platform's or all; in an order (LibrarySettings).
        const fs::path library_file = o.data / "library";
        LibrarySettings lib;
        lib.load(library_file);
        const std::vector<ArcadeGame> all = group_games(games);
        std::vector<ArcadeGame> shown;
        // The list again after a change, the chosen game kept when it is still in it.
        const auto filter = [&] {
            const std::string was = sel < shown.size() ? shown[sel].key() : "";
            shown.clear();
            for (const ArcadeGame &g : all)
                if ((!lib.playable_only || g.playable()) && (lib.platform < 0 || g.platform == lib.platform)) shown.push_back(g);
            sort_games(shown, lib.sort, last_played(o.data));
            sel = 0;
            for (size_t i = 0; i < shown.size(); ++i)
                if (shown[i].key() == was) sel = i;
        };
        filter();
        bool credits = false;
        size_t credits_top = 0, credits_last = 0;
        std::optional<GamePage> page; // a game's page is open
        bool l2_held = false, r2_held = false; // the triggers are axes: a press is one going past three quarters
        while (running) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_QUIT) running = false;
                if (e.type == SDL_EVENT_GAMEPAD_ADDED) ctl.open_pad();
                if (fullscreen_key(e, win, o.data)) continue;
                const bool key = e.type == SDL_EVENT_KEY_DOWN, padb = e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
                const auto is = [&](SDL_Scancode s, SDL_GamepadButton b) {
                    return (key && e.key.scancode == s) || (padb && e.gbutton.button == b);
                };
                int trigger = 0; // -1 L2, 1 R2
                if (e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION &&
                    (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)) {
                    const bool left = e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
                    bool &held = left ? l2_held : r2_held;
                    if (!held && e.gaxis.value > 24000) held = true, trigger = left ? -1 : 1;
                    else if (held && e.gaxis.value < 8000) held = false;
                }
                if (page && page->button_row >= 0) { // its controller buttons
                    buttons_event(e, page->map, page->buttons_file(o), page->button_row, page->waiting);
                    continue;
                }
                if (page) {
                    if (is(SDL_SCANCODE_UP, SDL_GAMEPAD_BUTTON_DPAD_UP)) {
                        if (page->extra > 0) page->extra = std::max(0, page->extra - page->step);
                        else page->item = std::max(0, page->item - 1);
                    }
                    if (is(SDL_SCANCODE_DOWN, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {
                        if (page->item < page->items() - 1) ++page->item;
                        else page->extra = std::min(page->extra + page->step, page->max_extra);
                    }
                    // Right folds out the version's settings, Left (or Esc, East) folds them back
                    const GamePage::Row at = page->row();
                    if (page->folds() && is(SDL_SCANCODE_RIGHT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) && at.option < 0 && page->options(at.version)) {
                        page->open = at.version;
                        page->item = page->index_of(at.version, 0);
                        page->extra = 0;
                    } else if (page->folds() && page->open >= 0 &&
                               (is(SDL_SCANCODE_LEFT, SDL_GAMEPAD_BUTTON_DPAD_LEFT) || is(SDL_SCANCODE_ESCAPE, SDL_GAMEPAD_BUTTON_EAST) ||
                                (key && e.key.scancode == SDL_SCANCODE_BACKSPACE))) {
                        page->item = page->index_of(page->open, -1);
                        page->open = -1;
                        page->extra = 0;
                        continue; // not back to the list
                    }
                    if (page->row().version != page->version) { // the options and controls follow the row chosen
                        page->version = page->row().version;
                        page->reload(o);
                    }
                    const int option = page->row().option;
                    if (is(SDL_SCANCODE_RETURN, SDL_GAMEPAD_BUTTON_SOUTH) && !(key && e.key.repeat)) {
                        if (option < 0 && page->chosen().playable()) {
                            running = play(r, page->chosen(), o, ctl);
                            page->reload(o); // its pause menu may have changed them
                            if (lib.sort == Sort::recent) filter(); // the game just played comes first
                        } else if (option == 0 || option == 1) {
                            if (option == 0) page->look.shape = Shape((int(page->look.shape) + 1) % 3);
                            else page->look.effect = Effect((int(page->look.effect) + 1) % 3);
                            save_look(o, page->chosen().romset, page->look);
                        } else if (option == 2) {
                            page->button_row = 0;
                            page->waiting = false;
                        } else if (option == 3) {
                            page->crosshair = !page->crosshair;
                            save_crosshair(o, page->chosen().romset, page->crosshair);
                        }
                    }
                    if (is(SDL_SCANCODE_ESCAPE, SDL_GAMEPAD_BUTTON_EAST) || (key && e.key.scancode == SDL_SCANCODE_BACKSPACE)) page.reset();
                    continue;
                }
                if (credits) {
                    if (is(SDL_SCANCODE_UP, SDL_GAMEPAD_BUTTON_DPAD_UP) && credits_top > 0) --credits_top;
                    if (is(SDL_SCANCODE_DOWN, SDL_GAMEPAD_BUTTON_DPAD_DOWN) && credits_top < credits_last) ++credits_top;
                    if (is(SDL_SCANCODE_ESCAPE, SDL_GAMEPAD_BUTTON_EAST) || is(SDL_SCANCODE_C, SDL_GAMEPAD_BUTTON_BACK))
                        credits = false;
                    continue;
                }
                if (is(SDL_SCANCODE_C, SDL_GAMEPAD_BUTTON_WEST)) {
                    credits = true;
                    credits_top = 0;
                    continue;
                }
                if (is(SDL_SCANCODE_ESCAPE, SDL_GAMEPAD_BUTTON_BACK)) running = false;
                // The platform: All, Classic 2D, Model 1 ... Lindbergh, round to All again; the order likewise.
                int platform_step = 0, sort_step = trigger;
                if (is(SDL_SCANCODE_LEFT, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) platform_step = -1;
                if (is(SDL_SCANCODE_RIGHT, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) platform_step = 1;
                if (key && e.key.scancode == SDL_SCANCODE_S) sort_step = (e.key.mod & SDL_KMOD_SHIFT) ? -1 : 1;
                const bool playable_step = is(SDL_SCANCODE_P, SDL_GAMEPAD_BUTTON_NORTH);
                if (playable_step || platform_step || sort_step) {
                    if (playable_step) lib.playable_only = !lib.playable_only;
                    lib.platform = (lib.platform + 1 + platform_step + kPlatformCount + 1) % (kPlatformCount + 1) - 1;
                    lib.sort = Sort((int(lib.sort) + sort_step + kSortCount) % kSortCount);
                    filter();
                    lib.save(library_file);
                }
                if (shown.empty()) continue;
                if (is(SDL_SCANCODE_UP, SDL_GAMEPAD_BUTTON_DPAD_UP)) sel = (sel + shown.size() - 1) % shown.size();
                if (is(SDL_SCANCODE_DOWN, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) sel = (sel + 1) % shown.size();
                if (is(SDL_SCANCODE_RETURN, SDL_GAMEPAD_BUTTON_SOUTH) && !(key && e.key.repeat)) {
                    page = GamePage{shown[sel], History::load(shown[sel])};
                    if (!page->folds()) page->open = 0; // one version: its settings out
                    page->reload(o);
                }
            }
            if (running && page) draw_page(r, *page, o);
            else if (running && credits) credits_last = draw_credits(r, credits_top);
            else if (running) draw_menu(r, shown, sel, o, pic, lib);
        }
        pic.drop();
    }
    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
