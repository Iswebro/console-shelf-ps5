// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "library.hpp"
namespace shelf {
// An explicitly selected test/export tree, or the real PS5 root for a permitted backend.
// Does not follow symbolic links/reparse points and never opens a file for writing.
struct ReadOnlyTree {
    char prefix[2048]{};
};
bool init_read_only_tree(ReadOnlyTree& tree, const char* prefix);
ReadOnlyFs tree_filesystem(ReadOnlyTree& tree);
} // namespace shelf
