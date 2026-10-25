// SPDX-License-Identifier: GPL-3.0-only
#include "shelf/ui.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace shelf {
namespace {
int clamp(int value, int maximum) {
    return std::max(0, std::min(value, maximum));
}
BrowseResult picker_matches(const UiState& state) {
    return browse(Preferences{}, Inventory{}, {state.console_query, {}});
}
void normalize_home(UiState& state) {
    const auto shown = shown_systems(state);
    if (!shown.count) {
        state.focus = 0;
        state.tools = true;
        state.tool = 2;
        return;
    }
    state.focus = clamp(state.focus, static_cast<int>(shown.count) - 1);
    if (state.page == Page::home || (state.page == Page::settings && state.return_page == Page::home) ||
        (state.page == Page::keyboard && state.keyboard_return == Page::home))
        state.selected_system = shown.indices[static_cast<std::size_t>(state.focus)];
}
void open_settings(UiState& state, SettingsTab tab) {
    if (state.page != Page::settings)
        state.return_page = state.page;
    state.page = Page::settings;
    state.tab = tab;
    state.settings_focus = 0;
}
void open_keyboard(UiState& state, bool consoles, bool tv = false) {
    state.keyboard_return = state.page;
    state.keyboard_for_consoles = consoles;
    state.keyboard_for_tv = tv;
    state.page = Page::keyboard;
    state.keyboard_focus = 0;
}
void changed(UiState& state) {
    state.changed = true;
    normalize_home(state);
}
bool picker_enabled(const Preferences& p, std::size_t id) {
    return p.shelf_single ? p.shelf_selected[id] : !p.hidden[id];
}
void picker_set(UiState& state, std::size_t id, bool enabled) {
    auto& p = state.settings.preferences;
    if (!p.shelf_single) {
        p.hidden[id] = !enabled;
        return;
    }
    if (enabled && !p.shelf_selected[id] &&
        static_cast<std::size_t>(std::count(p.shelf_selected.begin(), p.shelf_selected.end(), true)) >= shelf_capacity) {
        std::snprintf(state.notice, sizeof(state.notice), "All 8 spaces are filled. Remove a console before adding another.");
        return;
    }
    p.shelf_selected[id] = enabled;
}
void cycle(UiState& state, int direction) {
    auto& p = state.settings.preferences;
    if (state.tab == SettingsTab::appearance) {
        if (state.settings_focus == 0)
            p.layout = next_layout(p.layout, direction);
        if (state.settings_focus == 1 && p.layout == Layout::grid)
            p.grid_look = p.grid_look == GridLook::icons ? GridLook::cards : GridLook::icons;
        if (state.settings_focus == 2)
            p.icon = static_cast<AppIcon>((static_cast<int>(p.icon) + direction + 3) % 3);
        if (state.settings_focus == 3)
            p.monochrome_missing = !p.monochrome_missing;
        if (state.settings_focus == 4)
            p.reduced_motion = !p.reduced_motion;
        changed(state);
    } else if (state.tab == SettingsTab::browse) {
        if (state.settings_focus == 0)
            p.sort = static_cast<Sort>((static_cast<int>(p.sort) + direction + 8) % 8);
        if (state.settings_focus == 1)
            p.brand =
                static_cast<std::size_t>((static_cast<int>(p.brand) + direction + static_cast<int>(brands.size())) %
                                         static_cast<int>(brands.size()));
        if (state.settings_focus == 2)
            p.filter = static_cast<SystemFilter>((static_cast<int>(p.filter) + direction + 5) % 5);
        if (state.settings_focus == 3)
            p.shelf_single = !p.shelf_single;
        changed(state);
    } else if (state.tab == SettingsTab::room) {
        if (state.settings_focus == 0)
            p.tv_mode = static_cast<TvMode>((static_cast<int>(p.tv_mode) + direction + 7) % 7);
        if (state.settings_focus == 1) { open_keyboard(state, false, true); return; }
        if (state.settings_focus == 2)
            p.tv_period = static_cast<std::size_t>((static_cast<int>(p.tv_period) + direction + 4) % 4);
        if (state.settings_focus == 3) p.intro_all = !p.intro_all;
        changed(state);
    } else {
        if (state.settings_focus == 0) {
            p.scope = static_cast<Scope>((static_cast<int>(p.scope) + direction + 3) % 3);
            changed(state);
            return;
        }
        if (state.settings_focus == 1) {
            open_keyboard(state, true);
            return;
        }
        const auto matches = picker_matches(state);
        if (state.settings_focus == 2 || state.settings_focus == 3) {
            for (std::size_t i = 0; i < matches.count; ++i)
                picker_set(state, matches.indices[i], state.settings_focus == 2);
        } else {
            const auto entries = console_picker(state);
            const auto at = static_cast<std::size_t>(state.settings_focus - 4);
            if (at >= entries.count)
                return;
            const auto entry = entries.entries[at];
            if (entry.group) {
                bool all = true;
                for (std::size_t i = 0; i < matches.count; ++i)
                    if (catalog[matches.indices[i]].brand == brands[entry.index] && !picker_enabled(p, matches.indices[i]))
                        all = false;
                for (std::size_t i = 0; i < matches.count; ++i)
                    if (catalog[matches.indices[i]].brand == brands[entry.index])
                        picker_set(state, matches.indices[i], !all);
            } else
                picker_set(state, entry.index, !picker_enabled(p, entry.index));
        }
        changed(state);
    }
}
void erase_query(UiState& state) {
    char* query = state.keyboard_for_tv ? state.settings.tv_text : state.keyboard_for_consoles ? state.console_query : state.query;
    auto size = std::strlen(query);
    if (size) {
        --size;
        while (size && (static_cast<unsigned char>(query[size]) & 0xc0) == 0x80)
            --size;
        query[size] = 0;
    }
    if (state.keyboard_for_tv) state.changed = true;
    else normalize_home(state);
}
} // namespace

