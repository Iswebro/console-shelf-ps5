// SPDX-License-Identifier: GPL-3.0-only
#include "shelf/ui_test_bridge.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <limits>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif

namespace shelf {
namespace {
constexpr std::size_t request_limit = 160, frame_limit = 16 * 1024 * 1024;
constexpr std::uint64_t fnv_offset = 14695981039346656037ull, fnv_prime = 1099511628211ull;
bool path(char (&out)[2304], const char* directory, const char* name) {
    const int used = std::snprintf(out, sizeof(out), "%s/%s", directory, name);
    return used > 0 && static_cast<std::size_t>(used) < sizeof(out);
}
bool discard(const char* file) {
    return std::remove(file) == 0 || errno == ENOENT;
}
bool replace(const char* source, const char* target) {
    // Windows rename does not replace. No reader sees a partially written file;
    // readers may briefly see no file between removal and rename.
    return discard(target) && std::rename(source, target) == 0;
}
bool directory(const char* name) {
    struct stat info{};
    if (stat(name, &info) == 0)
        return (info.st_mode & S_IFMT) == S_IFDIR;
    if (errno != ENOENT)
        return false;
#ifdef _WIN32
    return _mkdir(name) == 0;
#else
    return mkdir(name, 0700) == 0;
#endif
}
bool session(const char* value) {
    if (!value || std::strlen(value) != 32)
        return false;
    for (unsigned i = 0; i < 32; ++i)
        if (!((value[i] >= '0' && value[i] <= '9') || (value[i] >= 'a' && value[i] <= 'f')))
            return false;
    return true;
}
bool decimal(const char*& at, const char* end, std::uint64_t& value) {
    if (at == end || *at < '0' || *at > '9')
        return false;
    const char* first = at;
    value = 0;
    while (at != end && *at >= '0' && *at <= '9') {
        const unsigned digit = static_cast<unsigned>(*at++ - '0');
        if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 10)
            return false;
        value = value * 10 + digit;
    }
    // A canonical decimal form makes accidental extra fields unambiguous.
    return !(at - first > 1 && *first == '0') && at != end && *at++ == ' ';
}
bool operation(const char* name, UiTestRequest& request) {
    if (std::strcmp(name, "snapshot") == 0) {
        request.operation = UiTestOperation::snapshot;
        return true;
    }
    if (std::strcmp(name, "stop") == 0) {
        request.operation = UiTestOperation::stop;
        return true;
    }
    constexpr const char* names[] = {"up", "down", "left", "right", "confirm", "back", "settings", "layout",
                                      "favorite", "previous-tab", "tab"};
    constexpr Action actions[] = {Action::up, Action::down, Action::left, Action::right, Action::confirm,
                                   Action::back, Action::settings, Action::layout, Action::favorite,
                                   Action::previous_tab, Action::next_tab};
    for (unsigned i = 0; i < sizeof(actions) / sizeof(actions[0]); ++i)
        if (std::strcmp(name, names[i]) == 0) {
            request.operation = UiTestOperation::action;
            request.action = actions[i];
            return true;
        }
    return false;
}
bool parse(const char* bytes, std::size_t size, const char* expected_session, std::uint64_t next,
           UiTestRequest& request) {
    constexpr char prefix[] = "ISSHELF_UI_TEST_1 ";
    if (!size || size > request_limit || bytes[size - 1] != '\n' || size < sizeof(prefix) + 32 + 6)
        return false;
    for (std::size_t i = 0; i + 1 < size; ++i)
        if (static_cast<unsigned char>(bytes[i]) < 32 || static_cast<unsigned char>(bytes[i]) > 126)
            return false;
    if (std::memcmp(bytes, prefix, sizeof(prefix) - 1) != 0)
        return false;
    const char* at = bytes + sizeof(prefix) - 1;
    const char* end = bytes + size - 1;
    if (std::memcmp(at, expected_session, 32) != 0 || at[32] != ' ')
        return false;
    at += 33;
    UiTestRequest value;
    if (!decimal(at, end, value.sequence) || !decimal(at, end, value.revision) ||
        value.sequence != next || value.sequence == std::numeric_limits<std::uint64_t>::max())
        return false;
    const auto remaining = static_cast<std::size_t>(end - at);
    if (!remaining || remaining >= sizeof(value.name))
        return false;
    std::memcpy(value.name, at, remaining);
    if (!operation(value.name, value))
        return false;
    request = value;
    return true;
}
std::uint32_t be32(const unsigned char* bytes) {
    return (static_cast<std::uint32_t>(bytes[0]) << 24) | (static_cast<std::uint32_t>(bytes[1]) << 16) |
           (static_cast<std::uint32_t>(bytes[2]) << 8) | bytes[3];
}
std::uint32_t crc(std::uint32_t value, const unsigned char* bytes, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) {
        value ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            value = (value >> 1) ^ ((value & 1) ? 0xedb88320u : 0u);
    }
    return value;
}
struct PngReader {
    FILE* file;
    std::size_t bytes = 0;
    std::uint64_t fnv = fnv_offset;
    bool read(unsigned char* out, std::size_t count) {
        if (count > frame_limit - bytes || std::fread(out, 1, count, file) != count)
            return false;
        bytes += count;
        for (std::size_t i = 0; i < count; ++i)
            fnv = (fnv ^ out[i]) * fnv_prime;
        return true;
    }
};
bool png(const char* file, std::size_t& bytes, std::uint64_t& hash) {
    FILE* input = std::fopen(file, "rb");
    if (!input)
        return false;
    PngReader reader{input};
    unsigned char buffer[4096];
    constexpr unsigned char signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
    bool okay = reader.read(buffer, 8) && std::memcmp(buffer, signature, 8) == 0;
    bool data = false, finished = false;
    for (unsigned chunk = 0; okay && !finished && chunk < 4096; ++chunk) {
        okay = reader.read(buffer, 8);
        if (!okay)
            break;
        const auto length = be32(buffer);
        const bool header = std::memcmp(buffer + 4, "IHDR", 4) == 0;
        const bool image = std::memcmp(buffer + 4, "IDAT", 4) == 0;
        const bool end = std::memcmp(buffer + 4, "IEND", 4) == 0;
        if (length > frame_limit || (chunk == 0 ? !header || length != 13 : header) ||
            (end && (length != 0 || !data))) {
            okay = false;
            break;
        }
        std::uint32_t check = crc(0xffffffffu, buffer + 4, 4);
        std::uint32_t left = length;
        while (okay && left) {
            const auto count = left < sizeof(buffer) ? static_cast<std::size_t>(left) : sizeof(buffer);
            okay = reader.read(buffer, count);
            if (okay && header) {
                const auto width = be32(buffer), height = be32(buffer + 4);
                okay = width && height && width <= 1920 && height <= 1080 && buffer[8] == 8 &&
                       (buffer[9] == 2 || buffer[9] == 6) && buffer[10] == 0 && buffer[11] == 0 && buffer[12] == 0;
            }
            if (okay)
                check = crc(check, buffer, count);
            left -= static_cast<std::uint32_t>(count);
        }
        okay = okay && reader.read(buffer, 4) && be32(buffer) == (check ^ 0xffffffffu);
        if (image && length)
            data = true;
        if (end)
            finished = true;
    }
    okay = okay && finished && std::fgetc(input) == EOF && !std::ferror(input);
    if (std::fclose(input) != 0)
        okay = false;
    if (okay) {
        bytes = reader.bytes;
        hash = reader.fnv;
    }
    return okay;
}
bool result_label(const char* value) {
    if (!value)
        return false;
    const auto size = std::strlen(value);
    if (!size || size > 64)
        return false;
    for (std::size_t i = 0; i < size; ++i)
        if (!((value[i] >= 'a' && value[i] <= 'z') || (value[i] >= 'A' && value[i] <= 'Z') ||
              (value[i] >= '0' && value[i] <= '9') || value[i] == '-' || value[i] == '_'))
            return false;
    return true;
}
} // namespace

