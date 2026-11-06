// Steering wheels and pedals (issue #318): the Logitech Driving Force GT
// and G29 first, any wheel SDL knows as one. SDL never lists a wheel as a
// gamepad, so it is opened as a joystick beside them.
//
// A wheel's setup is kept per model (USB vendor and product) in
// <data>/wheels/<vvvv>-<pppp> as key=value lines: which axis steers and
// where it is centred, which axes are the gas and brake pedals and their
// resting and fully pressed values (learned by calibration: a pedal can run
// either way), how far the wheel turns lock to lock and how far of that is
// the game's full lock (the steering range), a dead zone, and the wheel's
// button for each cabinet control, and how strongly its motor pulls it back
// to the centre (self-centring).
#pragma once

#include "app.h"

namespace insert_coin {

// The cabinet's controls a wheel's buttons can be: the shifter and the
// paddles (both shift), the view buttons, coin, start, the hand brake (Sega
// Rally) and the pause menu.
enum WheelAction { kWShiftUp, kWShiftDown, kWPaddleUp, kWPaddleDown, kWView1, kWView2, kWView3, kWView4,
                   kWCoin, kWStart, kWHandbrake, kWPause, kWActions };

const char *const kWheelActionKeys[kWActions] = {"shift_up", "shift_down", "paddle_up", "paddle_down", "view1", "view2",
                                                 "view3",    "view4",      "coin",      "start",       "handbrake", "pause"};

const char *const kWheelActionNames[kWActions] = {"Shift up", "Shift down", "Paddle up", "Paddle down", "View 1", "View 2",
                                                  "View 3",   "View 4",     "Coin",      "Start",       "Hand brake", "Pause menu"};

struct WheelSetup {
    int steer_axis = 0;
    int steer_center = 0;
    int gas_axis = -1, gas_rest = 0, gas_full = 0;       // -1: not calibrated yet
    int brake_axis = -1, brake_rest = 0, brake_full = 0;
    int lock = 900;  // degrees the wheel turns lock to lock (its driver's range)
    int range = 270; // degrees of that which are the game's full lock, close to a cabinet's wheel
    int deadzone = 0; // percent of full lock around the centre that counts as straight
    // How progressive the steering is, 0..100: 0 straight (linear), 100 a
    // cubic curve. Between them the turn is the two mixed: at 50 the centre
    // is half as quick as linear and the last part of the turn twice as quick.
    int curve = 50;
    // Self-centring, 0..100: how strongly the wheel's motor pulls it back to
    // the centre, as a cabinet's spring does (0 off). The games send no force
    // feedback yet, so without it a force-feedback wheel turns freely.
    int centring = 50;
    int button[kWActions];

    WheelSetup() { std::fill(button, button + kWActions, -1); }
    // The defaults for a wheel model: how far it turns, and its buttons where we know them.
    static WheelSetup defaults(uint16_t vendor, uint16_t product);
    static WheelSetup load(const fs::path &file, uint16_t vendor, uint16_t product);
    void save(const fs::path &file) const;
};

struct Wheel {
    SDL_Joystick *joy = nullptr;
    SDL_Haptic *haptic = nullptr; // its motor, when it can self-centre
    SDL_JoystickID id = 0;
    uint16_t vendor = 0, product = 0;
    std::string name;
    fs::path file; // its setup's file
    WheelSetup s;

    // Opens the first wheel connected, when none is open; its setup from <data>/wheels.
    void open(const fs::path &data);
    // A joystick went away: closes the wheel if it was this one.
    void removed(SDL_JoystickID which);
    // At exit: lets the wheel go (the spring off, as the driver starts it) and closes it.
    void close();
    // Sets the self-centring spring to the setup's strength (when the wheel has one).
    void centre_spring() const;
    bool can_centre() const { return haptic != nullptr; }

    int axis(int a) const { return joy && a >= 0 ? SDL_GetJoystickAxis(joy, a) : 0; }
    bool raw_button(int b) const { return joy && b >= 0 && SDL_GetJoystickButton(joy, b); }
    bool held(WheelAction a) const { return raw_button(s.button[a]); }
    // -1 full left .. +1 full right, over the steering range, with the dead zone.
    float steer() const;
    // 0 .. 1, from the calibrated resting and fully pressed values.
    float pedal(int axis_, int rest, int full) const;
    float gas() const { return pedal(s.gas_axis, s.gas_rest, s.gas_full); }
    float brake() const { return pedal(s.brake_axis, s.brake_rest, s.brake_full); }
    bool pedals_set() const { return s.gas_axis >= 0 && s.brake_axis >= 0; }

    int hat = 0; // the d-pad's last position (SDL_HAT_*), for wheel_as_pad
};

// The wheel's d-pad and buttons as a gamepad's in the library and the menus:
// for a joystick event from the wheel, `out` gets the gamepad event it
// stands for and true is returned, for the caller to push. Logitech's
// PlayStation layout (Driving Force GT, G29; HID button numbers): 1-4 Cross,
// Square, Circle, Triangle (South, West, East, North), 5/6 R1/L1 (the
// paddles: the shoulders), 7/8 R2/L2 (the triggers, as full and released
// axis moves), 9 Select (Back), 10 Start, 11/12 R3/L3 (the sticks' clicks),
// 21 PS (Guide); the d-pad as the d-pad. Such an event carries the wheel's id in
// gbutton.which, so the wheel's own screen can tell it from a gamepad's.
bool wheel_as_pad(const SDL_Event &e, Wheel &w, SDL_Event &out);

// The wheel's screen (pause menu, Wheel and pedals): the live steering and
// pedals, the steering range, the wheel's full turn, the dead zone,
// self-centring, calibration and the buttons.
struct WheelScreen {
    enum Row { kRange, kCurve, kLock, kDeadzone, kCentring, kCalibrate, kButtons, kReset, kBack, kRows };
    // Calibration's steps: let go of everything (the resting values), turn
    // the wheel (its axis), press the gas, press the brake.
    enum Step { kNone, kRest, kTurn, kGas, kBrake };
    int row = -1; // -1: closed
    Step step = kNone;
    int button_row = -1;  // the buttons' list is open: the row chosen (kWActions: Back)
    bool waiting = false; // for the wheel's button to put on that row
    std::vector<int> rest; // every axis at rest (calibration)
    int found = -1;        // the axis the current step has found, once pushed most of the way
    int found_peak = 0;

    bool open() const { return row >= 0; }
    void start() { row = 0, step = kNone, button_row = -1, waiting = false; }
};

void draw_wheel(SDL_Renderer *r, const Wheel &w, const WheelScreen &ws);

// The keys and gamepad (Up/Down a row, Left/Right change it, Return or South
// picks, Esc, Backspace or East back) and, while calibrating or learning a
// button, the wheel's own axes and buttons.
void wheel_event(const SDL_Event &e, Wheel &w, WheelScreen &ws);

// Calibration watches the wheel's axes each frame (not every axis sends an
// event when it starts already pushed).
void wheel_tick(Wheel &w, WheelScreen &ws);

} // namespace insert_coin
