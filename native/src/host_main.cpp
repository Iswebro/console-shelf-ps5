// SPDX-License-Identifier: GPL-3.0-only
// Shared native event loop. The host build never deploys or launches PS5 apps.
#define SDL_MAIN_HANDLED
#include "shelf/filesystem.hpp"
#include "shelf/ftp.hpp"
#include "shelf/graphics.hpp"
#include "shelf/artwork.hpp"
#include "shelf/launch_client.hpp"
#include "shelf/memory.hpp"
#include "shelf/navigation.hpp"
#if !defined(SHELF_PS5) || defined(SHELF_UI_TEST_BRIDGE)
#define SHELF_HAS_UI_TEST_BRIDGE
#include "shelf/ui_test_bridge.hpp"
#endif
#include <SDL.h>
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <new>
#ifdef SHELF_PS5
#include "platform/platform.h"
#include "shelf/memory.hpp"
#include "shelf_launch_asset.hpp"
#include <time.h>
#endif
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif
namespace {
void startup(const char* stage, const char* detail = nullptr) {
#ifdef SHELF_PS5
    // Owned diagnostic file only: no account values or game paths are logged.
    if (FILE* file = std::fopen("/data/console-shelf/startup.log", "ab")) {
        std::fprintf(file, "%s%s%s\n", stage, detail ? ": " : "", detail ? detail : "");
        std::fflush(file);
        std::fclose(file);
    }
#else
    (void)stage;
    (void)detail;
#endif
}
struct LibraryScan {
    shelf::ReadOnlyFs source;
    shelf::Library* library = nullptr;
    const char* data_root = nullptr;
    SDL_atomic_t cancelled{0}, done{0};
    SDL_Thread* thread = nullptr;
    static bool stopping(void* raw) {
        return SDL_AtomicGet(&static_cast<LibraryScan*>(raw)->cancelled) != 0;
    }
    static shelf::FsStatus list(void* raw, const char* path, shelf::EntryVisitor visit, void* destination) {
        auto& scan = *static_cast<LibraryScan*>(raw);
        return stopping(raw) ? shelf::FsStatus::failed
                             : scan.source.list(scan.source.context, path, visit, destination);
    }
    static shelf::FsStatus stat(void* raw, const char* path, shelf::FileStat& result) {
        auto& scan = *static_cast<LibraryScan*>(raw);
        return stopping(raw) ? shelf::FsStatus::failed : scan.source.stat(scan.source.context, path, result);
    }
    static shelf::FsStatus read(void* raw, const char* path, std::uint64_t offset, void* out, std::size_t cap,
                                std::size_t& used) {
        auto& scan = *static_cast<LibraryScan*>(raw);
        return stopping(raw) ? shelf::FsStatus::failed
                             : scan.source.read(scan.source.context, path, offset, out, cap, used);
    }
    static int run(void* raw) {
        auto& scan = *static_cast<LibraryScan*>(raw);
        shelf::discover_library({raw, list, stat, read, scan.source.cached}, *scan.library);
        shelf::cache_artwork({raw, list, stat, read, scan.source.cached}, *scan.library, scan.data_root, stopping, raw);
        SDL_AtomicSet(&scan.done, 1);
        return 0;
    }
    bool start() {
        thread = SDL_CreateThread(run, "Shelf library", this);
        return thread != nullptr;
    }
    bool ready() {
        return thread && SDL_AtomicGet(&done) != 0;
    }
    void stop() {
        SDL_AtomicSet(&cancelled, 1);
        if (thread) {
            SDL_WaitThread(thread, nullptr);
            thread = nullptr;
        }
    }
    ~LibraryScan() {
        stop();
    }
};
void accept_library(shelf::UiState& state, shelf::Library& library, shelf::ReadOnlyFtp& ftp, bool scan_only) {
    state.library = &library;
    state.inventory = library.inventory;
    startup(library.scan.complete ? "library scan complete" : "library scan partial");
    char info[180];
    std::snprintf(info, sizeof(info), "%zu games; %zu routes; %zu roots read; %zu unavailable", library.game_count,
                  library.route_count, library.scan.roots_read, library.scan.roots_denied);
    startup("library counts", info);
    const auto& diag = ftp.diagnostics;
    std::snprintf(info, sizeof(info), "%zu connections; %zu listings; %zu reads; first failure %s (%d)",
                  diag.connections, diag.listings, diag.reads, diag.first_failure, diag.error);
    startup("library connection", info);
    if (scan_only)
        std::printf("FTP diagnostics: %s\n", info);
    std::printf("Read-only library: %zu games, %zu emulator/core routes; scan %s.\n", library.game_count,
                library.route_count, library.scan.complete ? "complete" : "partial");
    std::snprintf(info, sizeof(info), "%zu sources; %zu cached; %zu missing; %zu unreadable", library.artwork.found,
                  library.artwork.cached, library.artwork.missing, library.artwork.failed);
    startup("library artwork", info);
    std::printf("Library artwork: %s.\n", info);
    for (std::size_t i = 0; i < shelf::system_count; ++i)
        if (state.inventory[i].game_count || state.inventory[i].emulator_count)
            std::printf("  %.*s: %zu games, %zu routes\n", static_cast<int>(shelf::catalog[i].id.size()),
                        shelf::catalog[i].id.data(), state.inventory[i].game_count, state.inventory[i].emulator_count);
    shelf::clear_ftp_cache(ftp);
    const auto shown = shelf::shown_systems(state);
    state.focus = std::min(state.focus, std::max(0, static_cast<int>(shown.count) - 1));
    if (shown.count)
        state.selected_system = shown.indices[static_cast<std::size_t>(state.focus)];
    if (!library.scan.complete && !state.notice[0])
        std::snprintf(state.notice, sizeof(state.notice),
                      "Library scan incomplete. Keep ftpsrv running and reopen isShelf to try again.");
}
#ifdef SHELF_PS5
std::uint64_t monotonic_ms() {
    struct timespec time{};
    return clock_gettime(CLOCK_MONOTONIC, &time) == 0
               ? static_cast<std::uint64_t>(time.tv_sec) * 1000 + static_cast<std::uint64_t>(time.tv_nsec) / 1000000
               : 0;
}
struct PlatformExit {
    ~PlatformExit() {
        const auto begin = monotonic_ms();
        startup("display shutdown starting");
        gs_vout_shutdown();
        char elapsed[80];
        std::snprintf(elapsed, sizeof(elapsed), "%llu ms", static_cast<unsigned long long>(monotonic_ms() - begin));
        startup("display shutdown complete", elapsed);
        startup("shell close requesting");
        const int status = gs_request_close();
        char message[48];
        std::snprintf(message, sizeof(message), "0x%08x", static_cast<unsigned>(status));
        startup("shell close requested", message);
        gs_wait_for_close();
    }
};
#endif
bool directory(const char* path) {
#ifdef _WIN32
    return _mkdir(path) == 0 || errno == EEXIST;
#else
    return mkdir(path, 0700) == 0 || errno == EEXIST;
#endif
}
bool filename(const char* value) {
    if (!value[0] || std::strstr(value, ".."))
        return false;
    for (const char* at = value; *at; ++at)
        if (!((*at >= 'a' && *at <= 'z') || (*at >= 'A' && *at <= 'Z') || (*at >= '0' && *at <= '9') || *at == '.' ||
              *at == '_' || *at == '-'))
            return false;
    return true;
}
bool report(const char* path, const shelf::UiState& state) {
    FILE* file = std::fopen(path, "wb");
    if (!file)
        return false;
    const auto& p = state.settings.preferences;
    const auto shown = shelf::shown_systems(state);
    std::fprintf(
        file, "{\"page\":%d,\"tab\":%d,\"focus\":%d,\"selected\":\"%.*s\",\"shown\":%zu,\"layout\":\"%s\",\"hidden\":[",
        static_cast<int>(state.page), static_cast<int>(state.tab), state.settings_focus,
        static_cast<int>(shelf::catalog[state.selected_system].id.size()),
        shelf::catalog[state.selected_system].id.data(), shown.count,
        p.layout == shelf::Layout::grid ? "grid" : p.layout == shelf::Layout::swipe ? "swipe" : "shelf");
    bool comma = false;
    for (std::size_t i = 0; i < shelf::system_count; ++i)
        if (p.hidden[i]) {
            std::fprintf(file, "%s\"%.*s\"", comma ? "," : "", static_cast<int>(shelf::catalog[i].id.size()),
                         shelf::catalog[i].id.data());
            comma = true;
        }
    std::fprintf(file,
                 "],\"queryLength\":%zu,\"pickerQueryLength\":%zu,\"launchConnected\":%s,\"libraryGames\":%zu,"
                 "\"libraryRoutes\":%zu,\"artworkCached\":%zu,\"artworkMissing\":%zu,\"artworkFailed\":%zu,\"monochromeMissing\":%s,\"shelfSingle\":%s,\"reducedMotion\":%s,"
                 "\"shelfPage\":%d,\"shelfSelected\":%zu,\"tvMode\":%d,\"tvTextLength\":%zu,\"favoriteGames\":%zu}\n",
                 std::strlen(state.query), std::strlen(state.console_query), state.launch_connected ? "true" : "false",
                 state.library ? state.library->game_count : 0, state.library ? state.library->route_count : 0,
                 state.library ? state.library->artwork.cached : 0, state.library ? state.library->artwork.missing : 0,
                 state.library ? state.library->artwork.failed : 0,
                 p.monochrome_missing ? "true" : "false", p.shelf_single ? "true" : "false",
                 p.reduced_motion ? "true" : "false", state.focus / 8,
                 static_cast<std::size_t>(std::count(p.shelf_selected.begin(), p.shelf_selected.end(), true)),
                 static_cast<int>(p.tv_mode), std::strlen(state.settings.tv_text),
                 static_cast<std::size_t>(std::count_if(state.settings.game_favorites.begin(), state.settings.game_favorites.end(),
                                                       [](std::uint64_t key) { return key != 0; })));
    return std::fclose(file) == 0;
}
bool mapped_button(Uint8 button, const shelf::UiState& state, shelf::Action& action) {
    using shelf::Action;
    switch (button) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
        action = Action::up;
        break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        action = Action::down;
        break;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
        action = Action::left;
        break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
        action = Action::right;
        break;
    case SDL_CONTROLLER_BUTTON_A:
        action = Action::confirm;
        break;
    case SDL_CONTROLLER_BUTTON_B:
        action = Action::back;
        break;
    case SDL_CONTROLLER_BUTTON_X:
        if (state.page != shelf::Page::keyboard && state.page != shelf::Page::home)
            return false;
        action = state.page == shelf::Page::keyboard ? Action::erase : Action::layout;
        break;
    case SDL_CONTROLLER_BUTTON_Y:
        action = Action::favorite;
        break;
    case SDL_CONTROLLER_BUTTON_START:
        action = Action::settings;
        break;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
        action = Action::previous_tab;
        break;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
        action = Action::next_tab;
        break;
    default:
        return false;
    }
    return true;
}
bool script_event(const char* token) {
    if (std::strncmp(token, "wait:", 5) == 0) {
        char* end = nullptr;
        const auto delay = std::strtoul(token + 5, &end, 10);
        if (!token[5] || *end || delay > 2000)
            return false;
        SDL_Delay(static_cast<Uint32>(delay));
        return true;
    }
    if (std::strcmp(token, "loading") == 0 || std::strcmp(token, "reveal") == 0 || std::strcmp(token, "prepare") == 0 ||
        std::strcmp(token, "launch-art") == 0)
        return true;
    if (std::strcmp(token, "circle-down") == 0 || std::strcmp(token, "circle-up") == 0) {
        SDL_Event event{};
        event.type = std::strcmp(token, "circle-down") == 0 ? SDL_CONTROLLERBUTTONDOWN : SDL_CONTROLLERBUTTONUP;
        event.cbutton.button = SDL_CONTROLLER_BUTTON_B;
        SDL_PushEvent(&event);
        return true;
    }
    if (std::strncmp(token, "shot:", 5) == 0 || std::strncmp(token, "report:", 7) == 0)
        return true;
    if (std::strncmp(token, "frame:", 6) == 0) {
        char* end = nullptr;
        const auto value = std::strtoul(token + 6, &end, 10);
        return token[6] && *end == 0 && value <= 2000;
    }
    const char* names[] = {"up",     "down",     "left",     "right",        "confirm", "back",
                           "layout", "favorite", "settings", "previous-tab", "tab"};
    const Uint8 buttons[] = {SDL_CONTROLLER_BUTTON_DPAD_UP,
                             SDL_CONTROLLER_BUTTON_DPAD_DOWN,
                             SDL_CONTROLLER_BUTTON_DPAD_LEFT,
                             SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
                             SDL_CONTROLLER_BUTTON_A,
                             SDL_CONTROLLER_BUTTON_B,
                             SDL_CONTROLLER_BUTTON_X,
                             SDL_CONTROLLER_BUTTON_Y,
                             SDL_CONTROLLER_BUTTON_START,
                             SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
                             SDL_CONTROLLER_BUTTON_RIGHTSHOULDER};
    for (int i = 0; i < 11; ++i)
        if (std::strcmp(token, names[i]) == 0) {
            SDL_Event event{};
            event.type = SDL_CONTROLLERBUTTONDOWN;
            event.cbutton.button = buttons[i];
            SDL_PushEvent(&event);
            event.type = SDL_CONTROLLERBUTTONUP;
            SDL_PushEvent(&event);
            return true;
        }
    if (std::strncmp(token, "text:", 5) == 0) {
        const char* value = token + 5;
        while (*value) {
            SDL_Event event{};
            event.type = SDL_TEXTINPUT;
            auto length = std::min<std::size_t>(std::strlen(value), sizeof(event.text.text) - 1);
            while (length && (static_cast<unsigned char>(value[length]) & 0xc0) == 0x80)
                --length;
            if (!length)
                return false;
            std::memcpy(event.text.text, value, length);
            SDL_PushEvent(&event);
            value += length;
        }
        return true;
    }
    SDL_Event event{};
    event.type = SDL_KEYDOWN;
    if (std::strcmp(token, "consoles") == 0)
        event.key.keysym.sym = SDLK_F3;
    else if (std::strcmp(token, "filters") == 0)
        event.key.keysym.sym = SDLK_F2;
    else if (std::strcmp(token, "search") == 0)
        event.key.keysym.sym = SDLK_F4;
    else if (std::strcmp(token, "erase") == 0)
        event.key.keysym.sym = SDLK_BACKSPACE;
    else
        return false;
    SDL_PushEvent(&event);
    return true;
}
} // namespace

