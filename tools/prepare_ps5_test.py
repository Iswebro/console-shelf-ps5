# SPDX-License-Identifier: GPL-3.0-only
"""Assemble a private native test folder. Never connects to, installs on or launches a PS5."""
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import zipfile

from ps5_common import CACHE, ROOT, run, sha256
from verify_ps5_build import require, source_fingerprint, verify_recorded_hashes
from verify_ps5_presentation import verify as verify_presentation


def main():
    build = CACHE / "build"
    verified = verify_recorded_hashes(build)
    launcher = verified.get("embeddedLaunchHelper", {})
    permission_notice = ("Standard SDK helper runtime: changes process capabilities and syscall bounds in memory. "
                         "Owner approval is required before using this private candidate."
                         if launcher.get("permissionChanges") else
                         "Restricted helper runtime: SDK credential/syscall patches and custom SO loading are disabled.")
    identity = json.loads((build / "build-source.json").read_text())
    require(identity == source_fingerprint(), "Rebuild the PS5 executable before assembling its test folder")
    status = subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT, text=True)
    require(not status.strip(), "Commit reviewed sources before assembling the test folder")
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    destination = CACHE / "test-packages" / ("ui-" + stamp + "-" + sha256(build / "eboot.bin")[:12])
    app = destination / "PPSA99541"
    app.mkdir(parents=True, exist_ok=False)

    def copy(source, relative):
        target = app / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        with target.open("xb") as out, Path(source).open("rb") as incoming:
            shutil.copyfileobj(incoming, out)

    copy(build / "eboot.bin", "eboot.bin")
    copy(build / "libc-a.prx", "sce_module/libc.prx")
    require(sha256(app / "sce_module/libc.prx") == "e6ff45d16adf687855cc3b33b0c8a4132b6504360b221e0a34c7e99fb3ba0036", "Wrong clean-room runtime")
    copy(ROOT / "native/ps5/param.json", "sce_sys/param.json")
    presentation = verify_presentation()
    for name in ("pic0.dds", "pic1.dds"):
        copy(ROOT / "native/ps5/sce_sys" / name, "sce_sys/" + name)
    metadata = json.loads((app / "sce_sys/param.json").read_text())
    require(metadata["titleId"] == "PPSA99541" and metadata["requiredSystemSoftwareVersion"] == "0x0000000000000000", "Wrong test identity or firmware requirement")
    images = sorted((ROOT / "assets/3d-icons").glob("*.png"))
    require(len(images) == 52, "Expected the 52 reviewed system illustrations")
    for image in images:
        copy(image, "assets/3d-icons/" + image.name)
    for image in ("shelf-icon.png", "shelf-icon-v1.png", "shelf-icon-premium-v2.png"):
        copy(ROOT / "assets" / image, "assets/" + image)
    copy(ROOT / "assets/branding/shelf-gaming-mark.png", "assets/branding/shelf-gaming-mark.png")
    scenery = json.loads((ROOT / "assets/backgrounds/manifest.json").read_text(encoding="utf-8"))
    require(scenery.get("schema") == 1 and len(scenery.get("assets", [])) == 2, "Expected two reviewed scenery assets")
    require({item["local"] for item in scenery["assets"]} == {
        "assets/backgrounds/gamer-lounge-v1.png", "assets/backgrounds/gamer-lounge-deep-v1.png"}, "Unexpected scenery paths")
    for item in scenery["assets"]:
        source = ROOT / item["local"]
        raw = source.read_bytes()
        require(len(raw) == item["bytes"] and hashlib.sha256(raw).hexdigest() == item["sha256"], "Scenery digest mismatch")
        require(raw[:8] == b"\x89PNG\r\n\x1a\n" and struct.unpack_from(">II", raw, 16) == (item["width"], item["height"]), "Scenery dimensions mismatch")
        copy(source, item["local"])
    copy(ROOT / "assets/backgrounds/manifest.json", "assets/backgrounds/manifest.json")
    for name in ("Poppins-Regular.ttf", "Poppins-SemiBold.ttf", "OFL.txt"):
        copy(ROOT / "native/assets/fonts" / name, "native/assets/fonts/" + name)
    for name in ("SDL2-PS5.txt", "libsamplerate-0.2.2.txt", "LGPL-2.1.txt", "LLVM-libcxx.txt"):
        copy(ROOT / "native/third_party/licenses" / name, "licenses/" + name)
    for name in ("LICENSE", "THIRD_PARTY.md"):
        copy(ROOT / name, name)
    copy(ROOT / "native/third_party/cjson/LICENSE", "licenses/cJSON-MIT.txt")
    copy(ROOT / "native/third_party/stb/stb_image.h", "licenses/stb_image-source-with-license.h")
    copy(ROOT / "native/third_party/stb/stb_truetype.h", "licenses/stb_truetype-source-with-license.h")
    copy(ROOT / "native/third_party/stb/stb_image_write.h", "licenses/stb_image_write-source-with-license.h")
    copy(ROOT / "native/third_party/pl_mpeg/pl_mpeg.h", "licenses/pl_mpeg-source-with-MIT-license.h")
    room_media = json.loads((ROOT / "assets/room/manifest.json").read_text(encoding="utf-8"))
    require(room_media.get("officialConsoleIntro") is False and room_media.get("silent") is True,
            "Expected original silent Shelf demo")
    for item in room_media["assets"]:
        require(item["local"] in {"assets/room/shelf-intro.mpg", "assets/room/shelf-intro.mp4"}, "Unexpected demo path")
        source = ROOT / item["local"]
        require(source.stat().st_size == item["bytes"] and sha256(source) == item["sha256"], "Room demo hash mismatch")
        copy(source, item["local"])
    copy(ROOT / "assets/room/manifest.json", "assets/room/manifest.json")
    intro_root = ROOT / "assets/room/intros"
    intro_manifest = json.loads((intro_root / "manifest.json").read_text(encoding="utf-8"))
    require(intro_manifest.get("officialConsoleIntros") is False and intro_manifest.get("silent") is True,
            "Expected original silent per-console intros")
    require(len(intro_manifest["consoles"]) == len(images) and
            {item["id"] for item in intro_manifest["consoles"]} == {image.stem for image in images},
            "Original intro catalogue is incomplete")
    for console in intro_manifest["consoles"]:
        ident = console["id"]
        require({item["file"] for item in console["assets"]} == {ident + ext for ext in (".png", ".mpg", ".mp4")},
                "Unexpected intro media names")
        require(len(console["assets"]) == 3, "Duplicate intro media entry")
        for item in console["assets"]:
            source = intro_root / item["file"]
            require(source.stat().st_size == item["bytes"] and sha256(source) == item["sha256"], "Original intro digest mismatch")
            copy(source, "assets/room/intros/" + item["file"])
    copy(intro_root / "manifest.json", "assets/room/intros/manifest.json")
    copy(ROOT / "assets/CREDITS.md", "assets/CREDITS.md")
    copy(build / "build-source.json", "build-source.json")

    executable = ROOT / ".cache/package-tools/prepare-package-art"
    arguments = [ROOT / presentation.get("icon", {}).get("source", "assets/shelf-icon.png"), app / "sce_sys/icon0.png"]
    if executable.with_suffix(".exe").exists():
        executable = executable.with_suffix(".exe")
        if os.name != "nt":
            arguments = [subprocess.check_output(["wslpath", "-w", str(path)], text=True).strip() for path in arguments]
    require(executable.is_file(), "Build tools/build-package-art.js first")
    original = sha256(ROOT / "assets/shelf-icon.png")
    run([executable, *arguments])
    require(original == sha256(ROOT / "assets/shelf-icon.png"), "Packaging changed the source artwork")
    icon = (app / "sce_sys/icon0.png").read_bytes()
    require(icon[:8] == b"\x89PNG\r\n\x1a\n" and struct.unpack_from(">II", icon, 16) == (512, 512), "Invalid tile icon")

    readme = f"""# isShelf: local UI test

Source: https://github.com/Iswebro/console-shelf-ps5/tree/{commit}
Unofficial test identity: PPSA99541. This build is not hardware-validated or a public release.

This private test displays Shelf's UI/settings and real library lists, and enables launch tests.
The bundled original helper is embedded in eboot.bin and sent only to the console's existing
loopback ELF loader when you select a launch action. It waits for Shelf's explicit commitment
and cooperative closure; it never kills another app or writes emulator settings.
{permission_notice}
The helper waits through a monitored idle second after Shelf closes, then sends one
SystemService launch request. It only observes the running target and the shell focus
flag afterward; there is no second launch or focus-changing API call.
Observed shell focus is not proof of TV output; that result needs the owner's console test.
It expects the console's existing ftpsrv on 127.0.0.1:2121; no PC DNS host is needed for discovery.
The app writes only its own preferences, launch request/status and diagnostics under /data/console-shelf.

Before copying, list /data/homebrew and /user/app and stop if this title identity already exists.
The isolated destination is /data/homebrew/PPSA99541. Never replace another app.
Verify full raw readback hashes; ftpsrv must report SELF conversion disabled before reading eboot.bin.
Use the existing trusted ShadowMount procedure to expose this new app tile, with other apps closed.
Keep firmware 12.20 and decline system update prompts. This folder contains no firmware updater.

First test: open Shelf; check the picture, D-pad/stick, Cross/Circle and Options.
Square cycles Grid, Swipe and Shelf. Shelf shows eight consoles on the room's two deep display counters;
L1/R1 swaps pages with only the consoles moving; Down continues between tiers.
Sort & filter -> Shelf pages also offers one curated display of up to eight consoles,
chosen in Consoles. Appearance -> Motion offers Reduced motion.
Play room customises the TV: console artwork, personal text or PNG, clock, rotating
favourite games, intro pictures/videos (installed emulators or all consoles), or screen off.
All 52 consoles include original Shelf stills and silent animations. Your own PNG/MPEG
in the Shelf data folder takes priority. These are Shelf originals, not manufacturer boot videos.
Triangle stars games in Shelf's library. Personal PNGs stay in /data/console-shelf:
tv-image.png or intros/<console-id>.png, up to 2048 x 2048. Silent intro videos use
intros/<console-id>.mpg (MPEG-1); the PC preparation tool makes compatible copies.
The TV stays visible in console details and libraries. Static for missing emulators/games
is optional in Play room; reduced motion freezes it and videos on a still frame.
Static is drawn with fine screen-resolution grain. The room menu has its own quiet dark backboard.
Up/Down stays on the current Shelf page; Up from its top row reaches the toolbar.
Sort, visibility and favourite settings apply to all three views.
Appearance -> Missing emulator artwork chooses Black & white (default) or Full colour.
Only systems without a detected emulator are desaturated; PS4/PS5 stay in colour.
The choice is independent of console visibility and survives closing/reopening Shelf.
The selection background is confirmed; opening still shows black before the grid.
Check Nintendo 64's library and the PS1/PS2/PS3 counts, then save a layout/visibility choice.
Close and reopen Shelf to check persistence. Report any blank screen, crash or incorrect input.
The game library shows a permanent launch panel before you select a game.
Library only / Open emulator library means choosing the game inside that emulator.
Direct play / Play game means the selected game is passed directly to RetroArch.
Changing Preferred emulator updates the panel; RetroArch route choices save across reopen.
Test PS2 -> Open emulator library first. Allow up to ten seconds after Shelf fades out.
Check that PS5SX2 appears on the TV, rather than remaining behind the PS5 home screen.
PS1/PS3 and other detected standalone emulators currently open their own libraries.
For N64 using RetroArch, a selected game uses the external --rom/--core forwarder contract.
All launch routes remain experimental until their console result is recorded. These checks and
the final source/license/relinking package are required before a public download.

Rollback: close Shelf and remove only its owned app folder using the established app-removal procedure.
Keep /data/console-shelf if you want to retain its preferences; existing emulator/game data is separate.

Licenses and credits are included. The exact complete library sources and relinking kit must still
be assembled before any public binary release. This local package is for the owner's private test only.
"""
    (app / "START-HERE.md").write_text(readme, encoding="utf8", newline="\n")
    files = [{"path": p.relative_to(app).as_posix(), "bytes": p.stat().st_size, "sha256": sha256(p)}
             for p in sorted(app.rglob("*")) if p.is_file()]
    manifest = {"schema": 1, "purpose": "private native launch test; not a public release", "titleId": "PPSA99541", "sourceCommit": commit,
                "hardwareTest": "not run", "launcherRuntime": launcher,
                "sourceFingerprintSHA256": hashlib.sha256((build / "build-source.json").read_bytes()).hexdigest(), "files": files}
    (destination / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    archive = destination / "Console-Shelf-UI-test.zip"
    with zipfile.ZipFile(archive, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as output:
        for item in files:
            output.write(app / item["path"], "PPSA99541/" + item["path"])
    with zipfile.ZipFile(archive) as completed:
        require(completed.testzip() is None, "Test package ZIP integrity failed")
        for item in files:
            raw = completed.read("PPSA99541/" + item["path"])
            require(len(raw) == item["bytes"] and hashlib.sha256(raw).hexdigest() == item["sha256"], "ZIP member changed")
    receipt = {"directory": str(destination.relative_to(ROOT)), "archiveBytes": archive.stat().st_size, "archiveSHA256": sha256(archive),
               "files": len(files), "appBytes": sum(item["bytes"] for item in files), "titleId": "PPSA99541", "sourceCommit": commit,
               "consoleInstall": "not performed", "publicRelease": "not published", "launcherRuntime": launcher}
    (CACHE / "test-packages/latest.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps(receipt, indent=2))


if __name__ == "__main__":
    main()
