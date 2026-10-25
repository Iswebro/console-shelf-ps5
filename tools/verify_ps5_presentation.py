# SPDX-License-Identifier: GPL-3.0-only
"""Verify Shelf's owned PS5 selection/launch assets without console access."""
import hashlib
import json
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def verify_dds(raw):
    require(len(raw) == 8294548 and raw[:4] == b"DDS ", "Incomplete 4K BC7 DDS")
    u32 = lambda offset: struct.unpack_from("<I", raw, offset)[0]
    require((u32(4), u32(12), u32(16), u32(24), u32(28), u32(76), raw[84:88],
             u32(128), u32(132), u32(136), u32(140)) ==
            (124, 2160, 3840, 1, 1, 32, b"DX10", 98, 3, 0, 1), "Unexpected PS5 DDS geometry/format")
    require(u32(8) & 0x1007 == 0x1007 and u32(108) & 0x1000 and u32(112) == 0,
            "Invalid 2D DDS capability/header flags")


def verify_png(raw, expected_size=(1920, 1080)):
    require(raw[:8] == b"\x89PNG\r\n\x1a\n", "Invalid source PNG")
    offset = 8
    first, ended = True, False
    while offset < len(raw):
        require(offset + 12 <= len(raw), "Truncated PNG chunk")
        size = struct.unpack_from(">I", raw, offset)[0]
        end = offset + size + 12
        require(end <= len(raw), "PNG chunk exceeds source")
        kind = raw[offset + 4:offset + 8]
        require(zlib.crc32(raw[offset + 4:end - 4]) == struct.unpack_from(">I", raw, end - 4)[0], "PNG CRC mismatch")
        if first:
            require(kind == b"IHDR" and size == 13 and struct.unpack_from(">II", raw, offset + 8) == expected_size,
                    "Unexpected Shelf artwork source dimensions")
            first = False
        if kind == b"IEND":
            require(size == 0 and end == len(raw), "Invalid PNG termination")
            ended = True
        offset = end
    require(ended, "Source PNG lacks IEND")


def verify(root=ROOT):
    manifest = json.loads((root / "native/ps5/presentation-assets.json").read_text())
    source = manifest.get("sourceImage", {"width": 1920, "height": 1080})
    source_size = (source["width"], source["height"])
    require(source_size in ((1920, 1080), (3840, 2160)), "Unsupported Shelf artwork source geometry")
    expected = {"assets/launch-background-source.png", "native/ps5/sce_sys/pic0.dds", "native/ps5/sce_sys/pic1.dds"}
    files = manifest["files"]
    require(len(files) == 3 and {item["path"] for item in files} == expected, "Unexpected presentation file set")
    for item in files:
        raw = (root / item["path"]).read_bytes()
        require(len(raw) == item["bytes"] and hashlib.sha256(raw).hexdigest() == item["sha256"],
                "Presentation asset changed: " + item["path"])
        verify_dds(raw) if item["path"].endswith(".dds") else verify_png(raw, source_size)
    icon = manifest.get("icon")
    if icon:
        require(icon.get("source") == "assets/branding/isshelf-home-icon.png" and
                (icon.get("width"), icon.get("height")) == (1024, 1024), "Unexpected home icon source")
        raw = (root / icon["source"]).read_bytes()
        require(len(raw) == icon["bytes"] and hashlib.sha256(raw).hexdigest() == icon["sha256"],
                "Home icon source changed")
        verify_png(raw, (1024, 1024))
        for key, expected_path in (("vectorSource", "assets/branding/isshelf-home-icon.svg"),
                                   ("markSource", "assets/branding/shelf-gaming-mark.svg")):
            record = icon[key]
            require(record.get("path") == expected_path, "Unexpected vector icon master")
            raw = (root / expected_path).read_bytes()
            require(len(raw) == record["bytes"] and hashlib.sha256(raw).hexdigest() == record["sha256"],
                    "Vector icon master changed")
    return manifest


if __name__ == "__main__":
    verify()
    print("PASS: source PNG CRCs, both complete 4K BC7 DX10 DDS files, exact sizes and SHA-256 pins.")
