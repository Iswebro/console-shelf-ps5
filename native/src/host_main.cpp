// SPDX-License-Identifier: GPL-3.0-only
// Shared native event loop. The host build never deploys or launches PS5 apps.
#define SDL_MAIN_HANDLED
#include "shelf/filesystem.hpp"
#include "shelf/ftp.hpp"
#include "shelf/graphics.hpp"
#include "shelf/memory.hpp"
#include <SDL.h>
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#ifdef SHELF_PS5
#include "platform/platform.h"
#include "shelf/memory.hpp"
#endif
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif
namespace {
#ifdef SHELF_PS5
struct PlatformExit {
    ~PlatformExit() {
        gs_vout_shutdown();
        if (sceSystemServiceLoadExec)
            sceSystemServiceLoadExec("exit", nullptr);
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
        p.layout == shelf::Layout::grid ? "grid" : "swipe");
    bool comma = false;
    for (std::size_t i = 0; i < shelf::system_count; ++i)
        if (p.hidden[i]) {
            std::fprintf(file, "%s\"%.*s\"", comma ? "," : "", static_cast<int>(shelf::catalog[i].id.size()),
                         shelf::catalog[i].id.data());
            comma = true;
        }
    std::fprintf(file,
                 "],\"queryLength\":%zu,\"pickerQueryLength\":%zu,\"launchConnected\":false,\"libraryGames\":%zu,"
                 "\"libraryRoutes\":%zu}\n",
                 std::strlen(state.query), std::strlen(state.console_query),
                 state.library ? state.library->game_count : 0, state.library ? state.library->route_count : 0);
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
        action = state.page == shelf::Page::keyboard ? Action::erase : Action::filters;
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
    if (std::strncmp(token, "shot:", 5) == 0 || std::strncmp(token, "report:", 7) == 0)
        return true;
    if (std::strncmp(token, "frame:", 6) == 0) {
        char* end = nullptr;
        const auto value = std::strtoul(token + 6, &end, 10);
        return token[6] && *end == 0 && value <= 2000;
    }
    const char* names[] = {"up",      "down",     "left",     "right",        "confirm", "back",
                           "filters", "favorite", "settings", "previous-tab", "tab"};
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
    char missing[1024];
    if (gs_missing_imports(missing, sizeof(missing)) != 0) {
        char message[1200];
        std::snprintf(message, sizeof(message), "Shelf cannot start: missing console functions: %s", missing);
        std::fprintf(stderr, "%s\n", message);
        gs_notify(message);
        return 1;
    }
    if (SDL_SetMemoryFunctions(gs_malloc, gs_calloc, gs_realloc, gs_free) != 0) {
        gs_notify("Shelf could not initialise its memory allocator.");
        return 1;
    }
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
        std::fprintf(stderr, "Cannot create the requested owned data directory.\n");
#ifdef SHELF_PS5
        gs_notify("Shelf cannot access its own settings folder: /data/console-shelf.");
#endif
        return 1;
    }
    if (!captures)
        captures = data;
    if (std::strlen(captures) > 2000 || !directory(captures)) {
        std::fprintf(stderr, "Cannot create capture directory.\n");
        return 1;
    }
    char settings_path[2304];
    std::snprintf(settings_path, sizeof(settings_path), "%s/settings.ini", data);
    shelf::UiState state;
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
    if (library_host || library_root) {
        const bool ready = library_host ? shelf::init_read_only_ftp(ftp, library_host, library_port)
                                        : shelf::init_read_only_tree(tree, library_root);
        if (!ready) {
            std::fprintf(stderr, "Library source could not initialise.\n");
            return 1;
        }
        void* memory = shelf::allocate(sizeof(shelf::Library));
        if (!memory) {
            std::fprintf(stderr, "Library memory allocation failed.\n");
            return 1;
        }
        library.value = new (memory) shelf::Library{};
        shelf::discover_library(library_host ? shelf::ftp_filesystem(ftp) : shelf::tree_filesystem(tree),
                                *library.value);
        state.library = library.value;
        state.inventory = library.value->inventory;
        std::printf("Read-only library: %zu games, %zu emulator/core routes; scan %s.\n", library.value->game_count,
                    library.value->route_count, library.value->scan.complete ? "complete" : "partial");
        for (std::size_t i = 0; i < shelf::system_count; ++i)
            if (state.inventory[i].game_count || state.inventory[i].emulator_count)
                std::printf("  %.*s: %zu games, %zu routes\n", static_cast<int>(shelf::catalog[i].id.size()),
                            shelf::catalog[i].id.data(), state.inventory[i].game_count,
                            state.inventory[i].emulator_count);
        shelf::clear_ftp_cache(ftp);
        if (scan_only)
            return library.value->scan.complete ? 0 : 2;
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
                      "Library scan incomplete. Keep ftpsrv running and reopen Shelf to try again.");
    SDL_SetMainReady();
#ifdef SHELF_PS5
    SDL_SetHint(SDL_HINT_ENABLE_SCREEN_KEYBOARD, "0");
    const char* video_error = gs_vout_init();
    if (video_error) {
        gs_notify(video_error);
        return 1;
    }
#else
    if (headless)
        SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
#endif
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    if (SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS
#ifndef SHELF_PS5
                 | SDL_INIT_VIDEO
#endif
                 ) < 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window* window = nullptr;
#ifndef SHELF_PS5
    window = headless
                 ? nullptr
                 : SDL_CreateWindow("Console Shelf - native host build", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                    1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!headless && !window) {
        std::fprintf(stderr, "Window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
#endif
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_ABGR8888);
    SDL_Renderer* renderer = surface ? SDL_CreateSoftwareRenderer(surface) : nullptr;
    if (!renderer || !shelf::graphics_init(renderer, assets)) {
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
    std::array<SDL_GameController*, 8> controllers{};
    if (!headless)
        for (int i = 0; i < SDL_NumJoysticks() && i < 8; ++i)
            if (SDL_IsGameController(i))
                controllers[static_cast<std::size_t>(i)] = SDL_GameControllerOpen(i);
    SDL_StartTextInput();
    char script[8192]{};
    if (script_arg)
        std::strcpy(script, script_arg);
    else if (headless)
        std::strcpy(script, "shot:home.png");
    char* next = script[0] ? script : nullptr;
    bool scripting = next != nullptr;
    int result = 0;
    Uint32 repeat_at = 0;
    shelf::Action held = shelf::Action::up;
    bool repeating = false;
    int held_kind = 0;
    Uint8 held_button = 0;
    bool redraw = true;
    int axis_x = 0, axis_y = 0;
    unsigned script_time = 0;
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
            redraw = true;
        }
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            shelf::Action action = shelf::Action::up;
            bool mapped = false;
            if (event.type == SDL_QUIT)
                state.exit_requested = true;
            else if (event.type == SDL_TEXTINPUT) {
                shelf::text_input(state, event.text.text);
                redraw = true;
            } else if (event.type == SDL_WINDOWEVENT)
                redraw = true;
            else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                mapped = mapped_button(event.cbutton.button, state, action);
                if (mapped && event.cbutton.button >= SDL_CONTROLLER_BUTTON_DPAD_UP &&
                    event.cbutton.button <= SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
                    held = action;
                    repeating = true;
                    held_kind = 1;
                    held_button = event.cbutton.button;
                    repeat_at = SDL_GetTicks() + 350;
                }
            } else if (event.type == SDL_CONTROLLERBUTTONUP) {
                if (held_kind == 1 && event.cbutton.button == held_button)
                    repeating = false;
            } else if (event.type == SDL_CONTROLLERDEVICEADDED && !headless) {
                for (auto& controller : controllers)
                    if (!controller && SDL_IsGameController(event.cdevice.which)) {
                        controller = SDL_GameControllerOpen(event.cdevice.which);
                        break;
                    }
            } else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
                repeating = false;
                for (auto& controller : controllers)
                    if (controller &&
                        SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller)) == event.cdevice.which) {
                        SDL_GameControllerClose(controller);
                        controller = nullptr;
                    }
            } else if (event.type == SDL_CONTROLLERAXISMOTION) {
                if (event.caxis.axis != SDL_CONTROLLER_AXIS_LEFTX && event.caxis.axis != SDL_CONTROLLER_AXIS_LEFTY)
                    continue;
                if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX)
                    axis_x = event.caxis.value;
                if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)
                    axis_y = event.caxis.value;
                if (std::abs(axis_x) > 16000 || std::abs(axis_y) > 16000) {
                    action = std::abs(axis_x) > std::abs(axis_y)
                                 ? (axis_x > 0 ? shelf::Action::right : shelf::Action::left)
                                 : (axis_y > 0 ? shelf::Action::down : shelf::Action::up);
                    if (!repeating || held != action || held_kind != 2) {
                        mapped = true;
                        held = action;
                        repeating = true;
                        held_kind = 2;
                        repeat_at = SDL_GetTicks() + 350;
                    }
                } else if (held_kind == 2)
                    repeating = false;
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
            if (mapped) {
                shelf::input(state, action);
                redraw = true;
            }
        }
        if (repeating && !scripting && SDL_TICKS_PASSED(SDL_GetTicks(), repeat_at)) {
            shelf::input(state, held);
            repeat_at = SDL_GetTicks() + 110;
            redraw = true;
        }
        if (state.changed) {
            if (!shelf::save_settings(settings_path, state.settings)) {
                std::snprintf(state.notice, sizeof(state.notice),
                              "Settings could not be saved. Your previous settings are kept.");
                result = 1;
            }
            state.changed = false;
        }
        if (redraw || shelf::graphics_animating()) {
            shelf::graphics_render(state, animate_script ? script_time : SDL_GetTicks(),
                                   animate_script || (!scripting && !headless));
            if (shelf::graphics_errors()) {
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
#endif
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
