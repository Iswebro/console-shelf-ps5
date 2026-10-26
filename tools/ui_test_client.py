# SPDX-License-Identifier: GPL-3.0-only
"""Send one menu action to a private, owner-enabled isShelf UI test bridge.

This client uses the console's existing anonymous ftpsrv, or an explicit local
bridge folder for host testing. It reads only response.json, request.txt and
frame.png, and publishes only request.tmp -> request.txt. It does not sign in to
PSN, install software, control the PS5 shell, or send arbitrary file paths.

Exactly one controller client may use a bridge session. An uncertain publish or
acknowledgement never causes an automatic action resend. Inspect the preserved
request and response before deciding what to do next. PNGs are app-rendered
frames; they do not establish what the TV displayed over HDMI.
"""

import argparse
import ftplib
import hashlib
import io
import ipaddress
import json
import os
import re
import sys
import time
from pathlib import Path


REMOTE_ROOT = "/data/console-shelf/ui-test"
RESPONSE_LIMIT = 4096
REQUEST_LIMIT = 160
FRAME_LIMIT = 16 * 1024 * 1024
UINT64_MAX = (1 << 64) - 1
ACTIONS = (
    "snapshot", "stop", "up", "down", "left", "right", "confirm", "back",
    "settings", "layout", "favorite", "previous-tab", "tab",
)
SESSION_RE = re.compile(r"[0-9a-f]{32}\Z", re.ASCII)
REQUEST_RE = re.compile(
    rb"ISSHELF_UI_TEST_1 ([0-9a-f]{32}) ([0-9]+) ([0-9]+) ([a-z-]+)\n\Z"
)


class ClientError(RuntimeError):
    """No new action should be attempted until this condition is understood."""


class MissingBridgeFile(ClientError):
    pass


class OutcomeUnknown(ClientError):
    """A command may have been consumed; it must not be resent automatically."""


def lan_ipv4(value):
    try:
        address = ipaddress.IPv4Address(value)
    except ipaddress.AddressValueError as error:
        raise argparse.ArgumentTypeError("Use a literal private-LAN or loopback IPv4 address.") from error
    allowed = ("10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16", "127.0.0.0/8")
    if not any(address in ipaddress.IPv4Network(network) for network in allowed):
        raise argparse.ArgumentTypeError("Only private-LAN or loopback IPv4 targets are accepted.")
    return str(address)


def fnv1a64(data):
    value = 0xCBF29CE484222325
    for byte in data:
        value = ((value ^ byte) * 0x100000001B3) & UINT64_MAX
    return f"{value:016x}"


