// The game list: the games this build knows (game_list.h), their cores and
// ROM sets, the list's order and filters, a game's page and the credits.
#pragma once

#include "art.h"
#include "input.h"
#include "render.h"
#include "states.h"

namespace insert_coin {

fs::path data_dir();

// The app's set and the other names its files may have (kOtherNames in
// game_list.h, from games.toml): MAME's parents holding a clone's whole
// set (hotd.7z for hotdo, doa.zip for doab), and the app's names for the Lindbergh games before it took MAME's (or2sp for
// outr2sdx, lgj for letsgoju, hotd4 for hotd4a).
std::vector<std::string> set_names(const std::string &set);

// The ROM sets in --roms, at its top or one folder down in a folder per
// board (DIR/Model2/srallyc.zip, DIR/Lindbergh/hotd4a.tar), found as
// scripts/rom_index.py finds them: <name>.zip, .7z or .tar, or a <name>/
// folder holding disk0 (or a PC-based game's program, runner.h); work folders skipped (working, temp, to-file...,
// names starting with _ or .); a name in two places: the copy at the top,
// then the first by path, with a warning. They are linked into one flat
// folder, dst (made again at each start), where each game's core also finds
// its parent and device sets beside it. In a board folder, a <name>/ folder
// holding one archive (RingEdge/initd8/<anything>.7z) is that archive under
// the folder's name (initd8.7z).
fs::path link_roms(const fs::path &roms, const fs::path &dst);

// The Lindbergh games took MAME's set names (2026-10-28): their saves, game
// folders, screen settings, buttons and pictures under the old names move to
// the new ones, once. ROM files under the old names are still found
// (set_names).
void migrate_renamed_sets(const fs::path &data);

fs::path exe_dir();

std::vector<Game> find_games(const Options &o);

// When each game was last started, for the Recently played order: lines of
// "<romset> <seconds since 1970>" in <data>/last-played.
std::map<std::string, double> last_played(const fs::path &data);

void mark_played(const fs::path &data, const std::string &romset);

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
std::vector<ArcadeGame> group_games(const std::vector<Game> &games);

// `games` in the list's order, as the Apple app's ArcadeGame.sorted: A-Z
// with "The House of the Dead" under H; Year, the same year A-Z; Platform,
// Model 1 to Lindbergh, each by year; Recently played (any of a game's
// versions), the games never played after the others, A-Z.
void sort_games(std::vector<ArcadeGame> &games, Sort sort, const std::map<std::string, double> &played);

// The game list's choices, remembered in <data>/library as "key=value"
// lines: all games or only the playable ones (P or North), the platform
// (-1 all; Left/Right or L1/R1) and the order (S or L2/R2).
struct LibrarySettings {
    bool playable_only = false;
    int platform = -1;
    Sort sort = Sort::title;

    void load(const fs::path &file);
    void save(const fs::path &file) const;
};

// Why a version can't be played (no core, an out-of-date one, no ROM set),
// or its set and core when it can.
std::string status(const Game &g, const Options &o);

// The game list: a header with the platform and order chosen, a row per
// game (scrolled to keep the chosen one in sight, at any window size, with
// how many more there are above and below), the buttons' hints at the bottom;
// the chosen game's picture (card_picture) behind the right half, its art
// asked for (Art::want_card).
void draw_menu(SDL_Renderer *r, const std::vector<ArcadeGame> &games, size_t sel, const Options &o, Art &art, Images &images,
               const LibrarySettings &lib);

// The set a game's picture is from: the first version with the player's
// own, else with downloaded art, else with a screen from the game, else the
// first; and the set its other art is for (the first version).
std::string picture_set(const ArcadeGame &game, const Options &o, const Art &art);

// A game's history for its page, from history/<romset>.md built in (one
// file per game, named after any of its versions), as the Apple app's
// GameHistory: "# Title", a facts line (developer, year, board), paragraphs,
// and under "## Read more" links ("- [Label](url)"), kept as "Label: url".
struct History {
    std::string facts;
    std::vector<std::string> paragraphs, links;

    static History load(const ArcadeGame &game);
    void parse(const std::string &text);
};

// A game's page (Return or South on a game in the list), as the Mac's
// GameDetailView: the title and board; Game Versions, a row per version
// with why it can't be played, if it can't (Return or South plays it); the
// version's options as in its pause menu (Shape, Effect, Controller
// buttons, and in the gun games the crosshair), folded out under it with
// Right and back with Left, Esc or East (issue #231; a game with one version
// has them out); its controls; About this game, its history. Up/Down move
// through the rows, then on down the history half a window at a time; Esc
// or East goes back to the list. The game's picture (card_picture) is at the
// top of the right half, its other art (kArtExtras, asked for when the page
// opens) in a row under it; I or North opens them full size (art_view).
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
    std::string region;    // its region (core_regions), or "" when it has none
    ButtonMap map;   // and buttons
    int button_row = -1; // the controller buttons are open (draw_buttons): the row picked
    bool waiting = false;
    int art_view = -1;   // the art is open full size: which of art_files (Left/Right step through)

