// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
const char* gs_vout_init(void);
void gs_vout_present(const void* pixels, int pitch);
void gs_vout_shutdown(void);
int gs_missing_imports(char* output, size_t capacity);
void gs_notify(const char* text);
int gs_request_close(void);
void gs_wait_for_close(void) __attribute__((noreturn));
// Required native lifecycle import. A weak-only reference remains null in the
// tested retail app loader even though the system module is present.
int sceSystemServiceLoadExec(const char* path, const char* const* argv);
#ifdef __cplusplus
}
#endif
