// SPDX-License-Identifier: GPL-3.0-only
#include "shelf/json.hpp"
#include "shelf/memory.hpp"
#include <cstring>
namespace shelf {
bool valid_utf8(std::string_view text) {
    std::size_t i = 0;
    while (i < text.size()) {
        auto a = static_cast<unsigned char>(text[i++]);
        if (a < 0x80) {
            if (a < 32 || a == 127)
                return false;
            continue;
        }
        int count;
        unsigned code, minimum;
        if (a >= 0xc2 && a <= 0xdf) {
            count = 1;
            code = a & 31;
            minimum = 0x80;
        } else if (a >= 0xe0 && a <= 0xef) {
            count = 2;
            code = a & 15;
            minimum = 0x800;
        } else if (a >= 0xf0 && a <= 0xf4) {
            count = 3;
            code = a & 7;
            minimum = 0x10000;
        } else
            return false;
        if (i + static_cast<std::size_t>(count) > text.size())
            return false;
        while (count--) {
            const auto b = static_cast<unsigned char>(text[i++]);
            if ((b & 0xc0) != 0x80)
                return false;
            code = (code << 6) | (b & 63);
        }
        if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))
            return false;
    }
    return true;
}
namespace {
bool valid_node(const cJSON* node) {
    if (!node)
        return false;
    if (node->string && !valid_utf8(node->string))
        return false;
    if (cJSON_IsString(node) && (!node->valuestring || !valid_utf8(node->valuestring)))
        return false;
    for (const auto* child = node->child; child; child = child->next) {
        if (!valid_node(child))
            return false;
        if (cJSON_IsObject(node))
            for (const auto* before = node->child; before != child; before = before->next)
                if (!before->string || !child->string || std::strcmp(before->string, child->string) == 0)
                    return false;
    }
    return true;
}
} // namespace
JsonDocument::~JsonDocument() {
    cJSON_Delete(root);
}
bool JsonDocument::parse(const char* bytes, std::size_t size) {
    cJSON_Delete(root);
    root = nullptr;
    if (!bytes || !size || std::memchr(bytes, 0, size))
        return false;
    // Disallow embedded NULs that would be truncated by C-string consumers.
    if (std::string_view(bytes, size).find("\\u0000") != std::string_view::npos)
        return false;
    static const bool hooks = [] {
        cJSON_Hooks h{allocate, release};
        cJSON_InitHooks(&h);
        return true;
    }();
    (void)hooks;
    const char* end = nullptr;
    root = cJSON_ParseWithLengthOpts(bytes, size, &end, false);
    if (!root || !end || end < bytes || end > bytes + size) {
        cJSON_Delete(root);
        root = nullptr;
        return false;
    }
    while (end != bytes + size && (*end == ' ' || *end == '\r' || *end == '\n' || *end == '\t'))
        ++end;
    if (end != bytes + size || !valid_node(root)) {
        cJSON_Delete(root);
        root = nullptr;
        return false;
    }
    return true;
}
bool JsonDocument::read(const ReadOnlyFs& fs, const char* path, std::size_t limit) {
    FileStat stat;
    if (fs.stat(fs.context, path, stat) != FsStatus::ok || stat.directory || !stat.size || stat.size > limit)
        return false;
    auto* bytes = static_cast<char*>(allocate(static_cast<std::size_t>(stat.size) + 1));
    if (!bytes)
        return false;
    std::size_t used = 0;
    const bool ok = fs.read(fs.context, path, 0, bytes, static_cast<std::size_t>(stat.size), used) == FsStatus::ok &&
                    used == stat.size && parse(bytes, used);
    release(bytes);
    return ok;
}
const char* json_string(const cJSON* object, const char* key) {
    const auto* item = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsString(item) && item->valuestring ? item->valuestring : "";
}
} // namespace shelf
