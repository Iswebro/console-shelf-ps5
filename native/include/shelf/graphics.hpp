// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ui.hpp"
struct SDL_Renderer;
namespace shelf {
bool graphics_init(SDL_Renderer* renderer, const char* asset_root);
void graphics_render(const UiState& state);
bool graphics_capture(const char* path);
unsigned graphics_errors();
void graphics_shutdown();
} // namespace shelf
