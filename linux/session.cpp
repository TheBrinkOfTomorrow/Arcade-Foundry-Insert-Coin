#include "session.h"

#include "runner.h"
#include "art.h"
#include "library.h"
#include "render.h"
#include "states.h"

#include "afcommon/tar.h"

#include <atomic>
#include <cinttypes>
#include <cstddef>
#include <cstdlib>
#include <map>
#include <mutex>
#include <thread>

namespace insert_coin {

namespace {

// Tells the player why a game didn't start or stopped: a message box, or
// for Game Cabinet (o.report) the report, which its Game Director reads.
void tell(const Options &o, const std::string &text) {
    if (o.report) {
        std::fprintf(stderr, "insert-coin: %s\n", text.c_str());
        o.report->fail(text);
    } else {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Arcade Foundry: Insert Coin", text.c_str(), nullptr);
    }
}

// The game's sound streams (the sound board's two, Star Wars Arcade's
// Digital Sound Board), each at its own rate, into SDL audio
// streams on one device (SDL resamples and mixes them).
struct Audio {
    SDL_AudioDeviceID dev = 0;
    SDL_AudioStream *streams[3] = {};

    void open(const afcore_api *api, afcore *core) {
        dev = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
        if (!dev) return;
        for (int s = 0; s < 3; ++s) {
            const double rate = api->audio_rate(core, s);
            if (rate <= 0) continue;
            const SDL_AudioSpec spec{SDL_AUDIO_F32, 2, int(rate + 0.5)};
            streams[s] = SDL_CreateAudioStream(&spec, nullptr);
            if (streams[s]) SDL_BindAudioStream(dev, streams[s]);
        }
    }
    // `sound`: Game Cabinet's copy of each batch (PlayReport::on_sound).
    void push(const afcore_api *api, afcore *core, PlayReport *report = nullptr) {
        static float buf[2 * 4096];
        for (int s = 0; s < 3; ++s) {
            size_t n;
            while ((n = api->audio_read(core, s, buf, 4096)) > 0) {
                if (streams[s]) SDL_PutAudioStreamData(streams[s], buf, int(n * 2 * sizeof(float)));
                if (report && report->on_sound) report->on_sound(s, api->audio_rate(core, s), buf, n);
            }
            // A stall (window drag): drop the backlog rather than lag behind.
            if (streams[s] && SDL_GetAudioStreamQueued(streams[s]) > int(api->audio_rate(core, s) * 8 * 0.25))
                SDL_ClearAudioStream(streams[s]);
        }
    }
    // A state was loaded: what was queued is from before it.
    void clear() {
        for (SDL_AudioStream *s : streams)
            if (s) SDL_ClearAudioStream(s);
    }
    void close() {
        for (SDL_AudioStream *&s : streams)
            if (s) { SDL_DestroyAudioStream(s); s = nullptr; }
        if (dev) SDL_CloseAudioDevice(dev);
        dev = 0;
    }
};

// A Lindbergh or Chihiro game packed as one file (<romset>.tar, as the
// Apple app keeps it in iCloud Drive and on the ROM server): a plain tar of
// the folder that holds disk0 (or the Chihiro game's XBE), the set's name as
// its top folder optional.
// Unpacked into `dst` by Cores-Common's tar unpacker (afcommon/tar.h), which
// refuses anything that would land outside the game folder. Returns an
// error, or "".
std::string unpack_tar(const fs::path &tar, const std::string &set, const std::string &required,
                       const fs::path &dst, const std::function<void(double)> &progress) {
    return afc::unpack_tar_file(tar, dst, afc::TarOptions{set_names(set), required}, progress);
}

// The folder a Lindbergh or Chihiro game plays from: its own, or its .tar unpacked
// into <data>/games/<romset> (again when the .tar changes). Shows
// "Unpacking…" meanwhile. Returns an empty path after telling the user why.
fs::path game_folder(SDL_Renderer *r, const Game &g, const Options &o) {
    if (g.rom.extension() != ".tar") return g.rom;
    std::error_code ec;
    const fs::path dst = o.data / "games" / g.romset, stamp = o.data / "games" / (g.romset + ".from");
    const std::string from = g.rom.string() + " " + std::to_string(fs::file_size(g.rom, ec)) + " " +
                             std::to_string(static_cast<long long>(fs::last_write_time(g.rom, ec).time_since_epoch().count())); // libc++ counts in __int128
    std::string had;
    if (std::FILE *f = std::fopen(stamp.c_str(), "r")) {
        char line[4096] = "";
        if (std::fgets(line, sizeof line, f)) had = line;
        std::fclose(f);
    }
    const std::string required = g.xbe.empty() ? "disk0" : g.xbe; // what the game folder holds
    if (had == from && fs::exists(dst / required, ec)) return dst;
    fs::create_directories(dst.parent_path(), ec);
    const auto show = [&](double f) {
        SDL_PumpEvents();
        SDL_SetRenderDrawColor(r, BACKGROUND.r, BACKGROUND.g, BACKGROUND.b, 255);
        SDL_RenderClear(r);
        SDL_SetRenderScale(r, 2, 2);
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        const std::string text = "Unpacking " + g.title + "... " + std::to_string(int(f * 100)) + "%";
        SDL_RenderDebugText(r, 24, 20, text.c_str());
        SDL_SetRenderScale(r, 1, 1);
        SDL_RenderPresent(r);
    };
    show(0);
    const std::string err = unpack_tar(g.rom, g.romset, required, dst, show);
    if (!err.empty()) {
        tell(o, err);
        return {};
    }
    if (std::FILE *f = std::fopen(stamp.c_str(), "w")) {
        std::fputs(from.c_str(), f);
        std::fclose(f);
    }
    return dst;
}

// A Chihiro game's EEPROMs come from MAME's BIOS set, chihiro.zip, anywhere
// in the ROM folder (linked into o.links like the games). The core finds it
// beside a game folder itself; a game unpacked from its .tar plays from
// <data>/games, so the core is told where it is ($AF_DEBUG_CH_BIOS, read at
// start). One the user set (or the old CH_BIOS, until 1.4.0) is left alone.
void chihiro_bios(const Options &o) {
    static const bool user_set = std::getenv("AF_DEBUG_CH_BIOS") != nullptr || std::getenv("CH_BIOS") != nullptr;
    if (user_set) return;
    std::error_code ec;
    const fs::path zip = o.links / "chihiro.zip";
    if (fs::exists(zip, ec)) setenv("AF_DEBUG_CH_BIOS", fs::canonical(zip, ec).c_str(), 1);
    else unsetenv("AF_DEBUG_CH_BIOS");
}

// SDL's renderer draws with OpenGL (and so does a software renderer's window
// surface), and a core with a GL context of its own (Model 3's EGL one) can't
// make that current while SDL's is: GLVND allows a thread one current context,
// and the core's GL then went into SDL's, so the window stayed blank. SDL's is
// let go around each call into a core and taken back after.
struct RendererGL {
    SDL_Window *win;
    SDL_GLContext ctx;
    explicit RendererGL(SDL_Renderer *r) : win(SDL_GetRenderWindow(r)), ctx(SDL_GL_GetCurrentContext()) {}
    void release() const { if (ctx) SDL_GL_MakeCurrent(nullptr, nullptr); }
    void take() const { if (ctx) SDL_GL_MakeCurrent(win, ctx); }
};

#ifdef __APPLE__
// The Mac build: a core that gives its screen as a GPU buffer (afcore 5.1
// screen_surface, an IOSurface-backed BGRA CVPixelBuffer: Model 3 on
// Metal; a Mac Lindbergh core gives pixels) is shown from that buffer, as the Apple app does,
// with no copy to the CPU and back. SDL's Metal renderer wraps each buffer
// as a texture; a core keeps a small ring of them, so each is wrapped once.
struct GpuScreens {
    SDL_Renderer *r;
    std::map<void *, SDL_Texture *> wrapped;
    int w = 0, h = 0;

