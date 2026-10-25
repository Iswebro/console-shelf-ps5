// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "launch_plan.hpp"
#include "library.hpp"
#include "presentation.hpp"
#include "settings.hpp"

namespace shelf {
enum class Page { home, details, library, settings, keyboard, quit };
enum class SettingsTab { appearance, browse, consoles };
enum class Action {
    up,
    down,
    left,
    right,
    confirm,
    back,
    settings,
    filters,
    consoles,
    favorite,
    previous_tab,
    next_tab,
    search,
    erase,
    layout
};
inline constexpr std::string_view keyboard_keys = "QWERTYUIOPASDFGHJKL<ZXCVBNM0123456789_!^";
struct PickerEntry {
    bool group;
    std::size_t index;
};
struct PickerList {
    std::array<PickerEntry, system_count + brands.size()> entries{};
    std::size_t count = 0;
};
struct UiState {
    Presentation presentation;
    Settings settings;
    Inventory inventory{};
    const Library* library = nullptr;
    Page page = Page::home, return_page = Page::home, keyboard_return = Page::home;
    SettingsTab tab = SettingsTab::appearance;
    std::size_t selected_system = 0;
    int focus = 0, tool = 0, detail_focus = 1, settings_focus = 0, keyboard_focus = 0;
    int game_focus = 0;
    bool tools = false, keyboard_for_consoles = false, changed = false, exit_requested = false;
    bool launch_connected = false, launch_requested = false, launch_busy = false;
    LaunchRequest pending_launch;
    char query[128]{}, console_query[128]{}, notice[192]{};
};
BrowseResult shown_systems(const UiState& state);
PickerList console_picker(const UiState& state);
void input(UiState& state, Action action);
void text_input(UiState& state, const char* utf8);
} // namespace shelf
