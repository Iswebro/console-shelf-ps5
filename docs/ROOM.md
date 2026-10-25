# Your Shelf play room

Shelf is a fixed-camera room with layered console artwork. It is not a navigable 3D room. The room and its deep wood counters stay still when pages change; only the hardware illustrations slide, while their name labels fade in place. The atmosphere takes inspiration from the virtual room in [d3t's SEGA Mega Drive Classics Hub](https://d3tltd.com/our-work/sega-mega-drive-classics-hub/). Shelf's room artwork and application code are documented in the project's own credits.

## Arrange the consoles

Handhelds and Switch hybrids use a smaller default room scale (62% of the standard artwork fit), so a portable does not visually dwarf PS2, PS3 or other full-sized systems. This is an illustration balance, not a claim of exact real-world dimensions. The contact anchor remains on the wood when size changes. Grid and Swipe remain focus-oriented catalogue views.

Use Square to choose **Shelf**. Eight consoles fit the two counters on PS5. **L1/R1** changes pages, retaining the same row/column where possible; it stops at either end. Up/Down moves between the two rows on the current page; Down stops on the bottom row. Up from the top row reaches the search/layout toolbar on every page, then Up reaches Settings. Keyboard Page Up/Down provides the page controls in the PC preview. Small preview screens use four or six spaces per page.

In **Settings → Sort & filter → Shelf pages**, choose:

- **Browse collection:** pages cover the current visible, filtered and sorted collection, including new consoles added to the catalogue later.
- **One curated shelf:** select up to eight systems in **Consoles**. The picker shows filled spaces, refuses a ninth selection, and supports **Fill spaces** or **Clear selection**. These choices do not hide systems from Grid or Swipe. To see every selected console, clear any search/brand/availability filter and use All consoles.

**Consoles → Show on my shelf** offers All consoles, Emulator or games, and Favourites only. Triangle on a console marks or removes its favourite. Favourites first under Sort changes the order; Favourites only changes membership. **Appearance → Motion → Reduced** removes page sliding and snaps the Swipe carousel to its selection. The browser also honours the device's reduced-motion preference.

## Personalise the TV

**Settings → Play room → TV display** offers:

| Display | Content |
| --- | --- |
| Selected console | Its illustration, name and discovered game/emulator counts |
| Console intro picture | The original Shelf still for that console, or your own PNG override, fading in on selection |
| Console intro video | The original Shelf animation for that console, or your own silent movie override; plays once when the selected console changes |
| My picture | Your personal PNG, cropped to fill the TV screen |
| Clock & Shelf logo | The device's local time/date and your personal message |
| My message | Your name or short message with the selected Shelf logo |
| Favourite games | Names and console illustrations for games starred in Shelf's library, rotating every 10, 20, 30 or 60 seconds |
| Screen off | The room's original unlit TV |

Console intros can apply to **installed emulators and built-in systems**, or **all consoles**. Matching original stills and silent 4.4-second animations are bundled for all 52 catalogue systems. These use Shelf's own hardware illustrations, names and colours; they are not the manufacturers' startup sequences. Personal media takes priority, and an invalid personal video falls back to the matching Shelf original. The generic Shelf demo remains a last fallback. Missing emulators remain unavailable to launch. Favourite-game rotation uses the games discovered by Shelf; it does not import another emulator's favourites or download covers. The TV and its saved display stay visible when opening a console's details or game library. Entering the same console does not restart its intro.

**TV static for missing content** is optional and starts Off. Choose No emulator installed, No games found, or No emulator or no games. This overrides the chosen content for matching consoles; Screen off still takes priority. Static stays inside the screen, with fine-grain noise drawn at the screen's own resolution rather than enlarged blocks. It is generated locally at 20 updates per second, with subtle scan-line shading; it does not need a video file. Reduced motion freezes static and intro videos on a still frame.

Choose **Personal message** and press Cross to edit it on PS5. Up to 95 UTF-8 bytes fit; spaces are available on the keyboard. The text, rotation interval, display mode, static policy, intro scope and favourites save in Shelf's own settings. Game favourites use local stable path identities; moving or renaming the game can require starring it again.

## Add your own pictures on PS5

Close Shelf, use your existing trusted local FTP connection, and copy only to its owned data folder:

- Personal TV picture: `/data/console-shelf/tv-image.png`
- Console intro still: `/data/console-shelf/intros/<console-id>.png`

Use a complete PNG up to **2048 × 2048 pixels**, preferably less than **8 MB**. Keep a copy of any existing personal picture before replacing it. Reopen Shelf and select the relevant TV mode. If the file is missing or unsupported, the room uses its message or console-art fallback.

Examples are `intros/ps2.png`, `intros/ps3.png`, `intros/n64.png`, `intros/genesis.png` and `intros/switch2.png`. The stable IDs are in `systems-data.js` and the console-art manifest. These images are personal content; they are not added to the public release or another app's folders.

## Add your own intro videos

The PS5 uses silent **MPEG-1 video in an MPEG program stream** (`.mpg`), up to 640 × 368 pixels, 30 fps, 60 seconds and 32 MB. Renaming an MP4 to `.mpg` does not convert it. A missing, corrupt or unsupported personal clip falls back to that console's bundled Shelf animation. No PC streaming host is needed.

Prepare a copy with Python and a portable [FFmpeg build](https://ffmpeg.org/download.html), keeping your original untouched:

```powershell
python tools/prepare-intro.py "C:\My clips\my-ps2-intro.mp4" --console ps2 --seconds 10 --ffmpeg "C:\Tools\ffmpeg\bin\ffmpeg.exe"
```

The tool creates a new directory under `.cache/room-intros` with a validated `ps2.mpg`, an MP4 for the browser preview, and a size/SHA-256 manifest. Existing output directories are refused. It does not contact the console. With Shelf closed, copy the MPEG to `/data/console-shelf/intros/ps2.mpg` through your existing local FTP connection, retaining a backup of any personal clip already there. Reopen Shelf, choose Console intro video, then select PS2. Use only clips you have permission to use.

In the **browser preview**, Play room has PNG and MP4 file pickers for individual console intros. Media stays in that browser's IndexedDB storage; text and choices stay in local storage. This does not transfer it to the console. The TV display is hidden on narrow phone previews where the TV is outside the cropped room; the console library remains usable.

## Prototype status

Browser and native host checks cover bounded selection, old-settings compatibility, personal-image fallback, text safety, favourite-game rotation and page transitions. Native frame checks compare the unchanged wood and floor pixels during motion. These are PC checks; this UI revision still needs physical PS5 acceptance before a verified prototype release.
