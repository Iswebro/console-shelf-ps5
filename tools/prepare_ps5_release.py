# SPDX-License-Identifier: GPL-3.0-only
"""Prepare public prototype assets after explicit hardware checks. Never publishes or contacts a console."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import re
import subprocess
import uuid
import zipfile

from ps5_common import CACHE, ROOT, member_path, sha256
from verify_ps5_build import require, source_fingerprint, verify_recorded_hashes

REQUIRED_CHECKS = ("ui_navigation", "ui_settings", "circle_close", "settings_persist", "restart_reopen",
                   "psxs5_menu", "ps5sx2_menu", "ps5sx3_menu", "retroarch_n64_game")
ROUTES = ("PSXS5: emulator shelf", "PS5SX2: emulator shelf", "PS5SX3: emulator shelf",
          "RetroArch: selected Nintendo 64 game")


def cache_path(relative):
    normalized = str(relative).replace("\\", "/")
    member_path(normalized)
    path = (ROOT / normalized).resolve()
    require(path.is_relative_to((ROOT / ".cache").resolve()), "Artifact path must stay inside the project cache")
    return path


def digest(raw):
    return {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}


def check(raw, record, label):
    require(digest(raw) == {key: record[key] for key in ("bytes", "sha256")}, label + " changed")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--acceptance", type=Path, required=True, help="Reviewed owner hardware-check JSON")
    parser.add_argument("--version", default="v0.1.0-alpha.1")
    args = parser.parse_args()
    require(re.fullmatch(r"v[0-9]+\.[0-9]+\.[0-9]+-(?:alpha|beta)\.[0-9]+", args.version) is not None,
            "Use an explicit prototype version")
    accepted = json.loads(args.acceptance.read_text(encoding="utf-8"))
    require(accepted.get("schema") == 1 and accepted.get("firmware") == "12.20", "Missing target hardware evidence")
    checks = accepted.get("checks", {})
    pending = [name for name in REQUIRED_CHECKS if checks.get(name) is not True]
    require(not pending, "Hardware checks still pending: " + ", ".join(pending))
    require(not subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT).strip(),
            "Commit reviewed source and documentation before release preparation")

    package = json.loads((CACHE / "test-packages/installed-current.json").read_text(encoding="utf-8"))
    kit = json.loads((CACHE / "source-kits/latest.json").read_text(encoding="utf-8"))
    require(package["sourceCommit"] == kit["sourceCommit"], "App and complete source kit have different revisions")
    build = verify_recorded_hashes(CACHE / "build")
    require(accepted.get("selfSHA256") == build["self"]["sha256"] == kit["applicationSelf"]["sha256"],
            "Hardware checks or source kit refer to a different executable")
    require(kit["coldRelinkTest"].startswith("passed;"), "Matching private source kit lacks an exact cold relink")
    source = cache_path(package["directory"])
    manifest = json.loads((source / "manifest.json").read_text(encoding="utf-8"))
    require(manifest["titleId"] == "PPSA99541" and manifest["sourceCommit"] == package["sourceCommit"],
            "Private app identity differs")
    files = {entry["path"]: entry for entry in manifest["files"]}
    require(len(files) == len(manifest["files"]) and "START-HERE.md" in files, "Invalid private app manifest")
    transfer = json.loads(cache_path(package["consoleReceipt"]).read_text(encoding="utf-8"))
    require(transfer["sourceCommit"] == package["sourceCommit"] and
            transfer.get("finalFullReadbackFiles") == len(files) and transfer.get("preferencesUnchanged") is True,
            "Matching complete console readback receipt is required")
    app_data = {}
    for name, entry in files.items():
        member_path(name)
        path = (source / "PPSA99541" / name).resolve()
        require(path.is_relative_to((source / "PPSA99541").resolve()), "App member escapes its folder")
        raw = path.read_bytes()
        check(raw, entry, name)
        app_data[name] = raw
    check(app_data["eboot.bin"], build["self"], "Hardware-tested program")
    fingerprint = json.loads(app_data["build-source.json"])
    require(fingerprint == source_fingerprint(), "Compiled inputs differ from the release source tree")
    runtime = manifest["launcherRuntime"]
    require(runtime["sha256"] == kit["launcherRuntime"]["sha256"] == build["embeddedLaunchHelper"]["sha256"],
            "Embedded helper differs between app, source kit and build")
    require(runtime["runtimeMode"] == "standard-sdk" and runtime["permissionChanges"] is True,
            "This hardware acceptance is for the explicitly selected standard-SDK variant")

    source_archive = cache_path(kit["directory"]) / "Console-Shelf-Source-and-Relink.zip"
    require(source_archive.stat().st_size == kit["archiveBytes"] and sha256(source_archive) == kit["archiveSHA256"],
            "Complete source kit archive changed")
    with zipfile.ZipFile(source_archive) as archive:
        require(archive.testzip() is None, "Source kit ZIP integrity failed")
        require(len(archive.namelist()) == len(set(archive.namelist())) == kit["files"], "Duplicate source kit members")
        kit_data = {}
        for name in archive.namelist():
            member_path(name)
            require(name.startswith("Console-Shelf/"), "Unexpected source-kit prefix")
            kit_data[name.removeprefix("Console-Shelf/")] = archive.read(name)
    kit_manifest = json.loads(kit_data["SOURCE-KIT-MANIFEST.json"])
    require(kit_manifest["sourceCommit"] == package["sourceCommit"], "Source manifest revision differs")
    require(kit_manifest["applicationSelf"] == build["self"], "Source manifest program differs")
    for entry in kit_manifest["files"]:
        check(kit_data[entry["path"]], entry, "Source member " + entry["path"])
    for name, expected in fingerprint.items():
        require(hashlib.sha256(kit_data[name]).hexdigest() == expected, "Corresponding compiled source differs")

    # Publish only closed, non-personal fields; never copy free-form owner logs or notes.
    evidence = {"schema": 1, "firmware": "12.20", "selfSHA256": build["self"]["sha256"],
                "compiledSourceCommit": package["sourceCommit"],
                "checks": {name: True for name in REQUIRED_CHECKS}, "verifiedRoutes": list(ROUTES),
                "basis": "Owner console reports; host checks and raw readbacks recorded separately",
                "limitations": ["Opening can show black before the grid", "Standalone emulators open their shelves",
                                "Other dedicated-emulator and native-title routes remain experimental",
                                "Discovery requires the existing console-local FTP service"]}
    # Preserve code, objects, libraries and upstream archives byte-for-byte. Only the
    # source kit's publication instructions and metadata change from the cold-tested baseline.
    instructions = kit_data["SOURCE-KIT.txt"].decode("utf-8")
    old = ("This is a private preparation artifact. Publication still requires the full\n"
           "hardware and launch acceptance in docs/PROTOTYPE-PLAN.md.")
    require(old in instructions, "Source kit publication notice was not found")
    instructions = instructions.replace(old,
        "Published prototype " + args.version + ". The matching executable has the owner\n"
        "hardware checks listed in the release's HARDWARE-CHECKS.json. Other routes\n"
        "remain experimental. Rebuilding or changing the program needs new tests.")
    instructions = instructions.replace("Console Shelf source and relink kit (development build)",
                                        "Console Shelf " + args.version + " source and relink kit")
    instructions += ("\nTracked project documents retain the original compiled-source snapshot.\n"
                     "The release hardware checks record later owner acceptance of that exact program.\n")
    kit_data["SOURCE-KIT.txt"] = instructions.encode("utf-8")
    kit_manifest.update({"privatePreparation": False, "releaseVersion": args.version,
                         "hardwareAcceptance": evidence, "coldRelinkBaselineArchiveSHA256": kit["archiveSHA256"]})
    for entry in kit_manifest["files"]:
        if entry["path"] == "SOURCE-KIT.txt":
            entry.update(digest(kit_data[entry["path"]]))
    kit_data["SOURCE-KIT-MANIFEST.json"] = (json.dumps(kit_manifest, indent=2) + "\n").encode()

    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ-") + uuid.uuid4().hex[:8]
    output = CACHE / "releases" / (args.version + "-" + stamp)
    output.mkdir(parents=True, exist_ok=False)
    source_zip = output / ("Console-Shelf-" + args.version + "-Source-and-Relink.zip")
    with zipfile.ZipFile(source_zip, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=7) as target:
        for name, raw in kit_data.items():
            target.writestr("Console-Shelf/" + name, raw)
    source_info = {"file": source_zip.name, "bytes": source_zip.stat().st_size, "sha256": sha256(source_zip)}

    guide = (ROOT / "docs/PS5-INSTALL.md").read_text(encoding="utf-8")
    old = ("Preparation guide for the first prototype. A public binary has not been published yet; see the "
           "[acceptance status](PROTOTYPE-PLAN.md). The tested app currently appears as **Console Shelf (UI test)**.")
    require(old in guide, "Install-guide preparation notice was not found")
    guide = guide.replace(old, "Native PS5 prototype " + args.version + ", tested on firmware **12.20**. "
                          "The app appears as **Console Shelf (UI test)**.")
    guide = guide.replace("from this repository's release page when available", "from this release")
    guide += "\n## Verified launch routes\n\n" + "\n".join("- " + route for route in ROUTES)
    guide += ("\n\nProgram source: https://github.com/Iswebro/console-shelf-ps5/tree/" + package["sourceCommit"] +
              "\n\nThe matching complete source/object/relink kit is supplied beside this download. "
              "Licenses and credits are included. HARDWARE-CHECKS.json records this build's checked scope.\n")
    app_data["START-HERE.md"] = guide.encode("utf-8")
    public_files = [{"path": name, **digest(raw)} for name, raw in sorted(app_data.items())]
    public_manifest = {"schema": 1, "version": args.version, "titleId": "PPSA99541",
                       "compiledSourceCommit": package["sourceCommit"], "launcherRuntime": runtime,
                       "hardwareChecks": evidence, "sourceKit": source_info, "files": public_files,
                       "programComparison": "All hardware-tested files unchanged except START-HERE.md documentation"}
    app_zip = output / ("Console-Shelf-PS5-" + args.version + ".zip")
    with zipfile.ZipFile(app_zip, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as target:
        for name, raw in sorted(app_data.items()):
            target.writestr("PPSA99541/" + name, raw)
        target.writestr("README.md", guide.encode("utf-8"))
        target.writestr("MANIFEST.json", json.dumps(public_manifest, indent=2) + "\n")
        target.writestr("HARDWARE-CHECKS.json", json.dumps(evidence, indent=2) + "\n")
    for path, expected in ((source_zip, {"Console-Shelf/" + name: raw for name, raw in kit_data.items()}),
                           (app_zip, {"PPSA99541/" + name: raw for name, raw in app_data.items()})):
        with zipfile.ZipFile(path) as completed:
            require(completed.testzip() is None, "Public ZIP integrity failed")
            require(len(completed.namelist()) == len(set(completed.namelist())), "Duplicate public ZIP members")
            for name, raw in expected.items():
                require(completed.read(name) == raw, "Public archive member changed")
    app_info = {"file": app_zip.name, "bytes": app_zip.stat().st_size, "sha256": sha256(app_zip)}
    sums = output / "SHA256SUMS.txt"
    sums.write_text("\n".join(info["sha256"] + "  " + info["file"] for info in (app_info, source_info)) + "\n",
                    encoding="ascii")
    receipt = {"schema": 1, "version": args.version, "directory": str(output.relative_to(ROOT)),
               "compiledSourceCommit": package["sourceCommit"], "applicationSelf": build["self"],
               "assets": [app_info, source_info, {"file": sums.name, **digest(sums.read_bytes())}],
               "sourceKitColdRelink": "pending for the public ZIP; original retained-input baseline passed",
               "consoleDocumentReadback": "pending", "published": False}
    (output / "release-preparation.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    (CACHE / "releases/latest.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(receipt, indent=2))


if __name__ == "__main__":
    main()
