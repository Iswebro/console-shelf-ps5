# Project references and credits

Console Shelf's current preview and native C++ UI are original code. The launcher and emulator reference implementations in the table below are not bundled; their public documentation and source informed the proposed PS5 integration. The native renderer separately uses the SDL, stb and font dependencies credited below. Names and trademarks remain with their respective owners.

| Project | Creator / publisher | How it informs Shelf | Source and license |
| --- | --- | --- | --- |
| [Jewel Case](https://github.com/SkaRift/JewelCase) | SkaRift | ROM discovery, emulator selection and game launch architecture proposed for the future native build | [Source](https://github.com/SkaRift/JewelCase), [GPL-3.0](https://github.com/SkaRift/JewelCase/blob/main/LICENSE) |
| [Estante](https://github.com/kauansilvalopes/estante) | Kauan Silva Lopes and contributors | Native PS5/PS4 title discovery and library architecture reference | [Source](https://github.com/kauansilvalopes/estante), [GPL-3.0-or-later](https://github.com/kauansilvalopes/estante/blob/main/LICENSE) |
| [PS5 RetroArch](https://github.com/mihawk-99/PS5_RetroArch) | Mihawk and contributors | RetroArch core and frontend game-request contract reference | [Source](https://github.com/mihawk-99/PS5_RetroArch) |
| [PS5SX2](https://github.com/Swordpdf/PS5SX2), [PSXS5](https://github.com/SynoPiia/PSXS5), [PS5-ECAM](https://github.com/premohq/PS5-ECAM), [ProsperoEden](https://github.com/Swordpdf/ProsperoEden) | Their respective authors | Example emulator routes in the demo catalogue | Linked source repositories |

## Native rendering dependencies

| Component | Creator / publisher | Use and terms |
| --- | --- | --- |
| [SDL2](https://github.com/libsdl-org/SDL/releases/tag/release-2.32.10) | Sam Lantinga and SDL contributors | Host window, controller input and software rendering. SDL2 2.32.10's official Windows package is downloaded into the ignored project cache and checked against GitHub's published SHA-256. The [zlib-style license](native/third_party/licenses/SDL2.txt) is retained; its binaries are not tracked in Git. The PS5 SDL port still requires build/link review. |
| [stb_image, stb_truetype, stb_image_write](https://github.com/nothings/stb) | Sean Barrett and contributors | Unmodified decoder/font/screenshot headers under `native/third_party/stb/`. Their MIT/Unlicense notices remain embedded; this project uses the MIT option. |
| [Poppins](https://github.com/google/fonts/tree/main/ofl/poppins) | Indian Type Foundry / Jonny Pinhorn and contributors | Unmodified Regular and SemiBold font files under `native/assets/fonts/`, with [SIL Open Font License 1.1](native/assets/fonts/OFL.txt). |

[Pinned source revisions and file hashes](native/third_party/SOURCE.json) identify the vendored stb headers, fonts and retained SDL license. The source headers/font downloads were compared to their pinned Git blob IDs before recording SHA-256. SDL's package has its own pinned release-asset digest in `tools/fetch-native-deps.js`.

No emulator binaries, payloads, games, BIOS, firmware, keys, account data, or complete reference-project source checkouts are bundled. Future incorporation of GPL reference code will retain the required copyright notices and source license.

Hardware image references and the generated icon credits are in [assets/CREDITS.md](assets/CREDITS.md). The original source photographs are linked there but are not included in the public repository.
