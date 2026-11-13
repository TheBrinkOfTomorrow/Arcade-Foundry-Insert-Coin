// PC-based games (#452; docs/design/APP-ARCHITECTURE.md, PC-based games):
// RingWide, RingEdge, Europa-R, the Sega PC games and ALLS. Each is a game
// folder the player points the app at (never copied), run by its core's
// runner, afgame_<set>, in a process of its own, so a crash in a large PC
// game never takes the library down.
#pragma once

#include "app.h"

#include <SDL3/SDL.h>

namespace insert_coin {

// The game's program, a path inside its game folder (games.toml's program),
// or "" when its core names none.
std::string pc_program(const std::string &set);

// The game folder chosen for a set (folder/<set> in the data folder), or empty.
fs::path chosen_folder(const Options &o, const std::string &set);
void save_chosen_folder(const Options &o, const std::string &set, const fs::path &dir);

// A PC-based game's runner and game folder: the runner beside the cores
// (afgame_<set>); the folder from --game-folder (for --play's game), the
// one chosen on its page, or its set's folder in the ROM folder, the first
// that holds its program.
void find_pc_game(Game &g, const Options &o);

// Opens the system's folder chooser for a set's game folder (Linux: the
// desktop portal or zenity). The answer comes later, from folder_answer.
void ask_for_folder(SDL_Window *win, const std::string &set, const fs::path &start);
// A folder chosen since the last call: true and its set and folder.
bool folder_answer(std::string &set, fs::path &dir);

// Plays a PC-based game: its runner as a child process, with
//   afgame_<set> --game-dir DIR --saves DIR [--fullscreen] --display N --settings FILE
// (the settings file: key=value lines). The app's window is hidden while it
// runs; when it ends the window comes back, and a failure shows the
// runner's last "error:" line (its whole output is logs/<set>.log).
void play_pc_game(SDL_Renderer *r, const Game &g, const Options &o);

} // namespace insert_coin
