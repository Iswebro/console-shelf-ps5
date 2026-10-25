// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstddef>
#include <cstdint>
struct plm_t;
namespace shelf {
// One local, silent MPEG-1 intro. No network, audio device or emulator access.
// Files are bounded and reassembled sequence headers are checked before video-frame allocation.
class Movie {
  public:
    Movie() = default;
    ~Movie() { close(); }
    Movie(const Movie&) = delete;
    Movie& operator=(const Movie&) = delete;
    bool open(const char* path, unsigned now);
    bool advance(unsigned now, bool still = false);
    void close();
    const std::uint8_t* pixels() const { return rgba_; }
    int width() const { return width_; }
    int height() const { return height_; }
    bool playing() const { return decoder_ && !ended_; }
  private:
    plm_t* decoder_ = nullptr;
    std::uint8_t* bytes_ = nullptr;
    std::uint8_t* rgba_ = nullptr;
    int width_ = 0, height_ = 0;
    unsigned started_ = 0, frames_ = 0;
    double rate_ = 0;
    bool ended_ = false, paused_ = false;
    bool decode();
};
} // namespace shelf
