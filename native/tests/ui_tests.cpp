// SPDX-License-Identifier: GPL-3.0-only
#include "shelf/carousel.hpp"
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
    UiState spatial;
    for (int i = 0; i < 4; ++i)
        input(spatial, Action::right);
    check(spatial.focus == 3, "grid right stops at its row edge instead of jumping down a row");
    input(spatial, Action::up);
    check(spatial.tools && spatial.tool == 1, "up from the last column reaches the layout action");
    input(spatial, Action::confirm);
    check(spatial.page == Page::home && spatial.settings.preferences.layout == Layout::swipe,
          "layout button switches view directly without opening settings");
    input(spatial, Action::up);
    check(spatial.tools && spatial.tool == 2, "Up reaches the settings button above the toolbar");
    input(spatial, Action::right);
    check(spatial.tool == 2, "horizontal input does not move vertically from the settings row");
    input(spatial, Action::down);
    check(spatial.tools && spatial.tool == 1, "Down from settings returns to the layout button below it");
    input(spatial, Action::left);
    check(spatial.tool == 0, "search and layout remain neighbours on the same row");
    input(spatial, Action::up);
    input(spatial, Action::confirm);
    check(spatial.page == Page::settings, "settings is opened through its spatial Up route");
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
    UiState carousel;
    carousel.settings.preferences.layout = Layout::swipe;
    input(carousel, Action::left);
    check(carousel.focus == static_cast<int>(system_count) - 1, "Swipe loops left from the first console");
    input(carousel, Action::right);
    check(carousel.focus == 0, "Swipe loops right from the last console");
    input(carousel, Action::up);
    check(carousel.tools, "up from any Swipe position reaches the toolbar");
    const auto center = orbit_pose(0, 1760), near = orbit_pose(1, 1760), far = orbit_pose(2, 1760);
    check(center.scale > near.scale && near.scale > far.scale && center.z > near.z && near.z > far.z,
          "carousel selection dominates a progressively receding collection");
    check(center.x == 0 && near.x > 0 && orbit_pose(-1, 1760).x == -near.x,
          "carousel has a centred and symmetrical arc");
    OrbitMotion motion;
    motion.select(static_cast<double>(system_count) - 1, system_count, 0, false);
    motion.update(100);
    check(motion.active && motion.position < 0 && motion.position > -1, "wrap animates by one slot");
    const double interrupted = motion.position;
    motion.select(1, system_count, 100, false);
    check(motion.position == interrupted, "reversing input does not jump the current frame");
    motion.update(520);
    check(!motion.active && std::abs(motion.position - 1) < .0001, "animation settles at its most recent target");
    motion.select(0, system_count, 600, true);
    check(!motion.active && std::abs(motion.position) < .0001, "reduced motion settles immediately");
    std::puts("PASS: native controller navigation, settings tabs, independent searches, group visibility, honest "
              "unavailable routes and page return.");
}
