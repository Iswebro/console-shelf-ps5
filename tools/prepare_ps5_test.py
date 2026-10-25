# SPDX-License-Identifier: GPL-3.0-only
"""Assemble a local UI-only app folder. Never connects to, installs on or launches a PS5."""
import datetime
import hashlib
import json
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
    verify_recorded_hashes(build)
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
    verify_presentation()
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
    for name in ("Poppins-Regular.ttf", "Poppins-SemiBold.ttf", "OFL.txt"):
        copy(ROOT / "native/assets/fonts" / name, "native/assets/fonts/" + name)
    for name in ("SDL2-PS5.txt", "libsamplerate-0.2.2.txt", "LGPL-2.1.txt"):
        copy(ROOT / "native/third_party/licenses" / name, "licenses/" + name)
    for name in ("LICENSE", "THIRD_PARTY.md"):
        copy(ROOT / name, name)
    copy(ROOT / "native/third_party/cjson/LICENSE", "licenses/cJSON-MIT.txt")
    copy(ROOT / "native/third_party/stb/stb_image.h", "licenses/stb_image-source-with-license.h")
    copy(ROOT / "native/third_party/stb/stb_truetype.h", "licenses/stb_truetype-source-with-license.h")
    copy(ROOT / "native/third_party/stb/stb_image_write.h", "licenses/stb_image_write-source-with-license.h")
    copy(ROOT / "assets/CREDITS.md", "assets/CREDITS.md")
    copy(build / "build-source.json", "build-source.json")

    executable = ROOT / ".cache/package-tools/prepare-package-art"
    arguments = [ROOT / "assets/shelf-icon.png", app / "sce_sys/icon0.png"]
    if executable.with_suffix(".exe").exists():
        executable = executable.with_suffix(".exe")
        arguments = [subprocess.check_output(["wslpath", "-w", str(path)], text=True).strip() for path in arguments]
    require(executable.is_file(), "Build tools/build-package-art.js first")
    original = sha256(ROOT / "assets/shelf-icon.png")
    run([executable, *arguments])
    require(original == sha256(ROOT / "assets/shelf-icon.png"), "Packaging changed the source artwork")
    icon = (app / "sce_sys/icon0.png").read_bytes()
    require(icon[:8] == b"\x89PNG\r\n\x1a\n" and struct.unpack_from(">II", icon, 16) == (512, 512), "Invalid tile icon")

    readme = f"""# Console Shelf: local UI test

Source: https://github.com/Iswebro/console-shelf-ps5/tree/{commit}
Unofficial test identity: PPSA99541. This build is not hardware-validated or a public release.

This test displays Shelf's Grid/Swipe UI, controller settings and read-only library lists.
Emulator/game launch actions remain unavailable. No launch helper is included.
It expects the console's existing ftpsrv on 127.0.0.1:2121; no PC DNS host is needed for discovery.
The app writes only its own settings under /data/console-shelf.

Before copying, list /data/homebrew and /user/app and stop if this title identity already exists.
The isolated destination is /data/homebrew/PPSA99541. Never replace another app.
Verify full raw readback hashes; ftpsrv must report SELF conversion disabled before reading eboot.bin.
Use the existing trusted ShadowMount procedure to expose this new app tile, with other apps closed.
Keep firmware 12.20 and decline system update prompts. This folder contains no firmware updater.

First test: open Shelf; check the picture, D-pad/stick, Cross/Circle and Options.
Check Nintendo 64's library and the PS1/PS2/PS3 counts, then save a layout/visibility choice.
Close and reopen Shelf to check persistence. Report any blank screen, crash or incorrect input.
These results are required before enabling launches or publishing a download.

Rollback: close Shelf and remove only its owned app folder using the established app-removal procedure.
Keep /data/console-shelf if you want to retain its preferences; existing emulator/game data is separate.

Licenses and credits are included. The exact complete library sources and relinking kit must still
be assembled before any public binary release. This local package is for the owner's private test only.
"""
    (app / "START-HERE.md").write_text(readme, encoding="utf8", newline="\n")
    files = [{"path": p.relative_to(app).as_posix(), "bytes": p.stat().st_size, "sha256": sha256(p)}
             for p in sorted(app.rglob("*")) if p.is_file()]
    manifest = {"schema": 1, "purpose": "local UI-only test; not a public release", "titleId": "PPSA99541", "sourceCommit": commit,
                "hardwareTest": "not run", "sourceFingerprintSHA256": hashlib.sha256((build / "build-source.json").read_bytes()).hexdigest(), "files": files}
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
               "consoleInstall": "not performed", "publicRelease": "not published"}
    (CACHE / "test-packages/latest.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps(receipt, indent=2))


if __name__ == "__main__":
    main()
