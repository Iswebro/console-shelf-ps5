# Private first UI test

The local test folder is a separate step from the first public release. It is for validating the shared native renderer, DualSense input, persistence and console-local read-only library scanning. **It does not launch emulators or games.** No launch payload is included. The owner has confirmed that the third test build displays the grid and responds to basic controls on firmware 12.20; navigation is not yet accepted and the console-local scan is incomplete.

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
2. Test D-pad/stick and Cross/Circle. From the search/layout row, Up should focus the settings button; Down returns from settings to layout. Options opens the single settings panel; Square or the labelled layout button switches Grid/Swipe immediately. Hold Circle for four seconds, check its progress indicator and closing fade, then reopen Shelf. Check that its branded loading screen animates and fades into the library. Releasing a hold early should leave the current page open; a short tap remains Back. The owner accepted UI-6's closing animation; its opening was not visible enough, so the revised handoff and prepared reveal require another check.
3. Select Nintendo 64 and its game library; check Mario Party and the other existing titles. Check PS1, PS2 and PS3 as well.
4. Change a layout/visibility choice, close Shelf, then reopen it to check persistence.
5. Report any black screen, crash, missing input or incorrect library count. Avoid repeated attempts after a console hang.

No binary release should be labelled compatible with firmware 12.20 until these results and the later supported launch routes are recorded. Remove only Shelf's owned app folder to roll back the program, retaining its preferences if wanted.
