// The keyboard, mouse and gamepads: full screen keys, the controller button
// map, the cabinet's controls (Controls) and the gun games' settings.
#pragma once

#include "app.h"
#include "wheel.h"

namespace insert_coin {

// Full screen (issue #226): F11 or Alt+Enter anywhere, and Full screen in
// the pause menu. The choice is kept in <data>/fullscreen ("1" or "0") for
// the next start; --fullscreen starts full screen whatever it says.
bool fullscreen_saved(const fs::path &data);

// Full screen as last asked for: SDL3 switches asynchronously, so the
// window's flags can lag behind a quick second press.
extern bool g_fullscreen;

void set_fullscreen(SDL_Window *w, bool on, const fs::path &data);

// F11 or Alt+Enter: switches full screen on or off; true when the event was one
bool fullscreen_key(const SDL_Event &e, SDL_Window *w, const fs::path &data);

// The cabinet's controls from the keyboard and the first gamepad.
// Keyboard: arrows steer, accelerate and brake; A/Z gear up/down; 1-4
// views; 5 coin; Return start; F2 test; F3 service; Esc pause menu.
// A steering wheel and pedals (wheel.h) drive the driving games beside these.
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
int pad_button(SDL_GamepadButton b);

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

    static ButtonMap load(const fs::path &file);
    void save(const fs::path &file) const;
};

struct Controls {
    ButtonMap map; // the game being played's (ButtonMap)
    SDL_Gamepad *pad = nullptr, *pad2 = nullptr;
    // A steering wheel and pedals (wheel.h): in the driving games (`drive`)
    // it steers, its pedals are the gas and brake, its buttons the shifter,
    // views, coin, start and hand brake, as set under Wheel and pedals in the
    // pause menu. A gamepad and the keyboard still work beside it.
    Wheel wheel;
    bool drive = true;      // the game is driven (GameControls::Drive)
    int wheel_coin_wait = -1; // the wheel's coin button: a pulse, as the gamepad's Back
    int gear = 1;
    float steer = 0;
    bool flight = false;    // set for the game being played (Game::flight)
    bool twin = false;      // (Game::twin)
    bool analog_gun = false; // (Game::analog_gun)
    bool gun = false;        // a light gun game (is_gun_game): the gamepads aim
    bool gun_button = false; // the gun's button, BUTTON3: HotD4's grenade, Line of Fire's missile (GameInfo.gunButton)
    bool gun_start = false;  // West (G, middle-click) presses its player's Start: Let's Go Jungle!'s action button (gun_start_game)
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
    uint32_t fight(SDL_Gamepad *p) const;

    afcore_input sample(SDL_Renderer *ren);

    // Virtual-On's two levers (see above): the left one as player 1's
    // fighting-game buttons (punch = shot, kick = dash), the right one as
    // buttons2 (the core puts them on IN1 and IN2, MAME's von ports).
    // Oratorio Tangram reads the levers the same way, and its shots and
    // turbos as TRIGGER1/BUTTON2 (left) and TRIGGER2/BUTTON3 (right), in b.
    afcore_input twin_sticks(const bool *k, uint32_t b);

    // Star Wars Arcade's stick, throttle and buttons (see above); `b` holds
    // the shared mapping's coin, starts, test and service.
    afcore_input fly(const bool *k, uint32_t b);
};

// The controller buttons: each button of the default layout and the one
// that stands in for it in this game; rows kPadButtons and kPadButtons + 1
// are Reset and Back. `waiting`: the next button pressed takes the row.
const int kButtonRows = kPadButtons + 2;

void draw_buttons(SDL_Renderer *r, const Game &g, const ButtonMap &map, int row, bool waiting);

// The controller buttons' keys and gamepad buttons: Up/Down choose a row,
// Return or South waits for the button to put on it (Esc cancels), or
// resets, or goes back; so do Esc, Backspace and East (`row` -1).
void buttons_event(const SDL_Event &e, ButtonMap &map, const fs::path &file, int &row, bool &waiting);

// The light gun and turret games (GameInfo.aimsGun in the Apple app's Catalog.swift).
bool is_gun_game(const Game &g);

// Let's Go Jungle!: the cabinet's big Start buttons are its action buttons
// during play, so West (G, middle-click) presses Start too (GameInfo.gunStart, #404).
bool gun_start_game(const Game &g);

// The gun games that get a crosshair: not Gunblade NY, Behind Enemy Lines
// or Rail Chase 2, whose turrets draw their own sight (as on the Apple devices).
bool shows_crosshair(const Game &g);

// Let's Go Jungle draws its own reticle where the gun aims: its crosshair
// is off until turned on (as on the Apple devices).
bool crosshair_by_default(const std::string &romset);

// A game's crosshair setting, as crosshair_by_default unless changed on its
// page: <data>/crosshair/<romset> holds "0" when off, "1" when on.
bool crosshair_on(const Options &o, const std::string &romset);

void save_crosshair(const Options &o, const std::string &romset, bool on);

// The regions a game's core offers (afcore 5.3's options, "region=export|usa|japan"),
// its default first; empty when it has none. The language follows the region.
std::vector<std::string> core_regions(const Game &g);
// The one chosen on its page, <data>/region/<romset> ("japan"), or the default.
std::string region_of(const Options &o, const Game &g);
void save_region(const Options &o, const std::string &romset, const std::string &region);

} // namespace insert_coin
