# PS5 cross-build status

Shelf now cross-compiles its C++ UI, SDL event loop and read-only discovery adapter into a native PS5 executable. Its software renderer uses a PS5 VideoOut backend and a direct-memory allocator. This is a development build: **no on-console app test or firmware 12.20 compatibility claim has been made, and no installable prototype is released yet**. Discovery works from PC against real console folders; its console-local FTP connection and launch adapters still need hardware validation.

## Build without a system-wide installation

On Windows, these commands use an **existing** WSL distribution named `Ubuntu`. The scripts do not install WSL, run apt, use sudo, alter shell profiles, set global environment variables, or connect to a console. On x86-64 Ubuntu/Linux, the Python scripts can be invoked directly instead. A compatible host libc, binutils (`ar`/the native linker), `dpkg-deb`, and Python 3.10+ must already exist. A compiler and development headers need not be installed globally.

```sh
npm run build:ps5
npm run test:ps5-platform
npm run verify:ps5
npm run test:ps5-verifier
```

The first build downloads the pinned target dependencies and an isolated Clang 18 prefix into ignored `.cache/ps5/`. Subsequent builds verify and reuse those archives. Nothing is deployed. The output is `.cache/ps5/build/eboot.elf`, `eboot.bin`, and `structural-check.json`; it is **not** a ready-to-install app folder. Do not copy the executable over another app.

The Ubuntu compiler packages are exact versions and SHA-256 values in [ubuntu-toolchain.lock.json](../native/ps5/ubuntu-toolchain.lock.json). Those pins were resolved from Ubuntu Noble's `main` and `universe` package indices, with their hashes checked against an InRelease signature verified using Ubuntu's installed archive keyring. `bootstrap_ps5_toolchain.py --resolve-lock` is an explicit maintainer action to refresh the lock; ordinary builds do not resolve new versions. Packages are extracted as data with `dpkg-deb`, without executing package scripts. The host's libc/loader are retained. A local linker-script overlay relocates the development-only `libc_nonshared.a` path, leaving the extracted package and OS files unchanged.

## Target dependencies and attribution

The [target dependency pins](../native/ps5/dependencies.json) identify:

- Public PS5 Payload SDK v0.42; SHA-256 checked against its GitHub release asset digest.
- PacBrew v0.40.2 static ports; SHA-256 checked against its GitHub release asset digest. Only its `target/user/homebrew` prefix is extracted. Shelf links SDL 2.30.12 (`SDL-2.30.12-ga47182a20`), libiconv 1.17 and libsamplerate 0.2.2. The Windows host uses a separate SDL 2.32.10 package.
- zlib 1.3.2, built locally from its pinned canonical source archive for the host packager.

The native startup, linker layout, module conversion, FSELF writer and clean-room runtime builder are imported from Jewel Case's pinned source, originally by BlackBearReloaded with converter/writer portions derived from SvenGDK's SharpProspero. Their original copyright and GPL notices are retained under [native/third_party/ps5-runtime](../native/third_party/ps5-runtime/). Its SOURCE.json records every imported file's byte count and SHA-256; the build checks them before compilation.

Shelf adapts Jewel Case's allocator, VideoOut and missing-import shims under [native/ps5/platform](../native/ps5/platform/). That directory records original paths/hashes and marks the adaptations. Changes add overflow checks, a bounded import report, a notification guard, the first back-buffer selection fix and joined display shutdown with direct-memory cleanup. A failed display close retains its scan-out memory for process teardown instead of unmapping active buffers. PS5 launch arguments cannot override Shelf's owned storage paths. The application uses no copied Jewel Case UI, assets, music, game database or embedded emulator.

The clean-room runtime is rebuilt twice from source. Both raw copies must match `8ee6e124993e1af26420cb455890fd002f5d6c7e78883c860ce45734e7d002bb`; both signed copies must match `e6ff45d16adf687855cc3b33b0c8a4132b6504360b221e0a34c7e99fb3ba0036`. A failed match stops the build. It contains no downloaded proprietary runtime binary.

## What the checks establish

The host platform tests exercise the actual PS5 allocator/display source using fake console functions. They check allocation overflow, failed realloc preservation, five startup failure paths, the first displayed frame, alternating buffer selection, joined cleanup and memory retention after a failed display close. They do not simulate the PS5 graphics driver or establish its ABI compatibility.

The packager inspects the completed FSELF's container integrity. A separate Python verifier checks native ELF identity, executable entry, LOAD bounds, page alignment, absence of writable/executable LOAD segments, the expected system-module dependency set and the SELF wrapper. Recorded artifact hashes reject changes after the container check. `test:ps5-verifier` checks that eight deliberately damaged copies are rejected. Build inputs are fingerprinted; verification refuses an executable if those inputs have since changed. The report explicitly leaves hardware, firmware and physical controller tests unverified. The regular Windows model/renderer tests remain separate.

## Before a first downloadable prototype

Choose a separate unofficial title identity and check for an installed collision. Assemble the owned app folder, artwork, complete licenses and corresponding source/relink instructions for all linked libraries, including LGPL libiconv and the precise PS5 SDL port revision. Validate discovery and launch contracts, transfer/readback hashes, then test display, DualSense input, saved settings, restart/reopen and supported routes on firmware 12.20. Keep prototype publication gated on those results; see the [acceptance plan](PROTOTYPE-PLAN.md).

No kernel helper, exploit, firmware updater, network configuration tool or console deployment command runs as part of this cross-build.

A separate [private UI test assembler](PS5-UI-TEST.md) can now prepare a local folder and ZIP after the reviewed sources are committed. It uses its own unofficial app identity and records each file's hash; it does not deploy or publish anything. That package is a hardware-test candidate, not the first public prototype release.
