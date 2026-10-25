# SPDX-License-Identifier: GPL-3.0-only
"""Build an isolated ELF-loader payload. Never sends or runs it on a PS5."""
import json
import struct
from bootstrap_ps5_toolchain import environment, prepare as prepare_compiler
from fetch_ps5_deps import prepare as prepare_dependencies
from ps5_common import CACHE, ROOT, run, sha256
from verify_ps5_build import require


def main():
    prefix = prepare_compiler()
    sdk, _, _ = prepare_dependencies()
    env = environment(prefix)
    output = CACHE / "launch-helper"
    output.mkdir(parents=True, exist_ok=True)
    source = ROOT / "native/ps5/helper/shelf_launch.cpp"
    protocol = ROOT / "native/include/shelf/launch_protocol.hpp"
    inputs = {p.relative_to(ROOT).as_posix(): sha256(p) for p in (source, protocol)}
    obj, elf = output / "shelf-launch.o", output / "shelf-launch.elf"
    run([prefix / "usr/bin/clang++-18", "-target", "x86_64-sie-ps5", "-isysroot", sdk,
         "-isystem", sdk / "target/include/c++/v1", "-isystem", sdk / "target/include",
         "-fvisibility-nodllstorageclass=default", "-fno-stack-protector", "-fno-plt", "-femulated-tls",
         "-std=c++17", "-O2", "-fno-exceptions", "-fno-rtti", "-DNDEBUG", "-Wall", "-Wextra", "-Werror",
         "-I", ROOT / "native/include", "-c", source, "-o", obj], env=env)
    # SDK v0.42's ordinary ELF-payload contract, distinct from the native app's
    # converter/container. Do not apply the app's SELF verifier to this payload.
    run([prefix / "usr/bin/ld.lld-18", "-m", "elf_x86_64", "-pie", "-T", sdk / "ldscripts/elf_x86_64.x",
         "--eh-frame-hdr", "-z", "max-page-size=0x4000", "-mllvm", "-emulated-tls", "--hash-style=gnu",
         "-o", elf, obj, sdk / "target/lib/crt1.o", "-L", sdk / "target/lib", "-lc", "-lkernel_web",
         "-lSceLibcInternal", "-lSceNet", "-lSceSystemService", "-lSceUserService"], env=env)
    raw = elf.read_bytes()
    # The pinned SDK's normal payload CRT uses System V ABI 0. Native app
    # conversion sets FreeBSD/Sony ABI 9 instead; that is a separate format.
    crt = (sdk / "target/lib/crt1.o").read_bytes()
    require(raw[:9] == crt[:9] == b"\x7fELF\x02\x01\x01\x00\x00", "Wrong SDK-payload ELF class/ABI")
    kind, machine = struct.unpack_from("<HH", raw, 16)
    require((kind, machine) == (3, 62), "Expected an x86-64 dynamic payload")
    entry, phoff = struct.unpack_from("<QQ", raw, 24)
    phsize, phcount = struct.unpack_from("<HH", raw, 54)
    require(phsize == 56 and 1 <= phcount <= 16 and phoff + phsize * phcount <= len(raw), "Invalid payload headers")
    segments = [struct.unpack_from("<IIQQQQQQ", raw, phoff + i * phsize) for i in range(phcount)]
    loads = [p for p in segments if p[0] == 1]
    require(any(p[1] & 1 and p[3] <= entry < p[3] + p[6] for p in loads), "Payload entry is outside code")
    for p in loads:
        require(p[5] <= p[6] and p[2] + p[5] <= len(raw) and p[7] == 0x4000 and p[2] % p[7] == p[3] % p[7],
                "Invalid SDK payload segment")
    require(inputs == {p.relative_to(ROOT).as_posix(): sha256(p) for p in (source, protocol)}, "Build input drift")
    result = {"elf": {"bytes": len(raw), "sha256": sha256(elf)}, "inputs": inputs,
              "hardwareTest": "not run", "transport": "not connected", "deployed": False,
              "abi": "System V ABI 0, matching pinned SDK v0.42 crt1.o; not a native app/SELF",
              "policy": "cooperative Shelf closure; one launch attempt; no process kill or emulator-file writes"}
    (output / "build.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
