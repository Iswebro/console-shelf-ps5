// SPDX-License-Identifier: GPL-3.0-only
#include "shelf/memory.hpp"
#include <cstdio>
#include "pl_mpeg/pl_mpeg.h"

#include "shelf/movie.hpp"
#include <cstdio>
#include <cstring>

namespace shelf {
void Movie::close() {
    if (decoder_) plm_destroy(decoder_);
    release(bytes_); release(rgba_);
    decoder_ = nullptr; bytes_ = rgba_ = nullptr;
    width_ = height_ = 0; rate_ = 0; frames_ = 0; ended_ = paused_ = false;
}
bool Movie::open(const char* path, unsigned now) {
    close();
    FILE* file = std::fopen(path, "rb");
    if (!file) return false;
    if (std::fseek(file, 0, SEEK_END) != 0) { std::fclose(file); return false; }
    const long size = std::ftell(file);
    if (size < 16 || size > 32 * 1024 * 1024 || std::fseek(file, 0, SEEK_SET) != 0) {
        std::fclose(file); return false;
    }
    bytes_ = static_cast<std::uint8_t*>(allocate(static_cast<std::size_t>(size)));
    const bool read = bytes_ && std::fread(bytes_, 1, static_cast<std::size_t>(size), file) == static_cast<std::size_t>(size) && !std::ferror(file);
    std::fclose(file);
    if (!read || std::memcmp(bytes_, "\0\0\1\xba", 4) != 0) { close(); return false; }
    int headers = 0, rate = 0;
    for (long i = 0; i + 7 < size; ++i) {
        const auto* b = bytes_ + i;
        if (b[0] || b[1] || b[2] != 1) continue;
        if (b[3] == 0xb5 && (b[4] >> 4) == 1) { close(); return false; } // MPEG-2 sequence extension.
        if (b[3] != 0xb3) continue;
        const int w = (b[4] << 4) | (b[5] >> 4), h = ((b[5] & 15) << 8) | b[6], r = b[7] & 15;
        if (w < 16 || h < 16 || w > 640 || h > 368 || w % 2 || h % 2 || r < 1 || r > 5 ||
            (headers && (w != width_ || h != height_ || r != rate))) { close(); return false; }
        width_ = w; height_ = h; rate = r; ++headers;
    }
    if (!headers) { close(); return false; }
    decoder_ = plm_create_with_memory(bytes_, static_cast<std::size_t>(size), 0);
    if (!decoder_) { close(); return false; }
    plm_set_audio_enabled(decoder_, 0);
    plm_set_loop(decoder_, 0);
    if (!plm_has_headers(decoder_) || plm_get_width(decoder_) != width_ || plm_get_height(decoder_) != height_) {
        close(); return false;
    }
    rate_ = plm_get_framerate(decoder_);
    if (!(rate_ >= 23 && rate_ <= 30)) { close(); return false; }
    rgba_ = static_cast<std::uint8_t*>(allocate(static_cast<std::size_t>(width_) * height_ * 4));
    if (rgba_) std::memset(rgba_, 255, static_cast<std::size_t>(width_) * height_ * 4);
    started_ = now;
    if (!rgba_ || !decode()) { close(); return false; }
    return true;
}
bool Movie::decode() {
    if (!decoder_ || ended_) return false;
    auto* frame = plm_decode_video(decoder_);
    if (!frame || frame->width != static_cast<unsigned>(width_) || frame->height != static_cast<unsigned>(height_)) {
        ended_ = true; return false;
    }
    plm_frame_to_rgba(frame, rgba_, width_ * 4);
    ++frames_;
    if (frames_ >= 1800 || plm_has_ended(decoder_)) ended_ = true;
    return true;
}
bool Movie::advance(unsigned now, bool still) {
    if (!playing()) return false;
    if (still) { paused_ = true; return false; }
    if (paused_) { started_ = now - static_cast<unsigned>((frames_ - 1) * 1000 / rate_); paused_ = false; }
    const unsigned elapsed = now - started_;
    if (elapsed > 60000) { ended_ = true; return false; }
    const unsigned wanted = static_cast<unsigned>(elapsed * rate_ / 1000) + 1;
    bool changed = false;
    for (unsigned catchup = 0; frames_ < wanted && catchup < 3 && playing(); ++catchup) changed = decode() || changed;
    // A slow renderer stretches the clip instead of queuing an unbounded decode burst.
    if (frames_ < wanted && playing()) started_ = now - static_cast<unsigned>((frames_ - 1) * 1000 / rate_);
    return changed;
}
} // namespace shelf
