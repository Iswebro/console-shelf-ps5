# isShelf beta release and store listings

Prepared 26 October 2026. This is a submission plan, not a published release or a claim that the current branding build passed on PS5.

## Release decision

**Recommended label: public beta**, once the final isShelf package passes the console checks below. UI-22 was accepted by the owner on firmware 12.20, including navigation, saved settings, normal restart, emulator menus and direct RetroArch launches. UI-23 changes the branding, home-screen art and icon options; its exact package still needs a console check. Describe firmware 12.20 as *tested*, not every jailbreak firmware as supported.

Before publication:

1. Freeze one source commit and a matching tagged version. `v0.1.0-beta.1` is a suitable first-beta tag if the build owner has not selected one; the release preparer accepts this pattern. Keep the established title ID `PPSA99541` and its user-data path.
2. Install that exact UI-23 candidate on the PS5 and record its package manifest, successful open, home-screen icon/background, Grid/Swipe/Shelf, settings and icon choice persistence, close/reopen, a normal console restart, and the already documented menu/direct launch routes. Record any limitation that remains.
3. Produce a store-installable ZIP whose top level is **only** `PPSA99541/`, containing `eboot.bin`, `sce_sys/` and the rest of the tested app folder. The present public release preparer also puts documentation and manifests at the ZIP root; retain those in the full download if useful, but verify the dedicated store ZIP against the same tested app bytes. Use a stable asset name such as `PPSA99541.zip`.
4. Update the GitHub repository description and README to the accepted isShelf status, with the exact tested firmware and download/install links. The live repository description still says "Offline PC design preview"; some directories use that text for discovery. Publish the tagged GitHub release with the app ZIP, complete source/relink archive, SHA-256 sums, release notes, and links to install, upgrade, recovery, removal, credits and known limitations. Name `PPSA99541.zip` explicitly as the installable download so catalog discovery does not confuse it with the source ZIP. The release must point to the source commit used for the executable. Read back the published assets and compare SHA-256 before listing them.
5. Confirm the tag contains `native/ps5/param.json` with the shipped `titleId` and raised `contentVersion`. Confirm the 1024×1024 `assets/branding/isshelf-home-icon.png` is committed at that tag; the ProsperoStore record can link to it directly. Check for a title ID collision again before submission.
6. Check links inside the store-only ZIP's `PPSA99541/START-HERE.md`. The release preparer now points its Room link at `docs/ROOM.md` under the release tag, so confirm that tagged page opens after publication. Re-read the final ZIP and its hash.

`tools/prepare_store_zip.py` is ready for step 3. Run `python tools/prepare_store_zip.py <full-release.zip> <new-PPSA99541.zip>` after the full release ZIP passes its own checks. It reads the full release's `MANIFEST.json`, verifies every app member and writes only the app folder to a new archive. Compare its printed SHA-256 with a fresh file hash, then attach that same store ZIP to the tagged GitHub release. Keep the fuller preparation ZIP for audit rather than publishing two competing app downloads; publish the source kit and checksums as separately named assets. The release preparer's original `SHA256SUMS.txt` names the fuller ZIP, so make the final published checksum list from the actual published assets, including `PPSA99541.zip`. Its two focused fixture tests pass with `python tools/test_prepare_store_zip.py`.

