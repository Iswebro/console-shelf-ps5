// SPDX-License-Identifier: GPL-3.0-only
#include "shelf/graphics.hpp"
#include "shelf/carousel.hpp"
#include "shelf/scene.generated.hpp"
#include "shelf/memory.hpp"
#include "shelf/movie.hpp"
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"
#include "stb/stb_truetype.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace shelf {
namespace {
constexpr int width = 1920, height = 1080;
constexpr SDL_Color white{246, 246, 255, 255}, muted{157, 168, 198, 255}, purple{203, 187, 255, 255},
    panel{22, 26, 28, 255};
SDL_Renderer* renderer = nullptr;
SDL_Texture* background = nullptr;
SDL_Texture* shelf_background = nullptr;
SDL_Texture* halo = nullptr;
std::array<SDL_Texture*, 2> settings_buttons{};
char root[2048]{};
char room_data[2048]{};
unsigned render_time = 0, tv_changed_at = 0;
std::size_t tv_selected = system_count;
struct Font {
    SDL_Texture* texture = nullptr;
    int size = 0;
    bool bold = false;
    stbtt_bakedchar chars[224]{};
};
std::array<Font, 32> fonts;
unsigned resource_errors = 0;
struct Art {
    SDL_Texture* texture = nullptr;
    char name[256]{};
    int w = 0, h = 0;
    int support_y = 0;
    bool monochrome = false;
    SDL_Rect bounds{};
    unsigned age = 0;
};
std::array<Art, 12> art;
unsigned art_age = 0;
OrbitMotion orbit;
ShelfPageMotion shelf_motion;
BrowseResult shelf_list{};
bool shelf_visible = false;
bool room_visible = false;
bool tv_motion = false;
Movie tv_movie;
SDL_Texture* movie_texture = nullptr;
SDL_Texture* noise_texture = nullptr;
Uint32* noise_pixels = nullptr;
int noise_width = 0, noise_height = 0;
std::size_t movie_selected = system_count;
bool movie_demo = false;
void stop_movie() {
    tv_movie.close();
    if (movie_texture) SDL_DestroyTexture(movie_texture);
    movie_texture = nullptr; movie_selected = system_count;
}
float room_scale() {
    return std::max(static_cast<float>(width) / room_scene::width, static_cast<float>(height) / room_scene::height);
}
int room_x(float x) {
    return static_cast<int>((width - room_scene::width * room_scale()) / 2 + x * room_scale() + .5f);
}
int room_y(float y) {
    return static_cast<int>((height - room_scene::height * room_scale()) / 2 + y * room_scale() + .5f);
}
bool orbit_visible = false;
BrowseResult orbit_list{};
const std::array<std::string_view, 8> sorts = {"Featured order",   "Name: A to Z",    "Name: Z to A",
                                               "Newest hardware",  "Oldest hardware", "Brand, then name",
                                               "Most games first", "Favorites first"};
const std::array<std::string_view, 5> filters = {"All systems", "Emulator detected", "Games detected",
                                                 "Built-in systems", "No launch route"};
const std::array<std::string_view, 4> icon_names = {"Gaming", "Classic mint", "Jade", "Vector (default)"};
const std::array<std::string_view, 4> icon_files = {"assets/shelf-icon.png", "assets/shelf-icon-v1.png",
                                                "assets/shelf-icon-premium-v2.png", "assets/branding/shelf-gaming-mark.png"};
void color(SDL_Color c) {
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
}
void rect(int x, int y, int w, int h, SDL_Color c) {
    color(c);
    SDL_Rect box{x, y, w, h};
    SDL_RenderFillRect(renderer, &box);
}
void line(int x1, int y1, int x2, int y2, SDL_Color c) {
    color(c);
    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
}
void rounded(int x, int y, int w, int h, int radius, SDL_Color c) {
    radius = std::min(radius, std::min(w, h) / 2);
    rect(x, y + radius, w, h - 2 * radius, c);
    color(c);
    for (int row = 0; row < radius; ++row) {
        const float dy = static_cast<float>(radius - row) - .5f;
        const int inset = radius - static_cast<int>(std::sqrt(static_cast<float>(radius * radius) - dy * dy));
        SDL_RenderDrawLine(renderer, x + inset, y + row, x + w - inset - 1, y + row);
        SDL_RenderDrawLine(renderer, x + inset, y + h - row - 1, x + w - inset - 1, y + h - row - 1);
    }
}
void box(int x, int y, int w, int h, bool selected, SDL_Color fill = panel) {
    rounded(x, y, w, h, 20, selected ? purple : SDL_Color{53, 59, 83, 255});
    const int edge = selected ? 3 : 1;
    rounded(x + edge, y + edge, w - edge * 2, h - edge * 2, 20 - edge, fill);
}
void room_wall_panel() {
    // A quiet smoked-glass backboard on the left wall, clear of both furniture and TV.
    rounded(56, 105, 1028, 277, 22, {4, 9, 8, 72});
    rounded(60, 103, 1020, 275, 20, {167, 153, 124, 62});
    rounded(61, 104, 1018, 273, 19, {13, 23, 21, 239});
    line(83, 105, 1054, 105, {206, 207, 181, 40});
}
void room_control(int x, int y, int w, int h, bool selected) {
    rounded(x, y, w, h, 14, selected ? SDL_Color{162, 221, 194, 255} : SDL_Color{83, 100, 89, 255});
    const int edge = selected ? 3 : 1;
    rounded(x + edge, y + edge, w - edge * 2, h - edge * 2, 14 - edge, {20, 31, 27, 255});
}
void settings_button(bool selected) {
    auto*& texture = settings_buttons[selected ? 1 : 0];
    if (!texture) {
        // The same rounded cog outline as the browser UI. Supersampling and
        // premultiplied coverage avoid jagged corners on a television.
        constexpr std::array<SDL_FPoint, 24> cog{
            {{9.7f, 3},   {9.1f, 5.2f},   {7.3f, 6.2f},   {5.1f, 5.6f},   {2.9f, 9.4f},   {4.5f, 11},
             {4.5f, 13},  {2.9f, 14.6f},  {5.1f, 18.4f},  {7.3f, 17.8f},  {9.1f, 18.8f},  {9.7f, 21},
             {14.3f, 21}, {14.9f, 18.8f}, {16.7f, 17.8f}, {18.9f, 18.4f}, {21.1f, 14.6f}, {19.5f, 13},
             {19.5f, 11}, {21.1f, 9.4f},  {18.9f, 5.6f},  {16.7f, 6.2f},  {14.9f, 5.2f},  {14.3f, 3}}};
        std::array<Uint32, 64 * 64> pixels{};
        for (int py = 0; py < 64; ++py)
            for (int px = 0; px < 64; ++px) {
                unsigned sums[4]{};
                for (int sy = 0; sy < 4; ++sy)
                    for (int sx = 0; sx < 4; ++sx) {
                        const float x = static_cast<float>(px) + (sx + .5f) / 4;
                        const float y = static_cast<float>(py) + (sy + .5f) / 4;
                        const float dx = std::abs(x - 32) - 13, dy = std::abs(y - 32) - 13;
                        const float ox = std::max(0.0f, dx), oy = std::max(0.0f, dy);
                        const float d = std::sqrt(ox * ox + oy * oy) + std::min(std::max(dx, dy), 0.0f) - 16;
                        SDL_Color sample{};
                        if (d <= 0) {
                            const float gradient = std::clamp(y / 64, 0.0f, 1.0f);
                            sample = {static_cast<Uint8>(29 - 9 * gradient), static_cast<Uint8>(32 - 9 * gradient),
                                      static_cast<Uint8>(52 - 11 * gradient), 255};
                            if (d >= -(selected ? 2.0f : 1.15f))
                                sample = selected ? purple : SDL_Color{65, 67, 92, 255};
                            const float gx = (x - 32) / 1.35f + 12, gy = (y - 32) / 1.35f + 12;
                            bool stroke =
                                std::abs(std::sqrt((gx - 12) * (gx - 12) + (gy - 12) * (gy - 12)) - 3.1f) <= .8f;
                            for (std::size_t i = 0; i < cog.size() && !stroke; ++i) {
                                const auto a = cog[i], b = cog[(i + 1) % cog.size()];
                                const float vx = b.x - a.x, vy = b.y - a.y;
                                const float t =
                                    std::clamp(((gx - a.x) * vx + (gy - a.y) * vy) / (vx * vx + vy * vy), 0.0f, 1.0f);
                                const float ex = gx - a.x - t * vx, ey = gy - a.y - t * vy;
                                stroke = ex * ex + ey * ey <= .64f;
                            }
                            if (stroke)
                                sample = selected ? SDL_Color{244, 237, 255, 255} : SDL_Color{198, 190, 223, 255};
                        } else if (selected && d < 3) {
                            const float falloff = 1 - d / 3;
                            sample = {203, 187, 255, static_cast<Uint8>(52 * falloff * falloff)};
                        }
                        sums[0] += sample.r * sample.a;
                        sums[1] += sample.g * sample.a;
                        sums[2] += sample.b * sample.a;
                        sums[3] += sample.a;
                    }
                const unsigned alpha = (sums[3] + 8) / 16;
                const unsigned red = sums[3] ? (sums[0] + sums[3] / 2) / sums[3] : 0;
                const unsigned green = sums[3] ? (sums[1] + sums[3] / 2) / sums[3] : 0;
                const unsigned blue = sums[3] ? (sums[2] + sums[3] / 2) / sums[3] : 0;
                pixels[static_cast<std::size_t>(py * 64 + px)] = red | (green << 8) | (blue << 16) | (alpha << 24);
            }
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, 64, 64);
        if (!texture || SDL_UpdateTexture(texture, nullptr, pixels.data(), 64 * 4) < 0) {
            ++resource_errors;
            return;
        }
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }
    const SDL_Rect bounds{1783, 24, 64, 64};
    SDL_RenderCopy(renderer, texture, nullptr, &bounds);
}
Font* font(int size, bool bold) {
    for (auto& entry : fonts)
        if (entry.texture && entry.size == size && entry.bold == bold)
            return &entry;
    Font* entry = nullptr;
    for (auto& candidate : fonts)
        if (!candidate.texture) {
            entry = &candidate;
            break;
        }
    if (!entry) {
        ++resource_errors;
        return nullptr;
    }
    char filename[2304];
    std::snprintf(filename, sizeof(filename), "%s/native/assets/fonts/Poppins-%s.ttf", root,
                  bold ? "SemiBold" : "Regular");
    FILE* file = std::fopen(filename, "rb");
    if (!file) {
        ++resource_errors;
        return nullptr;
    }
    std::fseek(file, 0, SEEK_END);
    const long length = std::ftell(file);
    std::rewind(file);
    if (length < 0 || length > 1024 * 1024) {
        std::fclose(file);
        ++resource_errors;
        return nullptr;
    }
    const int atlas = size <= 35 ? 512 : 1024;
    const auto atlas_pixels = static_cast<std::size_t>(atlas) * static_cast<std::size_t>(atlas);
    auto* bytes = static_cast<unsigned char*>(allocate(static_cast<std::size_t>(length)));
    auto* bitmap = static_cast<unsigned char*>(allocate_zero(atlas_pixels, 1));
    auto* pixels = static_cast<Uint32*>(allocate(atlas_pixels * 4));
    bool okay = bytes && bitmap && pixels;
    if (okay)
        okay = std::fread(bytes, 1, static_cast<std::size_t>(length), file) == static_cast<std::size_t>(length);
    std::fclose(file);
    if (okay)
        okay =
            stbtt_BakeFontBitmap(bytes, 0, static_cast<float>(size), bitmap, atlas, atlas, 32, 224, entry->chars) > 0;
    if (okay) {
        for (std::size_t i = 0; i < atlas_pixels; ++i)
            pixels[i] = (static_cast<Uint32>(bitmap[i]) << 24) | 0xffffffu;
        entry->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, atlas, atlas);
        if (entry->texture) {
            SDL_UpdateTexture(entry->texture, nullptr, pixels, atlas * 4);
            SDL_SetTextureBlendMode(entry->texture, SDL_BLENDMODE_BLEND);
            entry->size = size;
            entry->bold = bold;
        }
    }
    release(bytes);
    release(bitmap);
    release(pixels);
    if (!entry->texture)
        ++resource_errors;
    return entry->texture ? entry : nullptr;
}
unsigned codepoint(std::string_view text, std::size_t& at) {
    const auto c = static_cast<unsigned char>(text[at++]);
    if (c < 128)
        return c;
    if ((c & 0xe0) == 0xc0 && at < text.size())
        return ((c & 31u) << 6) | (static_cast<unsigned char>(text[at++]) & 63u);
    if ((c & 0xf0) == 0xe0 && at + 1 < text.size()) {
        const unsigned a = static_cast<unsigned char>(text[at++]), b = static_cast<unsigned char>(text[at++]);
        return ((c & 15u) << 12) | ((a & 63u) << 6) | (b & 63u);
    }
    return '?';
}
int text_width(std::string_view text, int size, bool bold = false) {
    auto* f = font(size, bold);
    if (!f)
        return 0;
    float advance = 0;
    for (std::size_t at = 0; at < text.size();) {
        unsigned c = codepoint(text, at);
        if (c < 32 || c > 255)
            c = '?';
        advance += f->chars[c - 32].xadvance;
    }
    return static_cast<int>(advance);
}
void text(std::string_view value, int x, int y, int size, SDL_Color tint = white, bool bold = false, int limit = 1800) {
    auto* f = font(size, bold);
    if (!f)
        return;
    SDL_SetTextureColorMod(f->texture, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(f->texture, tint.a);
    float cursor = static_cast<float>(x);
    for (std::size_t at = 0; at < value.size();) {
        unsigned c = codepoint(value, at);
        if (c < 32 || c > 255)
            c = '?';
        const auto& g = f->chars[c - 32];
        if (cursor + g.xadvance > static_cast<float>(x + limit))
            break;
        SDL_Rect source{g.x0, g.y0, g.x1 - g.x0, g.y1 - g.y0};
        SDL_Rect dest{static_cast<int>(cursor + g.xoff), y + size + static_cast<int>(g.yoff), source.w, source.h};
        SDL_RenderCopy(renderer, f->texture, &source, &dest);
        cursor += g.xadvance;
    }
}
void centered(std::string_view value, int x, int y, int size, SDL_Color tint = white, bool bold = false) {
    text(value, x - text_width(value, size, bold) / 2, y, size, tint, bold);
}
Art* image(std::string_view name, bool monochrome = false) {
    ++art_age;
    for (auto& entry : art)
        if (std::string_view(entry.name) == name && entry.monochrome == monochrome) {
            entry.age = art_age;
            return &entry;
        }
    auto* slot = &art[0];
    for (auto& candidate : art)
        if (candidate.age < slot->age)
            slot = &candidate;
    if (slot->texture)
        SDL_DestroyTexture(slot->texture);
    slot->texture = nullptr;
    std::snprintf(slot->name, sizeof(slot->name), "%.*s", static_cast<int>(name.size()), name.data());
    slot->age = art_age;
    slot->monochrome = monochrome;
    char filename[2304];
    const bool custom = name == "@room" || (name.size() > 7 && std::string_view(name.data(), 7) == "@intro/" &&
                         find_system(std::string_view(name.data() + 7, name.size() - 7)) < system_count);
    if (name == "@room") std::snprintf(filename, sizeof(filename), "%s/tv-image.png", room_data);
    else if (custom) std::snprintf(filename, sizeof(filename), "%s/intros/%s.png", room_data, slot->name + 7);
    else std::snprintf(filename, sizeof(filename), "%s/%s", root, slot->name);
    if (custom) {
        int iw = 0, ih = 0, ic = 0;
        if (!room_data[0] || !stbi_info(filename, &iw, &ih, &ic) || iw < 1 || ih < 1 || iw > 2048 || ih > 2048)
            return slot;
    }
    int channels = 0;
    unsigned char* data = stbi_load(filename, &slot->w, &slot->h, &channels, 4);
    if (data && slot->w > 0 && slot->h > 0 && slot->w <= 4096 && slot->h <= 4096) {
        int left = slot->w, top = slot->h, right = -1, bottom = -1;
        slot->support_y = 0;
        for (int y = 0; y < slot->h; ++y) {
            int solid = 0;
            for (int x = 0; x < slot->w; ++x) {
                const auto alpha = data[(static_cast<std::size_t>(y) * slot->w + x) * 4 + 3];
                if (alpha > 8) {
                    left = std::min(left, x);
                    top = std::min(top, y);
                    right = std::max(right, x);
                    bottom = std::max(bottom, y);
                }
                if (alpha >= 192)
                    ++solid;
            }
            if (solid >= 3)
                slot->support_y = y + 1;
        }
        if (!slot->support_y)
            slot->support_y = bottom >= 0 ? bottom + 1 : slot->h;
        if (right >= 0) {
            left = std::max(0, left - 12);
            top = std::max(0, top - 12);
            right = std::min(slot->w - 1, right + 12);
            bottom = std::min(slot->h - 1, bottom + 12);
            slot->bounds = {left, top, right - left + 1, bottom - top + 1};
        } else {
            slot->bounds = {0, 0, slot->w, slot->h};
        }
        if (monochrome)
            for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(slot->w) * slot->h; ++pixel) {
                auto* rgba = data + pixel * 4;
                const auto grey = static_cast<unsigned char>((54u * rgba[0] + 183u * rgba[1] + 19u * rgba[2]) / 256u);
                rgba[0] = rgba[1] = rgba[2] = grey;
            }
        slot->texture =
            SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, slot->w, slot->h);
        if (slot->texture) {
            SDL_UpdateTexture(slot->texture, nullptr, data, slot->w * 4);
            SDL_SetTextureBlendMode(slot->texture, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(slot->texture, SDL_ScaleModeLinear);
        }
    }
    stbi_image_free(data);
    if (!slot->texture && !custom)
        ++resource_errors;
    return slot;
}
void picture(std::string_view name, int x, int y, int w, int h, Uint8 alpha = 255, bool monochrome = false) {
    auto* entry = image(name, monochrome);
    if (!entry || !entry->texture)
        return;
    const float scale = std::min(static_cast<float>(w) / static_cast<float>(entry->w),
                                 static_cast<float>(h) / static_cast<float>(entry->h));
    const int dw = static_cast<int>(static_cast<float>(entry->w) * scale),
              dh = static_cast<int>(static_cast<float>(entry->h) * scale);
    SDL_Rect dest{x + (w - dw) / 2, y + (h - dh) / 2, dw, dh};
    SDL_SetTextureAlphaMod(entry->texture, alpha);
    SDL_RenderCopy(renderer, entry->texture, nullptr, &dest);
}
void shelf_picture(std::string_view name, int cx, int base, int w, int h, bool selected, int floor_top, int floor_end,
                   bool monochrome, Uint8 alpha = 255, float display_scale = 1.f) {
    auto* entry = image(name, monochrome);
    if (!entry || !entry->texture)
        return;
    const auto& bounds = entry->bounds;
    const float fit = std::min(static_cast<float>(w) / bounds.w, static_cast<float>(h) / bounds.h) *
                      (selected ? 1.045f : 1.f) * display_scale;
    const int dw = static_cast<int>(bounds.w * fit), dh = static_cast<int>(bounds.h * fit);
    const int feet = static_cast<int>((entry->support_y - bounds.y) * fit);
    SDL_Rect dest{cx - dw / 2, base - feet, dw, dh};
    // Use the illustration's silhouette so both the rear console and its lower
    // controller cast a contact shadow, each at its own height on the wood.
    // Clipping to the horizontal surface keeps shadows off the wall/fascia.
    SDL_Rect floor{80, floor_top, 1760, floor_end - floor_top};
    SDL_RenderSetClipRect(renderer, &floor);
    SDL_SetTextureColorMod(entry->texture, 0, 0, 0);
    SDL_Rect shadow = dest;
    shadow.y += 7;
    SDL_SetTextureAlphaMod(entry->texture, static_cast<Uint8>(45u * alpha / 255u));
    SDL_RenderCopy(renderer, entry->texture, &bounds, &shadow);
    shadow.y -= 4;
    SDL_SetTextureAlphaMod(entry->texture, static_cast<Uint8>(105u * alpha / 255u));
    SDL_RenderCopy(renderer, entry->texture, &bounds, &shadow);
    SDL_RenderSetClipRect(renderer, nullptr);
    SDL_SetTextureColorMod(entry->texture, 255, 255, 255);
    // Solid hardware stays opaque; selection uses scale/light rather than
    // allowing the shelf's back edge to show through the console body.
    SDL_SetTextureAlphaMod(entry->texture, alpha);
    SDL_RenderCopy(renderer, entry->texture, &bounds, &dest);
}
void orbit_picture(std::string_view name, double cx, double cy, double w, double h, double yaw, Uint8 alpha, bool monochrome) {
    auto* entry = image(name, monochrome);
    if (!entry || !entry->texture)
        return;
    const auto& bounds = entry->bounds;
    const double fit = std::min(w / bounds.w, h / bounds.h);
    const double hw = bounds.w * fit / 2, hh = bounds.h * fit / 2;
    const double angle = yaw * 3.141592653589793 / 180;
    SDL_Vertex vertices[4]{};
    for (int i = 0; i < 4; ++i) {
        const double x = (i == 0 || i == 3) ? -hw : hw;
        const double y = i < 2 ? -hh : hh;
        const double projection = 1200 / (1200 + x * std::sin(angle));
        vertices[i].position = {static_cast<float>(cx + x * std::cos(angle) * projection),
                                static_cast<float>(cy + y * projection)};
        vertices[i].color = {255, 255, 255, alpha};
        vertices[i].tex_coord = {static_cast<float>(bounds.x + (i == 0 || i == 3 ? 0 : bounds.w)) / entry->w,
                                 static_cast<float>(bounds.y + (i < 2 ? 0 : bounds.h)) / entry->h};
    }
    const int indices[] = {0, 1, 2, 0, 2, 3};
    if (SDL_RenderGeometry(renderer, entry->texture, vertices, 4, indices, 6) < 0)
        ++resource_errors;
}
SDL_Color accent(const SystemDefinition& s) {
    if (s.brand == "Nintendo")
        return {190, 92, 150, 255};
    if (s.brand == "Sega")
        return {70, 140, 230, 255};
    if (s.brand == "PlayStation")
        return {131, 119, 234, 255};
    return {86, 180, 163, 255};
}
void glow(int x, int y, int w, int h, SDL_Color tint, Uint8 alpha = 100) {
    SDL_Rect dest{x, y, w, h};
    SDL_SetTextureColorMod(halo, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(halo, alpha);
    SDL_RenderCopy(renderer, halo, nullptr, &dest);
}
void icon(int kind, int x, int y, SDL_Color tint = purple) {
    if (kind == 0) {
        for (int a = 0; a < 360; a += 8) {
            const double r = static_cast<double>(a) * 3.141592653589793 / 180;
            rect(x + static_cast<int>(std::cos(r) * 10), y + static_cast<int>(std::sin(r) * 10), 2, 2, tint);
        }
        line(x + 8, y + 8, x + 16, y + 16, tint);
    }
    if (kind == 1) {
        line(x - 13, y - 7, x + 13, y - 7, tint);
        line(x - 13, y + 7, x + 13, y + 7, tint);
        rounded(x - 7, y - 12, 9, 10, 4, tint);
        rounded(x + 2, y + 2, 9, 10, 4, tint);
    }
    if (kind == 2) {
        for (int a = 0; a < 4; ++a)
            rounded(x - 13 + (a % 2) * 15, y - 13 + (a / 2) * 15, 10, 10, 3, tint);
    }
    if (kind == 3) {
        // A continuous cog outline, not a sun/light-mode symbol.
        int previous_x = 0, previous_y = 0;
        for (int a = 0; a <= 32; ++a) {
            const double angle = static_cast<double>(a) * 3.141592653589793 / 16;
            const int radius = a % 4 == 0 || a % 4 == 3 ? 15 : 11;
            const int px = x + static_cast<int>(std::cos(angle) * radius);
            const int py = y + static_cast<int>(std::sin(angle) * radius);
            if (a)
                line(previous_x, previous_y, px, py, tint);
            previous_x = px;
            previous_y = py;
        }
        for (int a = 0; a < 360; a += 8) {
            const double r = static_cast<double>(a) * 3.141592653589793 / 180;
            rect(x + static_cast<int>(std::cos(r) * 5), y + static_cast<int>(std::sin(r) * 5), 2, 2, tint);
        }
    }
    if (kind == 4) {
        rounded(x - 5, y - 13, 10, 26, 3, tint);
        rounded(x - 15, y - 8, 6, 16, 2, tint);
        rounded(x + 9, y - 8, 6, 16, 2, tint);
    }
}
void switch_control(int x, int y, bool on) {
    rounded(x, y, 58, 30, 15, on ? purple : SDL_Color{62, 72, 100, 255});
    rounded(x + (on ? 31 : 4), y + 4, 22, 22, 11, on ? SDL_Color{38, 28, 65, 255} : muted);
}
void label(int x, int y, std::string_view heading, std::string_view value, bool selected, int w = 1032) {
    box(x, y, w, 86, selected);
    text(heading, x + 22, y + 12, 21, muted);
    text(value, x + 22, y + 39, 27, white, true, w - 80);
    text("<  >", x + w - 94, y + 29, 24, selected ? purple : muted);
}
void footer(std::string_view hint) {
    rect(0, 1015, width, 65, {9, 12, 26, 245});
    text(hint, 80, 1032, 21, muted);
}
void splash(float opacity, unsigned time_ms, bool loading, float appearance = 1, AppIcon icon = AppIcon::vector) {
    const auto alpha = static_cast<Uint8>(std::clamp(opacity, 0.0f, 1.0f) * 255);
    rect(0, 0, width, height, {11, 13, 28, alpha});
    const auto ink = alpha;
    const int rise = static_cast<int>(12 * (1 - appearance));
    const int side = static_cast<int>(116 * (.92f + .08f * appearance));
    picture(icon_files[static_cast<std::size_t>(icon)], 960 - side / 2, 347 + rise, side, side, ink);
    centered("isShelf", 960, 505 + rise, 48, {241, 240, 251, ink}, true);
    centered(loading ? "Finding your emulators and games" : "Your systems, together", 960, 578 + rise, 23,
             {180, 174, 205, ink});
    if (loading) {
        for (unsigned dot = 0; dot < 3; ++dot) {
            const auto distance = (dot + 3 - time_ms / 340 % 3) % 3;
            rounded(931 + static_cast<int>(dot) * 24, 639, 9, 9, 4,
                    {194, 177, 255, static_cast<Uint8>(ink / (distance + 1))});
        }
        char hint[80];
        std::snprintf(hint, sizeof(hint), "Hold Circle for %u seconds to close", close_hold_ms / 1000);
        centered(hint, 960, 984, 19, {140, 140, 165, alpha});
    }
}
std::string_view availability(const UiState& state, std::size_t id) {
    const auto& a = state.inventory[id];
    if (state.library && !state.library->scan.complete && !state.library->scan.roots_read)
        return "Library connection unavailable";
    if (a.built_in)
        return "Built-in system";
    if (a.launch == LaunchCapability::direct_game)
        return "Direct game route";
    if (a.launch == LaunchCapability::emulator_menu)
        return "Opens emulator menu";
    if (a.emulator_count)
        return "Emulator detected";
    return "No emulator detected";
}
void tv_lines(std::string_view value, int x, int y, int w, int size, SDL_Color tint = white) {
    if (text_width(value, size, true) <= w) { centered(value, x + w / 2, y, size, tint, true); return; }
    std::size_t cut = value.size();
    while (cut && text_width(std::string_view(value.data(), cut), size, true) > w) --cut;
    const auto space = std::string_view(value.data(), cut).find_last_of(' ');
    if (space != std::string_view::npos && space > cut / 2) cut = space;
    text(std::string_view(value.data(), cut), x, y, size, tint, true, w);
    if (cut < value.size()) {
        if (value[cut] == ' ') ++cut;
        text(std::string_view(value.data() + cut, value.size() - cut), x, y + size + 7, size, tint, true, w);
    }
}
void tv_static(const SDL_Rect& target, bool still) {
    static unsigned last_phase = ~0u;
    const unsigned phase = still ? 0 : render_time / 50;
    const int w = std::min(960, std::max(1, target.w)), h = std::min(540, std::max(1, target.h));
    if (noise_width != w || noise_height != h) {
        if (noise_texture) SDL_DestroyTexture(noise_texture);
        release(noise_pixels);
        noise_pixels = static_cast<Uint32*>(allocate(static_cast<std::size_t>(w) * h * 4));
        noise_texture = noise_pixels ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, w, h) : nullptr;
        noise_width = w; noise_height = h;
        last_phase = ~0u;
    }
    if (!noise_texture || !noise_pixels) return;
    if (phase != last_phase) {
        unsigned random = phase + 4711;
        for (int row = 0; row < h; ++row) {
            const int shade = ((static_cast<unsigned>(row) + phase * 2) % 216 < 24 ? 5 : 0) + (row % 2 ? 3 : 0);
            for (int col = 0; col < w; ++col) {
                random ^= random << 13; random ^= random >> 17; random ^= random << 5;
                const Uint32 level = 62 + ((random & 255) + ((random >> 8) & 255)) * 82 / 510 - shade;
                noise_pixels[row * w + col] = 0xff000000u | level | (level << 8) | (level << 16);
            }
        }
        SDL_UpdateTexture(noise_texture, nullptr, noise_pixels, w * 4);
        last_phase = phase;
    }
    SDL_RenderCopy(renderer, noise_texture, nullptr, &target);
}
bool tv_video(const SystemDefinition& system, std::size_t id, const SDL_Rect& target, bool still) {
    bool changed = false;
    if (movie_selected != id) {
        stop_movie(); movie_selected = id; movie_demo = false;
        char path[4096];
        std::snprintf(path, sizeof(path), "%s/intros/%.*s.mpg", room_data, static_cast<int>(system.id.size()), system.id.data());
        bool loaded = room_data[0] && tv_movie.open(path, render_time);
        if (!loaded) {
            std::snprintf(path, sizeof(path), "%s/assets/room/intros/%.*s.mpg", root, static_cast<int>(system.id.size()), system.id.data());
            loaded = tv_movie.open(path, render_time);
        }
        if (!loaded) {
            std::snprintf(path, sizeof(path), "%s/assets/room/shelf-intro.mpg", root);
            loaded = tv_movie.open(path, render_time); movie_demo = true;
        }
        if (loaded) {
            movie_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING,
                                               tv_movie.width(), tv_movie.height());
            changed = true;
        }
    }
    if (!movie_texture) return false;
    changed = tv_movie.advance(render_time, still) || changed;
    if (changed) SDL_UpdateTexture(movie_texture, nullptr, tv_movie.pixels(), tv_movie.width() * 4);
    const float fit = std::min(static_cast<float>(target.w) / tv_movie.width(), static_cast<float>(target.h) / tv_movie.height());
    const int w = static_cast<int>(tv_movie.width() * fit), h = static_cast<int>(tv_movie.height() * fit);
    SDL_Rect fitted{target.x + (target.w - w) / 2, target.y + (target.h - h) / 2, w, h};
    SDL_RenderCopy(renderer, movie_texture, nullptr, &fitted);
    tv_motion = !still && tv_movie.playing();
    if (movie_demo) {
        rect(target.x, target.y + target.h - 30, target.w, 30, {4, 12, 15, 220});
        centered("isShelf demo - add your own console intro", target.x + target.w / 2, target.y + target.h - 24, 16, muted);
    }
    return true;
}
void room_tv(const UiState& state) {
    const auto& p = state.settings.preferences;
    tv_motion = false;
    if (p.tv_mode == TvMode::off) { stop_movie(); return; }
    const int x = room_x(static_cast<float>(room_scene::tv_x)), y = room_y(static_cast<float>(room_scene::tv_y));
    const int w = static_cast<int>(room_scene::tv_width * room_scale()), h = static_cast<int>(room_scene::tv_height * room_scale());
    SDL_Rect clip{x, y, w, h};
    SDL_RenderSetClipRect(renderer, &clip);
    rect(x, y, w, h, {4, 12, 15, 238});
    const auto id = state.selected_system;
    const auto& system = catalog[id];
    const bool eligible = p.intro_all || state.inventory[id].built_in || state.inventory[id].emulator_count;
    const bool noise = tv_noise_for(p, state.inventory[id]);
    if (p.tv_mode != TvMode::video || !eligible || noise) stop_movie();
    if (tv_selected != id) { tv_selected = id; tv_changed_at = render_time; }
    const float reveal = p.tv_mode == TvMode::intro && !p.reduced_motion ?
                         std::min(1.f, static_cast<unsigned>(render_time - tv_changed_at) / 420.f) : 1;
    tv_motion = reveal < 1;
    const auto alpha = static_cast<Uint8>(225 * reveal);
    bool pictured = false;
    if (noise) { tv_static(clip, p.reduced_motion); tv_motion = !p.reduced_motion; pictured = true; }
    if (!noise && p.tv_mode == TvMode::video && eligible)
        pictured = tv_video(system, id, clip, p.reduced_motion || state.presentation.loading);
    if (!noise && (p.tv_mode == TvMode::picture || p.tv_mode == TvMode::intro)) {
        char intro[96]; std::snprintf(intro, sizeof(intro), "@intro/%.*s", static_cast<int>(system.id.size()), system.id.data());
        auto* custom = (p.tv_mode == TvMode::picture || eligible) ? image(p.tv_mode == TvMode::picture ? std::string_view("@room") : std::string_view(intro)) : nullptr;
        if (p.tv_mode == TvMode::intro && eligible && (!custom || !custom->texture)) {
            std::snprintf(intro, sizeof(intro), "assets/room/intros/%.*s.png", static_cast<int>(system.id.size()), system.id.data());
            custom = image(intro);
        }
        if (custom && custom->texture) {
            SDL_Rect source{0, 0, custom->w, custom->h};
            const float fit = std::max(static_cast<float>(w) / custom->w, static_cast<float>(h) / custom->h);
            source.w = static_cast<int>(w / fit); source.h = static_cast<int>(h / fit);
            source.x = (custom->w - source.w) / 2; source.y = (custom->h - source.h) / 2;
            SDL_SetTextureColorMod(custom->texture, 255, 255, 255);
            SDL_SetTextureAlphaMod(custom->texture, alpha);
            SDL_RenderCopy(renderer, custom->texture, &source, &clip);
            pictured = true;
        }
        if (p.tv_mode == TvMode::intro && !eligible) {
            picture(icon_files[static_cast<std::size_t>(p.icon)], x + w / 2 - 38, y + 75, 76, 76, 180);
            centered("Your play room", x + w / 2, y + 174, 25, {205, 224, 221, 200}, true);
            pictured = true;
        }
    }
    if (!pictured && (p.tv_mode == TvMode::console || p.tv_mode == TvMode::intro || p.tv_mode == TvMode::video)) {
        picture(system.image, x + w / 2 - 140, y + 21, 280, h - 97, alpha, use_monochrome(p, state.inventory[id], system.id));
        tv_lines(system.name, x + 32, y + h - 79, w - 64, 25, {233, 249, 246, alpha});
        if (p.tv_mode == TvMode::console) {
            char counts[96]; std::snprintf(counts, sizeof(counts), "%zu games  /  %zu emulator routes",
                                         state.inventory[id].game_count, state.inventory[id].emulator_count);
            centered(counts, x + w / 2, y + h - 39, 17, {166, 202, 197, alpha});
        } else centered(eligible ? "Add your own intro in Play room" : "Intro set to installed consoles only",
                        x + w / 2, y + h - 39, 17, {166, 202, 197, alpha});
    } else if (!pictured && (p.tv_mode == TvMode::picture || p.tv_mode == TvMode::message)) {
        picture(icon_files[static_cast<std::size_t>(p.icon)], x + w / 2 - 31, y + 33, 62, 62, 210);
        tv_lines(state.settings.tv_text, x + 42, y + 129, w - 84, 31, {230, 248, 243, 230});
        if (p.tv_mode == TvMode::picture)
            centered("Add tv-image.png to your isShelf data folder", x + w / 2, y + h - 38, 17, muted);
    } else if (!pictured && p.tv_mode == TvMode::clock) {
        const auto now = std::time(nullptr); const auto* local = std::localtime(&now);
        char clock[16] = "--:--", date[80]{};
        if (local) { std::strftime(clock, sizeof(clock), "%H:%M", local); std::strftime(date, sizeof(date), "%A, %d %B", local); }
        picture(icon_files[static_cast<std::size_t>(p.icon)], x + w / 2 - 27, y + 22, 54, 54, 190);
        centered(clock, x + w / 2, y + 88, 66, {229, 249, 244, 230}, true);
        centered(date, x + w / 2, y + 172, 20, {180, 212, 204, 230});
        tv_lines(state.settings.tv_text, x + 42, y + 222, w - 84, 18, {180, 212, 204, 200});
    } else if (!pictured && p.tv_mode == TvMode::favorites) {
        static const Library* previous_library = nullptr;
        static std::size_t previous_count = 0, count = 0;
        static std::array<std::uint64_t, 64> previous_keys{};
        static std::array<std::size_t, 64> games{};
        if (previous_library != state.library || previous_count != (state.library ? state.library->game_count : 0) ||
            previous_keys != state.settings.game_favorites) {
            previous_library = state.library; previous_count = state.library ? state.library->game_count : 0;
            previous_keys = state.settings.game_favorites; count = 0;
            for (std::size_t i = 0; state.library && i < state.library->game_count && count < games.size(); ++i) {
                const auto key = game_favorite_key(state.library->games[i]);
                if (std::find(previous_keys.begin(), previous_keys.end(), key) != previous_keys.end()) games[count++] = i;
            }
        }
        if (count) {
            const auto& game = state.library->games[games[(render_time / (tv_periods[p.tv_period] * 1000u)) % count]];
            picture(catalog[game.system].image, x + w / 2 - 115, y + 15, 230, 150, 215);
            tv_lines(game.name, x + 35, y + 176, w - 70, 25, {233, 249, 246, 230});
            centered("FROM YOUR FAVOURITES", x + w / 2, y + h - 31, 15, {161, 207, 195, 210}, true);
        } else {
            picture(icon_files[static_cast<std::size_t>(p.icon)], x + w / 2 - 30, y + 41, 60, 60, 200);
            centered("Your favourite games", x + w / 2, y + 136, 27, {233, 249, 246, 230}, true);
            centered("Press Triangle on a game in isShelf's library", x + w / 2, y + 193, 19, {180, 212, 204, 230});
        }
    }
    // Glass, a soft upper reflection and edge falloff keep content inside the photographed screen.
    for (int row = 0; row < 18; ++row) rect(x, y + row, w, 1, {202, 224, 219, static_cast<Uint8>(20 - row)});
    const SDL_Vertex reflection[] = {{{static_cast<float>(x), static_cast<float>(y)}, {221, 232, 228, 13}, {0, 0}},
                                    {{static_cast<float>(x + w * 3 / 5), static_cast<float>(y)}, {221, 232, 228, 5}, {0, 0}},
                                    {{static_cast<float>(x + w / 3), static_cast<float>(y + h)}, {221, 232, 228, 0}, {0, 0}},
                                    {{static_cast<float>(x), static_cast<float>(y + h)}, {221, 232, 228, 7}, {0, 0}}};
    const int triangles[] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(renderer, nullptr, reflection, 4, triangles, 6);
    line(x, y + h - 1, x + w, y + h - 1, {0, 0, 0, 120});
    SDL_RenderSetClipRect(renderer, nullptr);
}
void home(const UiState& state) {
    const auto shown = shown_systems(state);
    const auto& p = state.settings.preferences;
    const bool room = p.layout == Layout::shelf;
    if (room) room_wall_panel();
    text("ONE HOME FOR YOUR SYSTEMS", 80, 119, 19, room ? SDL_Color{176, 215, 190, 255} : purple, true);
    if (room)
        text("Your console collection", 80, 154, 50, white, true);
    else {
        text("What do you want", 80, 153, 59, white, true);
        text("to play today?", 80, 218, 59, purple, true);
    }
    const int tool_y = room ? 244 : 314, summary_y = room ? 316 : 391;
    if (room) room_control(80, tool_y, 780, 66, state.tools && state.tool == 0);
    else box(80, tool_y, 1560, 66, state.tools && state.tool == 0);
    icon(0, 111, tool_y + 32, room ? SDL_Color{185, 205, 193, 255} : muted);
    text(state.query[0] ? state.query : "Find a console, brand or core...", 143, tool_y + 16, 24,
         state.query[0] ? white : room ? SDL_Color{185, 205, 193, 255} : muted, false, room ? 690 : 1420);
    const int layout_x = room ? 880 : 1664;
    if (room) room_control(layout_x, tool_y, 180, 66, state.tools && state.tool == 1);
    else box(layout_x, tool_y, 180, 66, state.tools && state.tool == 1);
    icon(p.layout == Layout::grid ? 4 : 2, layout_x + 32, tool_y + 33);
    text(layout_labels[static_cast<std::size_t>(next_layout(p.layout))], layout_x + 58, tool_y + 17, 24, white, true);
    char summary[160];
    std::snprintf(summary, sizeof(summary), "%s  /  %.*s  /  %zu systems", layout_labels[static_cast<std::size_t>(p.layout)].data(),
                  static_cast<int>(sorts[static_cast<std::size_t>(p.sort)].size()),
                  sorts[static_cast<std::size_t>(p.sort)].data(), shown.count);
    text(summary, 84, summary_y, 20, room ? SDL_Color{190, 205, 196, 255} : muted);
    if (!shown.count) {
        centered("Your shelf is ready for your choices", 960, 620, 42, white, true);
        centered(p.layout == Layout::shelf && p.shelf_single ? "Choose up to 8 consoles in Settings > Consoles." :
                 p.scope == Scope::favorites ? "Add favourites with Triangle, or change the view in Settings." :
                 "Choose All consoles or enable systems in settings.", 960, 679, 26, muted);
        footer("Cross  Choose consoles    Options  Settings    Circle  Back");
        return;
    }
    if (p.layout == Layout::grid) {
        const int base = (state.focus / 8) * 8;
        for (int slot = 0; slot < 8; ++slot) {
            const int at = base + slot;
            if (at >= static_cast<int>(shown.count))
                break;
            const auto id = shown.indices[static_cast<std::size_t>(at)];
            const auto& s = catalog[id];
            const int x = 80 + (slot % 4) * 442, y = 442 + (slot / 4) * 280;
            const bool active = !state.tools && at == state.focus;
            box(x, y, 416, 258, active, {23, 27, 47, 255});
            const bool monochrome = use_monochrome(p, state.inventory[id], s.id);
            glow(x + 30, y + 2, 350, 200, monochrome ? SDL_Color{128, 139, 154, 255} : accent(s), active ? 120 : 62);
            if (p.grid_look == GridLook::icons)
                picture(s.image, x + 20, y + 7, 376, 166, 255, monochrome);
            else {
                text(s.brand, x + 22, y + 25, 22, purple, true);
                text("CONSOLE", x + 22, y + 70, 38, muted, true);
            }
            text(s.name, x + 20, y + 173, 26, white, true, 374);
            text(availability(state, id), x + 20, y + 212, 19, muted, false, 374);
            if (p.favorite[id])
                text("*", x + 377, y + 12, 30, purple, true);
        }
    } else if (p.layout == Layout::shelf) {
        const int first = shelf_motion.page() * 8;
        const int offset = static_cast<int>(shelf_motion.offset());
        const auto opacity = static_cast<Uint8>(255 * shelf_motion.opacity());
        for (int tier = 0; tier < 2; ++tier) {
            if (first + tier * 4 >= static_cast<int>(shown.count))
                break;
            const auto& plane = room_scene::tiers[static_cast<std::size_t>(tier)];
            const int contact = room_y(static_cast<float>(plane.feet));
            const int art_height = static_cast<int>(plane.art_height * room_scale());
            const float column = static_cast<float>(room_scene::right - room_scene::left) / room_scene::columns;
            for (int col = 0; col < 4; ++col) {
                const int at = first + tier * 4 + col;
                if (at >= static_cast<int>(shown.count))
                    break;
                const auto id = shown.indices[static_cast<std::size_t>(at)];
                const auto& system = catalog[id];
                const int cx = room_x(room_scene::left + (col + .5f) * column);
                const bool active = !state.tools && at == state.focus;
                const int hardware_width = static_cast<int>(280 * system.room_scale);
                const int hardware_height = static_cast<int>(art_height * system.room_scale);
                if (active)
                    glow(cx + offset - hardware_width / 2, contact - hardware_height, hardware_width, hardware_height + 10, {125, 223, 211, 255},
                         static_cast<Uint8>(48u * opacity / 255u));
                const int shadow_width = static_cast<int>(222 * system.room_scale);
                glow(cx + offset - shadow_width / 2, contact - 11, shadow_width, 22, {0, 0, 0, 255}, static_cast<Uint8>(225u * opacity / 255u));
                shelf_picture(system.image, cx + offset, contact, static_cast<int>(column * room_scale() * .83f), art_height,
                              active, room_y(static_cast<float>(plane.wood_top)), room_y(static_cast<float>(plane.wood_end)),
                              use_monochrome(p, state.inventory[id], system.id), opacity, system.room_scale);
                if (active)
                    rounded(cx - 31, contact + 8, 62, 3, 1, {150, 235, 219, opacity});
                const int label_y = room_y(static_cast<float>(plane.label));
                centered(system.name, cx + 1, label_y + 2, 20, {0, 0, 0, static_cast<Uint8>(200u * opacity / 255u)}, true);
                centered(system.name, cx, label_y, 20, active ? SDL_Color{white.r, white.g, white.b, opacity} :
                         SDL_Color{219, 225, 238, opacity}, true);
                if (p.favorite[id])
                    text("*", cx + offset + 130, contact - art_height, 25, {purple.r, purple.g, purple.b, opacity}, true);
            }
        }
        char position[64];
        std::snprintf(position, sizeof(position), "%d-%d / %zu", first + 1,
                      std::min(first + 8, static_cast<int>(shown.count)), shown.count);
        text(availability(state, state.selected_system), 84, summary_y + 29, 19, {190, 205, 196, 255}, false, 500);
        text(position, 912, summary_y, 19, {190, 205, 196, 255});
        char pages[96];
        std::snprintf(pages, sizeof(pages), "%s  %d / %d  %s", state.focus >= 8 ? "L1  <" : "",
                      state.focus / 8 + 1, (static_cast<int>(shown.count) + 7) / 8,
                      state.focus / 8 < (static_cast<int>(shown.count) - 1) / 8 ? ">  R1" : "");
        rounded(790, 919, 340, 52, 22, {12, 19, 20, 220});
        centered(pages, 960, 934, 22, white, true);
    } else {
        const auto& selected = catalog[state.selected_system];
        glow(610, 742, 700, 130, accent(selected), 65);
        // A quiet elliptical floor anchors the curved collection without cards.
        for (int degree = 0; degree < 360; degree += 3) {
            const double a = degree * 3.141592653589793 / 180;
            const double b = (degree + 3) * 3.141592653589793 / 180;
            line(960 + static_cast<int>(std::cos(a) * 770), 748 + static_cast<int>(std::sin(a) * 66),
                 960 + static_cast<int>(std::cos(b) * 770), 748 + static_cast<int>(std::sin(b) * 66),
                 {98, 101, 143, 19});
        }
        struct Slot {
            std::size_t id;
            double offset;
        };
        std::array<Slot, 9> slots{};
        std::size_t count = 0;
        for (std::size_t i = 0; i < shown.count; ++i) {
            const double offset =
                orbit_distance(static_cast<double>(i) - orbit.position, static_cast<double>(shown.count));
            if (std::abs(offset) < 4 && count < slots.size())
                slots[count++] = {shown.indices[i], offset};
        }
        std::sort(slots.begin(), slots.begin() + count,
                  [](const Slot& a, const Slot& b) { return std::abs(a.offset) > std::abs(b.offset); });
        SDL_Rect clip{80, 432, 1760, 420};
        SDL_RenderSetClipRect(renderer, &clip);
        for (std::size_t i = 0; i < count; ++i) {
            const auto pose = orbit_pose(slots[i].offset, 1760);
            const double projection = 1200 / (1200 - pose.z), scale = pose.scale * projection;
            orbit_picture(catalog[slots[i].id].image, 960 + pose.x * projection, 630 + pose.y * projection, 320 * scale,
                          310 * scale, pose.yaw, static_cast<Uint8>(255 * pose.opacity),
                          use_monochrome(p, state.inventory[slots[i].id], catalog[slots[i].id].id));
        }
        SDL_RenderSetClipRect(renderer, nullptr);
        centered(selected.brand, 960, 845, 18, muted);
        centered(selected.name, 960, 873, 38, white, true);
        centered(availability(state, state.selected_system), 960, 930, 21, muted);
        char position[48];
        std::snprintf(position, sizeof(position), "%d / %zu", state.focus + 1, shown.count);
        text(position, 1730, 391, 20, muted);
    }
    footer("D-pad / Stick  Browse    Cross  Open    Triangle  Favourite    Square  Layout    Options  Settings    "
           "Circle  Back");
}
void detail(const UiState& state) {
    const auto id = state.selected_system;
    const auto& s = catalog[id];
    text("YOUR SYSTEM COLLECTION", 80, 145, 20, purple, true);
    text(s.name, 80, 194, 58, white, true, 1100);
    text(s.brand, 84, 275, 27, muted);
    if (state.settings.preferences.layout == Layout::shelf) {
        const auto& plane = room_scene::tiers[0];
        const int contact = room_y(static_cast<float>(plane.feet));
        shelf_picture(s.image, room_x(1320), contact, 310, static_cast<int>(plane.art_height * room_scale()), false,
                      room_y(static_cast<float>(plane.wood_top)), room_y(static_cast<float>(plane.wood_end)),
                       use_monochrome(state.settings.preferences, state.inventory[id], s.id), 255, s.room_scale);
    } else {
        glow(1190, 230, 640, 630, accent(s), 110);
        picture(s.image, 1160, 205, 640, 640, 255, use_monochrome(state.settings.preferences, state.inventory[id], s.id));
    }
    text(availability(state, id), 84, 366, 29, purple, true);
    char count[80];
    std::snprintf(count, sizeof(count), "%zu discovered games", state.inventory[id].game_count);
    if (state.page != Page::library)
        text(count, 84, 415, 24, muted);
    if (state.page == Page::library) {
        const auto* route = state.library ? preferred_route(*state.library, id, state.settings.preferred_emulator[id].data()) : nullptr;
        const auto games = state.library ? games_for_system(*state.library, id) : GameList{};
        LaunchRequest preview;
        const auto plan = state.library && state.game_focus >= 0 && static_cast<std::size_t>(state.game_focus) < games.count
                              ? plan_game(*state.library, games.indices[static_cast<std::size_t>(state.game_focus)],
                                          state.settings.preferred_emulator[id].data(), preview)
                              : LaunchPlan::unavailable;
        const auto caption = launch_caption(plan);
        const SDL_Color mode_color = plan == LaunchPlan::menu ? SDL_Color{239, 205, 151, 255}
                                                              : SDL_Color{159, 220, 194, 255};
        rounded(80, 400, 1000, 126, 18, {13, 23, 25, 248});
        rect(80, 420, 4, 86, mode_color);
        text(route ? route->name : state.inventory[id].built_in ? "PS5 system" : "Launch information",
             104, 412, 19, {207, 218, 213, 255}, false, 690);
        text(caption.badge, 842, 412, 17, mode_color, true, 220);
        text(caption.action, 104, 440, 29, white, true, 940);
        text(caption.expectation, 104, 482, 20, {215, 222, 219, 255}, false, 940);
        if (!state.library) {
            text("Your library", 84, 554, 38, white, true);
            text("Connect a library to see your games here.", 84, 615, 26, muted);
        } else {
            if (!games.count)
                text("No games found for this console yet.", 84, 558, 26, muted);
            const int start = state.game_focus / 8 * 8;
            for (int row = 0; row < 8 && static_cast<std::size_t>(start + row) < games.count; ++row) {
                const auto& game = state.library->games[games.indices[static_cast<std::size_t>(start + row)]];
                const int y = 546 + row * 52;
                box(80, y, 1000, 46, !state.tools && state.game_focus == start + row);
                text(game.name, 100, y + 7, 23, white, !state.tools && state.game_focus == start + row, 895);
                const auto key = game_favorite_key(game);
                if (std::find(state.settings.game_favorites.begin(), state.settings.game_favorites.end(), key) != state.settings.game_favorites.end())
                    text("*", 1026, y + 9, 27, purple, true);
            }
            if (games.count) {
                char position[80];
                std::snprintf(position, sizeof(position), "%d / %zu", state.game_focus + 1, games.count);
                text(position, 84, 973, 19, muted);
            }
        }
        char actions[240];
        std::snprintf(actions, sizeof(actions), "Cross  %s    D-pad  Browse    Left / Right  Page    Triangle  Favourite    Circle  Back", caption.action);
        footer(actions);
        return;
    }
    const auto& route = state.settings.preferred_emulator[id];
    const auto* detected = state.library ? preferred_route(*state.library, id, route.data()) : nullptr;
    label(80, 515, "Preferred emulator",
          state.inventory[id].built_in ? std::string_view("PS5 system")
          : detected                   ? std::string_view(detected->name)
                                       : std::string_view("No emulator detected"),
          !state.tools && state.detail_focus == 0, 920);
    box(80, 631, 443, 82, !state.tools && state.detail_focus == 1);
    text("Open emulator library", 107, 652, 27,
         state.inventory[id].emulator_count ? white : muted, true);
    box(547, 631, 453, 82, !state.tools && state.detail_focus == 2);
    text("Show game library", 575, 652, 29, white, true);
    text("Choose Show game library to see how each game will open.", 84, 765, 23, muted);
    footer("D-pad  Choose action    Cross  Select    Triangle  Favourite    Circle  Back    Options  Settings");
}
void settings(const UiState& state) {
    rect(0, 0, width, height, {4, 6, 16, 180});
    box(350, 76, 1220, 900, false, {20, 23, 41, 255});
    text("MAKE IT YOURS", 394, 108, 19, purple, true);
    text("isShelf settings", 394, 139, 43, white, true);
    text("Changes save automatically", 396, 199, 22, muted);
    const std::array<std::string_view, 4> tabs = {"Appearance", "Sort & filter", "Consoles", "Play room"};
    for (int i = 0; i < 4; ++i) {
        text(tabs[static_cast<std::size_t>(i)], 394 + i * 250, 250, 25,
             static_cast<int>(state.tab) == i ? purple : muted, true);
        if (static_cast<int>(state.tab) == i)
            rounded(394 + i * 250, 292, 210, 3, 1, purple);
    }
    line(350, 304, 1570, 304, {49, 55, 78, 255});
    const auto& p = state.settings.preferences;
    if (state.tab == SettingsTab::appearance) {
        text("Your shelf, your style", 394, 338, 34, white, true);
        text("Choose Grid, curved Swipe, or a lit display Shelf.", 394, 388, 24, muted);
        label(394, 429, "Layout", layout_labels[static_cast<std::size_t>(p.layout)], state.settings_focus == 0);
        label(394, 519, "Grid look", p.layout != Layout::grid ? "Available in Grid layout" :
              p.grid_look == GridLook::icons ? "3D console icons" : "Classic cards",
              state.settings_focus == 1);
        label(394, 609, "isShelf logo", icon_names[static_cast<std::size_t>(p.icon)], state.settings_focus == 2);
        label(394, 699, "Missing emulator artwork", p.monochrome_missing ? "Black & white" : "Full colour",
              state.settings_focus == 3);
        label(394, 789, "Motion", p.reduced_motion ? "Reduced: instant page changes" : "Smooth console transitions",
              state.settings_focus == 4);
    } else if (state.tab == SettingsTab::browse) {
        text("Find your next favourite", 394, 338, 34, white, true);
        text("Choose an order and narrow down your collection.", 394, 388, 24, muted);
        label(394, 435, "Sort by", sorts[static_cast<std::size_t>(p.sort)], state.settings_focus == 0);
        label(394, 539, "Brand", p.brand ? brands[p.brand] : "All brands", state.settings_focus == 1);
        label(394, 643, "Availability", filters[static_cast<std::size_t>(p.filter)], state.settings_focus == 2);
        label(394, 747, "Shelf pages", p.shelf_single ? "One curated shelf: choose up to 8 consoles" :
              "Browse collection: 8 consoles per page", state.settings_focus == 3);
        text(p.shelf_single ? "Choose your 8 in Consoles. Grid and Swipe keep your full collection." :
              "L1 / R1 changes pages in Shelf. Your filters apply to every page.", 400, 855, 21, muted);
    } else if (state.tab == SettingsTab::room) {
        text("Your room, your screen", 394, 338, 34, white, true);
        text("TV content sits behind the glass. Shelf layout only.", 394, 388, 24, muted);
        label(394, 421, "TV display", tv_labels[static_cast<std::size_t>(p.tv_mode)], state.settings_focus == 0);
        label(394, 505, "Personal message - Cross to edit", state.settings.tv_text, state.settings_focus == 1);
        char interval[64]; std::snprintf(interval, sizeof(interval), "Every %u seconds", tv_periods[p.tv_period]);
        label(394, 589, "Favourite game rotation", interval, state.settings_focus == 2);
        label(394, 673, "Console intros", p.intro_all ? "All consoles" : "Installed emulators and built-in systems", state.settings_focus == 3);
        label(394, 757, "TV static for missing content", noise_labels[static_cast<std::size_t>(p.tv_noise)], state.settings_focus == 4);
        text("Own intros: /data/console-shelf/intros/<console-id>.png or .mpg", 400, 854, 20, muted);
        text("isShelf originals included. Your personal PNG or MPEG-1 clip takes priority.", 400, 882, 19, muted);
    } else {
        label(394, 330, "Show on my shelf", p.scope == Scope::all ? "All consoles" :
              p.scope == Scope::available ? "Emulator or games" : "Favourites only",
              state.settings_focus == 0);
        box(394, 432, 1032, 58, state.settings_focus == 1);
        icon(0, 424, 460, muted);
        text(state.console_query[0] ? state.console_query : "Find a console or brand...", 454, 446, 22,
             state.console_query[0] ? white : muted);
        std::size_t enabled = 0;
        for (std::size_t i = 0; i < system_count; ++i)
            if (p.shelf_single ? p.shelf_selected[i] : !p.hidden[i]) ++enabled;
        char tally[80];
        std::snprintf(tally, sizeof(tally), "%zu of %zu %s", enabled, p.shelf_single ? shelf_capacity : system_count,
                      p.shelf_single ? "display spaces filled" : "enabled");
        text(tally, 398, 509, 20, muted);
        rounded(981, 500, 207, 42, 10, state.settings_focus == 2 ? purple : SDL_Color{29, 34, 57, 255});
        centered(p.shelf_single ? "Fill spaces" : state.console_query[0] ? "Show results" : "Show all", 1084, 507, 20,
                 state.settings_focus == 2 ? panel : muted, true);
        rounded(1200, 500, 227, 42, 10, state.settings_focus == 3 ? purple : SDL_Color{29, 34, 57, 255});
        centered(p.shelf_single ? "Clear selection" : state.console_query[0] ? "Hide results" : "Hide all", 1313, 507, 20,
                 state.settings_focus == 3 ? panel : muted, true);
        const auto picker = console_picker(state);
        const int selected = state.settings_focus - 4;
        const int start = std::max(0, selected - 3);
        SDL_Rect clip{390, 555, 1050, 321};
        SDL_RenderSetClipRect(renderer, &clip);
        for (int row = 0; row < 5; ++row) {
            const int at = start + row;
            if (at >= static_cast<int>(picker.count))
                break;
            const auto e = picker.entries[static_cast<std::size_t>(at)];
            const int y = 555 + row * 66;
            const bool active = at == selected;
            if (active)
                rounded(394, y, 1032, 62, 12, {58, 48, 88, 255});
            if (e.group) {
                text(brands[e.index], 414, y + 17, 23, purple, true);
                text("Cross: toggle brand", 1170, y + 20, 18, muted);
            } else {
                const auto& s = catalog[e.index];
                const bool enabled_item = p.shelf_single ? p.shelf_selected[e.index] : !p.hidden[e.index];
                picture(s.image, 408, y + 3, 61, 55, enabled_item ? 255 : 100,
                        use_monochrome(p, state.inventory[e.index], s.id));
                text(s.name, 489, y + 17, 25, enabled_item ? white : muted, true, 785);
                switch_control(1334, y + 17, enabled_item);
            }
        }
        SDL_RenderSetClipRect(renderer, nullptr);
        if (!picker.count)
            centered("No consoles match this search", 910, 647, 27, muted);
    }
    rect(351, 892, 1218, 83, {15, 19, 35, 255});
    text("L1 / R1  Sections     D-pad  Choose     Cross / Left / Right  Change     Circle  Done", 394, 919, 20, muted);
}
void keyboard(const UiState& state) {
    rect(0, 0, width, height, {4, 6, 16, 210});
    box(330, 215, 1260, 650, false, {20, 23, 41, 255});
    text(state.keyboard_for_tv ? "Your room message" : "Find a console", 380, 248, 38, white, true);
    box(380, 318, 1160, 66, false);
    text(state.keyboard_for_tv ? state.settings.tv_text : state.keyboard_for_consoles ? state.console_query : state.query,
         407, 333, 29, purple, true);
    for (int i = 0; i < 40; ++i) {
        const int x = 380 + (i % 10) * 117, y = 418 + (i / 10) * 78;
        const bool active = state.keyboard_focus == i;
        rounded(x, y, 105, 64, 12, active ? purple : SDL_Color{35, 40, 63, 255});
        const char key = keyboard_keys[static_cast<std::size_t>(i)];
        char label_text[8] = {key, 0};
        const char* displayed = key == '<'   ? "Del"
                                : key == '_' ? "Space"
                                : key == '!' ? "Clear"
                                : key == '^' ? "Done"
                                             : label_text;
        centered(displayed, x + 52, y + 15, key == '_' || key == '!' || key == '^' ? 19 : 26, active ? panel : white,
                 true);
    }
    text("Cross  Type     Square  Delete     Triangle  Clear     Options / Circle  Done", 384, 783, 22, muted);
}
} // namespace

