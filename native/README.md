# Native prototype work

The native target is C/C++ with SDL2. The HTML/JavaScript preview remains a design reference and a fast way to test interactions. It is not the runtime shipped to the PS5. The emulator still runs as its own app, so the preview's choice of language does not change emulator performance.

The C++17 development app now renders the console grid, floating-icon Swipe view, console details, settings sections, console visibility picker and a controller-operated search keyboard at 1920×1080. SDL controller events and the left stick drive the interface. Settings use stable console IDs, a verified temporary file, atomic replacement and a backup of the previous valid file. Loading damaged settings never rewrites that source file.

Hardware names, icon paths, brands and suggested cores are generated with `node tools/build-native-catalog.js`; demo games and installed-status labels are omitted. The PC build currently has **no discovery or launch connection**. Its empty inventory is expected and says nothing about the games on a PS5. Runtime adapters must supply real evidence. An existing game file can make a console visible without making its launch button available.

## Build and inspect on PC

On Windows with the existing Visual Studio C++ workload:

```sh
npm run build:native-host
npm run start:native-host
```

The build fetches the official SDL2 2.32.10 Windows development ZIP into ignored `.cache/native-deps/`, verifies its asset size and GitHub-published SHA-256, checks the archive's paths and extracts it there. It installs nothing globally. SDL's DLL is copied only into `.cache/native-host/`. Sources, decoder headers and licensed fonts are tracked in this repository. The interactive runner keeps its settings under `.cache/native-host/user-data/` and does not read the owner's game folders or connect to a PS5. On Linux, an existing C++17 compiler and SDL2 development installation with `sdl2-config` are required.

| DualSense | PC keyboard | Action |
| --- | --- | --- |
| D-pad / left stick | Arrow keys | Browse / choose a control |
| Cross | Enter | Select or change |
| Circle | Escape | Back / close settings |
| Options | F1 | Settings / finish search |
| Square | F2 | Sort & filter; delete while using the search keyboard |
| L1 / R1 | Shift+Tab / Tab | Settings sections |
| Triangle | — | Favourite; clear while using the search keyboard |
| — | F3 / F4 | Console picker / main search |

The host app also accepts typed text while its search keyboard is open. A first press of Circle on the home screen opens an exit confirmation.

## Verification

`npm run test:native-renderer` builds the app and drives it through its SDL event loop without opening a visible window. It captures seven 1080p screens, changes visibility through an independent picker search, uses controller buttons to type on the search keyboard, reopens the executable to verify persistence, and walks all 52 system images. The test checks the vendored file hashes first. Captures and machine-readable reports remain under ignored `.cache/native-render-tests/`. These are **simulated SDL controller events**, not a physical DualSense or PS5 test.

Run `npm run test:native` for the model, persistence and navigation tests. On Windows it uses an existing Visual Studio C++ installation, configures that compiler only in its child process and writes to ignored `.cache/native-host/`. On Linux it uses an existing `c++` compiler with C++17 support. It installs no packages.

The model tests also cover availability evidence, individual visibility, sorting, navigation, backup recovery, damaged settings and failed saves. All owned C++ sources compile with warnings treated as errors. The raster renderer keeps a bounded artwork cache and uses smaller font atlases for small text; it redraws when input or window state changes.

PS5 cross-compilation, VideoOut/direct-memory integration, physical controller testing, real discovery, launch adapters and packaging remain in the [prototype milestone](../docs/PROTOTYPE-PLAN.md). No PS5 download has been released yet. The runtime dependencies and font licenses are in [third-party notices](../THIRD_PARTY.md).