def _no_duplicate_keys(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ClientError("Bridge response has a duplicate JSON key.")
        result[key] = value
    return result


def _integer(value, minimum=0, maximum=UINT64_MAX):
    return type(value) is int and minimum <= value <= maximum


def decode_response(raw):
    if not raw or len(raw) > RESPONSE_LIMIT or b"\0" in raw:
        raise ClientError("Bridge response is empty, oversized or contains a NUL.")
    try:
        result = json.loads(raw.decode("utf-8"), object_pairs_hook=_no_duplicate_keys)
    except (UnicodeError, ValueError) as error:
        raise ClientError("Bridge response is not valid UTF-8 JSON.") from error
    if not isinstance(result, dict):
        raise ClientError("Bridge response must be a JSON object.")
    if type(result.get("protocol")) is not int or result["protocol"] != 1:
        raise ClientError("Unsupported bridge protocol.")
    if type(result.get("active")) is not bool:
        raise ClientError("Bridge response lacks its activation state.")
    if not isinstance(result.get("session"), str) or not SESSION_RE.fullmatch(result["session"]):
        raise ClientError("Bridge response has an invalid session.")
    for name in ("sequence", "nextSequence", "revision", "frameBytes"):
        if not _integer(result.get(name)):
            raise ClientError(f"Bridge response has an invalid {name}.")
    if result["sequence"] == UINT64_MAX or result["nextSequence"] != result["sequence"] + 1:
        raise ClientError("Bridge response has an inconsistent next sequence.")
    if result.get("action") not in ACTIONS:
        raise ClientError("Bridge response names an unsupported action.")
    if not isinstance(result.get("result"), str) or not re.fullmatch(r"[a-z][a-z-]{0,31}", result["result"]):
        raise ClientError("Bridge response has an invalid result.")
    for name in ("actionApplied", "settingsSaved", "frameAvailable"):
        if type(result.get(name)) is not bool:
            raise ClientError(f"Bridge response has an invalid {name}.")
    if result["frameBytes"] > FRAME_LIMIT:
        raise ClientError("Bridge frame exceeds the transfer bound.")
    if not isinstance(result.get("frameFNV64"), str) or not re.fullmatch(r"[0-9a-f]{16}", result["frameFNV64"]):
        raise ClientError("Bridge response has an invalid frame checksum.")
    if result["frameAvailable"] and result["frameBytes"] < 24:
        raise ClientError("Bridge response names an empty frame.")
    if not _integer(result.get("page"), 0, 4) or not _integer(result.get("tab"), 0, 3):
        raise ClientError("Bridge response has an invalid page or tab.")
    for name in ("focus", "gameFocus"):
        if not _integer(result.get(name), -1, 100000):
            raise ClientError(f"Bridge response has an invalid {name}.")
    if not isinstance(result.get("selectedSystem"), str) or not re.fullmatch(r"[a-z0-9_-]{1,64}", result["selectedSystem"]):
        raise ClientError("Bridge response has an invalid selected system.")
    return result


def validate_frame(data, response):
    if len(data) != response["frameBytes"] or fnv1a64(data) != response["frameFNV64"]:
        raise ClientError("Returned frame does not match its acknowledged byte count and checksum.")
    if not data.startswith(b"\x89PNG\r\n\x1a\n") or data[12:16] != b"IHDR":
        raise ClientError("Returned frame is not a PNG image.")
    width, height = int.from_bytes(data[16:20], "big"), int.from_bytes(data[20:24], "big")
    if not 0 < width <= 4096 or not 0 < height <= 4096 or width * height > 16 * 1024 * 1024:
        raise ClientError("Returned PNG dimensions exceed the image bound.")


class _PinnedFTP(ftplib.FTP):
    def makepasv(self):
        _advertised, port = super().makepasv()
        return self.sock.getpeername()[0], port


class FtpTransport:
    """One FTP connection, fixed owned files and no reconnect/action retry."""

    def __init__(self, host, port=2121, timeout=5):
        self.host = lan_ipv4(host)
        if type(port) is not int or not 1 <= port <= 65535:
            raise ClientError("Invalid FTP port.")
        self.ftp = _PinnedFTP(timeout=timeout)
        try:
            self.ftp.connect(self.host, port)
            self.ftp.login("anonymous", "isshelf-ui-test@localhost")
        except Exception:
            self.ftp.close()
            raise

    def read(self, name, limit):
        if name not in ("response.json", "request.txt", "frame.png"):
            raise ClientError("Client refused an unsupported remote read.")
        chunks, used = [], 0

        def receive(chunk):
            nonlocal used
            used += len(chunk)
            if used > limit:
                raise ClientError("Bridge file exceeds its transfer bound.")
            chunks.append(chunk)

        try:
            self.ftp.retrbinary(f"RETR {REMOTE_ROOT}/{name}", receive, blocksize=65536)
        except ftplib.error_perm as error:
            if str(error).startswith("550"):
                raise MissingBridgeFile("Bridge file is unavailable; enable the private UI test session.") from error
            raise
        return b"".join(chunks)

    def publish(self, payload):
        if len(payload) > REQUEST_LIMIT:
            raise ClientError("Request exceeds its transfer bound.")
        self.ftp.storbinary(f"STOR {REMOTE_ROOT}/request.tmp", io.BytesIO(payload))
        self.ftp.rename(f"{REMOTE_ROOT}/request.tmp", f"{REMOTE_ROOT}/request.txt")

    def close(self):
        self.ftp.close()


class LocalTransport:
    """Explicit bridge directory for the compiled host fixture; no network."""

    def __init__(self, root):
        self.root = Path(root).resolve(strict=True)
        if not self.root.is_dir():
            raise ClientError("Local bridge root is not a directory.")

    def read(self, name, limit):
        if name not in ("response.json", "request.txt", "frame.png"):
            raise ClientError("Client refused an unsupported local read.")
        path = self.root / name
        if path.is_symlink():
            raise ClientError("Bridge files must not be symbolic links.")
        try:
            with path.open("rb") as source:
                data = source.read(limit + 1)
        except FileNotFoundError as error:
            raise MissingBridgeFile("Bridge file is unavailable; enable the private UI test session.") from error
        if len(data) > limit:
            raise ClientError("Bridge file exceeds its transfer bound.")
        return data

    def publish(self, payload):
        if len(payload) > REQUEST_LIMIT:
            raise ClientError("Request exceeds its transfer bound.")
        temp, final = self.root / "request.tmp", self.root / "request.txt"
        if temp.is_symlink() or final.is_symlink():
            raise ClientError("Bridge files must not be symbolic links.")
        with temp.open("wb") as output:
            output.write(payload)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temp, final)

    def close(self):
        pass


