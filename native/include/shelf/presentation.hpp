// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <cstdint>
namespace shelf {
inline constexpr std::uint32_t close_hold_ms = 3000;
struct Presentation {
    bool loading = false, preparing = false, artwork = false;
    bool launching = false;
    float reveal = 1, appear = 1, hold = 0, close = 0;
};
class OpeningGate {
    bool unknown = false, foreground = false, fallback = false;
    std::uint32_t unknown_at = 0, visible_at = 0;

  public:
    void sample(std::uint32_t now, int status, unsigned flags) {
        if (status == 0) {
            unknown = fallback = false;
            const bool next = flags == 0;
            if (next && !foreground)
                visible_at = now;
            foreground = next;
        } else {
            foreground = false;
            if (!unknown) {
                unknown = true;
                fallback = false;
                unknown_at = now;
            }
            if (!fallback && static_cast<std::uint32_t>(now - unknown_at) >= 5000) {
                fallback = true;
                visible_at = now;
            }
        }
    }
    bool ready(std::uint32_t now) const {
        return (foreground || fallback) && static_cast<std::uint32_t>(now - visible_at) >= 120;
    }
    std::uint32_t age(std::uint32_t now) const {
        return foreground || fallback ? static_cast<std::uint32_t>(now - visible_at) : 0;
    }
    bool used_fallback() const {
        return fallback;
    }
};
class CloseGesture {
    bool down = false, committed = false;
    std::int32_t owner = -1;
    std::uint32_t pressed_at = 0, committed_at = 0;

  public:
    void begin_close(std::uint32_t now) {
        if (!committed) {
            committed = true;
            committed_at = now;
            down = false;
        }
    }
    void press(std::int32_t device, std::uint32_t now) {
        if (down || committed)
            return;
        down = true;
        owner = device;
        pressed_at = now;
    }
    bool release(std::int32_t device, std::uint32_t now) {
        if (!down || owner != device || committed)
            return false;
        if (static_cast<std::uint32_t>(now - pressed_at) >= close_hold_ms) {
            committed = true;
            committed_at = now;
            return false;
        }
        down = false;
        return static_cast<std::uint32_t>(now - pressed_at) < 500;
    }
    void cancel(std::int32_t device = -1) {
        if (!committed && (device == -1 || owner == device))
            down = false;
    }
    void update(std::uint32_t now, Presentation& view) {
        const auto elapsed = static_cast<std::uint32_t>(now - pressed_at);
        if (down && !committed && elapsed >= close_hold_ms) {
            committed = true;
            committed_at = now;
        }
        view.hold = down ? std::min(1.0f, elapsed / static_cast<float>(close_hold_ms)) : 0;
        const float t = committed ? std::min(1.0f, (now - committed_at) / 360.0f) : 0;
        view.close = t * t * (3 - 2 * t);
    }
    bool closing() const {
        return committed;
    }
    bool active() const {
        return down || committed;
    }
    bool finished(std::uint32_t now) const {
        return committed && static_cast<std::uint32_t>(now - committed_at) >= 360;
    }
};
} // namespace shelf
