#include "art.h"
#include "app.h"
#include "render.h"

#ifdef AF_HAVE_CURL
#include <curl/curl.h>
#endif

#include <cstdio>

namespace insert_coin {

const char *art_key(ArtKind k) {
    static const char *const keys[] = {"title", "ingame", "flyer", "marquee", "cabinet"};
    return keys[int(k)];
}

const char *art_name(ArtKind k) {
    static const char *const names[] = {"Title screen", "In game", "Flyer", "Marquee", "Cabinet"};
    return names[int(k)];
}

std::string libretro_name(const std::string &description) {
    static const std::string swapped = "&*/:`<>?\\|\"";
    std::string out;
    for (const char c : description) {
        const unsigned char u = (unsigned char)(swapped.find(c) != std::string::npos ? '_' : c);
        if (std::isalnum(u) || std::strchr("-._~()!',_", u)) out += char(u);
        else {
            char b[4];
            std::snprintf(b, sizeof b, "%%%02X", u);
            out += b;
        }
    }
    return out;
}

std::string json_string(const std::string &json, const std::string &key) {
    const size_t k = json.find("\"" + key + "\"");
    if (k == std::string::npos) return "";
    size_t i = k + key.size() + 2;
    while (i < json.size() && (json[i] == ' ' || json[i] == ':' || json[i] == '\t' || json[i] == '\n' || json[i] == '\r')) ++i;
    if (i >= json.size() || json[i] != '"') return ""; // null, a number
    std::string out;
    for (++i; i < json.size() && json[i] != '"'; ++i) {
        if (json[i] != '\\' || i + 1 >= json.size()) {
            out += json[i];
            continue;
        }
        const char e = json[++i];
        if (e == 'n') out += '\n';
        else if (e == 't') out += '\t';
        else if (e == 'u' && i + 4 < json.size()) { // as UTF-8 (pairs beyond the first plane are rare in titles: kept as two)
            const unsigned cp = unsigned(std::strtoul(json.substr(i + 1, 4).c_str(), nullptr, 16));
            i += 4;
            if (cp < 0x80) out += char(cp);
            else if (cp < 0x800) out += char(0xc0 | (cp >> 6)), out += char(0x80 | (cp & 0x3f));
            else out += char(0xe0 | (cp >> 12)), out += char(0x80 | ((cp >> 6) & 0x3f)), out += char(0x80 | (cp & 0x3f));
        } else out += e; // \" \\ \/
    }
    return out;
}

void Art::start(const fs::path &data) {
    dir_ = data / "art";
    const char *test = std::getenv("AF_ART_URL");
    test_ = test && *test ? test : "";
    adb_ = test_.empty() ? "https://adb.arcadeitalia.net" : test_;
    libretro_ = test_.empty() ? "https://thumbnails.libretro.com/MAME" : test_ + "/libretro";
    // MAME's screens downloaded for those boards before art_wanted: removed, with their misses.
    for (const GameListEntry &g : kGameList)
        for (const ArtKind k : {ArtKind::title, ArtKind::ingame})
            if (!art_wanted(g.set, k)) {
                std::error_code ec;
                fs::remove(file(g.set, k), ec);
                fs::remove(dir_ / g.set / (std::string(art_key(k)) + ".miss"), ec);
            }
#ifdef AF_HAVE_CURL
    started_ = true;
    worker_ = std::thread([this] { run(); });
#endif
}

Art::~Art() {
    {
        std::lock_guard<std::mutex> lock(m_);
        stop_ = true;
    }
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

bool Art::built() {
#ifdef AF_HAVE_CURL
    return true;
#else
    return false;
#endif
}

// Sets on a board MAME can't run yet (Lindbergh, Chihiro): their title and
// in-game pictures in both sources are MAME's own screen (its "Screenless
// System" card, or noise), so those two kinds aren't downloaded or shown;
// the list falls back to the app's screen from the game, then the flyer.
bool art_wanted(const std::string &set, ArtKind k) {
    if (k != ArtKind::title && k != ArtKind::ingame) return true;
    for (const GameListEntry &g : kGameList)
        if (set == g.set) {
            const std::string board = kPlatforms[g.platform].key;
            return board != "lindbergh" && board != "chihiro";
        }
    return true;
}

fs::path Art::file(const std::string &set, ArtKind k) const { return dir_ / set / (std::string(art_key(k)) + ".png"); }

bool Art::has(const std::string &set, ArtKind k) const {
    std::error_code ec;
    return !dir_.empty() && art_wanted(set, k) && fs::exists(file(set, k), ec);
}

bool Art::missed(const std::string &set, ArtKind k) const {
    std::error_code ec;
    return fs::exists(dir_ / set / (std::string(art_key(k)) + ".miss"), ec);
}

void Art::mark(const std::string &set, ArtKind k) const {
    std::error_code ec;
    fs::create_directories(dir_ / set, ec);
    if (std::FILE *f = std::fopen((dir_ / set / (std::string(art_key(k)) + ".miss")).c_str(), "w")) std::fclose(f);
}

// The title screen, then the in-game shot, then the flyer.
void Art::want_card(const std::string &set) {
    for (const ArtKind k : {ArtKind::title, ArtKind::ingame, ArtKind::flyer}) {
        if (!art_wanted(set, k)) continue;
        if (has(set, k)) return;
        if (!missed(set, k)) return want(set, k);
    }
}

void Art::want(const std::string &set, std::initializer_list<ArtKind> kinds) {
    if (!started_ || !enabled) return;
    bool added = false;
    {
        std::lock_guard<std::mutex> lock(m_);
        if (offline_.count(set)) return;
        for (const ArtKind k : kinds) {
            if (!art_wanted(set, k) || has(set, k) || missed(set, k)) continue;
            bool queued = busy_ && current_.set == set && current_.kind == k;
            for (const Job &j : queue_) queued |= j.set == set && j.kind == k;
            if (!queued) queue_.push_back({set, k}), added = true;
        }
    }
    if (added) cv_.notify_all();
}

void Art::hold(bool on) {
    {
        std::lock_guard<std::mutex> lock(m_);
        held_ = on;
    }
    cv_.notify_all();
}

void Art::remove_all() {
    std::lock_guard<std::mutex> lock(m_); // not while a picture is being written
    queue_.clear();
    answers_.clear();
    offline_.clear();
    std::error_code ec;
    if (!dir_.empty()) fs::remove_all(dir_, ec);
    ++arrived_;
}

#ifdef AF_HAVE_CURL
// libretro-thumbnails' folder for the same kind of picture, or null.
static const char *libretro_folder(ArtKind k) {
    switch (k) {
    case ArtKind::title: return "Named_Titles";
    case ArtKind::ingame: return "Named_Snaps";
    case ArtKind::flyer: return "Named_Boxarts"; // MAME's "box art" there is the flyer
    default: return nullptr;
    }
}

namespace {
size_t keep(char *p, size_t size, size_t n, void *to) {
    static_cast<std::string *>(to)->append(p, size * n);
    return size * n;
}

// GET url into body: the HTTP status, or 0 when there was no answer.
long get(void *curl, const std::string &url, std::string &body) {
    CURL *c = static_cast<CURL *>(curl);
    body.clear();
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, keep);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &body);
    if (curl_easy_perform(c) != CURLE_OK) return 0;
    long status = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
    return status;
}
} // namespace

