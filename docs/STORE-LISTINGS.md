# isShelf beta store submissions

Updated 26 October 2026. The [public v0.1.0-beta.1 release](https://github.com/Iswebro/console-shelf-ps5/releases/tag/v0.1.0-beta.1) is available. Its installable `PPSA99541.zip` and the other six public downloads passed SHA-256 readback. The owner accepted the release on a PS5 running firmware 12.20. Other firmware versions are not yet verified.

## Submitted

| Destination | Status | Record |
| --- | --- | --- |
| [RecompStore](https://recompstore.com/submit) | Submitted for developer review. The form confirmed “Thanks, it's in.” Placement is pending. | App: isShelf; developer: Iswebro; private contact: Discord `@iswebro`; title ID: `PPSA99541`; release ZIP, tagged install guide, icon, backdrop and one PS5 screenshot supplied. |
| [ProsperoStore](https://prosperostore.net/app/PPSA99541/) | [PR #777](https://github.com/darkxex/ps5-homebrew-catalog/pull/777) merged. The web listing now responds with isShelf; appearance and installation in the native PS5 store remain to be checked on console. | `apps/PPSA99541.json` contains the public release URL, exact SHA-256 and pinned icon. The PR explains isShelf's distinct features and credits Estante. |
| [Overdrive](https://github.com/MyLuxy/overdrive#features) | isShelf is available in the live feed used by its Homebrew section. Overdrive's actual importer accepted the entry and enabled the correct download and SHA-256. Reopen Overdrive, then search Homebrew for isShelf; the console display is pending owner confirmation. | No separate submission is needed. The importer is identical in the installed v1.0.9 source and current main. [Feed verification](verification/overdrive-live-feed-2026-10-26.json). |
| [BlackBox](https://github.com/D3ATHLY/blackbox) | [Issue #59](https://github.com/D3ATHLY/blackbox/issues/59) asks whether third-party native PS5 apps are eligible and how to submit them. Its published README has no native-app intake format. This is an inquiry, not a listing. | Repository, release and title ID linked for the maintainer. |

The former `blackbearreloaded/ps5-homebrew-catalog` URL resolves to `darkxex/ps5-homebrew-catalog`. One catalog PR covers ProsperoStore; no duplicate entry is needed.

## Release facts used in the submissions

| Field | Value |
| --- | --- |
| App | isShelf by Iswebro, GPL-3.0 |
| Type | Native PS5 library front end; app / utilities |
| Title ID | `PPSA99541` |
| Version | `v0.1.0-beta.1` |
| Install asset | [PPSA99541.zip](https://github.com/Iswebro/console-shelf-ps5/releases/download/v0.1.0-beta.1/PPSA99541.zip), 104,371,163 bytes |
| SHA-256 | `4cca4e0c05b58b74733f8bc4392ba5b26bbd05232dde41932be11093a4ab91d6` |
| Tagged source | `5c34e67aba3e51ca209d56215f97b586ff58c1e5` |
| Install instructions | [Tagged installation guide](https://github.com/Iswebro/console-shelf-ps5/blob/v0.1.0-beta.1/docs/PS5-INSTALL.md) |
| Credits | [THIRD_PARTY.md](https://github.com/Iswebro/console-shelf-ps5/blob/v0.1.0-beta.1/THIRD_PARTY.md) |
| Publication checks | [Verification record](verification/v0.1.0-beta.1-publication-2026-10-26.json) |

The catalog record passed `python -m catalog check` with 143 valid records and no errors or warnings. `python -m catalog verify PPSA99541` matched the published asset to its SHA-256 and found content version `00.100.023`. The pinned 1024×1024 PNG icon URL was downloaded and matched the tagged local image.

Submission summary: “A native PS5 library front end with 52 system categories, saved layouts and clearly labelled emulator launch routes.” isShelf organizes and discovers an existing library and installed emulators. It does not bundle games, ROMs, BIOS, keys or emulators. The listing credits Estante, Jewel Case and other referenced projects, and explains isShelf's own Grid, Swipe and Shelf room layouts, personal TV, saved filters and preferences, and direct-play versus emulator-menu guidance.

The owner's console intro videos are private media. They are not in the public release or any store submission.

## After review

1. On the PS5, confirm that isShelf appears in the native ProsperoStore app and that its install or update path presents the expected `v0.1.0-beta.1` release. The [web listing](https://prosperostore.net/app/PPSA99541/) is available.
2. Confirm Overdrive's Homebrew listing on the console after reopening the app. Also check [PS5 Library](https://github.com/deox1111/ps5-library#the-default-catalog) and [PS5 Scene Wire](https://github.com/SpiderNic96/ps5-scene-wire#track-another-app). They consume or refer to the ProsperoStore catalog; no separate native-app submission route was identified. If Scene Wire misses the app, propose its one-line repository entry in `api/_sources.js`.
3. Wait for RecompStore's response at Discord `@iswebro` and verify the public listing before announcing placement.
4. If BlackBox confirms that it accepts native PS5 app folders, follow its specified submission route. Do not place an app record into its game-metadata catalog without that confirmation.
5. [PKG-Zone](https://pkg-zone.com/?console=PS5) has PS5 entries but no verified submission route for a `PPSA99541/` app-folder ZIP. [myfl.uk](https://myfl.uk/about/) is a curated resource directory, not a native installer catalog. Revisit either only if an appropriate route is published.
