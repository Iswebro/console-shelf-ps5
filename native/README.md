# Native prototype work

The native target is C/C++ with SDL2. The HTML/JavaScript preview remains a design reference and a fast way to test interactions. It is not the runtime shipped to the PS5. The emulator still runs as its own app, so the preview's choice of language does not change emulator performance.

The C++17 development app now renders the console grid, floating-icon Swipe view, console details, settings sections, console visibility picker and a controller-operated search keyboard at 1920×1080. SDL controller events and the left stick drive the interface. Settings use stable console IDs, a verified temporary file, atomic replacement and a backup of the previous valid file. Loading damaged settings never rewrites that source file.

Hardware names, icon paths, brands and suggested cores are generated with `node tools/build-native-catalog.js`; demo games and installed-status labels are omitted. The PC build starts with an empty inventory unless an explicit read-only library source is supplied. Its [discovery adapter](../docs/DISCOVERY.md) now identifies real installed cores and supported library folders, with paginated game lists and saved preferred-emulator choices. No launch capability is inferred from discovery. The owner has confirmed the grid and basic PS5 input; the refined controls, console-local connection and complete hardware acceptance still need testing.

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

Swipe is a curved carousel with a larger centre console, progressively smaller neighbours, subtle yaw and a quiet floor glow. Left/right loop at either end, while Up reaches the toolbar. Its 420 ms ease-out retargets from the current frame without queuing moves; drawing stops once it settles. Grid navigation stays bounded. Display sizing uses the visible illustration's bounds, keeping tall hardware intact and wide hardware readable without changing the PNG files.

## Verification

`npm run test:native-renderer` builds the app and drives it through its SDL event loop without opening a visible window. It captures twenty-one 1080p screens, changes visibility through an independent picker search, uses controller buttons to type on the search keyboard, reopens the executable to verify persistence, and walks all 52 system images. Timed carousel and startup transitions must finish pixel-identically to a settled reference pose. Circle hold/cancel/fade tests retain ordinary Back taps. The FTP integration test also commits background inventory and cancels a deliberately stalled greeting before its network timeout. The test checks the vendored file hashes first. Captures and machine-readable reports remain under ignored `.cache/native-render-tests/`. These are **simulated SDL controller events**, not full physical DualSense or PS5 acceptance.

Run `npm run test:native` for the model, persistence, discovery and navigation tests. On Windows it uses an existing Visual Studio C++ installation, configures that compiler only in its child process and writes to ignored `.cache/native-tests/`. On Linux it uses an existing `c++` compiler with C++17 support. It installs no packages. After building the host, `npm run test:library-ftp` checks the read-only protocol against a loopback-only fake server without contacting a console.

The model tests also cover availability evidence, individual visibility, sorting, carousel wrap/depth/retargeting, navigation, backup recovery, damaged settings and failed saves. All owned C++ sources compile with warnings treated as errors. The raster renderer keeps a bounded artwork cache and uses smaller font atlases for small text; it redraws on changes and while a carousel movement is settling.

## PS5 cross-build

`npm run build:ps5` now builds the native executable using a pinned compiler prefix and verified public SDK/SDL archives inside ignored `.cache/ps5/`. Windows uses an existing WSL Ubuntu instance; the build does not install packages globally or connect to a console. `npm run test:ps5-platform` runs host mocks against the actual allocator/VideoOut failure and cleanup paths. `npm run verify:ps5` checks the executable's structure independently of its packager. See the [PS5 build instructions and limits](../docs/PS5-BUILD.md).

VideoOut/direct-memory integration and real discovery are implemented and cross-compiled. Discovery has been checked from PC against the owner's console. The private app identity and raw transfers are verified, with basic on-console display/input confirmed. Full controller acceptance, discovery inside the app sandbox and launch adapters remain in the [prototype milestone](../docs/PROTOTYPE-PLAN.md). One settings cog opens the configuration panel; the separate Grid/Swipe action switches layout immediately (Square on the home screen). No PS5 download has been released yet. The runtime dependencies and font licenses are in [third-party notices](../THIRD_PARTY.md).