void Art::run() {
    CURL *curl = curl_easy_init(); // one handle: one connection, kept open between pictures
    if (!curl) return;
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L); // the Arcade Database can take minutes over a picture
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L); // but not stalled for half a minute
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "ArcadeFoundryInsertCoin/" INSERT_COIN_VERSION);
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(m_);
            cv_.wait(lock, [&] { return stop_ || (!held_ && enabled && !queue_.empty()); });
            if (stop_) break;
            job = current_ = queue_.front();
            queue_.pop_front();
            busy_ = true;
        }
        const Outcome got = fetch(curl, job);
        std::lock_guard<std::mutex> lock(m_);
        busy_ = false;
        if (got == Outcome::offline) {
            offline_[job.set] = true;
            std::erase_if(queue_, [&](const Job &j) { return j.set == job.set; });
            continue;
        }
        if (got == Outcome::missing) {
            mark(job.set, job.kind);
            if (job.kind == ArtKind::title && !has(job.set, ArtKind::ingame) && !missed(job.set, ArtKind::ingame))
                queue_.push_front({job.set, ArtKind::ingame}); // the list falls back to the in-game shot
            else if (job.kind == ArtKind::ingame && !has(job.set, ArtKind::flyer) && !missed(job.set, ArtKind::flyer))
                queue_.push_front({job.set, ArtKind::flyer}); // then the flyer
        }
        ++arrived_;
    }
    curl_easy_cleanup(curl);
}

