# Installing Console Shelf

Preparation guide for the first prototype. A public binary has not been published yet; see the [acceptance status](PROTOTYPE-PLAN.md). The tested app currently appears as **Console Shelf (UI test)**.

## What you need

- An existing working PS5 homebrew setup. The current hardware target is firmware **12.20**.
- Your usual enabler and ShadowMountPlus, plus the existing ELF loader on console-local port 9021.
- The console's existing `ftpsrv` on port 2121. Shelf currently uses it for library discovery, including over loopback. Keep it available while using Shelf, on a trusted local network.

Installing Shelf does not set up a jailbreak, update firmware, change DNS/firewall settings or edit your payload autoload list. It reads the emulator libraries where they already are.

Shelf includes its own small launcher helper. When you select a launch, the existing ELF loader runs that helper; it continues briefly after Shelf closes and asks the system to open the selected app. You do not manually load another Shelf payload or add one to autoload.

The tested **standard-sdk** build retains the SDK's normal helper-process capability and syscall-bound initialization in RAM. Read the manifest's runtime notice before choosing this variant. The default restricted source build keeps the loader's permissions, but its running-app query is refused on the owner's console. Neither variant contains a firmware updater.

## First installation

Keep firmware 12.20 and decline any system software update prompt.

1. Download the prototype app ZIP and its checksum file from this repository's release page when available. On Windows, use `Get-FileHash -Algorithm SHA256 -LiteralPath 'C:\path\to\download.zip'` and compare the complete value with `SHA256SUMS.txt`.
2. Extract the ZIP on your PC. The app folder is `PPSA99541`; keep all its subfolders together. You do not need Node.js, a compiler or the PC preview to use the installed app.
3. Close games, emulators and downloads on the PS5. Start your existing FTP service and connect using your established local FTP client.
4. Check both `/data/homebrew/PPSA99541` and `/user/app/PPSA99541` for an existing title. This is an unofficial ID, not a globally reserved one. If it belongs to another app or you cannot identify it, stop. For an existing Shelf installation, use the upgrade steps below.
5. Copy the complete `PPSA99541` folder into `/data/homebrew/`. Compare the copied files with the included size/SHA-256 manifest. Do not open Shelf during the transfer.
6. Let your existing ShadowMountPlus workflow discover the folder. If necessary, run ShadowMountPlus once through your usual Payload Manager dashboard and wait for its library-synchronised message. Open **Console Shelf (UI test)**.

Shelf itself needs no PC host afterward; it uses the services on the console. A console illustration in All consoles does not mean an emulator exists for it; installed routes and real game counts come from discovery.

## Controls and first check

Browse with the D-pad or left stick; Cross selects and Circle goes back. Square switches Grid/Swipe. Options or the gear opens settings. A short Circle tap on Shelf's home page shows a small close hint; hold Circle for three seconds to close with a fade.

Open a console to choose a preferred detected emulator or show its game library. Standalone emulators can open their own shelves; a selected RetroArch game uses a separate direct-game route. Use the release's verified-route list to distinguish accepted behavior from experimental plans. Opening can still show black before the grid; the home-screen selection artwork is confirmed.

## Upgrading and recovery

Close Shelf and other apps before replacing files. Keep a PC backup of its program folder and `/data/console-shelf`, where its preferences live. When reading or backing up `eboot.bin` through `ftpsrv`, send the custom `SELF` command and confirm the reply explicitly says conversion is **disabled**; otherwise the server can transform the file during transfer and its hash will differ. This setting is per FTP connection.

Replace only Shelf's owned program files, putting `eboot.bin` last, and verify the complete new file manifest before reopening. Preserve `/data/console-shelf/settings.ini` and its recovery files. An ordinary program update does not require re-registering the app's artwork.

If a launch fails, preserve `/data/console-shelf/startup.log`, `launch-helper.log` and `launch-status.txt` before the next attempt. These are private diagnostics; review them before sharing. Restore the previous verified program folder to roll back. Other emulator folders, BIOS, games, saves and account settings are separate.

To remove Shelf, close it, retain your backup and use your established ShadowMountPlus homebrew-removal procedure for **PPSA99541 only**, including its owned source folder. Keep `/data/console-shelf` if you want its preferences for a future installation. Do not remove another title or a shared emulator/game directory.
