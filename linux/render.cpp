#include "render.h"
#include "input.h"

namespace insert_coin {

namespace {

// A gun's crosshair: a ring with four ticks, in the colour given, with a
// dark outline so it shows on any picture.
void draw_crosshair(SDL_Renderer *r, float x, float y, float size, SDL_Color c) {
    const auto shape = [&](float ox, float oy) {
        const int n = 24;
        SDL_FPoint ring[n + 1];
        for (int i = 0; i <= n; i++) {
            const float a = float(i) * 6.2831853f / n;
            ring[i] = {x + ox + std::cos(a) * size * 0.5f, y + oy + std::sin(a) * size * 0.5f};
        }
        SDL_RenderLines(r, ring, n + 1);
        const float in = size * 0.2f, out = size * 0.75f;
        SDL_RenderLine(r, x + ox - out, y + oy, x + ox - in, y + oy);
        SDL_RenderLine(r, x + ox + in, y + oy, x + ox + out, y + oy);
        SDL_RenderLine(r, x + ox, y + oy - out, x + ox, y + oy - in);
        SDL_RenderLine(r, x + ox, y + oy + in, x + ox, y + oy + out);
    };
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    for (const float d : {-1.f, 1.f}) shape(d, 0), shape(0, d);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
    shape(0, 0);
}

} // namespace

SDL_Texture *Overlay::get(SDL_Renderer *r, int ow, int oh, int screen_lines, Effect e) {
    if (e == Effect::none || ow < 1 || oh < 1) return nullptr;
    if (tex && ow == w && oh == h && lines == screen_lines && e == effect) return tex;
    drop();
    w = ow, h = oh, lines = screen_lines, effect = e;
    tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
    if (!tex) return nullptr;
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_MOD);
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    std::vector<uint32_t> px(size_t(w) * h);
    const double per_line = double(h) / std::max(lines, 1);
    const double fade = std::clamp(per_line - 1.0, 0.0, 1.0); // none under two pixels a line
    const bool mask = e == Effect::crt && double(w) / (lines * 4.0 / 3.0) >= 2.5;
    for (int y = 0; y < h; ++y) {
        const double f = std::fmod((y + 0.5) / per_line, 1.0);
        const double scan = 1 - fade * (1 - (0.45 + 0.55 * std::sin(f * SDL_PI_D)));
        for (int x = 0; x < w; ++x) {
            double rgb[3] = {scan, scan, scan};
            if (mask)
                for (int c = 0; c < 3; ++c) rgb[c] *= c == x % 3 ? 1.0 : 0.78;
            if (e == Effect::crt) {
                const double ex = std::min(x + 0.5, w - x - 0.5), ey = std::min(y + 0.5, h - y - 0.5);
                const double edge = std::clamp(std::min(ex, ey) / 10.0, 0.0, 1.0);
                const double cx = 2.0 * x / w - 1, cy = 2.0 * y / h - 1;
                for (double &v : rgb) v *= edge * (1 - 0.12 * (cx * cx + cy * cy));
            }
            px[size_t(y) * w + x] = 0xff000000u | uint32_t(std::lround(rgb[0] * 255)) << 16 |
                                    uint32_t(std::lround(rgb[1] * 255)) << 8 | uint32_t(std::lround(rgb[2] * 255));
        }
    }
    SDL_UpdateTexture(tex, nullptr, px.data(), w * 4);
    return tex;
}

SDL_FRect g_picture{0, 0, 0, 0};

SDL_FRect screen_rect(int ow, int oh, int w, int h, const Look &look, bool square_pixels) {
    // the classic 2D boards' narrower screens (Out Run's 320 x 224): 4:3
    const double shape = square_pixels ? double(w) / h : w < 496 ? 4.0 / 3.0 : 4.0 / 3.0 * (w / 496.0);
    double dw = ow, dh = ow / shape;
    if (dh > oh) { dh = oh; dw = oh * shape; }
    if (look.shape == Shape::stretch) { dw = ow; dh = oh; }
    if (look.shape == Shape::integer) {
        const double n = std::floor(std::min(double(oh) / h, ow / (h * shape)));
        if (n >= 1) { dh = n * h; dw = dh * shape; }
    }
    dw *= look.zoom;
    dh *= look.zoom;
    const double cx = ow * (0.5 + look.shift_x), cy = oh * (0.5 + look.shift_y);
    return SDL_FRect{float(cx - dw / 2), float(cy - dh / 2), float(dw), float(dh)};
}

