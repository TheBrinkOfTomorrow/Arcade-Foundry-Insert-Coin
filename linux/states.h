// Save states and Resume (docs/design/SAVE-STATES.md): a game's whole board
// at one frame, written by its core (afcore 5.4 save_state) into a slot and
// loaded back (load_state).
//
// A game's states are in <data>/states/<romset>/, beside saves/ (states are
// not saves: each one holds the EEPROM and backup RAM as they were at its
// frame): slot 0 is the Resume slot (resume.afstate), slots 1 to 3 the
// player's (1.afstate ...). Each state is deflated (raw deflate, as the
// Apple app's NSData .zlib) and has beside it a .bmp of its screen and a
// .json: {"date": seconds since 1970, "seconds": play time, "core_version":
// "x.y.z", "app_version": "x.y.z"}.
#pragma once

#include "app.h"

namespace insert_coin {

const int kStateSlots = 3; // the player's, besides the Resume slot (0)

// The core has save states (afcore 5.4, and a board that can save them).
bool has_states(const afcore_api *api);

fs::path states_dir(const Options &o, const std::string &romset);

// What a slot holds, from its .json (the state itself is read only to load it).
struct StateSlot {
    bool exists = false;
    double date = 0;      // when it was saved, seconds since 1970
    double seconds = 0;   // play time at its frame (frames since power-on / the board's rate)
    std::string core_version;
    bool older = false;   // made by another core version: it doesn't load
    fs::path picture;     // its screen, or empty

    // "10 Nov 2026 16:40 (3:05)", the play time in brackets (or "Empty";
    // " older version" after one that doesn't load).
    std::string describe() const;
};

StateSlot read_slot(const Options &o, const Game &g, int slot);

// Saves the core's board into a slot, with `px` (w x h) as its picture.
// Between run_frame calls, on the thread that runs them. Returns "" or why not.
std::string save_slot(const Options &o, const Game &g, afcore *core, int slot, const uint32_t *px, int w, int h);

// Loads a slot into the core; on failure the game is as it was. Returns "" or why not.
std::string load_slot(const Options &o, const Game &g, afcore *core, int slot);

void remove_slot(const Options &o, const Game &g, int slot);

// The game's Resume slot, offered on its page: there is one, from this core version,
// and Resume where I left off is on.
bool can_resume(const Options &o, const Game &g);

} // namespace insert_coin
