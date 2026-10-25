# Main goal: first native PS5 prototype

Build Console Shelf as a native PS5 app and publish the first downloadable prototype in this repository once its package and core behaviour have been verified. The initial hardware target is firmware **12.20** with the existing itsPLK/Payload Manager setup. A successful PC test or cross-compile alone will not be labelled PS5 compatibility.

## Current progress

- [x] Public repository, source license, project references and image credits.
- [x] Console-first Grid and floating-icon Swipe design with 52 system illustrations.
- [x] Compact settings icons opening Appearance, Sort & filter and Consoles panels.
- [x] Searchable individual console switches, whole-brand controls, Show/Hide results and saved visibility.
- [x] Original C++17 hardware catalogue and browsing model, compiled and tested on Windows with warnings treated as errors.
- [ ] Reproducible native PS5 SDK/SDL toolchain and package identity.
- [ ] Native renderer, controller navigation and settings persistence.
- [ ] Runtime emulator and game discovery in existing library locations.
- [ ] Verified launch adapters and firmware 12.20 console tests.
- [ ] Source-linked prototype ZIP, SHA-256, install/rollback instructions and first release.

## Runtime decision

Use C/C++ with SDL2 for the installed app. The JavaScript/HTML preview remains the interaction and visual reference; a PS5 owner will not need Node.js or the PC preview to run the native Shelf. Artwork and catalogue metadata can be reused. Controller input, drawing, persistence and console integration require native implementation.

[Jewel Case](https://github.com/SkaRift/JewelCase) is the primary reference for the native runtime and emulator launch architecture. [Estante](https://github.com/kauansilvalopes/estante) is the reference for native PS5/PS4 discovery. Reuse only reviewed components with their source and copyright notices; keep Shelf's own console-first UI and art. The current native model is original code and does not yet contain either project's implementation.

The `native/` model uses only hardware metadata and suggested core names. Installed status and game counts start empty and must come from runtime discovery, rather than the PC demo. Visibility is independent of launch capability: a system may have game files while no compatible emulator is detected. Saved choices will use system IDs so catalogue reordering cannot change the owner's preferences.

## First prototype acceptance

1. **Open and navigate:** launch, browse, open settings, choose a console, back out and close cleanly using the DualSense. Test Grid and Swipe on a television, including a normal console restart and reopen.
2. **Personalise:** persist layout, sort, filters, favorites, per-console visibility and preferred emulator in Shelf-owned storage using atomic writes. Recover gracefully from missing or damaged settings.
3. **Discover real libraries:** identify installed emulators and native titles; scan verified existing game roots and playlists. Exclude app dependencies, BIOS, keys, saves, temporary files and incomplete downloads from the game list. Deduplicate without moving game files.
4. **Launch honestly:** distinguish a verified direct-game route, an emulator-menu route and an unavailable route. Validate the loader and destination title before issuing a request. Start with the existing N64/RetroArch, PS1, PS2 and PS3 libraries. Add other adapters when their contracts are verified. Reference-only hardware remains browseable in All consoles and has no launch action.
5. **Protect the setup:** use a separate app identity after checking for collisions, preserve existing apps and game data, retain firmware 12.20 and current payload order. Any required administrator, network or payload-configuration change needs separate approval.
6. **Release evidence:** validate the final ELF/package layout, verify transferred file hashes, test the packaged build on the console, and record the exact supported launch routes and dependencies. Publish that same artifact with SHA-256, matching source commit, complete notices and simple installation, recovery and removal instructions. List all remaining limitations openly.

## Immediate engineering steps

Establish a pinned build environment for the official public PS5 payload SDK and SDL2, then bring up a small native renderer using the compiled catalogue/model and existing transparent art. The Windows host model check already uses the installed Visual Studio compiler without changing system settings. The current Ubuntu/WSL instance lacks the native compiler, make and SDL development packages; an isolated build environment or an explicitly approved package setup is still needed for PS5 cross-compilation.

Next, port the settings and controller flow to that renderer before connecting read-only discovery and launch helpers. Keep local PS5 test evidence separate from the public source. There is no prototype download until the acceptance steps above are met.
