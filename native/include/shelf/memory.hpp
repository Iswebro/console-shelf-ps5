// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstddef>
#include <cstdlib>
#ifdef SHELF_PS5
extern "C" {
void* gs_malloc(std::size_t size);
void* gs_calloc(std::size_t count, std::size_t size);
void* gs_realloc(void* address, std::size_t size);
void gs_free(void* address);
}
#endif
namespace shelf {
inline void* allocate(std::size_t size) {
#ifdef SHELF_PS5
    return gs_malloc(size);
#else
    return std::malloc(size);
#endif
}
inline void* allocate_zero(std::size_t count, std::size_t size) {
#ifdef SHELF_PS5
    return gs_calloc(count, size);
#else
    return std::calloc(count, size);
#endif
}
inline void release(void* address) {
#ifdef SHELF_PS5
    gs_free(address);
#else
    std::free(address);
#endif
}
} // namespace shelf
