// SPDX-License-Identifier: GPL-3.0-only
// Shelf uses the existing loader's process permissions and system SPRX modules.
// No credential/syscall-bound patching and no third-party shared-object loader.
#include <stddef.h>
int __patch_init(void) {
    return 0;
}
int __rtld_so_init(void) {
    return 0;
}
void* __rtld_so_new(void* parent, const char* soname) {
    (void)parent;
    (void)soname;
    return NULL;
}
