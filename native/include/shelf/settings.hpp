// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "model.hpp"

namespace shelf {
struct Settings {
    Preferences preferences;
    std::array<std::array<char, 48>, system_count> preferred_emulator{};
};
enum class SettingsLoad { loaded, missing, backup, invalid };
// Reads only the named owned settings file and its backup. Never writes during load.
SettingsLoad load_settings(const char* path, Settings& output);
// Same-directory temporary file, flush, backup of a valid previous file, atomic replace.
bool save_settings(const char* path, const Settings& settings);
} // namespace shelf
