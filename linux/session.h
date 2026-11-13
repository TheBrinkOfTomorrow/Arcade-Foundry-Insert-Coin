// A game being played: its core, sound, game thread, pause menu and link play.
#pragma once

#include "input.h"

namespace insert_coin {

// Runs one game until Quit in its pause menu (or the window closes: returns false).
// `resume`: from its Resume slot (states.h); else from power-on, which drops
// the Resume slot (the game's saves move on past it). Leaving the game saves
// the Resume slot when Resume where I left off is on (not while linked, nor
// for Game Cabinet).
bool play(SDL_Renderer *r, const Game &g, const Options &o, Controls &ctl, bool resume = false);

} // namespace insert_coin
