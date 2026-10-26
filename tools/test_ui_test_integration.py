# SPDX-License-Identifier: GPL-3.0-only
"""Exercise the actual native event loop using only a fresh, local owned tree."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parent.parent
folder = ROOT / ".cache/ui-test-integration" / uuid.uuid4().hex
data = folder / "data"
evidence = folder / "evidence"
data.mkdir(parents=True)
executable = ROOT / ".cache/native-host" / ("shelf-host.exe" if os.name == "nt" else "shelf-host")
client = ROOT / "tools/ui_test_client.py"
flags = {"creationflags": subprocess.CREATE_NO_WINDOW} if os.name == "nt" else {}
bridge = data / "ui-test"


def response():
    return json.loads((bridge / "response.json").read_text(encoding="utf-8"))


def wait_for(predicate, seconds=20):
    until = time.monotonic() + seconds
    while time.monotonic() < until:
        try:
            value = response()
            if predicate(value):
                return value
        except (OSError, ValueError):
            pass
        time.sleep(.05)
    raise AssertionError("Native bridge response did not arrive")


def command(action):
    subprocess.run([sys.executable, str(client), "--local-root", str(bridge), "--action", action,
                    "--output-dir", str(evidence)], cwd=ROOT, check=True, timeout=25, **flags)
    return response()


def frame_digest():
    return hashlib.sha256((bridge / "frame.png").read_bytes()).hexdigest()


with (folder / "native.log").open("wb") as log:
    process = subprocess.Popen([str(executable), "--asset-root", str(ROOT), "--data-dir", str(data),
                                "--headless", "--ui-test"], cwd=ROOT, stdout=log, stderr=log, **flags)
    try:
        ready = wait_for(lambda value: value["result"] == "ready" and value["frameAvailable"])
        assert ready["active"] and ready["sequence"] == 0
        observed = command("snapshot")
        assert observed["result"] == "ok" and not observed["actionApplied"]
        first_frame = frame_digest()
        moved = command("right")
        assert moved["result"] == "ok" and moved["actionApplied"]
        assert moved["selectedSystem"] != observed["selectedSystem"]
        assert moved["revision"] > observed["revision"] and frame_digest() != first_frame

        # Duplicate input must not advance or repeat selection, even after more polls.
        request = (bridge / "request.txt").read_bytes()
        (bridge / "request.txt").write_bytes(request)
        time.sleep(.4)
        assert response()["sequence"] == moved["sequence"]
        assert response()["selectedSystem"] == moved["selectedSystem"]

        # A command based on an earlier image is acknowledged but never applied.
        stale = (f'ISSHELF_UI_TEST_1 {moved["session"]} {moved["nextSequence"]} '
                 f'{observed["revision"]} left\n').encode("ascii")
        (bridge / "request.tmp").write_bytes(stale)
        (bridge / "request.tmp").replace(bridge / "request.txt")
        rejected = wait_for(lambda value: value["sequence"] == moved["nextSequence"])
        assert rejected["result"] == "stale" and not rejected["actionApplied"]
        assert rejected["selectedSystem"] == moved["selectedSystem"]

        details = command("confirm")
        assert details["page"] == 1 and details["actionApplied"]
        settings = command("settings")
        assert settings["page"] == 3
        changed = command("confirm")
        assert changed["settingsSaved"] and changed["actionApplied"]
        assert (data / "settings.ini").is_file()
        stopped = command("stop")
        assert not stopped["active"] and stopped["result"] == "stopped"
        assert process.poll() is None, "Stopping test control must leave the app open"
    finally:
        process.terminate()
        process.wait(timeout=10)

# Default host invocation performs no bridge I/O.
disabled = folder / "disabled"
subprocess.run([str(executable), "--asset-root", str(ROOT), "--data-dir", str(disabled), "--headless"],
               cwd=ROOT, check=True, timeout=30, stdout=subprocess.DEVNULL, **flags)
assert not (disabled / "ui-test").exists()
invalid = subprocess.run([str(executable), "--asset-root", str(ROOT), "--data-dir", str(folder / "invalid"),
                          "--ui-test", "--script", "right"], cwd=ROOT, capture_output=True, timeout=10, **flags)
assert invalid.returncode != 0 and not (folder / "invalid/ui-test").exists()
summary = {"result": "passed", "nativeEventLoop": True, "transport": "local fixture",
           "commands": ["snapshot", "right", "confirm", "settings", "confirm", "stop"],
           "duplicateRejected": True, "staleRevisionRejected": True, "disabledByDefault": True,
           "ps5Contacted": False, "physicalDisplay": "not tested"}
(folder / "verification.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
print(json.dumps({**summary, "evidence": str(folder)}))