bool apply_ui_test_action(UiState& state, Action action) {
    UiState candidate = state;
    input(candidate, action);
    if (candidate.launch_requested || candidate.exit_requested)
        return false;
    state = candidate;
    return true;
}
bool UiTestBridge::start(const char* data_root, const char* session32) {
    stop();
    session_[0] = directory_[0] = request_path_[0] = frame_new_path_[0] = 0;
    if (!data_root || !*data_root || std::strlen(data_root) > 2000 || !session(session32))
        return false;
    for (const unsigned char* at = reinterpret_cast<const unsigned char*>(data_root); *at; ++at)
        if (*at < 32 || *at == 127)
            return false;
    char response[2304], pending[2304], frame[2304];
    if (!path(directory_, data_root, "ui-test") || !directory(directory_) ||
        !path(request_path_, directory_, "request.txt") || !path(frame_new_path_, directory_, "frame.new.png") ||
        !path(response, directory_, "response.json") || !path(pending, directory_, "response.new.json") ||
        !path(frame, directory_, "frame.png") || !discard(response) || !discard(pending) ||
        !discard(frame) || !discard(frame_new_path_))
        return false;
    std::memcpy(session_, session32, sizeof(session_));
    next_sequence_ = 1;
    enabled_ = true;
    return true;
}
void UiTestBridge::stop() { enabled_ = false; }
bool UiTestBridge::poll(UiTestRequest& request) {
    if (!enabled_)
        return false;
    FILE* input = std::fopen(request_path_, "rb");
    if (!input)
        return false;
    char bytes[request_limit + 1];
    const auto size = std::fread(bytes, 1, sizeof(bytes), input);
    const bool okay = !std::ferror(input);
    const bool closed = std::fclose(input) == 0;
    if (!okay || !closed || !parse(bytes, size, session_, next_sequence_, request))
        return false;
    ++next_sequence_;
    return true;
}
bool UiTestBridge::publish(const UiTestRequest& request, std::uint64_t revision, const char* result,
                           bool applied, bool saved, const UiState& state, bool captured) {
    if (!session_[0] || !result_label(result))
        return false;
    UiTestRequest checked;
    if (!std::memchr(request.name, 0, sizeof(request.name)) || !operation(request.name, checked))
        return false;
    char frame[2304], response[2304], pending[2304];
    if (!path(frame, directory_, "frame.png") || !path(response, directory_, "response.json") ||
        !path(pending, directory_, "response.new.json"))
        return false;
    std::size_t bytes = 0;
    std::uint64_t hash = 0;
    const bool available = captured && png(frame_new_path_, bytes, hash) && replace(frame_new_path_, frame);
    if (!available) {
        bytes = 0;
        hash = 0;
        discard(frame_new_path_);
        discard(frame);
    }
    const bool remain_active = enabled_ && request.operation != UiTestOperation::stop;
    if (request.operation == UiTestOperation::stop)
        stop();
    FILE* output = std::fopen(pending, "wb");
    if (!output)
        return false;
    const auto selected = state.selected_system < system_count ? catalog[state.selected_system].id : std::string_view{};
    const int written = std::fprintf(output,
        "{\"protocol\":1,\"active\":%s,\"session\":\"%s\",\"nextSequence\":%llu,\"sequence\":%llu,"
        "\"revision\":%llu,\"action\":\"%s\",\"result\":\"%s\",\"actionApplied\":%s,\"settingsSaved\":%s,"
        "\"frameAvailable\":%s,\"frameBytes\":%zu,\"frameFNV64\":\"%016llx\",\"page\":%d,"
        "\"selectedSystem\":\"%.*s\",\"focus\":%d,\"gameFocus\":%d,\"tab\":%d}\n",
        remain_active ? "true" : "false", session_, static_cast<unsigned long long>(next_sequence_),
        static_cast<unsigned long long>(request.sequence), static_cast<unsigned long long>(revision), request.name, result,
        applied ? "true" : "false", saved ? "true" : "false", available ? "true" : "false", bytes,
        static_cast<unsigned long long>(hash), static_cast<int>(state.page), static_cast<int>(selected.size()),
        selected.data() ? selected.data() : "", state.focus, state.game_focus, static_cast<int>(state.tab));
    const bool flushed = written > 0 && std::fflush(output) == 0 && !std::ferror(output);
    const bool closed = std::fclose(output) == 0;
    const bool published = flushed && closed && replace(pending, response);
    if (!published)
        discard(pending);
    return published;
}
} // namespace shelf
