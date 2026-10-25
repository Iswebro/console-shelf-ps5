// SPDX-License-Identifier: GPL-3.0-only
// A native app requests shell replacement and waits. Returning through the
// payload C runtime's exit path issues a forbidden process syscall (SIGSYS).
// LoadExec's asynchronous contract follows Jewel Case's GPL native main loop.
#include "platform.h"
#include <stdint.h>
int sceKernelUsleep(uint32_t microseconds);
int gs_request_close(void) {
    return sceSystemServiceLoadExec ? sceSystemServiceLoadExec("exit", 0) : -1;
}
void gs_wait_for_close(void) {
    for (;;)
        sceKernelUsleep(100000);
}