    using SurfaceFn = void *(*)(afcore *, int *, int *);
    // The core's screen_surface, or null when it has none (a core before 5.1).
    static SurfaceFn get_fn(const afcore_api *api) {
        const size_t at = offsetof(afcore_api, screen_surface);
        return api->size >= at + sizeof(void *) ? api->screen_surface : nullptr;
    }
    SDL_Texture *texture(void *buffer, int bw, int bh) {
        if (bw != w || bh != h) { // the core made new buffers
            clear();
            w = bw;
            h = bh;
        }
        if (const auto it = wrapped.find(buffer); it != wrapped.end()) return it->second;
        const SDL_PropertiesID props = SDL_CreateProperties();
        SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER, SDL_PIXELFORMAT_ARGB8888);
        SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER, SDL_TEXTUREACCESS_STATIC);
        SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, bw);
        SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, bh);
        SDL_SetPointerProperty(props, SDL_PROP_TEXTURE_CREATE_METAL_PIXELBUFFER_POINTER, buffer);
        SDL_Texture *t = SDL_CreateTextureWithProperties(r, props);
        SDL_DestroyProperties(props);
        if (!t) {
            std::fprintf(stderr, "insert-coin: the core's screen buffer: %s (showing its pixels instead)\n", SDL_GetError());
            return nullptr;
        }
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_NONE); // opaque, whatever the alpha byte holds
        wrapped[buffer] = t;
        return t;
    }
    void clear() {
        for (auto &[buffer, t] : wrapped) SDL_DestroyTexture(t);
        wrapped.clear();
    }
};
#endif

} // namespace

