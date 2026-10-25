// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "library.hpp"
namespace shelf {
// Anonymous read-only client for an already running ftpsrv. No start/stop/upload commands.
struct ReadOnlyFtp {
    char host[16]{};
    unsigned short port = 2121;
    void* directories = nullptr;
    std::size_t entry_count = 0, directory_count = 0;
    bool network_ready = false;
    ReadOnlyFtp() = default;
    ReadOnlyFtp(const ReadOnlyFtp&) = delete;
    ReadOnlyFtp& operator=(const ReadOnlyFtp&) = delete;
    ~ReadOnlyFtp();
};
bool init_read_only_ftp(ReadOnlyFtp& client, const char* numeric_ipv4, unsigned short port = 2121);
void clear_ftp_cache(ReadOnlyFtp& client);
ReadOnlyFs ftp_filesystem(ReadOnlyFtp& client);
} // namespace shelf
