#include "states.h"
#include "render.h"

#include "afcommon/state.h"

#include <zlib.h>

#include <cstddef>
#include <ctime>

namespace insert_coin {

namespace {

std::string slot_name(int slot) { return slot == 0 ? "resume" : std::to_string(slot); }

fs::path slot_file(const Options &o, const Game &g, int slot, const char *ext) {
    return states_dir(o, g.romset) / (slot_name(slot) + ext);
}

bool read_file(const fs::path &file, std::vector<uint8_t> &out) {
    std::FILE *f = std::fopen(file.c_str(), "rb");
    if (!f) return false;
    out.clear();
    uint8_t buf[1 << 16];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.insert(out.end(), buf, buf + n);
    std::fclose(f);
    return true;
}

// Written beside, then moved over the old file, so a state is never half written.
bool write_file(const fs::path &file, const void *data, size_t size) {
    const fs::path tmp = file.string() + ".new";
    std::FILE *f = std::fopen(tmp.c_str(), "wb");
    if (!f) return false;
    const bool ok = std::fwrite(data, 1, size, f) == size;
    if (std::fclose(f) != 0 || !ok) return false;
    std::error_code ec;
    fs::rename(tmp, file, ec);
    return !ec;
}

// Raw deflate (no zlib header), as NSData's .zlib in the Apple app.
bool deflate_raw(const uint8_t *in, size_t n, std::vector<uint8_t> &out) {
    z_stream z{};
    if (deflateInit2(&z, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK) return false;
    out.resize(deflateBound(&z, uLong(n)));
    z.next_in = const_cast<uint8_t *>(in);
    z.avail_in = uInt(n);
    z.next_out = out.data();
    z.avail_out = uInt(out.size());
    const int rc = deflate(&z, Z_FINISH);
    out.resize(z.total_out);
    deflateEnd(&z);
    return rc == Z_STREAM_END;
}

// Into `out`, which is as big as the state may be (the core's state_size).
bool inflate_raw(const std::vector<uint8_t> &in, std::vector<uint8_t> &out) {
    // out's size is a first guess: a state's size depends on its frame (Model
    // 1 and 2 save their queues as they are), so the buffer grows as needed.
    z_stream z{};
    if (inflateInit2(&z, -15) != Z_OK) return false;
    if (out.size() < 4096) out.resize(4096);
    z.next_in = const_cast<uint8_t *>(in.data());
    z.avail_in = uInt(in.size());
    int rc = Z_OK;
    while (rc == Z_OK) {
        if (z.total_out == out.size()) out.resize(out.size() * 2);
        z.next_out = out.data() + z.total_out;
        z.avail_out = uInt(out.size() - z.total_out);
        rc = inflate(&z, Z_NO_FLUSH);
        if (rc == Z_BUF_ERROR && z.avail_out == 0) rc = Z_OK; // full: grow and go on
    }
    out.resize(z.total_out);
    inflateEnd(&z);
    return rc == Z_STREAM_END;
}

// The number after "key": in the slot's small JSON (written by save_slot).
std::string json_value(const std::string &text, const char *key) {
    const std::string k = std::string("\"") + key + "\":";
    size_t at = text.find(k);
    if (at == std::string::npos) return "";
    at += k.size();
    while (at < text.size() && (text[at] == ' ' || text[at] == '"')) ++at;
    size_t end = at;
    while (end < text.size() && text[end] != '"' && text[end] != ',' && text[end] != '}') ++end;
    return text.substr(at, end - at);
}

} // namespace

bool has_states(const afcore_api *api) {
    const size_t at = offsetof(afcore_api, load_state);
    return api && api->size >= at + sizeof(void *) && api->state_size && api->save_state && api->load_state;
}

fs::path states_dir(const Options &o, const std::string &romset) { return o.data / "states" / romset; }

std::string StateSlot::describe() const {
    if (!exists) return "Empty";
    char when[64] = "";
    const std::time_t t = std::time_t(date);
    if (const std::tm *tm = std::localtime(&t)) std::strftime(when, sizeof when, "%d %b %Y %H:%M", tm);
    const long s = long(seconds);
    char played[32];
    if (s >= 3600) std::snprintf(played, sizeof played, "%ld:%02ld:%02ld", s / 3600, s / 60 % 60, s % 60);
    else std::snprintf(played, sizeof played, "%ld:%02ld", s / 60, s % 60);
    std::string text = std::string(when) + " (" + played + ")";
    if (older) text += " older version";
    return text;
}

StateSlot read_slot(const Options &o, const Game &g, int slot) {
    StateSlot s;
    std::error_code ec;
    if (!fs::exists(slot_file(o, g, slot, ".afstate"), ec)) return s;
    std::vector<uint8_t> json;
    read_file(slot_file(o, g, slot, ".json"), json);
    const std::string text(json.begin(), json.end());
    s.exists = true;
    s.date = std::atof(json_value(text, "date").c_str());
    s.seconds = std::atof(json_value(text, "seconds").c_str());
    s.core_version = json_value(text, "core_version");
    s.older = g.api && s.core_version != version_string(g.api->core_version);
    if (fs::exists(slot_file(o, g, slot, ".bmp"), ec)) s.picture = slot_file(o, g, slot, ".bmp");
    return s;
}

std::string save_slot(const Options &o, const Game &g, afcore *core, int slot, const uint32_t *px, int w, int h) {
    if (!has_states(g.api)) return "This game's core has no save states.";
    std::vector<uint8_t> raw(g.api->state_size(core));
    char err[256] = "";
    const size_t n = raw.empty() ? 0 : g.api->save_state(core, raw.data(), raw.size(), err, sizeof err);
    if (n == 0) return err[0] ? err : "The state wasn't saved.";
    afc_state_info info{};
    afc_state_info_read(raw.data(), n, &info, nullptr, 0);
    std::vector<uint8_t> packed;
    if (!deflate_raw(raw.data(), n, packed)) return "The state couldn't be compressed.";
    const fs::path dir = states_dir(o, g.romset);
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (!write_file(slot_file(o, g, slot, ".afstate"), packed.data(), packed.size()))
        return "The state couldn't be written to " + dir.string() + ".";
    if (px && w > 0 && h > 0) save_picture(slot_file(o, g, slot, ".bmp"), px, w, h);
    char json[256];
    std::snprintf(json, sizeof json, "{\"date\": %.0f, \"seconds\": %.1f, \"core_version\": \"%s\", \"app_version\": \"%s\"}\n",
                  double(std::time(nullptr)), g.api->frame_hz > 0 ? double(info.frame) / g.api->frame_hz : 0.0,
                  version_string(g.api->core_version).c_str(), INSERT_COIN_VERSION);
    write_file(slot_file(o, g, slot, ".json"), json, std::strlen(json));
    return "";
}

std::string load_slot(const Options &o, const Game &g, afcore *core, int slot) {
    if (!has_states(g.api)) return "This game's core has no save states.";
    std::vector<uint8_t> packed;
    if (!read_file(slot_file(o, g, slot, ".afstate"), packed)) return "There is no state in that slot.";
    std::vector<uint8_t> raw(g.api->state_size(core));
    if (!inflate_raw(packed, raw) || raw.empty()) return "The state file is damaged.";
    char err[256] = "";
    if (g.api->load_state(core, raw.data(), raw.size(), err, sizeof err) != 0) return err[0] ? err : "The state didn't load.";
    return "";
}

void remove_slot(const Options &o, const Game &g, int slot) {
    std::error_code ec;
    for (const char *ext : {".afstate", ".bmp", ".json"}) fs::remove(slot_file(o, g, slot, ext), ec);
}

bool can_resume(const Options &o, const Game &g) {
    if (!o.resume || !has_states(g.api)) return false;
    const StateSlot s = read_slot(o, g, 0);
    return s.exists && !s.older;
}

} // namespace insert_coin
