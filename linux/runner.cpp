// PC-based games: their game folder and their runner process (runner.h).
#include "runner.h"

#include "input.h"
#include "library.h"

#include <fcntl.h>
#include <mutex>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

namespace insert_coin {

std::string pc_program(const std::string &set) {
    for (const auto &e : kPcPrograms)
        if (e[0] && set == e[0]) return e[1];
    return "";
}

fs::path chosen_folder(const Options &o, const std::string &set) {
    std::string line;
    if (std::FILE *f = std::fopen((o.data / "folder" / set).c_str(), "r")) {
        char buf[4096];
        if (std::fgets(buf, sizeof buf, f)) line = buf;
        std::fclose(f);
    }
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
    return line;
}

void save_chosen_folder(const Options &o, const std::string &set, const fs::path &dir) {
    std::error_code ec;
    fs::create_directories(o.data / "folder", ec);
    if (std::FILE *f = std::fopen((o.data / "folder" / set).c_str(), "w")) {
        std::fprintf(f, "%s\n", dir.c_str());
        std::fclose(f);
    }
}

namespace {

bool holds_game(const fs::path &dir, const std::string &program) {
    std::error_code ec;
    return !dir.empty() && fs::is_directory(dir, ec) && (program.empty() || fs::is_regular_file(dir / program, ec));
}

} // namespace

void find_pc_game(Game &g, const Options &o) {
    std::error_code ec;
    const fs::path runner = o.cores / ("afgame_" + g.romset);
    g.runner = fs::is_regular_file(runner, ec) ? runner : fs::path();
    g.rom.clear();
    std::vector<fs::path> places;
    if (!o.game_folder.empty() && o.play == g.romset) places.push_back(o.game_folder);
    places.push_back(chosen_folder(o, g.romset));
    for (const std::string &name : set_names(g.romset)) places.push_back(o.links / name);
    for (const fs::path &p : places)
        if (holds_game(p, g.program)) {
            g.rom = fs::canonical(p, ec);
            if (ec) g.rom = p;
            return;
        }
}

namespace {

// The folder chooser answers on a thread of its own.
std::mutex g_answer_lock;
std::string g_asked_set, g_answer_set;
fs::path g_answer_dir;

void SDLCALL folder_chosen(void *, const char *const *files, int) {
    std::lock_guard<std::mutex> hold(g_answer_lock);
    if (files && files[0]) { // null: failed; an empty list: cancelled
        g_answer_set = g_asked_set;
        g_answer_dir = files[0];
    }
}

// The runner's display: the app window's, by its place in SDL's list (the
// runner's SDL lists them in the same order).
int display_index(SDL_Window *win) {
    const SDL_DisplayID mine = win ? SDL_GetDisplayForWindow(win) : 0;
    int count = 0, at = 0;
    if (SDL_DisplayID *all = SDL_GetDisplays(&count)) {
        for (int i = 0; i < count; ++i)
            if (all[i] == mine) at = i;
        SDL_free(all);
    }
    return at;
}

// The runner's last "error:" line in its log, without the "error:".
std::string last_error(const fs::path &log) {
    std::string found;
    if (std::FILE *f = std::fopen(log.c_str(), "r")) {
        char line[1024];
        while (std::fgets(line, sizeof line, f))
            if (!std::strncmp(line, "error:", 6)) found = line + 6;
        std::fclose(f);
    }
    while (!found.empty() && std::isspace((unsigned char)found.front())) found.erase(found.begin());
    while (!found.empty() && std::isspace((unsigned char)found.back())) found.pop_back();
    return found;
}

void say(SDL_Window *win, const std::string &text) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Arcade Foundry: Insert Coin", text.c_str(), win);
}

} // namespace

void ask_for_folder(SDL_Window *win, const std::string &set, const fs::path &start) {
    {
        std::lock_guard<std::mutex> hold(g_answer_lock);
        g_asked_set = set;
    }
    std::error_code ec;
    const std::string where = fs::is_directory(start, ec) ? start.string() : "";
    SDL_ShowOpenFolderDialog(folder_chosen, nullptr, win, where.empty() ? nullptr : where.c_str(), false);
}

bool folder_answer(std::string &set, fs::path &dir) {
    std::lock_guard<std::mutex> hold(g_answer_lock);
    if (g_answer_set.empty()) return false;
    set = g_answer_set;
    dir = g_answer_dir;
    g_answer_set.clear();
    return true;
}

void play_pc_game(SDL_Renderer *r, const Game &g, const Options &o) {
    SDL_Window *win = SDL_GetRenderWindow(r);
    std::error_code ec;
    const fs::path saves = o.data / "saves" / g.romset, logs = o.data / "logs", runs = o.data / "runner";
    fs::create_directories(saves, ec);
    fs::create_directories(logs, ec);
    fs::create_directories(runs, ec);
    // What the runner needs from the app's settings, as key=value lines.
    const fs::path settings = runs / (g.romset + ".settings"), log = logs / (g.romset + ".log");
    if (std::FILE *f = std::fopen(settings.c_str(), "w")) {
        std::fprintf(f, "buttons=%s\n", (o.data / "buttons" / g.romset).c_str());
        std::fprintf(f, "region=%s\n", region_of(o, g).c_str());
        std::fclose(f);
    }
    const std::string display = std::to_string(display_index(win));
    std::vector<std::string> args = {g.runner.string(), "--game-dir", g.rom.string(), "--saves", saves.string()};
    if (g_fullscreen || o.fullscreen) args.push_back("--fullscreen");
    args.insert(args.end(), {"--display", display, "--settings", settings.string()});
    std::vector<char *> argv;
    for (std::string &a : args) argv.push_back(a.data());
    argv.push_back(nullptr);

    posix_spawn_file_actions_t files;
    posix_spawn_file_actions_init(&files);
    posix_spawn_file_actions_addopen(&files, 0, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&files, 1, log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    posix_spawn_file_actions_adddup2(&files, 1, 2);
    pid_t pid = 0;
    const int failed = posix_spawn(&pid, argv[0], &files, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&files);
    if (failed) {
        say(win, "The game's runner didn't start (" + g.runner.string() + "): " + std::strerror(failed));
        return;
    }
    mark_played(o.data, g.romset);
    if (o.speed) std::printf("insert-coin: %s runs in its own process (%d); its output is in %s\n", g.romset.c_str(), int(pid), log.c_str());
    // The game has the screen until it ends. The library keeps reading its
    // events (so it isn't "not responding") and drops them afterwards; a
    // quit asked of the app meanwhile ends the game first.
    SDL_HideWindow(win);
    int status = 0;
    bool quit = false;
    for (;;) {
        const pid_t done = waitpid(pid, &status, WNOHANG);
        if (done == pid || (done < 0 && errno != EINTR)) break;
        SDL_Event e;
        while (SDL_PollEvent(&e))
            if (e.type == SDL_EVENT_QUIT && !quit) {
                quit = true;
                kill(pid, SIGTERM);
            }
        SDL_Delay(50);
    }
    SDL_ShowWindow(win);
    SDL_RaiseWindow(win);
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    if (quit) {
        SDL_Event e{};
        e.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&e);
        return;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) return;
    std::string why = last_error(log);
    if (why.empty())
        why = WIFSIGNALED(status) ? std::string("The game stopped: ") + strsignal(WTERMSIG(status)) + "."
                                  : "The game stopped with exit code " + std::to_string(WEXITSTATUS(status)) + ".";
    say(win, why + "\n\nIts output is in " + log.string());
}

} // namespace insert_coin