bool Art::query(void *curl, const std::string &set, Answer &out) {
    std::string body;
    if (get(curl, adb_ + "/service_scraper.php?ajax=query_mame&game_name=" + set, body) != 200) return false;
    const size_t result = body.find("\"result\"");
    if (result == std::string::npos) return false;
    const std::string first = body.substr(result); // a set it doesn't know has an empty list: nothing found below
    if (json_string(first, "game_name").empty()) return true;
    out.description = json_string(first, "title");
    for (const ArtKind k : {ArtKind::title, ArtKind::ingame, ArtKind::flyer, ArtKind::marquee, ArtKind::cabinet}) {
        std::string url = json_string(first, std::string("url_image_") + art_key(k));
        if (url.empty()) continue;
        for (size_t a; (a = url.find("&amp;")) != std::string::npos;) url.replace(a, 5, "&");
        if (const size_t q = url.find('?'); !test_.empty() && q != std::string::npos) url = test_ + "/" + url.substr(q);
        out.urls[k] = url;
    }
    return true;
}

Art::Outcome Art::fetch(void *curl, const Job &job) {
    Answer a;
    bool known = false;
    {
        std::lock_guard<std::mutex> lock(m_); // remove_all clears them
        if (const auto it = answers_.find(job.set); it != answers_.end()) a = it->second, known = true;
    }
    if (!known) {
        if (!query(curl, job.set, a)) return Outcome::offline;
        std::lock_guard<std::mutex> lock(m_);
        answers_[job.set] = a;
    }
    const fs::path to = file(job.set, job.kind);
    if (const auto u = a.urls.find(job.kind); u != a.urls.end()) {
        const Outcome got = download(curl, u->second, job.kind, to);
        if (got != Outcome::missing) return got;
    }
    const char *folder = libretro_folder(job.kind);
    if (!folder || a.description.empty()) return Outcome::missing;
    return download(curl, libretro_ + "/" + folder + "/" + libretro_name(a.description) + ".png", job.kind, to);
}