SDL_Texture* build_room_background(const char* image_name) {
    auto* pixels = static_cast<Uint32*>(allocate(static_cast<std::size_t>(width) * height * 4));
    if (!pixels)
        return nullptr;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const float dx = static_cast<float>(x - 1700) / 1000, dy = static_cast<float>(y + 80) / 850;
            const float glow_amount = std::max(0.f, 1.f - dx * dx - dy * dy);
            const auto r = static_cast<Uint32>(11 + glow_amount * 17), g = static_cast<Uint32>(13 + glow_amount * 8),
                       b = static_cast<Uint32>(28 + glow_amount * 28);
            pixels[y * width + x] = 0xff000000u | (b << 16) | (g << 8) | r;
        }
    // Bake each complete room once; no per-tier image panels or per-frame resize.
    char gallery_path[2304];
    std::snprintf(gallery_path, sizeof(gallery_path), "%s/%s", root, image_name);
    int gw = 0, gh = 0, channels = 0;
    unsigned char* gallery = stbi_load(gallery_path, &gw, &gh, &channels, 4);
    if (gallery && gw > 0 && gh > 0 && gw <= 4096 && gh <= 4096) {
        const float fit = std::max(static_cast<float>(width) / gw, static_cast<float>(height) / gh);
        const float ox = (gw - width / fit) / 2, oy = (gh - height / fit) / 2;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                const float sx = std::clamp(ox + (x + .5f) / fit - .5f, 0.f, static_cast<float>(gw - 1));
                const float sy = std::clamp(oy + (y + .5f) / fit - .5f, 0.f, static_cast<float>(gh - 1));
                const int ix = static_cast<int>(sx), iy = static_cast<int>(sy);
                const int nx = std::min(ix + 1, gw - 1), ny = std::min(iy + 1, gh - 1);
                const float fx = sx - ix, fy = sy - iy;
                const float visibility = .88f - .12f * static_cast<float>(y) / height;
                Uint32 output = 0xff000000u;
                for (int channel = 0; channel < 3; ++channel) {
                    const auto sample = [&](int px, int py) {
                        return gallery[(static_cast<std::size_t>(py) * gw + px) * 4 + channel];
                    };
                    const float top = sample(ix, iy) * (1 - fx) + sample(nx, iy) * fx;
                    const float bottom = sample(ix, ny) * (1 - fx) + sample(nx, ny) * fx;
                    const auto base = (pixels[y * width + x] >> (channel * 8)) & 255;
                    const auto value = static_cast<Uint32>((top * (1 - fy) + bottom * fy) * visibility +
                                                           base * (1 - visibility));
                    output |= value << (channel * 8);
                }
                pixels[y * width + x] = output;
            }
    } else
        ++resource_errors;
    stbi_image_free(gallery);
    auto* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, width, height);
    if (texture)
        SDL_UpdateTexture(texture, nullptr, pixels, width * 4);
    release(pixels);
    return texture;
}
bool graphics_init(SDL_Renderer* target, const char* asset_root, const char* data_root) {
    renderer = target;
    if (std::strlen(asset_root) >= sizeof(root))
        return false;
    std::strcpy(root, asset_root);
    if (data_root && std::strlen(data_root) >= sizeof(room_data)) return false;
    std::snprintf(room_data, sizeof(room_data), "%s", data_root ? data_root : "");
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    background = build_room_background(room_scene::base_image);
    shelf_background = build_room_background(room_scene::shelf_image);
    Uint32 halo_pixels[256 * 256];
    for (int y = 0; y < 256; ++y)
        for (int x = 0; x < 256; ++x) {
            const float dx = static_cast<float>(x - 128) / 128, dy = static_cast<float>(y - 128) / 128;
            const auto a = static_cast<Uint32>(std::max(0.f, 1.f - dx * dx - dy * dy) * 200);
            halo_pixels[y * 256 + x] = (a << 24) | 0xffffffu;
        }
    halo = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, 256, 256);
    if (halo) {
        SDL_UpdateTexture(halo, nullptr, halo_pixels, 256 * 4);
        SDL_SetTextureBlendMode(halo, SDL_BLENDMODE_BLEND);
    }
    return background && shelf_background && halo && font(24, false) && font(27, true);
}
void graphics_render(const UiState& state, unsigned time_ms, bool animate) {
    render_time = time_ms;
    if (state.presentation.artwork) {
        SDL_SetRenderDrawColor(renderer, 11, 13, 28, 255);
        SDL_RenderClear(renderer);
        splash(1, 0, false);
        SDL_RenderPresent(renderer);
        return;
    }
    const bool show_orbit = (!state.presentation.loading || state.presentation.preparing) && state.page == Page::home &&
                            state.settings.preferences.layout == Layout::swipe;
    if (show_orbit) {
        const auto list = shown_systems(state);
        if (!orbit_visible || list.count != orbit_list.count || list.indices != orbit_list.indices) {
            orbit = {};
            orbit.position = orbit.target = state.focus;
            orbit_list = list;
        }
        orbit.select(state.focus, static_cast<double>(list.count), time_ms, !animate || state.settings.preferences.reduced_motion);
        orbit.update(time_ms);
    } else
        orbit.active = false;
    orbit_visible = show_orbit;
    const bool show_shelf = state.page == Page::home && state.settings.preferences.layout == Layout::shelf;
    if (show_shelf) {
        const auto list = shown_systems(state);
        if (!shelf_visible || list.count != shelf_list.count || list.indices != shelf_list.indices)
            shelf_motion.reset(state.focus / 8);
        shelf_motion.select(state.focus / 8, time_ms, !animate || state.settings.preferences.reduced_motion);
        shelf_list = list;
    } else shelf_motion.active = false;
    shelf_visible = show_shelf;
    room_visible = state.settings.preferences.layout == Layout::shelf;
    if (!room_visible) { stop_movie(); tv_motion = false; }
    SDL_SetRenderDrawColor(renderer, 11, 13, 28, 255);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, room_visible ? shelf_background : background, nullptr, nullptr);
    if (!state.presentation.loading || state.presentation.preparing) {
        if (room_visible) room_tv(state);
        picture(icon_files[static_cast<std::size_t>(state.settings.preferences.icon)], 80, 30, 50, 50);
        text("isShelf", 147, 36, 29, white, true);
#ifdef SHELF_PS5
        text("PS5 / DEVELOPMENT BUILD", 1388, 42, 18, muted, true);
#else
        text("NATIVE DEVELOPMENT BUILD / PC", 1388, 42, 18, muted, true);
#endif
        settings_button(state.tools && state.tool == 2);
        line(80, 99, room_visible ? 1100 : 1840, 99, {43, 48, 69, 255});
        if (state.page == Page::home)
            home(state);
        else if (state.page == Page::details || state.page == Page::library)
            detail(state);
        else if (state.page == Page::settings)
            settings(state);
        else if (state.page == Page::keyboard) {
            if (state.keyboard_for_consoles)
                settings(state);
            keyboard(state);
        }
        if (state.notice[0] && state.presentation.hold <= 0) {
            rounded(300, 931, 1320, 64, 16, {57, 46, 84, 255});
            centered(state.notice, 960, 948, 24, white);
        }
    }
    if (state.presentation.loading)
        splash(1, time_ms, true, state.presentation.appear, state.settings.preferences.icon);
    else if (state.presentation.reveal < 1)
        splash(1 - state.presentation.reveal, time_ms, false, 1, state.settings.preferences.icon);
    if (state.presentation.hold > 0 && state.presentation.close == 0) {
        rounded(660, 909, 600, 88, 18, {30, 30, 48, 245});
        centered("Hold Circle to close isShelf", 960, 924, 24, white);
        rounded(707, 969, 506, 5, 2, {64, 62, 83, 255});
        rounded(707, 969, std::max(1, static_cast<int>(506 * state.presentation.hold)), 5, 2, purple);
    } else if (state.presentation.close_hint > 0 && !state.presentation.loading && state.presentation.close == 0) {
        const float opacity = state.presentation.close_hint;
        rounded(660, 909, 600, 88, 18, {30, 30, 48, static_cast<Uint8>(245 * opacity)});
        centered("Hold Circle to close isShelf", 960, 924, 24, {white.r, white.g, white.b, static_cast<Uint8>(255 * opacity)});
        centered("Keep holding for 3 seconds", 960, 961, 18,
                 {muted.r, muted.g, muted.b, static_cast<Uint8>(255 * opacity)});
    }
    if (state.presentation.close > 0) {
        const auto alpha = static_cast<Uint8>(state.presentation.close * 255);
        rect(0, 0, width, height, {9, 12, 26, alpha});
        picture(icon_files[static_cast<std::size_t>(state.settings.preferences.icon)], 916, 413, 88, 88, alpha);
        centered(state.presentation.launching ? "Opening your selection" : "Closing isShelf", 960, 552, 29,
                 {241, 240, 251, alpha});
    }
    SDL_RenderPresent(renderer);
}
bool graphics_animating() {
    return (orbit_visible && orbit.active) || (shelf_visible && shelf_motion.active) || (room_visible && tv_motion);
}
bool graphics_capture(const char* path) {
    auto* pixels = static_cast<Uint32*>(allocate(static_cast<std::size_t>(width) * height * 4));
    if (!pixels)
        return false;
    const bool okay = SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ABGR8888, pixels, width * 4) == 0 &&
                      stbi_write_png(path, width, height, 4, pixels, width * 4) != 0;
    release(pixels);
    return okay;
}
unsigned graphics_errors() {
    return resource_errors;
}
void graphics_shutdown() {
    stop_movie();
    if (noise_texture) SDL_DestroyTexture(noise_texture);
    noise_texture = nullptr;
    release(noise_pixels); noise_pixels = nullptr; noise_width = noise_height = 0;
    for (auto*& texture : settings_buttons) {
        if (texture)
            SDL_DestroyTexture(texture);
        texture = nullptr;
    }
    for (auto& f : fonts) {
        if (f.texture)
            SDL_DestroyTexture(f.texture);
        f = Font{};
    }
    for (auto& entry : art) {
        if (entry.texture)
            SDL_DestroyTexture(entry.texture);
        entry = Art{};
    }
    if (background)
        SDL_DestroyTexture(background);
    if (shelf_background)
        SDL_DestroyTexture(shelf_background);
    if (halo)
        SDL_DestroyTexture(halo);
    background = nullptr;
    shelf_background = nullptr;
    halo = nullptr;
    renderer = nullptr;
}
} // namespace shelf
