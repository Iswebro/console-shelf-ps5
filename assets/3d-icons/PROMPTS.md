# Console icon generation brief

Each system icon was generated separately with the image generator. The original 18 icons used corresponding hardware photos from [`../source-manifest.json`](../source-manifest.json). The Switch 2 icon used [Nintendo's official press photo](https://nintendoes-newsroom.prgloo.com/resources/nintendo-switch-2). The other 33 new icons used the existing Shelf icon style as a visual reference and prompts describing the known hardware shape; they did not use a specific third-party photograph. The outputs are transparent PNG illustrations, not manufacturer renders or editable 3D meshes.

## Shared visual direction

> Create one premium 3D-style product icon of the referenced game console, including its distinctive controller or handheld controls where appropriate. Use the reference for the console's silhouette, proportions, recognizable components, and color. Show a clean three-quarter view with subtle studio lighting, realistic materials, refined edges, and soft contact shadows. Center the complete console and controller as an isolated cutout on a fully transparent background. No room, shelf, backdrop, card, text label, or decorative frame. Keep the shape easy to recognize at small icon size.

The original prompts covered PlayStation 5, 4, 3, 2, and 1; Nintendo 64, GameCube, Wii, Wii U, 3DS, DS, Switch, NES, Super Nintendo, and Game Boy Advance; PSP; Sega Mega Drive; and Dreamcast.

The Mega Drive was subsequently redrawn from Evan-Amos's public-domain PAL Model 1 photo, with the neutral reset/power/Start colours and dark three-button pad. A follow-up edit corrected the front panel to exactly two controller ports, one occupied by the pad's plug and one empty. The icon uses the same transparent product-illustration style as the surrounding Shelf consoles.

The expansion covered Switch 2; Game Boy, Game Boy Color, Famicom Disk System, Pokémon mini and Virtual Boy; Atari 2600, 5200, 7800, Jaguar and Lynx; Master System, Game Gear, Sega CD, 32X, Saturn, SG-1000 and NAOMI; PC Engine, PC-FX and SuperGrafx; WonderSwan and WonderSwan Color; Neo Geo CD, Neo Geo Pocket and Pocket Color; 3DO and Atomiswave. Arcade FinalBurn Neo and MAME, C64, Amiga, DOS and ScummVM are styled as platform emblems rather than claims of one exact machine.

The prompts called out silhouette, color, controllers, or screens where relevant. Some tiny markings and proportions are illustrative rather than exact. The revised Switch 2 icon follows the official photo's docked console and separate Joy-Con grip, with no horizontal handheld screen in front. The same transparent icon appears in Grid, Swipe and detail views; the official red-background photo and first generated version are omitted from the public repository. The image represents officially pictured hardware only: it does not imply PS5 emulation support. No PS6 icon was generated because no official PS6 console design could be verified.

The icons were generated for this local PC preview on 25 October 2026. The source photos are credited in [`../CREDITS.md`](../CREDITS.md). `manifest.json` records each local icon's SHA-256 hash and the reference type; a null reference means that no specific photo was used for that icon.