int main(int argc, char** argv) {
#ifdef SHELF_PS5
    PlatformExit platform_exit;
    startup("main entered", "ui-25 emulator artwork cache; dedicated PSSwanStation launch integration");
    char missing[1024];
    if (gs_missing_imports(missing, sizeof(missing)) != 0) {
        startup("missing imports", missing);
        char message[1200];
        std::snprintf(message, sizeof(message), "isShelf cannot start: missing console functions: %s", missing);
        std::fprintf(stderr, "%s\n", message);
        gs_notify(message);
        return 1;
    }
    startup("imports ready");
    if (SDL_SetMemoryFunctions(gs_malloc, gs_calloc, gs_realloc, gs_free) != 0) {
        startup("SDL allocator rejected", SDL_GetError());
        gs_notify("isShelf could not initialise its memory allocator.");
        return 1;
    }
    startup("SDL allocator ready");
    const char* assets = "/app0";
    const char* data = "/data/console-shelf";
#else
    const char* assets = nullptr;
    const char* data = nullptr;
#endif
    const char* captures = nullptr;
    const char* script_arg = nullptr;
    bool headless = false;
    bool animate_script = false;
    const char* library_host = nullptr;
    const char* library_root = nullptr;
    unsigned short library_port = 2121;
    bool scan_only = false;
    bool background_requested = false;
#ifdef SHELF_HAS_UI_TEST_BRIDGE
    bool ui_test_requested = false;
#endif
#ifndef SHELF_PS5
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--asset-root") == 0 && i + 1 < argc)
            assets = argv[++i];
        else if (std::strcmp(argv[i], "--data-dir") == 0 && i + 1 < argc)
            data = argv[++i];
        else if (std::strcmp(argv[i], "--capture-dir") == 0 && i + 1 < argc)
            captures = argv[++i];
        else if (std::strcmp(argv[i], "--script") == 0 && i + 1 < argc)
            script_arg = argv[++i];
        else if (std::strcmp(argv[i], "--headless") == 0)
            headless = true;
        else if (std::strcmp(argv[i], "--animate-script") == 0)
            animate_script = true;
        else if (std::strcmp(argv[i], "--library-ftp") == 0 && i + 1 < argc)
            library_host = argv[++i];
        else if (std::strcmp(argv[i], "--library-root") == 0 && i + 1 < argc)
            library_root = argv[++i];
        else if (std::strcmp(argv[i], "--library-port") == 0 && i + 1 < argc) {
            char* end = nullptr;
            const auto port = std::strtoul(argv[++i], &end, 10);
            if (!*argv[i] || *end || port == 0 || port > 65535)
                return 1;
            library_port = static_cast<unsigned short>(port);
        } else if (std::strcmp(argv[i], "--scan-only") == 0)
            scan_only = true;
        else if (std::strcmp(argv[i], "--background-scan") == 0)
            background_requested = true;
        else if (std::strcmp(argv[i], "--ui-test") == 0)
            ui_test_requested = true;
        else {
            std::fprintf(stderr, "Unknown or incomplete host option: %s\n", argv[i]);
            return 1;
        }
    }
