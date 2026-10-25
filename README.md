# Console Shelf

A console-first library project for PS5 homebrew. This repository contains a **browser design preview** and a **native C++/SDL development build for PC**. The PS5 package is still in development. Open [index.html](index.html) in a browser for the design preview, or follow the [native build instructions](native/README.md) to run the C++ app on PC. Both currently work offline with no console launch connection.

![Floating console icons in Swipe view](previews/console-images-swipe.png)

## What works in the preview

- Browse 52 console and platform cards in a Grid or a card-free, floating-icon Swipe view. Grid can use 3D-style system icons or classic text cards.
- Switch between **All consoles** and **Emulator or games**. The latter shows systems with a recorded emulator route, built-in PS5/PS4 route, or example game in the bundled demo inventory. The selection persists in the browser.
- Search, filter by brand or evidence level, sort in several ways, and save favorites.
- Open compact toolbar icons for Appearance, Sort & filter and Consoles. Search the console picker, toggle individual systems or whole brands, and show/hide search results in one click. These choices persist on the device.
- Open a console detail page, choose a preferred emulator, and view example games. The emulator button is a preview action only.
- Choose among three generated app icons for the preview header and browser tab.

See the [mobile Swipe view](previews/mobile-swipe.png) and [Switch 2 icon](previews/switch2-3d-card.png). Nintendo's press photo informed the Switch 2 illustration but is **not** displayed or bundled.

## Inventory and compatibility

The [demo system catalogue](docs/SYSTEMS.md) comes from a dated PS5 setup snapshot. Its 46 RetroArch core mappings and example game names are **not** a live scan of another user's console. A mapped core or listed emulator does not guarantee a game runs. In the demo, “Emulator or games” shows 51 of 52 cards; Switch 2 is the hardware-reference-only card and has no launch route. A future native build must replace the demo with each owner's actual emulator and game inventory.

The next main milestone is a [native PS5 prototype and its first verified download](docs/PROTOTYPE-PLAN.md), targeting firmware 12.20. The [C++17 app](native/README.md) renders the console shelf and settings, handles controller-style navigation and search, and saves preferences with recovery from a backup. Model, persistence and simulated SDL controller tests pass on PC. The [PS5 executable now cross-compiles](docs/PS5-BUILD.md), with native display/memory integration checked by host mocks and executable structure checks. Real discovery, launch adapters, physical controller and firmware tests remain in development. No PS5 package is published here.

![Searchable console visibility settings](previews/settings-consoles.png)

The native C++ renderer running on PC:

![Native development build](previews/native-home.png)

## Development check

The preview itself needs no build step. With Node.js and npm, run `npm ci` and `npm test` for the browser interaction and icon-integrity check. The test uses an installed Microsoft Edge when available; otherwise install Playwright's Chromium with `npx playwright install chromium`. Tests leave the checked-in screenshots alone; set `SHELF_CAPTURE_PREVIEWS=1` to regenerate them intentionally. Run `npm run test:native` for the native model using an existing Visual Studio C++ toolset on Windows, or a C++17 compiler on Linux. See [native build notes](native/README.md).

## Credits and license

The application source in this repository is [GPL-3.0](LICENSE). The generated icons have separate [image credits and reuse terms](assets/CREDITS.md). [Third-party project credits](THIRD_PARTY.md) distinguish incorporated GPL platform/build source, rendering dependencies and emulator references. No emulator binaries, games, ROMs, BIOS, keys, proprietary firmware/runtime files or user credentials are included. Console and game names are used for identification; this project is not affiliated with their manufacturers.
