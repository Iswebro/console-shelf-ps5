// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <cstdint>
namespace shelf {
struct Presentation {
    bool loading = false;
    float reveal = 1, appear = 1, hold = 0, close = 0;
};
class CloseGesture {
    bool down = false, committed = false;
    std::int32_t owner = -1;
    std::uint32_t pressed_at = 0, committed_at = 0;

  public:
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
        if (static_cast<std::uint32_t>(now - pressed_at) >= 4000) {
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
        if (down && !committed && elapsed >= 4000) {
            committed = true;
            committed_at = now;
        }
        view.hold = down ? std::min(1.0f, elapsed / 4000.0f) : 0;
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
