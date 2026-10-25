// SPDX-License-Identifier: GPL-3.0-only
#include "shelf/movie.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
namespace {
void check(bool okay, const char* why) {
    if (!okay) { std::fprintf(stderr, "FAIL movie: %s\n", why); std::exit(1); }
}
std::uint64_t digest(const shelf::Movie& movie) {
    std::uint64_t hash = 14695981039346656037ull;
    for (int i = 0; i < movie.width() * movie.height() * 4; ++i) { hash ^= movie.pixels()[i]; hash *= 1099511628211ull; }
    return hash;
}
}
int main(int argc, char** argv) {
    check(argc >= 3, "sample and owned test output are required");
    shelf::Movie movie;
    check(movie.open(argv[1], 100), "the packaged demo decodes as MPEG-1");
    check(movie.width() == 640 && movie.height() == 360, "decoded sample dimensions match its manifest");
    const auto first = digest(movie);
    check(!movie.advance(120) && digest(movie) == first, "render ticks before the next frame reuse its pixels");
    for (unsigned t = 140; t <= 1100; t += 40) movie.advance(t);
    check(digest(movie) != first, "the actual moving sample produces different decoded frames");
    movie.close(); check(!movie.pixels() && !movie.playing(), "closing releases pixels and playback state");
    for (int i = 0; i < 10; ++i) {
        check(movie.open(argv[1], 100), "the decoder can be reopened after cleanup");
        const auto still = digest(movie);
        check(!movie.advance(1100, true) && digest(movie) == still, "reduced motion retains the first frame");
    }
    check(movie.open(argv[1], 0xfffffff0u), "playback can start near clock wrap");
    for (unsigned t = 24; t <= 1024; t += 40) movie.advance(t);
    check(digest(movie) != first, "playback timing survives the timer wrapping");
    check(!movie.advance(70000) && !movie.playing(), "an abandoned clip ends instead of decoding an unlimited backlog");
    for (int i = 3; i < argc; ++i) {
        if (!movie.open(argv[i], 100)) { std::fprintf(stderr, "FAIL original intro: %s\n", argv[i]); return 1; }
        check(movie.width() == 640 && movie.height() == 336, "original intro dimensions match the room manifest");
        const auto start = digest(movie);
        for (unsigned time = 140; time <= 1100; time += 40) movie.advance(time);
        check(digest(movie) != start, "every original intro contains real moving frames");
    }
    FILE* file = std::fopen(argv[1], "rb"); check(file != nullptr, "sample can be inspected");
    std::fseek(file, 0, SEEK_END); const auto size = std::ftell(file); std::rewind(file);
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
    check(std::fread(bytes.data(), 1, bytes.size(), file) == bytes.size(), "full sample read"); std::fclose(file);
    auto later = bytes; unsigned sequences = 0;
    for (std::size_t i = 0; i + 8 < later.size(); ++i) if (!later[i] && !later[i + 1] && later[i + 2] == 1 && later[i + 3] == 0xb3 && ++sequences == 2) {
        later[i + 4] = 255; later[i + 5] |= 240; break;
    }
    check(sequences >= 2, "sample includes later sequence headers");
    file = std::fopen(argv[2], "wb"); check(file != nullptr, "owned future-header fixture can be created");
    std::fwrite(later.data(), 1, later.size(), file); std::fclose(file);
    check(!movie.open(argv[2], 0), "oversized dimensions in a later packet are refused before video allocation");
    auto mpeg2 = bytes; bool extension = false;
    for (std::size_t i = 0; i + 8 < mpeg2.size(); ++i) if (!mpeg2[i] && !mpeg2[i + 1] && mpeg2[i + 2] == 1 && mpeg2[i + 3] == 0xb8) {
        mpeg2[i + 3] = 0xb5; mpeg2[i + 4] = 0x10; extension = true; break;
    }
    check(extension, "sample includes a replaceable GOP header");
    file = std::fopen(argv[2], "wb"); std::fwrite(mpeg2.data(), 1, mpeg2.size(), file); std::fclose(file);
    check(!movie.open(argv[2], 0), "MPEG-2 sequence extensions remain unsupported and fail before video allocation");
    bool altered = false;
    for (std::size_t i = 0; i + 8 < bytes.size(); ++i) if (!bytes[i] && !bytes[i + 1] && bytes[i + 2] == 1 && bytes[i + 3] == 0xb3) {
        bytes[i + 4] = 255; bytes[i + 5] |= 240; altered = true; break;
    }
    check(altered, "test sample contains a sequence header");
    file = std::fopen(argv[2], "wb"); check(file != nullptr, "owned invalid fixture can be created");
    check(std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size(), "invalid fixture written"); std::fclose(file);
    check(!movie.open(argv[2], 0) && !movie.pixels(), "oversized coded dimensions are refused before decoder allocation");
    file = std::fopen(argv[2], "wb"); std::fwrite("not an MPEG video", 1, 17, file); std::fclose(file);
    check(!movie.open(argv[2], 0), "unrecognised media fails without replacing the UI");
    std::puts("PASS: real MPEG decode, frame pacing, still mode, cleanup/reopen, timer wrap, backlog limit and invalid-media refusal.");
    if (argc > 3) std::printf("PASS: all %d original console intro movies decode and animate.\n", argc - 3);
}
