# SPDX-License-Identifier: GPL-3.0-only
"""Assemble a private complete source/object kit; never publish or deploy it."""
import argparse
import datetime
import hashlib
import io
import json
from pathlib import Path
import subprocess
import uuid
import zipfile
from prepare_corresponding_source import prepare, SOURCE_CACHE
from ps5_common import ROOT, CACHE, member_path, sha256
from relink_ps5 import LIBRARIES, OBJECTS, checked
from verify_ps5_build import require, source_fingerprint

INSTRUCTIONS = """Console Shelf source and relink kit (development build)

This folder contains the tracked application source, artwork, credits and
licenses, complete source archives and original release recipes for the three
linked libraries, plus Shelf's application objects and replaceable static
libraries. PKGBUILD files are references; this kit does not run them.

From the extracted project folder, with the prerequisites in docs/PS5-BUILD.md:
  npm run relink:ps5
This downloads the hash-pinned public SDK/compiler dependencies if absent,
rebuilds the converter from source, and links the retained application objects.
It must reproduce the reference ELF and eboot.bin exactly. New outputs go into
.cache/ps5/relink-checks; it does not change an installed console application.

To link a modified PS5 libiconv static library instead:
  npm run relink:ps5 -- --libiconv /absolute/WSL/path/to/libiconv.a
--sdl and --libsamplerate similarly replace the other static libraries.
On Windows these arguments use WSL paths, for example /mnt/c/Build/libiconv.a.
Library sources and recipes are in .cache/ps5/corresponding-source. Rebuild a
library for the PS5 target with the public SDK before supplying its .a file.
An overridden library is allowed to change the executable hash. Structural
validation is not proof of hardware compatibility for the modified program.

To change and rebuild the complete app, use npm run build:ps5 instead.
Read docs/PS5-UI-TEST.md for the current test scope. This kit does not install,
launch, update firmware or grant privileges. No games, BIOS, keys, account
information, console logs or personal inventory are included.

This is a private preparation artifact. Publication still requires the full
hardware and launch acceptance in docs/PROTOTYPE-PLAN.md.
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True, help="Verified matching object/library snapshot")
    args = parser.parse_args()
    require(not subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT).strip(),
            "Commit reviewed source before assembling the source kit")
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    inputs = args.inputs.resolve()
    record = json.loads((inputs / "link-inputs.json").read_text())
    checked(inputs / "build-source.json", record["sourceFingerprint"])
    fingerprint = json.loads((inputs / "build-source.json").read_text())
    require(source_fingerprint() == fingerprint, "Source kit does not match the compiled application inputs")
    for path, digest in fingerprint.items():
        raw = subprocess.check_output(["git", "cat-file", "blob", commit + ":" + path], cwd=ROOT)
        require(hashlib.sha256(raw).hexdigest() == digest, "Committed source differs from application build")
    require(record["objectOrder"] == [name + ".o" for name in OBJECTS], "Unexpected object snapshot")
    sources = prepare()
    name = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ-") + uuid.uuid4().hex[:8]
    output = CACHE / "source-kits" / name
    output.mkdir(parents=True)
    archive = output / "Console-Shelf-Source-and-Relink.zip"
    members = []
    def add(target, path, raw):
        member_path(path)
        target.writestr("Console-Shelf/" + path, raw)
        members.append({"path": path, "bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()})
    tracked = subprocess.check_output(["git", "archive", "--format=zip", commit], cwd=ROOT)
    with zipfile.ZipFile(archive, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=7) as target:
        with zipfile.ZipFile(io.BytesIO(tracked)) as source:
            require(source.testzip() is None, "Tracked source archive failed integrity")
            for item in source.infolist():
                if item.is_dir():
                    continue
                require(not item.filename.startswith((".cache/", ".git/")), "Unexpected private source member")
                add(target, item.filename, source.read(item))
        base = ".cache/ps5/relink-inputs/"
        for name in ("build-source.json", "link-inputs.json"):
            add(target, base + name, (inputs / name).read_bytes())
        for group, names in (("objects", record["objectOrder"]), ("libraries", LIBRARIES)):
            for name in names:
                path = checked(inputs / group / name, record[group][name])
                add(target, base + group + "/" + name, path.read_bytes())
        for source in sources["sources"]:
            for key in ("archive", "recipe"):
                path = SOURCE_CACHE / source[key]
                add(target, ".cache/ps5/corresponding-source/" + path.name, path.read_bytes())
        add(target, "SOURCE-KIT.txt", INSTRUCTIONS.encode())
        metadata = {"schema": 1, "sourceCommit": commit, "applicationSelf": record["referenceSelf"],
                    "sourceArchives": sources["sources"], "privatePreparation": True,
                    "hardwareAcceptance": "pending", "files": members.copy()}
        add(target, "SOURCE-KIT-MANIFEST.json", (json.dumps(metadata, indent=2) + "\n").encode())
    with zipfile.ZipFile(archive) as completed:
        require(completed.testzip() is None, "Source kit ZIP failed integrity")
        require(len(completed.namelist()) == len(members), "Unexpected source kit member count")
        for item in members:
            raw = completed.read("Console-Shelf/" + item["path"])
            require(len(raw) == item["bytes"] and hashlib.sha256(raw).hexdigest() == item["sha256"],
                    "Source kit member changed")
    result = {"directory": str(output.relative_to(ROOT)), "sourceCommit": commit, "files": len(members),
              "archiveBytes": archive.stat().st_size, "archiveSHA256": sha256(archive),
              "applicationSelf": record["referenceSelf"], "coldRelinkTest": "not yet run", "published": False}
    (output / "source-kit-verification.json").write_text(json.dumps(result, indent=2) + "\n")
    (CACHE / "source-kits/latest.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