#else
    // Console storage paths are owned constants, never supplied by launch arguments.
    (void)argc;
    (void)argv;
    library_host = "127.0.0.1";
#endif
#ifdef SHELF_HAS_UI_TEST_BRIDGE
    if (ui_test_requested && (script_arg || scan_only)) {
        std::fprintf(stderr, "UI test control requires the interactive event loop.\n");
        return 1;
    }
#endif
    if (library_host && library_root) {
        std::fprintf(stderr, "Choose one explicit library source.\n");
        return 1;
    }
    if (!assets || !data || std::strlen(assets) > 2000 || std::strlen(data) > 2000 ||
        (script_arg && std::strlen(script_arg) > 8191)) {
        std::fprintf(stderr, "Use --asset-root and --data-dir; optional --headless --script and --capture-dir.\n");
        return 1;
    }
    if (!directory(data)) {
        char error[64];
        std::snprintf(error, sizeof(error), "errno=%d", errno);
        startup("owned directory unavailable", error);
        std::fprintf(stderr, "Cannot create the requested owned data directory.\n");
#ifdef SHELF_PS5
        gs_notify("isShelf cannot access its own settings folder: /data/console-shelf.");
#endif
        return 1;
    }
    if (!captures)
        captures = data;
    if (std::strlen(captures) > 2000 || !directory(captures)) {
        startup("capture directory unavailable");
        std::fprintf(stderr, "Cannot create capture directory.\n");
        return 1;
    }
    startup("owned data ready");
    char settings_path[2304];
    std::snprintf(settings_path, sizeof(settings_path), "%s/settings.ini", data);
    shelf::UiState state;
