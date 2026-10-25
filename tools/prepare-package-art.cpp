// SPDX-License-Identifier: GPL-3.0-only
// Deterministic packaging conversion of Shelf's existing logo, not new artwork.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#define STBI_ONLY_PNG
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
int main(int argc, char** argv) {
    if (argc != 3)
        return 1;
    if (FILE* existing = std::fopen(argv[2], "rb")) {
        std::fclose(existing);
        return 1;
    }
    int w = 0, h = 0, channels = 0;
    auto* input = stbi_load(argv[1], &w, &h, &channels, 4);
    if (!input || w != h || w < 1 || w > 4096) {
        stbi_image_free(input);
        return 1;
    }
    constexpr int side = 512;
    auto* output = static_cast<unsigned char*>(std::malloc(side * side * 4));
    if (!output) {
        stbi_image_free(input);
        return 1;
    }
    for (int y = 0; y < side; ++y)
        for (int x = 0; x < side; ++x) {
            std::uint64_t sums[4]{};
            const int left = x * w, top = y * h, right = (x + 1) * w, bottom = (y + 1) * h;
            for (int sy = top / side; sy < std::min(h, (bottom + side - 1) / side); ++sy)
                for (int sx = left / side; sx < std::min(w, (right + side - 1) / side); ++sx) {
                    const auto weight =
                        static_cast<std::uint64_t>(std::min(right, (sx + 1) * side) - std::max(left, sx * side)) *
                        static_cast<std::uint64_t>(std::min(bottom, (sy + 1) * side) - std::max(top, sy * side));
                    const auto* pixel = input + (sy * w + sx) * 4;
                    sums[3] += weight * pixel[3];
                    for (int c = 0; c < 3; ++c)
                        sums[c] += weight * pixel[3] * pixel[c];
                }
            auto* pixel = output + (y * side + x) * 4;
            for (int c = 0; c < 3; ++c)
                pixel[c] = sums[3] ? static_cast<unsigned char>((sums[c] + sums[3] / 2) / sums[3]) : 0;
            const auto area = static_cast<std::uint64_t>(w) * h;
            pixel[3] = static_cast<unsigned char>((sums[3] + area / 2) / area);
        }
    const bool ok = stbi_write_png(argv[2], side, side, 4, output, side * 4) != 0;
    std::free(output);
    stbi_image_free(input);
    return ok ? 0 : 1;
}
