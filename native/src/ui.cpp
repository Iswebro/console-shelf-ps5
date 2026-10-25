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
void open_keyboard(UiState& state, bool consoles) {
    state.keyboard_return = state.page;
    state.keyboard_for_consoles = consoles;
    state.page = Page::keyboard;
    state.keyboard_focus = 0;
}
void changed(UiState& state) {
    state.changed = true;
    normalize_home(state);
}
void cycle(UiState& state, int direction) {
    auto& p = state.settings.preferences;
    if (state.tab == SettingsTab::appearance) {
        if (state.settings_focus == 0)
            p.layout = p.layout == Layout::grid ? Layout::swipe : Layout::grid;
        if (state.settings_focus == 1 && p.layout == Layout::grid)
            p.grid_look = p.grid_look == GridLook::icons ? GridLook::cards : GridLook::icons;
        if (state.settings_focus == 2)
            p.icon = static_cast<AppIcon>((static_cast<int>(p.icon) + direction + 3) % 3);
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
        changed(state);
    } else {
        if (state.settings_focus == 0) {
            p.scope = p.scope == Scope::all ? Scope::available : Scope::all;
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
                p.hidden[matches.indices[i]] = state.settings_focus == 3;
        } else {
            const auto entries = console_picker(state);
            const auto at = static_cast<std::size_t>(state.settings_focus - 4);
            if (at >= entries.count)
                return;
            const auto entry = entries.entries[at];
            if (entry.group) {
                bool all = true;
                for (std::size_t i = 0; i < matches.count; ++i)
                    if (catalog[matches.indices[i]].brand == brands[entry.index] && p.hidden[matches.indices[i]])
                        all = false;
                for (std::size_t i = 0; i < matches.count; ++i)
                    if (catalog[matches.indices[i]].brand == brands[entry.index])
                        p.hidden[matches.indices[i]] = all;
            } else
                p.hidden[entry.index] = !p.hidden[entry.index];
        }
        changed(state);
    }
}
void erase_query(UiState& state) {
    char* query = state.keyboard_for_consoles ? state.console_query : state.query;
    auto size = std::strlen(query);
    if (size) {
        --size;
        while (size && (static_cast<unsigned char>(query[size]) & 0xc0) == 0x80)
            --size;
        query[size] = 0;
    }
    normalize_home(state);
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
    char* query = state.keyboard_for_consoles ? state.console_query : state.query;
    const auto size = std::strlen(query), extra = std::strlen(utf8);
    if (size + extra >= 128)
        return;
    for (std::size_t i = 0; i < extra; ++i)
        if (static_cast<unsigned char>(utf8[i]) < 32)
            return;
    std::memcpy(query + size, utf8, extra + 1);
    normalize_home(state);
}

void input(UiState& state, Action action) {
    state.notice[0] = 0;
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
            (state.keyboard_for_consoles ? state.console_query : state.query)[0] = 0;
            normalize_home(state);
        }
        if (action == Action::confirm) {
            const auto key = keyboard_keys[static_cast<std::size_t>(state.keyboard_focus)];
            if (key == '^')
                state.page = state.keyboard_return;
            else if (key == '<')
                erase_query(state);
            else if (key == '!') {
                (state.keyboard_for_consoles ? state.console_query : state.query)[0] = 0;
                normalize_home(state);
            } else {
                const char text[] = {key == '_' ? ' ' : key, 0};
                text_input(state, text);
            }
        }
        return;
    }
    if (state.page == Page::quit) {
        if (action == Action::confirm)
            state.exit_requested = true;
        else if (action == Action::back)
            state.page = Page::home;
        return;
    }
    if (action == Action::settings) {
        if (state.page == Page::settings)
            state.page = state.return_page;
        else
            open_settings(state, SettingsTab::appearance);
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
                static_cast<SettingsTab>((static_cast<int>(state.tab) + (action == Action::next_tab ? 1 : -1) + 3) % 3);
            state.settings_focus = 0;
            return;
        }
        const int max_focus =
            state.tab == SettingsTab::consoles ? 3 + static_cast<int>(console_picker(state).count) : 2;
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
            if (state.detail_focus == 2)
                state.page = Page::library;
            else
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
        return;
    }
    if (action == Action::back) {
        state.page = Page::quit;
        return;
    }
    if (action == Action::search) {
        open_keyboard(state, false);
        return;
    }
    auto shown = shown_systems(state);
    if (state.tools || !shown.count) {
        state.tools = true;
        if (action == Action::left)
            state.tool = clamp(state.tool - 1, 3);
        if (action == Action::right)
            state.tool = clamp(state.tool + 1, 3);
        if (action == Action::down && shown.count) {
            state.tools = false;
            normalize_home(state);
        }
        if (action == Action::confirm) {
            if (state.tool == 0)
                open_keyboard(state, false);
            else
                open_settings(state, state.tool == 1   ? SettingsTab::browse
                                     : state.tool == 2 ? SettingsTab::consoles
                                                       : SettingsTab::appearance);
        }
        return;
    }
    const int stride = state.settings.preferences.layout == Layout::grid ? 4 : 1;
    if (action == Action::up && state.focus < stride) {
        state.tools = true;
        state.tool = 3;
    } else if (action == Action::up)
        state.focus -= stride;
    if (action == Action::down)
        state.focus = clamp(state.focus + stride, static_cast<int>(shown.count) - 1);
    if (action == Action::left)
        state.focus = clamp(state.focus - 1, static_cast<int>(shown.count) - 1);
    if (action == Action::right)
        state.focus = clamp(state.focus + 1, static_cast<int>(shown.count) - 1);
    normalize_home(state);
    if (action == Action::confirm) {
        state.page = Page::details;
        state.detail_focus = 1;
    }
    if (action == Action::favorite) {
        const auto selected = state.selected_system;
        state.settings.preferences.favorite[selected] = !state.settings.preferences.favorite[selected];
        state.changed = true;
        shown = shown_systems(state);
        for (std::size_t i = 0; i < shown.count; ++i)
            if (shown.indices[i] == selected)
                state.focus = static_cast<int>(i);
    }
}
} // namespace shelf
