// Game art (issue #352), as the Apple app's GameArt: downloaded on the
// player's machine, by MAME set name, from the Arcade Database
// (adb.arcadeitalia.net), and from libretro-thumbnails (by the game's MAME
// description) when the Arcade Database has none of that kind. The app
// ships none: the art is Sega's.
//
// - Lazy: the chosen game's title screen in the list, the rest when its
//   page opens. One download at a time on a thread of its own (the Arcade
//   Database asks for one connection per device), none while a game plays.
// - Kept for good in <data>/art/<set>/<kind>.png, never checked or updated
//   again. A kind neither source has is remembered (<kind>.miss); a failed
//   connection is tried again the next time the app starts.
// - Only the set name is sent.
// - Settings: Download game art (on by default) and Remove downloaded art,
//   which never touches the player's own pictures (<data>/pictures/own/).
// - Built only with libcurl (libcurl4-openssl-dev or libcurl-devel): without
//   it nothing downloads, and the list shows the screens from the games.
// AF_ART_URL in the environment points both sources at a test server:
// <AF_ART_URL>/service_scraper.php?..., <AF_ART_URL>/?mame=... and
// <AF_ART_URL>/libretro/<folder>/<name>.png.
#pragma once

#include "app.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <thread>

namespace insert_coin {

enum class ArtKind { title, ingame, flyer, marquee, cabinet };

const char *art_key(ArtKind k);  // "title" ... the file's name
const char *art_name(ArtKind k); // "Title screen" ...

// Under About this game on the page, in this order.
constexpr ArtKind kArtExtras[] = {ArtKind::flyer, ArtKind::marquee, ArtKind::cabinet, ArtKind::ingame};

// libretro-thumbnails' file name for a MAME description: &*/:`<>?\|" become
// _, then the rest is escaped for the address.
std::string libretro_name(const std::string &description);

// The value of "key" in a JSON text (the first one), unescaped; "" when
// there is none or it isn't a string.
std::string json_string(const std::string &json, const std::string &key);

// False for the title and in-game pictures of a set on a board MAME can't
// run yet (Lindbergh, Chihiro): MAME's own screen, not the game's.
bool art_wanted(const std::string &set, ArtKind k);

class Art {
public:
    // Downloads only after start; with `enabled` false nothing is queued.
    void start(const fs::path &data);
    ~Art();

    static bool built(); // with libcurl

    std::atomic<bool> enabled{true};

    fs::path file(const std::string &set, ArtKind k) const;
    bool has(const std::string &set, ArtKind k) const;
    bool missed(const std::string &set, ArtKind k) const;

    // The list's picture: the title screen, then the in-game shot, then the flyer.
    void want_card(const std::string &set);
    void want(const std::string &set, std::initializer_list<ArtKind> kinds);
    void want(const std::string &set, ArtKind k) { want(set, {k}); }

    // While a game plays, nothing downloads.
    void hold(bool on);

    // Every downloaded picture and miss goes (not the player's own pictures).
    void remove_all();

    // Bumped when a picture arrives or goes, so the screens draw again.
    unsigned arrived() const { return arrived_.load(); }

private:
    struct Job {
        std::string set;
        ArtKind kind;
    };
    struct Answer { // the Arcade Database's, for one set
        std::string description;
        std::map<ArtKind, std::string> urls;
    };
    enum class Outcome { saved, missing, offline };

    void run();
    Outcome fetch(void *curl, const Job &job);
    bool query(void *curl, const std::string &set, Answer &out);
    Outcome download(void *curl, const std::string &url, ArtKind kind, const fs::path &to);
    void mark(const std::string &set, ArtKind k) const;

    fs::path dir_;
    std::string adb_, libretro_, test_;
    std::thread worker_;
    std::mutex m_;
    std::condition_variable cv_;
    std::deque<Job> queue_;
    Job current_;                           // the one downloading now
    bool busy_ = false;
    std::map<std::string, Answer> answers_; // this run
    std::map<std::string, bool> offline_;   // sets that couldn't be reached this run
    bool held_ = false, stop_ = false, started_ = false;
    std::atomic<unsigned> arrived_{0};
};

// Images for the list and the page (BMP or PNG files), loaded again when
// their file changes. Main thread only.
struct Images {
    struct Entry {
        fs::file_time_type time{};
        SDL_Texture *tex = nullptr;
        int w = 0, h = 0;
    };
    std::map<std::string, Entry> cache;

    const Entry *get(SDL_Renderer *r, const fs::path &file);
    void drop();
    ~Images() { drop(); }
};

// The player's own picture for a game: <data>/pictures/own/<set>.png (one
// they put there) or .bmp ("Use this screen as the picture" in the pause
// menu); "" when there is none.
fs::path own_picture(const Options &o, const std::string &set);

// What a game's picture is: the player's own, else the downloaded title
// screen (or in-game shot), else the app's screen from the game; "" when
// there is none yet.
fs::path card_picture(const Options &o, const Art &art, const std::string &set);

} // namespace insert_coin
