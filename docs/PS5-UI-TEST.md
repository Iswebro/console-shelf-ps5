# Private first UI test

The local test folder is a separate step from the first public release. It validates the shared native renderer, DualSense input, persistence, console-local read-only library scanning and now an **experimental cooperative launch handoff**. The candidate embeds Shelf's original helper for the existing console-local loader. Start with the PS5SX2 menu test; no console launch is verified yet. The owner confirmed the grid, settings navigation and closing fade on firmware 12.20. The configured scan reports 92 games and 55 emulator/core routes without connection failures. The opening transition remains a known limitation.

The first private build failed its startup test. The console stayed responsive; the app returned from startup, then its C runtime exit hit SIGSYS. The corrected build waits for asynchronous shell closure and writes a small stage log under its owned data folder to identify the original startup failure. Hardware acceptance remains pending. No public download is available.

That journal identified the early failure: `sceSystemServiceLoadExec` remained null as a weak-only import. The next build makes this required lifecycle import strong, matching the reference native app, and its ELF verifier rejects a weak, missing or misbound shell import. This is a link-time correction; physical acceptance still requires another owner test.

The unofficial test title identity is `PPSA99541`, shown as **Console Shelf (UI test)**. It is not a globally reserved ID. The owner's console was checked for a collision; every installation must repeat that check and refuse to overwrite any existing title. The target app folder is `/data/homebrew/PPSA99541`; preferences are written only to `/data/console-shelf/settings.ini` and its recovery files. Existing game libraries are read in place.

## Assemble locally

With reviewed sources committed and the verified executable current:

```sh
npm run build:ps5
npm run verify:ps5
npm run prepare:ps5-ui-test
```

The preparation command compiles a small host-only artwork converter with the existing C++ compiler. It makes a 512×512 app tile from Shelf's existing logo using integer area resampling with premultiplied alpha; the original image is unchanged. It adds the 52 console illustrations, licensed fonts, the source-built clean-room runtime, credits and license texts to an isolated timestamped folder under ignored `.cache/ps5/test-packages/`.

The assembler requires a clean Git tree, a source fingerprint matching the tested executable, recorded binary hashes and the expected runtime hash. It records the exact source commit and every packaged file's size/SHA-256. The local ZIP receives a full integrity check and each member is compared to its manifest. The receipt is `.cache/ps5/test-packages/latest.json`. The command makes no console connection or installation, changes no OS settings and publishes nothing.

This private UI package is **not for public redistribution**. The complete corresponding library sources and relinking materials remain a release requirement, along with successful hardware and launch tests.

## Console check

Use the owner's established local FTP transfer workflow, with no game or download running. Recheck the title identity, create only the new owned app folder, and verify complete raw file readbacks. Before reading `eboot.bin`, ftpsrv must explicitly report SELF conversion disabled. Keep firmware 12.20 and the current payload order.

After exposing the new tile through the already established ShadowMount procedure:

1. Open **Console Shelf (UI test)** and check that the picture is stable and fits the TV.
2. Test D-pad/stick and Cross/Circle. From the search/layout row, Up should focus the settings button; Down returns from settings to layout. Options opens the single settings panel; Square or the labelled layout button switches Grid/Swipe immediately. Hold Circle for three seconds, check its progress indicator and closing fade, then reopen Shelf. Check that its branded loading screen animates and fades into the library. Releasing a hold early should leave the current page open; a short tap remains Back. The owner accepted the earlier closing fade and UI-7's settings navigation. UI-8's executable and submitted frames were verified on the console, but its opening remained unseen and the owner reported needing four seconds or longer to close; neither result is accepted. UI-9 prepares the scene before starting the entrance clock and records separate button-hold, fade and display-shutdown timings. It writes only two fixed diagnostic PNGs under its owned data folder; collect/verify them privately and remove the console copies afterward. Journal timings and submitted frames alone do not prove what appeared on the TV.
3. Select Nintendo 64 and its game library; check Mario Party and the other existing titles. Check PS1, PS2 and PS3 as well.
4. Change a layout/visibility choice, close Shelf, then reopen it to check persistence.
5. Report any black screen, crash, missing input or incorrect library count. Avoid repeated attempts after a console hang.

No binary release should be labelled compatible with firmware 12.20 until these results and the later supported launch routes are recorded. Remove only Shelf's owned app folder to roll back the program, retaining its preferences if wanted.

## PS5-managed launch picture

The native template supports static selection and launch backgrounds as `sce_sys/pic0.dds` and `pic1.dds`: 3840x2160 BC7_UNORM DX10 DDS, one 2D image and one mip level. Shelf now supplies its own matching artwork, generated from the native renderer's `launch-art` scene, with the original PNG and conversion provenance retained. `python tools/verify_ps5_presentation.py` checks complete payload sizes, image headers, PNG CRCs and SHA-256 pins; the package assembler includes both backgrounds. The Microsoft encoder is not bundled.

These are standard PS5-managed static presentation assets; the moving transition remains app code. The owner confirms the selection background appears, but opening the app still shows black before the shelf. This is not an accepted launch-animation fix. The candidate disables the two diagnostic PNG writes in normal startup to remove their measured delay; developers can explicitly define `SHELF_CAPTURE_STARTUP` for the bounded probes.

An existing title without these assets needs its own registration refreshed before the PS5 adopts them. First verify that ShadowMountPlus reports this exact title as an owned, managed folder at `/data/homebrew/PPSA99541`, not a package or image. With apps closed, retain a full raw program backup and the owned preferences, add/verify both artwork files, and use the existing ShadowMountPlus title-only uninstall/scan API to register that same folder again. Never use its source-deletion operation. Recheck every program/artwork hash and the preference bytes afterward. This registration refresh was verified for the owner's title; it need not be repeated for an ordinary program update. Keep firmware 12.20 and leave other titles, PSN, global shell settings and payload order untouched.