#ifdef SHELF_PS5
    state.launch_connected = true;
#endif
    struct LibraryOwner {
        shelf::Library* value = nullptr;
        ~LibraryOwner() {
            if (value) {
                value->~Library();
                shelf::release(value);
            }
        }
    } library;
    shelf::ReadOnlyFtp ftp;
    shelf::ReadOnlyTree tree;
    LibraryScan scan;
    const bool background_scan = !scan_only && (background_requested || (!headless && !script_arg));
    if (library_host || library_root) {
        const bool ready = library_host ? shelf::init_read_only_ftp(ftp, library_host, library_port)
                                        : shelf::init_read_only_tree(tree, library_root);
        if (!ready) {
            startup("library source init failed");
            std::fprintf(stderr, "Library source could not initialise.\n");
            return 1;
        }
        void* memory = shelf::allocate(sizeof(shelf::Library));
        if (!memory) {
            std::fprintf(stderr, "Library memory allocation failed.\n");
            return 1;
        }
        library.value = new (memory) shelf::Library{};
        scan.source = library_host ? shelf::ftp_filesystem(ftp) : shelf::tree_filesystem(tree);
        scan.library = library.value;
        scan.data_root = data;
        ftp.cancelled = LibraryScan::stopping;
        ftp.cancel_context = &scan;
        if (!background_scan) {
            startup("library scan starting");
            LibraryScan::run(&scan);
            accept_library(state, *library.value, ftp, scan_only);
            if (scan_only)
                return library.value->scan.complete ? 0 : 2;
        } else
            state.presentation.loading = true;
    } else if (scan_only)
        return 1;
    const auto loaded = shelf::load_settings(settings_path, state.settings);
    if (loaded == shelf::SettingsLoad::backup)
        std::snprintf(state.notice, sizeof(state.notice), "Settings recovered from the last valid backup.");
    if (loaded == shelf::SettingsLoad::invalid)
        std::snprintf(state.notice, sizeof(state.notice),
                      "Settings could not be read. Defaults are in use; the original file is kept.");
    if (state.library && !state.library->scan.complete && !state.notice[0])
        std::snprintf(state.notice, sizeof(state.notice),
                      "Library scan incomplete. Keep ftpsrv running and reopen isShelf to try again.");
    SDL_SetMainReady();
#ifdef SHELF_PS5
    startup("video starting");
    SDL_SetHint(SDL_HINT_ENABLE_SCREEN_KEYBOARD, "0");
    const char* video_error = gs_vout_init();
    if (video_error) {
        startup("video failed", video_error);
        gs_notify(video_error);
        return 1;
    }
    startup("video ready");
#else
    if (headless)
        SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
