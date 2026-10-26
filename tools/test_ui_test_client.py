# SPDX-License-Identifier: GPL-3.0-only
"""Loopback/local-only tests for the private isShelf UI test client."""

import argparse
import json
import os
import socket
import socketserver
import tempfile
import threading
import time
import unittest
from pathlib import Path

import ui_test_client as client


SESSION = "0123456789abcdef0123456789abcdef"
PNG = (b"\x89PNG\r\n\x1a\n" + (13).to_bytes(4, "big") + b"IHDR"
       + (1920).to_bytes(4, "big") + (1080).to_bytes(4, "big") + b"fixture-frame")


def response(sequence=0, action="snapshot", frame=PNG, **changes):
    result = {
        "protocol": 1, "active": True, "session": SESSION,
        "sequence": sequence, "nextSequence": sequence + 1, "revision": sequence + 1,
        "action": action, "result": "ready" if sequence == 0 else "ok",
        "actionApplied": action not in ("snapshot", "stop"), "settingsSaved": True,
        "frameAvailable": True, "frameBytes": len(frame), "frameFNV64": client.fnv1a64(frame),
        "page": 0, "selectedSystem": "ps1", "focus": sequence, "gameFocus": 0, "tab": 0,
    }
    result.update(changes)
    return result


def encoded(value):
    return json.dumps(value, separators=(",", ":")).encode("utf-8")


class MemoryTransport:
    def __init__(self, initial=None, on_publish=None):
        self.files = {"response.json": encoded(initial or response()), "frame.png": PNG}
        self.published = []
        self.on_publish = on_publish

    def read(self, name, limit):
        if name not in self.files:
            raise client.MissingBridgeFile("Missing fixture file")
        value = self.files[name]
        if len(value) > limit:
            raise client.ClientError("Fixture file exceeds transfer bound")
        return value

    def publish(self, payload):
        self.published.append(payload)
        self.files["request.txt"] = payload
        match = client.REQUEST_RE.fullmatch(payload)
        sequence, action = int(match[2]), match[4].decode("ascii")
        self.files["response.json"] = encoded(response(sequence, action))
        if self.on_publish:
            self.on_publish(self, sequence, action)


class FtpFixture:
    """Small FTP server; permits only the bridge's fixed files and operations."""

    def __init__(self, acknowledge=True):
        self.files = {client.BRIDGE_FILES["response.json"]: encoded(response()),
                      client.BRIDGE_FILES["frame.png"]: PNG}
        self.commands = []
        self.errors = []
        self.acknowledge = acknowledge
        fixture = self

        class Handler(socketserver.StreamRequestHandler):
            def reply(self, text):
                self.wfile.write((text + "\r\n").encode("ascii"))
                self.wfile.flush()

            def handle(self):
                self.request.settimeout(3)
                self.passive = None
                self.rename_from = None
                self.reply("220 Local UI test fixture")
                try:
                    while True:
                        raw = self.rfile.readline(512)
                        if not raw:
                            return
                        text = raw.rstrip(b"\r\n").decode("ascii")
                        verb, _, arg = text.partition(" ")
                        fixture.commands.append((verb, arg))
                        if verb == "USER":
                            self.reply("331 Anonymous password")
                        elif verb == "PASS":
                            self.reply("230 Logged in")
                        elif verb == "TYPE":
                            if arg != "I":
                                raise AssertionError("Only binary transfers are permitted")
                            self.reply("200 Binary")
                        elif verb == "PASV":
                            self.passive = socket.socket()
                            self.passive.bind(("127.0.0.1", 0))
                            self.passive.listen(1)
                            self.passive.settimeout(3)
                            port = self.passive.getsockname()[1]
                            # The client must ignore this untrusted advertised IP.
                            self.reply(f"227 Passive (203,0,113,99,{port >> 8},{port & 255})")
                        elif verb in ("RETR", "STOR"):
                            prefix = client.REMOTE_ROOT + "/"
                            if not arg.startswith(prefix):
                                raise AssertionError("Request left the owned bridge directory")
                            name = arg[len(prefix):]
                            if verb == "RETR" and name not in client.BRIDGE_FILES.values():
                                raise AssertionError("Unexpected remote read")
                            if verb == "STOR" and name != client.REQUEST_TEMP:
                                raise AssertionError("Unexpected remote write")
                            if verb == "RETR" and name not in fixture.files:
                                self.reply("550 File unavailable")
                                self.passive.close()
                                self.passive = None
                                continue
                            self.reply("150 Data follows")
                            peer, _ = self.passive.accept()
                            with peer:
                                peer.settimeout(3)
                                if verb == "RETR":
                                    peer.sendall(fixture.files[name])
                                else:
                                    parts = []
                                    while chunk := peer.recv(4096):
                                        parts.append(chunk)
                                    fixture.files[name] = b"".join(parts)
                            self.passive.close()
                            self.passive = None
                            self.reply("226 Complete")
                        elif verb == "RNFR":
                            if arg != client.REMOTE_ROOT + "/" + client.REQUEST_TEMP:
                                raise AssertionError("Unexpected rename source")
                            self.rename_from = client.REQUEST_TEMP
                            self.reply("350 Ready for destination")
                        elif verb == "RNTO":
                            if self.rename_from != client.REQUEST_TEMP or arg != client.REMOTE_ROOT + "/" + client.BRIDGE_FILES["request.txt"]:
                                raise AssertionError("Unexpected rename destination")
                            fixture.files[client.BRIDGE_FILES["request.txt"]] = fixture.files.pop(client.REQUEST_TEMP)
                            if fixture.acknowledge:
                                match = client.REQUEST_RE.fullmatch(fixture.files[client.BRIDGE_FILES["request.txt"]])
                                if not match:
                                    raise AssertionError("Client published malformed protocol")
                                fixture.files[client.BRIDGE_FILES["response.json"]] = encoded(response(int(match[2]), match[4].decode("ascii")))
                            self.reply("250 Renamed")
                        else:
                            raise AssertionError("Unexpected FTP command: " + verb)
                except Exception as error:
                    fixture.errors.append(error)
                finally:
                    if self.passive:
                        self.passive.close()

        class Server(socketserver.ThreadingTCPServer):
            allow_reuse_address = True
            daemon_threads = True

        self.server = Server(("127.0.0.1", 0), Handler)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)

    def __enter__(self):
        self.thread.start()
        return self

    def __exit__(self, *_):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=3)
        if self.errors:
            raise self.errors[0]


class ClientTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def test_address_admission(self):
        for address in ("127.0.0.1", "10.5.4.3", "172.16.1.2", "192.168.2.20"):
            self.assertEqual(client.lan_ipv4(address), address)
        for address in ("example.com", "8.8.8.8", "0.0.0.0", "::1", "169.254.1.2", "172.32.0.1"):
            with self.subTest(address=address), self.assertRaises(argparse.ArgumentTypeError):
                client.lan_ipv4(address)

    def test_frame_checksum_vectors(self):
        self.assertEqual(client.fnv1a64(b""), "cbf29ce484222325")
        self.assertEqual(client.fnv1a64(b"hello"), "a430d84680aabd0b")

    def test_strict_response_admission(self):
        self.assertEqual(client.decode_response(encoded(response())), response())
        cases = [b"", b"{}\0", b"[1]", b"{" + b"x" * 4096,
                 b'{"protocol":1,"protocol":1}', b"invalid json"]
        for field, value in (("protocol", True), ("active", 1), ("session", "BAD"),
                             ("sequence", -1), ("revision", 1 << 64), ("nextSequence", 2),
                             ("action", "launch"), ("frameFNV64", "none"), ("frameBytes", client.FRAME_LIMIT + 1),
                             ("page", 5), ("selectedSystem", "../../game"), ("actionApplied", 1)):
            changed = response()
            changed[field] = value
            cases.append(encoded(changed))
        for raw in cases:
            with self.subTest(raw=raw[:80]), self.assertRaises(client.ClientError):
                client.decode_response(raw)

    def test_single_action_and_create_only_outputs(self):
        transport = MemoryTransport()
        result = client.execute(transport, "right", self.root)
        self.assertEqual(len(transport.published), 1)
        self.assertEqual(transport.published[0], f"ISSHELF_UI_TEST_1 {SESSION} 1 1 right\n".encode())
        self.assertEqual(result["frame"].read_bytes(), PNG)
        saved = json.loads((result["folder"] / "result.json").read_text())
        self.assertEqual(saved["frameSHA256"], result["frameSHA256"])
        transport.files["response.json"] = encoded(response())
        transport.files.pop("request.txt")
        with self.assertRaisesRegex(client.ClientError, "output already exists"):
            client.execute(transport, "right", self.root)
        self.assertEqual(len(transport.published), 1)

    def test_inactive_missing_and_in_flight_do_not_publish(self):
        for mode in ("inactive", "missing", "pending"):
            transport = MemoryTransport()
            if mode == "inactive":
                transport.files["response.json"] = encoded(response(active=False))
            elif mode == "missing":
                transport.files.pop("response.json")
            else:
                transport.files["request.txt"] = f"ISSHELF_UI_TEST_1 {SESSION} 1 1 right\n".encode()
            with self.subTest(mode=mode), self.assertRaises(client.ClientError):
                client.execute(transport, "snapshot", self.root / mode)
            self.assertEqual(transport.published, [])

    def test_session_or_ack_action_mismatch_is_unknown_without_resend(self):
        for field, value in (("session", "f" * 32), ("action", "left"), ("sequence", 2)):
            def mismatch(transport, sequence, action, field=field, value=value):
                changed = response(sequence, action)
                changed[field] = value
                if field == "sequence":
                    changed["nextSequence"] = value + 1
                transport.files["response.json"] = encoded(changed)
            transport = MemoryTransport(on_publish=mismatch)
            with self.subTest(field=field), self.assertRaises(client.OutcomeUnknown):
                client.execute(transport, "right", self.root / field, timeout=0.05, poll_interval=0.01)
            self.assertEqual(len(transport.published), 1)

    def test_frame_failure_keeps_acknowledged_action_separate(self):
        for mode in ("length", "checksum", "binding"):
            class CorruptTransport(MemoryTransport):
                def read(self, name, limit):
                    result = super().read(name, limit)
                    if name == "frame.png" and self.published:
                        if mode == "length":
                            result += b"extra"
                        elif mode == "checksum":
                            result = result[:-1] + bytes([result[-1] ^ 1])
                        else:
                            self.files["response.json"] = encoded(response(2, "left"))
                    return result
            transport = CorruptTransport()
            with self.subTest(mode=mode), self.assertRaisesRegex(client.ClientError, "Action acknowledged"):
                client.execute(transport, "right", self.root / mode)
            self.assertEqual(len(transport.published), 1)
            folder = next((self.root / mode).iterdir())
            failed = json.loads((folder / "outcome.json").read_text())
            self.assertEqual(failed["actionOutcome"], "acknowledged")
            self.assertTrue(failed["actionApplied"])
            self.assertFalse((folder / "frame.png").exists())

    def test_capture_unavailable_never_reuses_old_frame(self):
        def no_frame(transport, sequence, action):
            transport.files["response.json"] = encoded(response(
                sequence, action, result="capture-failed", frameAvailable=False, frameBytes=0, frameFNV64="0" * 16))
        transport = MemoryTransport(on_publish=no_frame)
        result = client.execute(transport, "right", self.root)
        self.assertIsNone(result["frame"])
        self.assertEqual(result["response"]["result"], "capture-failed")

    def test_rejection_and_stop_are_acknowledged_without_new_action(self):
        for mode in ("stale", "stopped"):
            def rejected(transport, sequence, action):
                transport.files["response.json"] = encoded(response(
                    sequence, action, result=mode, active=mode != "stopped", actionApplied=False))
            transport = MemoryTransport(on_publish=rejected)
            action = "stop" if mode == "stopped" else "right"
            result = client.execute(transport, action, self.root / mode)
            self.assertEqual(len(transport.published), 1)
            self.assertEqual(result["response"]["result"], mode)
            self.assertFalse(result["response"]["actionApplied"])

    def test_timeout_and_upload_error_never_resend(self):
        for mode in ("timeout", "lost-publish-reply"):
            class UncertainTransport(MemoryTransport):
                def publish(self, payload):
                    self.published.append(payload)
                    if mode == "lost-publish-reply":
                        raise OSError("Simulated connection loss")
            transport = UncertainTransport()
            with self.subTest(mode=mode), self.assertRaises(client.OutcomeUnknown):
                client.execute(transport, "right", self.root / mode, timeout=0.03, poll_interval=0.01)
            self.assertEqual(len(transport.published), 1)
            folder = next((self.root / mode).iterdir())
            record = json.loads((folder / "outcome.json").read_text())
            self.assertEqual(record["actionOutcome"], "unknown")
            self.assertFalse(record["automaticResend"])

    def test_inspect_is_read_only_even_for_inactive_session(self):
        transport = MemoryTransport(response(active=False))
        result = client.inspect(transport, self.root)
        self.assertEqual(transport.published, [])
        self.assertEqual(result["frame"].read_bytes(), PNG)

    def test_local_filesystem_exchange(self):
        bridge = self.root / "data"
        bridge.mkdir()
        (bridge / client.BRIDGE_FILES["response.json"]).write_bytes(encoded(response()))
        (bridge / client.BRIDGE_FILES["frame.png"]).write_bytes(PNG)
        (bridge / "settings.ini").write_bytes(b"OWNER PREFERENCES")
        (bridge / "response.json").write_bytes(b"UNRELATED FILE")
        errors = []
        def serve():
            try:
                deadline = time.monotonic() + 2
                request = bridge / client.BRIDGE_FILES["request.txt"]
                while not request.exists() and time.monotonic() < deadline:
                    time.sleep(0.005)
                match = client.REQUEST_RE.fullmatch(request.read_bytes())
                self.assertIsNotNone(match)
                raw = encoded(response(int(match[2]), match[4].decode("ascii")))
                (bridge / "ui-test-response.tmp").write_bytes(raw)
                (bridge / "ui-test-response.tmp").replace(bridge / client.BRIDGE_FILES["response.json"])
            except Exception as error:
                errors.append(error)
        worker = threading.Thread(target=serve)
        worker.start()
        try:
            result = client.execute(client.LocalTransport(bridge), "settings", self.root / "out")
            self.assertEqual(result["response"]["sequence"], 1)
            self.assertFalse((bridge / client.REQUEST_TEMP).exists())
            self.assertEqual((bridge / "settings.ini").read_bytes(), b"OWNER PREFERENCES")
            self.assertEqual((bridge / "response.json").read_bytes(), b"UNRELATED FILE")
            self.assertFalse((bridge / "ui-test").exists())
        finally:
            worker.join(timeout=3)
        if errors:
            raise errors[0]

    @unittest.skipUnless(os.name == "nt", "Windows sharing semantics")
    def test_local_read_handle_allows_publisher_replacement(self):
        published = self.root / "ui-test-response.json"
        replacement = self.root / "ui-test-response.new.json"
        published.write_bytes(b"old complete response")
        replacement.write_bytes(b"new complete response")
        with client._open_shared_read(published) as reader:
            # Match native UiTestBridge::replace: remove the old name, then
            # rename the fully written replacement. Windows MoveFileEx used by
            # os.replace has stricter open-target behavior on this filesystem.
            os.remove(published)
            os.rename(replacement, published)
            self.assertEqual(reader.read(), b"old complete response")
            self.assertEqual(published.read_bytes(), b"new complete response")

    def test_ftp_fixed_files_and_single_publish(self):
        with FtpFixture() as fixture:
            transport = client.FtpTransport("127.0.0.1", fixture.server.server_address[1])
            try:
                result = client.execute(transport, "right", self.root)
                self.assertEqual(result["response"]["sequence"], 1)
                self.assertEqual(result["frame"].read_bytes(), PNG)
            finally:
                transport.close()
        for verb in ("STOR", "RNFR", "RNTO"):
            self.assertEqual(sum(command[0] == verb for command in fixture.commands), 1)
        self.assertEqual([arg for verb, arg in fixture.commands if verb == "STOR"],
                         [client.REMOTE_ROOT + "/ui-test-request.tmp"])
        self.assertEqual([arg for verb, arg in fixture.commands if verb == "RNFR"],
                         [client.REMOTE_ROOT + "/ui-test-request.tmp"])
        self.assertEqual([arg for verb, arg in fixture.commands if verb == "RNTO"],
                         [client.REMOTE_ROOT + "/ui-test-request.txt"])
        self.assertTrue(all(arg in {client.REMOTE_ROOT + "/ui-test-" + suffix for suffix in
                                   ("response.json", "request.txt", "frame.png")}
                            for verb, arg in fixture.commands if verb == "RETR"))
        self.assertTrue(all(verb in ("USER", "PASS", "TYPE", "PASV", "RETR", "STOR", "RNFR", "RNTO")
                            for verb, _ in fixture.commands))

    def test_ftp_timeout_polls_reads_without_resending(self):
        with FtpFixture(acknowledge=False) as fixture:
            transport = client.FtpTransport("127.0.0.1", fixture.server.server_address[1])
            try:
                with self.assertRaises(client.OutcomeUnknown):
                    client.execute(transport, "right", self.root, timeout=0.1, poll_interval=0.01)
            finally:
                transport.close()
        self.assertEqual(sum(verb == "STOR" for verb, _ in fixture.commands), 1)
        self.assertEqual(sum(verb == "RNTO" for verb, _ in fixture.commands), 1)


if __name__ == "__main__":
    unittest.main()