def _write_new(path, data):
    with path.open("xb") as output:
        output.write(data)


def _json_bytes(value):
    return (json.dumps(value, indent=2, sort_keys=True) + "\n").encode("utf-8")


def _ensure_idle(transport, response):
    try:
        old = transport.read("request.txt", REQUEST_LIMIT)
    except MissingBridgeFile:
        return
    match = REQUEST_RE.fullmatch(old)
    if match and match[1].decode("ascii") == response["session"]:
        if int(match[2]) >= response["nextSequence"]:
            raise ClientError("An earlier command has no acknowledgement. Observe its result before sending another.")
    elif response["session"].encode("ascii") in old:
        raise ClientError("The current session has an unrecognised request; inspect it before sending another.")


def execute(transport, action, output_dir, timeout=15, poll_interval=0.1):
    """Publish once, wait for its bound acknowledgement and preserve evidence.

    output_dir is a base folder. A fresh session-sequence-action child is created
    before publication, so a collision never occurs after a new action is sent.
    A failed/uncertain action stays preserved and is never automatically resent.
    """
    if action not in ACTIONS:
        raise ClientError("Unsupported UI test action.")
    if not 0 < timeout <= 15 or not 0 < poll_interval <= 1:
        raise ClientError("Invalid acknowledgement timing bound.")
    response = decode_response(transport.read("response.json", RESPONSE_LIMIT))
    if not response["active"]:
        raise ClientError("UI test bridge is inactive; the owner must enable it in the private app.")
    _ensure_idle(transport, response)
    session, sequence, revision = response["session"], response["nextSequence"], response["revision"]
    payload = f"ISSHELF_UI_TEST_1 {session} {sequence} {revision} {action}\n".encode("ascii")
    if len(payload) > REQUEST_LIMIT:
        raise ClientError("Request exceeds its transfer bound.")
    base = Path(output_dir)
    base.mkdir(parents=True, exist_ok=True)
    folder = base / f"{session}-{sequence:06d}-{action}"
    try:
        folder.mkdir()
    except FileExistsError as error:
        raise ClientError("This command's output already exists; refusing to resend or overwrite it.") from error
    _write_new(folder / "request.txt", payload)
    _write_new(folder / "before.json", _json_bytes(response))
    published = False
    acknowledgement = None
    try:
        # Mark uncertain before the first write: a lost reply cannot prove the
        # server did not accept the upload/rename.
        published = True
        transport.publish(payload)
        deadline = time.monotonic() + timeout
        raw_ack = None
        while time.monotonic() < deadline:
            try:
                raw_ack = transport.read("response.json", RESPONSE_LIMIT)
            except MissingBridgeFile:
                time.sleep(min(poll_interval, max(0, deadline - time.monotonic())))
                continue
            current = decode_response(raw_ack)
            if current["session"] != session:
                raise OutcomeUnknown("Bridge session changed after publication; the action outcome is unknown.")
            if current["sequence"] == sequence:
                if current["action"] != action:
                    raise OutcomeUnknown("Another command used this sequence; the action outcome is unknown.")
                acknowledgement = current
                break
            if current["sequence"] > sequence or not current["active"]:
                raise OutcomeUnknown("Bridge advanced or stopped without this acknowledgement; action outcome is unknown.")
            time.sleep(min(poll_interval, max(0, deadline - time.monotonic())))
        if acknowledgement is None:
            raise OutcomeUnknown("No matching acknowledgement arrived; action outcome is unknown. Do not resend it.")
        _write_new(folder / "response.json", raw_ack)
        frame_path = None
        record = {"response": acknowledgement, "actionOutcome": "acknowledged", "frameSHA256": None}
        if acknowledgement["frameAvailable"]:
            frame = transport.read("frame.png", FRAME_LIMIT)
            validate_frame(frame, acknowledgement)
            after_raw = transport.read("response.json", RESPONSE_LIMIT)
            decode_response(after_raw)
            if after_raw != raw_ack:
                raise ClientError("Bridge response changed while downloading the frame; image binding is unknown.")
            frame_path = folder / "frame.png"
            _write_new(frame_path, frame)
            record["frameSHA256"] = hashlib.sha256(frame).hexdigest()
        _write_new(folder / "result.json", _json_bytes(record))
        return {"response": acknowledgement, "folder": folder.resolve(),
                "frame": frame_path.resolve() if frame_path else None,
                "frameSHA256": record["frameSHA256"]}
    except Exception as error:
        _write_new(folder / "outcome.json", _json_bytes({
            "actionOutcome": "acknowledged" if acknowledgement else "unknown" if published else "not-sent",
            "actionApplied": acknowledgement["actionApplied"] if acknowledgement else None,
            "frameOutcome": "unverified",
            "automaticResend": False,
            "error": str(error),
        }))
        if acknowledgement:
            raise ClientError(f"Action acknowledged (applied: {str(acknowledgement['actionApplied']).lower()}), "
                              f"but frame/evidence verification failed. Do not resend the action. {error}") from error
        if isinstance(error, OutcomeUnknown):
            raise
        if published:
            raise OutcomeUnknown(f"Command was published or may have been published; do not resend. {error}") from error
        raise


