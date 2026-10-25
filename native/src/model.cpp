// SPDX-License-Identifier: GPL-3.0-only
#include "shelf/model.hpp"
#include <algorithm>

namespace shelf {
namespace {
char fold_ascii(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}
bool contains(std::string_view value, std::string_view query) {
    if (query.size() > value.size())
        return false;
    for (std::size_t offset = 0; offset <= value.size() - query.size(); ++offset) {
        bool matches = true;
        for (std::size_t i = 0; i < query.size(); ++i) {
            if (fold_ascii(value[offset + i]) != fold_ascii(query[i])) {
                matches = false;
                break;
            }
        }
        if (matches)
            return true;
    }
    return false;
}
} // namespace

std::size_t find_system(std::string_view id) {
    for (std::size_t i = 0; i < system_count; ++i)
        if (catalog[i].id == id)
            return i;
    return system_count;
}

bool is_available(const Availability& evidence) {
    return evidence.built_in || evidence.emulator_count > 0 || evidence.game_count > 0;
}

BrowseResult browse(const Preferences& preferences, const Inventory& inventory, BrowseRequest request) {
    BrowseResult result;
    const auto brand = !request.brand.empty()              ? request.brand
                       : preferences.brand < brands.size() ? brands[preferences.brand]
                                                           : std::string_view{};
    for (std::size_t i = 0; i < system_count; ++i) {
        const auto& system = catalog[i];
        if (preferences.hidden[i] || (preferences.scope == Scope::available && !is_available(inventory[i])))
            continue;
        if (!brand.empty() && system.brand != brand)
            continue;
        const auto& evidence = inventory[i];
        if ((preferences.filter == SystemFilter::emulators && evidence.emulator_count == 0) ||
            (preferences.filter == SystemFilter::games && evidence.game_count == 0) ||
            (preferences.filter == SystemFilter::built_in && !evidence.built_in) ||
            (preferences.filter == SystemFilter::no_route && evidence.launch != LaunchCapability::unavailable))
            continue;
        if (!contains(system.name, request.query) && !contains(system.brand, request.query) &&
            !contains(system.suggested_core, request.query))
            continue;
        result.indices[result.count++] = i;
    }
    // Stable identity is the final tie-break, so repeated scans never shuffle equal items.
    std::sort(result.indices.begin(), result.indices.begin() + result.count, [&](std::size_t a, std::size_t b) {
        const auto& left = catalog[a];
        const auto& right = catalog[b];
        switch (preferences.sort) {
        case Sort::name_asc:
            if (left.name != right.name)
                return left.name < right.name;
            break;
        case Sort::name_desc:
            if (left.name != right.name)
                return left.name > right.name;
            break;
        case Sort::newest:
            if (left.year != right.year)
                return left.year > right.year;
            break;
        case Sort::oldest:
            if (left.year == 0 || right.year == 0) {
                if (left.year != right.year)
                    return right.year == 0;
            } else if (left.year != right.year)
                return left.year < right.year;
            break;
        case Sort::brand:
            if (left.brand != right.brand)
                return left.brand < right.brand;
            if (left.name != right.name)
                return left.name < right.name;
            break;
        case Sort::games:
            if (inventory[a].game_count != inventory[b].game_count)
                return inventory[a].game_count > inventory[b].game_count;
            break;
        case Sort::favorites:
            if (preferences.favorite[a] != preferences.favorite[b])
                return preferences.favorite[a];
            break;
        case Sort::featured:
            break;
        }
        return a < b;
    });
    return result;
}
} // namespace shelf
