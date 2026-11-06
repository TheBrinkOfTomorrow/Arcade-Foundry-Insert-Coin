// Arcade Foundry: Insert Coin for Linux: the same games as the Apple app, through SDL3. A
// list of the games this build has cores for (build/cores/linux), then the
// game in the window with its sound, keyboard and gamepad. The cores are
// opened at run time (dlopen), one shared library per game with the C
// interface in cores/arcade-foundry-cores-common/include/afcore.h.
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
// DIR/<romset>.tar, unpacked into ~/.local/share/arcade-foundry/games/ when first played;
// a Chihiro game: DIR/<romset>/ (the folder that holds its XBE), with MAME's
// chihiro BIOS set unzipped beside it as DIR/chihiro/ for the baseboard's EEPROMs.
// Saved settings: ~/.local/share/arcade-foundry/saves/<romset>/.
//
// This file reads the command line and runs the game list; the rest is in
// library, session, input and render (.h and .cpp), with the shared types in
// app.h.

#include "input.h"
#include "library.h"
#include "session.h"

#include <SDL3/SDL_main.h>

#include <clocale>
#include <optional>

using namespace insert_coin;

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
#ifdef __APPLE__
    if (!fs::exists(o.cores)) o.cores = exe_dir() / "../build/cores/macosx";
#else
    if (!fs::exists(o.cores)) o.cores = exe_dir() / "../build/cores/linux";
#endif
    // The saved settings (the Settings screen), then the command line's over them for this run.
    const fs::path settings_file = o.data / "settings";
    AppSettings settings;
    settings.load(settings_file);
    settings.apply(o);
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

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | SDL_INIT_HAPTIC)) {
        std::fprintf(stderr, "insert-coin: %s\n", SDL_GetError());
        return 1;
    }
    g_fullscreen = o.fullscreen || fullscreen_saved(o.data);
    SDL_Window *win = SDL_CreateWindow("Arcade Foundry: Insert Coin", 992, 768, SDL_WINDOW_RESIZABLE | (g_fullscreen ? SDL_WINDOW_FULLSCREEN : 0));
#ifdef __APPLE__
    // Metal, so a core's screen can be shown from its GPU buffer (session.cpp).
    SDL_Renderer *r = win ? SDL_CreateRenderer(win, "metal") : nullptr;
#else
    SDL_Renderer *r = win ? SDL_CreateRenderer(win, nullptr) : nullptr;
