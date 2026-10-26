# Native prototype work

The native target is C/C++ with SDL2. The HTML/JavaScript preview remains a design reference and a fast way to test interactions. It is not the runtime shipped to the PS5. The emulator still runs as its own app, so the preview's choice of language does not change emulator performance.

The C++17 development app renders Grid, floating-icon Swipe and display Shelf layouts, console details, settings sections, console visibility picker and a controller-operated search keyboard at 1920×1080. Shelf places eight consoles on the deep display counters built into a full living-room background. Measured room coordinates align the artwork and contact shadows to the actual wood planes. The compact Shelf header leaves both rows clear. The base room and Shelf room are each resampled once at startup, avoiding a large image resize on each software-rendered frame. SDL controller events and the left stick drive the interface. Appearance offers an optional black-and-white style for systems without a detected emulator. It defaults on, leaves built-in PS4/PS5 in colour, and never treats game files alone as emulator evidence. Old schema-1 preferences remain readable; the colour choice saves independently of visibility and layout. Settings use stable console IDs, a verified temporary file, atomic replacement and a backup of the previous valid file. Loading damaged settings never rewrites that source file.

Hardware names, icon paths, brands and suggested cores are generated with `node tools/build-native-catalog.js`; demo games and installed-status labels are omitted. The PC build starts with an empty inventory unless an explicit read-only library source is supplied. Its [discovery adapter](../docs/DISCOVERY.md) identifies real installed cores and supported library folders, with paginated game lists and saved preferred-emulator choices. The configured scan also completes inside the private PS5 app. No launch capability is inferred from discovery. The owner accepted UI-24’s final navigation, settings, restart, persistence and initial launch routes on firmware 12.20. The [first beta](https://github.com/Iswebro/console-shelf-ps5/releases/tag/v0.1.0-beta.1) includes that tested program; the known black opening gap and experimental routes remain documented.

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
| Square | F2 | Cycle Grid → Swipe → Shelf on the home screen; delete while using the search keyboard |
| L1 / R1 | Shift+Tab / Tab | Settings sections |
| Triangle | — | Favourite; clear while using the search keyboard |
| — | F3 / F4 | Console picker / main search |

The host app also accepts typed text while its search keyboard is open. A short Circle tap on the home screen shows a compact hint that fades away after three seconds. Hold Circle for three seconds to close with the usual fade.

Swipe is a curved carousel with a larger centre console, progressively smaller neighbours, subtle yaw and a quiet floor glow. Left/right loop at either end, while Up reaches the toolbar. Its 420 ms ease-out retargets from the current frame without queuing moves; drawing stops once it settles. Grid navigation stays bounded. Display sizing uses the visible illustration's bounds, keeping tall hardware intact and wide hardware readable without changing the PNG files.

## Verification

`npm run test:native-renderer` builds the app and drives it through its SDL event loop without opening a visible window. Its 47 captures include the room TV modes, console-only page swaps, visibility changes, search keyboard, persistence, and all 52 system illustrations. Fixed wood and floor pixels must remain identical through page motion; the final pose must match the unanimated page. Timed carousel and startup transitions must finish pixel-identically to a settled reference pose; heavy scene preparation stays covered by the loading picture. Spatial toolbar tests cover Up to settings and Down back to layout. Three-second Circle hold/cancel/fade tests retain ordinary Back taps. The separate FTP integration check covers background inventory and cancellation before network timeout. Captures and reports remain under ignored `.cache/native-render-tests/`. These are **simulated SDL controller events**, not full physical DualSense or PS5 acceptance. `npm run test:room` checks the browser's capped display, TV geometry, personal-picture persistence, text safety, favourite-game rotation, intro scope and phone layout. See [room setup](../docs/ROOM.md).

Run `npm run test:native` for the model, persistence, discovery and navigation tests. On Windows it uses an existing Visual Studio C++ installation, configures that compiler only in its child process and writes to ignored `.cache/native-tests/`. On Linux it uses an existing `c++` compiler with C++17 support. It installs no packages. After building the host, `npm run test:library-ftp` checks the read-only protocol against a loopback-only fake server without contacting a console.

The model tests also cover availability evidence, individual visibility, sorting, carousel wrap/depth/retargeting, navigation, backup recovery, damaged settings and failed saves. All owned C++ sources compile with warnings treated as errors. The raster renderer keeps a bounded artwork cache and uses smaller font atlases for small text; it redraws on changes and while a carousel movement is settling.

## PS5 cross-build

`npm run build:ps5` now builds the native executable using a pinned compiler prefix and verified public SDK/SDL archives inside ignored `.cache/ps5/`. Windows uses an existing WSL Ubuntu instance; the build does not install packages globally or connect to a console. `npm run test:ps5-platform` runs host mocks against the actual allocator/VideoOut failure and cleanup paths. `npm run verify:ps5` checks the executable's structure independently of its packager. See the [PS5 build instructions and limits](../docs/PS5-BUILD.md).

VideoOut/direct-memory integration and real discovery are implemented and cross-compiled. Discovery has been checked from PC and inside the console app. The private app identity and raw transfers are verified, with display/input, settings navigation and the closing fade confirmed. The opening transition still appears black before the shelf. An experimental launch candidate embeds an original cooperative helper; PC tests cover its route planning and loopback transport, with the owner confirming UI-15 foreground opening for PSXS5, PS5SX2 and PS5SX3, and direct Mario Party 3 launch through RetroArch. Full acceptance remains in the [prototype milestone](../docs/PROTOTYPE-PLAN.md). One settings cog opens the configuration panel; Square cycles Grid/Swipe/Shelf on the home screen. Hold Circle for three seconds to close. No PS5 download has been released yet. The runtime dependencies and font licenses are in [third-party notices](../THIRD_PARTY.md).
