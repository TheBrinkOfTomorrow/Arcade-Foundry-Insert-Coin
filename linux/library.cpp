#include "library.h"

#include "credits.h"
#include "history.h"

#include <dlfcn.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include <ctime>

namespace insert_coin {

namespace {

// a region as the game's page shows it (core_regions)
std::string region_name(const std::string &region) {
    if (region == "export") return "Export (English)";
    if (region == "usa") return "USA";
    if (region == "japan") return "Japan";
    return region;
}

// A core of this interface's major version, or null (*outdated: the version
// of one for another major).
const afcore_api *load_core(const fs::path &dir, const std::string &romset, uint32_t *outdated) {
#ifdef __APPLE__
    // The Mac's cores are the Apple app's frameworks (build/cores/macosx).
    const fs::path lib = dir / ("afcore_" + romset + ".framework") / ("afcore_" + romset);
#else
    const fs::path lib = dir / ("libafcore_" + romset + ".so");
#endif
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

} // namespace

void LibrarySettings::load(const fs::path &file) {
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

void LibrarySettings::save(const fs::path &file) const {
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    if (std::FILE *f = std::fopen(file.c_str(), "w")) {
        std::fprintf(f, "playable=%d\nplatform=%s\nsort=%s\n", playable_only ? 1 : 0,
                     platform < 0 ? "all" : kPlatforms[platform].key, kSortKeys[int(sort)]);
        std::fclose(f);
    }
}

void AppSettings::load(const fs::path &file) {
    std::FILE *f = std::fopen(file.c_str(), "r");
    if (!f) return;
    char line[256];
    while (std::fgets(line, sizeof line, f)) {
        std::string l = line;
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
        const size_t eq = l.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = l.substr(0, eq), v = l.substr(eq + 1);
        if (key == "show_speed") show_speed = v != "0";
        else if (key == "game_thread") game_thread = v != "0";
        else if (key == "download_art") download_art = v != "0";
        else look.parse(l);
    }
    std::fclose(f);
}

void AppSettings::save(const fs::path &file) const {
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    if (std::FILE *f = std::fopen(file.c_str(), "w")) {
        std::string screen = look.str();
        std::replace(screen.begin(), screen.end(), ',', '\n');
        std::fprintf(f, "%s\nshow_speed=%d\ngame_thread=%d\ndownload_art=%d\n", screen.c_str(), show_speed ? 1 : 0, game_thread ? 1 : 0,
                     download_art ? 1 : 0);
        std::fclose(f);
    }
}

void change_setting(AppSettings &s, SettingRow row, int step, SDL_Window *win, const fs::path &data) {
    Look &l = s.look;
    switch (row) {
    case SettingRow::shape: l.shape = Shape((int(l.shape) + step + 3) % 3); break;
    case SettingRow::effect: l.effect = Effect((int(l.effect) + step + 3) % 3); break;
    case SettingRow::smooth: l.smooth = !l.smooth; break;
    // Size 80 to 120% in fives, the moves -10 to 10% of the window in ones, as the Apple app's steppers
    case SettingRow::size: l.zoom = std::clamp(std::round(l.zoom * 20 + step) / 20, 0.8, 1.2); break;
    case SettingRow::move_x: l.shift_x = std::clamp(std::round(l.shift_x * 100 + step) / 100, -0.1, 0.1); break;
    case SettingRow::move_y: l.shift_y = std::clamp(std::round(l.shift_y * 100 + step) / 100, -0.1, 0.1); break;
    case SettingRow::fullscreen: set_fullscreen(win, !g_fullscreen, data); break;
    case SettingRow::show_speed: s.show_speed = !s.show_speed; break;
    case SettingRow::game_thread: s.game_thread = !s.game_thread; break;
    case SettingRow::download_art: s.download_art = !s.download_art; break;
    case SettingRow::remove_art:
    case SettingRow::credits: break;
    }
}

void draw_settings(SDL_Renderer *r, const AppSettings &s, int item, const std::string &art_note) {
    SDL_SetRenderDrawColor(r, BACKGROUND.r, BACKGROUND.g, BACKGROUND.b, 255);
    SDL_RenderClear(r);
    int ow = 0, oh = 0;
    SDL_GetCurrentRenderOutputSize(r, &ow, &oh);
    SDL_SetRenderScale(r, 2, 2);
    const Look &l = s.look;
    const auto on = [](bool b) { return std::string(b ? "On" : "Off"); };
    const auto percent = [](double v) {
        char b[16];
        std::snprintf(b, sizeof b, "%+d%%", int(std::lround(v * 100)));
        return std::string(v == 0 ? "0%" : b);
    };
    struct Line {
        const char *heading; // a section's heading before the row, or null
        std::string label, value;
    };
    const std::string interface = std::to_string(AFCORE_API_MAJOR) + "." + std::to_string(AFCORE_API_MINOR) + "." +
                                  std::to_string(AFCORE_API_PATCH);
    const Line rows[kSettingRows] = {
        {"SCREEN (every game)", "Shape", Look::name(l.shape)},
        {nullptr, "Effect", Look::name(l.effect)},
        {nullptr, "Smooth pixels", on(l.smooth)},
        {nullptr, "Size", std::to_string(int(std::lround(l.zoom * 100))) + "%"},
        {nullptr, "Move across", percent(l.shift_x)},
        {nullptr, "Move down", percent(l.shift_y)},
        {nullptr, "Full screen (F11)", on(g_fullscreen)},
        {"SPEED", "Show speed (title bar; corner in full screen)", on(s.show_speed)},
        {nullptr, "Game on its own thread (Model 1, 2, Chihiro)", on(s.game_thread)},
        {"GAME ART", "Download game art", Art::built() ? on(s.download_art) : "Not in this build"},
        {nullptr, "Remove downloaded art", art_note},
        {"ABOUT", "Credits", ">"},
    };
    float y = 16;
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_RenderDebugText(r, 24, y, "SETTINGS");
    y += 22;
    for (int i = 0; i < kSettingRows; ++i) {
        if (rows[i].heading) {
            y += 6;
            SDL_SetRenderDrawColor(r, 255, 255, 80, 255);
            SDL_RenderDebugText(r, 24, y, rows[i].heading);
            y += 14;
            if (i == int(SettingRow::shape)) {
                SDL_SetRenderDrawColor(r, 140, 140, 150, 255);
                SDL_RenderDebugText(r, 36, y, "A game's own, from its pause menu or page, wins");
                y += 14;
            }
            if (i == int(SettingRow::download_art)) {
                SDL_SetRenderDrawColor(r, 140, 140, 150, 255);
                SDL_RenderDebugText(r, 36, y, "Arcade Database, libretro-thumbnails; sends the set name");
                y += 12;
                SDL_RenderDebugText(r, 36, y, "Your own pictures (pictures/own/) always win and stay");
                y += 14;
            }
            if (i == int(SettingRow::credits)) { // the version lines, not rows
                SDL_SetRenderDrawColor(r, 200, 200, 210, 255);
                SDL_RenderDebugText(r, 36, y, "Arcade Foundry: Insert Coin " INSERT_COIN_VERSION);
                y += 12;
                SDL_RenderDebugText(r, 36, y, ("Core interface " + interface).c_str());
                y += 14;
            }
        }
        if (i == item) SDL_SetRenderDrawColor(r, 255, 255, 80, 255);
        else SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        const std::string line = std::string(i == item ? "> " : "  ") + rows[i].label;
        SDL_RenderDebugText(r, 24, y, line.c_str());
        SDL_RenderDebugText(r, std::max(24.f + 8 * float(line.size() + 2), ow / 2.f - 24 - 8 * float(rows[i].value.size())), y,
                            rows[i].value.c_str());
        y += 12;
    }
    SDL_SetRenderScale(r, 1, 1);
    SDL_SetRenderDrawColor(r, 140, 140, 150, 255);
    SDL_RenderDebugText(r, 48, oh - 36.f, "Up/Down choose   Left/Right or Return (South) change   Esc (East) back");
    SDL_RenderDebugText(r, 48, oh - 22.f, "Command-line options (--shape, --sharp, --size ...) win for that run and are not saved");
    SDL_RenderPresent(r);
}

History History::load(const ArcadeGame &game) {
    History h;
    for (const Game *v : game.versions)
        for (const HistoryFile &f : HISTORY_MD)
            if (h.facts.empty() && v->romset == f.romset) h.parse(f.text);
    return h;
}

void History::parse(const std::string &text) {
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

// A Chihiro game folder: it holds the game's XBE (vcop3/vc3.xbe).
bool holds_xbe(const fs::path &dir) {
    std::error_code e;
    for (fs::directory_iterator it(dir, e), end; !e && it != end; it.increment(e)) {
        std::string x = it->path().extension().string();
        for (char &c : x) c = char(std::tolower((unsigned char)c));
        if (x == ".xbe") return true;
    }
    return false;
}

// A Chihiro set's XBE (games.toml), or "".
std::string chihiro_xbe(const std::string &set) {
    for (const auto &e : kChihiroXbe)
        if (set == e[0]) return e[1];
    return "";
}

std::vector<std::string> set_names(const std::string &set) {
    std::vector<std::string> all{set};
    for (const auto &n : kOtherNames)
        if (set == n[1]) all.push_back(n[0]);
    return all;
}

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
                if (fs::is_directory(p / "disk0", e) || holds_xbe(p)) found[name].push_back({depth, p});
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

void migrate_renamed_sets(const fs::path &data) {
    std::error_code ec;
    for (const auto &r : kRenamedSets) { // game_list.h, from games.toml's renamed_from
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
#ifdef __APPLE__
    char buf[4096];
    uint32_t size = sizeof buf;
    if (_NSGetExecutablePath(buf, &size) != 0) return fs::current_path();
    fs::path p = fs::canonical(buf, ec);
#else
    fs::path p = fs::read_symlink("/proc/self/exe", ec);
#endif
    return ec ? fs::current_path() : p.parent_path();
}

std::vector<Game> find_games(const Options &o) {
    // game_list.h (scripts/gen_games.py, from games.toml): the sets in the
    // library's order, less the ones left out of this release (hidden there).
    std::vector<Game> games;
    for (const GameListEntry &e : kGameList) {
        if (e.hidden) continue;
        Game g;
        g.romset = e.set;
        g.title = e.title;
        g.detail = e.detail;
        g.platform = e.platform;
        g.controls = e.controls;
        g.flight = e.controls == GameControls::Flight;
        g.twin = e.controls == GameControls::Twin;
        g.analog_gun = e.controls == GameControls::AnalogGun;
        g.stick_y = e.stick_y;
        g.gun_button = e.gun_button;
        const std::string platform = kPlatforms[e.platform].key;
        g.folder = platform == "lindbergh" || platform == "chihiro";
        if (platform == "chihiro") g.xbe = chihiro_xbe(e.set);
        games.push_back(std::move(g));
    }
    for (Game &g : games) {
        g.api = load_core(o.cores, g.romset, &g.outdated);
        if (!g.xbe.empty()) { // a Chihiro game: the folder that holds its XBE
            std::error_code ec;
            for (const std::string &name : set_names(g.romset))
                if (g.rom.empty() && fs::exists(o.links / name / g.xbe, ec)) g.rom = o.links / name;
            for (const std::string &name : set_names(g.romset))
                if (g.rom.empty() && fs::exists(o.links / (name + ".tar"), ec)) g.rom = o.links / (name + ".tar");
            continue;
        }
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

std::vector<ArcadeGame> group_games(const std::vector<Game> &games) {
    std::vector<ArcadeGame> all;
    for (const Game &g : games) {
        auto it = std::find_if(all.begin(), all.end(), [&](const ArcadeGame &a) { return a.title == g.title && a.platform == g.platform; });
        if (it == all.end()) it = all.insert(all.end(), ArcadeGame{g.title, g.platform, {}});
        it->versions.push_back(&g);
    }
    return all;
}

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

std::string status(const Game &g, const Options &o) {
    return g.outdated ? "core for interface " + version_string(g.outdated) + ": rebuild it with scripts/build_cores.py"
           : !g.api ? "not in this build: scripts/build_cores.py with its ROM set"
           : g.rom.empty() && !g.xbe.empty() ? "no game: put its folder (" + g.romset + "/, holding " + g.xbe + ") or " + g.romset + ".tar in " + o.roms.string()
           : g.rom.empty() && g.folder ? "no game: put its folder (" + g.romset + "/, holding disk0) or " + g.romset + ".tar in " + o.roms.string()
           : g.rom.empty() ? "no ROM set: put " + g.romset + ".zip in " + o.roms.string()
                           : g.romset + "  core " + version_string(g.api->core_version);
}

std::string picture_set(const ArcadeGame &game, const Options &o, const Art &art) {
    std::error_code ec;
    for (const Game *v : game.versions)
        if (!own_picture(o, v->romset).empty()) return v->romset;
    for (const Game *v : game.versions)
        if (art.has(v->romset, ArtKind::title) || art.has(v->romset, ArtKind::ingame)) return v->romset;
    for (const Game *v : game.versions)
        if (!art_wanted(v->romset, ArtKind::title) && art.has(v->romset, ArtKind::flyer)) return v->romset;
    for (const Game *v : game.versions)
        if (fs::exists(picture_file(o, v->romset), ec)) return v->romset;
    return game.versions[0]->romset;
}

namespace {
// An image fitted into box (its shape kept), centred.
void draw_fit(SDL_Renderer *r, const Images::Entry &e, const SDL_FRect &box, Uint8 alpha = 255) {
    const float scale = std::min(box.w / float(e.w), box.h / float(e.h));
    const float w = e.w * scale, h = e.h * scale;
    const SDL_FRect dst{box.x + (box.w - w) / 2, box.y + (box.h - h) / 2, w, h};
    SDL_SetTextureAlphaMod(e.tex, alpha);
    SDL_RenderTexture(r, e.tex, nullptr, &dst);
}
} // namespace

void draw_menu(SDL_Renderer *r, const std::vector<ArcadeGame> &games, size_t sel, const Options &o, Art &art, Images &images,
               const LibrarySettings &lib) {
    SDL_SetRenderDrawColor(r, BACKGROUND.r, BACKGROUND.g, BACKGROUND.b, 255);
    SDL_RenderClear(r);
    int ow = 0, oh = 0;
    SDL_GetCurrentRenderOutputSize(r, &ow, &oh);
    if (!games.empty()) {
        const std::string set = picture_set(games[sel], o, art);
        if (own_picture(o, set).empty()) art.want_card(set);
        if (const Images::Entry *e = images.get(r, card_picture(o, art, set))) {
            const float w = ow * 0.5f, h = w * 3 / 4; // the right half, 4:3
            draw_fit(r, *e, SDL_FRect{ow - w - 24, (oh - h) / 2, w, h}, 110); // behind the list's text
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
    SDL_RenderDebugText(r, 48, oh - 22.f, "Left/Right (L1 R1) platform   S, Shift+S (R2 L2) sort   Tab (Start) settings   F11 full screen");
    SDL_RenderPresent(r);
}

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
    const bool fishes = g.controls == GameControls::Fishing;
    const bool fights = g.controls == GameControls::Fighting;
    std::vector<ControlLine> lines = g.flight ? flight : g.twin ? twin : is_gun_game(g) ? gun : fishes ? fishing : fights ? fighting : driving;
    if (g.gun_button) // before Player 2
        lines.insert(lines.end() - 1, {g.romset == "loffire" ? "Missile" : "Grenade", "Middle-click or G", "West (X)"});
    lines.insert(lines.end(), cabinet.begin(), cabinet.end());
    return lines;
}

std::vector<std::pair<fs::path, std::string>> art_files(const GamePage &page, const Options &o, const Art &art) {
    std::vector<std::pair<fs::path, std::string>> out;
    const std::string set = picture_set(page.game, o, art), extras = page.game.versions[0]->romset;
    const fs::path picture = card_picture(o, art, set);
    if (!picture.empty())
        out.push_back({picture, !own_picture(o, set).empty()               ? "Your picture"
                                : picture == art.file(set, ArtKind::title)  ? art_name(ArtKind::title)
                                : picture == art.file(set, ArtKind::ingame) ? art_name(ArtKind::ingame)
                                : picture == art.file(set, ArtKind::flyer)  ? art_name(ArtKind::flyer)
                                                                            : "From the game"});
    for (const ArtKind k : kArtExtras)
        if (art.has(extras, k) && art.file(extras, k) != picture) out.push_back({art.file(extras, k), art_name(k)});
    return out;
}

void draw_page(SDL_Renderer *r, GamePage &page, const Options &o, const Art &art, Images &images) {
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
    // The text at twice the size: across the window, or its left half when there are pictures for the right.
    const auto files = art_files(page, o, art);
    const size_t cols = size_t(std::max(20, ((files.empty() ? ow : ow / 2) / 2 - 36) / SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE));
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
        const std::string text[kPageOptions + 2] = {std::string("Shape: ") + Look::name(page.look.shape),
                                                    std::string("Effect: ") + Look::name(page.look.effect),
                                                    std::string("Controller buttons: ") + (page.map.is_default() ? "default" : "changed"),
                                                    std::string("Crosshair (gamepad aim): ") + (page.crosshair ? "on" : "off"),
                                                    std::string("Region (the game's language follows): ") + region_name(page.region)};
        add(std::string(on ? "    > " : "      ") + text[page.option_kind(row.option)], on ? chosen : white, i);
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
    if (page.art_view >= 0 && !files.empty()) { // one picture, as large as the window allows
        page.art_view = std::min(page.art_view, int(files.size()) - 1);
        const auto &[file, name] = files[size_t(page.art_view)];
        if (const Images::Entry *e = images.get(r, file)) draw_fit(r, *e, SDL_FRect{24, 24, ow - 48.f, oh - 72.f});
        SDL_SetRenderDrawColor(r, 140, 140, 150, 255);
        const std::string hint = name + "  (" + std::to_string(page.art_view + 1) + " of " + std::to_string(files.size()) +
                                 ")   Left/Right next   Esc (East) back   Art: Arcade Database, libretro-thumbnails";
        SDL_RenderDebugText(r, 48, oh - 22.f, hint.c_str());
        SDL_RenderPresent(r);
        return;
    }
    // The right half: the game's picture at 4:3, the other art in a row under it.
    {
        const float x = ow / 2.f + 12, w = ow / 2.f - 36, h = w * 3 / 4;
        float y = 40;
        size_t first_extra = 0;
        if (!files.empty() && files[0].first == card_picture(o, art, picture_set(page.game, o, art))) {
            if (const Images::Entry *e = images.get(r, files[0].first)) draw_fit(r, *e, SDL_FRect{x, y, w, h});
            first_extra = 1;
        }
        y += h + 16;
        const size_t extras = files.size() - first_extra;
        if (extras) {
            const float tw = (w - 12 * float(extras - 1)) / float(extras), th = std::min(tw * 4 / 3, oh - y - 60);
            for (size_t i = 0; i < extras; ++i)
                if (const Images::Entry *e = images.get(r, files[first_extra + i].first))
                    draw_fit(r, *e, SDL_FRect{x + float(i) * (tw + 12), y, tw, th});
            SDL_SetRenderDrawColor(r, 140, 140, 150, 255);
            SDL_RenderDebugText(r, x, y + th + 8, "I (North) shows the art full size");
        }
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

} // namespace insert_coin
