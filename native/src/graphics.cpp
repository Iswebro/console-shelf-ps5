// SPDX-License-Identifier: GPL-3.0-only
#include "shelf/graphics.hpp"
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"
#include "stb/stb_truetype.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace shelf {
namespace {
constexpr int width = 1920, height = 1080;
constexpr SDL_Color white{246, 246, 255, 255}, muted{157, 168, 198, 255}, purple{203, 187, 255, 255},
    panel{22, 25, 44, 255};
SDL_Renderer* renderer = nullptr;
SDL_Texture* background = nullptr;
SDL_Texture* halo = nullptr;
char root[2048]{};
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
    unsigned age = 0;
};
std::array<Art, 12> art;
unsigned art_age = 0;
const std::array<std::string_view, 8> sorts = {"Featured order",   "Name: A to Z",    "Name: Z to A",
                                               "Newest hardware",  "Oldest hardware", "Brand, then name",
                                               "Most games first", "Favorites first"};
const std::array<std::string_view, 5> filters = {"All systems", "Emulator detected", "Games detected",
                                                 "Built-in systems", "No launch route"};
const std::array<std::string_view, 3> icon_names = {"Gaming", "Classic mint", "Jade"};
const std::array<std::string_view, 3> icon_files = {"assets/shelf-icon.png", "assets/shelf-icon-v1.png",
                                                    "assets/shelf-icon-premium-v2.png"};
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
    auto* bytes = static_cast<unsigned char*>(std::malloc(static_cast<std::size_t>(length)));
    auto* bitmap = static_cast<unsigned char*>(std::calloc(atlas_pixels, 1));
    auto* pixels = static_cast<Uint32*>(std::malloc(atlas_pixels * 4));
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
    std::free(bytes);
    std::free(bitmap);
    std::free(pixels);
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
Art* image(std::string_view name) {
    ++art_age;
    for (auto& entry : art)
        if (std::string_view(entry.name) == name) {
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
    char filename[2304];
    std::snprintf(filename, sizeof(filename), "%s/%s", root, slot->name);
    int channels = 0;
    unsigned char* data = stbi_load(filename, &slot->w, &slot->h, &channels, 4);
    if (data && slot->w > 0 && slot->h > 0 && slot->w <= 4096 && slot->h <= 4096) {
        slot->texture =
            SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, slot->w, slot->h);
        if (slot->texture) {
            SDL_UpdateTexture(slot->texture, nullptr, data, slot->w * 4);
            SDL_SetTextureBlendMode(slot->texture, SDL_BLENDMODE_BLEND);
        }
    }
    stbi_image_free(data);
    if (!slot->texture)
        ++resource_errors;
    return slot;
}
void picture(std::string_view name, int x, int y, int w, int h, Uint8 alpha = 255) {
    auto* entry = image(name);
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
        for (int a = 0; a < 8; ++a) {
            const double r = static_cast<double>(a) * 3.141592653589793 / 4;
            line(x + static_cast<int>(std::cos(r) * 9), y + static_cast<int>(std::sin(r) * 9),
                 x + static_cast<int>(std::cos(r) * 15), y + static_cast<int>(std::sin(r) * 15), tint);
        }
        for (int a = 0; a < 360; a += 8) {
            const double r = static_cast<double>(a) * 3.141592653589793 / 180;
            rect(x + static_cast<int>(std::cos(r) * 8), y + static_cast<int>(std::sin(r) * 8), 2, 2, tint);
        }
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
std::string_view availability(const Availability& a) {
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
void home(const UiState& state) {
    const auto shown = shown_systems(state);
    const auto& p = state.settings.preferences;
    text("ONE HOME FOR YOUR SYSTEMS", 80, 119, 19, purple, true);
    text("What do you want", 80, 153, 59, white, true);
    text("to play today?", 80, 218, 59, purple, true);
    box(80, 314, 1500, 66, state.tools && state.tool == 0);
    icon(0, 111, 346, muted);
    text(state.query[0] ? state.query : "Find a console, brand or core...", 143, 330, 24,
         state.query[0] ? white : muted);
    for (int i = 1; i < 3; ++i) {
        box(1603 + (i - 1) * 84, 314, 66, 66, state.tools && state.tool == i);
        icon(i, 1636 + (i - 1) * 84, 347);
    }
    char summary[160];
    std::snprintf(summary, sizeof(summary), "%s  /  %.*s  /  %zu systems", p.layout == Layout::grid ? "Grid" : "Swipe",
                  static_cast<int>(sorts[static_cast<std::size_t>(p.sort)].size()),
                  sorts[static_cast<std::size_t>(p.sort)].data(), shown.count);
    text(summary, 84, 391, 20, muted);
    if (!shown.count) {
        centered("Your shelf is ready for your choices", 960, 620, 42, white, true);
        centered("Choose All consoles or enable systems in settings.", 960, 679, 26, muted);
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
            glow(x + 30, y + 2, 350, 200, accent(s), active ? 120 : 62);
            if (p.grid_look == GridLook::icons)
                picture(s.image, x + 20, y + 7, 376, 166);
            else {
                text(s.brand, x + 22, y + 25, 22, purple, true);
                text("CONSOLE", x + 22, y + 70, 38, muted, true);
            }
            text(s.name, x + 20, y + 173, 26, white, true, 374);
            text(availability(state.inventory[id]), x + 20, y + 212, 19, muted, false, 374);
            if (p.favorite[id])
                text("*", x + 377, y + 12, 30, purple, true);
        }
    } else {
        for (int offset = -2; offset <= 2; ++offset) {
            const int at = state.focus + offset;
            if (at < 0 || at >= static_cast<int>(shown.count))
                continue;
            const auto id = shown.indices[static_cast<std::size_t>(at)];
            const auto& s = catalog[id];
            const int cx = 960 + offset * 500;
            glow(cx - 210, 490, 420, 390, accent(s), offset == 0 ? 130 : 50);
            picture(s.image, cx - 230, 466, 460, 348, offset == 0 ? 255 : 160);
            centered(s.name, cx, 824, offset == 0 ? 35 : 27, offset == 0 ? white : muted, true);
            if (offset == 0) {
                centered(availability(state.inventory[id]), cx, 880, 22, muted);
                rounded(cx - 30, 933, 60, 4, 2, purple);
            }
        }
    }
    footer(
        "D-pad  Browse    Cross  Open    Triangle  Favourite    Square  Filter    Options  Settings    Circle  Back");
}
void detail(const UiState& state) {
    const auto id = state.selected_system;
    const auto& s = catalog[id];
    text("YOUR SYSTEM COLLECTION", 80, 145, 20, purple, true);
    text(s.name, 80, 194, 58, white, true, 1100);
    text(s.brand, 84, 275, 27, muted);
    glow(1190, 230, 640, 630, accent(s), 110);
    picture(s.image, 1160, 205, 640, 640);
    text(availability(state.inventory[id]), 84, 366, 29, purple, true);
    char count[80];
    std::snprintf(count, sizeof(count), "%zu discovered games", state.inventory[id].game_count);
    text(count, 84, 415, 24, muted);
    if (state.page == Page::library) {
        text("Game library", 84, 522, 38, white, true);
        text("Discovery adapters are not connected in this host build yet.", 84, 587, 26, muted);
        footer("Circle  Back to console    Options  Settings");
        return;
    }
    const auto& route = state.settings.preferred_emulator[id];
    label(80, 515, "Preferred emulator",
          route[0] ? std::string_view(route.data()) : std::string_view("No emulator detected"), state.detail_focus == 0,
          920);
    box(80, 631, 443, 82, state.detail_focus == 1);
    text("Open emulator", 107, 652, 29, state.inventory[id].emulator_count ? white : muted, true);
    box(547, 631, 453, 82, state.detail_focus == 2);
    text("Show game library", 575, 652, 29, white, true);
    text("An emulator-menu route opens the emulator's own shelf.", 84, 765, 23, muted);
    footer("D-pad  Choose action    Cross  Select    Triangle  Favourite    Circle  Back    Options  Settings");
}
void settings(const UiState& state) {
    rect(0, 0, width, height, {4, 6, 16, 180});
    box(350, 76, 1220, 900, false, {20, 23, 41, 255});
    text("MAKE IT YOURS", 394, 108, 19, purple, true);
    text("Shelf settings", 394, 139, 43, white, true);
    text("Changes save automatically", 396, 199, 22, muted);
    const std::array<std::string_view, 3> tabs = {"Appearance", "Sort & filter", "Consoles"};
    for (int i = 0; i < 3; ++i) {
        text(tabs[static_cast<std::size_t>(i)], 394 + i * 250, 250, 25,
             static_cast<int>(state.tab) == i ? purple : muted, true);
        if (static_cast<int>(state.tab) == i)
            rounded(394 + i * 250, 292, 210, 3, 1, purple);
    }
    line(350, 304, 1570, 304, {49, 55, 78, 255});
    const auto& p = state.settings.preferences;
    if (state.tab == SettingsTab::appearance) {
        text("Your shelf, your style", 394, 338, 34, white, true);
        text("Browse as a grid or a row of floating console icons.", 394, 388, 24, muted);
        label(394, 452, "Layout", p.layout == Layout::grid ? "Grid" : "Swipe", state.settings_focus == 0);
        label(394, 563, "Grid look", p.grid_look == GridLook::icons ? "3D console icons" : "Classic cards",
              state.settings_focus == 1);
        label(394, 674, "App icon", icon_names[static_cast<std::size_t>(p.icon)], state.settings_focus == 2);
        text("Swipe always uses floating icons without cards.", 400, 802, 22, muted);
    } else if (state.tab == SettingsTab::browse) {
        text("Find your next favourite", 394, 338, 34, white, true);
        text("Choose an order and narrow down your collection.", 394, 388, 24, muted);
        label(394, 452, "Sort by", sorts[static_cast<std::size_t>(p.sort)], state.settings_focus == 0);
        label(394, 563, "Brand", p.brand ? brands[p.brand] : "All brands", state.settings_focus == 1);
        label(394, 674, "Availability", filters[static_cast<std::size_t>(p.filter)], state.settings_focus == 2);
    } else {
        label(394, 330, "Show on my shelf", p.scope == Scope::all ? "All consoles" : "Emulator or games",
              state.settings_focus == 0);
        box(394, 432, 1032, 58, state.settings_focus == 1);
        icon(0, 424, 460, muted);
        text(state.console_query[0] ? state.console_query : "Find a console or brand...", 454, 446, 22,
             state.console_query[0] ? white : muted);
        std::size_t enabled = 0;
        for (bool hidden : p.hidden)
            if (!hidden)
                ++enabled;
        char tally[80];
        std::snprintf(tally, sizeof(tally), "%zu of %zu enabled", enabled, system_count);
        text(tally, 398, 509, 20, muted);
        rounded(981, 500, 207, 42, 10, state.settings_focus == 2 ? purple : SDL_Color{29, 34, 57, 255});
        centered(state.console_query[0] ? "Show results" : "Show all", 1084, 507, 20,
                 state.settings_focus == 2 ? panel : muted, true);
        rounded(1200, 500, 227, 42, 10, state.settings_focus == 3 ? purple : SDL_Color{29, 34, 57, 255});
        centered(state.console_query[0] ? "Hide results" : "Hide all", 1313, 507, 20,
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
                picture(s.image, 408, y + 3, 61, 55, p.hidden[e.index] ? 100 : 255);
                text(s.name, 489, y + 17, 25, p.hidden[e.index] ? muted : white, true, 785);
                switch_control(1334, y + 17, !p.hidden[e.index]);
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
    text("Find a console", 380, 248, 38, white, true);
    box(380, 318, 1160, 66, false);
    text(state.keyboard_for_consoles ? state.console_query : state.query, 407, 333, 29, purple, true);
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

bool graphics_init(SDL_Renderer* target, const char* asset_root) {
    renderer = target;
    if (std::strlen(asset_root) >= sizeof(root))
        return false;
    std::strcpy(root, asset_root);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    auto* pixels = static_cast<Uint32*>(std::malloc(static_cast<std::size_t>(width) * height * 4));
    if (!pixels)
        return false;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const float dx = static_cast<float>(x - 1700) / 1000, dy = static_cast<float>(y + 80) / 850;
            const float glow_amount = std::max(0.f, 1.f - dx * dx - dy * dy);
            const auto r = static_cast<Uint32>(11 + glow_amount * 17), g = static_cast<Uint32>(13 + glow_amount * 8),
                       b = static_cast<Uint32>(28 + glow_amount * 28);
            pixels[y * width + x] = 0xff000000u | (b << 16) | (g << 8) | r;
        }
    background = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, width, height);
    if (background)
        SDL_UpdateTexture(background, nullptr, pixels, width * 4);
    std::free(pixels);
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
    return background && halo && font(24, false) && font(27, true);
}
void graphics_render(const UiState& state) {
    SDL_SetRenderDrawColor(renderer, 11, 13, 28, 255);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, background, nullptr, nullptr);
    picture(icon_files[static_cast<std::size_t>(state.settings.preferences.icon)], 80, 30, 50, 50);
    text("Console Shelf", 147, 36, 29, white, true);
    text("NATIVE DEVELOPMENT BUILD / PC", 1388, 42, 18, muted, true);
    box(1786, 27, 58, 58, state.tools && state.tool == 3);
    icon(3, 1815, 56);
    line(80, 99, 1840, 99, {43, 48, 69, 255});
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
    } else {
        home(state);
        rect(0, 0, width, height, {4, 6, 16, 180});
        box(570, 367, 780, 310, false);
        centered("Close Console Shelf?", 960, 413, 40, white, true);
        centered("Cross: close     Circle: keep browsing", 960, 524, 25, muted);
    }
    if (state.notice[0]) {
        rounded(300, 931, 1320, 64, 16, {57, 46, 84, 255});
        centered(state.notice, 960, 948, 24, white);
    }
    SDL_RenderPresent(renderer);
}
bool graphics_capture(const char* path) {
    auto* pixels = static_cast<Uint32*>(std::malloc(static_cast<std::size_t>(width) * height * 4));
    if (!pixels)
        return false;
    const bool okay = SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ABGR8888, pixels, width * 4) == 0 &&
                      stbi_write_png(path, width, height, 4, pixels, width * 4) != 0;
    std::free(pixels);
    return okay;
}
unsigned graphics_errors() {
    return resource_errors;
}
void graphics_shutdown() {
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
    if (halo)
        SDL_DestroyTexture(halo);
    background = nullptr;
    halo = nullptr;
    renderer = nullptr;
}
} // namespace shelf
