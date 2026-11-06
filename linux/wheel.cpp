#include "wheel.h"
#include "render.h"

namespace insert_coin {

namespace {

// Logitech's wheels and how far each turns lock to lock under Linux's
// hid-logitech driver (its default range). The Driving Force GT starts as a
// Driving Force (c294, 200 degrees) and the driver switches it to its own
// mode (c29a, 900 degrees).
int default_lock(uint16_t vendor, uint16_t product) {
    if (vendor != 0x046d) return 900;
    switch (product) {
    case 0xc294: return 200; // Driving Force (EX), or a wheel still in that mode
    case 0xc295: return 240; // Momo Force
    case 0xca03: return 240; // Momo Racing
    default: return 900;     // Driving Force Pro and GT, G25, G27, G29, G920, G923, PRO
    }
}

int clampi(int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }

} // namespace

WheelSetup WheelSetup::defaults(uint16_t vendor, uint16_t product) {
    WheelSetup s;
    s.lock = default_lock(vendor, product);
    s.range = std::min(s.range, s.lock);
    if (vendor == 0x046d && product == 0xc29a) {
        // The Driving Force GT under hid-logitech (evtest, 2026-11-03):
        // ABS_X the wheel (0..16383), ABS_Y the gas and ABS_Z the brake, each
        // 255 at rest and 0 pressed (SDL: +32767 .. -32768). SDL numbers the
        // buttons in key code order, which here is the HID button number - 1:
        // 1-4 the face buttons, 9 Select, 10 Start, 13/14 the shifter, 21 PS.
        s.steer_axis = 0;
        s.gas_axis = 1, s.gas_rest = 32767, s.gas_full = -32768;
        s.brake_axis = 2, s.brake_rest = 32767, s.brake_full = -32768;
        const int b[kWActions] = {12, 13, 4, 5, 0, 1, 2, 3, 8, 9, -1, 20}; // paddles: 5 right (up), 6 left (down)
        std::copy(b, b + kWActions, s.button);
    }
    return s;
}

WheelSetup WheelSetup::load(const fs::path &file, uint16_t vendor, uint16_t product) {
    WheelSetup s = defaults(vendor, product);
    std::FILE *f = std::fopen(file.c_str(), "r");
    if (!f) return s;
    char line[128];
    while (std::fgets(line, sizeof line, f)) {
        std::string t(line);
        while (!t.empty() && (t.back() == '\n' || t.back() == '\r')) t.pop_back();
        const size_t eq = t.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = t.substr(0, eq);
        const int v = std::atoi(t.c_str() + eq + 1);
        if (k == "steer_axis") s.steer_axis = v;
        else if (k == "steer_center") s.steer_center = v;
        else if (k == "gas_axis") s.gas_axis = v;
        else if (k == "gas_rest") s.gas_rest = v;
        else if (k == "gas_full") s.gas_full = v;
        else if (k == "brake_axis") s.brake_axis = v;
        else if (k == "brake_rest") s.brake_rest = v;
        else if (k == "brake_full") s.brake_full = v;
        else if (k == "lock") s.lock = clampi(v, 90, 1080);
        else if (k == "range") s.range = clampi(v, 30, 1080);
        else if (k == "deadzone") s.deadzone = clampi(v, 0, 20);
        else if (k == "curve") s.curve = clampi(v, 0, 100);
        else if (k == "centring") s.centring = clampi(v, 0, 100);
        else
            for (int a = 0; a < kWActions; ++a)
                if (k == kWheelActionKeys[a]) s.button[a] = v;
    }
    std::fclose(f);
    s.range = std::min(s.range, s.lock);
    return s;
}

void WheelSetup::save(const fs::path &file) const {
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    std::FILE *f = std::fopen(file.c_str(), "w");
    if (!f) return;
    std::fprintf(f, "steer_axis=%d\nsteer_center=%d\ngas_axis=%d\ngas_rest=%d\ngas_full=%d\nbrake_axis=%d\nbrake_rest=%d\nbrake_full=%d\n",
                 steer_axis, steer_center, gas_axis, gas_rest, gas_full, brake_axis, brake_rest, brake_full);
    std::fprintf(f, "lock=%d\nrange=%d\ndeadzone=%d\ncurve=%d\ncentring=%d\n", lock, range, deadzone, curve, centring);
    for (int a = 0; a < kWActions; ++a)
        if (button[a] >= 0) std::fprintf(f, "%s=%d\n", kWheelActionKeys[a], button[a]);
    std::fclose(f);
}

void Wheel::open(const fs::path &data) {
    if (joy) return;
    int n = 0;
    SDL_JoystickID *ids = SDL_GetJoysticks(&n);
    for (int i = 0; ids && i < n && !joy; ++i) {
        if (SDL_GetJoystickTypeForID(ids[i]) != SDL_JOYSTICK_TYPE_WHEEL) continue;
        joy = SDL_OpenJoystick(ids[i]);
        if (!joy) continue;
        id = ids[i];
        vendor = SDL_GetJoystickVendor(joy);
        product = SDL_GetJoystickProduct(joy);
        const char *nm = SDL_GetJoystickName(joy);
        name = nm ? nm : "Wheel";
        char key[16];
        std::snprintf(key, sizeof key, "%04x-%04x", vendor, product);
        file = data / "wheels" / key;
        s = WheelSetup::load(file, vendor, product);
        // Self-centring is the driver's autocentre (Linux FF_AUTOCENTER),
        // which needs the wheel's event device to be writable.
        if (SDL_IsJoystickHaptic(joy)) haptic = SDL_OpenHapticFromJoystick(joy);
        if (haptic && !(SDL_GetHapticFeatures(haptic) & SDL_HAPTIC_AUTOCENTER)) {
            SDL_CloseHaptic(haptic);
            haptic = nullptr;
        }
        centre_spring();
        std::printf("insert-coin: wheel %s (%s), %d axes, %d buttons, %s%s\n", name.c_str(), key, SDL_GetNumJoystickAxes(joy),
                    SDL_GetNumJoystickButtons(joy), haptic ? "self-centring" : "no self-centring",
                    pedals_set() ? "" : "; calibrate it under Wheel and pedals in the pause menu");
    }
    SDL_free(ids);
}

void Wheel::removed(SDL_JoystickID which) {
    if (!joy || which != id) return;
    if (haptic) SDL_CloseHaptic(haptic);
    haptic = nullptr;
    SDL_CloseJoystick(joy);
    joy = nullptr;
    id = 0;
}

void Wheel::close() {
    if (haptic) SDL_SetHapticAutocenter(haptic, 0);
    if (joy) removed(id);
}

void Wheel::centre_spring() const {
    if (haptic && !SDL_SetHapticAutocenter(haptic, clampi(s.centring, 0, 100)))
        std::printf("insert-coin: wheel self-centring: %s\n", SDL_GetError());
}

float Wheel::steer() const {
    if (!joy) return 0;
    const int v = axis(s.steer_axis), c = clampi(s.steer_center, -16384, 16383);
    // -1 .. +1 across the wheel's whole turn, either side of where it rests
    const float n = v >= c ? float(v - c) / float(32767 - c) : float(v - c) / float(c + 32768);
    const float degrees = n * s.lock * 0.5f;
    float out = std::clamp(degrees / (std::max(s.range, 30) * 0.5f), -1.f, 1.f);
    const float dz = s.deadzone / 100.f;
    if (std::fabs(out) <= dz) return 0;
    const float x = (std::fabs(out) - dz) / (1 - dz), k = s.curve / 100.f;
    return std::copysign((1 - k) * x + k * x * x * x, out); // gentle near the centre, quicker towards full lock
}

bool wheel_as_pad(const SDL_Event &e, Wheel &w, SDL_Event &out) {
    if (!w.joy) return false;
    SDL_GamepadButton b = SDL_GAMEPAD_BUTTON_INVALID;
    if (e.type == SDL_EVENT_JOYSTICK_HAT_MOTION && e.jhat.which == w.id) {
        const int now = e.jhat.value, pressed = now & ~w.hat;
        w.hat = now;
        if (pressed & SDL_HAT_UP) b = SDL_GAMEPAD_BUTTON_DPAD_UP;
        else if (pressed & SDL_HAT_DOWN) b = SDL_GAMEPAD_BUTTON_DPAD_DOWN;
        else if (pressed & SDL_HAT_LEFT) b = SDL_GAMEPAD_BUTTON_DPAD_LEFT;
        else if (pressed & SDL_HAT_RIGHT) b = SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
    } else if ((e.type == SDL_EVENT_JOYSTICK_BUTTON_DOWN || e.type == SDL_EVENT_JOYSTICK_BUTTON_UP) && e.jbutton.which == w.id) {
        const int n = e.jbutton.button;
        if (n == 6 || n == 7) { // R2, L2: a trigger pulled all the way, or let go
            out = SDL_Event{};
            out.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
            out.gaxis.which = w.id;
            out.gaxis.axis = Uint8(n == 6 ? SDL_GAMEPAD_AXIS_RIGHT_TRIGGER : SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
            out.gaxis.value = e.type == SDL_EVENT_JOYSTICK_BUTTON_DOWN ? 32767 : 0;
            return true;
        }
        if (e.type == SDL_EVENT_JOYSTICK_BUTTON_UP) return false;
        static const SDL_GamepadButton pad[] = {
            SDL_GAMEPAD_BUTTON_SOUTH,          SDL_GAMEPAD_BUTTON_WEST,          SDL_GAMEPAD_BUTTON_EAST,  SDL_GAMEPAD_BUTTON_NORTH,
            SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_INVALID, SDL_GAMEPAD_BUTTON_INVALID,
            SDL_GAMEPAD_BUTTON_BACK,           SDL_GAMEPAD_BUTTON_START,         SDL_GAMEPAD_BUTTON_RIGHT_STICK, SDL_GAMEPAD_BUTTON_LEFT_STICK};
        if (n < int(std::size(pad))) b = pad[n];
        else if (n == 20) b = SDL_GAMEPAD_BUTTON_GUIDE;
    }
    if (b == SDL_GAMEPAD_BUTTON_INVALID) return false;
    out = SDL_Event{};
    out.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    out.gbutton.which = w.id;
    out.gbutton.button = Uint8(b);
    out.gbutton.down = true;
    return true;
}

float Wheel::pedal(int a, int rest, int full) const {
    if (!joy || a < 0 || full == rest) return 0;
    const float t = float(axis(a) - rest) / float(full - rest);
    return t < 0.03f ? 0.f : std::min(1.f, (t - 0.03f) / 0.94f); // a little slack at the top, where a pedal rests
}

namespace {

// A bar from -1..1 (centred) or 0..1, `w` wide.
void bar(SDL_Renderer *r, float x, float y, float w, float v, bool centred) {
    const SDL_FRect frame{x, y, w, 7};
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_RenderRect(r, &frame);
    SDL_SetRenderDrawColor(r, 255, 170, 0, 255);
    SDL_FRect fill{x + 1, y + 1, 0, 5};
    if (centred) {
        const float mid = x + w / 2, len = (w / 2 - 1) * std::clamp(v, -1.f, 1.f);
        fill.x = len < 0 ? mid + len : mid;
        fill.w = std::fabs(len);
    } else fill.w = (w - 2) * std::clamp(v, 0.f, 1.f);
    SDL_RenderFillRect(r, &fill);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
}

const char *step_text(WheelScreen::Step s) {
    switch (s) {
    case WheelScreen::kRest: return "Centre the wheel, let go of the pedals, then press Return or a wheel button.";
    case WheelScreen::kTurn: return "Turn the wheel to one side.";
    case WheelScreen::kGas: return "Press the gas pedal all the way, then let go.";
    case WheelScreen::kBrake: return "Press the brake pedal all the way, then let go.";
    default: return "";
    }
}

// Long lines over two (the debug font at twice the size fits about 40 characters).
void wrapped(SDL_Renderer *r, float x, float y, const std::string &text) {
    const size_t width = 40;
    std::string line;
    for (size_t at = 0; at < text.size();) {
        size_t sp = text.find(' ', at);
        if (sp == std::string::npos) sp = text.size();
        const std::string word = text.substr(at, sp - at);
        if (!line.empty() && line.size() + 1 + word.size() > width) {
            SDL_RenderDebugText(r, x, y, line.c_str());
            y += 12;
            line.clear();
        }
        line += (line.empty() ? "" : " ") + word;
        at = sp + 1;
    }
    if (!line.empty()) SDL_RenderDebugText(r, x, y, line.c_str());
}

} // namespace

void draw_wheel(SDL_Renderer *r, const Wheel &w, const WheelScreen &ws) {
    float sx = 0, sy = 0;
    SDL_GetRenderScale(r, &sx, &sy);
    const SDL_FPoint at = menu_frame(r);
    const float x = at.x - 60, y = at.y - 60;
    SDL_RenderDebugText(r, x, y, "WHEEL AND PEDALS");
    if (!w.joy) {
        SDL_RenderDebugText(r, x, y + 12, "No wheel connected.");
        wrapped(r, x, y + 34, "Connect one (SDL must know it as a wheel: Logitech's are), then come back. Esc, Backspace or East goes back.");
        SDL_SetRenderScale(r, sx, sy);
        return;
    }
    SDL_RenderDebugText(r, x, y + 12, w.name.substr(0, 40).c_str());
    SDL_RenderDebugText(r, x, y + 30, "Steering");
    bar(r, x + 80, y + 30, 160, w.steer(), true);
    SDL_RenderDebugText(r, x, y + 42, "Gas");
    bar(r, x + 80, y + 42, 160, w.gas(), false);
    SDL_RenderDebugText(r, x, y + 54, "Brake");
    bar(r, x + 80, y + 54, 160, w.brake(), false);
    float ty = y + 74;
    if (ws.button_row >= 0) {
        for (int a = 0; a <= kWActions; ++a) {
            std::string line = a == ws.button_row ? "> " : "  ";
            if (a < kWActions) {
                char b[64];
                if (ws.waiting && a == ws.button_row) std::snprintf(b, sizeof b, "%-12s press a wheel button", kWheelActionNames[a]);
                else if (w.s.button[a] < 0) std::snprintf(b, sizeof b, "%-12s -", kWheelActionNames[a]);
                else std::snprintf(b, sizeof b, "%-12s button %d%s", kWheelActionNames[a], w.s.button[a] + 1, w.held(WheelAction(a)) ? " *" : "");
                line += b;
            } else line += "Back";
            SDL_RenderDebugText(r, x, ty, line.c_str());
            ty += 12;
        }
        std::string held; // which buttons are down now, to find one's number
        for (int b = 0; b < SDL_GetNumJoystickButtons(w.joy); ++b)
            if (w.raw_button(b)) held += " " + std::to_string(b + 1);
        wrapped(r, x, ty + 8, ws.waiting ? "Press the wheel's button for it (Esc cancels, Delete clears)."
                              : held.empty() ? "Choose a row, then press the wheel's button for it."
                                             : "Held now:" + held);
        SDL_SetRenderScale(r, sx, sy);
        return;
    }
    if (ws.step != WheelScreen::kNone) {
        SDL_RenderDebugText(r, x, ty, "CALIBRATING (Esc stops)");
        wrapped(r, x, ty + 18, step_text(ws.step));
        SDL_SetRenderScale(r, sx, sy);
        return;
    }
    char range[48], curve[48], lock[48], dz[48], centring[48];
    std::snprintf(range, sizeof range, "Steering range: %d deg", w.s.range);
    std::snprintf(curve, sizeof curve, "Steering curve: %d%%", w.s.curve);
    std::snprintf(lock, sizeof lock, "Wheel turns: %d deg", w.s.lock);
    std::snprintf(dz, sizeof dz, "Dead zone: %d%%", w.s.deadzone);
    if (!w.can_centre()) std::snprintf(centring, sizeof centring, "Self-centring: not on this wheel");
    else if (w.s.centring == 0) std::snprintf(centring, sizeof centring, "Self-centring: off");
    else std::snprintf(centring, sizeof centring, "Self-centring: %d%%", w.s.centring);
    const char *const rows[WheelScreen::kRows] = {range, curve, lock, dz, centring, w.pedals_set() ? "Calibrate" : "Calibrate (not done yet)",
                                                  "Wheel buttons", "Reset this wheel", "Back"};
    for (int i = 0; i < WheelScreen::kRows; ++i) {
        const std::string line = std::string(i == ws.row ? "> " : "  ") + rows[i];
        SDL_RenderDebugText(r, x, ty, line.c_str());
        ty += 12;
    }
    static const char *const help[WheelScreen::kRows] = {
        "Left/Right: how far to turn for the game's full lock. A cabinet's wheel turns about 270 degrees.",
        "Left/Right: 0% steers in a straight line; more is gentler near the centre and quicker towards full lock.",
        "Left/Right: the wheel's own turn lock to lock, as its driver sets it (900 for the Driving Force GT and G29).",
        "Left/Right: how much around the centre counts as straight ahead.",
        "Left/Right: how strongly the wheel pulls itself back to the centre, like a cabinet's spring (0 is off).",
        "Return: learn the steering axis and the pedals.",
        "Return: choose the wheel's buttons for the shifter, views, coin, start and the pause menu.",
        "Return: forget this wheel's calibration and buttons.",
        ""};
    wrapped(r, x, ty + 8, help[ws.row]);
    SDL_SetRenderScale(r, sx, sy);
}

void wheel_event(const SDL_Event &e, Wheel &w, WheelScreen &ws) {
    const bool key = e.type == SDL_EVENT_KEY_DOWN;
    // the wheel's own buttons count as themselves here, not as the gamepad buttons wheel_as_pad makes of them
    if ((e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) && w.joy && e.gbutton.which == w.id) return;
    const bool padb = e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    const bool wheelb = e.type == SDL_EVENT_JOYSTICK_BUTTON_DOWN && w.joy && e.jbutton.which == w.id;
    const auto is = [&](SDL_Scancode s, SDL_GamepadButton b) {
        return (key && e.key.scancode == s) || (padb && e.gbutton.button == b);
    };
    const bool back = is(SDL_SCANCODE_ESCAPE, SDL_GAMEPAD_BUTTON_EAST) || (key && e.key.scancode == SDL_SCANCODE_BACKSPACE);
    const bool pick = is(SDL_SCANCODE_RETURN, SDL_GAMEPAD_BUTTON_SOUTH);
    if (!w.joy) {
        if (back || pick) ws.row = -1;
        return;
    }
    if (ws.step != WheelScreen::kNone) { // calibrating: wheel_tick does the rest
        if (back) {
            ws.step = WheelScreen::kNone;
            w.s = WheelSetup::load(w.file, w.vendor, w.product); // as it was
        } else if (ws.step == WheelScreen::kRest && (pick || wheelb)) {
            const int n = SDL_GetNumJoystickAxes(w.joy);
            ws.rest.assign(size_t(std::max(n, 0)), 0);
            for (int a = 0; a < n; ++a) ws.rest[size_t(a)] = w.axis(a);
            ws.found = -1;
            ws.step = WheelScreen::kTurn;
        }
        return;
    }
    if (ws.button_row >= 0) {
        if (ws.waiting) {
            if (wheelb) {
                w.s.button[ws.button_row] = e.jbutton.button;
                w.s.save(w.file);
                ws.waiting = false;
            } else if (key && e.key.scancode == SDL_SCANCODE_DELETE) {
                w.s.button[ws.button_row] = -1;
                w.s.save(w.file);
                ws.waiting = false;
            } else if (back) ws.waiting = false;
            return;
        }
        if (is(SDL_SCANCODE_UP, SDL_GAMEPAD_BUTTON_DPAD_UP)) ws.button_row = std::max(0, ws.button_row - 1);
        if (is(SDL_SCANCODE_DOWN, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) ws.button_row = std::min(int(kWActions), ws.button_row + 1);
        if (pick && ws.button_row < kWActions) ws.waiting = true;
        else if (pick || back) ws.button_row = -1;
        return;
    }
    if (is(SDL_SCANCODE_UP, SDL_GAMEPAD_BUTTON_DPAD_UP)) ws.row = std::max(0, ws.row - 1);
    if (is(SDL_SCANCODE_DOWN, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) ws.row = std::min(WheelScreen::kRows - 1, ws.row + 1);
    const int step = is(SDL_SCANCODE_RIGHT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) ? 1 : is(SDL_SCANCODE_LEFT, SDL_GAMEPAD_BUTTON_DPAD_LEFT) ? -1 : 0;
    if (step) {
        if (ws.row == WheelScreen::kRange) w.s.range = clampi(w.s.range + 10 * step, 30, w.s.lock);
        if (ws.row == WheelScreen::kLock) {
            w.s.lock = clampi(w.s.lock + 10 * step, 90, 1080);
            w.s.range = std::min(w.s.range, w.s.lock);
        }
        if (ws.row == WheelScreen::kDeadzone) w.s.deadzone = clampi(w.s.deadzone + step, 0, 20);
        if (ws.row == WheelScreen::kCurve) w.s.curve = clampi(w.s.curve + 10 * step, 0, 100);
        if (ws.row == WheelScreen::kCentring && w.can_centre()) {
            w.s.centring = clampi(w.s.centring + 10 * step, 0, 100);
            w.centre_spring();
        }
        if (ws.row <= WheelScreen::kCentring) w.s.save(w.file);
    }
    if (back || (pick && ws.row == WheelScreen::kBack)) ws.row = -1;
    else if (pick && ws.row == WheelScreen::kCalibrate) ws.step = WheelScreen::kRest;
    else if (pick && ws.row == WheelScreen::kButtons) ws.button_row = 0, ws.waiting = false;
    else if (pick && ws.row == WheelScreen::kReset) {
        std::error_code ec;
        fs::remove(w.file, ec);
        w.s = WheelSetup::defaults(w.vendor, w.product);
        w.centre_spring();
    }
}

void wheel_tick(Wheel &w, WheelScreen &ws) {
    if (!w.joy || ws.step < WheelScreen::kTurn) return;
    const int n = std::min(SDL_GetNumJoystickAxes(w.joy), int(ws.rest.size()));
    const auto rest = [&](int a) { return ws.rest[size_t(a)]; };
    if (ws.step == WheelScreen::kTurn) {
        // The axis turned furthest from where it rested, once past a quarter of its travel.
        int best = -1, far = 16384 / 2;
        for (int a = 0; a < n; ++a)
            if (std::abs(w.axis(a) - rest(a)) > far) best = a, far = std::abs(w.axis(a) - rest(a));
        if (best >= 0) {
            w.s.steer_axis = best;
            w.s.steer_center = rest(best);
            ws.found = -1;
            ws.step = WheelScreen::kGas;
        }
        return;
    }
    // A pedal: the axis (not the steering's, nor the gas's) pressed past half
    // its travel; its furthest value is fully pressed, and it counts once let go.
    if (ws.found < 0) {
        for (int a = 0; a < n; ++a) {
            if (a == w.s.steer_axis || (ws.step == WheelScreen::kBrake && a == w.s.gas_axis)) continue;
            if (std::abs(w.axis(a) - rest(a)) > 32768 / 2) {
                ws.found = a;
                ws.found_peak = w.axis(a);
                break;
            }
        }
        return;
    }
    const int a = ws.found, v = w.axis(a);
    if (std::abs(v - rest(a)) > std::abs(ws.found_peak - rest(a))) ws.found_peak = v;
    if (std::abs(v - rest(a)) > 32768 / 8) return; // still pressed
    if (ws.step == WheelScreen::kGas) {
        w.s.gas_axis = a, w.s.gas_rest = rest(a), w.s.gas_full = ws.found_peak;
        ws.step = WheelScreen::kBrake;
    } else {
        w.s.brake_axis = a, w.s.brake_rest = rest(a), w.s.brake_full = ws.found_peak;
        ws.step = WheelScreen::kNone;
        w.s.save(w.file);
    }
    ws.found = -1;
}

} // namespace insert_coin
