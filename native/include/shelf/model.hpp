// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "catalog.generated.hpp"

namespace shelf {
enum class Layout { grid, swipe };
enum class GridLook { icons, cards };
enum class Scope { all, available };
enum class Sort { featured, name_asc, name_desc, newest, oldest, brand, games, favorites };
enum class LaunchCapability { unavailable, emulator_menu, direct_game };

// Filled only by runtime adapters. A suggested core is never evidence of installation.
struct Availability {
    bool built_in = false;
    std::size_t emulator_count = 0;
    std::size_t game_count = 0;
    LaunchCapability launch = LaunchCapability::unavailable;
};
using Inventory = std::array<Availability, system_count>;

struct Preferences {
    Layout layout = Layout::grid;
    GridLook grid_look = GridLook::icons;
    Scope scope = Scope::all;
    Sort sort = Sort::featured;
    // These arrays are in-memory state. Persistence must use stable system IDs.
    std::array<bool, system_count> hidden{};
    std::array<bool, system_count> favorite{};
};

struct BrowseRequest {
    std::string_view query;
    std::string_view brand;
};
struct BrowseResult {
    std::array<std::size_t, system_count> indices{};
    std::size_t count = 0;
};

// Returns system_count when absent. Unknown identities must never alias the first system.
std::size_t find_system(std::string_view id);
bool is_available(const Availability& evidence);
BrowseResult browse(const Preferences& preferences, const Inventory& inventory, BrowseRequest request = {});
} // namespace shelf
