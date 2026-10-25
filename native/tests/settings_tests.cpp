// SPDX-License-Identifier: GPL-3.0-only
#include "shelf/settings.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif
namespace {
void check(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "FAIL settings: %s\n", message);
        std::exit(1);
    }
}
void write(const char* path, const char* text) {
    FILE* file = std::fopen(path, "wb");
    check(file != nullptr, "test file open");
    check(std::fwrite(text, 1, std::strlen(text), file) == std::strlen(text), "test file write");
    std::fclose(file);
}
} // namespace
void test_settings() {
    using namespace shelf;
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    char directory[128], path[160];
    std::snprintf(directory, sizeof(directory), "settings-test-%lld", static_cast<long long>(stamp));
#ifdef _WIN32
    check(_mkdir(directory) == 0, "owned test directory");
#else
    check(mkdir(directory, 0700) == 0, "owned test directory");
#endif
    std::snprintf(path, sizeof(path), "%s/settings.ini", directory);
    Settings settings;
    check(load_settings(path, settings) == SettingsLoad::missing, "first run uses defaults");
    settings.preferences.hidden[find_system("ps2")] = true;
    settings.preferences.favorite[find_system("n64")] = true;
    settings.preferences.sort = Sort::favorites;
    settings.preferences.brand = 2;
    std::strcpy(settings.preferred_emulator[find_system("ps2")].data(), "ps5sx2");
    check(save_settings(path, settings), "initial atomic save");
    Settings read;
    check(load_settings(path, read) == SettingsLoad::loaded, "reload valid settings");
    check(read.preferences.hidden[find_system("ps2")] && read.preferences.favorite[find_system("n64")],
          "stable console IDs survive reload");
    check(read.preferences.sort == Sort::favorites && read.preferences.brand == 2, "sort and brand persist");
    check(std::strcmp(read.preferred_emulator[find_system("ps2")].data(), "ps5sx2") == 0,
          "preferred emulator persists");
    settings.preferences.layout = Layout::swipe;
    check(save_settings(path, settings), "replacement preserves a valid backup");
    check(load_settings(path, read) == SettingsLoad::loaded && read.preferences.layout == Layout::swipe,
          "latest save is current");
    write(path, "version=999\nlayout=grid\n");
    check(load_settings(path, read) == SettingsLoad::backup && read.preferences.layout == Layout::grid,
          "damaged/unsupported current file recovers last valid backup");
    FILE* file = std::fopen(path, "rb");
    char original[64]{};
    check(file != nullptr, "corrupt source retained");
    std::fread(original, 1, 63, file);
    std::fclose(file);
    check(std::strstr(original, "version=999") != nullptr, "load never rewrites damaged data");
    check(save_settings(path, read), "explicit subsequent save can replace damaged current file");
    std::strcpy(read.preferred_emulator[find_system("ps2")].data(), "../invalid");
    check(!save_settings(path, read), "invalid route identifier cannot enter settings");
    check(load_settings(path, read) == SettingsLoad::loaded, "rejected save preserves current settings");
    read.preferences.layout = Layout::shelf;
    check(save_settings(path, read), "save third layout without changing the settings schema");
    Settings displayed;
    check(load_settings(path, displayed) == SettingsLoad::loaded && displayed.preferences.layout == Layout::shelf &&
              displayed.preferences.favorite[find_system("n64")], "Shelf layout and existing choices survive reopen");
    check(!save_settings("missing-parent/does-not-exist/settings.ini", read), "unwritable save reports failure");
    char invalid[160];
    std::snprintf(invalid, sizeof(invalid), "%s/invalid.ini", directory);
    write(invalid, "version=1\nlayout=grid\nlayout=swipe\nlook=icons\nscope=all\nsort=featured\nfilter=all\nicon="
                   "gaming\nbrand=\n");
    check(load_settings(invalid, read) == SettingsLoad::invalid, "duplicate keys are corruption, not missing settings");
    check(read.preferences.layout == Layout::grid && !read.preferences.hidden[find_system("ps2")],
          "unrecoverable corruption returns clean defaults");
    check(save_settings(invalid, Settings{}), "create a clean file for binary-corruption check");
    FILE* binary = std::fopen(invalid, "ab");
    check(binary != nullptr, "binary-corruption fixture");
    const char zero = 0;
    check(std::fwrite(&zero, 1, 1, binary) == 1, "binary-corruption fixture write");
    std::fclose(binary);
    check(load_settings(invalid, read) == SettingsLoad::invalid,
          "embedded NUL bytes cannot be accepted as text settings");
    std::puts("PASS: native settings roundtrip, stable IDs, atomic replacement, backup recovery, invalid data "
              "preservation and save failures.");
}