void draw_screen(SDL_Renderer *r, SDL_Texture *tex, int w, int h, const Look &look, Overlay &overlay, bool square_pixels) {
    int ow = 0, oh = 0;
    SDL_GetCurrentRenderOutputSize(r, &ow, &oh);
    const SDL_FRect dst = screen_rect(ow, oh, w, h, look, square_pixels);
    g_picture = dst;
    SDL_SetTextureScaleMode(tex, look.smooth ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
    SDL_RenderTexture(r, tex, nullptr, &dst);
    if (SDL_Texture *o = overlay.get(r, int(std::lround(dst.w)), int(std::lround(dst.h)), h, look.effect))
        SDL_RenderTexture(r, o, nullptr, &dst);
}

uint64_t fnv(const uint32_t *px, size_t n) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i)
        for (int b = 0; b < 4; ++b) {
            h ^= (px[i] >> (8 * b)) & 0xff;
            h *= 0x100000001b3ULL;
        }
    return h;
}

fs::path picture_file(const Options &o, const std::string &romset) { return o.data / "pictures" / (romset + ".bmp"); }

double picture_seconds(const std::string &platform_key) {
    return platform_key == "lindbergh" || platform_key == "chihiro" ? 150 : 30;
}

void save_picture(const fs::path &file, const uint32_t *px, int w, int h) {
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    // XRGB: the screen's alpha isn't coverage (Model 1's polygons come with 0), so it is ignored.
    SDL_Surface *screen = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_XRGB8888, const_cast<uint32_t *>(px), w * 4);
    SDL_Surface *pic = SDL_CreateSurface(h * 4 / 3, h, SDL_PIXELFORMAT_ARGB8888); // square pixels (the board's are not)
    if (screen && pic && SDL_BlitSurfaceScaled(screen, nullptr, pic, nullptr, SDL_SCALEMODE_LINEAR) && !SDL_SaveBMP(pic, file.c_str()))
        std::fprintf(stderr, "insert-coin: %s: %s\n", file.c_str(), SDL_GetError());
    SDL_DestroySurface(pic);
    SDL_DestroySurface(screen);
}

SDL_FPoint menu_frame(SDL_Renderer *r) {
    int ow = 0, oh = 0;
    SDL_GetCurrentRenderOutputSize(r, &ow, &oh);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 170);
    const SDL_FRect all{0, 0, float(ow), float(oh)};
    SDL_RenderFillRect(r, &all);
    SDL_SetRenderScale(r, 2, 2);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    return SDL_FPoint{ow / 4.f - 60, oh / 4.f - 30};
}

void draw_pause(SDL_Renderer *r, const Game &g, int item, bool operator_mode, const Look &look) {
    const std::string shape = std::string("Shape: ") + Look::name(look.shape), effect = std::string("Effect: ") + Look::name(look.effect);
    const char *const full = g_fullscreen ? "Full screen: on (F11)" : "Full screen: off (F11)";
    const char *const menu[kMenuItems] = {"Resume", operator_mode ? "Leave operator mode" : "Operator mode (Test, Service)",
                                          shape.c_str(), effect.c_str(), "Controller buttons", "Wheel and pedals", "Use this screen as the picture",
                                          full, "Quit to library"};
    float sx = 0, sy = 0;
    SDL_GetRenderScale(r, &sx, &sy);
    const SDL_FPoint at = menu_frame(r);
    const float x = at.x, y = at.y;
    SDL_RenderDebugText(r, x, y, "PAUSED");
    SDL_RenderDebugText(r, x, y + 12, g.title.c_str());
    for (int i = 0; i < kMenuItems; ++i) {
        const std::string line = std::string(i == item ? "> " : "  ") + menu[i];
        SDL_RenderDebugText(r, x, y + 34 + 14 * i, line.c_str());
    }
    SDL_SetRenderScale(r, sx, sy);
}

Look game_look(const Options &o, const std::string &romset) {
    Look look = o.look;
    if (std::FILE *f = std::fopen((o.data / "screen" / romset).c_str(), "r")) {
        char buf[256] = "";
        const size_t n = std::fread(buf, 1, sizeof buf - 1, f);
        std::fclose(f);
        look.parse(std::string(buf, n));
    }
    return look;
}

void save_look(const Options &o, const std::string &romset, const Look &look) {
    std::error_code ec;
    fs::create_directories(o.data / "screen", ec);
    if (std::FILE *f = std::fopen((o.data / "screen" / romset).c_str(), "w")) {
        std::fprintf(f, "%s\n", look.str().c_str());
        std::fclose(f);
    }
}

void draw_crosshairs(SDL_Renderer *r, const Controls &ctl) {
    if (!ctl.crosshair || !ctl.pad || g_picture.w <= 0) return;
    const SDL_FRect &p = g_picture;
    const float size = std::max(24.f, p.w / 22);
    if (ctl.pad2 && ctl.second_joined) draw_crosshair(r, p.x + ctl.aim2_x * p.w, p.y + ctl.aim2_y * p.h, size, {60, 140, 255, 255});
    draw_crosshair(r, p.x + ctl.aim_x * p.w, p.y + ctl.aim_y * p.h, size, {255, 40, 40, 255});
}

} // namespace insert_coin