def inspect(transport, output_dir):
    """Preserve the latest response/frame without writing to the bridge."""
    raw = transport.read("response.json", RESPONSE_LIMIT)
    response = decode_response(raw)
    base = Path(output_dir)
    base.mkdir(parents=True, exist_ok=True)
    folder = base / f"{response['session']}-{response['sequence']:06d}-inspect"
    try:
        folder.mkdir()
    except FileExistsError as error:
        raise ClientError("This inspection's output already exists; choose a fresh output directory.") from error
    _write_new(folder / "response.json", raw)
    frame_path, frame_sha = None, None
    if response["frameAvailable"]:
        frame = transport.read("frame.png", FRAME_LIMIT)
        validate_frame(frame, response)
        if transport.read("response.json", RESPONSE_LIMIT) != raw:
            raise ClientError("Response changed during inspection; inspect again using fresh local output.")
        frame_path = folder / "frame.png"
        _write_new(frame_path, frame)
        frame_sha = hashlib.sha256(frame).hexdigest()
    _write_new(folder / "result.json", _json_bytes({
        "response": response, "actionOutcome": "read-only-inspection", "frameSHA256": frame_sha,
    }))
    return {"response": response, "folder": folder.resolve(),
            "frame": frame_path.resolve() if frame_path else None, "frameSHA256": frame_sha}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    connection = parser.add_mutually_exclusive_group(required=True)
    connection.add_argument("--host", type=lan_ipv4, help="Literal private-LAN/loopback PS5 IPv4 address")
    connection.add_argument("--local-root", type=Path, help="Host fixture's ui-test folder containing response.json")
    parser.add_argument("--port", type=int, default=2121, help="Existing ftpsrv port (default: 2121)")
    operation = parser.add_mutually_exclusive_group()
    operation.add_argument("--action", choices=ACTIONS, default="snapshot", help="One discrete action (default: snapshot)")
    operation.add_argument("--inspect", action="store_true", help="Read the latest response/frame; never send an action")
    parser.add_argument("--output-dir", type=Path, default=Path(".cache/ui-test-client"),
                        help="Base for create-only request/response/frame folders")
    args = parser.parse_args(argv)
    transport = None
    try:
        transport = LocalTransport(args.local_root) if args.local_root else FtpTransport(args.host, args.port)
        result = inspect(transport, args.output_dir) if args.inspect else execute(transport, args.action, args.output_dir)
        response = result["response"]
        print(f"UI test: {response['result']}; action applied: {str(response['actionApplied']).lower()}; "
              f"page: {response['page']}; selected system: {response['selectedSystem']}")
        if result["frame"]:
            print(f"Frame: {result['frame']}")
            print(f"SHA-256: {result['frameSHA256']}")
        else:
            print("No new frame was available for this acknowledgement.")
        print(f"Evidence: {result['folder']}")
        return 0 if (response["result"] in ("ok", "ready", "stopped")
                     and response["settingsSaved"] and response["frameAvailable"]) else 2
    except (ClientError, OSError, ftplib.Error, argparse.ArgumentTypeError) as error:
        print(f"UI test stopped: {error}", file=sys.stderr)
        return 2
    finally:
        if transport:
            transport.close()


if __name__ == "__main__":
    raise SystemExit(main())
