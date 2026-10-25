# SPDX-License-Identifier: GPL-3.0-only
"""Fetch and verify pinned target dependencies into .cache/ps5 only."""
import json
from ps5_common import CACHE, ROOT, prepare_archive


def prepare():
    pins = json.loads((ROOT / "native/ps5/dependencies.json").read_text())
    sdk = prepare_archive(pins["sdk"], "ps5-payload-sdk-v0.42.zip", CACHE / "sdk-v0.42", "ps5-payload-sdk/")
    ports = prepare_archive(pins["ports"], "pacbrew-v0.40.2.tar.gz", CACHE / "ports-v0.40.2",
                            "opt/ps5-payload-sdk/target/user/homebrew/")
    zlib = prepare_archive(pins["zlib"], "zlib-1.3.2.tar.gz", CACHE / "zlib-1.3.2", "zlib-1.3.2/")
    for required in [sdk / "target/include/stdlib.h", sdk / "bin/prospero-lld",
                     ports / "lib/libSDL2.a", zlib / "zlib.h"]:
        if not required.is_file():
            raise RuntimeError(f"Dependency has an unexpected layout: {required}")
    return sdk, ports, zlib


if __name__ == "__main__":
    print("Verified isolated dependencies:", *(str(p) for p in prepare()), sep="\n")
