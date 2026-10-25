// SPDX-License-Identifier: GPL-3.0-only
#include "shelf/ui.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace {
void check(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "FAIL UI: %s\n", message);
        std::exit(1);
    }
}
} // namespace
void test_ui() {
    using namespace shelf;
    UiState state;
    input(state, Action::right);
    check(state.selected_system == find_system("ps4"), "D-pad moves through console identities");
    input(state, Action::confirm);
    check(state.page == Page::details, "Cross opens a console");
    input(state, Action::confirm);
    check(std::strstr(state.notice, "No compatible emulator") != nullptr, "absent route is explained");
    input(state, Action::right);
    input(state, Action::confirm);
    check(state.page == Page::library, "library is a separate action");
    input(state, Action::back);
    input(state, Action::back);
    check(state.page == Page::home, "Circle returns through the page stack");
    input(state, Action::settings);
    input(state, Action::confirm);
    check(state.settings.preferences.layout == Layout::swipe && state.changed,
          "native layout control changes saved preferences");
    input(state, Action::next_tab);
    input(state, Action::next_tab);
    check(state.tab == SettingsTab::consoles, "shoulders change settings tabs");
    input(state, Action::down);
    input(state, Action::confirm);
    check(state.page == Page::keyboard, "controller search opens on-screen keyboard");
    text_input(state, "Nintendo");
    input(state, Action::settings);
    check(state.page == Page::settings, "Options completes search");
    check(console_picker(state).count == 17, "picker contains Nintendo header and sixteen consoles");
    input(state, Action::down);
    input(state, Action::down);
    input(state, Action::confirm);
    check(state.settings.preferences.hidden[find_system("n64")] &&
              !state.settings.preferences.hidden[find_system("ps2")],
          "Hide results only changes matched consoles");
    input(state, Action::up);
    input(state, Action::confirm);
    check(!state.settings.preferences.hidden[find_system("n64")], "Show results restores matched consoles");
    input(state, Action::back);
    input(state, Action::search);
    text_input(state, "PLAYSTATION");
    input(state, Action::back);
    check(shown_systems(state).count == 6, "main search is independent of console-picker search");
    input(state, Action::settings);
    input(state, Action::back);
    check(state.page == Page::home, "settings returns to the prior screen");
    input(state, Action::back);
    check(state.page == Page::quit && !state.exit_requested, "leaving home requires a separate confirmation");
    input(state, Action::back);
    check(state.page == Page::home, "quit can be cancelled");
    UiState detail_state;
    input(detail_state, Action::right);
    input(detail_state, Action::confirm);
    input(detail_state, Action::settings);
    input(detail_state, Action::confirm);
    input(detail_state, Action::back);
    check(detail_state.page == Page::details && detail_state.selected_system == find_system("ps4"),
          "global settings retain the console being viewed");
    std::puts("PASS: native controller navigation, settings tabs, independent searches, group visibility, honest "
              "unavailable routes and page return.");
}
