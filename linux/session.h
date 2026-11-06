// A game being played: its core, sound, game thread, pause menu and link play.
#pragma once

#include "input.h"

namespace insert_coin {

// Runs one game until Quit in its pause menu (or the window closes: returns false).
bool play(SDL_Renderer *r, const Game &g, const Options &o, Controls &ctl);

} // namespace insert_coin
