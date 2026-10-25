// SPDX-License-Identifier: GPL-3.0-only
// Owned launch protocol and cooperative handoff policy. No console API or
// filesystem operations live here; the payload/app integration is separate.
#pragma once
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string_view>

namespace shelf {
inline constexpr char shelf_title_id[] = "PPSA99541";
inline constexpr std::size_t launch_message_capacity = 2048;
enum class LaunchMode { emulator_menu, retroarch_game, native_title };
struct LaunchRequest {
    char id[33]{}, title[10]{}, rom[768]{}, core[64]{};
    std::uint64_t issued = 0;
    LaunchMode mode = LaunchMode::emulator_menu;
};
inline bool launch_title_valid(std::string_view title) {
    if (title.size() != 9 || (title.substr(0, 4) != "PPSA" && title.substr(0, 4) != "CUSA"))
        return false;
    for (char c : title.substr(4))
        if (c < '0' || c > '9')
            return false;
    return title != shelf_title_id;
}
inline bool launch_menu_known(std::string_view title) {
    for (auto known : {"PPSA99169", "PPSA99203", "PPSA97510", "PPSA99303", "PPSA99008", "PPSA99764", "PPSA99360",
                       "PPSA99064", "PPSA99247", "PPSA98510"})
        if (title == known)
            return true;
    return false;
}
template <std::size_t N> inline bool launch_bounded(const char (&text)[N]) {
    return std::memchr(text, 0, N) != nullptr;
}
inline bool launch_media_path(std::string_view path) {
    bool permitted = false;
    for (std::string_view root : {"/data/homebrew/PPSA99169/content/", "/data/homebrew/PPSA99169/roms/",
                                  "/data/PSXS5/games/", "/data/SwanStationPS5/games/"})
        if (path.size() > root.size() && path.substr(0, root.size()) == root)
            permitted = true;
    if (!permitted || path.back() == '/' || path.find('\\') != path.npos)
        return false;
    for (unsigned char c : path)
        if (c < 32 || c == 127)
            return false;
    std::size_t start = 1;
    while (start < path.size()) {
        const auto end = path.find('/', start);
        const auto part = path.substr(start, end == path.npos ? path.size() - start : end - start);
        if (part.empty() || part == "." || part == "..")
            return false;
        start = end == path.npos ? path.size() : end + 1;
    }
    return true;
}
inline bool launch_request_valid(const LaunchRequest& r, std::uint64_t now) {
    if (!launch_bounded(r.id) || !launch_bounded(r.title) || !launch_bounded(r.rom) || !launch_bounded(r.core) ||
        std::strlen(r.id) != 32 || !launch_title_valid(r.title) || !r.issued || !now)
        return false;
    for (char c : std::string_view(r.id))
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
            return false;
    if ((now >= r.issued && now - r.issued > 30) || (r.issued > now && r.issued - now > 2))
        return false;
    if (r.mode == LaunchMode::emulator_menu)
        return launch_menu_known(r.title) && !r.rom[0] && !r.core[0];
    if (r.mode == LaunchMode::native_title)
        return !r.rom[0] && !r.core[0];
    if (r.mode != LaunchMode::retroarch_game || std::strcmp(r.title, "PPSA99169") != 0 || !r.core[0] ||
        !launch_media_path(r.rom))
        return false;
    for (char c : std::string_view(r.core))
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'))
            return false;
    return true;
}
inline const char* launch_mode_name(LaunchMode mode) {
    if (mode == LaunchMode::emulator_menu)
        return "menu";
    if (mode == LaunchMode::retroarch_game)
        return "retroarch";
    return mode == LaunchMode::native_title ? "native" : "invalid";
}
inline bool encode_launch_request(const LaunchRequest& request, std::uint64_t now, char* output, std::size_t capacity) {
    if (!output || !capacity || !launch_request_valid(request, now))
        return false;
    const int used = std::snprintf(output, capacity,
                                   "console-shelf-launch 1\nid=%s\nissued=%llu\nmode=%s\ntitle=%s\nrom=%s\ncore=%s\n",
                                   request.id, static_cast<unsigned long long>(request.issued),
                                   launch_mode_name(request.mode), request.title, request.rom, request.core);
    return used > 0 && static_cast<std::size_t>(used) < capacity &&
           static_cast<std::size_t>(used) < launch_message_capacity;
}
inline bool parse_launch_request(std::string_view message, std::uint64_t now, LaunchRequest& output) {
    if (message.empty() || message.size() >= launch_message_capacity || message.back() != '\n')
        return false;
    for (unsigned char c : message)
        if ((c < 32 && c != '\n') || c == 127)
            return false;
    constexpr std::string_view header = "console-shelf-launch 1\n";
    if (message.substr(0, header.size()) != header)
        return false;
    message.remove_prefix(header.size());
    auto field = [&message](std::string_view tag, char* value, std::size_t capacity) {
        const auto newline = message.find('\n');
        if (newline == message.npos || message.substr(0, tag.size()) != tag || newline < tag.size() ||
            newline - tag.size() >= capacity)
            return false;
        const auto length = newline - tag.size();
        std::memcpy(value, message.data() + tag.size(), length);
        value[length] = 0;
        message.remove_prefix(newline + 1);
        return true;
    };
    LaunchRequest candidate;
    char issued[21]{}, mode[16]{};
    if (!field("id=", candidate.id, sizeof(candidate.id)) || !field("issued=", issued, sizeof(issued)) ||
        !field("mode=", mode, sizeof(mode)) || !field("title=", candidate.title, sizeof(candidate.title)) ||
        !field("rom=", candidate.rom, sizeof(candidate.rom)) ||
        !field("core=", candidate.core, sizeof(candidate.core)) || !message.empty() || !issued[0])
        return false;
    for (char c : std::string_view(issued)) {
        if (c < '0' || c > '9' || candidate.issued > (UINT64_MAX - static_cast<unsigned>(c - '0')) / 10)
            return false;
        candidate.issued = candidate.issued * 10 + static_cast<unsigned>(c - '0');
    }
    if (std::strcmp(mode, "menu") == 0)
        candidate.mode = LaunchMode::emulator_menu;
    else if (std::strcmp(mode, "retroarch") == 0)
        candidate.mode = LaunchMode::retroarch_game;
    else if (std::strcmp(mode, "native") == 0)
        candidate.mode = LaunchMode::native_title;
    else
        return false;
    if (!launch_request_valid(candidate, now))
        return false;
    output = candidate;
    return true;
}
// RetroArch v4's public external-forwarder contract. These are argument values,
// never shell commands; filenames with spaces remain a single argv element.
inline std::array<const char*, 6> launch_arguments(const LaunchRequest& request) {
    if (request.mode == LaunchMode::retroarch_game)
        return {"--rom", request.rom, "--core", request.core, "--exit-after-game", nullptr};
    return {};
}
enum class HandoffResult { invalid, foreground_changed, reply_failed, close_timeout, started, unconfirmed, refused };
struct HandoffOps {
    void* context = nullptr;
    // 0: no foreground app; 1: title filled; -1: current title is unknown.
    int (*current)(void*, char (&title)[10]) = nullptr;
    bool (*ready)(void*, const char* request_id) = nullptr;
    void (*wait)(void*, unsigned milliseconds) = nullptr;
    // One launch attempt only. No process kill, alternate launch or retry.
    int (*launch)(void*, const LaunchRequest&) = nullptr;
    std::uint64_t (*now)(void*) = nullptr;
};
inline HandoffResult cooperative_handoff(const LaunchRequest& request, const HandoffOps& ops) {
    if (!ops.current || !ops.ready || !ops.wait || !ops.launch || !ops.now ||
        !launch_request_valid(request, ops.now(ops.context)))
        return HandoffResult::invalid;
    char title[10]{};
    if (ops.current(ops.context, title) != 1 || !launch_bounded(title) || std::strcmp(title, shelf_title_id) != 0)
        return HandoffResult::foreground_changed;
    if (!ops.ready(ops.context, request.id))
        return HandoffResult::reply_failed;
    unsigned idle = 0;
    for (unsigned i = 0; i < 60; ++i) {
        title[0] = 0;
        const int current = ops.current(ops.context, title);
        if (current < 0 || (current && (!launch_bounded(title) || std::strcmp(title, shelf_title_id) != 0)))
            return HandoffResult::foreground_changed;
        idle = current == 0 ? idle + 1 : 0;
        if (idle >= 2)
            break;
        if (i == 59)
            return HandoffResult::close_timeout;
        ops.wait(ops.context, 250);
    }
    if (!launch_request_valid(request, ops.now(ops.context)))
        return HandoffResult::invalid;
    const int result = ops.launch(ops.context, request);
    for (unsigned i = 0; i < (result >= 0 ? 32u : 16u); ++i) {
        title[0] = 0;
        const int current = ops.current(ops.context, title);
        if (current == 1 && launch_bounded(title) && std::strcmp(title, request.title) == 0)
            return HandoffResult::started;
        // An unknown or different app can never authorize a second launch.
        if (current != 0)
            break;
        ops.wait(ops.context, 250);
    }
    return result >= 0 ? HandoffResult::unconfirmed : HandoffResult::refused;
}
} // namespace shelf
