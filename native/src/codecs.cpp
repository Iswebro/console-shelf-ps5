// SPDX-License-Identifier: GPL-3.0-only
// Bundled, pinned stb sources retain their embedded MIT/Unlicense notices.
#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
