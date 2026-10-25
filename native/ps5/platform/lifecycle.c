// SPDX-License-Identifier: GPL-3.0-only
// A native app requests shell replacement and waits. Returning through the
// payload C runtime's exit path issues a forbidden process syscall (SIGSYS).
// LoadExec's asynchronous contract follows Jewel Case's GPL native main loop.
#include "platform.h"
#include <stdint.h>
#include <string.h>
int sceKernelUsleep(uint32_t microseconds);
int sceSystemServiceGetStatus(void* status);
int gs_app_visibility(unsigned* flags) {
    if (!flags)
        return -1;
    /* Public SystemService status ABI (also documented by shadPS4): event count
     * followed by byte-sized overlay/background flags. A generously sized
     * output buffer retains reserved fields without importing an SDK struct. */
    _Alignas(16) unsigned char status[512];
    memset(status, 0, sizeof(status));
    int result = sceSystemServiceGetStatus(status);
    if (result)
        return result;
    if (status[4] > 1 || status[5] > 1)
        return -2;
    *flags = (unsigned)status[4] | ((unsigned)status[5] << 1);
    return 0;
}
int gs_request_close(void) {
    return sceSystemServiceLoadExec("exit", 0);
}
void gs_wait_for_close(void) {
    for (;;)
        sceKernelUsleep(100000);
}
