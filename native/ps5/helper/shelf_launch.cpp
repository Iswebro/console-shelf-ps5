// SPDX-License-Identifier: GPL-3.0-only
// Shelf's cooperative launcher. Public launch ABI/architecture references:
// Jewel Case helper/jc-launch.c at 765297d2decd1be6c8076261f484e36724250bd7
// and OpenOrbis include/orbis/_types/sys_service.h at
// 06fcf7fd108fa080035cdd3259ed22eb07e0508c. No process kill, external
// file-write requests, new server, payload autoload or firmware-update call.
#include "shelf/launch_protocol.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <unistd.h>

extern "C" {
int sceUserServiceInitialize(void*);
int sceUserServiceGetForegroundUser(std::uint32_t*);
int sceSystemServiceGetAppIdOfRunningBigApp();
int sceSystemServiceGetAppTitleId(int, char*);
int sceLncUtilInitialize();
struct LncParams {
    std::uint32_t size;
    std::int32_t user;
    std::uint32_t options;
    std::uint64_t crash_report, check_flags;
};
int sceLncUtilLaunchApp(const char*, const char**, LncParams*);
}
static_assert(sizeof(LncParams) == 32 && offsetof(LncParams, check_flags) == 24, "Unexpected launch ABI");
namespace {
constexpr char request_path[] = "/data/console-shelf/launch-request.txt";
constexpr char status_path[] = "/data/console-shelf/launch-status.txt";
constexpr char status_temporary[] = "/data/console-shelf/launch-status.tmp";
struct Context {
    const shelf::LaunchRequest* request = nullptr;
    int origin_app = 0;
    std::uint32_t user = 0;
    int launch_status = 0;
    int probe_status = 0;
    FILE* journal = nullptr;
    ~Context() {
        if (journal)
            std::fclose(journal);
    }
};
std::uint64_t now(void*) {
    const auto value = std::time(nullptr);
    return value > 0 ? static_cast<std::uint64_t>(value) : 0;
}
bool status(const shelf::LaunchRequest& request, shelf::LaunchStage state, int code) {
    shelf::LaunchStatus reply;
    std::memcpy(reply.id, request.id, sizeof(reply.id));
    reply.issued = request.issued;
    reply.stage = state;
    reply.code = static_cast<std::uint32_t>(code);
    char message[256];
    if (!shelf::encode_launch_status(reply, message, sizeof(message)))
        return false;
    FILE* file = std::fopen(status_temporary, "wb");
    if (!file)
        return false;
    bool okay = std::fwrite(message, 1, std::strlen(message), file) == std::strlen(message);
    okay = std::fflush(file) == 0 && okay;
    okay = std::fclose(file) == 0 && okay;
    if (okay && std::rename(status_temporary, status_path) == 0)
        return true;
    std::remove(status_temporary);
    return false;
}
int current(void* raw, char (&title)[10]) {
    auto& context = *static_cast<Context*>(raw);
    const int id = sceSystemServiceGetAppIdOfRunningBigApp();
    if (context.journal) {
        std::fprintf(context.journal, "current app: 0x%08x\n", static_cast<unsigned>(id));
        std::fflush(context.journal);
    }
    // -1 and zero are the no-app values used by the reference helper. Other
    // errors must not become permission to launch over an unknown application.
    if (id == 0 || id == -1)
        return 0;
    if (id < 0) {
        context.probe_status = id;
        return -1;
    }
    char value[32]{};
    const int title_status = sceSystemServiceGetAppTitleId(id, value);
    context.probe_status = title_status;
    if (context.journal) {
        std::fprintf(context.journal, "title query: 0x%08x; title: %.31s\n", static_cast<unsigned>(title_status), value);
        std::fflush(context.journal);
    }
    if (title_status != 0 || !std::memchr(value, 0, sizeof(value)) ||
        std::strlen(value) != 9)
        return -1;
    for (unsigned i = 0; i < 9; ++i)
        if (!((value[i] >= 'A' && value[i] <= 'Z') || (value[i] >= '0' && value[i] <= '9')))
            return -1;
    if (std::strcmp(value, shelf::shelf_title_id) == 0) {
        if (context.origin_app && context.origin_app != id)
            return -1;
        context.origin_app = id;
    }
    std::memcpy(title, value, 10);
    return 1;
}
bool ready(void* raw, const char* id) {
    const auto& context = *static_cast<Context*>(raw);
    return context.request && std::strcmp(id, context.request->id) == 0 &&
           status(*context.request, shelf::LaunchStage::ready, 0);
}
shelf::HandoffConsent consent(void* raw) {
    const auto& context = *static_cast<Context*>(raw);
    FILE* file = std::fopen(status_path, "rb");
    if (!file || !context.request) {
        if (file)
            std::fclose(file);
        return shelf::HandoffConsent::cancelled;
    }
    char message[256];
    const auto size = std::fread(message, 1, sizeof(message), file);
    const bool failed = std::ferror(file) != 0;
    std::fclose(file);
    shelf::LaunchStatus reply;
    if (failed || !shelf::parse_launch_status({message, size}, now(nullptr), reply) ||
        std::strcmp(reply.id, context.request->id) != 0 || reply.issued != context.request->issued)
        return shelf::HandoffConsent::cancelled;
    if (reply.stage == shelf::LaunchStage::committed)
        return shelf::HandoffConsent::committed;
    return reply.stage == shelf::LaunchStage::ready ? shelf::HandoffConsent::pending : shelf::HandoffConsent::cancelled;
}
void wait(void*, unsigned milliseconds) {
    usleep(milliseconds * 1000);
}
int launch(void* raw, const shelf::LaunchRequest& request) {
    auto& context = *static_cast<Context*>(raw);
    auto args = shelf::launch_arguments(request);
    LncParams params{};
    params.size = sizeof(params);
    params.user = static_cast<std::int32_t>(context.user);
    // Public LaunchApp flag SkipSystemUpdate. This launches only a selected
    // installed title; the helper has no update/reboot/install functionality.
    params.check_flags = 2;
    context.launch_status = sceLncUtilLaunchApp(request.title, args[0] ? args.data() : nullptr, &params);
    return context.launch_status;
}
shelf::LaunchStage result_stage(shelf::HandoffResult result) {
    switch (result) {
    case shelf::HandoffResult::invalid:
        return shelf::LaunchStage::invalid;
    case shelf::HandoffResult::foreground_changed:
        return shelf::LaunchStage::foreground_changed;
    case shelf::HandoffResult::reply_failed:
        return shelf::LaunchStage::reply_failed;
    case shelf::HandoffResult::close_timeout:
        return shelf::LaunchStage::close_timeout;
    case shelf::HandoffResult::started:
        return shelf::LaunchStage::started;
    case shelf::HandoffResult::unconfirmed:
        return shelf::LaunchStage::unconfirmed;
    case shelf::HandoffResult::refused:
        return shelf::LaunchStage::refused;
    case shelf::HandoffResult::cancelled:
        return shelf::LaunchStage::cancelled;
    }
    return shelf::LaunchStage::invalid;
}
} // namespace
int main() {
    char message[shelf::launch_message_capacity];
    FILE* incoming = std::fopen(request_path, "rb");
    if (!incoming)
        return 1;
    const auto used = std::fread(message, 1, sizeof(message), incoming);
    const bool error = std::ferror(incoming) != 0;
    std::fclose(incoming);
    shelf::LaunchRequest request;
    if (error || !shelf::parse_launch_request({message, used}, now(nullptr), request))
        return 1;
    // Only the reader that successfully consumes this request may acknowledge
    // it or launch anything. A duplicate helper has nothing left to consume.
    if (unlink(request_path) != 0)
        return 1;
    Context context;
    context.request = &request;
    context.journal = std::fopen("/data/console-shelf/launch-helper.log", "wb");
    sceUserServiceInitialize(nullptr);
    const int initialized = sceLncUtilInitialize();
    const int user_status = sceUserServiceGetForegroundUser(&context.user);
    if (context.journal) {
        std::fprintf(context.journal,
#ifdef SHELF_STANDARD_SDK_RUNTIME
                     "ui-13 standard SDK helper; init: 0x%08x; user query: 0x%08x\n",
#else
                     "ui-13 restricted helper; init: 0x%08x; user query: 0x%08x\n",
#endif
                     static_cast<unsigned>(initialized), static_cast<unsigned>(user_status));
        std::fflush(context.journal);
    }
    if (initialized < 0 || user_status != 0 || context.user > 0x7fffffffu) {
        status(request, shelf::LaunchStage::service_unavailable, user_status ? user_status : initialized);
        return 1;
    }
    const auto result = shelf::cooperative_handoff(request, {&context, current, ready, wait, launch, now, consent});
    status(request, result_stage(result),
           result == shelf::HandoffResult::foreground_changed ? context.probe_status : context.launch_status);
    return result == shelf::HandoffResult::started ? 0 : 1;
}
