// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "cjson/cJSON.h"
#include "library.hpp"
namespace shelf {
// Bounded metadata only. Uses the same allocator as the PS5 renderer.
struct JsonDocument {
    cJSON* root = nullptr;
    JsonDocument() = default;
    JsonDocument(const JsonDocument&) = delete;
    JsonDocument& operator=(const JsonDocument&) = delete;
    ~JsonDocument();
    bool parse(const char* bytes, std::size_t size);
    bool read(const ReadOnlyFs& fs, const char* path, std::size_t limit);
};
bool valid_utf8(std::string_view value);
const char* json_string(const cJSON* object, const char* key);
} // namespace shelf
