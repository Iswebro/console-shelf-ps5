// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "library.hpp"
namespace shelf {
struct FtpDiagnostics {
    // Fixed operation names and numeric errors only; no paths, credentials or replies.
    const char* first_failure = "none";
    int error = 0;
    std::size_t connections = 0, listings = 0, reads = 0;
    bool has_failure = false;
    void failed(const char* operation, int code) {
        if (!has_failure) {
            has_failure = true;
            first_failure = operation;
            error = code;
        }
    }
};
// Anonymous read-only client for an already running ftpsrv. No start/stop/upload commands.
struct ReadOnlyFtp {
    char host[16]{};
    unsigned short port = 2121;
    void* directories = nullptr;
    std::size_t entry_count = 0, directory_count = 0;
    bool network_ready = false;
    FtpDiagnostics diagnostics;
    bool (*cancelled)(void*) = nullptr;
    void* cancel_context = nullptr;
    ReadOnlyFtp() = default;
    ReadOnlyFtp(const ReadOnlyFtp&) = delete;
    ReadOnlyFtp& operator=(const ReadOnlyFtp&) = delete;
    ~ReadOnlyFtp();
};
bool init_read_only_ftp(ReadOnlyFtp& client, const char* numeric_ipv4, unsigned short port = 2121);
void clear_ftp_cache(ReadOnlyFtp& client);
ReadOnlyFs ftp_filesystem(ReadOnlyFtp& client);
} // namespace shelf
