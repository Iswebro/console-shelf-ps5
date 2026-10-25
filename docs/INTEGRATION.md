# Native PS5 integration plan

Console Shelf is presently an offline PC design preview. The console-first navigation, visual options, sort and filters work in a browser. They do not scan a PS5 or launch an emulator.

The [first native prototype](PROTOTYPE-PLAN.md) is now the main goal. Its C++17 renderer, catalogue/browsing model, navigation and recoverable settings pass host checks. The PS5 display/memory backend, console adapters and packaged build still need implementation and validation.

The proposed native app would build on [Jewel Case](https://github.com/SkaRift/JewelCase) for read-only game discovery and launch routing, with [Estante](https://github.com/kauansilvalopes/estante) as a reference for native PS5/PS4 titles. It would keep the console-first Grid and Swipe UI from this project. Both projects publish GPL source; see [credits](../THIRD_PARTY.md).

An installed emulator or RetroArch core is a route to try, not a promise that every game will work. Jewel Case's [v1.1.1 notes](https://github.com/SkaRift/JewelCase/releases/tag/v1.1.1) say PSXS5 and PS5SX2 open their own menus rather than a selected game. PS5SX3 needs its own adapter. [RetroArch v4's game-request interface](https://github.com/mihawk-99/PS5_RetroArch/blob/v4.0.0-beta-4/src/ps5_game.h) offers a specific game launch route, but the end-to-end integration still needs console testing. The Shelf should read existing emulator folders and deduplicate titles instead of moving or copying game libraries into a new layout.

The native build also needs controller input, title and game inventory, per-system preferred emulator settings, graceful handling when a loader is unavailable, save-preserving upgrades, and tests on the target firmware. Switch 2 remains a hardware reference with no verified PS5 emulator route. No PS5 package is published from this repository yet.
