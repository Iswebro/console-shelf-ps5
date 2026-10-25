# Console icon references and credits

The Shelf includes 52 newly generated, transparent 3D-style system icons in [`3d-icons/`](3d-icons/). The Switch 2 card and detail view both show the revised generated icon. The icons are stylized illustrations, **not photographs or rotatable 3D models**, and small details may differ from the real hardware. The original Wikimedia Commons photographs are linked below, not bundled in the public repository. Grid view can also show text cards; Swipe and Shelf always use the console illustrations.

## Gallery and display shelves

The [gallery backdrop](backgrounds/gallery-room-v1.png) and [empty display shelf](backgrounds/display-shelf-v1.png) were generated for this project with OpenAI's built-in image generation tool on 25 October 2026, from text prompts without external reference images. They are illustrative scenery. Consoles are rendered separately using the credited system illustrations; they are not baked into the shelves. The original generated PNGs are retained unchanged. Dimensions, SHA-256 and the shelf contact line are recorded in the [scenery manifest](backgrounds/manifest.json). Exact prompts: [gallery](backgrounds/gallery-room-v1.prompt.txt), [shelf](backgrounds/display-shelf-v1.prompt.txt). These two generated scenery assets are offered under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) to the extent rights are held; credit Console Shelf / Iswebro and note the AI-assisted generation.

## Hardware references

The first 18 icons were generated with the photographs credited below as visual references. The generated icons are changed, stylized illustrations. For icons based on CC BY-SA 3.0 photographs (PS3, Wii U, Super Nintendo, PSP and Dreamcast), the corresponding generated icon is offered under [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/) to the extent rights in the generated changes are held. Credit the photographers below and note the AI-assisted stylization if you reuse those icons. Other generated icons are offered under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) to the extent rights are held. The original source photographs are not distributed here. Console names and hardware designs belong to their respective owners.

| Console | Image | Creator | License |
| --- | --- | --- | --- |
| PlayStation 5 | [Source](https://commons.wikimedia.org/wiki/File:Black_and_white_Playstation_5_base_edition_with_controller.png) | Howardcorn33 | [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) |
| PlayStation 4 | [Source](https://commons.wikimedia.org/wiki/File:PS4-Console-wDS4.png) | Evan-Amos | Public domain |
| PlayStation 3 | [Source](https://commons.wikimedia.org/wiki/File:PS3-Fat-Console-Set.jpg) | Evan-Amos | [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/) |
| PlayStation 2 | [Source](https://commons.wikimedia.org/wiki/File:Sony_PlayStation_2.png) | Perchybits | [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) |
| PlayStation 1 | [Source](https://commons.wikimedia.org/wiki/File:Sony-PlayStation-5501-Console-FL.png) | Evan-Amos | Public domain |
| Nintendo 64 | [Source](https://commons.wikimedia.org/wiki/File:Nintendo-64-wController-L.jpg) | Evan-Amos | Public domain |
| GameCube | [Source](https://commons.wikimedia.org/wiki/File:GameCube-Set.jpg) | Evan-Amos | Public domain |
| Wii | [Source](https://commons.wikimedia.org/wiki/File:Wii-Console.png) | Evan-Amos | Public domain |
| Wii U | [Source](https://commons.wikimedia.org/wiki/File:Wii_U_Console_and_Gamepad.png) | Takimata; edited by Tokyoship | [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/) |
| Nintendo 3DS | [Source](https://commons.wikimedia.org/wiki/File:Nintendo-3DS-AquaOpen.png) | Evan-Amos | Public domain |
| Nintendo DS | [Source](https://commons.wikimedia.org/wiki/File:Nintendo-DS-Fat-Blue.png) | Evan-Amos | Public domain |
| Nintendo Switch | [Source](https://commons.wikimedia.org/wiki/File:Nintendo-Switch-Console-Docked-wJoyConRB.jpg) | Evan-Amos | Public domain |
| NES | [Source](https://commons.wikimedia.org/wiki/File:NES-Console-Set.png) | Evan-Amos | Public domain |
| Super Nintendo | [Source](https://commons.wikimedia.org/wiki/File:SNES-Mod1-Console-Set.png) | Evan-Amos | [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/) |
| Game Boy Advance | [Source](https://commons.wikimedia.org/wiki/File:Nintendo-Game-Boy-Advance-Purple-FL.png) | Evan-Amos | Public domain |
| PSP | [Source](https://commons.wikimedia.org/wiki/File:PSP-1000.png) | Evan-Amos; derivative by Alphathon | [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/) |
| Mega Drive | [PAL Model 1 reference](https://commons.wikimedia.org/wiki/File:Sega-Mega-Drive-EU-Mk1-wController-FL.jpg) | Evan-Amos | Public domain |
| Dreamcast | [Source](https://commons.wikimedia.org/wiki/File:Dreamcast-Console-Set.png) | Evan-Amos | [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/) |

The Mega Drive icon was revised on 25 October 2026 to depict the PAL Model 1 using Evan-Amos's reference above. It replaces the earlier Japanese-model illustration and uses the PAL hardware's neutral controls and three-button pad. The previous generated icon is kept only in the ignored local archive. The app icon variants were also generated for Console Shelf and are offered under CC BY 4.0 to the extent rights are held.

The Switch 2 icon is a newly generated dock-and-grip illustration based on [Nintendo's official Switch 2 press image](https://nintendoes-newsroom.prgloo.com/resources/nintendo-switch-2). The press photo is not bundled or displayed. Nintendo owns the hardware design and press image. The card is a visual reference only; no Switch 2 PS5 emulator has been verified.

The 33 other expansion icons were prompted from general hardware features and an existing Shelf icon as a style reference, without a specific downloaded photograph. See [the prompt brief](3d-icons/PROMPTS.md) for the list. Platform icons for arcade boards and computers are especially interpretive. No PS6 icon was created because an official PS6 console design could not be verified.

The first 18 reference URLs and local research file hashes are in [source-manifest.json](source-manifest.json); those local photographs are omitted from the public repository. Generated icon hashes and reference types are in [3d-icons/manifest.json](3d-icons/manifest.json).

The original static launch composition in `launch-background-source.png` is rendered by Shelf's native `launch-art` scene using its own app icon and the licensed Poppins fonts. It contains no external photograph or personal library data. The composition is offered under CC BY 4.0 to the extent rights are held. The matching PS5 DDS files and the verified, host-only Microsoft DirectXTex encoder are identified in `native/ps5/presentation-assets.json`; the encoder is not redistributed.
