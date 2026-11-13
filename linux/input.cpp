#include "input.h"
#include "render.h"

#include <cstddef>

namespace insert_coin {

ButtonMap ButtonMap::load(const fs::path &file) {
    ButtonMap m;
    std::FILE *f = std::fopen(file.c_str(), "r");
    if (!f) return m;
    char buf[512] = "";
    const std::string text(buf, std::fread(buf, 1, sizeof buf - 1, f));
    std::fclose(f);
    const auto index = [](const std::string &key) {
        for (int i = 0; i < kPadButtons; ++i)
            if (key == kPadKeys[i]) return i;
        return -1;
    };
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find_first_of(",\n", at);
        if (end == std::string::npos) end = text.size();
        const std::string pair = text.substr(at, end - at);
        if (const size_t eq = pair.find('='); eq != std::string::npos) {
            const int from = index(pair.substr(0, eq)), to_ = index(pair.substr(eq + 1));
            if (from >= 0 && to_ >= 0) m.to[from] = to_;
        }
        at = end + 1;
    }
    return m;
}

void ButtonMap::save(const fs::path &file) const {
    std::error_code ec;
    if (is_default()) {
        fs::remove(file, ec);
        return;
    }
    fs::create_directories(file.parent_path(), ec);
    std::string text;
    for (int i = 0; i < kPadButtons; ++i)
        if (to[i] != i) text += std::string(text.empty() ? "" : ",") + kPadKeys[i] + "=" + kPadKeys[to[i]];
    if (std::FILE *f = std::fopen(file.c_str(), "w")) {
        std::fprintf(f, "%s\n", text.c_str());
        std::fclose(f);
    }
}