#endif
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    startup("SDL starting");
    if (SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS
#ifndef SHELF_PS5
                 | SDL_INIT_VIDEO
#endif
                 ) < 0) {
        startup("SDL failed", SDL_GetError());
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    startup("SDL ready");
    SDL_Window* window = nullptr;
#ifndef SHELF_PS5
    window = headless
                 ? nullptr
                 : SDL_CreateWindow("isShelf - native host build", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                    1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!headless && !window) {
        std::fprintf(stderr, "Window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
#endif
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_ABGR8888);
    SDL_Renderer* renderer = surface ? SDL_CreateSoftwareRenderer(surface) : nullptr;
    startup("graphics starting");
    if (!renderer || !shelf::graphics_init(renderer, assets, data)) {
        startup("graphics failed", SDL_GetError());
        std::fprintf(stderr, "Native renderer or packaged fonts could not load.\n");
        if (renderer)
            SDL_DestroyRenderer(renderer);
        if (surface)
            SDL_FreeSurface(surface);
        if (window)
            SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    startup("graphics ready");
    std::array<SDL_GameController*, 8> controllers{};
    if (!headless)
        for (int i = 0; i < SDL_NumJoysticks() && i < 8; ++i)
            if (SDL_IsGameController(i))
                controllers[static_cast<std::size_t>(i)] = SDL_GameControllerOpen(i);
    SDL_StartTextInput();
    char script[8192]{};
    if (script_arg)
        std::strcpy(script, script_arg);
    else if (headless
#ifdef SHELF_HAS_UI_TEST_BRIDGE
             && !ui_test_requested
#endif
    )
        std::strcpy(script, "shot:home.png");
    char* next = script[0] ? script : nullptr;
    bool scripting = next != nullptr;
    int result = 0;
    shelf::NavigationInput navigation;
    shelf::CloseGesture close_gesture;
    shelf::CloseHint close_hint;
    shelf::LaunchClient launcher;
    bool redraw = true;
    bool first_frame = true;
    bool scan_started = false;
    const auto opened_at = scripting ? 0u : SDL_GetTicks();
    const bool automatic_intro = !scripting && state.presentation.loading;
    bool library_accepted = !background_scan || !scan.library, opening_prepared = !automatic_intro;
    shelf::OpeningGate opening_gate;
    bool intro_started = !automatic_intro, previous_visible = false;
    bool visibility_sampled = false, frame_acknowledged = false, opening_logged = false;
    int last_visibility_status = 0;
    unsigned last_visibility_flags = 0, visibility_notes = 0;
    unsigned visible_time = 0, last_visibility_at = opened_at;
    unsigned intro_frames = 0, reveal_frames = 0;
    unsigned circle_at = 0;
    bool circle_tracked = false, circle_committed_logged = false;
#if defined(SHELF_PS5) && defined(SHELF_CAPTURE_STARTUP)
    bool captured_loading = false, captured_reveal = false;
#endif
    unsigned reveal_at = 0;
    bool reveal_pending = false;
    unsigned measured_frames = 0, max_render_ms = 0, max_present_ms = 0;
    unsigned script_time = 0;
    unsigned tv_redraw_at = 0;
#ifdef SHELF_HAS_UI_TEST_BRIDGE
    shelf::UiTestBridge ui_test;
    shelf::UiTestRequest ui_test_request;
    std::uint64_t ui_revision = 1;
    unsigned ui_test_started = 0, ui_test_polled = 0, ui_test_capture_at = 0;
    bool ui_test_pending = false, ui_test_applied = false, ui_test_saved = true;
    bool ui_test_finish = false;
    const char* ui_test_result = "ready";
    SDL_JoystickID ui_test_controller = -1;
    bool ui_test_left = false, ui_test_right = false, ui_test_chord = false;
    auto local_ui_changed = [&]() {
        ++ui_revision;
        if (ui_test_pending && ui_test_request.sequence)
            ui_test_result = "interrupted";
    };
    auto arm_ui_test = [&]() {
        char session[33];
        std::snprintf(session, sizeof(session), "%016llx%016llx",
                      static_cast<unsigned long long>(SDL_GetPerformanceCounter()),
                      static_cast<unsigned long long>(std::time(nullptr)));
        if (!ui_test.start(data, session)) {
            std::snprintf(state.notice, sizeof(state.notice), "UI test control could not open its own folder.");
            redraw = true;
            return;
        }
        ui_test_started = SDL_GetTicks();
        ui_test_polled = ui_test_started;
        ui_test_request = {};
        std::strcpy(ui_test_request.name, "snapshot");
        ui_test_pending = true;
        ui_test_applied = false;
        ui_test_saved = true;
        ui_test_finish = false;
        ui_test_result = "ready";
        ui_test_capture_at = ui_test_started + 500;
        ++ui_revision;
        std::snprintf(state.notice, sizeof(state.notice), "UI test control enabled for 10 minutes. L3 + R3 disables it.");
        startup("ISSHELF_PRIVATE_UI_TEST_BRIDGE_V1", "owner enabled; menu actions only");
        redraw = true;
    };
    if (ui_test_requested)
        arm_ui_test();
#ifdef SHELF_UI_TEST_BRIDGE
    std::snprintf(state.notice, sizeof(state.notice), "Private UI test build: click L3 + R3 to enable menu control.");
    // This retained marker is independently checked before release packaging.
    startup("ISSHELF_PRIVATE_UI_TEST_BRIDGE_V1", "disabled until owner clicks L3 + R3");
#endif
#endif
    while (!state.exit_requested) {
        char* token = nullptr;
        if (scripting) {
            if (!next)
                break;
            token = next;
            char* comma = std::strchr(next, ',');
            if (comma) {
                *comma = 0;
                next = comma + 1;
            } else
                next = nullptr;
            if (!script_event(token)) {
                std::fprintf(stderr, "Unknown scripted action.\n");
                result = 1;
                break;
            }
            if (std::strncmp(token, "frame:", 6) == 0)
                script_time += static_cast<unsigned>(std::strtoul(token + 6, nullptr, 10));
            if (std::strcmp(token, "loading") == 0)
                state.presentation.loading = true;
            if (std::strcmp(token, "launch-art") == 0)
                state.presentation.artwork = true;
            if (std::strcmp(token, "reveal") == 0) {
                state.presentation.loading = false;
                state.presentation.reveal = 0;
                reveal_pending = true;
            }
            redraw = true;
        }
        const unsigned now = scripting ? script_time : SDL_GetTicks();
        if (automatic_intro && (state.presentation.loading || state.presentation.reveal < 1)) {
            unsigned flags = 0;
            int visibility_status = 0;
            bool rendered_frame = !first_frame;
#ifdef SHELF_PS5
            visibility_status = gs_app_visibility(&flags);
            rendered_frame = gs_vout_has_frame() != 0;
#endif
            opening_gate.sample(now, visibility_status, flags);
            if (visibility_notes < 8 && (!visibility_sampled || visibility_status != last_visibility_status ||
                                         flags != last_visibility_flags)) {
                char info[120];
                std::snprintf(info, sizeof(info), "%u ms; result 0x%08x; overlay %u; background %u", now - opened_at,
                              static_cast<unsigned>(visibility_status), flags & 1, (flags >> 1) & 1);
                startup("opening visibility", info);
                ++visibility_notes;
            }
            visibility_sampled = true;
            last_visibility_status = visibility_status;
            last_visibility_flags = flags;
            if (rendered_frame && !frame_acknowledged) {
                frame_acknowledged = true;
                char info[64];
                std::snprintf(info, sizeof(info), "%u ms after graphics ready", now - opened_at);
                startup("first rendered flip acknowledged", info);
            }
            const bool visible = rendered_frame && opening_prepared && opening_gate.ready(now);
            if (visible && previous_visible)
                visible_time += now - last_visibility_at;
            previous_visible = visible;
            last_visibility_at = now;
            if (visible && !intro_started) {
                intro_started = true;
                char info[120];
                std::snprintf(info, sizeof(info), "%u ms; status %s", now - opened_at,
                              opening_gate.used_fallback() ? "unavailable; timed fallback" : "foreground; no overlay");
                startup("opening entrance started", info);
            }
        }
        const unsigned visual_now = automatic_intro ? visible_time : now;
        const unsigned visual_start = automatic_intro ? 0 : opened_at;
        if (scan.ready()) {
            scan.stop();
            accept_library(state, *library.value, ftp, false);
#ifdef SHELF_HAS_UI_TEST_BRIDGE
            local_ui_changed();
#endif
            library_accepted = true;
            redraw = true;
        }
        if (state.presentation.loading && background_scan && scan.library && library_accepted && opening_prepared &&
            intro_started && visual_now - visual_start >= 1100) {
            state.presentation.loading = false;
            state.presentation.reveal = 0;
            reveal_pending = true;
            redraw = true;
        }
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
#ifdef SHELF_HAS_UI_TEST_BRIDGE
            if (event.type == SDL_KEYDOWN || event.type == SDL_TEXTINPUT ||
                event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONUP)
                local_ui_changed();
            if ((event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONUP) &&
                (event.cbutton.button == SDL_CONTROLLER_BUTTON_LEFTSTICK ||
                 event.cbutton.button == SDL_CONTROLLER_BUTTON_RIGHTSTICK)) {
                if (!ui_test_left && !ui_test_right)
                    ui_test_controller = event.cbutton.which;
                if (ui_test_controller == event.cbutton.which) {
                    const bool down = event.type == SDL_CONTROLLERBUTTONDOWN;
                    (event.cbutton.button == SDL_CONTROLLER_BUTTON_LEFTSTICK ? ui_test_left : ui_test_right) = down;
                    if (ui_test_left && ui_test_right && !ui_test_chord && !state.presentation.loading &&
                        !state.launch_busy && !close_gesture.active()) {
                        if (ui_test.active()) {
                            ui_test.stop();
                            ui_test_pending = true;
                            ui_test_result = "owner-stopped";
                            ui_test_capture_at = now;
                            std::snprintf(state.notice, sizeof(state.notice), "UI test control disabled.");
                            redraw = true;
                        } else
                            arm_ui_test();
                        ui_test_chord = true;
                    }
                    if (!ui_test_left && !ui_test_right)
                        ui_test_chord = false;
                }
                continue;
            }
#endif
            shelf::Action action = shelf::Action::up;
            bool mapped = false;
            if (event.type == SDL_QUIT)
                state.exit_requested = true;
            else if (event.type == SDL_TEXTINPUT) {
                shelf::text_input(state, event.text.text);
                redraw = true;
            } else if (event.type == SDL_WINDOWEVENT) {
#ifdef SHELF_HAS_UI_TEST_BRIDGE
                if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
                    local_ui_changed();
#endif
                if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
                    navigation.cancel();
                if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
                    close_gesture.cancel();
                if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
                    close_hint.dismiss();
                redraw = true;
            } else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_B) {
                    close_hint.dismiss();
                    state.launch_requested = false;
                    if (state.launch_busy) {
                        launcher.cancel();
                        state.launch_busy = false;
                        std::snprintf(state.notice, sizeof(state.notice), "Launch cancelled.");
                    }
                    if (!close_gesture.active()) {
                        circle_at = now;
                        circle_tracked = true;
                        startup("Circle hold received", "3000 ms threshold");
                    }
                    close_gesture.press(event.cbutton.which, now);
                    navigation.cancel();
                    redraw = true;
                } else
                    mapped = mapped_button(event.cbutton.button, state, action);
                if (mapped && shelf::NavigationInput::directional(action))
                    mapped = navigation.button(action, true, event.cbutton.which, SDL_GetTicks(), action);
            } else if (event.type == SDL_CONTROLLERBUTTONUP) {
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_B) {
                    mapped = close_gesture.release(event.cbutton.which, now);
                    if (circle_tracked) {
                        char duration[80];
                        std::snprintf(duration, sizeof(duration), "%u ms; %s", now - circle_at,
                                      close_gesture.closing()  ? "committed"
                                      : close_gesture.active() ? "other controller"
                                                               : "released early");
                        startup("Circle released", duration);
                        if (!close_gesture.active())
                            circle_tracked = false;
                    }
                    action = shelf::Action::back;
                    redraw = true;
                } else if (mapped_button(event.cbutton.button, state, action) &&
                           shelf::NavigationInput::directional(action))
                    navigation.button(action, false, event.cbutton.which, SDL_GetTicks(), action);
            } else if (event.type == SDL_CONTROLLERDEVICEADDED && !headless) {
                for (auto& controller : controllers)
                    if (!controller && SDL_IsGameController(event.cdevice.which)) {
                        controller = SDL_GameControllerOpen(event.cdevice.which);
                        break;
                    }
            } else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
#ifdef SHELF_HAS_UI_TEST_BRIDGE
                if (ui_test_controller == event.cdevice.which) {
                    ui_test_left = ui_test_right = ui_test_chord = false;
                    ui_test_controller = -1;
                    local_ui_changed();
                }
#endif
                navigation.disconnected(event.cdevice.which);
                close_gesture.cancel(event.cdevice.which);
                for (auto& controller : controllers)
                    if (controller &&
                        SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller)) == event.cdevice.which) {
                        SDL_GameControllerClose(controller);
                        controller = nullptr;
                    }
            } else if (event.type == SDL_CONTROLLERAXISMOTION) {
                if (event.caxis.axis != SDL_CONTROLLER_AXIS_LEFTX && event.caxis.axis != SDL_CONTROLLER_AXIS_LEFTY)
                    continue;
                mapped = navigation.axis(event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX, event.caxis.value,
                                         event.caxis.which, SDL_GetTicks(), action);
#ifdef SHELF_HAS_UI_TEST_BRIDGE
                if (mapped)
                    local_ui_changed();
#endif
            } else if (event.type == SDL_KEYDOWN) {
                mapped = true;
                switch (event.key.keysym.sym) {
                case SDLK_UP:
                    action = shelf::Action::up;
                    break;
                case SDLK_DOWN:
                    action = shelf::Action::down;
                    break;
                case SDLK_LEFT:
                    action = shelf::Action::left;
                    break;
                case SDLK_RIGHT:
                    action = shelf::Action::right;
                    break;
                case SDLK_RETURN:
                    action = shelf::Action::confirm;
                    break;
                case SDLK_ESCAPE:
                    action = shelf::Action::back;
                    break;
                case SDLK_F1:
                    action = shelf::Action::settings;
                    break;
                case SDLK_F2:
                    action = shelf::Action::filters;
                    break;
                case SDLK_F3:
                    action = shelf::Action::consoles;
                    break;
                case SDLK_F4:
                    action = shelf::Action::search;
                    break;
                case SDLK_PAGEUP:
                    action = shelf::Action::previous_tab;
                    break;
                case SDLK_PAGEDOWN:
                    action = shelf::Action::next_tab;
                    break;
                case SDLK_TAB:
                    action =
                        (event.key.keysym.mod & KMOD_SHIFT) ? shelf::Action::previous_tab : shelf::Action::next_tab;
                    break;
                case SDLK_BACKSPACE:
                    action = shelf::Action::erase;
                    break;
                default:
                    mapped = false;
                }
            }
            if (mapped && !state.presentation.loading && !close_gesture.closing() &&
                (!state.launch_busy || action == shelf::Action::back)) {
                if (action != shelf::Action::back)
                    close_hint.dismiss();
                if (action == shelf::Action::back) {
                    state.launch_requested = false;
                    if (state.launch_busy) {
                        launcher.cancel();
                        state.launch_busy = false;
                    }
                }
                if (!shelf::NavigationInput::directional(action))
                    navigation.cancel();
                shelf::input(state, action);
                redraw = true;
            }
        }
