# SPDX-License-Identifier: GPL-3.0-only
"""Reject damaged/unexpected executables using copies of a real cross-build."""
import struct
import shutil
import uuid
from ps5_common import CACHE
from verify_ps5_build import verify, verify_recorded_hashes, public_nid

build = CACHE / "build"
elf = build / "eboot.elf"
self_file = build / "eboot.bin"
verify(elf, self_file)
folder = CACHE / "verifier-tests" / str(uuid.uuid4())
folder.mkdir(parents=True)
original = elf.read_bytes()


def reject(name, mutation):
    damaged = bytearray(original)
    mutation(damaged)
    file = folder / (name + ".elf")
    file.write_bytes(damaged)
    try:
        verify(file, self_file)
    except (ValueError, struct.error):
        return
    raise RuntimeError(f"Verifier accepted a damaged fixture: {name}")


reject("magic", lambda b: b.__setitem__(0, 0))
reject("architecture", lambda b: struct.pack_into("<H", b, 18, 3))
reject("entry-outside-code", lambda b: struct.pack_into("<Q", b, 24, 0xffffffffffffffff))
phoff = struct.unpack_from("<Q", original, 32)[0]
reject("writable-executable", lambda b: struct.pack_into("<I", b, phoff + 4, 7))
reject("load-exceeds-file", lambda b: struct.pack_into("<Q", b, phoff + 32, len(original) * 2))
module = original.index(b"libScePad.prx")
reject("foreign-module", lambda b: b.__setitem__(slice(module, module + 13), b"libBadPad.prx"))
headers = [struct.unpack_from("<IIQQQQQQ", original, phoff + i * 56)
           for i in range(struct.unpack_from("<H", original, 56)[0])]
dynamic = next(p for p in headers if p[0] == 2)
tags = dict(struct.unpack_from("<qQ", original, dynamic[2] + offset)
            for offset in range(0, dynamic[5], 16))
def file_offset(address):
    p = next(p for p in headers if p[0] == 1 and p[3] <= address < p[3] + p[5])
    return p[2] + address - p[3]
strings = file_offset(tags[5])
shell_name = original.index(b"JoBqSQt1yyA#", strings, strings + tags[10])
symbols = file_offset(tags[6])
shell_symbol = next(symbols + offset for offset in range(0, tags[0x6100003f], 24)
                    if struct.unpack_from("<I", original, symbols + offset)[0] == shell_name - strings)
reject("required-shell-import-weak", lambda b: b.__setitem__(shell_symbol + 4, 0x22))
reject("required-shell-import-missing", lambda b: b.__setitem__(shell_name, ord("X")))
shell_end = original.index(b"\0", shell_name)
reject("required-shell-import-wrong-provider", lambda b: b.__setitem__(shell_end - 1, ord("H")))
reject("required-shell-import-wrong-library", lambda b: b.__setitem__(shell_end - 3, ord("G")))
visibility_name = original.index((public_nid("sceSystemServiceGetStatus") + "#").encode(), strings, strings + tags[10])
visibility_symbol = next(symbols + offset for offset in range(0, tags[0x6100003f], 24)
                         if struct.unpack_from("<I", original, symbols + offset)[0] == visibility_name - strings)
reject("required-visibility-import-weak", lambda b: b.__setitem__(visibility_symbol + 4, 0x22))
reject("required-visibility-import-missing", lambda b: b.__setitem__(visibility_name, ord("X")))
bad_self = folder / "bad-wrapper.bin"
bad_self.write_bytes(b"BAD!" + self_file.read_bytes()[4:])
try:
    verify(elf, bad_self)
except ValueError:
    pass
else:
    raise RuntimeError("Verifier accepted a foreign SELF wrapper")
recorded = folder / "artifact-hash"
recorded.mkdir()
for name in ("eboot.elf", "eboot.bin", "structural-check.json"):
    shutil.copyfile(build / name, recorded / name)
verify_recorded_hashes(recorded)
damaged = bytearray(self_file.read_bytes())
damaged[-1] ^= 1
(recorded / "eboot.bin").write_bytes(damaged)
try:
    verify_recorded_hashes(recorded)
except ValueError:
    pass
else:
    raise RuntimeError("Verifier accepted a SELF changed after container validation")
print("PASS: PS5 verifier rejects fourteen damaged/foreign fixtures, including weak/missing/misbound lifecycle and visibility imports, without modifying the original executable.")
