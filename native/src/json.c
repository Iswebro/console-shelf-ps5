// SPDX-License-Identifier: GPL-3.0-only
// Unmodified MIT parser implementation; its original license is retained.
#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#if defined(__clang__) && defined(SHELF_PS5)
// SDK v0.42's C11 math macros contain redundant qualified _Generic branches.
// Limit the compatibility suppression to the unmodified parser translation unit.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunreachable-code-generic-assoc"
#endif
#define CJSON_HIDE_SYMBOLS
#define CJSON_NESTING_LIMIT 64
#include "../third_party/cjson/cJSON.c"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#if defined(__clang__) && defined(SHELF_PS5)
#pragma clang diagnostic pop
#endif