uint32_t Controls::fight(SDL_Gamepad *p) const {
    if (!p) return 0;
    const auto btn = [&](SDL_GamepadButton x) { return pad_btn(p, x); };
    float x = SDL_GetGamepadAxis(p, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f, y = SDL_GetGamepadAxis(p, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
    if (fishing) x = y = 0;
    uint32_t b = 0;
    if (btn(SDL_GAMEPAD_BUTTON_WEST)) b |= AFCORE_PUNCH;
    if (btn(SDL_GAMEPAD_BUTTON_SOUTH)) b |= AFCORE_KICK;
    if (btn(SDL_GAMEPAD_BUTTON_EAST)) b |= AFCORE_GUARD;
    if (btn(SDL_GAMEPAD_BUTTON_NORTH)) b |= AFCORE_BUTTON2; // Virtua Fighter 3's Escape
    if (btn(SDL_GAMEPAD_BUTTON_DPAD_UP) || y < -0.5f) b |= AFCORE_UP;
    if (btn(SDL_GAMEPAD_BUTTON_DPAD_DOWN) || y > 0.5f) b |= AFCORE_DOWN;
    if (btn(SDL_GAMEPAD_BUTTON_DPAD_LEFT) || x < -0.5f) b |= AFCORE_LEFT;
    if (btn(SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || x > 0.5f) b |= AFCORE_RIGHT;
    return b;
}

afcore_input Controls::sample(SDL_Renderer *ren) {
    afcore_input in{};
    in.size = sizeof in;
    const bool *k = SDL_GetKeyboardState(nullptr);
    float target = 0, accel = 0, brake = 0;
    bool up = k[SDL_SCANCODE_A], down = k[SDL_SCANCODE_Z];
    uint32_t b = 0;
    if (pad) {
        const auto axis = [&](SDL_GamepadAxis a) { return pad_axis(pad, a); };
        target = axis(SDL_GAMEPAD_AXIS_LEFTX);
        if (std::fabs(target) < 0.08f) target = 0;
        accel = std::max(0.f, axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
        brake = std::max(0.f, axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
        const auto btn = [&](SDL_GamepadButton x) { return pad_btn(pad, x); };
        up = up || btn(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
        down = down || btn(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
        if (btn(SDL_GAMEPAD_BUTTON_SOUTH)) b |= AFCORE_VIEW1;
        if (btn(SDL_GAMEPAD_BUTTON_EAST)) { b |= AFCORE_VIEW2; in.handbrake = 1; }
        if (btn(SDL_GAMEPAD_BUTTON_WEST)) b |= AFCORE_VIEW3;
        if (btn(SDL_GAMEPAD_BUTTON_NORTH)) b |= AFCORE_VIEW4;
        const bool coin = btn(SDL_GAMEPAD_BUTTON_BACK), start = btn(SDL_GAMEPAD_BUTTON_START);
        if (!coin && !start) chord = false;
        if (!chord) {
            if (coin && !coin_held) coin_wait = 0;
            if (start && !start_held) start_wait = 0;
        }
        coin_held = coin;
        start_held = start;
    }
    // A coin is a short pulse; start lasts as long as Start is held (codes hold it).
    if (coin_wait >= 0) {
        if (coin_wait >= grace) b |= AFCORE_COIN;
        if (++coin_wait >= grace + pulse) coin_wait = -1;
    }
    if (start_wait >= 0) {
        if (start_wait >= grace) b |= AFCORE_START;
        if (++start_wait >= grace + pulse && !start_held) start_wait = -1;
    }
    // The wheel (driving games only): it steers when turned past the
    // gamepad's stick, its pedals add to the triggers, its buttons to the rest.
    if (wheel.joy && drive && !flight && !twin) {
        const float ws = wheel.steer();
        if (std::fabs(ws) >= std::fabs(target)) target = ws;
        accel = std::max(accel, wheel.gas());
        brake = std::max(brake, wheel.brake());
        up = up || wheel.held(kWShiftUp) || wheel.held(kWPaddleUp);
        down = down || wheel.held(kWShiftDown) || wheel.held(kWPaddleDown);
        if (wheel.held(kWView1)) b |= AFCORE_VIEW1;
        if (wheel.held(kWView2)) b |= AFCORE_VIEW2;
        if (wheel.held(kWView3)) b |= AFCORE_VIEW3;
        if (wheel.held(kWView4)) b |= AFCORE_VIEW4;
        if (wheel.held(kWHandbrake)) in.handbrake = 1;
        if (wheel.held(kWStart)) b |= AFCORE_START;
    }
    if (wheel.joy) { // coin works in every game, as a short pulse
        const bool c = wheel.held(kWCoin);
        if (c && wheel_coin_wait < 0) wheel_coin_wait = 0;
        if (wheel_coin_wait >= 0) {
            if (wheel_coin_wait < pulse) b |= AFCORE_COIN;
            if (++wheel_coin_wait >= pulse && !c) wheel_coin_wait = -1;
            else if (wheel_coin_wait >= pulse) wheel_coin_wait = pulse; // held: one coin until let go
        }
        if (!drive && wheel.held(kWStart)) b |= AFCORE_START;
    }
    const bool l = k[SDL_SCANCODE_LEFT], r = k[SDL_SCANCODE_RIGHT];
    if (l || r) steer += std::clamp((r ? 1.f : 0.f) - (l ? 1.f : 0.f) - steer, -0.12f, 0.12f); // keys ramp, as in the recomp's app
    else steer = target;
    if (k[SDL_SCANCODE_UP]) accel = 1;
    if (k[SDL_SCANCODE_DOWN]) brake = 1;
    if (k[SDL_SCANCODE_1]) b |= AFCORE_VIEW1;
    if (k[SDL_SCANCODE_2]) { b |= AFCORE_VIEW2; in.handbrake = 1; }
    if (k[SDL_SCANCODE_3]) b |= AFCORE_VIEW3;
    if (k[SDL_SCANCODE_4]) b |= AFCORE_VIEW4;
    if (k[SDL_SCANCODE_5]) b |= AFCORE_COIN;
    if (k[SDL_SCANCODE_RETURN] && !(SDL_GetModState() & SDL_KMOD_ALT)) b |= AFCORE_START; // Alt+Enter: full screen
    if (k[SDL_SCANCODE_F2]) b |= AFCORE_TEST;
    if (k[SDL_SCANCODE_F3]) b |= AFCORE_SERVICE;
    b |= fight(pad); // Virtua Fighter 2 (the other games ignore these)
    if (k[SDL_SCANCODE_A]) b |= AFCORE_PUNCH;
    if (k[SDL_SCANCODE_S]) b |= AFCORE_KICK;
    if (k[SDL_SCANCODE_D]) b |= AFCORE_GUARD;
    if (k[SDL_SCANCODE_F]) b |= AFCORE_BUTTON2;
    if (k[SDL_SCANCODE_UP]) b |= AFCORE_UP;
    if (k[SDL_SCANCODE_DOWN]) b |= AFCORE_DOWN;
    if (l) b |= AFCORE_LEFT;
    if (r) b |= AFCORE_RIGHT;
    in.buttons2 = fight(pad2);
    if (pad2 && pad_btn(pad2, SDL_GAMEPAD_BUTTON_START)) b |= AFCORE_START2, second_joined = true;
    if (up) b |= AFCORE_SHIFT_UP; // Manx TT, INDY 500 and Virtua Racing read the shift buttons themselves
    if (down) b |= AFCORE_SHIFT_DOWN;
    if (up && !up_held) gear = std::min(gear + 1, 4);
    if (down && !down_held) gear = std::max(gear - 1, 1);
    up_held = up;
    down_held = down;
    if (operator_mode) {
        b &= AFCORE_COIN | AFCORE_START | AFCORE_START2 | AFCORE_TEST | AFCORE_SERVICE;
        in.buttons2 = 0;
        const bool test = pad && SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_SOUTH);
        const bool service = pad && SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_EAST);
        if (!test && !service) operator_armed = true;
        if (operator_armed && test) b |= AFCORE_TEST;
        if (operator_armed && service) b |= AFCORE_SERVICE;
        in.steer = std::clamp(steer, -1.f, 1.f); // the stick and pedals stay: test mode's input checks
        in.accel = accel;
        in.brake = brake;
        in.gear = gear;
        in.gun_x[0] = in.gun_x[1] = in.gun_y[0] = in.gun_y[1] = -1.f;
        in.buttons = b;
        return in;
    }
    if (flight) return fly(k, b & (AFCORE_COIN | AFCORE_START | AFCORE_START2 | AFCORE_TEST | AFCORE_SERVICE));
    if (twin) return twin_sticks(k, b & (AFCORE_COIN | AFCORE_START | AFCORE_TEST | AFCORE_SERVICE));
    in.steer = std::clamp(steer, -1.f, 1.f);
    in.accel = accel;
    in.brake = brake;
    in.gear = gear;
    if (stick_y && pad) in.stick_y = std::clamp(pad_axis(pad, SDL_GAMEPAD_AXIS_LEFTY), -1.f, 1.f); // -1 pushed forward
    // Virtua Cop 2's gun: the mouse over the picture (where draw_screen put it)
    float mx = 0, my = 0, rx = 0, ry = 0;
    const SDL_MouseButtonFlags mb = SDL_GetMouseState(&mx, &my);
    SDL_RenderCoordinatesFromWindow(ren, mx, my, &rx, &ry);
    int ow = 0, oh = 0;
    SDL_GetCurrentRenderOutputSize(ren, &ow, &oh);
    SDL_FRect pic = g_picture;
    if (pic.w <= 0 || pic.h <= 0) pic = screen_rect(ow, oh, 496, 384, Look{});
    const double dw = pic.w, dh = pic.h, left = pic.x, top = pic.y;
    // the mouse aims when it moves, the gamepad's stick from there
    if (mx != mouse_x || my != mouse_y || !gun || !pad) {
        aim_x = float((rx - left) / dw);
        aim_y = float((ry - top) / dh);
    }
    mouse_x = mx, mouse_y = my;
    bool fire = (mb & SDL_BUTTON_LMASK) || k[SDL_SCANCODE_SPACE], reload = mb & SDL_BUTTON_RMASK;
    bool grenade = (mb & SDL_BUTTON_MMASK) || k[SDL_SCANCODE_G];
    bool fire2 = false, reload2 = false, grenade2 = false;
    const bool second = gun && pad2;
    if (gun && pad) {
        move_aim(pad, aim_x, aim_y);
        fire = fire || pad_fire(pad);
        reload = reload || pad_reload(pad);
        grenade = grenade || pad_btn(pad, SDL_GAMEPAD_BUTTON_WEST);
    }
    if (second) {
        move_aim(pad2, aim2_x, aim2_y);
        fire2 = pad_fire(pad2);
        reload2 = pad_reload(pad2);
        grenade2 = pad_btn(pad2, SDL_GAMEPAD_BUTTON_WEST);
    }
    if (analog_gun) { // the aim stays on screen; the second trigger reloads (The Lost World)
        in.gun_x[0] = aim_x;
        in.gun_y[0] = aim_y;
        in.gun_x[1] = second ? aim2_x : -1.f; // player 2 without a gamepad: its edge, never fired
        in.gun_y[1] = second ? aim2_y : -1.f;
        if (fire && !reload) b |= AFCORE_TRIGGER1;
        if (reload) b |= AFCORE_TRIGGER2;
        in.buttons = b;
        // player 2's triggers
        in.buttons2 = (fire2 && !reload2 ? AFCORE_TRIGGER1 : 0) | (reload2 ? AFCORE_TRIGGER2 : 0);
        return in;
    }
    // TRIGGER1 is player 1's trigger, TRIGGER2 player 2's; a shot off screen reloads
    in.gun_x[0] = reload ? -1.f : aim_x;
    in.gun_y[0] = reload ? -1.f : aim_y;
    in.gun_x[1] = second && !reload2 ? aim2_x : -1.f; // player 2 without a gamepad: off screen
    in.gun_y[1] = second && !reload2 ? aim2_y : -1.f;
    if (fire || reload) b |= AFCORE_TRIGGER1;
    if (fire2 || reload2) b |= AFCORE_TRIGGER2;
    if (gun_button) { // player 1's in buttons, player 2's in buttons2
        if (grenade) b |= AFCORE_BUTTON3;
        if (grenade2) in.buttons2 |= AFCORE_BUTTON3;
    } else if (gun_start) { // Start is the action button: West as well as Start
        if (grenade) b |= AFCORE_START;
        if (grenade2) b |= AFCORE_START2;
    }
    in.buttons = b;
    return in;
}

afcore_input Controls::twin_sticks(const bool *k, uint32_t b) {
    afcore_input in{};
    in.size = sizeof in;
    uint32_t r = 0;
    const auto lever = [](float x, float y, uint32_t &to) {
        if (y < -0.5f) to |= AFCORE_UP;
        if (y > 0.5f) to |= AFCORE_DOWN;
        if (x < -0.5f) to |= AFCORE_LEFT;
        if (x > 0.5f) to |= AFCORE_RIGHT;
    };
    if (pad) {
        const auto axis = [&](SDL_GamepadAxis a) { return pad_axis(pad, a); };
        const auto btn = [&](SDL_GamepadButton x) { return pad_btn(pad, x); };
        float lx = axis(SDL_GAMEPAD_AXIS_LEFTX), ly = axis(SDL_GAMEPAD_AXIS_LEFTY);
        if (btn(SDL_GAMEPAD_BUTTON_DPAD_LEFT)) lx = -1;
        if (btn(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) lx = 1;
        if (btn(SDL_GAMEPAD_BUTTON_DPAD_UP)) ly = -1;
        if (btn(SDL_GAMEPAD_BUTTON_DPAD_DOWN)) ly = 1;
        lever(lx, ly, b);
        lever(axis(SDL_GAMEPAD_AXIS_RIGHTX), axis(SDL_GAMEPAD_AXIS_RIGHTY), r);
        if (axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 0.5f) b |= AFCORE_PUNCH | AFCORE_TRIGGER1;
        if (btn(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) b |= AFCORE_KICK | AFCORE_BUTTON2;
        if (axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 0.5f) r |= AFCORE_PUNCH, b |= AFCORE_TRIGGER2;
        if (btn(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) r |= AFCORE_KICK, b |= AFCORE_BUTTON3;
    }
    lever(float(k[SDL_SCANCODE_D]) - float(k[SDL_SCANCODE_A]), float(k[SDL_SCANCODE_S]) - float(k[SDL_SCANCODE_W]), b);
    lever(float(k[SDL_SCANCODE_L]) - float(k[SDL_SCANCODE_J]), float(k[SDL_SCANCODE_K]) - float(k[SDL_SCANCODE_I]), r);
    if (k[SDL_SCANCODE_Q]) b |= AFCORE_PUNCH | AFCORE_TRIGGER1;
    if (k[SDL_SCANCODE_E]) b |= AFCORE_KICK | AFCORE_BUTTON2;
    if (k[SDL_SCANCODE_U]) r |= AFCORE_PUNCH, b |= AFCORE_TRIGGER2;
    if (k[SDL_SCANCODE_O]) r |= AFCORE_KICK, b |= AFCORE_BUTTON3;
    in.buttons = b;
    in.buttons2 = r;
    in.gun_x[0] = in.gun_y[0] = in.gun_x[1] = in.gun_y[1] = -1.f;
    return in;
}

afcore_input Controls::fly(const bool *k, uint32_t b) {
    afcore_input in{};
    in.size = sizeof in;
    float y = 0, throttle = 0;
    bool fire = k[SDL_SCANCODE_SPACE];
    if (pad) {
        const auto axis = [&](SDL_GamepadAxis a) { return pad_axis(pad, a); };
        const auto btn = [&](SDL_GamepadButton x) { return pad_btn(pad, x); };
        y = axis(SDL_GAMEPAD_AXIS_LEFTY); // SDL: down is positive, as pulling back is
        if (std::fabs(y) < 0.08f) y = 0;
        throttle = std::max(0.f, axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
        fire = fire || btn(SDL_GAMEPAD_BUTTON_SOUTH) || btn(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
        if (btn(SDL_GAMEPAD_BUTTON_EAST)) b |= AFCORE_BUTTON2;
        if (btn(SDL_GAMEPAD_BUTTON_WEST)) b |= AFCORE_BUTTON3;
        if (btn(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) b |= AFCORE_TRIGGER2; // Star Wars Trilogy's second trigger
        // Wing War's four view buttons: North steps to the next view
        const bool north = btn(SDL_GAMEPAD_BUTTON_NORTH);
        if (north && !view_held) view = view % 4 + 1, view_frames = 6;
        view_held = north;
    }
    if (view_frames > 0) {
        --view_frames;
        b |= view == 1 ? AFCORE_VIEW1 : view == 2 ? AFCORE_VIEW2 : view == 3 ? AFCORE_VIEW3 : AFCORE_VIEW4;
    }
    if (k[SDL_SCANCODE_1]) b |= AFCORE_VIEW1;
    if (k[SDL_SCANCODE_2]) b |= AFCORE_VIEW2;
    if (k[SDL_SCANCODE_3]) b |= AFCORE_VIEW3;
    if (k[SDL_SCANCODE_4]) b |= AFCORE_VIEW4;
    if (k[SDL_SCANCODE_UP]) y = -1;
    if (k[SDL_SCANCODE_DOWN]) y = 1;
    if (k[SDL_SCANCODE_A]) key_throttle = std::min(1.f, key_throttle + 0.03f);
    if (k[SDL_SCANCODE_Z]) key_throttle = std::max(0.f, key_throttle - 0.03f);
    if (k[SDL_SCANCODE_X]) b |= AFCORE_BUTTON2;
    if (k[SDL_SCANCODE_C]) b |= AFCORE_BUTTON3;
    if (k[SDL_SCANCODE_V]) b |= AFCORE_TRIGGER2;
    if (k[SDL_SCANCODE_6]) b |= AFCORE_START2;
    if (fire) b |= AFCORE_TRIGGER1;
    in.steer = std::clamp(steer, -1.f, 1.f); // the left stick, or the arrows ramping as for a wheel
    in.stick_y = std::clamp(y, -1.f, 1.f);
    in.accel = std::max(throttle, key_throttle);
    in.buttons = b;
    in.gun_x[0] = in.gun_y[0] = in.gun_x[1] = in.gun_y[1] = -1.f;
    return in;
}

bool fullscreen_saved(const fs::path &data) {
    std::FILE *f = std::fopen((data / "fullscreen").c_str(), "r");
    if (!f) return false;
    const int c = std::fgetc(f);
    std::fclose(f);
    return c == '1';
}

bool g_fullscreen = false;

void set_fullscreen(SDL_Window *w, bool on, const fs::path &data) {
    g_fullscreen = on;
    SDL_SetWindowFullscreen(w, on);
    std::error_code ec;
    fs::create_directories(data, ec);
    if (std::FILE *f = std::fopen((data / "fullscreen").c_str(), "w")) {
        std::fputs(on ? "1\n" : "0\n", f);
        std::fclose(f);
    }
}

bool fullscreen_key(const SDL_Event &e, SDL_Window *w, const fs::path &data) {
    if (e.type != SDL_EVENT_KEY_DOWN || e.key.repeat) return false;
    if (e.key.scancode != SDL_SCANCODE_F11 && !(e.key.scancode == SDL_SCANCODE_RETURN && (e.key.mod & SDL_KMOD_ALT))) return false;
    set_fullscreen(w, !g_fullscreen, data);
    return true;
}

int pad_button(SDL_GamepadButton b) {
    switch (b) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return kA;
    case SDL_GAMEPAD_BUTTON_EAST: return kB;
    case SDL_GAMEPAD_BUTTON_WEST: return kX;
    case SDL_GAMEPAD_BUTTON_NORTH: return kY;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return kL1;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return kR1;
    case SDL_GAMEPAD_BUTTON_BACK: return kBack;
    case SDL_GAMEPAD_BUTTON_START: return kStart;
    default: return -1;
    }
}

void draw_buttons(SDL_Renderer *r, const Game &g, const ButtonMap &map, int row, bool waiting) {
    float sx = 0, sy = 0;
    SDL_GetRenderScale(r, &sx, &sy);
    const SDL_FPoint at = menu_frame(r);
    const float x = at.x - 40, y = at.y - 40;
    SDL_RenderDebugText(r, x, y, "CONTROLLER BUTTONS");
    SDL_RenderDebugText(r, x, y + 12, g.title.c_str());
    for (int i = 0; i < kButtonRows; ++i) {
        std::string line = i == row ? "> " : "  ";
        if (i < kPadButtons) {
            char b[96];
            std::snprintf(b, sizeof b, "%-22s %s", kPadNames[i],
                          waiting && i == row ? "press a button..." : map.to[i] == i ? "-" : kPadNames[map.to[i]]);
            line += b;
        } else line += i == kPadButtons ? "Reset to the default" : "Back";
        SDL_RenderDebugText(r, x, y + 34 + 14 * i, line.c_str());
    }
    SDL_RenderDebugText(r, x, y + 44 + 14 * kButtonRows,
                        waiting ? "Press its new button (Esc cancels)."
                                : "Choose a row, then press its new button.");
    SDL_SetRenderScale(r, sx, sy);
}

void buttons_event(const SDL_Event &e, ButtonMap &map, const fs::path &file, int &row, bool &waiting) {
    const bool key = e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat, padb = e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    const auto is = [&](SDL_Scancode s, SDL_GamepadButton b) {
        return (key && e.key.scancode == s) || (padb && e.gbutton.button == b);
    };
    if (waiting) {
        int pressed = padb ? pad_button(SDL_GamepadButton(e.gbutton.button)) : -1;
        if (e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION && e.gaxis.value > 24000) {
            if (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER) pressed = kL2;
            if (e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) pressed = kR2;
        }
        if (pressed >= 0) {
            map.to[row] = pressed;
            map.save(file);
            waiting = false;
        } else if (key && (e.key.scancode == SDL_SCANCODE_ESCAPE || e.key.scancode == SDL_SCANCODE_BACKSPACE)) waiting = false;
        return;
    }
    if (is(SDL_SCANCODE_UP, SDL_GAMEPAD_BUTTON_DPAD_UP)) row = std::max(0, row - 1);
    if (is(SDL_SCANCODE_DOWN, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) row = std::min(kButtonRows - 1, row + 1);
    const bool pick = is(SDL_SCANCODE_RETURN, SDL_GAMEPAD_BUTTON_SOUTH);
    if (pick && row < kPadButtons) waiting = true;
    else if (pick && row == kPadButtons) {
        map.reset();
        map.save(file);
    } else if (pick || is(SDL_SCANCODE_BACKSPACE, SDL_GAMEPAD_BUTTON_EAST) || (key && e.key.scancode == SDL_SCANCODE_ESCAPE))
        row = -1; // back
}

bool is_gun_game(const Game &g) {
    return g.controls == GameControls::Gun || g.controls == GameControls::Turret || g.controls == GameControls::AnalogGun;
}

bool gun_start_game(const Game &g) {
    return g.romset == "letsgoju" || g.romset == "letsgojua" || g.romset == "letsgojusp";
}

bool shows_crosshair(const Game &g) {
    return is_gun_game(g) && g.controls != GameControls::Turret;
}

bool crosshair_by_default(const std::string &romset) {
    return romset != "letsgoju" && romset != "letsgojua" && romset != "letsgojusp";
}

bool crosshair_on(const Options &o, const std::string &romset) {
    std::FILE *f = std::fopen((o.data / "crosshair" / romset).c_str(), "r");
    if (!f) return crosshair_by_default(romset);
    const int c = std::fgetc(f);
    std::fclose(f);
    return c != '0';
}

void save_crosshair(const Options &o, const std::string &romset, bool on) {
    std::error_code ec;
    fs::create_directories(o.data / "crosshair", ec);
    if (std::FILE *f = std::fopen((o.data / "crosshair" / romset).c_str(), "w")) {
        std::fputs(on ? "1\n" : "0\n", f);
        std::fclose(f);
    }
}

std::vector<std::string> core_regions(const Game &g) {
    std::vector<std::string> out;
    if (!g.api || g.api->size < offsetof(afcore_api, set_option) + sizeof g.api->set_option || !g.api->options) return out;
    const std::string options = g.api->options;
    for (size_t at = 0; at < options.size();) {
        const size_t nl = std::min(options.find('\n', at), options.size());
        const std::string line = options.substr(at, nl - at);
        if (line.rfind("region=", 0) == 0)
            for (size_t v = 7; v <= line.size();) {
                const size_t bar = std::min(line.find('|', v), line.size());
                if (bar > v) out.push_back(line.substr(v, bar - v));
                v = bar + 1;
            }
        at = nl + 1;
    }
    return out;
}

std::string region_of(const Options &o, const Game &g) {
    const std::vector<std::string> regions = core_regions(g);
    if (regions.empty()) return "";
    std::string chosen;
    if (std::FILE *f = std::fopen((o.data / "region" / g.romset).c_str(), "r")) {
        char buf[32] = "";
        if (std::fgets(buf, sizeof buf, f)) chosen = buf;
        std::fclose(f);
        while (!chosen.empty() && (chosen.back() == '\n' || chosen.back() == '\r')) chosen.pop_back();
    }
    for (const std::string &r : regions)
        if (r == chosen) return r;
    return regions[0];
}

void save_region(const Options &o, const std::string &romset, const std::string &region) {
    std::error_code ec;
    fs::create_directories(o.data / "region", ec);
    if (std::FILE *f = std::fopen((o.data / "region" / romset).c_str(), "w")) {
        std::fprintf(f, "%s\n", region.c_str());
        std::fclose(f);
    }
}

} // namespace insert_coin