#ifdef SHELF_PS5
        if (state.launch_requested && !state.exit_requested && !close_gesture.active()) {
            state.launch_requested = false;
            auto& request = state.pending_launch;
            const auto epoch = std::time(nullptr);
            request.issued = epoch > 0 ? static_cast<std::uint64_t>(epoch) : 0;
            std::snprintf(request.id, sizeof(request.id), "%016llx%016llx",
                          static_cast<unsigned long long>(SDL_GetPerformanceCounter()),
                          static_cast<unsigned long long>(request.issued));
            if (launcher.start(request, data, shelf_launch_payload, sizeof(shelf_launch_payload))) {
                state.launch_busy = true;
                navigation.cancel();
                startup("launcher request started", shelf::launch_mode_name(request.mode));
            } else {
                std::snprintf(state.notice, sizeof(state.notice), "%s", launcher.error());
                startup("launcher request refused");
            }
            redraw = true;
        }
        if (state.launch_busy) {
            const auto status = launcher.state();
            if (state.exit_requested || close_gesture.active()) {
                launcher.cancel();
                state.launch_busy = false;
            } else if (status == shelf::LaunchClientState::ready) {
                unsigned visibility = 0;
                const bool foreground = gs_app_visibility(&visibility) == 0 && visibility == 0;
                if (foreground && launcher.commit()) {
                    state.presentation.launching = true;
                    circle_at = now;
                    close_gesture.begin_close(now);
                    startup("launcher committed", "cooperative close; no process kill");
                } else {
                    launcher.cancel();
                    std::snprintf(state.notice, sizeof(state.notice), "%s",
                                  foreground ? launcher.error()
                                             : "Launch cancelled while isShelf was not in the foreground.");
                }
                state.launch_busy = false;
                redraw = true;
            } else if (status == shelf::LaunchClientState::failed || status == shelf::LaunchClientState::cancelled) {
                std::snprintf(state.notice, sizeof(state.notice), "%s",
                              status == shelf::LaunchClientState::failed ? launcher.error() : "Launch cancelled.");
                state.launch_busy = false;
                redraw = true;
                startup("launcher did not commit");
            }
        }
