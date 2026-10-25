# Your Shelf play room

Shelf is a fixed-camera room with layered console artwork. It is not a navigable 3D room. The room and its deep wood counters stay still when pages change; only the hardware illustrations slide, while their name labels fade in place. The atmosphere takes inspiration from the virtual room in [d3t's SEGA Mega Drive Classics Hub](https://d3tltd.com/our-work/sega-mega-drive-classics-hub/). Shelf's room artwork and application code are documented in the project's own credits.

## Arrange the consoles

Use Square to choose **Shelf**. Eight consoles fit the two counters on PS5. **L1/R1** changes pages, retaining the same row/column where possible; it stops at either end. D-pad/left-stick navigation also continues between rows. Keyboard Page Up/Down provides the equivalent in the PC preview. Small preview screens use four or six spaces per page.

In **Settings → Sort & filter → Shelf pages**, choose:

- **Browse collection:** pages cover the current visible, filtered and sorted collection, including new consoles added to the catalogue later.
- **One curated shelf:** select up to eight systems in **Consoles**. The picker shows filled spaces, refuses a ninth selection, and supports **Fill spaces** or **Clear selection**. These choices do not hide systems from Grid or Swipe. To see every selected console, clear any search/brand/availability filter and use All consoles.

**Consoles → Show on my shelf** offers All consoles, Emulator or games, and Favourites only. Triangle on a console marks or removes its favourite. Favourites first under Sort changes the order; Favourites only changes membership. **Appearance → Motion → Reduced** removes page sliding and snaps the Swipe carousel to its selection. The browser also honours the device's reduced-motion preference.

## Personalise the TV

**Settings → Play room → TV display** offers:

| Display | Content |
| --- | --- |
| Selected console | Its illustration, name and discovered game/emulator counts |
| Console intro picture | Your per-console still, fading in on selection; otherwise the console illustration |
| My picture | Your personal PNG, cropped to fill the TV screen |
| Clock & Shelf logo | The device's local time/date and your personal message |
| My message | Your name or short message with the selected Shelf logo |
| Favourite games | Names and console illustrations for games starred in Shelf's library, rotating every 10, 20, 30 or 60 seconds |
| Screen off | The room's original unlit TV |

Console intro pictures can apply to **installed emulators and built-in systems**, or **all consoles**. Missing emulators still remain unavailable to launch. Video decoding and official boot animations/audio are not included in this prototype. Favourite-game rotation uses the games discovered by Shelf; it does not import another emulator's favourites or download covers.

Choose **Personal message** and press Cross to edit it on PS5. Up to 95 UTF-8 bytes fit; spaces are available on the keyboard. The text, rotation interval, display mode, intro scope and favourites save in Shelf's own settings. Game favourites use local stable path identities; moving or renaming the game can require starring it again.

## Add your own pictures on PS5

Close Shelf, use your existing trusted local FTP connection, and copy only to its owned data folder:

- Personal TV picture: `/data/console-shelf/tv-image.png`
- Console intro still: `/data/console-shelf/intros/<console-id>.png`

Use a complete PNG up to **2048 × 2048 pixels**, preferably less than **8 MB**. Keep a copy of any existing personal picture before replacing it. Reopen Shelf and select the relevant TV mode. If the file is missing or unsupported, the room uses its message or console-art fallback.

Examples are `intros/ps2.png`, `intros/ps3.png`, `intros/n64.png`, `intros/genesis.png` and `intros/switch2.png`. The stable IDs are in `systems-data.js` and the console-art manifest. These images are personal content; they are not added to the public release or another app's folders.

In the **browser preview**, Play room has PNG file pickers for your TV and individual console intros. Pictures stay in that browser's IndexedDB storage; text and choices stay in local storage. This does not transfer them to the console. The TV display is hidden on narrow phone previews where the photographed TV is outside the cropped room; the console library remains usable.

## Prototype status

Browser and native host checks cover bounded selection, old-settings compatibility, personal-image fallback, text safety, favourite-game rotation and page transitions. Native frame checks compare the unchanged wood and floor pixels during motion. These are PC checks; this UI revision still needs physical PS5 acceptance before a verified prototype release.
