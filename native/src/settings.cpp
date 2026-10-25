// SPDX-License-Identifier: GPL-3.0-only
#include "shelf/settings.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace shelf {
namespace {
constexpr std::array<const char*, 2> layout_names = {"grid", "swipe"};
constexpr std::array<const char*, 2> look_names = {"icons", "cards"};
constexpr std::array<const char*, 2> scope_names = {"all", "available"};
constexpr std::array<const char*, 8> sort_names = {"featured", "az",    "za",    "newest",
                                                   "oldest",   "brand", "games", "favorites"};
constexpr std::array<const char*, 5> filter_names = {"all", "emulators", "games", "built_in", "no_route"};
constexpr std::array<const char*, 3> icon_names = {"gaming", "classic", "jade"};
#ifdef _WIN32
bool wide_path(const char* path, wchar_t (&output)[4096]) {
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, output, 4096) > 0;
}
FILE* open_file(const char* path, const char* mode) {
    wchar_t wide[4096];
    if (!wide_path(path, wide))
        return nullptr;
    FILE* file = nullptr;
    if (_wfopen_s(&file, wide, std::strcmp(mode, "rb") == 0 ? L"rb" : L"wb") != 0)
        return nullptr;
    return file;
}
bool replace_file(const char* from, const char* to) {
    wchar_t source[4096], target[4096];
    return wide_path(from, source) && wide_path(to, target) &&
           MoveFileExW(source, target, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}
#else
FILE* open_file(const char* path, const char* mode) {
    return std::fopen(path, mode);
}
bool replace_file(const char* from, const char* to) {
    return std::rename(from, to) == 0;
}
#endif
bool flush_file(FILE* file) {
    if (std::fflush(file) != 0)
        return false;
#ifdef _WIN32
    return _commit(_fileno(file)) == 0;
#else
    return fsync(fileno(file)) == 0;
#endif
}
template <std::size_t N> int choice(const char* value, const std::array<const char*, N>& options) {
    for (std::size_t i = 0; i < N; ++i)
        if (std::strcmp(value, options[i]) == 0)
            return static_cast<int>(i);
    return -1;
}
bool valid_route(const char* value) {
    const auto length = std::strlen(value);
    if (length >= 48)
        return false;
    for (std::size_t i = 0; i < length; ++i) {
        const auto c = value[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
              c == '.'))
            return false;
    }
    return true;
}
bool valid(const Settings& settings) {
    const auto& p = settings.preferences;
    if (static_cast<unsigned>(p.layout) >= layout_names.size() ||
        static_cast<unsigned>(p.grid_look) >= look_names.size() ||
        static_cast<unsigned>(p.scope) >= scope_names.size() || static_cast<unsigned>(p.sort) >= sort_names.size() ||
        static_cast<unsigned>(p.filter) >= filter_names.size() || static_cast<unsigned>(p.icon) >= icon_names.size() ||
        p.brand >= brands.size())
        return false;
    for (const auto& route : settings.preferred_emulator) {
        if (!std::memchr(route.data(), 0, route.size()) || !valid_route(route.data()))
            return false;
    }
    return true;
}
bool read_file(const char* path, Settings& output) {
    FILE* file = open_file(path, "rb");
    if (!file)
        return false;
    char checked_bytes[32769];
    const auto checked_size = std::fread(checked_bytes, 1, sizeof(checked_bytes), file);
    if (checked_size == sizeof(checked_bytes) || std::ferror(file) || std::memchr(checked_bytes, 0, checked_size)) {
        std::fclose(file);
        return false;
    }
    std::rewind(file);
    Settings parsed;
    std::array<bool, 8 + system_count * 3> seen{};
    char line[256];
    std::size_t bytes = 0;
    bool okay = true;
    while (std::fgets(line, sizeof(line), file)) {
        const auto length = std::strlen(line);
        bytes += length;
        if (bytes > 32768 || (!std::strchr(line, '\n') && !std::feof(file))) {
            okay = false;
            break;
        }
        line[std::strcspn(line, "\r\n")] = 0;
        if (line[0] == 0 || line[0] == '#')
            continue;
        char* equals = std::strchr(line, '=');
        if (!equals) {
            okay = false;
            break;
        }
        *equals = 0;
        const char* value = equals + 1;
        int key = -1;
        if (std::strcmp(line, "version") == 0) {
            key = 0;
            if (std::strcmp(value, "1") != 0)
                okay = false;
        } else if (std::strcmp(line, "layout") == 0) {
            key = 1;
            int n = choice(value, layout_names);
            if (n < 0)
                okay = false;
            else
                parsed.preferences.layout = static_cast<Layout>(n);
        } else if (std::strcmp(line, "look") == 0) {
            key = 2;
            int n = choice(value, look_names);
            if (n < 0)
                okay = false;
            else
                parsed.preferences.grid_look = static_cast<GridLook>(n);
        } else if (std::strcmp(line, "scope") == 0) {
            key = 3;
            int n = choice(value, scope_names);
            if (n < 0)
                okay = false;
            else
                parsed.preferences.scope = static_cast<Scope>(n);
        } else if (std::strcmp(line, "sort") == 0) {
            key = 4;
            int n = choice(value, sort_names);
            if (n < 0)
                okay = false;
            else
                parsed.preferences.sort = static_cast<Sort>(n);
        } else if (std::strcmp(line, "filter") == 0) {
            key = 5;
            int n = choice(value, filter_names);
            if (n < 0)
                okay = false;
            else
                parsed.preferences.filter = static_cast<SystemFilter>(n);
        } else if (std::strcmp(line, "icon") == 0) {
            key = 6;
            int n = choice(value, icon_names);
            if (n < 0)
                okay = false;
            else
                parsed.preferences.icon = static_cast<AppIcon>(n);
        } else if (std::strcmp(line, "brand") == 0) {
            key = 7;
            bool found = false;
            for (std::size_t i = 0; i < brands.size(); ++i)
                if (brands[i] == value) {
                    parsed.preferences.brand = i;
                    found = true;
                    break;
                }
            if (!found)
                okay = false;
        } else {
            const char* dot = std::strchr(line, '.');
            if (!dot)
                continue; // Unknown future keys never become console identities.
            const auto id = find_system(dot + 1);
            if (id == system_count)
                continue;
            const auto prefix = static_cast<std::size_t>(dot - line);
            if ((prefix == 6 && std::strncmp(line, "hidden", 6) == 0) ||
                (prefix == 8 && std::strncmp(line, "favorite", 8) == 0)) {
                if (std::strcmp(value, "0") != 0 && std::strcmp(value, "1") != 0)
                    okay = false;
                const bool favorite = prefix == 8;
                key = static_cast<int>(8 + id + (favorite ? system_count : 0));
                (favorite ? parsed.preferences.favorite : parsed.preferences.hidden)[id] = value[0] == '1';
            } else if (prefix == 8 && std::strncmp(line, "emulator", 8) == 0) {
                key = static_cast<int>(8 + system_count * 2 + id);
                if (!valid_route(value))
                    okay = false;
                else
                    std::memcpy(parsed.preferred_emulator[id].data(), value, std::strlen(value) + 1);
            }
        }
        if (key >= 0) {
            if (seen[static_cast<std::size_t>(key)])
                okay = false;
            seen[static_cast<std::size_t>(key)] = true;
        }
        if (!okay)
            break;
    }
    if (std::ferror(file))
        okay = false;
    std::fclose(file);
    for (std::size_t i = 0; i < 8; ++i)
        if (!seen[i])
            okay = false;
    if (!okay || !valid(parsed))
        return false;
    output = parsed;
    return true;
}
bool copy_file(const char* from, const char* to) {
    FILE* source = open_file(from, "rb");
    if (!source)
        return false;
    FILE* target = open_file(to, "wb");
    if (!target) {
        std::fclose(source);
        return false;
    }
    char buffer[4096];
    bool okay = true;
    for (;;) {
        const auto size = std::fread(buffer, 1, sizeof(buffer), source);
        if (size && std::fwrite(buffer, 1, size, target) != size) {
            okay = false;
            break;
        }
        if (size < sizeof(buffer)) {
            if (std::ferror(source))
                okay = false;
            break;
        }
    }
    if (!flush_file(target))
        okay = false;
    if (std::fclose(target) != 0)
        okay = false;
    std::fclose(source);
    return okay;
}
} // namespace

