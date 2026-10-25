// SPDX-License-Identifier: GPL-3.0-only
#include "shelf/navigation.hpp"
#include <cstdio>
#include <cstdlib>
namespace {
void check(bool value, const char* why) {
    if (!value) {
        std::fprintf(stderr, "FAIL navigation: %s\n", why);
        std::exit(1);
    }
}
} // namespace
void test_navigation() {
    using namespace shelf;
    NavigationInput nav;
    Action out{};
    check(!nav.axis(true, 3000, 7, 0, out), "resting stick drift does not move selection");
    check(nav.axis(true, 22000, 7, 0, out) && out == Action::right, "stick responds immediately");
    check(!nav.axis(false, 22500, 7, 10, out), "small diagonal wobble does not change direction");
    check(!nav.axis(true, 21000, 7, 20, out), "axis event frequency cannot accelerate navigation");
    check(nav.tick(280, out) && out == Action::right, "held gesture repeats after its initial pause");
    check(nav.axis(false, 32000, 7, 300, out) && out == Action::down, "deliberate stronger direction turns");
    nav.axis(true, 0, 7, 310, out);
    nav.axis(false, 0, 7, 311, out);
    check(!nav.tick(1000, out), "neutral stops repeat");
    check(nav.button(Action::left, true, 7, 1100, out), "D-pad tap responds immediately");
    nav.button(Action::left, false, 8, 1101, out);
    check(nav.tick(1380, out) && out == Action::left, "another controller cannot release a held button");
    check(!nav.axis(true, 32000, 7, 1390, out), "stick does not steal an active D-pad hold");
    nav.cancel();
    check(!nav.tick(2000, out) && !nav.axis(true, 31000, 7, 2001, out),
          "opening a page cancels repeat and requires the stick to return to neutral");
    nav.axis(true, 0, 7, 2002, out);
    check(nav.axis(true, -24000, 7, 2003, out) && out == Action::left, "a new gesture works after neutral");
    nav.disconnected(7);
    check(!nav.tick(3000, out), "disconnect stops a held gesture");
    nav.button(Action::down, true, 9, 0xffffff00u, out);
    check(!nav.tick(0x10u, out) && nav.tick(0x18u, out), "repeat timing handles the clock wrapping");
    nav.cancel();
    nav.button(Action::right, true, 9, 5000, out);
    check(nav.tick(9000, out) && !nav.tick(9000, out) && !nav.tick(9074, out) && nav.tick(9075, out),
          "a delayed frame emits one move and sustained holds accelerate without a backlog");
    std::puts(
        "PASS: physical gesture repeat, diagonal hysteresis, drift, page changes, device ownership and clock wrap.");
}