// One picture, kept as PNG at most 1600 pixels across (the title screen and
// in-game shot 1024); missing when the answer isn't a picture SDL reads
// (PNG or BMP) at least 64 pixels each way.
Art::Outcome Art::download(void *curl, const std::string &url, ArtKind kind, const fs::path &to) {
    std::string body;
    const long status = get(curl, url, body);
    if (status == 0 || status == 429 || status >= 500) return Outcome::offline;
    if (status != 200) return Outcome::missing;
    SDL_Surface *s = SDL_LoadSurface_IO(SDL_IOFromConstMem(body.data(), body.size()), true);
    if (!s) return Outcome::missing; // HTML (the Arcade Database's answer to an unknown kind), JPEG
    Outcome got = Outcome::missing;
    // The Arcade Database answers a missing picture with 200 and a stand-in:
    // its 140 x 105 "no image" picture, or a blank (all black) frame at the
    // game's resolution. So under 160 x 120, or one colour, is missing.
    bool blank = true;
    if (SDL_Surface *c = SDL_ConvertSurface(s, SDL_PIXELFORMAT_XRGB8888)) {
        const uint32_t first = *static_cast<const uint32_t *>(c->pixels) & 0xffffff;
        for (int y = 0; y < c->h && blank; y += std::max(1, c->h / 48))
            for (int x = 0; x < c->w && blank; x += std::max(1, c->w / 64))
                blank = (static_cast<const uint32_t *>(c->pixels)[size_t(y) * (c->pitch / 4) + x] & 0xffffff) == first;
        SDL_DestroySurface(c);
    }
    if (s->w >= 160 && s->h >= 120 && !blank) {
        const int longest = kind == ArtKind::title || kind == ArtKind::ingame ? 1024 : 1600;
        const double scale = std::min(1.0, double(longest) / std::max(s->w, s->h));
        SDL_Surface *out = scale < 1 ? SDL_ScaleSurface(s, std::max(1, int(s->w * scale)), std::max(1, int(s->h * scale)), SDL_SCALEMODE_LINEAR) : s;
        std::error_code ec;
        fs::create_directories(to.parent_path(), ec);
        const fs::path part = fs::path(to).concat(".part");
        if (out && SDL_SavePNG(out, part.c_str())) {
            fs::rename(part, to, ec);
            if (!ec) got = Outcome::saved;
        }
        if (out && out != s) SDL_DestroySurface(out);
    }
    SDL_DestroySurface(s);
    return got;
}
#else
void Art::run() {}
#endif

const Images::Entry *Images::get(SDL_Renderer *r, const fs::path &file) {
    if (file.empty()) return nullptr;
    std::error_code ec;
    const auto t = fs::last_write_time(file, ec);
    auto it = cache.find(file.string());
    if (ec) {
        if (it != cache.end()) {
            if (it->second.tex) SDL_DestroyTexture(it->second.tex);
            cache.erase(it);
        }
        return nullptr;
    }
    if (it != cache.end() && it->second.time == t) return it->second.tex ? &it->second : nullptr;
    Entry &e = cache[file.string()];
    if (e.tex) SDL_DestroyTexture(e.tex);
    e = Entry{t};
    if (SDL_Surface *s = SDL_LoadSurface(file.c_str())) {
        e.tex = SDL_CreateTextureFromSurface(r, s);
        e.w = s->w;
        e.h = s->h;
        SDL_DestroySurface(s);
    }
    return e.tex ? &e : nullptr;
}

void Images::drop() {
    for (auto &[_, e] : cache)
        if (e.tex) SDL_DestroyTexture(e.tex);
    cache.clear();
}

fs::path own_picture(const Options &o, const std::string &set) {
    std::error_code ec;
    for (const char *ext : {".png", ".bmp"})
        if (const fs::path p = o.data / "pictures" / "own" / (set + ext); fs::exists(p, ec)) return p;
    return {};
}

fs::path card_picture(const Options &o, const Art &art, const std::string &set) {
    if (fs::path own = own_picture(o, set); !own.empty()) return own;
    if (art.has(set, ArtKind::title)) return art.file(set, ArtKind::title);
    if (art.has(set, ArtKind::ingame)) return art.file(set, ArtKind::ingame);
    // Lindbergh and Chihiro (no MAME screens): the flyer before the app's screen.
    if (!art_wanted(set, ArtKind::title) && art.has(set, ArtKind::flyer)) return art.file(set, ArtKind::flyer);
    std::error_code ec;
    if (fs::path shot = picture_file(o, set); fs::exists(shot, ec)) return shot;
    if (art.has(set, ArtKind::flyer)) return art.file(set, ArtKind::flyer);
    return {};
}

} // namespace insert_coin
