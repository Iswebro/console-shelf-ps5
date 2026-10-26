# Private first UI test

The local test folder is separate from the first public release. It tests the shared native renderer, DualSense input, persistence, console-local read-only discovery and an **experimental cooperative launch handoff**. The app embeds isShelf's original on-demand helper for the existing console-local loader. The owner confirmed the grid, settings navigation, compact Circle hint and three-second hold-to-close on firmware 12.20 in earlier revisions. The opening transition remains a known limitation.

Earlier launch candidates left emulators behind the home screen. The corrected UI-15 helper makes one SystemService launch request, then reads the shell focus flag without sending another launch. The owner confirmed foreground PSXS5, PS5SX2 and PS5SX3 menus and direct RetroArch games in subsequent UI-22/UI-23 tests. UI-24 retains those exact helper bytes; the owner confirmed its final restart and launch pass. A reported process or focus flag does not prove TV output. The accepted UI-24 program is published as [v0.1.0-beta.1](https://github.com/Iswebro/console-shelf-ps5/releases/tag/v0.1.0-beta.1).

Earlier startup diagnostics identified a missing strong lifecycle import. The executable verifier now rejects weak, missing or misbound required shell imports. Hardware and launch acceptance are tracked separately from compilation and structural checks.

The unofficial test title identity is `PPSA99541`, shown as **isShelf** in UI-24. Earlier console receipts retain the former **Console Shelf (UI test)** name. It is not a globally reserved ID. The owner's console was checked for a collision; every installation must repeat that check and refuse to overwrite any existing title. The target app folder is `/data/homebrew/PPSA99541`; preferences are written only to `/data/console-shelf/settings.ini` and its recovery files. Existing game libraries are read in place.

## Current candidate: UI-24

The program built from `5c34e67aba3e51ca209d56215f97b586ff58c1e5` has SELF SHA-256 `9834451dc2ca0f59f6fe487900ebe3ab55f08b23f53fb9aabf16faaac2eda88e`. Its [242 complete console readbacks](verification/ui24-console-install-2026-10-26.json) preserved preferences. The registration refresh kept the program folder and verified both registered metadata caches against the packaged artwork. The owner's screenshots confirm the isShelf name, Vector tile, Glass and Light background and Vector mark inside a bundled TV intro.

The earlier unchanged-TV-logo report is retained in the [partial owner record](verification/ui24-owner-checks-2026-10-26.json). The owner's [later console recording](verification/ui24-owner-tv-video-check-2026-10-26.json) shows Vector, Gaming, Classic mint and Jade updating both the header and TV badge, then returning to Vector. That resolves the visual issue on the actual console. A fresh raw program hash and startup marker still match UI-24. The owner then [confirmed the final restart, layouts, persistence and launch sequence separately](verification/ui24-owner-final-checks-2026-10-26.json). The same build's log records Circle holds of 3032 and 3017 milliseconds, followed by completed closing fades.

The [PC hot-switch test](verification/ui24-tv-logo-hot-switch-2026-10-26.json) remains separate evidence. The matching [complete source kit cold-relinks exactly](verification/ui24-source-kit-relink-2026-10-26.json); a [clean rebuild](verification/ui24-clean-source-build-2026-10-26.json) also reproduces the installed ELF, SELF and source fingerprint. The [release input audit](verification/ui24-release-input-audit-2026-10-26.json) passes without substituting for hardware acceptance. The [first public beta](https://github.com/Iswebro/console-shelf-ps5/releases/tag/v0.1.0-beta.1) is published; [all public downloads](verification/v0.1.0-beta.1-publication-2026-10-26.json) match their declared hashes.

## Assemble locally

With reviewed sources committed and the verified executable current:

```sh
npm run build:ps5
npm run verify:ps5
npm run prepare:ps5-ui-test
```

Default builds retain the restricted helper runtime, which refuses the running-app query on the owner's console. The explicitly approved alternative uses `node tools/build-ps5.js build_ps5.py --standard-helper-runtime` instead of the first build command. It retains the SDK's normal process-capability/syscall initialization in RAM; obtain the console owner's approval before using this variant. Build and package receipts label the choice. See [integration scope](INTEGRATION.md).

The preparation command compiles a small host-only artwork converter with the existing C++ compiler. It makes a 512×512 app tile from Shelf's existing logo using integer area resampling with premultiplied alpha; the original image is unchanged. It adds the 52 console illustrations, licensed fonts, the source-built clean-room runtime, credits and license texts to an isolated timestamped folder under ignored `.cache/ps5/test-packages/`.

The assembler requires a clean Git tree, a source fingerprint matching the tested executable, recorded binary hashes and the expected runtime hash. It records the exact source commit and every packaged file's size/SHA-256. The local ZIP receives a full integrity check and each member is compared to its manifest. The receipt is `.cache/ps5/test-packages/latest.json`. The command makes no console connection or installation, changes no OS settings and publishes nothing.

This private UI package is **not for public redistribution**. The complete corresponding library sources and relinking materials remain a release requirement, along with successful hardware and launch tests.

## Console check

Use the owner's established local FTP transfer workflow, with no game or download running. Recheck the title identity, create only the new owned app folder, and verify complete raw file readbacks. Before reading `eboot.bin`, ftpsrv must explicitly report SELF conversion disabled. Keep firmware 12.20 and the current payload order.

After exposing the new tile through the already established ShadowMount procedure:

1. Open **isShelf (UI test)** and check that the picture is stable and fits the TV.
2. Test D-pad/stick and Cross/Circle. From the search/layout row, Up focuses the settings button; Down returns to layout. Options opens the single settings panel; Square or the labelled layout button cycles Grid/Swipe/Shelf. A short Circle tap on the home page shows a small three-second hint; on other pages it remains Back. Hold Circle for three seconds to check its progress indicator and closing fade. Releasing early leaves the app open. The owner accepted the compact hint and closing behavior. Opening still shows black before the grid; submitted diagnostic frames do not prove a visible opening animation. Startup PNG probes are disabled by default.
3. Select Nintendo 64 and its game library; check Mario Party and the other existing titles. Check PS1, PS2 and PS3 as well.
4. Change a layout/visibility choice, close Shelf, then reopen it to check persistence.
5. Report any black screen, crash, missing input or incorrect library count. Avoid repeated attempts after a console hang.
6. Open a game library and read its persistent launch panel before pressing Cross. **Library only / Open emulator library** means choosing the game inside the emulator; **Direct play / Play game** passes the selected game to RetroArch. The panel and footer must change when Preferred emulator changes, and remain visible while browsing. Close and reopen Shelf to check a saved RetroArch preference.
7. Choose PlayStation 2 → Open emulator library once, then wait up to ten seconds without pressing PS. Check that PS5SX2 appears on the TV, rather than remaining behind the home screen. Then test PS1/PS3 menu routes and selected Nintendo 64 and PS1 games through RetroArch. The helper passes game arguments once and stops observing focus if the target changes; it never retries the launch.

No binary release should be labelled compatible with firmware 12.20 until these results and the later supported launch routes are recorded. Remove only Shelf's owned app folder to roll back the program, retaining its preferences if wanted.

## PS5-managed launch picture

The native template supports static selection and launch backgrounds as `sce_sys/pic0.dds` and `pic1.dds`: 3840x2160 BC7_UNORM DX10 DDS, one 2D image and one mip level. Shelf now supplies its own matching artwork, generated from the native renderer's `launch-art` scene, with the original PNG and conversion provenance retained. `python tools/verify_ps5_presentation.py` checks complete payload sizes, image headers, PNG CRCs and SHA-256 pins; the package assembler includes both backgrounds. The Microsoft encoder is not bundled.

These are standard PS5-managed static presentation assets; the moving transition remains app code. The owner confirms the selection background appears, but opening the app still shows black before the shelf. This is not an accepted launch-animation fix. The candidate disables the two diagnostic PNG writes in normal startup to remove their measured delay; developers can explicitly define `SHELF_CAPTURE_STARTUP` for the bounded probes.

## UI-20 follow-up checks

The [PC evidence](verification/ui20-pc-checks-2026-10-25.json) records 57 native frames, browser checks and real MPEG decoding; this does not claim physical PS5 acceptance. The launch helper remains byte-identical to the owner-tested UI-15 helper.

- In Shelf layout, change to page two with R1. Up/Down should move between its two rows; Up from the top row reaches the toolbar without going back through earlier pages.
- Keep Clock or My message selected, then enter a console and its game library. The TV should retain that display. The upper-row artwork should clear the screen's lower edge.
- Choose Play room → Console intro video. Select an installed console: the labelled Shelf demo should move inside the TV. Your own compatible clip in Shelf's data folder replaces it. See the [room guide](ROOM.md).
- Try the optional static policy for a console without an emulator. Reduced motion should freeze grain and video. Restore your preferred display afterward.
- PSXS5 currently opens its own library. Its inspected startup and companion source do not expose a supported direct-game command. Shelf labels that route accordingly; RetroArch's supported game/core forwarder remains separate. No PSXS5 settings are changed to imitate a direct launch.

The normal-console-restart check remains deferred at the owner's request. Keep firmware 12.20 and the established payload sequence.

An existing title without these assets needs its own registration refreshed before the PS5 adopts them. First verify that ShadowMountPlus reports this exact title as an owned, managed folder at `/data/homebrew/PPSA99541`, not a package or image. With apps closed, retain a full raw program backup and the owned preferences, add/verify both artwork files, and use the existing ShadowMountPlus title-only uninstall/scan API to register that same folder again. Never use its source-deletion operation. Recheck every program/artwork hash and the preference bytes afterward. This registration refresh was verified for the owner's title; it need not be repeated for an ordinary program update. Keep firmware 12.20 and leave other titles, PSN, global shell settings and payload order untouched.
