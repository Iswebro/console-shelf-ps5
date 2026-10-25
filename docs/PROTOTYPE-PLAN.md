# Main goal: first native PS5 prototype

Build Console Shelf as a native PS5 app and publish the first downloadable prototype in this repository once its package and core behaviour have been verified. The initial hardware target is firmware **12.20** with the existing itsPLK/Payload Manager setup. A successful PC test or cross-compile alone will not be labelled PS5 compatibility.

## Current progress

- [x] Public repository, source license, project references and image credits.
- [x] Console-first Grid and floating-icon Swipe design with 52 system illustrations.
- [x] Curved carousel with centre emphasis, progressive depth, short wrap motion and timed native renderer checks.
- [x] One settings entry with Appearance, Sort & filter and Consoles tabs; a separate direct layout toggle.
- [x] Searchable individual console switches, whole-brand controls, Show/Hide results and saved visibility.
- [x] Original C++17 hardware catalogue and browsing model, compiled and tested on Windows with warnings treated as errors.
- [x] Pinned project-local PS5 SDK/SDL/Clang toolchain and successful native executable cross-build.
- [x] Separate unofficial identity, collision check, local UI-only test folder/ZIP, and full raw console file readbacks.
- [x] Native C++/SDL renderer, navigation model and settings persistence exercised on PC.
- [x] Simulated SDL controller event tests and 1080p renderer captures with persisted reopen.
- [x] PS5 VideoOut/direct-memory integration, host-mocked failure/cleanup checks and executable structure validation.
- [ ] Physical PS5 display/controller validation on firmware 12.20.
- [x] Read-only emulator/core and game discovery in configured existing library locations, checked from PC against the owner's console.
- [x] The same configured discovery inside the PS5 app sandbox, with complete scan evidence.
- [ ] Further dedicated-emulator and library-layout adapters.
- [ ] Verified launch adapters and firmware 12.20 console tests.
- [x] Complete linked-library sources and a private source/object kit, cold-relinked to the exact executable.
- [ ] Source-linked prototype ZIP, SHA-256, install/rollback instructions and first release.

## Runtime decision

Use C/C++ with SDL2 for the installed app. The JavaScript/HTML preview remains the interaction and visual reference; a PS5 owner will not need Node.js or the PC preview to run the native Shelf. Artwork and catalogue metadata can be reused. Controller input, drawing, persistence and console integration require native implementation.

[Jewel Case](https://github.com/SkaRift/JewelCase) is the primary reference for the native runtime and emulator launch architecture. Its reviewed display/memory/shim components and underlying BlackBearReloaded startup/build/runtime source are now incorporated with pinned provenance and original notices. [Estante](https://github.com/kauansilvalopes/estante) remains a reference for native PS5/PS4 discovery. Shelf keeps its original catalogue, console-first UI and artwork; see [credits](../THIRD_PARTY.md).

The `native/` model uses only hardware metadata and suggested core names. Installed status and game counts start empty and must come from runtime discovery, rather than the PC demo. Visibility is independent of launch capability: a system may have game files while no compatible emulator is detected. Saved choices will use system IDs so catalogue reordering cannot change the owner's preferences.

## First prototype acceptance

1. **Open and navigate:** launch, browse, open settings, choose a console, back out and close cleanly using the DualSense. Test Grid and Swipe on a television, including a normal console restart and reopen.
2. **Personalise:** persist layout, sort, filters, favorites, per-console visibility and preferred emulator in Shelf-owned storage using atomic writes. Recover gracefully from missing or damaged settings.
3. **Discover real libraries:** identify installed emulators and native titles; scan verified existing game roots and playlists. Exclude app dependencies, BIOS, keys, saves, temporary files and incomplete downloads from the game list. Deduplicate without moving game files.
4. **Launch honestly:** distinguish a verified direct-game route, an emulator-menu route and an unavailable route. Validate the loader and destination title before issuing a request. Start with the existing N64/RetroArch, PS1, PS2 and PS3 libraries. Add other adapters when their contracts are verified. Reference-only hardware remains browseable in All consoles and has no launch action.
5. **Protect the setup:** use a separate app identity after checking for collisions, preserve existing apps and game data, retain firmware 12.20 and current payload order. Any required administrator, network or payload-configuration change needs separate approval.
6. **Release evidence:** validate the final ELF/package layout, verify transferred file hashes, test the packaged build on the console, and record the exact supported launch routes and dependencies. Publish that same artifact with SHA-256, matching source commit, complete notices and simple installation, recovery and removal instructions. List all remaining limitations openly.

## Immediate engineering steps

The native C++ renderer and settings flow now run on Windows with the existing Visual Studio compiler and a pinned, SHA-256-verified official SDL2 package in the project cache. Fonts and decoder sources are pinned and credited. The software-rendered host uses the same console artwork and controller-navigation model intended for the PS5. Its SDL event tests are simulated. The owner has confirmed the grid and basic controls on firmware 12.20; efficient navigation and the full hardware acceptance remain unproven.

The PS5 executable now cross-compiles using verified public SDK/PacBrew releases and a Clang 18 prefix extracted from pinned, signed-metadata-verified Ubuntu packages. Everything stays under the project cache; no apt, sudo or global configuration change is required. The clean-room runtime rebuilds twice to the reference hashes. Host mocks cover native display/memory failure paths, and the executable passes container integrity and independent ELF structure checks; see [build evidence scope](PS5-BUILD.md).

Read-only discovery works from PC and inside the private PS5 app, with bounded metadata parsing, core evidence, disc reference handling and real native game lists. The sixth build displays its first loading frame before starting the background scan; its console journal records 92 games, 55 routes and no connection failures. This covers the configured roots, not every possible dedicated-emulator layout. Every updated program file matched its full raw readback. The owner confirmed basic display/input on an earlier build; refined controls and loading/closing transitions still need acceptance. A private source/object kit has reproduced the exact executable after extraction with a fresh compiler prefix and rebuilt converter. Further adapters, verified launches and complete hardware acceptance remain before publication. Keep private console evidence separate from public source.