#endif
        if (state.close_hint_requested) {
            state.close_hint_requested = false;
            if (!close_gesture.active())
                close_hint.show(now);
        }
        if (close_hint.update(now, state.presentation))
            redraw = true;
        close_gesture.update(now, state.presentation);
        if (close_gesture.closing()) {
            if (!circle_committed_logged) {
                char duration[80];
                std::snprintf(duration, sizeof(duration), "%u ms after button event", now - circle_at);
                startup(state.presentation.launching ? "handoff fade started" : "Circle hold committed", duration);
                circle_committed_logged = true;
            }
            navigation.cancel();
            SDL_AtomicSet(&scan.cancelled, 1);
        }
        if (close_gesture.finished(now)) {
            char duration[80];
            std::snprintf(duration, sizeof(duration), "%u ms after button event", now - circle_at);
            startup(state.presentation.launching ? "handoff fade complete" : "closing fade complete", duration);
            state.exit_requested = true;
        }
        if (state.presentation.loading) {
            const float progress = intro_started ? std::min(1.0f, (visual_now - visual_start) / 600.0f) : 0;
            state.presentation.appear = progress * progress * (3 - 2 * progress);
        }
        if (!state.presentation.loading && state.presentation.reveal < 1 && !reveal_pending) {
            const float progress = std::min(1.0f, (visual_now - reveal_at) / 1000.0f);
            state.presentation.reveal = progress * progress * (3 - 2 * progress);
        }
        shelf::Action repeated{};
        if (!scripting && !state.presentation.loading && !close_gesture.closing() && navigation.tick(now, repeated)) {
            shelf::input(state, repeated);
#ifdef SHELF_HAS_UI_TEST_BRIDGE
            local_ui_changed();
#endif
            redraw = true;
        }
#ifdef SHELF_HAS_UI_TEST_BRIDGE
        if (ui_test.active() && static_cast<unsigned>(now - ui_test_started) >= 600000) {
            ui_test.stop();
            ui_test_pending = true;
            ui_test_result = "expired";
            ui_test_capture_at = now;
            std::snprintf(state.notice, sizeof(state.notice), "UI test control expired. Click L3 + R3 to enable again.");
            redraw = true;
        }
        if (ui_test.active() && !ui_test_pending && static_cast<unsigned>(now - ui_test_polled) >= 250) {
            ui_test_polled = now;
            if (ui_test.poll(ui_test_request)) {
                ui_test_pending = true;
                ui_test_applied = false;
                ui_test_saved = true;
                ui_test_finish = false;
                ui_test_result = "ok";
                ui_test_capture_at = now + 500;
                if (ui_test_request.operation == shelf::UiTestOperation::stop) {
                    ui_test_finish = true;
                    ui_test_result = "stopped";
                } else if (ui_test_request.revision != ui_revision)
                    ui_test_result = "stale";
                else if (state.presentation.loading || state.launch_busy || close_gesture.active() ||
                         state.exit_requested || state.presentation.reveal < 1)
                    ui_test_result = "busy";
                else if (ui_test_request.operation == shelf::UiTestOperation::action) {
                    // Trial state prevents a remote Confirm from leaving isShelf.
                    if (!shelf::apply_ui_test_action(state, ui_test_request.action))
                        ui_test_result = "launch-blocked";
                    else {
                        navigation.cancel();
                        close_hint.dismiss();
                        ++ui_revision;
                        ui_test_applied = true;
                    }
                }
                redraw = true;
            }
        }
        if (ui_test_pending && SDL_TICKS_PASSED(now, ui_test_capture_at))
            redraw = true;
        state.ui_test_active = ui_test.active() && !ui_test_finish;
