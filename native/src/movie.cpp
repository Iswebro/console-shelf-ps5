// SPDX-License-Identifier: GPL-3.0-only
#include "shelf/memory.hpp"
#include <cstdio>
#include "pl_mpeg/pl_mpeg.h"

#include "shelf/movie.hpp"
#include <cstdio>
#include <cstring>

namespace shelf {
namespace {
bool bounded_headers(std::uint8_t* bytes, std::size_t size, int& width, int& height) {
    // Sequence headers can straddle PES packets. Validate the reassembled video
    // payload before creating a video decoder or allocating its frame surfaces.
    auto* buffer = plm_buffer_create_with_memory(bytes, size, 0);
    if (!buffer) return false;
    auto* demux = plm_demux_create(buffer, 1);
    if (!demux) { plm_buffer_destroy(buffer); return false; }
    bool okay = plm_demux_has_headers(demux) != 0;
    unsigned headers = 0, pending = 0, used = 0;
    int kind = 0, rate = 0;
    std::uint8_t fields[4]{};
    std::uint32_t prefix = ~0u;
    const auto base = reinterpret_cast<std::uintptr_t>(bytes);
    std::uintptr_t previous = 0;
    while (okay) {
        auto* packet = plm_demux_decode(demux);
        if (!packet) break;
        if (packet->type != PLM_DEMUX_PACKET_VIDEO_1) continue;
        const auto address = reinterpret_cast<std::uintptr_t>(packet->data);
        if (address < base || address - base > size || packet->length > size - (address - base) || address <= previous) {
            okay = false; break;
        }
        previous = address;
        for (std::size_t i = 0; i < packet->length && okay; ++i) {
            const auto byte = packet->data[i];
            prefix = (prefix << 8) | byte;
            if (pending) {
                fields[used++] = byte;
                if (used == pending) {
                    if (kind == 0xb5) okay = (fields[0] >> 4) != 1; // MPEG-2 sequence extension.
                    else {
                        const int w = (fields[0] << 4) | (fields[1] >> 4), h = ((fields[1] & 15) << 8) | fields[2], r = fields[3] & 15;
                        okay = w >= 16 && h >= 16 && w <= 640 && h <= 368 && !(w % 2) && !(h % 2) && r >= 1 && r <= 5 &&
                               (!headers || (w == width && h == height && r == rate));
                        if (okay) { width = w; height = h; rate = r; ++headers; }
                    }
                    pending = used = 0;
                }
            }
            if (prefix == 0x000001b3 || prefix == 0x000001b5) {
                kind = static_cast<int>(prefix & 255); pending = kind == 0xb3 ? 4 : 1; used = 0;
            }
        }
    }
    plm_demux_destroy(demux);
    return okay && headers && !pending;
}
} // namespace
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
    if (!bounded_headers(bytes_, static_cast<std::size_t>(size), width_, height_)) { close(); return false; }
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
