# Main goal: first native PS5 prototype

Build isShelf as a native PS5 app and publish the first downloadable prototype in this repository once its package and core behaviour have been verified. The initial hardware target is firmware **12.20** with the existing itsPLK/Payload Manager setup. A successful PC test or cross-compile alone will not be labelled PS5 compatibility.

## Current progress

- [x] Public repository, source license, project references and image credits.
- [x] Console-first Grid, floating-icon Swipe and integrated room Shelf with 52 systems. The owner accepted UI-19's room, console-only paging and TV clearance.
- [x] Curved carousel with centre emphasis, progressive depth, short wrap motion and timed native renderer checks.
- [x] One settings entry with Appearance, Sort & filter, Consoles and Play room tabs; a separate direct layout toggle.
- [x] Deep counters, L1/R1 pages, an eight-console curated display, favourites-only browsing and optional reduced motion; native host and browser checks pass.
- [x] Personal TV modes and local silent intro-video playback. The owner accepted UI-20's smaller handheld proportions and confirmed that its sample video and static work. UI-21 adds original stills and animations for all 52 systems, personal overrides, a quiet room-menu backboard and fine-grain static; the TV stays visible in console libraries. These changes pass PC checks and await current-build physical acceptance.
- [x] Searchable individual console switches, whole-brand controls, Show/Hide results and saved visibility.
- [x] Original C++17 hardware catalogue and browsing model, compiled and tested on Windows with warnings treated as errors.
- [x] Pinned project-local PS5 SDK/SDL/Clang toolchain and successful native executable cross-build.
- [x] Separate unofficial identity, collision check, local UI-only test folder/ZIP, and full raw console file readbacks.
- [x] Native C++/SDL renderer, navigation model and settings persistence exercised on PC.
- [x] Simulated SDL controller event tests and 1080p renderer captures with persisted reopen.
- [x] PS5 VideoOut/direct-memory integration, host-mocked failure/cleanup checks and executable structure validation.
- [x] UI-22 owner acceptance on firmware 12.20: controls, saved choices, normal console restart, foreground PSXS5/PS5SX2/PS5SX3 menus and direct N64/PS1 RetroArch launch. The owner also confirmed Quick Menu and playlist switching after a Shelf-forwarded PS1 game.
- [x] UI-24 isShelf branding installed with 242 complete raw readbacks: the owner's screenshots confirm the Vector home tile and Glass and Light background. Vector/Gaming/Classic mint/Jade remain selectable and the launcher helper bytes are unchanged.
- [x] UI-24 TV-logo switching: the owner's later recording shows all four logo choices updating both the header and bundled TV intro badge. A fresh raw program hash still matches the installed candidate; the earlier unresolved report is retained as history.
- [x] UI-24 final functional acceptance: the owner separately confirmed the exact-package restart, layouts, saved settings, foreground PSXS5/PS5SX2/PS5SX3 menus and direct RetroArch Nintendo 64 launch. The same build's log confirms the three-second close gesture. The accepted program and full package manifest are pinned in the final owner record.
- [x] Read-only emulator/core and game discovery in configured existing library locations, checked from PC against the owner's console.
- [x] The same configured discovery inside the PS5 app sandbox, with complete scan evidence.
- [ ] Further dedicated-emulator and library-layout adapters.
- [x] Four initial firmware 12.20 launch tests on UI-15: PSXS5/PS5SX2/PS5SX3 menus foreground; Mario Party 3 through RetroArch starts directly.
- [x] Complete linked-library sources and a private source/object kit, cold-relinked to the earlier UI-only executable.
- [x] Matching UI-20 complete source/relink kit: a fresh compiler prefix and rebuilt converter reproduce its exact ELF and SELF. The 19 application objects were retained during that cold relink. A separate clean build then recompiled every application/helper source inside the extracted archive and reproduced the exact installed ELF, SELF and source fingerprint without reusing application objects.
- [x] UI-20 release-input audit binds all 84 installed files to a fresh full console ledger, checks all 312 source-kit members, reproduces the tile icon, and compares 79 source assets/notices. LICENSE differs only in CRLF/LF; both original hashes are retained.
- [x] Matching UI-21 source/relink archive with 475 members: a fresh compiler prefix and rebuilt converter reproduce the installed ELF and SELF exactly. A separate clean build recompiles all 19 application objects and the helper, reproducing the exact program and source fingerprint. Its input audit binds all 241 console readbacks, reproduces the tile icon and compares 236 assets/notices, retaining the same bounded LICENSE line-ending difference.
- [ ] Source-linked prototype ZIP, SHA-256, install/rollback instructions and first release.

## Runtime decision

Use C/C++ with SDL2 for the installed app. The JavaScript/HTML preview remains the interaction and visual reference; a PS5 owner will not need Node.js or the PC preview to run the native Shelf. Artwork and catalogue metadata can be reused. Controller input, drawing, persistence and console integration require native implementation.

