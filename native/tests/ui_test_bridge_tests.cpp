// SPDX-License-Identifier: GPL-3.0-only
#include "shelf/ui_test_bridge.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif
namespace {
void check(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "FAIL UI test bridge: %s\n", message);
        std::exit(1);
    }
}
void make_directory(const char* path) {
#ifdef _WIN32
    check(_mkdir(path) == 0, "create owned fixture directory");
#else
    check(mkdir(path, 0700) == 0, "create owned fixture directory");
#endif
}
void write(const std::string& path, const void* bytes, std::size_t count) {
    FILE* output = std::fopen(path.c_str(), "wb");
    check(output != nullptr, "open owned fixture");
    check(std::fwrite(bytes, 1, count, output) == count, "write owned fixture");
    check(std::fclose(output) == 0, "close owned fixture");
}
void write(const std::string& path, const std::string& text) { write(path, text.data(), text.size()); }
std::string read(const std::string& path) {
    FILE* input = std::fopen(path.c_str(), "rb");
    check(input != nullptr, "open response fixture");
    std::string text;
    char bytes[1024];
    std::size_t count;
    while ((count = std::fread(bytes, 1, sizeof(bytes), input)) != 0)
        text.append(bytes, count);
    check(!std::ferror(input) && std::fclose(input) == 0, "read complete response fixture");
    return text;
}
bool exists(const std::string& path) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file)
        return false;
    std::fclose(file);
    return true;
}
std::string command(const char* session, std::uint64_t sequence, std::uint64_t revision, const char* action) {
    char text[180];
    std::snprintf(text, sizeof(text), "ISSHELF_UI_TEST_1 %s %llu %llu %s\n", session,
                  static_cast<unsigned long long>(sequence), static_cast<unsigned long long>(revision), action);
    return text;
}
std::uint32_t crc(const unsigned char* bytes, std::size_t count) {
    std::uint32_t value = 0xffffffffu;
    for (std::size_t i = 0; i < count; ++i) {
        value ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            value = (value >> 1) ^ ((value & 1) ? 0xedb88320u : 0u);
    }
    return value ^ 0xffffffffu;
}
void be32(std::vector<unsigned char>& bytes, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8)
        bytes.push_back(static_cast<unsigned char>(value >> shift));
}
void chunk(std::vector<unsigned char>& bytes, const char* type, const std::vector<unsigned char>& content) {
    be32(bytes, static_cast<std::uint32_t>(content.size()));
    const auto begin = bytes.size();
    for (unsigned i = 0; i < 4; ++i)
        bytes.push_back(static_cast<unsigned char>(type[i]));
    bytes.insert(bytes.end(), content.begin(), content.end());
    be32(bytes, crc(bytes.data() + begin, 4 + content.size()));
}
std::vector<unsigned char> image(std::uint32_t width = 1) {
    std::vector<unsigned char> bytes{137, 80, 78, 71, 13, 10, 26, 10}, header;
    be32(header, width);
    be32(header, 1);
    header.insert(header.end(), {8, 6, 0, 0, 0});
    chunk(bytes, "IHDR", header);
    // A complete zlib stream containing one unfiltered RGBA pixel.
    chunk(bytes, "IDAT", {0x78, 0x01, 0x01, 0x05, 0x00, 0xfa, 0xff, 0x00,
                           0x11, 0x22, 0x33, 0xff, 0x02, 0x14, 0x01, 0x66});
    chunk(bytes, "IEND", {});
    return bytes;
}
std::string fnv(const std::vector<unsigned char>& bytes) {
    std::uint64_t value = 14695981039346656037ull;
    for (auto byte : bytes)
        value = (value ^ byte) * 1099511628211ull;
    char text[17];
    std::snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(value));
    return text;
}
void contains(const std::string& json, const char* text, const char* message) {
    check(json.find(text) != std::string::npos, message);
}
} // namespace
void test_ui_test_bridge() {
    using namespace shelf;
    constexpr const char* session = "0123456789abcdef0123456789abcdef";
    constexpr const char* other = "1123456789abcdef0123456789abcdef";
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    char root[128];
    std::snprintf(root, sizeof(root), "ui-test-bridge-%lld", static_cast<long long>(stamp));
    make_directory(root);
    const std::string directory = std::string(root) + "/ui-test/";
    const auto request_file = directory + "request.txt", response = directory + "response.json";
    const auto published_frame = directory + "frame.png";
    UiTestBridge bridge;
    UiTestRequest request;
    check(!bridge.active() && !bridge.poll(request), "bridge starts disabled");
    check(!bridge.start(root, "short") && !bridge.start(root, "0123456789ABCDEF0123456789abcdef"),
          "invalid sessions cannot activate transport");
    check(bridge.start(root, session) && bridge.active(), "start creates owned transport directory");
    check(!exists(response) && !bridge.poll(request), "start has no stale response or command");
    check(std::string(bridge.frame_path()) == directory + "frame.new.png", "capture path is fixed and owned");
    UiState state;
    state.selected_system = find_system("ps2");
    state.focus = 7;
    state.game_focus = 2;
    state.tab = SettingsTab::room;
    std::strcpy(state.query, "private-query");
    std::strcpy(state.settings.tv_text, "private-room-text");
    std::strcpy(state.notice, "private-path/data/game.cue");
    UiTestRequest ready;
    std::strcpy(ready.name, "snapshot");
    check(bridge.publish(ready, 4, "ready", false, true, state, false), "initial ready response publishes");
    auto json = read(response);
    contains(json, "\"sequence\":0", "ready has synthetic sequence zero");
    contains(json, "\"nextSequence\":1", "ready names next expected command");
    contains(json, "\"selectedSystem\":\"ps2\"", "response uses stable console ID");
    contains(json, "\"frameAvailable\":false", "no capture never claims an image");
    check(json.find("private") == std::string::npos, "responses omit queries, messages, game paths and notices");
    check(!exists(directory + "response.new.json") && json.back() == '\n', "response publishes complete then renames");

    const auto valid = command(session, 1, 4, "snapshot");
    std::vector<std::string> invalid{
        valid.substr(0, valid.size() - 1), valid + "\n", valid + "extra", command(other, 1, 4, "snapshot"),
        command(session, 0, 4, "snapshot"), command(session, 2, 4, "snapshot"), command(session, 1, 4, "unknown"),
        command(session, 1, 4, "../frame.png"), command(session, 1, 4, "right left"),
        command(session, 1, 4, "wait:100"), command(session, 1, 4, "circle-down"),
        command(session, 1, 4, "text:abc"), command(session, 1, 4, "RIGHT"),
        "ISSHELF_UI_TEST_1 " + std::string(session) + " 01 4 snapshot\n",
        "ISSHELF_UI_TEST_1 " + std::string(session) + " 1 04 snapshot\n",
        "ISSHELF_UI_TEST_1 " + std::string(session) + " 18446744073709551616 4 snapshot\n",
        "ISSHELF_UI_TEST_1 " + std::string(session) + " 1 18446744073709551616 snapshot\n",
        "ISSHELF_UI_TEST_1 " + std::string(session) + " 1 4  snapshot\n",
        std::string(180, 'x') + "\n"
    };
    auto zero = valid;
    zero[zero.size() - 2] = 0;
    invalid.push_back(zero);
    auto crlf = valid;
    crlf.insert(crlf.size() - 1, "\r");
    invalid.push_back(crlf);
    for (const auto& text : invalid) {
        write(request_file, text);
        check(!bridge.poll(request), "malformed, stale, replayable or unsupported commands never dispatch");
    }
    write(directory + "request.new.txt", valid);
    check(!bridge.poll(request), "incomplete upload path is never read");
    check(std::remove(request_file.c_str()) == 0 &&
              std::rename((directory + "request.new.txt").c_str(), request_file.c_str()) == 0,
          "fixture atomically publishes request");
    check(bridge.poll(request) && request.sequence == 1 && request.revision == 4 &&
              request.operation == UiTestOperation::snapshot, "exact next snapshot is accepted");
    check(!bridge.poll(request), "retained request never dispatches a second time");

    write(request_file, command(session, 2, 4, "right"));
    check(bridge.poll(request) && request.sequence == 2 && request.action == Action::right &&
              request.operation == UiTestOperation::action, "one discrete action is parsed");
    const auto pixels = image();
    write(bridge.frame_path(), pixels.data(), pixels.size());
    check(bridge.publish(request, 5, "applied", true, true, state, true), "matching PNG response publishes");
    json = read(response);
    contains(json, "\"nextSequence\":3", "command sequence is consumed before response");
    contains(json, "\"frameAvailable\":true", "complete validated PNG is available");
    contains(json, "\"actionApplied\":true", "successful action records applied outcome");
    const auto hash_field = "\"frameFNV64\":\"" + fnv(pixels) + "\"";
    contains(json, hash_field.c_str(), "response hash binds exact PNG bytes");
    check(read(published_frame) == std::string(reinterpret_cast<const char*>(pixels.data()), pixels.size()) &&
              !exists(bridge.frame_path()), "image is renamed completely before response");
    check(!bridge.poll(request), "response publication does not make action replayable");
    for (const auto& broken : {std::vector<unsigned char>(pixels.begin(), pixels.end() - 1), image(1921)}) {
        write(bridge.frame_path(), broken.data(), broken.size());
        check(bridge.publish(request, 5, "applied", true, false, state, true),
              "capture failure still publishes independent action result");
        json = read(response);
        contains(json, "\"actionApplied\":true", "failed capture does not deny an already applied action");
        contains(json, "\"settingsSaved\":false", "persistence status is independent");
        contains(json, "\"frameAvailable\":false", "incomplete or oversized frame is unavailable");
        contains(json, "\"frameBytes\":0", "failed capture has zero image size");
        contains(json, "\"frameFNV64\":\"0000000000000000\"", "failed capture carries no stale digest");
        check(!exists(published_frame) && !exists(bridge.frame_path()), "failed image cannot reuse stale frame");
    }
    auto corrupt = pixels;
    corrupt[corrupt.size() - 1] ^= 1;
    write(bridge.frame_path(), corrupt.data(), corrupt.size());
    check(bridge.publish(request, 5, "applied", true, true, state, true), "corrupt CRC returns response");
    contains(read(response), "\"frameAvailable\":false", "corrupt CRC is rejected");
    write(bridge.frame_path(), pixels.data(), pixels.size());
    check(bridge.publish(request, 5, "capture-failed", true, true, state, false), "explicit capture failure publishes");
    check(!exists(bridge.frame_path()) && !exists(published_frame), "explicit failed capture cannot expose previous files");
    const auto safe_response = read(response);
    check(!bridge.publish(request, 5, "bad\"result", true, true, state, false) && read(response) == safe_response,
          "untrusted response labels cannot inject JSON or replace response");

    std::uint64_t sequence = 3;
    constexpr const char* names[] = {"up", "down", "left", "right", "confirm", "back", "settings", "layout",
                                      "favorite", "previous-tab", "tab"};
    constexpr Action actions[] = {Action::up, Action::down, Action::left, Action::right, Action::confirm,
                                   Action::back, Action::settings, Action::layout, Action::favorite,
                                   Action::previous_tab, Action::next_tab};
    for (unsigned i = 0; i < sizeof(actions) / sizeof(actions[0]); ++i) {
        write(request_file, command(session, sequence, 0, names[i]));
        check(bridge.poll(request) && request.sequence == sequence++ && request.revision == 0 &&
                  request.action == actions[i], "allowlist maps exactly one action and preserves revision for caller");
        check(!bridge.poll(request), "each allowlisted action is consumed once");
    }
    write(request_file, command(session, sequence, 5, "stop"));
    check(bridge.poll(request) && request.operation == UiTestOperation::stop, "stop is distinct from UI Back");
    check(bridge.publish(request, 6, "stopped", false, true, state, false) && !bridge.active(),
          "stop publishes inactive state and deactivates transport");
    contains(read(response), "\"active\":false", "final response remains available after stop");
    check(!bridge.poll(request), "stopped session cannot dispatch");
    write(published_frame, "old-frame");
    check(bridge.start(root, other) && !exists(response) && !exists(published_frame),
          "new session makes previous response and frame unavailable");
    check(!bridge.poll(request), "old session request cannot act after reopen");
    write(request_file, command(other, 1, 0, "snapshot"));
    check(bridge.poll(request) && request.sequence == 1, "new process session starts a fresh sequence");
    bridge.stop();
    check(!bridge.poll(request) && bridge.publish(request, 0, "closed", false, true, state, false),
          "stopped transport rejects commands but can publish its final response");
    contains(read(response), "\"active\":false", "explicit stop is reflected in terminal response");
    for (const char* terminal : {"owner-stopped", "expired"}) {
        check(bridge.publish(request, 1, terminal, false, true, state, false),
              "owner cancellation and expiry publish after stop");
        const auto terminal_response = read(response);
        contains(terminal_response, "\"active\":false", "terminal status cannot reactivate transport");
        contains(terminal_response, terminal, "terminal status preserves its exact disposition");
        check(!bridge.poll(request), "terminal publication never reenables request dispatch");
    }
    check(!bridge.start(root, "invalid") && !bridge.publish(request, 0, "closed", false, true, state, false),
          "failed start cannot publish using an older session");
    write(request_file, "malformed-stale-request");
    check(bridge.start(root, session) && !bridge.poll(request),
          "a retained malformed old request stays inert after session start");
    write(request_file, command(session, 1, 0, "snapshot"));
    check(bridge.poll(request), "malformed old file cannot consume the fresh session sequence");
    bridge.stop();

    UiState home;
    check(apply_ui_test_action(home, Action::right) && home.focus == 1, "remote direction applies one menu step");
    check(apply_ui_test_action(home, Action::back) && !home.exit_requested && home.close_hint_requested,
          "remote Back never simulates a hold-to-close");
    check(apply_ui_test_action(home, Action::settings) && home.page == Page::settings,
          "ordinary UI transitions remain available");
    static Library library;
    const auto ps1 = find_system("ps1");
    library.route_count = 1;
    auto& route = library.routes[0];
    route.system = ps1;
    std::strcpy(route.id, "psswanstation");
    std::strcpy(route.title_id, "PPSA99248");
    std::strcpy(route.folder, "/data/homebrew/PPSA99248");
    UiState launch;
    launch.library = &library;
    launch.launch_connected = true;
    launch.page = Page::details;
    launch.selected_system = ps1;
    launch.detail_focus = 1;
    std::strcpy(launch.notice, "kept-original-notice");
    check(!apply_ui_test_action(launch, Action::confirm) && !launch.launch_requested &&
              std::strcmp(launch.notice, "kept-original-notice") == 0,
          "emulator-menu launch attempt is refused without changing live state");
    library.game_count = 1;
    auto& game = library.games[0];
    game.system = ps1;
    game.kind = MediaKind::cue;
    std::strcpy(game.name, "Private test game");
    std::strcpy(game.path, "/data/PSXS5/games/private.cue");
    launch.page = Page::library;
    check(!apply_ui_test_action(launch, Action::confirm) && !launch.launch_requested &&
              std::strcmp(launch.notice, "kept-original-notice") == 0,
          "selected PS1 direct launch is refused without changing live state");
    game.system = find_system("ps5");
    game.kind = MediaKind::native_title;
    std::strcpy(game.title_id, "PPSA12345");
    std::strcpy(game.path, "/user/app/PPSA12345");
    launch.selected_system = game.system;
    check(!apply_ui_test_action(launch, Action::confirm) && !launch.launch_requested,
          "native-title launch is also refused");
    launch.exit_requested = true;
    check(!apply_ui_test_action(launch, Action::right) && launch.exit_requested,
          "an already exiting state cannot be changed by remote input");
    std::puts("PASS: private UI bridge bounded protocol, session/replay rejection, atomic frame receipts, capture failures and launch suppression.");
}