#endif
    if (!r) {
        std::fprintf(stderr, "insert-coin: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetRenderVSync(r, 1);
    Controls ctl;
    ctl.open_pad();
    ctl.wheel.open(o.data);
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
        Images images;
        Art art; // game art (art.h), downloading on its own thread
        art.enabled = settings.download_art;
        art.start(o.data);
        bool running = true;
        // All the games, or only those with a core and a ROM set; one
        // platform's or all; in an order (LibrarySettings).
        const fs::path library_file = o.data / "library";
        LibrarySettings lib;
        lib.load(library_file);
        const std::vector<ArcadeGame> all = group_games(games);
        // A platform with no games in this release (all hidden in games.toml)
        // is left out of the platform choices.
        const auto has_games = [&](int platform) {
            if (platform < 0) return true;
            for (const ArcadeGame &g : all)
                if (g.platform == platform) return true;
            return false;
        };
        if (!has_games(lib.platform)) lib.platform = -1;
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
        int settings_item = -1; // the Settings screen is open: the row chosen
        std::string art_note;   // Remove downloaded art's value: what the art takes, or "Removed"
        const auto art_size = [&] {
            std::error_code ec;
            uintmax_t bytes = 0;
            if (fs::is_directory(o.data / "art", ec))
                for (const auto &f : fs::recursive_directory_iterator(o.data / "art", ec))
                    if (f.is_regular_file(ec)) bytes += f.file_size(ec);
            char b[32];
            if (bytes < 1000000) std::snprintf(b, sizeof b, "%ju KB", (bytes + 999) / 1000);
            else std::snprintf(b, sizeof b, "%.1f MB", double(bytes) / 1e6);
            return std::string(b);
        };
        // A change on the Settings screen: saved, and in force from now on (over the command line's).
        const auto change = [&](int step) {
            const Look before = settings.look;
            const bool speed = settings.show_speed, thread = settings.game_thread;
            change_setting(settings, SettingRow(settings_item), step, win, o.data);
            settings.save(settings_file);
            art.enabled = settings.download_art;
            if (settings.look.str() != before.str()) o.look = settings.look;
            if (settings.show_speed != speed) o.show_speed = settings.show_speed;
            if (settings.game_thread != thread) o.game_thread = settings.game_thread;
        };
        std::optional<GamePage> page; // a game's page is open
        bool l2_held = false, r2_held = false; // the triggers are axes: a press is one going past three quarters
        while (running) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_QUIT) running = false;
                if (e.type == SDL_EVENT_GAMEPAD_ADDED) ctl.open_pad();
                if (e.type == SDL_EVENT_JOYSTICK_ADDED) ctl.wheel.open(o.data);
                if (e.type == SDL_EVENT_JOYSTICK_REMOVED) ctl.wheel.removed(e.jdevice.which);
                if (SDL_Event pe; wheel_as_pad(e, ctl.wheel, pe)) SDL_PushEvent(&pe); // its d-pad and buttons work the menus
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
                if (page && page->art_view >= 0) { // its art full size: Left/Right step through, Esc (East) or I (North) back
                    const int n = int(art_files(*page, o, art).size());
                    if (n && is(SDL_SCANCODE_LEFT, SDL_GAMEPAD_BUTTON_DPAD_LEFT)) page->art_view = (page->art_view + n - 1) % n;
                    if (n && is(SDL_SCANCODE_RIGHT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) page->art_view = (page->art_view + 1) % n;
                    if (is(SDL_SCANCODE_ESCAPE, SDL_GAMEPAD_BUTTON_EAST) || is(SDL_SCANCODE_I, SDL_GAMEPAD_BUTTON_NORTH) ||
                        (key && e.key.scancode == SDL_SCANCODE_BACKSPACE))
                        page->art_view = -1;
                    continue;
                }
                if (page && is(SDL_SCANCODE_I, SDL_GAMEPAD_BUTTON_NORTH) && !(key && e.key.repeat)) {
                    if (!art_files(*page, o, art).empty()) page->art_view = 0;
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
                    const int option = page->row().option < 0 ? -1 : page->option_kind(page->row().option);
                    if (is(SDL_SCANCODE_RETURN, SDL_GAMEPAD_BUTTON_SOUTH) && !(key && e.key.repeat)) {
                        if (option < 0 && page->chosen().playable()) {
                            art.hold(true); // no downloads while a game plays
                            running = play(r, page->chosen(), o, ctl);
                            art.hold(false);
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
                        } else if (option == 4) {   // the next region, from the next start
                            const std::vector<std::string> regions = core_regions(page->chosen());
                            size_t at = 0;
                            while (at < regions.size() && regions[at] != page->region) ++at;
                            if (!regions.empty()) {
                                page->region = regions[(at + 1) % regions.size()];
                                save_region(o, page->chosen().romset, page->region);
                            }
                        }
                    }
                    if (is(SDL_SCANCODE_ESCAPE, SDL_GAMEPAD_BUTTON_EAST) || (key && e.key.scancode == SDL_SCANCODE_BACKSPACE)) page.reset();
                    continue;
                }
                if (credits) { // back to the list, or to Settings when they were opened from there
                    if (is(SDL_SCANCODE_UP, SDL_GAMEPAD_BUTTON_DPAD_UP) && credits_top > 0) --credits_top;
                    if (is(SDL_SCANCODE_DOWN, SDL_GAMEPAD_BUTTON_DPAD_DOWN) && credits_top < credits_last) ++credits_top;
                    if (is(SDL_SCANCODE_ESCAPE, SDL_GAMEPAD_BUTTON_EAST) || is(SDL_SCANCODE_C, SDL_GAMEPAD_BUTTON_BACK) ||
                        (key && e.key.scancode == SDL_SCANCODE_BACKSPACE))
                        credits = false;
                    continue;
                }
                if (settings_item >= 0) {
                    if (is(SDL_SCANCODE_UP, SDL_GAMEPAD_BUTTON_DPAD_UP)) settings_item = std::max(0, settings_item - 1);
                    if (is(SDL_SCANCODE_DOWN, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) settings_item = std::min(kSettingRows - 1, settings_item + 1);
                    const bool on_credits = settings_item == int(SettingRow::credits),
                               on_remove = settings_item == int(SettingRow::remove_art);
                    if (!on_credits && !on_remove && is(SDL_SCANCODE_LEFT, SDL_GAMEPAD_BUTTON_DPAD_LEFT)) change(-1);
                    if (!on_credits && !on_remove && is(SDL_SCANCODE_RIGHT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) change(1);
                    if (is(SDL_SCANCODE_RETURN, SDL_GAMEPAD_BUTTON_SOUTH) && !(key && e.key.repeat)) {
                        if (on_credits) {
                            credits = true;
                            credits_top = 0;
                        } else if (on_remove) {
                            art.remove_all();
                            images.drop();
                            art_note = "Removed";
                        } else change(1);
                    }
                    if (is(SDL_SCANCODE_ESCAPE, SDL_GAMEPAD_BUTTON_EAST) || is(SDL_SCANCODE_TAB, SDL_GAMEPAD_BUTTON_START) ||
                        (key && e.key.scancode == SDL_SCANCODE_BACKSPACE))
                        settings_item = -1;
                    continue;
                }
                if (is(SDL_SCANCODE_TAB, SDL_GAMEPAD_BUTTON_START) && !(key && e.key.repeat)) {
                    settings_item = 0;
                    art_note = art_size();
                    continue;
                }
                if (is(SDL_SCANCODE_C, SDL_GAMEPAD_BUTTON_WEST)) {
                    credits = true;
                    credits_top = 0;
                    continue;
                }
                if (is(SDL_SCANCODE_ESCAPE, SDL_GAMEPAD_BUTTON_BACK)) running = false;
                // The platform: All, Classic 2D, Model 1 ... Chihiro (those with games), round to All again; the order likewise.
                int platform_step = 0, sort_step = trigger;
                if (is(SDL_SCANCODE_LEFT, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) platform_step = -1;
                if (is(SDL_SCANCODE_RIGHT, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) platform_step = 1;
                if (key && e.key.scancode == SDL_SCANCODE_S) sort_step = (e.key.mod & SDL_KMOD_SHIFT) ? -1 : 1;
                const bool playable_step = is(SDL_SCANCODE_P, SDL_GAMEPAD_BUTTON_NORTH);
                if (playable_step || platform_step || sort_step) {
                    if (playable_step) lib.playable_only = !lib.playable_only;
                    if (platform_step) do
                        lib.platform = (lib.platform + 1 + platform_step + kPlatformCount + 1) % (kPlatformCount + 1) - 1;
                    while (!has_games(lib.platform));
                    lib.sort = Sort((int(lib.sort) + sort_step + kSortCount) % kSortCount);
                    filter();
                    lib.save(library_file);
                }
                if (shown.empty()) continue;
                if (is(SDL_SCANCODE_UP, SDL_GAMEPAD_BUTTON_DPAD_UP)) sel = (sel + shown.size() - 1) % shown.size();
                if (is(SDL_SCANCODE_DOWN, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) sel = (sel + 1) % shown.size();
                if (is(SDL_SCANCODE_RETURN, SDL_GAMEPAD_BUTTON_SOUTH) && !(key && e.key.repeat)) {
                    page = GamePage{shown[sel], History::load(shown[sel])};
                    art.want(shown[sel].versions[0]->romset, {ArtKind::flyer, ArtKind::marquee, ArtKind::cabinet, ArtKind::ingame});
                    if (!page->folds()) page->open = 0; // one version: its settings out
                    page->reload(o);
                }
            }
            if (running && page) draw_page(r, *page, o, art, images);
            else if (running && credits) credits_last = draw_credits(r, credits_top);
            else if (running && settings_item >= 0) draw_settings(r, settings, settings_item, art_note);
            else if (running) draw_menu(r, shown, sel, o, art, images, lib);
        }
        images.drop();
    }
    ctl.wheel.close();
    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