The current public repository is [Iswebro/console-shelf-ps5](https://github.com/Iswebro/console-shelf-ps5). At preparation time it had **no published GitHub release**. Do not fill in a version, artifact URL or SHA-256 from an unposted local file.

## Listing order

| Destination | Route after release | Current finding |
| --- | --- | --- |
| [ProsperoStore catalog](https://github.com/darkxex/ps5-homebrew-catalog/blob/main/docs/submitting.md) | Pull request adding `apps/PPSA99541.json` from Iswebro's GitHub account. | Native PS5 apps are eligible. The release asset must be a ZIP of the app folder from a published GitHub release; prereleases are accepted. The catalog pins its SHA-256 and icon URL, then a maintainer reviews it. Category: `utilities`. Its [review policy](https://github.com/darkxex/ps5-homebrew-catalog/blob/main/docs/review-policy.md) requires a derivative to be substantially different from a listed app. Explain isShelf's own catalogue, layouts, discovery, persistence and launch-route behavior, with clear Estante credit. |
| [RecompStore](https://recompstore.com/submit) | Submit the app through its site with repository, release file, description and pictures. | Its [Apps section](https://recompstore.com/apps) includes Estante, so the description and screenshots must make isShelf's distinct behavior clear. Its [README](https://github.com/elripalda/RecompStore/blob/main/README.md) says installs are drawn from developer GitHub releases and can use ZIP/7z. Its [title ID checker](https://recompstore.com/ids) reported `PPSA99541` absent on 26 October 2026; check again before sending. |
| [PS5 Library](https://github.com/deox1111/ps5-library#the-default-catalog) | Check its default homebrew catalog after the ProsperoStore listing is merged. | PS5 Library reads the `homebrew.page` catalog by default. That feed currently redirects to the live `prosperostore.net` catalog, so one ProsperoStore record should reach PS5 Library. Check the actual in-app result before claiming placement. |
| [Overdrive Homebrew](https://github.com/MyLuxy/overdrive#features) | Check its Homebrew section after the ProsperoStore merge. | Its own README says it imports native apps from `prosperostore.net` and installs from each developer's GitHub release. This is downstream of the catalog, with no separate isShelf submission route identified. |
| [PS5 Scene Wire](https://github.com/SpiderNic96/ps5-scene-wire#track-another-app) | Check whether the release appears in its app feed after the ProsperoStore merge. If absent, propose a one-line repository entry in `api/_sources.js`. | Its source imports the `homebrew.page` catalog, which currently redirects to the live `prosperostore.net` catalog. This is a discovery index, not an installer store. |
| [BlackBox](https://github.com/D3ATHLY/blackbox) | Ask its maintainer whether it accepts third-party native app listings. | The published README describes its catalog as game metadata and documents no app submission route. Treat as a lead until the maintainer confirms eligibility and format. |
| [PKG-Zone](https://pkg-zone.com/?console=PS5) | Revisit if it provides a native PS5 app-folder format and submission route. | Its visible catalog includes PS5 entries, but the published installation instructions are for its HB-Store and a PS4-style setup. No verified route for an isShelf `PPSA99541/` ZIP was found. |
| [myfl.uk directory](https://myfl.uk/about/) | Consider a project suggestion after the release if the maintainer is expanding the hand-picked resource list. | It is a small curated directory linking to source projects, not a console installer or open submission catalog. |

The former `blackbearreloaded/ps5-homebrew-catalog` GitHub URL resolves to `darkxex/ps5-homebrew-catalog`; submit one catalog pull request, not duplicate records under both names. Other clients that read this catalog may show the resulting listing without separate submissions.

## Submission copy

**Name:** isShelf<br>
**Author:** Iswebro<br>
**Type:** App / Utilities<br>
**Title ID:** `PPSA99541`<br>
**Short description:** A native PS5 library front end with 52 system categories, saved layouts and clearly labelled emulator launch routes.<br>
**Repository:** https://github.com/Iswebro/console-shelf-ps5<br>
**Tested firmware:** 12.20; report other firmware only after direct testing.<br>
**Dependencies:** Existing homebrew enabler, ShadowMountPlus, console-local ELF loader on port 9021 and `ftpsrv` on port 2121 for library discovery.<br>
**Attribution:** Original isShelf app by Iswebro; acknowledge Estante and other upstream references in `THIRD_PARTY.md`. The listing should explain the functional differences rather than present the logo or name change as the main distinction.

Draft ProsperoStore record after the release exists (replace every angle-bracket value with verified release data):

```json
{
  "titleid": "PPSA99541",
  "name": "isShelf",
  "kind": "app",
  "category": "utilities",
  "description": "A native PS5 library front end with 52 system categories, saved layouts and clearly labelled emulator launch routes.",
  "license": "GPL-3.0",
  "author": "Iswebro",
  "version": "<RELEASE_TAG>",
  "source_repo": "https://github.com/Iswebro/console-shelf-ps5",
  "artifact_url": "https://github.com/Iswebro/console-shelf-ps5/releases/download/<RELEASE_TAG>/PPSA99541.zip",
  "sha256": "<VERIFIED_LOWERCASE_SHA256>",
  "icon_url": "https://raw.githubusercontent.com/Iswebro/console-shelf-ps5/<RELEASE_TAG>/assets/branding/isshelf-home-icon.png"
}
```

Before opening the PR, run the catalog's documented checks against the real record and verify the published icon URL. The current catalog had no `apps/PPSA99541.json` when this plan was prepared; recheck at submission time.

## RecompStore form draft

These are the site's actual fields, inspected on 26 October 2026. Fill in the final release fields only after the public asset has been read back. Do not send this form yet.

| Form field | Prepared answer |
| --- | --- |
| What is it? | **App** |
| Name | `isShelf` |
| Made by | `Iswebro` |
| How can we reach you? | Discord `@iswebro` (owner-provided) |
| GitHub repository | `https://github.com/Iswebro/console-shelf-ps5` |
| Release file | `PPSA99541.zip`, if that exact store ZIP is published; otherwise the verified asset's real name |
| Version | The published tag/version, after release |
| Title ID | `PPSA99541` |
| One-line summary | `A native PS5 library front end with 52 system categories, saved layouts and clear emulator launch routes.` |
| Description | See the ready text below; update the final-tested route list if it changes |
| What players need | `Jailbroken PS5; ShadowMountPlus, an ELF loader and ftpsrv; installed emulators for game launches.` |
| Credits | `Created by Iswebro. Estante by Kauan Silva Lopes informed native library discovery and the S bookshelf mark; Jewel Case by SkaRift and BlackBearReloaded's PS5 runtime contributed credited GPL components. Full credits and licenses: GitHub THIRD_PARTY.md and assets/CREDITS.md.` |
| Web link 1 | Tagged GitHub release URL after publication |
| Web link 2 | Tagged installation guide URL after publication |
| Web link 3 | Leave blank unless Iswebro supplies a product/community link |
| Anything else? | `Public beta tested on firmware 12.20. isShelf is a distinct console-first app: 52 browsable system categories, Grid/Swipe/Shelf layouts, a customizable TV room, persistent filters and emulator preferences, read-only discovery, and direct-versus-menu launch explanations. It does not bundle emulators, games, BIOS or keys.` |
| Rights checkbox | Review the final archive, notices and artwork credits, then check when sending |

Ready description:

> isShelf by Iswebro is a native, controller-first PS5 library front end. Browse 52 console and computer system categories in Grid, Swipe or a customizable Shelf room. It discovers existing libraries and installed emulators without moving games. Layout, favorites, visibility and preferred emulator choices persist. Before launch, the app explains whether a selection starts a game directly, opens an emulator's own library, or is unavailable. This is an organizer and launcher, not an emulator; it includes no games, ROMs, BIOS or keys. The first beta is tested on firmware 12.20. Other firmware and emulator combinations remain unconfirmed unless the tagged release notes say otherwise. Installation, dependencies, checked routes and recovery steps are in the linked GitHub release.

Optional picture preparation from the current tree:

- **Icon:** `assets/branding/isshelf-home-icon.png` (1024×1024 PNG, about 126 KB). RecompStore requests a square image at least 512×512.
- **Backdrop:** `assets/backgrounds/shelf-ps5-home-v2.png` (3840×2160 PNG, about 3.9 MB). The form recommends 16:9 at 3840×2160.
- **Transparent title logo:** optional. The existing transparent S mark is `assets/branding/shelf-gaming-mark.png`; it does not spell the app name, so use a final isShelf wordmark if the art handoff supplies one, or leave the logo field blank.
- **Screenshots:** up to four from the final PS5 build. Capture Grid, Swipe, Shelf room and a console library/launch panel or Settings after the UI-23 test. Do not present PC mockups as console captures.

The form accepts PNG, JPEG or WebP pictures up to 8 MB each. The icon and backdrop above fit those stated size limits, but confirm the final files again at submission. RecompStore's contact field is shown only to its developer according to the form.

## Other submission follow-through

- **PS5 Library, Overdrive Homebrew and PS5 Scene Wire:** After the ProsperoStore PR merges, inspect their app listings for `isShelf`. If Scene Wire misses it, prepare a one-line `Iswebro/console-shelf-ps5` entry for its [`api/_sources.js`](https://github.com/SpiderNic96/ps5-scene-wire/blob/main/api/_sources.js) and open a PR with the released app link.
- **BlackBox:** Its [README](https://github.com/D3ATHLY/blackbox) gives no third-party native-app intake. After the release, ask via its issue/contact channel: `Iswebro has released isShelf, a GPL-3.0 native PS5 library front end (PPSA99541). Does BlackBox accept listings for third-party homebrew app-folder ZIPs from developer GitHub releases? If so, what metadata, format and review route should we use?` Do not claim a BlackBox listing before a maintainer provides a route.
- **PKG-Zone and myfl.uk:** Recheck their current formats and intake route after launch. Their pages are directory leads, with no confirmed native app-folder submission path today.