[Jewel Case](https://github.com/SkaRift/JewelCase) is the primary reference for the native runtime and emulator launch architecture. Its reviewed display/memory/shim components and underlying BlackBearReloaded startup/build/runtime source are now incorporated with pinned provenance and original notices. [Estante](https://github.com/kauansilvalopes/estante) remains a reference for native PS5/PS4 discovery. Shelf keeps its original catalogue, console-first UI and artwork; see [credits](../THIRD_PARTY.md).

The `native/` model uses only hardware metadata and suggested core names. Installed status and game counts start empty and must come from runtime discovery, rather than the PC demo. Visibility is independent of launch capability: a system may have game files while no compatible emulator is detected. Saved choices will use system IDs so catalogue reordering cannot change the owner's preferences.

## First prototype acceptance

1. **Open and navigate:** launch, browse, open settings, choose a console, back out and close cleanly using the DualSense. Test Grid, Swipe and Shelf on a television, including a normal console restart and reopen.
2. **Personalise:** persist layout, sort, filters, favorites, per-console visibility and preferred emulator in Shelf-owned storage using atomic writes. Recover gracefully from missing or damaged settings.
3. **Discover real libraries:** identify installed emulators and native titles; scan verified existing game roots and playlists. Exclude app dependencies, BIOS, keys, saves, temporary files and incomplete downloads from the game list. Deduplicate without moving game files.
4. **Launch honestly:** distinguish a verified direct-game route, an emulator-menu route and an unavailable route. Validate the loader and destination title before issuing a request. Start with the existing N64/RetroArch, PS1, PS2 and PS3 libraries. Add other adapters when their contracts are verified. Reference-only hardware remains browseable in All consoles and has no launch action.
5. **Protect the setup:** use a separate app identity after checking for collisions, preserve existing apps and game data, retain firmware 12.20 and current payload order. Any required administrator, network or payload-configuration change needs separate approval.
6. **Release evidence:** validate the final ELF/package layout, verify transferred file hashes, test the packaged build on the console, and record the exact supported launch routes and dependencies. Publish that same artifact with SHA-256, matching source commit, complete notices and simple installation, recovery and removal instructions. List all remaining limitations openly.

## Immediate engineering steps

The native C++ renderer and settings flow run on Windows with the existing Visual Studio compiler and a pinned, SHA-256-verified official SDL2 package in the project cache. Fonts and decoder sources are pinned and credited. Its SDL event tests are simulated. The owner confirmed the grid, settings navigation and closing fade on firmware 12.20. The console measured a 3009 ms Circle hold and a completed fade at 3393 ms. Static PS5 selection artwork is confirmed; opening still shows black before the shelf and remains a documented limitation.

The PS5 executable now cross-compiles using verified public SDK/PacBrew releases and a Clang 18 prefix extracted from pinned, signed-metadata-verified Ubuntu packages. Everything stays under the project cache; no apt, sudo or global configuration change is required. The clean-room runtime rebuilds twice to the reference hashes. Host mocks cover native display/memory failure paths, and the executable passes container integrity and independent ELF structure checks; see [build evidence scope](PS5-BUILD.md).

Read-only discovery works from PC and inside the private PS5 app, with bounded metadata parsing, core evidence, disc references and real native game lists. The latest PC scan of configured console roots found 91 games and 55 routes; an earlier in-app journal recorded 92 and 55. This covers configured roots, not every dedicated-emulator layout. UI-21 has 241 complete raw file readbacks with preferences preserved during installation, 62 native host captures, browser room/video checks and bounded MPEG decoding checks for all 52 original animations. The [installation](verification/ui21-console-install-2026-10-25.json), [PC checks](verification/ui21-pc-checks-2026-10-25.json), [exact cold relink](verification/ui21-source-kit-relink-2026-10-25.json), [clean source rebuild](verification/ui21-clean-source-build-2026-10-25.json) and [input audit](verification/ui21-release-input-audit-2026-10-25.json) are separate from physical acceptance. Earlier revisions retain their own historical evidence.

The owner confirmed UI-19's room, L1/R1 paging, clock display, Circle closure and saved clock after reopening; UI-20 adds accepted handheld proportions and confirmed native video/static capability. UI-15 retains four owner-accepted launch baselines with the same helper bytes. Each revision keeps its own acceptance ledger; earlier results are not relabelled as current-build tests. PSXS5 currently opens its own game shelf, so a selected PS1 game is labelled as a menu route. Recheck the final candidate's launch routes and normal console restart before publishing. Keep private console evidence separate from public source.

`npm run verify:ps5-release-inputs` checks the existing app and source archives without preparing a release or evaluating hardware acceptance. `npm run test:ps5-release-inputs` covers malformed or incomplete ledgers, changed assets/notices, source membership and standalone guide links. The real release still requires the owner-check file bound to the full tested package manifest. Its ZIP includes the room guide, with links that resolve from both the root README and app's START-HERE.