#endif
        if (state.changed) {
            if (!shelf::save_settings(settings_path, state.settings)) {
                std::snprintf(state.notice, sizeof(state.notice),
                              "Settings could not be saved. Your previous settings are kept.");
                result = 1;
#ifdef SHELF_HAS_UI_TEST_BRIDGE
                local_ui_changed();
                if (ui_test_pending) {
                    ui_test_saved = false;
                    ui_test_result = "save-failed";
                }
#endif
            }
            state.changed = false;
        }
        if (!scripting && state.settings.preferences.layout == shelf::Layout::shelf &&
            (state.settings.preferences.tv_mode == shelf::TvMode::clock || state.settings.preferences.tv_mode == shelf::TvMode::favorites) &&
            static_cast<unsigned>(now - tv_redraw_at) >= 1000) {
            redraw = true;
            tv_redraw_at = now;
        }
        if (redraw || shelf::graphics_animating() || state.presentation.loading || state.presentation.reveal < 1 ||
            close_gesture.active()) {
            const auto frame_start = SDL_GetTicks();
            const bool preparing =
                (token && std::strcmp(token, "prepare") == 0) ||
                (automatic_intro && state.presentation.loading && library_accepted && !opening_prepared);
            state.presentation.preparing = preparing;
            const bool covered_reveal = reveal_pending && !state.presentation.loading;
            shelf::graphics_render(state, animate_script ? script_time : SDL_GetTicks(),
                                   !preparing && (animate_script || (!scripting && !headless)));
            const auto render_end = SDL_GetTicks();
            if (shelf::graphics_errors()) {
                startup("render resource failed");
                std::fprintf(stderr, "A packaged image/font or rendering resource failed to load.\n");
                result = 1;
                break;
            }
            if (window) {
                SDL_Surface* target = SDL_GetWindowSurface(window);
                if (target) {
                    SDL_BlitScaled(surface, nullptr, target, nullptr);
                    SDL_UpdateWindowSurface(window);
                }
            }
#ifdef SHELF_PS5
            gs_vout_present(surface->pixels, surface->pitch);
#ifdef SHELF_CAPTURE_STARTUP
            // Two bounded, owned diagnostic pictures let us distinguish correct
            // drawing from a delayed display handoff. No game/account files are read.
            bool captured = false;
            if (!captured_loading && state.presentation.loading && intro_started && state.presentation.appear >= 1) {
                captured_loading = captured = true;
                startup(shelf::graphics_capture("/data/console-shelf/diagnostic-loading.png")
                            ? "loading diagnostic picture saved"
                            : "loading diagnostic picture failed");
            }
            if (!captured_reveal && !state.presentation.loading && state.presentation.reveal >= .4f &&
                state.presentation.reveal < .8f) {
                captured_reveal = captured = true;
                startup(shelf::graphics_capture("/data/console-shelf/diagnostic-reveal.png")
                            ? "reveal diagnostic picture saved"
                            : "reveal diagnostic picture failed");
            }
            if (captured)
                last_visibility_at = SDL_GetTicks();
#endif
#endif
#ifdef SHELF_HAS_UI_TEST_BRIDGE
            if (ui_test_pending && SDL_TICKS_PASSED(SDL_GetTicks(), ui_test_capture_at)) {
                const bool captured = shelf::graphics_capture(ui_test.frame_path());
                if (ui_test_finish)
                    ui_test.stop();
                const bool published = ui_test.publish(ui_test_request, ui_revision, ui_test_result,
                                                       ui_test_applied, ui_test_saved, state, captured);
                ui_test_pending = false;
                ui_test_applied = false;
                if (!published)
                    ui_test.stop();
            }
#endif
            if (preparing) {
                state.presentation.preparing = false;
                opening_prepared = true;
                last_visibility_at = SDL_GetTicks();
                char duration[100];
                std::snprintf(duration, sizeof(duration), "%u ms render; %u ms after graphics ready",
                              render_end - frame_start, last_visibility_at - opened_at);
                startup("covered scene prepared", duration);
            }
            // Bounded timing evidence, not a claim about displayed frame rate.
            if (measured_frames < 16) {
                if (measured_frames) {
                    max_render_ms = std::max(max_render_ms, render_end - frame_start);
                    max_present_ms = std::max(max_present_ms, SDL_GetTicks() - render_end);
                }
                if (++measured_frames == 16) {
                    char timing[100];
                    std::snprintf(timing, sizeof(timing), "max render %u ms; frame handoff %u ms (15 samples)",
                                  max_render_ms, max_present_ms);
                    startup("frame costs", timing);
                }
            }
            if (first_frame) {
                startup("first frame submitted");
                first_frame = false;
            }
            if (state.presentation.loading && intro_started)
                ++intro_frames;
            if (!state.presentation.loading && state.presentation.reveal < 1)
                ++reveal_frames;
            if (automatic_intro && !opening_logged && !state.presentation.loading && state.presentation.reveal >= 1) {
                char info[150];
                std::snprintf(info, sizeof(info),
                              "%u ms wall; %u ms foreground clock; %u entrance frames; %u reveal frames",
                              SDL_GetTicks() - opened_at, visible_time, intro_frames, reveal_frames);
                startup("opening animation submitted", info);
                opening_logged = true;
            }
            if (state.presentation.loading && scan.library && !scan_started) {
                scan_started = true;
                startup("library scan starting", "background after first submitted frame");
                if (!scan.start()) {
                    startup("library worker unavailable", SDL_GetError());
                    library.value->scan.complete = false;
                    accept_library(state, *library.value, ftp, false);
                    library_accepted = true;
                }
            }
            if (covered_reveal) {
                // Image decoding and texture preparation must not consume the reveal's time.
                reveal_at = automatic_intro ? visible_time : scripting ? script_time : SDL_GetTicks();
                // Exclude this covered frame's image decoding from the foreground animation clock too.
                last_visibility_at = SDL_GetTicks();
                reveal_pending = false;
                startup("opening reveal ready", "prepared frame; 1000 ms blend");
            }
            redraw = false;
        }
        if (token && (std::strncmp(token, "shot:", 5) == 0 || std::strncmp(token, "report:", 7) == 0)) {
            const bool shot = std::strncmp(token, "shot:", 5) == 0;
            const char* name = token + (shot ? 5 : 7);
            if (!filename(name)) {
                std::fprintf(stderr, "Invalid capture filename.\n");
                result = 1;
                break;
            }
            char output[2304];
            std::snprintf(output, sizeof(output), "%s/%s", captures, name);
            if (!(shot ? shelf::graphics_capture(output) : report(output, state))) {
                std::fprintf(stderr, "Capture/report failed.\n");
                result = 1;
                break;
            }
        }
        if (!scripting)
            SDL_Delay(16);
    }
#ifdef SHELF_HAS_UI_TEST_BRIDGE
    if (ui_test.active() || ui_test_pending) {
        ui_test.stop();
        ui_test.publish(ui_test_request, ui_revision, "closed", ui_test_applied, ui_test_saved, state, false);
    }
#endif
    scan.stop();
    launcher.stop();
    for (auto* controller : controllers)
        if (controller)
            SDL_GameControllerClose(controller);
    shelf::graphics_shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(surface);
    if (window)
        SDL_DestroyWindow(window);
    SDL_Quit();
    std::printf("Native host session finished: %zu systems in view.\n", shelf::shown_systems(state).count);
    return result;
}