SettingsLoad load_settings(const char* path, Settings& output) {
    output = Settings{};
    FILE* probe = open_file(path, "rb");
    const bool missing = !probe && errno == ENOENT;
    if (probe)
        std::fclose(probe);
    if (read_file(path, output))
        return SettingsLoad::loaded;
    char backup[4096];
    if (std::snprintf(backup, sizeof(backup), "%s.bak", path) < 0 || std::strlen(path) + 5 >= sizeof(backup))
        return SettingsLoad::invalid;
    if (read_file(backup, output))
        return SettingsLoad::backup;
    return missing ? SettingsLoad::missing : SettingsLoad::invalid;
}

bool save_settings(const char* path, const Settings& settings) {
    if (!valid(settings) || std::strlen(path) > 4000)
        return false;
    char temporary[4096], backup[4096], backup_temporary[4096];
    std::snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    std::snprintf(backup, sizeof(backup), "%s.bak", path);
    std::snprintf(backup_temporary, sizeof(backup_temporary), "%s.bak.tmp", path);
    FILE* file = open_file(temporary, "wb");
    if (!file)
        return false;
    const auto& p = settings.preferences;
    bool okay = std::fprintf(
                    file,
                    "# Console Shelf settings; schema 1; console keys use stable "
                    "IDs\nversion=1\nlayout=%s\nlook=%s\nscope=%s\nsort=%s\nfilter=%s\nicon=%s\nbrand=%.*s\n",
                    layout_names[static_cast<std::size_t>(p.layout)], look_names[static_cast<std::size_t>(p.grid_look)],
                    scope_names[static_cast<std::size_t>(p.scope)], sort_names[static_cast<std::size_t>(p.sort)],
                    filter_names[static_cast<std::size_t>(p.filter)], icon_names[static_cast<std::size_t>(p.icon)],
                    static_cast<int>(brands[p.brand].size()), brands[p.brand].data()) > 0;
    for (std::size_t i = 0; i < system_count && okay; ++i) {
        const auto id = catalog[i].id;
        okay =
            std::fprintf(file, "hidden.%.*s=%d\nfavorite.%.*s=%d\nemulator.%.*s=%s\n", static_cast<int>(id.size()),
                         id.data(), p.hidden[i] ? 1 : 0, static_cast<int>(id.size()), id.data(), p.favorite[i] ? 1 : 0,
                         static_cast<int>(id.size()), id.data(), settings.preferred_emulator[i].data()) > 0;
    }
    if (!flush_file(file))
        okay = false;
    if (std::fclose(file) != 0)
        okay = false;
    Settings verified;
    if (!okay || !read_file(temporary, verified))
        return false;
    Settings previous;
    if (read_file(path, previous) && (!copy_file(path, backup_temporary) || !replace_file(backup_temporary, backup)))
        return false;
    return replace_file(temporary, path);
}
} // namespace shelf