BrowseResult shown_systems(const UiState& state) {
    return browse(state.settings.preferences, state.inventory, {state.query, {}});
}
PickerList console_picker(const UiState& state) {
    PickerList list;
    const auto matches = picker_matches(state);
    for (std::size_t brand = 1; brand < brands.size(); ++brand) {
        bool added = false;
        for (std::size_t i = 0; i < matches.count; ++i) {
            const auto index = matches.indices[i];
            if (catalog[index].brand != brands[brand])
                continue;
            if (!added) {
                list.entries[list.count++] = {true, brand};
                added = true;
            }
            list.entries[list.count++] = {false, index};
        }
    }
    return list;
}

void text_input(UiState& state, const char* utf8) {
    if (state.page != Page::keyboard)
        return;
    char* query = state.keyboard_for_tv ? state.settings.tv_text : state.keyboard_for_consoles ? state.console_query : state.query;
    const auto size = std::strlen(query), extra = std::strlen(utf8);
    if (size + extra >= (state.keyboard_for_tv ? sizeof(state.settings.tv_text) : sizeof(state.query)))
        return;
    for (std::size_t i = 0; i < extra; ++i)
        if (static_cast<unsigned char>(utf8[i]) < 32)
            return;
    std::memcpy(query + size, utf8, extra + 1);
    if (state.keyboard_for_tv) state.changed = true;
    else normalize_home(state);
}

