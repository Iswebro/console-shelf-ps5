// SPDX-License-Identifier: GPL-3.0-only
#include "shelf/filesystem.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif
namespace shelf {
namespace {
FsStatus error_status() {
#ifdef _WIN32
    const auto e = GetLastError();
    return e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND ? FsStatus::missing
           : e == ERROR_ACCESS_DENIED                             ? FsStatus::denied
                                                                  : FsStatus::failed;
#else
    return errno == ENOENT || errno == ENOTDIR ? FsStatus::missing
           : errno == EACCES                   ? FsStatus::denied
                                               : FsStatus::failed;
#endif
}
#ifdef _WIN32
bool wide(const char* path, wchar_t (&out)[4096]) {
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, out, 4096) > 0;
}
#endif
FsStatus physical_stat(const char* path, FileStat& result) {
#ifdef _WIN32
    wchar_t name[4096];
    if (!wide(path, name))
        return FsStatus::invalid;
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(name, GetFileExInfoStandard, &data))
        return error_status();
    if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
        return FsStatus::denied;
    result.directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    result.size = (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
    const auto ticks =
        (static_cast<std::uint64_t>(data.ftLastWriteTime.dwHighDateTime) << 32) | data.ftLastWriteTime.dwLowDateTime;
    result.modified =
        ticks >= 116444736000000000ull ? static_cast<std::int64_t>((ticks - 116444736000000000ull) / 10000000ull) : 0;
#else
    struct stat data{};
    if (lstat(path, &data) != 0)
        return error_status();
    if (S_ISLNK(data.st_mode) || (!S_ISDIR(data.st_mode) && !S_ISREG(data.st_mode)))
        return FsStatus::denied;
    result.directory = S_ISDIR(data.st_mode);
    result.size = static_cast<std::uint64_t>(data.st_size);
    result.modified = static_cast<std::int64_t>(data.st_mtime);
#endif
    return FsStatus::ok;
}
FsStatus map_path(const ReadOnlyTree& tree, const char* path, char (&output)[3072]) {
    if (!path || !valid_virtual_path(path))
        return FsStatus::invalid;
    const auto root_length = std::strlen(tree.prefix), length = std::strlen(path);
    if (root_length + length >= sizeof(output))
        return FsStatus::invalid;
    std::snprintf(output, sizeof(output), "%s%s", tree.prefix, path);
    // Check every component before opening: an inner symlink is not a scan root.
    for (std::size_t i = root_length + 1; i <= root_length + length; ++i) {
        if (output[i] && output[i] != '/')
            continue;
        const char saved = output[i];
        output[i] = 0;
        FileStat stat;
        const auto status = physical_stat(output, stat);
        output[i] = saved;
        if (status != FsStatus::ok)
            return status;
        if (saved && !stat.directory)
            return FsStatus::invalid;
    }
    return FsStatus::ok;
}
FsStatus tree_stat(void* context, const char* path, FileStat& result) {
    char physical[3072];
    const auto status = map_path(*static_cast<ReadOnlyTree*>(context), path, physical);
    return status == FsStatus::ok ? physical_stat(physical, result) : status;
}
FsStatus tree_read(void* context, const char* path, std::uint64_t offset, void* output, std::size_t cap,
                   std::size_t& used) {
    used = 0;
    char physical[3072];
    const auto status = map_path(*static_cast<ReadOnlyTree*>(context), path, physical);
    if (status != FsStatus::ok)
        return status;
    FileStat stat;
    if (physical_stat(physical, stat) != FsStatus::ok || stat.directory)
        return FsStatus::invalid;
#ifdef _WIN32
    wchar_t name[4096];
    if (!wide(physical, name))
        return FsStatus::invalid;
    FILE* file = _wfopen(name, L"rb");
    if (!file)
        return FsStatus::failed;
    const bool seeked = offset <= 0x7fffffffffffffffull && _fseeki64(file, static_cast<__int64>(offset), SEEK_SET) == 0;
#else
    FILE* file = std::fopen(physical, "rb");
    if (!file)
        return FsStatus::failed;
    const bool seeked = offset <= 0x7fffffffffffffffull && fseeko(file, static_cast<off_t>(offset), SEEK_SET) == 0;
#endif
    if (!seeked) {
        std::fclose(file);
        return FsStatus::invalid;
    }
    used = std::fread(output, 1, cap, file);
    const bool good = !std::ferror(file);
    std::fclose(file);
    return good ? FsStatus::ok : FsStatus::failed;
}
FsStatus tree_list(void* context, const char* path, EntryVisitor visitor, void* destination) {
    char physical[3072];
    const auto status = map_path(*static_cast<ReadOnlyTree*>(context), path, physical);
    if (status != FsStatus::ok)
        return status;
#ifdef _WIN32
    wchar_t name[4096];
    char pattern[3072];
    if (std::snprintf(pattern, sizeof(pattern), "%s/*", physical) >= static_cast<int>(sizeof(pattern)) ||
        !wide(pattern, name))
        return FsStatus::invalid;
    WIN32_FIND_DATAW data{};
    const HANDLE search = FindFirstFileW(name, &data);
    if (search == INVALID_HANDLE_VALUE)
        return error_status();
    bool stopped = false;
    do {
        if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
            continue;
        char entry[1024];
        if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, data.cFileName, -1, entry, sizeof(entry), nullptr,
                                 nullptr))
            continue;
#else
    DIR* directory = opendir(physical);
    if (!directory)
        return error_status();
    bool stopped = false;
    bool read_error = false;
    for (;;) {
        errno = 0;
        auto* data = readdir(directory);
        if (!data) {
            read_error = errno != 0;
            break;
        }
        const char* entry = data->d_name;
#endif
        if (std::strcmp(entry, ".") == 0 || std::strcmp(entry, "..") == 0)
            continue;
        char candidate[path_capacity];
        if (std::snprintf(candidate, sizeof(candidate), "%s%s%s", path, std::strcmp(path, "/") == 0 ? "" : "/",
                          entry) >= static_cast<int>(sizeof(candidate)))
            continue;
        FileStat info;
        if (tree_stat(context, candidate, info) == FsStatus::ok && !visitor(destination, entry, info)) {
            stopped = true;
            break;
        }
#ifdef _WIN32
    } while (FindNextFileW(search, &data));
    const auto last = GetLastError();
    FindClose(search);
    if (!stopped && last != ERROR_NO_MORE_FILES)
        return FsStatus::failed;
#else
    }
    closedir(directory);
    if (read_error)
        return FsStatus::failed;
#endif
    return stopped ? FsStatus::limit : FsStatus::ok;
}
} // namespace
bool init_read_only_tree(ReadOnlyTree& tree, const char* prefix) {
    if (!prefix || std::strlen(prefix) >= sizeof(tree.prefix))
        return false;
    std::snprintf(tree.prefix, sizeof(tree.prefix), "%s", prefix);
    std::size_t length = std::strlen(tree.prefix);
    while (length && (tree.prefix[length - 1] == '/' || tree.prefix[length - 1] == '\\'))
        tree.prefix[--length] = 0;
    if (length) {
        FileStat stat;
        return physical_stat(tree.prefix, stat) == FsStatus::ok && stat.directory;
    }
    return true;
}
ReadOnlyFs tree_filesystem(ReadOnlyTree& tree) {
    return {&tree, tree_list, tree_stat, tree_read};
}
} // namespace shelf
