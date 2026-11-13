// Drawing: the game's screen with its shape and effect, the list's
// pictures, the pause menu, the guns' crosshairs, and each game's screen
// settings.
#pragma once

#include "app.h"

namespace insert_coin {

struct Controls;

// Where the picture went in the last draw_screen, in render coordinates
// (the mouse aims across it).
extern SDL_FRect g_picture;

// The picture's place in an ow x oh output for a w x h screen: 4:3 for
// 496 x 384, a widened screen keeping that pixel shape, and a Lindbergh
// screen (square_pixels) at its own shape; then the look's shape, size and
// position.
SDL_FRect screen_rect(int ow, int oh, int w, int h, const Look &look, bool square_pixels = false);

// Scanlines and the CRT look, drawn over the picture with SDL_BLENDMODE_MOD
// (each pixel's colour multiplies the picture's): dark gaps between the
// game's lines and, for the CRT, the shadow mask's stripes and darker
// edges. SDL's renderer has no shaders, so there is no curve or glow here,
// as there is on the Apple devices. Built again when the size changes.
struct Overlay {
    SDL_Texture *tex = nullptr;
    int w = 0, h = 0, lines = 0;
    Effect effect = Effect::none;

    SDL_Texture *get(SDL_Renderer *r, int ow, int oh, int screen_lines, Effect e);
    void drop() {
        if (tex) SDL_DestroyTexture(tex);
        tex = nullptr;
    }
};

void draw_screen(SDL_Renderer *r, SDL_Texture *tex, int w, int h, const Look &look, Overlay &overlay, bool square_pixels);

uint64_t fnv(const uint32_t *px, size_t n);

// The app's picture of each game: <data>/pictures/<romset>.bmp, the game's
// screen at 4:3, taken after half a minute of its first play (picture_seconds). The player's
// own ("Use this screen as the picture" in the pause menu) is
// <data>/pictures/own/<romset>.bmp (own_picture in art.h). The list shows the
// player's own, else the downloaded art, else this (card_picture).
fs::path picture_file(const Options &o, const std::string &romset);

// Seconds of play before that screen is taken: 30, or 150 on Lindbergh and
// Chihiro, which are still loading at half a minute (Sega Race TV's
// INITIALIZE STEP screen).
double picture_seconds(const std::string &platform_key);

void save_picture(const fs::path &file, const uint32_t *px, int w, int h);

// The pause menu over the stopped game: its items, in order. Save state
// and Load state (states.h) only for a game whose core has them, and not
// while linked. Operator mode turns on or off (Controls::operator_mode),
// Shape and Effect step through the screen's (kept as this game's own),
// Controller buttons opens draw_buttons, Wheel and pedals draw_wheel,
// Picture keeps the screen as the player's own picture of the game.
enum class PauseItem { resume, save_state, load_state, operator_mode, shape, effect, buttons, wheel, picture, fullscreen, quit };

std::vector<PauseItem> pause_items(bool states);

// The pause menu's frame: the game dimmed, text at twice the size; returns
// where the text starts.
SDL_FPoint menu_frame(SDL_Renderer *r);

// `note`: a line under the items (what Save state or Load state did), or "".
void draw_pause(SDL_Renderer *r, const Game &g, const std::vector<PauseItem> &items, int item, bool operator_mode, const Look &look,
                const std::string &note);

// Save state and Load state's slots (1 to kStateSlots) under the pause menu's
// title, each with its date and play time; `lines[i]` describes slot i + 1.
// The chosen slot's picture (`picture`, or null) on the right.
void draw_slots(SDL_Renderer *r, const Game &g, bool saving, const std::vector<std::string> &lines, int row, SDL_Texture *picture);

// A game's screen settings: its own (chosen in its pause menu), else every game's.
Look game_look(const Options &o, const std::string &romset);

void save_look(const Options &o, const std::string &romset, const Look &look);

// Where the gamepads aim, while one is connected (the mouse shows where it
// is): player 1's in red, player 2's in blue once player 2 pressed Start
// on the second gamepad (the game's own player state is out of sight), as
// on the Apple devices.
void draw_crosshairs(SDL_Renderer *r, const Controls &ctl);

} // namespace insert_coin