bool play(SDL_Renderer *r, const Game &g, const Options &o, Controls &ctl, bool resume) {
    if (g.pc) { // a PC-based game: its runner, in a process of its own
        play_pc_game(r, g, o);
        return true;
    }
    char err[512] = "";
    const fs::path rom = g.folder ? game_folder(r, g, o) : g.rom;
    if (rom.empty()) return true;
    const fs::path saves = o.data / "saves" / g.romset;
    std::error_code ec;
    fs::create_directories(saves, ec);
    const RendererGL gl(r);
    gl.release();
    const std::string region = region_of(o, g);
    if (!region.empty()) g.api->set_option("region", region.c_str());
    if (!g.xbe.empty()) chihiro_bios(o);
    afcore *core = g.api->create(rom.c_str(), saves.c_str(), err, sizeof err);
    gl.take();
    if (!core) {
        tell(o, err);
        return true;
    }
    if (o.report) o.report->started = true;
    mark_played(o.data, g.romset);
    if (o.link_port > 0 && g.api->link_start(core, o.link_port, o.link_next.c_str(), o.link_sync, err, sizeof err) != 0)
        std::fprintf(stderr, "insert-coin: %s\n", err);
    // Save states (states.h): not while linked (the other cabinets can't go
    // back with this one), nor for Game Cabinet (its Game Director's games are linked).
    const bool states = has_states(g.api) && o.link_port <= 0 && !o.report;
    if (states && resume) {
        gl.release();
        const std::string why = load_slot(o, g, core, 0);
        gl.take();
        if (!why.empty()) tell(o, "The game starts from the beginning: its Resume slot didn't load.\n" + why);
    } else if (states) {
        remove_slot(o, g, 0); // played from power-on, the saves move on past it
    }
    ctl.flight = g.flight;
    ctl.drive = g.controls == GameControls::Drive;
    ctl.twin = g.twin;
    ctl.analog_gun = g.analog_gun;
    ctl.gun = is_gun_game(g);
    ctl.gun_button = g.gun_button;
    ctl.gun_start = gun_start_game(g);
    ctl.crosshair = shows_crosshair(g) && crosshair_on(o, g.romset);
    ctl.aim_x = ctl.aim_y = ctl.aim2_x = ctl.aim2_y = 0.5f;
    ctl.second_joined = false;
    ctl.fishing = g.controls == GameControls::Fishing;
    ctl.stick_y = g.stick_y;
    const fs::path buttons_file = o.data / "buttons" / g.romset;
    ctl.map = ButtonMap::load(buttons_file);
    ctl.operator_mode = false;
    Audio audio;
    audio.open(g.api, core);
    SDL_Texture *pixel_tex = nullptr; // the screen's pixels, uploaded
    SDL_Texture *tex = nullptr;       // what is shown: pixel_tex, or the core's GPU buffer (Mac)
    int tex_w = 0, tex_h = 0;
#ifdef __APPLE__
    GpuScreens gpu{r};
    auto *const screen_surface = GpuScreens::get_fn(g.api);
    bool gpu_failed = false; // SDL couldn't wrap a buffer: pixels from then on
#endif
    Look look = game_look(o, g.romset);
    Overlay overlay;
    g_picture = SDL_FRect{0, 0, 0, 0};
    const double hz = g.api->frame_hz;
    uint64_t last = SDL_GetPerformanceCounter();
    const double freq = double(SDL_GetPerformanceFrequency());
    double owed = 0;
    std::atomic<long> frames{0}, played{0}; // the game thread counts them too
    // The app's picture after half a minute, or two and a half on the slow
    // booting boards (unless the player has their own), the player's own
    // from the pause menu.
    const fs::path picture = picture_file(o, g.romset);
    const double picture_after = picture_seconds(kPlatforms[g.platform].key);
    const bool has_picture = fs::exists(picture) || !own_picture(o, g.romset).empty();
    const auto keep_screen = [&](bool own) {
        int w = 0, h = 0;
        if (const uint32_t *px = played > 0 ? g.api->screen(core, &w, &h) : nullptr) {
            std::error_code ec;
            if (own) fs::remove(o.data / "pictures" / "own" / (g.romset + ".png"), ec); // this one wins over one put there
            save_picture(own ? o.data / "pictures" / "own" / (g.romset + ".bmp") : picture, px, w, h);
        }
    };
    bool running = true, quit = false, failed = false;
    bool paused = false; // the pause menu is open
    int item = 0;
    const std::vector<PauseItem> items = pause_items(states);
    int slots = 0;        // Save state (1) or Load state (2) is open: its slots
    int slot_row = 0;     // the slot chosen there
    std::vector<std::string> slot_lines; // each slot's date and play time
    SDL_Texture *slot_picture = nullptr; // the chosen slot's screen
    std::string note;     // under the pause menu: what Save state or Load state did
    const auto show_slots = [&](int mode) {
        slots = mode;
        slot_row = 0;
        slot_lines.clear();
        for (int s = 1; s <= kStateSlots; ++s) slot_lines.push_back(read_slot(o, g, s).describe());
    };
    const auto pick_picture = [&] {
        if (slot_picture) SDL_DestroyTexture(slot_picture);
        slot_picture = nullptr;
        const StateSlot s = read_slot(o, g, slot_row + 1);
        if (s.picture.empty()) return;
        if (SDL_Surface *bmp = SDL_LoadBMP(s.picture.c_str())) {
            slot_picture = SDL_CreateTextureFromSurface(r, bmp);
            SDL_DestroySurface(bmp);
        }
    };
    int button_row = -1;   // the controller buttons are open (draw_buttons): the row picked
    bool waiting = false;  // for the button to put on that row
    WheelScreen wheel_screen; // Wheel and pedals (wheel.h), open from the pause menu
    int last_link = -1;
    // The window title's speed, once a second: loop presents and game
    // frames a second, the game's as a share of the board's rate.
    SDL_Window *win = SDL_GetRenderWindow(r);
    uint64_t stats_start = SDL_GetPerformanceCounter();
    long stats_presents = 0, stats_frames = 0;
    const uint64_t run_start = stats_start; // --speed
    double slowest = 0;                     // --speed: the slowest second's game fps
    std::string corner; // the same in a corner in full screen, where there is no title bar (as on iPad)
    long counted = 0; // played, as far as count_speed has seen it
    // The game on its own thread (Model 1/2, which have no GL of their own):
    // it runs the frames at the board's rate and leaves each finished screen
    // for this thread to show, so a slow present never holds up the next
    // frame (a Raspberry Pi 4 near its limit ran two or three frames a
    // present instead). It is stopped while the pause menu is open, so the
    // menu's calls into the core never meet it. Input is sampled here and
    // handed over; the frames, sound, the 30-second picture and the link's
    // state are the thread's.
    // Chihiro games too: their core draws on a GL context of its own thread
    // and paces the frames by its own vertical blank, so run_frame waits for
    // the game's next frame. Here, that wait and the present's wait for the
    // display's refresh, two 60 Hz clocks out of step, together took two or
    // three refreshes a loop: the screen showed 30 or 20 fps while the game
    // ran at 60.
    const bool chihiro = std::string(kPlatforms[g.platform].key) == "chihiro";
    const bool threaded = o.game_thread && ((!g.folder && g.platform <= 1) || chihiro);
    std::thread worker;
    std::mutex shared_m;              // guards the rest of the shared_ things
    afcore_input shared_in = ctl.sample(r);
    std::vector<uint32_t> shared_px;  // the newest finished screen
    int shared_w = 0, shared_h = 0;
    uint64_t shared_seq = 0, shown_seq = 0;
    std::string worker_error;         // the core stopped: why
    std::atomic<bool> stop_worker{false}, worker_done{false};
    const auto worker_loop = [&] {
        uint64_t wlast = SDL_GetPerformanceCounter();
        double wowed = 0;
        while (!stop_worker) {
            const uint64_t now = SDL_GetPerformanceCounter();
            wowed += (now - wlast) / freq * hz;
            wlast = now;
            if (o.frames > 0) wowed = 1; // tests: as fast as it goes
            if (wowed > 4) wowed = 1;    // a stall: skip ahead rather than race
            if (wowed < 1) {
                SDL_DelayPrecise(uint64_t((1 - wowed) / hz * 1e9));
                continue;
            }
            wowed -= 1;
            afcore_input in;
            {
                std::lock_guard<std::mutex> lock(shared_m);
                in = shared_in;
            }
            g.api->run_frame(core, &in);
            if (const char *why = g.api->error(core)) {
                std::lock_guard<std::mutex> lock(shared_m);
                worker_error = why;
                worker_done = true;
                return;
            }
            audio.push(g.api, core, o.report);
            if (++played == long(hz * picture_after) && o.frames == 0 && !has_picture) keep_screen(false);
            int w = 0, h = 0;
            if (const uint32_t *px = g.api->screen(core, &w, &h)) {
                std::lock_guard<std::mutex> lock(shared_m);
                shared_px.assign(px, px + size_t(w) * size_t(h));
                shared_w = w, shared_h = h;
                ++shared_seq;
            }
            int id = 0, count = 0;
            const int link = o.link_port > 0 ? g.api->link_state(core, &id, &count) : AFCORE_LINK_OFF;
            if (o.report && o.link_port > 0) o.report->link = link, o.report->link_id = id, o.report->link_count = count;
            if (link != last_link && o.link_port > 0) {
                static const char *names[] = {"off (set it in the game's test mode)", "waiting", "up", "lost"};
                if (link == AFCORE_LINK_UP) std::printf("insert-coin: link up, cabinet %d of %d\n", id, count);
                else std::printf("insert-coin: link %s\n", names[link]);
                last_link = link;
            }
            if (o.frames > 0 && ++frames >= o.frames) {
                worker_done = true;
                return;
            }
        }
    };
    const auto end_worker = [&] {
        if (!worker.joinable()) return;
        stop_worker = true;
        worker.join();
        stop_worker = false;
    };
    const auto count_speed = [&](long ran_frames) {
        ++stats_presents;
        stats_frames += ran_frames;
        const double span = (SDL_GetPerformanceCounter() - stats_start) / freq;
        if (span < 1) return;
        const double screen = stats_presents / span, game = stats_frames / span;
        char title[256];
        if (paused) std::snprintf(title, sizeof title, "%s \u2014 %.0f fps \u00b7 paused", g.title.c_str(), screen);
        else std::snprintf(title, sizeof title, "%s \u2014 %.0f fps \u00b7 game %.1f fps (%.0f%%)", g.title.c_str(), screen, game, 100 * game / hz);
        SDL_SetWindowTitle(win, o.show_speed ? title : g.title.c_str());
        if (o.report) {
            o.report->fps = screen;
            o.report->game_fps = paused ? 0 : game;
            o.report->speed_pct = paused ? 0 : 100 * game / hz;
        }
        char text[128]; // the debug font is ASCII only
        if (paused) std::snprintf(text, sizeof text, "%.0f fps - paused", screen);
        else std::snprintf(text, sizeof text, "%.0f fps - game %.1f fps (%.0f%%)", screen, game, 100 * game / hz);
        corner = text;
        if (o.speed && !paused) {
            const double at = (SDL_GetPerformanceCounter() - run_start) / freq;
            std::printf("speed: %s %6.1f s: screen %.0f fps, game %.1f fps (%.0f%%)\n", g.romset.c_str(), at, screen, game, 100 * game / hz);
            std::fflush(stdout);
            if (slowest == 0 || game < slowest) slowest = game;
        }
        stats_start = SDL_GetPerformanceCounter();
        stats_presents = stats_frames = 0;
    };
    while (running) {
        if (o.report) {
            if (o.report->stop) { running = false; break; }
            o.report->paused = paused;
        }
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) { running = false; quit = true; }
            if (e.type == SDL_EVENT_GAMEPAD_ADDED) ctl.open_pad();
            if (e.type == SDL_EVENT_JOYSTICK_ADDED) ctl.wheel.open(o.data);
            if (e.type == SDL_EVENT_JOYSTICK_REMOVED) ctl.wheel.removed(e.jdevice.which);
            if (SDL_Event pe; wheel_as_pad(e, ctl.wheel, pe)) SDL_PushEvent(&pe); // its d-pad and buttons work the menus
            if (fullscreen_key(e, win, o.data)) continue;
            const bool key = e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat, padb = e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
            const auto is = [&](SDL_Scancode s, SDL_GamepadButton b) {
                return (key && e.key.scancode == s) || (padb && e.gbutton.button == b);
            };
            if (paused && button_row >= 0) { // the controller buttons
                buttons_event(e, ctl.map, buttons_file, button_row, waiting);
                continue;
            }
            if (paused && wheel_screen.open()) { // the wheel and pedals
                wheel_event(e, ctl.wheel, wheel_screen);
                continue;
            }
            if (paused && slots) { // Save state or Load state's slots
                if (is(SDL_SCANCODE_UP, SDL_GAMEPAD_BUTTON_DPAD_UP)) slot_row = std::max(0, slot_row - 1), pick_picture();
                if (is(SDL_SCANCODE_DOWN, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) slot_row = std::min(kStateSlots - 1, slot_row + 1), pick_picture();
                if (is(SDL_SCANCODE_ESCAPE, SDL_GAMEPAD_BUTTON_EAST) || (key && e.key.scancode == SDL_SCANCODE_BACKSPACE)) {
                    slots = 0;
                } else if (is(SDL_SCANCODE_RETURN, SDL_GAMEPAD_BUTTON_SOUTH)) {
                    const int slot = slot_row + 1;
                    gl.release();
                    std::string why;
                    if (slots == 1) {
                        int w = 0, h = 0;
                        const uint32_t *px = played > 0 ? g.api->screen(core, &w, &h) : nullptr;
                        why = save_slot(o, g, core, slot, px, w, h);
                    } else {
                        why = load_slot(o, g, core, slot);
                    }
                    gl.take();
                    note = !why.empty() ? why : slots == 1 ? "Saved in slot " + std::to_string(slot) : "";
                    if (why.empty() && slots == 2) { // back in the game, at that frame
                        paused = false;
                        audio.clear();
                        if (audio.dev) SDL_ResumeAudioDevice(audio.dev);
                    }
                    slots = 0;
                }
                continue;
            }
            const bool chord = padb && (e.gbutton.button == SDL_GAMEPAD_BUTTON_START || e.gbutton.button == SDL_GAMEPAD_BUTTON_BACK) && ctl.chord_down();
            const bool wheel_pause = e.type == SDL_EVENT_JOYSTICK_BUTTON_DOWN && ctl.wheel.joy && e.jbutton.which == ctl.wheel.id &&
                                     ctl.wheel.s.button[kWPause] >= 0 && e.jbutton.button == ctl.wheel.s.button[kWPause];
            if (chord || wheel_pause || (key && e.key.scancode == SDL_SCANCODE_ESCAPE)) {
                paused = !paused;
                item = 0;
                note.clear();
                button_row = -1;
                wheel_screen.row = -1;
                if (paused) end_worker(), g.api->save(core);
                if (audio.dev) { if (paused) SDL_PauseAudioDevice(audio.dev); else SDL_ResumeAudioDevice(audio.dev); }
            } else if (paused) {
                if (is(SDL_SCANCODE_UP, SDL_GAMEPAD_BUTTON_DPAD_UP)) item = std::max(0, item - 1);
                if (is(SDL_SCANCODE_DOWN, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) item = std::min(int(items.size()) - 1, item + 1);
                const bool back = is(SDL_SCANCODE_BACKSPACE, SDL_GAMEPAD_BUTTON_EAST);
                const bool pick = is(SDL_SCANCODE_RETURN, SDL_GAMEPAD_BUTTON_SOUTH);
                const PauseItem it = items[size_t(item)];
                if (pick || back) note.clear();
                if (pick && it == PauseItem::buttons) {
                    button_row = 0;
                    waiting = false;
                } else if (pick && it == PauseItem::wheel) {
                    wheel_screen.start();
                } else if (pick && (it == PauseItem::save_state || it == PauseItem::load_state)) {
                    show_slots(it == PauseItem::save_state ? 1 : 2);
                    pick_picture();
                } else if (pick && (it == PauseItem::shape || it == PauseItem::effect)) { // the menu stays open to see the change
                    if (it == PauseItem::shape) look.shape = Shape((int(look.shape) + 1) % 3);
                    else look.effect = Effect((int(look.effect) + 1) % 3);
                    save_look(o, g.romset, look);
                } else if (pick && it == PauseItem::fullscreen) {
                    set_fullscreen(win, !g_fullscreen, o.data);
                } else if (back || pick) {
                    if (!back && it == PauseItem::operator_mode) {
                        ctl.operator_mode = !ctl.operator_mode;
                        ctl.operator_armed = false;
                    }
                    if (!back && it == PauseItem::picture) keep_screen(true);
                    if (!back && it == PauseItem::quit) running = false;
                    paused = false;
                    if (audio.dev) SDL_ResumeAudioDevice(audio.dev);
                }
            }
        }
        if (paused) {
            last = SDL_GetPerformanceCounter(); // no catching up on the time spent in the menu
            SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
            SDL_RenderClear(r);
            if (tex) draw_screen(r, tex, tex_w, tex_h, look, overlay, g.folder);
            if (button_row >= 0) draw_buttons(r, g, ctl.map, button_row, waiting);
            else if (wheel_screen.open()) {
                wheel_tick(ctl.wheel, wheel_screen);
                draw_wheel(r, ctl.wheel, wheel_screen);
            }
            else if (slots) draw_slots(r, g, slots == 1, slot_lines, slot_row, slot_picture);
            else draw_pause(r, g, items, item, ctl.operator_mode, look, note);
            SDL_RenderPresent(r);
            count_speed(0);
            continue;
        }
        if (threaded) {
            if (!worker.joinable() && !worker_done) worker = std::thread(worker_loop);
            afcore_input in = ctl.sample(r);
            if (o.report) o.report->take(in);
            bool fresh = false;
            {
                std::lock_guard<std::mutex> lock(shared_m);
                shared_in = in;
                if (shared_seq != shown_seq && shared_w > 0) {
                    if (!pixel_tex || tex_w != shared_w || tex_h != shared_h) {
                        if (pixel_tex) SDL_DestroyTexture(pixel_tex);
                        pixel_tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, shared_w, shared_h);
                        tex_w = shared_w;
                        tex_h = shared_h;
                    }
                    SDL_UpdateTexture(pixel_tex, nullptr, shared_px.data(), shared_w * 4);
                    if (o.report && o.report->on_screen) o.report->on_screen(shared_px.data(), shared_w, shared_h);
                    tex = pixel_tex;
                    shown_seq = shared_seq;
                    fresh = true;
                }
            }
            if (worker_done) {
                end_worker();
                running = false;
                if (!worker_error.empty()) {
                    const std::string text = g.title + " stopped: its core can't run this yet.\n" + worker_error;
                    tell(o, text);
                    failed = true;
                }
                if (o.hash && !shared_px.empty())
                    std::printf("%s: %ld frames, last screen hash %016" PRIx64 "\n", g.romset.c_str(), frames.load(),
                                fnv(shared_px.data(), shared_px.size()));
            } else if (!fresh) {
                SDL_DelayNS(500000); // nothing new to show yet
                continue;
            }
        }
        const uint64_t now = SDL_GetPerformanceCounter();
        owed += (now - last) / freq * hz;
        last = now;
        if (o.frames > 0) owed = 1; // tests: one frame per loop, as fast as it goes
        if (owed > 4) owed = 1;     // a stall: skip ahead rather than race
        if (threaded) owed = 0;     // the game thread runs them
        bool ran = false;
        if (owed >= 1) gl.release();
        while (owed >= 1) {
            owed -= 1;
            afcore_input in = ctl.sample(r);
            if (o.report) o.report->take(in);
            g.api->run_frame(core, &in);
            if (const char *why = g.api->error(core)) {
                const std::string text = g.title + " stopped: its core can't run this yet.\n" + why;
                tell(o, text);
                running = false;
                failed = true;
                break;
            }
            audio.push(g.api, core, o.report);
            ran = true;
            // A game without a picture takes one after half a minute (not in tests).
            if (++played == long(hz * picture_after) && o.frames == 0 && !has_picture) keep_screen(false);
            if (o.frames > 0 && ++frames >= o.frames) running = false;
        }
        gl.take();
        bool on_gpu = false;
#ifdef __APPLE__
        if (ran && screen_surface && !gpu_failed) {
            int w = 0, h = 0;
            if (void *buffer = screen_surface(core, &w, &h)) {
                if (SDL_Texture *t = gpu.texture(buffer, w, h)) {
                    tex = t;
                    tex_w = w;
                    tex_h = h;
                    on_gpu = true;
                } else {
                    gpu_failed = true;
                }
            }
        }
#endif
        if (ran && on_gpu && !running && o.hash) {
            int w = 0, h = 0;
            const uint32_t *px = g.api->screen(core, &w, &h);
            std::printf("%s: %ld frames, last screen hash %016" PRIx64 "\n", g.romset.c_str(), frames.load(), fnv(px, size_t(w) * h));
        }
        if (ran && !on_gpu) {
            int w = 0, h = 0;
            const uint32_t *px = g.api->screen(core, &w, &h);
            if (!pixel_tex || tex != pixel_tex || tex_w != w || tex_h != h) {
                if (pixel_tex) SDL_DestroyTexture(pixel_tex);
                // XRGB, so the screen is opaque whatever a core leaves in the alpha byte
                // (Model 1's polygons have 0 there, which blended them to black).
                pixel_tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
                tex_w = w;
                tex_h = h;
            }
            SDL_UpdateTexture(pixel_tex, nullptr, px, w * 4);
            tex = pixel_tex;
            if (o.report && o.report->on_screen) o.report->on_screen(px, w, h);
            if (!running && o.hash)
                std::printf("%s: %ld frames, last screen hash %016" PRIx64 "\n", g.romset.c_str(), frames.load(), fnv(px, size_t(w) * h));
        }
        int id = 0, count = 0;
        const int link = o.link_port > 0 && !threaded ? g.api->link_state(core, &id, &count) : AFCORE_LINK_OFF;
        if (o.report && o.link_port > 0 && !threaded) o.report->link = link, o.report->link_id = id, o.report->link_count = count;
        if (link != last_link && o.link_port > 0 && !threaded) {
            static const char *names[] = {"off (set it in the game's test mode)", "waiting", "up", "lost"};
            if (link == AFCORE_LINK_UP) std::printf("insert-coin: link up, cabinet %d of %d\n", id, count);
            else std::printf("insert-coin: link %s\n", names[link]);
            last_link = link;
        }
        SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
        SDL_RenderClear(r);
        if (tex) draw_screen(r, tex, tex_w, tex_h, look, overlay, g.folder);
        if (tex && ctl.gun) draw_crosshairs(r, ctl);
        if (ctl.operator_mode) {
            SDL_SetRenderDrawColor(r, 255, 170, 0, 255);
            SDL_RenderDebugText(r, 8, 8, "OPERATOR MODE: gamepad A = Test, B = Service (or F2, F3). Pause to leave.");
        }
        if (o.show_speed && !corner.empty() && (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN)) {
            int ow = 0, oh = 0;
            SDL_GetCurrentRenderOutputSize(r, &ow, &oh);
            const float tw = 8.f * float(corner.size());
            const SDL_FRect box{float(ow) - tw - 20, 8, tw + 12, 20};
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, 0, 0, 0, 128);
            SDL_RenderFillRect(r, &box);
            SDL_SetRenderDrawColor(r, 255, 255, 255, 204);
            SDL_RenderDebugText(r, box.x + 6, box.y + 6, corner.c_str());
        }
        SDL_RenderPresent(r);
        const long now_played = played;
        if (o.report) o.report->frames = now_played;
        count_speed(now_played - counted);
        counted = now_played;
    }
    end_worker();
    // Resume where I left off: the game as it was left, for its page's Resume.
    if (states && o.resume && !failed && played > 0) {
        int w = 0, h = 0;
        gl.release();
        const uint32_t *px = g.api->screen(core, &w, &h);
        const std::string why = save_slot(o, g, core, 0, px, w, h);
        gl.take();
        if (!why.empty()) std::fprintf(stderr, "insert-coin: the Resume slot wasn't saved: %s\n", why.c_str());
    }
    if (slot_picture) SDL_DestroyTexture(slot_picture);
    SDL_SetWindowTitle(win, "Arcade Foundry: Insert Coin");
    if (o.speed && played > 0) {
        const double span = (SDL_GetPerformanceCounter() - run_start) / freq;
        std::printf("speed: %s total: %ld frames in %.1f s, game %.1f fps (%.0f%% of %.2f), slowest second %.1f fps\n",
                    g.romset.c_str(), played.load(), span, played / span, 100 * played / span / hz, hz, slowest);
        std::fflush(stdout);
    }
    if (pixel_tex) SDL_DestroyTexture(pixel_tex);
#ifdef __APPLE__
    gpu.clear();
#endif
    overlay.drop();
    audio.close();
    gl.release();
    g.api->destroy(core); // saves the settings
    gl.take();
    return !quit;
}

} // namespace insert_coin