void input(UiState& state, Action action) {
    state.notice[0] = 0;
    state.close_hint_requested = false;
    if (state.page == Page::keyboard) {
        if (action == Action::back || action == Action::settings) {
            state.page = state.keyboard_return;
            return;
        }
        if (action == Action::up)
            state.keyboard_focus = clamp(state.keyboard_focus - 10, 39);
        if (action == Action::down)
            state.keyboard_focus = clamp(state.keyboard_focus + 10, 39);
        if (action == Action::left)
            state.keyboard_focus = clamp(state.keyboard_focus - 1, 39);
        if (action == Action::right)
            state.keyboard_focus = clamp(state.keyboard_focus + 1, 39);
        if (action == Action::erase)
            erase_query(state);
        if (action == Action::favorite) {
            (state.keyboard_for_tv ? state.settings.tv_text : state.keyboard_for_consoles ? state.console_query : state.query)[0] = 0;
            if (state.keyboard_for_tv) state.changed = true;
            else normalize_home(state);
        }
        if (action == Action::confirm) {
            const auto key = keyboard_keys[static_cast<std::size_t>(state.keyboard_focus)];
            if (key == '^')
                state.page = state.keyboard_return;
            else if (key == '<')
                erase_query(state);
            else if (key == '!') {
                (state.keyboard_for_tv ? state.settings.tv_text : state.keyboard_for_consoles ? state.console_query : state.query)[0] = 0;
                if (state.keyboard_for_tv) state.changed = true;
                else normalize_home(state);
            } else {
                const char text[] = {key == '_' ? ' ' : key, 0};
                text_input(state, text);
            }
        }
        return;
    }
    if (action == Action::settings) {
        if (state.page == Page::settings)
            state.page = state.return_page;
        else
            open_settings(state, state.tab);
        return;
    }
    if (action == Action::layout && state.page == Page::home) {
        auto& layout = state.settings.preferences.layout;
        layout = next_layout(layout);
        changed(state);
        return;
    }
    if (action == Action::filters) {
        open_settings(state, SettingsTab::browse);
        return;
    }
    if (action == Action::consoles) {
        open_settings(state, SettingsTab::consoles);
        return;
    }
    if (state.page == Page::settings) {
        if (action == Action::back) {
            state.page = state.return_page;
            return;
        }
        if (action == Action::previous_tab || action == Action::next_tab) {
            state.tab =
                static_cast<SettingsTab>((static_cast<int>(state.tab) + (action == Action::next_tab ? 1 : -1) + 4) % 4);
            state.settings_focus = 0;
            return;
        }
        const int max_focus =
            state.tab == SettingsTab::consoles ? 3 + static_cast<int>(console_picker(state).count) :
            state.tab == SettingsTab::appearance ? 4 : 3;
        if (action == Action::up)
            state.settings_focus = clamp(state.settings_focus - 1, max_focus);
        if (action == Action::down)
            state.settings_focus = clamp(state.settings_focus + 1, max_focus);
        if (action == Action::left || action == Action::right || action == Action::confirm)
            cycle(state, action == Action::left ? -1 : 1);
        return;
    }
    if (state.page == Page::details) {
        if (action == Action::back) {
            state.page = Page::home;
            return;
        }
        if (state.tools) {
            if (action == Action::down) {
                state.tools = false;
                state.detail_focus = 0;
            }
            if (action == Action::confirm)
                input(state, Action::settings);
            return;
        }
        if (action == Action::up && state.detail_focus == 0) {
            state.tools = true;
            state.tool = 2;
            return;
        }
        if (state.detail_focus == 0 && state.library &&
            (action == Action::left || action == Action::right || action == Action::confirm)) {
            const auto* selected = preferred_route(*state.library, state.selected_system,
                                                   state.settings.preferred_emulator[state.selected_system].data());
            std::array<std::size_t, max_routes> routes{};
            int count = 0, current = 0;
            for (std::size_t i = 0; i < state.library->route_count; ++i)
                if (state.library->routes[i].system == state.selected_system) {
                    if (&state.library->routes[i] == selected)
                        current = count;
                    routes[static_cast<std::size_t>(count++)] = i;
                }
            if (count) {
                current = (current + count + (action == Action::left ? -1 : 1)) % count;
                std::snprintf(state.settings.preferred_emulator[state.selected_system].data(), 48, "%s",
                              state.library->routes[routes[static_cast<std::size_t>(current)]].id);
                state.changed = true;
            }
            return;
        }
        if (action == Action::up || action == Action::left)
            state.detail_focus = clamp(state.detail_focus - 1, 2);
        if (action == Action::down || action == Action::right)
            state.detail_focus = clamp(state.detail_focus + 1, 2);
        if (action == Action::favorite) {
            state.settings.preferences.favorite[state.selected_system] =
                !state.settings.preferences.favorite[state.selected_system];
            state.changed = true;
        }
        if (action == Action::confirm) {
            if (state.detail_focus == 2) {
                state.page = Page::library;
                state.game_focus = 0;
            } else if (state.detail_focus == 1 && state.launch_connected && state.library &&
                       plan_emulator(*state.library, state.selected_system,
                                     state.settings.preferred_emulator[state.selected_system].data(),
                                     state.pending_launch) != LaunchPlan::unavailable) {
                state.launch_requested = true;
                std::snprintf(state.notice, sizeof(state.notice), "Preparing the emulator...");
            } else
                std::snprintf(state.notice, sizeof(state.notice), "%s",
                              state.inventory[state.selected_system].emulator_count
                                  ? "Launch adapter is not connected in this development build."
                                  : "No compatible emulator has been detected yet.");
        }
        return;
    }
    if (state.page == Page::library) {
        if (action == Action::back)
            state.page = Page::details;
        if (state.tools) {
            if (action == Action::down) {
                state.tools = false;
                state.game_focus = 0;
            }
            if (action == Action::confirm)
                input(state, Action::settings);
            return;
        }
        if (action == Action::up && state.game_focus == 0) {
            state.tools = true;
            state.tool = 2;
            return;
        }
        if (state.library) {
            const auto games = games_for_system(*state.library, state.selected_system);
            const int maximum = games.count ? static_cast<int>(games.count) - 1 : 0;
            if (action == Action::up)
                state.game_focus = clamp(state.game_focus - 1, maximum);
            if (action == Action::down)
                state.game_focus = clamp(state.game_focus + 1, maximum);
            if (action == Action::left)
                state.game_focus = clamp(state.game_focus - 8, maximum);
            if (action == Action::right)
                state.game_focus = clamp(state.game_focus + 8, maximum);
            if (action == Action::favorite && games.count) {
                const auto key = game_favorite_key(state.library->games[games.indices[static_cast<std::size_t>(state.game_focus)]]);
                auto& favorites = state.settings.game_favorites;
                auto slot = std::find(favorites.begin(), favorites.end(), key);
                if (slot != favorites.end()) *slot = 0;
                else {
                    slot = std::find(favorites.begin(), favorites.end(), 0);
                    if (slot != favorites.end()) *slot = key;
                    else std::snprintf(state.notice, sizeof(state.notice), "Your 64 favourite game spaces are full.");
                }
                state.changed = true;
            }
            if (action == Action::confirm && games.count) {
                const auto plan =
                    state.launch_connected
                        ? plan_game(*state.library, games.indices[static_cast<std::size_t>(state.game_focus)],
                                    state.settings.preferred_emulator[state.selected_system].data(),
                                    state.pending_launch)
                        : LaunchPlan::unavailable;
                if (plan != LaunchPlan::unavailable) {
                    state.launch_requested = true;
                    std::snprintf(state.notice, sizeof(state.notice), "%s",
                                  plan == LaunchPlan::menu
                                      ? "This emulator opens its own library. Choose the game there."
                                      : "Preparing your game...");
                } else
                    std::snprintf(state.notice, sizeof(state.notice),
                                  "A direct launch route is not available for this game yet.");
            }
        }
        return;
    }
    if (action == Action::back) {
        state.close_hint_requested = true;
        return;
    }
    if (action == Action::search) {
        open_keyboard(state, false);
        return;
    }
    auto shown = shown_systems(state);
    if (state.settings.preferences.layout == Layout::shelf && shown.count &&
        (action == Action::previous_tab || action == Action::next_tab)) {
        const int size = static_cast<int>(shelf_capacity);
        const int last_page = (static_cast<int>(shown.count) - 1) / size;
        const int page = state.focus / size;
        const int next = clamp(page + (action == Action::next_tab ? 1 : -1), last_page);
        if (next != page) {
            state.focus = std::min(next * size + state.focus % size, static_cast<int>(shown.count) - 1);
            state.tools = false;
            normalize_home(state);
        }
        return;
    }
    if (state.tools || !shown.count) {
        state.tools = true;
        if (action == Action::up)
            state.tool = 2;
        if (action == Action::left && state.tool == 1)
            state.tool = 0;
        if (action == Action::right && state.tool == 0)
            state.tool = 1;
        if (action == Action::down && state.tool == 2)
            state.tool = 1;
        else if (action == Action::down && shown.count) {
            state.tools = false;
            normalize_home(state);
        }
        if (action == Action::confirm) {
            if (state.tool == 0)
                open_keyboard(state, false);
            else if (state.tool == 1)
                input(state, Action::layout);
            else
                open_settings(state, !shown.count ? SettingsTab::consoles : state.tab);
        }
        return;
    }
    const int stride = state.settings.preferences.layout == Layout::swipe ? 1 : 4;
    const bool swipe = state.settings.preferences.layout == Layout::swipe;
    if (action == Action::up && (swipe || state.focus < stride)) {
        state.tools = true;
        state.tool = swipe || state.focus < 3 ? 0 : 1;
    } else if (action == Action::up)
        state.focus -= stride;
    if (action == Action::down && !swipe)
        state.focus = clamp(state.focus + stride, static_cast<int>(shown.count) - 1);
    if (action == Action::left)
        state.focus = swipe ? (state.focus + static_cast<int>(shown.count) - 1) % static_cast<int>(shown.count)
                            : std::max(state.focus - state.focus % stride, state.focus - 1);
    if (action == Action::right)
        state.focus = swipe ? (state.focus + 1) % static_cast<int>(shown.count)
                            : std::min({state.focus + 1, state.focus - state.focus % stride + stride - 1,
                                        static_cast<int>(shown.count) - 1});
    normalize_home(state);
    if (action == Action::confirm) {
        state.page = Page::details;
        state.detail_focus = state.inventory[state.selected_system].built_in ? 2 : 1;
    }
    if (action == Action::favorite) {
        const auto selected = state.selected_system;
        state.settings.preferences.favorite[selected] = !state.settings.preferences.favorite[selected];
        state.changed = true;
        shown = shown_systems(state);
        for (std::size_t i = 0; i < shown.count; ++i)
            if (shown.indices[i] == selected)
                state.focus = static_cast<int>(i);
        normalize_home(state);
    }
}
} // namespace shelf
