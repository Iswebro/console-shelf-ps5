# Private first UI test

The local test folder is separate from the first public release. It tests the shared native renderer, DualSense input, persistence, console-local read-only discovery and an **experimental cooperative launch handoff**. The app embeds Shelf's original on-demand helper for the existing console-local loader. The owner confirmed the grid, settings navigation, compact Circle hint and three-second hold-to-close on firmware 12.20. Discovery reports 92 games and 55 emulator/core routes without connection failures. The opening transition remains a known limitation.

The previous standard-runtime helper starts PS5SX2 and RetroArch, but both remain behind the PS5 home screen. A delayed running-title request also failed this test on firmware 12.20. The current candidate makes one SystemService launch request, then reads the shell focus flag without sending another launch. A reported process or focus flag does not prove TV output. Check the TV result before accepting a launch route. No public download is available.

Earlier startup diagnostics identified a missing strong lifecycle import. The executable verifier now rejects weak, missing or misbound required shell imports. Hardware and launch acceptance are tracked separately from compilation and structural checks.

The unofficial test title identity is `PPSA99541`, shown as **Console Shelf (UI test)**. It is not a globally reserved ID. The owner's console was checked for a collision; every installation must repeat that check and refuse to overwrite any existing title. The target app folder is `/data/homebrew/PPSA99541`; preferences are written only to `/data/console-shelf/settings.ini` and its recovery files. Existing game libraries are read in place.

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

1. Open **Console Shelf (UI test)** and check that the picture is stable and fits the TV.
2. Test D-pad/stick and Cross/Circle. From the search/layout row, Up focuses the settings button; Down returns to layout. Options opens the single settings panel; Square or the labelled layout button switches Grid/Swipe. A short Circle tap on the home page shows a small three-second hint; on other pages it remains Back. Hold Circle for three seconds to check its progress indicator and closing fade. Releasing early leaves the app open. The owner accepted the compact hint and closing behavior. Opening still shows black before the grid; submitted diagnostic frames do not prove a visible opening animation. Startup PNG probes are disabled by default.
3. Select Nintendo 64 and its game library; check Mario Party and the other existing titles. Check PS1, PS2 and PS3 as well.
4. Change a layout/visibility choice, close Shelf, then reopen it to check persistence.
5. Report any black screen, crash, missing input or incorrect library count. Avoid repeated attempts after a console hang.
6. Choose PlayStation 2 → Test emulator launch once, then wait up to ten seconds without pressing PS. Check that PS5SX2 appears on the TV, rather than remaining behind the home screen. Only after acceptance, test PS1/PS3 menu routes and a selected Nintendo 64 game through RetroArch. The helper passes game arguments once and stops observing focus if the target changes; it never retries the launch.

No binary release should be labelled compatible with firmware 12.20 until these results and the later supported launch routes are recorded. Remove only Shelf's owned app folder to roll back the program, retaining its preferences if wanted.

## PS5-managed launch picture

The native template supports static selection and launch backgrounds as `sce_sys/pic0.dds` and `pic1.dds`: 3840x2160 BC7_UNORM DX10 DDS, one 2D image and one mip level. Shelf now supplies its own matching artwork, generated from the native renderer's `launch-art` scene, with the original PNG and conversion provenance retained. `python tools/verify_ps5_presentation.py` checks complete payload sizes, image headers, PNG CRCs and SHA-256 pins; the package assembler includes both backgrounds. The Microsoft encoder is not bundled.

These are standard PS5-managed static presentation assets; the moving transition remains app code. The owner confirms the selection background appears, but opening the app still shows black before the shelf. This is not an accepted launch-animation fix. The candidate disables the two diagnostic PNG writes in normal startup to remove their measured delay; developers can explicitly define `SHELF_CAPTURE_STARTUP` for the bounded probes.

An existing title without these assets needs its own registration refreshed before the PS5 adopts them. First verify that ShadowMountPlus reports this exact title as an owned, managed folder at `/data/homebrew/PPSA99541`, not a package or image. With apps closed, retain a full raw program backup and the owned preferences, add/verify both artwork files, and use the existing ShadowMountPlus title-only uninstall/scan API to register that same folder again. Never use its source-deletion operation. Recheck every program/artwork hash and the preference bytes afterward. This registration refresh was verified for the owner's title; it need not be repeated for an ordinary program update. Keep firmware 12.20 and leave other titles, PSN, global shell settings and payload order untouched.
