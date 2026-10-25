# SPDX-License-Identifier: GPL-3.0-only
"""Independent structural checks; a PASS here is not a PS5 hardware test."""
import json
import struct
from ps5_common import CACHE, ROOT, sha256


def require(condition, message):
    if not condition:
        raise ValueError(message)


def source_fingerprint():
    inputs = []
    for directory in ("native/src", "native/include", "native/ps5", "native/third_party/stb",
                      "native/third_party/ps5-runtime"):
        inputs.extend(p for p in (ROOT / directory).rglob("*") if p.is_file())
    inputs += [ROOT / "tools" / name for name in ("build_ps5.py", "bootstrap_ps5_toolchain.py", "fetch_ps5_deps.py",
                                                   "ps5_common.py", "verify_ps5_build.py")]
    return {p.relative_to(ROOT).as_posix(): sha256(p) for p in sorted(inputs)}


def verify_recorded_hashes(folder):
    recorded = json.loads((folder / "structural-check.json").read_text())
    for filename, key in (("eboot.elf", "elf"), ("eboot.bin", "self")):
        file = folder / filename
        expected = recorded[key]
        require(file.stat().st_size == expected["bytes"] and sha256(file) == expected["sha256"],
                f"{filename} changed after its build/container validation; rebuild first")


def verify(elf_path, self_path):
    raw = elf_path.read_bytes()
    require(raw[:9] == b"\x7fELF\x02\x01\x01\x09\x02", "Expected PS5/FreeBSD ELF64 little-endian ABI")
    kind, machine, version, entry, phoff = struct.unpack_from("<HHIQQ", raw, 16)
    require((kind, machine, version) == (0xfe10, 62, 1), "Expected native PS5 x86-64 executable")
    ehsize, phsize, phcount = struct.unpack_from("<HHH", raw, 52)
    require(ehsize == 64 and phsize == 56 and 1 <= phcount <= 64, "Invalid ELF header geometry")
    require(phoff + phsize * phcount <= len(raw), "Program headers exceed file")
    headers = [struct.unpack_from("<IIQQQQQQ", raw, phoff + i * phsize) for i in range(phcount)]
    loads = [p for p in headers if p[0] == 1]
    require(loads and any((p[1] & 1) and p[3] <= entry < p[3] + p[6] for p in loads), "Entry is outside executable memory")
    for _, flags, offset, va, _, filesz, memsz, align in loads:
        require(filesz <= memsz and offset + filesz <= len(raw), "Invalid LOAD range")
        require(align == 0x4000 and offset % align == va % align, "Invalid PS5 page alignment")
        require(flags & 3 != 3, "A LOAD segment is both writable and executable")
    def virtual(address, length):
        for p in loads:
            if p[3] <= address and address + length <= p[3] + p[5]:
                return p[2] + address - p[3]
        raise ValueError("Dynamic address is outside file-backed LOAD memory")
    dynamic = [p for p in headers if p[0] == 2]
    require(len(dynamic) == 1, "Expected one dynamic segment")
    p = dynamic[0]
    require(p[2] + p[5] <= len(raw) and p[5] % 16 == 0, "Invalid dynamic segment range")
    tags = [struct.unpack_from("<qQ", raw, p[2] + n) for n in range(0, p[5], 16)]
    require(tags and tags[-1] == (0, 0), "Dynamic segment lacks terminator")
    table = dict(tags)
    start = virtual(table[5], table[10])
    strings = raw[start:start + table[10]]
    needed = []
    for tag, offset in tags:
        if tag == 1:
            require(offset < len(strings), "Module name is outside string table")
            end = strings.find(b"\0", offset)
            require(end > offset, "Unterminated module name")
            needed.append(strings[offset:end].decode("ascii"))
    allowed = {"libSceAudioOut.prx", "libSceLibcInternal.prx", "libScePad.prx", "libSceSystemService.prx",
               "libSceUserService.prx", "libSceVideoOut.prx", "libkernel.prx"}
    require(set(needed) == allowed, f"Unexpected system dependency set: {needed}")
    signed = self_path.read_bytes()
    require(signed[:4] == bytes.fromhex("4f153d1d"), "Expected the native plaintext homebrew FSELF format")
    require(b"\x7fELF\x02\x01\x01\x09\x02" in signed[:4096], "SELF lacks its native ELF header")
    return {"structuralCheck": "passed", "hardwareTest": "not run", "firmware12_20": "unverified",
            "elf": {"bytes": len(raw), "sha256": sha256(elf_path), "programHeaders": phcount,
                    "loadSegments": len(loads), "neededModules": needed},
            "self": {"bytes": len(signed), "sha256": sha256(self_path)}}


if __name__ == "__main__":
    folder = CACHE / "build"
    identity = json.loads((folder / "build-source.json").read_text())
    require(identity == source_fingerprint(), "Sources changed since this executable was built; rebuild first")
    verify_recorded_hashes(folder)
    result = verify(folder / "eboot.elf", folder / "eboot.bin")
    (folder / "structural-check.json").write_text(json.dumps(result, indent=2) + "\n")
    print("PASS: independent PS5 ELF identity, segment bounds/alignment, executable entry, dependency set and SELF wrapper.")
    print("Firmware 12.20 and physical controller/display remain unverified.")
