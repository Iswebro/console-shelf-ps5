// SPDX-License-Identifier: GPL-3.0-only
// Shelf's cooperative launcher. Public launch ABI/architecture references:
// Jewel Case helper/jc-launch.c at 765297d2decd1be6c8076261f484e36724250bd7
// OpenOrbis include/orbis/_types/sys_service.h at 06fcf7fd108fa080035cdd3259ed22eb07e0508c,
// websrv src/ps5/sys.c at 7748df9d2a43822a5ebfda149f43d4f2696d640b,
// and ShadowMountPlus src/sm_shellcore_flags.c at 9ffcbe131575153266e951ab473bb6fd100d8c80.
// No process kill, external
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
struct AppLaunchContext {
    std::uint32_t size;
    std::uint32_t user;
    std::uint32_t options;
    std::uint64_t crash_report;
    std::uint32_t check_flags;
};
int sceSystemServiceLaunchApp(const char*, const char**, AppLaunchContext*);
int sceKernelOpenEventFlag(std::intptr_t*, const char*);
int sceKernelPollEventFlag(std::intptr_t, std::uint64_t, unsigned, std::uint64_t*);
int sceKernelCloseEventFlag(std::intptr_t);
}
static_assert(sizeof(AppLaunchContext) == 32 && offsetof(AppLaunchContext, check_flags) == 24, "Unexpected launch ABI");
namespace {
constexpr char request_path[] = "/data/console-shelf/launch-request.txt";
constexpr char status_path[] = "/data/console-shelf/launch-status.txt";
constexpr char status_temporary[] = "/data/console-shelf/launch-status.tmp";
struct Context {
    const shelf::LaunchRequest* request = nullptr;
    int origin_app = 0, target_app = 0;
    std::uint32_t user = 0;
    int launch_status = 0;
    int probe_status = 0;
    std::intptr_t focus_handle = -1;
    bool focus_open = false;
    FILE* journal = nullptr;
    ~Context() {
        if (focus_open)
            sceKernelCloseEventFlag(focus_handle);
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
    // Never replace a newer request's reply, even if this helper finishes late.
    FILE* prior = std::fopen(status_path, "rb");
    if (!prior)
        return false;
    char existing[256];
    const auto count = std::fread(existing, 1, sizeof(existing), prior);
    const bool bad = std::ferror(prior) != 0;
    std::fclose(prior);
    shelf::LaunchStatus owner;
    if (bad || !shelf::parse_launch_status({existing, count}, now(nullptr), owner) ||
        std::strcmp(owner.id, request.id) != 0 || owner.issued != request.issued)
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
    if (context.request && std::strcmp(value, context.request->title) == 0) {
        if (context.target_app && context.target_app != id)
            return -1;
        context.target_app = id;
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
    std::uint32_t user = 0;
    if (sceUserServiceGetForegroundUser(&user) != 0 || user != context.user ||
        !shelf::launch_request_valid(request, now(nullptr)) || consent(raw) != shelf::HandoffConsent::committed)
        return context.launch_status = -1;
    auto args = shelf::launch_arguments(request);
    AppLaunchContext params{};
    params.size = sizeof(params);
    params.user = user;
    // Public LaunchApp flag SkipSystemUpdate. This launches only a selected
    // installed title; the helper has no update/reboot/install functionality.
    params.check_flags = 2;
    context.launch_status = sceSystemServiceLaunchApp(request.title, args[0] ? args.data() : nullptr, &params);
    if (context.journal) {
        std::fprintf(context.journal, "SystemService launch: 0x%08x; check_flags: 0x%08x\n",
                     static_cast<unsigned>(context.launch_status), params.check_flags);
        std::fflush(context.journal);
    }
    return context.launch_status;
}
int focused(void* raw, const shelf::LaunchRequest& request) {
    auto& context = *static_cast<Context*>(raw);
    char title[10]{};
    std::uint32_t user = 0;
    if (!context.target_app || current(raw, title) != 1 || std::strcmp(title, request.title) != 0 ||
        sceUserServiceGetForegroundUser(&user) != 0 || user != context.user ||
        !shelf::launch_request_valid(request, now(nullptr)) || consent(raw) != shelf::HandoffConsent::committed)
        return -1;
    if (!context.focus_open) {
        const int opened = sceKernelOpenEventFlag(&context.focus_handle, "SceShellCoreUtilAppFocus");
        if (opened < 0) {
            if (context.journal)
                std::fprintf(context.journal, "read-only focus flag unavailable: 0x%08x\n", static_cast<unsigned>(opened));
            return -1;
        }
        context.focus_open = true;
    }
    std::uint64_t value = 0;
    int observed = sceKernelPollEventFlag(context.focus_handle, UINT64_MAX, 2u, &value);
    // OR over every bit reports EBUSY for zero; never use clear/set wait modes.
    if (static_cast<std::uint32_t>(observed) == 0x80020010u) {
        value = 0;
        observed = 0;
    }
    if (context.journal) {
        std::fprintf(context.journal, "read-only shell focus: rc=0x%08x value=0x%016llx target=0x%08x\n",
                     static_cast<unsigned>(observed), static_cast<unsigned long long>(value),
                     static_cast<unsigned>(context.target_app));
        std::fflush(context.journal);
    }
    if (observed < 0 || value > UINT32_MAX)
        return -1;
    return static_cast<std::uint32_t>(value) == static_cast<std::uint32_t>(context.target_app) ? 1 : 0;
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
    case shelf::HandoffResult::foreground_observed:
        return shelf::LaunchStage::foreground_observed;
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
                     "ui-15 standard SDK helper; init: 0x%08x; user query: 0x%08x\n",
#else
                     "ui-15 restricted helper; init: 0x%08x; user query: 0x%08x\n",
#endif
                     static_cast<unsigned>(initialized), static_cast<unsigned>(user_status));
        std::fflush(context.journal);
    }
    if (initialized < 0 || user_status != 0 || context.user > 0x7fffffffu) {
        status(request, shelf::LaunchStage::service_unavailable, user_status ? user_status : initialized);
        return 1;
    }
    const auto result = shelf::cooperative_handoff(request, {&context, current, ready, wait, launch, now, consent, focused});
    status(request, result_stage(result),
           result == shelf::HandoffResult::foreground_changed ? context.probe_status : context.launch_status);
    return result == shelf::HandoffResult::foreground_observed ? 0 : 1;
}
