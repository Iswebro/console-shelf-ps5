# Game artwork

Private build `00.100.026` includes game cover thumbnails, a large selected-game image, and cover art in the room TV's favourite-game mode. Portrait, square and landscape images keep their original proportions. A game without an image uses its console illustration. The public `v0.1.0-beta.1` download predates this feature.

## Existing emulator artwork

isShelf finds images when it scans the library. Games, emulator folders and saves stay in place.

| Source | Matching rule |
| --- | --- |
| PSXS5 | The exact game path in `library.txt` supplies its serial. Front images in `covers/default` or `covers/boxart` take priority; `covers/3d` is a fallback. Filename-matched covers work too. Conflicting serial rows are ignored. |
| PS5SX2 | An exact game path and matching file size in `chd-serials.txt` supply the serial used in `/data/PCSX2/covers`. Back and spine images are not substituted for the front. |
| PSSwanStation / SwanStationPS5 | Existing `covers` folders, matched by game filename or a known serial. |
| RetroArch | The system's `thumbnails` database folder, using `Named_Boxarts`, then `Named_Titles` or `Named_Snaps`. Matching uses the game filename or displayed name. |
| PS3 | The discovered game's `PS3_GAME/ICON0.PNG`. |
| PS4 / PS5 | The registered game's `sce_sys/icon0.png`. |
| A game's own folder | `cover.png`, `cover.jpg`, `front.png` or `front.jpg`, including upper-case extensions. |

PNG and JPEG sources are supported. These images help identify games; they do not establish that a game or emulator is compatible.

## Cache and privacy

The app reads artwork through the console's existing ftpsrv and keeps resized PNG copies inside `/data/console-shelf/covers`. The cache uses a stable key derived from each game's system and path. Its private `index.json` records the artwork source and whether it is ready. Artwork is read in the background discovery worker, and unchanged cache entries are reused on the next opening.

Transfers are limited to known artwork locations, 8 MiB per source image and 64 MiB per opening. Decoding is bounded to 16 million pixels, and cached images have a maximum side of 960 pixels. Very large collections can fill the cache over several openings. Damaged or oversized art leaves the game available with its console illustration. Cancellation still interrupts the FTP waits.

The artwork reader uses only the two named game-to-serial indexes, public game metadata, and image files. It does not read emulator account settings. Cover files from an owner's console are personal cache data, not public release assets. The app does not automatically download covers from the Internet.

## Owner's collection check

The 26 October 2026 check found 92 library entries. Eighty images came from the existing PS1, PS2, PS3 and native PS5 libraries. Ten matching N64 front covers from the [Libretro thumbnail collection](https://github.com/libretro-thumbnails/Nintendo_-_Nintendo_64) were added to the owner's private cache. The two GameShark utilities had no exact image in the checked sources and use the PS1 fallback.

The owner's startup check of build `00.100.025` failed with `CE-108255-1`. Console diagnostics identified a stack overflow in the artwork serial-index reader. Build `00.100.026` moves that bounded buffer to the heap, gives the discovery worker an explicit 256 KiB stack, and rejects scan functions with stack frames larger than 16 KiB during PS5 compilation. A 64 KiB worker regression reproduces the old failure and passes with the fix, including maximum-size, oversized and short-read indexes.

The replacement build is installed and its five changed program records were read back. Settings are unchanged byte for byte; the private intro and artwork folder listings are unchanged. Native PC checks and installation verification remain separate from acceptance on the physical PS5. Opening, physical artwork display and the selected-game PS1 handoff await the owner's check. [Artwork record](verification/emulator-artwork-2026-10-26.json); [startup fix record](verification/ui26-startup-stack-fix-2026-10-27.json).
