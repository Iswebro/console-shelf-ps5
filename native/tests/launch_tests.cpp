// SPDX-License-Identifier: GPL-3.0-only
#include "shelf/launch_protocol.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
namespace {
void check(bool value, const char* why) {
    if (!value) {
        std::fprintf(stderr, "FAIL launch protocol: %s\n", why);
        std::exit(1);
    }
}
shelf::LaunchRequest request() {
    shelf::LaunchRequest result;
    std::strcpy(result.id, "0123456789abcdef0123456789abcdef");
    std::strcpy(result.title, "PPSA99203");
    result.issued = 1000;
    return result;
}
struct FakeHandoff {
    unsigned checks = 0, launches = 0, waits = 0, ready_calls = 0;
    int scenario = 0, launch_result = 1;
    std::uint64_t clock = 1000;
    static int current(void* raw, char (&title)[10]) {
        auto& f = *static_cast<FakeHandoff*>(raw);
        ++f.checks;
        if (f.scenario == 1 || (f.scenario == 2 && f.checks > 1)) {
            std::strcpy(title, "CUSA00001");
            return 1;
        }
        if (f.scenario == 3) {
            std::strcpy(title, shelf::shelf_title_id);
            return 1;
        }
        if (f.scenario == 4 && f.checks > 1)
            return -1;
        if (f.checks <= 2) {
            std::strcpy(title, shelf::shelf_title_id);
            return 1;
        }
        if (f.launches && f.scenario != 5) {
            std::strcpy(title, "PPSA99203");
            return 1;
        }
        return 0;
    }
    static bool ready(void* raw, const char* id) {
        auto& f = *static_cast<FakeHandoff*>(raw);
        ++f.ready_calls;
        check(std::strlen(id) == 32, "ready response is bound to its request ID");
        return f.scenario != 6;
    }
    static void wait(void* raw, unsigned milliseconds) {
        auto& f = *static_cast<FakeHandoff*>(raw);
        check(milliseconds == 250, "handoff polling stays bounded");
        ++f.waits;
        if (f.scenario == 7)
            f.clock += 31;
    }
    static int launch(void* raw, const shelf::LaunchRequest& r) {
        auto& f = *static_cast<FakeHandoff*>(raw);
        ++f.launches;
        check(std::strcmp(r.title, "PPSA99203") == 0, "only the requested target is launched");
        return f.launch_result;
    }
    static std::uint64_t now(void* raw) {
        return static_cast<FakeHandoff*>(raw)->clock;
    }
    static shelf::HandoffConsent consent(void* raw) {
        auto& f = *static_cast<FakeHandoff*>(raw);
        if (f.scenario == 8)
            return shelf::HandoffConsent::pending;
        if (f.scenario == 9 || (f.scenario == 10 && f.waits >= 2))
            return shelf::HandoffConsent::cancelled;
        return shelf::HandoffConsent::committed;
    }
    shelf::HandoffOps ops() {
        return {this, current, ready, wait, launch, now, consent};
    }
};
} // namespace
void test_launch() {
    using namespace shelf;
    auto r = request();
    check(launch_request_valid(r, 1000), "known emulator menu is accepted");
    char message[launch_message_capacity];
    check(encode_launch_request(r, 1000, message, sizeof(message)), "menu request is encoded");
    const std::string original(message);
    LaunchStatus response;
    std::strcpy(response.id, r.id);
    response.issued = r.issued;
    response.stage = LaunchStage::ready;
    response.code = 0x80940033;
    char encoded_status[256];
    LaunchStatus decoded_status;
    check(encode_launch_status(response, encoded_status, sizeof(encoded_status)) &&
              parse_launch_status(encoded_status, 1000, decoded_status) && decoded_status.stage == LaunchStage::ready &&
              decoded_status.code == response.code && std::strcmp(decoded_status.id, r.id) == 0,
          "reply roundtrip binds its ID, timestamp, state and numeric code");
    check(!parse_launch_status(std::string(encoded_status) + "state=committed\n", 1000, decoded_status) &&
              !parse_launch_status(encoded_status, 1031, decoded_status),
          "duplicate and stale replies are rejected");
    LaunchRequest parsed;
    check(parse_launch_request(original, 1000, parsed) && std::strcmp(parsed.id, r.id) == 0 &&
              std::strcmp(parsed.title, r.title) == 0 && parsed.mode == r.mode,
          "request roundtrip keeps its identity and route");
    check(!parse_launch_request(original + "title=PPSA99169\n", 1000, parsed), "duplicate/extra fields are rejected");
    check(!parse_launch_request(original.substr(0, original.size() - 1), 1000, parsed), "partial records are rejected");
    check(!parse_launch_request(original, 1031, parsed) && !parse_launch_request(original, 997, parsed),
          "expired and future requests cannot be replayed");
    check(!parse_launch_request(std::string("\0", 1) + original, 1000, parsed), "embedded NUL is rejected");
    auto malformed = original;
    malformed.replace(malformed.find("1000"), 4, "18446744073709551616");
    check(!parse_launch_request(malformed, 1000, parsed), "timestamp overflow is rejected");
    std::strcpy(r.title, "NPXS40087");
    check(!launch_request_valid(r, 1000), "system utilities are not launch targets");
    std::strcpy(r.title, shelf_title_id);
    check(!launch_request_valid(r, 1000), "a request cannot relaunch Shelf itself");
    std::strcpy(r.title, "PPSA00001");
    check(!launch_request_valid(r, 1000), "an unknown emulator does not gain a menu contract");
    r.mode = LaunchMode::native_title;
    check(launch_request_valid(r, 1000), "a native game has an explicit separate mode");
    r.mode = LaunchMode::retroarch_game;
    std::strcpy(r.title, "PPSA99169");
    std::strcpy(r.rom, "/data/homebrew/PPSA99169/content/n64/Mario Party 3.z64");
    std::strcpy(r.core, "mupen64plus_next");
    check(launch_request_valid(r, 1000) && encode_launch_request(r, 1000, message, sizeof(message)) &&
              parse_launch_request(message, 1000, parsed),
          "an external RetroArch game request is accepted");
    const auto argv = launch_arguments(r);
    check(std::strcmp(argv[0], "--rom") == 0 && argv[1] == r.rom && std::strcmp(argv[2], "--core") == 0 &&
              argv[3] == r.core && std::strcmp(argv[4], "--exit-after-game") == 0 && !argv[5],
          "external argv preserves spaces and does not use the internal frontend mode");
    for (const char* path :
         {"/data/homebrew/PPSA99169/content/../settings.ini", "/data/homebrew/PPSA99169/content//game.z64",
          "/user/settings/game.z64", "/data/homebrew/PPSA99169/content/a\nwrite=/data/other", "/data/PSXS5/games/"}) {
        std::strcpy(r.rom, path);
        check(!launch_request_valid(r, 1000), "out-of-root, traversal, empty segments and line injection are rejected");
    }
    std::strcpy(r.rom, "/data/PSXS5/games/Own game.cue");
    std::strcpy(r.core, "../other");
    check(!launch_request_valid(r, 1000), "a core ID cannot select an arbitrary file");
    r = request();
    std::memset(r.title, 'A', sizeof(r.title));
    check(!launch_request_valid(r, 1000), "nonterminated fields are bounded before string operations");
    r = request();
    FakeHandoff f;
    check(cooperative_handoff(r, f.ops()) == HandoffResult::started && f.launches == 1 && f.ready_calls == 1,
          "cooperative closure leads to one observed launch");
    for (int scenario : {1, 2, 3, 4, 6, 8, 9, 10}) {
        FakeHandoff refused;
        refused.scenario = scenario;
        const auto result = cooperative_handoff(r, refused.ops());
        check(result != HandoffResult::started && result != HandoffResult::unconfirmed && refused.launches == 0,
              "busy/unknown foreground, failed acknowledgement and closure timeout never launch");
    }
    FakeHandoff accepted;
    accepted.scenario = 5;
    check(cooperative_handoff(r, accepted.ops()) == HandoffResult::unconfirmed && accepted.launches == 1 &&
              accepted.waits < 100,
          "an accepted but unobserved launch is not called successful or retried");
    FakeHandoff denied;
    denied.scenario = 5;
    denied.launch_result = -1;
    check(cooperative_handoff(r, denied.ops()) == HandoffResult::refused && denied.launches == 1,
          "a refused launch has no alternate call or retry");
    FakeHandoff appearing;
    appearing.launch_result = -1;
    check(cooperative_handoff(r, appearing.ops()) == HandoffResult::started && appearing.launches == 1,
          "an observed target wins over a negative return without a second launch");
    FakeHandoff expired;
    expired.scenario = 7;
    check(cooperative_handoff(r, expired.ops()) == HandoffResult::invalid && expired.launches == 0,
          "a request expiring during closure cannot launch later");
    std::puts("PASS: bounded launch protocol, stale/injected request refusal, exact RetroArch argv and cooperative "
              "one-attempt handoff mocks. No console launch performed.");
}