    std::vector<bool> resumable; // each version has a Resume slot to offer (can_resume, states.h), from reload

    struct Row {
        int version, option; // option -1: the version's own row (Play), -2: Resume, above it
    };
    const Game &chosen() const { return *game.versions[version]; }
    bool folds() const { return game.versions.size() > 1; }
    int options(int v) const {
        const Game &g = *game.versions[v];
        if (g.pc) return 1; // its game folder; the runner has its own screen and controls for now
        return g.api ? kPageOptions + (shows_crosshair(g) ? 1 : 0) + (core_regions(g).empty() ? 0 : 1) : 0;
    }
    // what option row k of the chosen version is: 0 shape, 1 effect, 2
    // buttons, 3 crosshair, 4 region (rows only for what it has)
    int option_kind(int k) const {
        if (chosen().pc) return 5;
        if (k < kPageOptions) return k;
        if (k == kPageOptions && shows_crosshair(chosen())) return 3;
        return 4;
    }
    // each version's row, its options under it when folded out
    std::vector<Row> rows() const {
        std::vector<Row> out;
        for (int v = 0; v < int(game.versions.size()); ++v) {
            if (size_t(v) < resumable.size() && resumable[size_t(v)]) out.push_back({v, -2});
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
        region = region_of(o, chosen());
        map = ButtonMap::load(buttons_file(o));
        // the cursor stays on the same row (a version's Resume row comes and goes)
        const Row at = item < items() ? row() : Row{version, -1};
        resumable.clear();
        for (const Game *g : game.versions) resumable.push_back(can_resume(o, *g));
        item = index_of(at.version, at.option == -2 && !resumable[size_t(at.version)] ? -1 : at.option);
    }
};

// A game's controls on the keyboard and a gamepad, as Controls reads them
// (keep the two in step; ControlsHelp.swift is the Apple app's list).
struct ControlLine {
    const char *control, *keyboard, *gamepad;
};

std::vector<ControlLine> control_lines(const Game &g);

// The page as lines of text at twice the size, scrolled to keep the row
// chosen in sight (and on by `extra`).
void draw_page(SDL_Renderer *r, GamePage &page, const Options &o, const Art &art, Images &images);

// The page's pictures, in the order they are shown: the game's picture,
// then its other art this machine has, with their names.
std::vector<std::pair<fs::path, std::string>> art_files(const GamePage &page, const Options &o, const Art &art);

// The app's settings (issue #315), as the Apple app's Settings: every
// game's screen settings (a game's own, from its pause menu or its page,
// win), full screen, the speed readout, the game thread and Resume where I
// left off (states.h), kept in
// <data>/settings as "key=value" lines (the screen's as Look::str's pairs).
// Game art (art.h): Download game art, and Remove downloaded art (done by
// the caller, which holds the Art).
// The command line's options win over them for that run, and are not saved.
struct AppSettings {
    Look look;
    bool show_speed = true, game_thread = true, download_art = true;
    bool resume = false; // Resume where I left off, off until the player turns it on

    void load(const fs::path &file);
    void save(const fs::path &file) const;
    void apply(Options &o) const {
        o.look = look;
        o.show_speed = show_speed;
        o.game_thread = game_thread;
        o.resume = resume;
    }
};

// The Settings screen (Tab, or the gamepad's Start button, from the list):
// a row per setting under SCREEN, SPEED, GAME ART and ABOUT; Up/Down choose,
// Left/Right or Return (South) change, Return on Credits opens them, Esc
// (East) back.
enum class SettingRow { shape, effect, smooth, size, move_x, move_y, fullscreen, show_speed, game_thread, resume, download_art, remove_art, credits };

const int kSettingRows = 13;

// Changes row `row` one step (-1 back, 1 on) in `s`; full screen through
// set_fullscreen. Remove downloaded art and Credits have nothing to change.
void change_setting(AppSettings &s, SettingRow row, int step, SDL_Window *win, const fs::path &data);

// `art_note`: what Remove downloaded art last did, or what the art takes now.
void draw_settings(SDL_Renderer *r, const AppSettings &s, int item, const std::string &art_note);

// CREDITS.md as plain text: headings in capitals, bold marks dropped,
// [label](url) as "label (url)". Lines are wrapped at `cols` (0: not
// wrapped), a list item's next lines indented under its text.
struct CreditLine {
    std::string text;
    bool heading;
};

std::vector<CreditLine> credits_lines(size_t cols);

// The credits screen (C, or the gamepad's West button, from the list; Esc,
// East or Back to leave):
// `top` is the first line shown; returns the most it can be.
size_t draw_credits(SDL_Renderer *r, size_t &top);

} // namespace insert_coin
