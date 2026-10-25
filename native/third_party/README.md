# Native dependency custody

`SOURCE.json` records pinned revisions, origin URLs, byte counts and SHA-256 for committed third-party files. The unmodified stb headers and Poppins files were also checked against the Git blob IDs returned for those exact revisions. Embedded stb licenses and the font's OFL notice are preserved.

The host SDL license is copied unmodified from the official SDL2 2.32.10 Windows development ZIP. The ZIP is verified against its GitHub-published SHA-256 (`af347939395a58b365846aaea27391e69f9ec9d4dd650d6ac40802159b418a6e`) before extraction by `tools/fetch-native-deps.js`. Windows development libraries and DLLs stay in ignored `.cache/`, with no global installation. `SDL2-PS5.txt` comes unmodified from the separately verified PacBrew v0.40.2 archive, which provides PS5 SDL 2.30.12.

The native renderer loads only the bundled font assets. Decoder/font files have not been modified; the surrounding application code lives in `native/src/`. See the root [third-party notices](../../THIRD_PARTY.md) and [image credits](../../assets/CREDITS.md).

`ps5-runtime/` contains the exact reviewed GPL native startup/runtime/build source imported through Jewel Case. Its own SOURCE.json records all original bytes and hashes. It is source code, not a proprietary runtime binary. The adapted console display/memory/shim code lives under `native/ps5/platform/`; original paths and hashes are recorded there. The [PS5 build notes](../../docs/PS5-BUILD.md) explain reproduction and remaining console/release checks.
