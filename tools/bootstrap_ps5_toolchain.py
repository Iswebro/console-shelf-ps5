# SPDX-License-Identifier: GPL-3.0-only
"""An optional Ubuntu/WSL compiler prefix, extracted locally without apt or sudo.

--resolve-lock is a maintainer action: verify Ubuntu's signed Noble metadata and
record exact package hashes. Normal runs use only that committed lock file.
No package maintainer scripts are executed and the host libc is never replaced.
"""
import argparse
import json
import lzma
import os
from pathlib import Path
import platform
from ps5_common import CACHE, ROOT, fetch, run, sha256

LOCK = ROOT / "native/ps5/ubuntu-toolchain.lock.json"
BASE = "https://archive.ubuntu.com/ubuntu/"
KEYRING = Path("/usr/share/keyrings/ubuntu-archive-keyring.gpg")
# Runtime and development files actually needed by Clang, the host packager,
# and zlib. Runtime libc/libgcc/libstdc++ are supplied by the compatible host.
PACKAGES = ["clang-18", "lld-18", "llvm-18-linker-tools", "libclang-cpp18",
            "libclang-common-18-dev", "libclang1-18", "libllvm18", "libedit2",
            "libffi8", "libxml2", "libicu74", "libz3-4", "libc6-dev", "linux-libc-dev",
            "libgcc-13-dev", "libstdc++-13-dev", "gcc-13-base", "make",
            "zlib1g-dev"]


def resolve_lock():
    metadata = CACHE / "ubuntu-metadata"
    release = fetch(BASE + "dists/noble/InRelease", metadata / "noble-InRelease")
    cleartext = metadata / "noble-Release"
    verified = run(["gpgv", "--keyring", KEYRING, "--output", "-", release], capture_output=True)
    cleartext.write_bytes(verified.stdout)
    print("Verified Ubuntu InRelease signature with the system Ubuntu archive keyring", flush=True)
    checksums = {}
    active = False
    for line in cleartext.read_text().splitlines():
        if line == "SHA256:":
            active = True
        elif line and not line.startswith(" "):
            active = False
        elif active:
            digest, size, name = line.split()
            checksums[name] = (digest, int(size))
    selected = {}
    indices = []
    for component in ("main", "universe"):
        name = f"{component}/binary-amd64/Packages.xz"
        digest, size = checksums[name]
        archive = fetch(BASE + "dists/noble/" + name, metadata / (component + "-Packages.xz"), digest, size)
        indices.append({"path": name, "sha256": digest, "bytes": size})
        with lzma.open(archive, "rt") as f:
            for paragraph in f.read().split("\n\n"):
                fields = {}
                for line in paragraph.splitlines():
                    if not line.startswith(" ") and ": " in line:
                        key, value = line.split(": ", 1)
                        fields[key] = value
                name = fields.get("Package")
                if name in PACKAGES:
                    selected[name] = {
                        "name": name, "version": fields["Version"],
                        "url": BASE + fields["Filename"], "bytes": int(fields["Size"]),
                        "sha256": fields["SHA256"],
                        "depends": fields.get("Depends", "")
                    }
    if set(selected) != set(PACKAGES):
        raise RuntimeError(f"Packages absent from signed indices: {set(PACKAGES) - set(selected)}")
    data = {"schema": 1, "suite": "Ubuntu 24.04 noble amd64", "inReleaseUrl": BASE + "dists/noble/InRelease",
            "inReleaseSha256": sha256(release), "signatureVerifiedWith": str(KEYRING), "indices": indices,
            "hostRequirements": ["Linux x86_64", "glibc >= 2.38", "libstdc++ with GLIBCXX_3.4.30", "binutils", "Python 3.10+"],
            "packages": [selected[name] for name in PACKAGES]}
    LOCK.write_text(json.dumps(data, indent=2) + "\n")
    print(f"Wrote signed-metadata pins to {LOCK}", flush=True)


def prepare():
    if platform.system() != "Linux" or platform.machine() != "x86_64":
        raise RuntimeError("This isolated compiler prefix requires x86_64 Linux or WSL Ubuntu")
    lock = json.loads(LOCK.read_text())
    identity = sha256(LOCK)
    prefix = CACHE / "toolchain" / identity[:12]
    stamp = prefix / ".shelf-toolchain-sha256"
    if stamp.exists() and stamp.read_text().strip() == identity:
        return prefix
    prefix.mkdir(parents=True, exist_ok=True)
    for package in lock["packages"]:
        archive = fetch(package["url"], CACHE / "downloads" / Path(package["url"]).name,
                        package["sha256"], package["bytes"])
        # Extract data only. dpkg-deb does not install packages, update dpkg state,
        # execute package scripts, or require root.
        run(["dpkg-deb", "--extract", archive, prefix])
    stamp.write_text(identity + "\n")
    return prefix


def environment(prefix):
    env = os.environ.copy()
    libs = str(prefix / "usr/lib/x86_64-linux-gnu")
    env["LD_LIBRARY_PATH"] = libs + (":" + env["LD_LIBRARY_PATH"] if env.get("LD_LIBRARY_PATH") else "")
    env["PATH"] = str(prefix / "usr/bin") + ":" + env["PATH"]
    return env


def host_options(prefix):
    gcc = prefix / "usr/lib/gcc/x86_64-linux-gnu/13"
    libs = prefix / "usr/lib/x86_64-linux-gnu"
    compile_flags = ["--gcc-install-dir=" + str(gcc),
                     "-idirafter", prefix / "usr/include", "-idirafter", prefix / "usr/include/x86_64-linux-gnu",
                     "-B" + str(libs), "-B" + str(gcc)]
    # Ubuntu's development linker script embeds one absolute *development*
    # archive path. Keep the extracted package intact and relocate that path in
    # a separate local overlay; actual libc/loader remain the compatible host's.
    overlay = CACHE / "host-link"
    overlay.mkdir(parents=True, exist_ok=True)
    script = (libs / "libc.so").read_text()
    original = "/usr/lib/x86_64-linux-gnu/libc_nonshared.a"
    if original not in script:
        raise RuntimeError("Unexpected pinned libc development linker script")
    script = script.replace(original, json.dumps(str(libs / "libc_nonshared.a")))
    (overlay / "libc.so").write_text(script)
    link_flags = ["-L" + str(overlay), "-L" + str(libs), "-L" + str(gcc)]
    return compile_flags, link_flags


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--resolve-lock", action="store_true")
    args = parser.parse_args()
    if args.resolve_lock:
        resolve_lock()
    else:
        prefix = prepare()
        run([prefix / "usr/bin/clang-18", "--version"], env=environment(prefix))
        run([prefix / "usr/bin/ld.lld-18", "--version"], env=environment(prefix))
        print("Isolated compiler prefix:", prefix)
