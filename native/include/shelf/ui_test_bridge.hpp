// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ui.hpp"
#include <cstdint>

namespace shelf {
// UI test timers are always shorter than half the 32-bit clock cycle. Within
// that bound, unsigned subtraction handles wrap, while a future start or stale
// loop sample is rejected instead of appearing to have elapsed for 49 days.
inline constexpr bool ui_test_elapsed(std::uint32_t now, std::uint32_t start, std::uint32_t interval) {
    const auto elapsed = static_cast<std::uint32_t>(now - start);
    return interval < 0x80000000u && elapsed < 0x80000000u && elapsed >= interval;
}
enum class UiTestOperation { snapshot, action, stop };
struct UiTestRequest {
    std::uint64_t sequence = 0, revision = 0;
    UiTestOperation operation = UiTestOperation::snapshot;
    Action action = Action::up;
    char name[20]{};
};
// Trial the same UI transition used by physical input, and refuse any action
// that would leave the app. Refused actions do not mutate the live state.
bool apply_ui_test_action(UiState& state, Action action);

// Private UI testing transport. The caller owns opt-in, expiry and dispatch;
// this class only exchanges bounded records in the app's own data directory.
// The session prevents stale replay and does not authenticate an FTP client.
class UiTestBridge {
    bool enabled_ = false;
    std::uint64_t next_sequence_ = 1;
    char session_[33]{};
    char directory_[2304]{}, request_path_[2304]{}, frame_new_path_[2304]{};

  public:
    bool start(const char* data_root, const char* session32);
    void stop();
    bool active() const { return enabled_; }
    // Accepts exactly the next sequence and consumes it before returning.
    bool poll(UiTestRequest& request);
    const char* frame_path() const { return frame_new_path_; }
    // True means the response was published. frameAvailable in that response
    // separately records whether a complete matching PNG was published. A
    // stopped initialized session can publish its final inactive response.
    bool publish(const UiTestRequest& request, std::uint64_t revision, const char* result,
                 bool applied, bool saved, const UiState& state, bool captured);
};
} // namespace shelf
