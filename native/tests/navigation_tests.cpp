// SPDX-License-Identifier: GPL-3.0-only
#include "shelf/navigation.hpp"
#include "shelf/presentation.hpp"
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
    CloseGesture close;
    Presentation view;
    close.press(7, 100);
    close.update(2099, view);
    check(view.hold > .49f && view.hold < .51f && !close.closing(), "Circle shows actual hold progress");
    check(!close.release(7, 2100), "an interrupted long hold cancels without navigating back");
    close.update(2200, view);
    check(view.hold == 0 && !close.finished(5000), "releasing before four seconds does not close");
    close.press(7, 6000);
    check(close.release(7, 6100), "a short Circle tap retains normal Back behaviour");
    close.press(7, 7000);
    check(!close.release(8, 7100), "another controller cannot end a hold");
    close.update(10999, view);
    check(!close.closing(), "four-second threshold is not rounded down");
    close.update(11000, view);
    check(close.closing() && !close.finished(11000), "four seconds starts a separate closing animation");
    close.update(11180, view);
    check(view.close > .49f && view.close < .51f, "close transition has an intermediate frame");
    check(close.finished(11360), "close waits until the fade completes");
    std::puts("PASS: four-second Circle hold, cancellation, ordinary Back taps and timed closing fade.");
}
