# SPDX-License-Identifier: GPL-3.0-only
"""Small in-memory packaging fixtures. No console, hardware claims or release ZIPs."""
import copy
import posixpath
import re
import unittest
from prepare_ps5_release import (digest, public_guides, public_source_documents, verify_asset_sources,
                                 verify_console_manifest, verify_source_members)


class ReleaseInputTests(unittest.TestCase):
    def setUp(self):
        self.raw = {"fixture.bin": b"fixture program", "credits.txt": b"fixture notice"}
        self.files = {name: {"path": name, **digest(raw)} for name, raw in self.raw.items()}
        self.readback = {"files": list(self.files.values()), "selfMode": "226 SELF transfer mode disabled"}

    def test_complete_console_ledger(self):
        verify_console_manifest(self.files, self.readback)

    def test_partial_console_ledger_rejected(self):
        partial = copy.deepcopy(self.readback); partial["files"].pop()
        with self.assertRaisesRegex(ValueError, "full file ledger"):
            verify_console_manifest(self.files, partial)

    def test_changed_asset_in_console_ledger_rejected(self):
        changed = copy.deepcopy(self.readback); changed["files"][1].update(digest(b"different notice"))
        with self.assertRaisesRegex(ValueError, "differs"):
            verify_console_manifest(self.files, changed)

    def test_duplicate_console_member_rejected(self):
        duplicate = copy.deepcopy(self.readback); duplicate["files"].append(duplicate["files"][0])
        with self.assertRaisesRegex(ValueError, "unique full"):
            verify_console_manifest(self.files, duplicate)

    def test_transformed_self_mode_rejected(self):
        transformed = {**self.readback, "selfMode": "226 SELF transfer mode enabled"}
        with self.assertRaisesRegex(ValueError, "raw SELF"):
            verify_console_manifest(self.files, transformed)

    def test_source_member_set_is_exact(self):
        data = {"SOURCE-KIT-MANIFEST.json": b"fixture metadata", "source.cpp": b"fixture source"}
        manifest = {"files": [{"path": "source.cpp", **digest(data["source.cpp"])}]}
        verify_source_members(data, manifest)
        with self.assertRaisesRegex(ValueError, "undeclared"):
            verify_source_members({**data, "unlisted.bin": b"not source"}, manifest)
        duplicate = {"files": manifest["files"] * 2}
        with self.assertRaisesRegex(ValueError, "duplicate"):
            verify_source_members(data, duplicate)

    def test_assets_and_notices_match_corresponding_sources(self):
        app = {"assets/3d-icons/n64.png": b"fixture image", "licenses/cJSON-MIT.txt": b"fixture license"}
        source = {"assets/3d-icons/n64.png": b"fixture image", "native/third_party/cjson/LICENSE": b"fixture license"}
        result = verify_asset_sources(app, source)
        self.assertEqual(result["byteExactFiles"], 2)
        with self.assertRaisesRegex(ValueError, "differs"):
            verify_asset_sources({**app, "assets/3d-icons/n64.png": b"other image"}, source)
        with self.assertRaisesRegex(ValueError, "lacks corresponding"):
            verify_asset_sources(app, {"assets/3d-icons/n64.png": b"fixture image"})

    def test_only_exact_license_newline_difference_is_recorded(self):
        result = verify_asset_sources({"LICENSE": b"fixture license\r\n"}, {"LICENSE": b"fixture license\n"})
        self.assertEqual(result["byteExactFiles"], 0)
        notice = result["lineEndingOnlyNotices"][0]
        self.assertEqual(notice["packaged"], digest(b"fixture license\r\n"))
        self.assertEqual(notice["source"], digest(b"fixture license\n"))
        with self.assertRaisesRegex(ValueError, "differs"):
            verify_asset_sources({"LICENSE": b"changed license\r\n"}, {"LICENSE": b"fixture license\n"})
        with self.assertRaisesRegex(ValueError, "differs"):
            verify_asset_sources({"assets/CREDITS.md": b"credits\r\n"}, {"assets/CREDITS.md": b"credits\n"})

    def test_unmapped_program_file_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "lacks corresponding"):
            verify_asset_sources({"unknown.dat": b"fixture"}, {"unknown.dat": b"fixture"})

    def test_room_link_resolves_from_both_guides(self):
        members = public_guides("See [room setup](ROOM.md).", b"fixture room guide")
        self.assertEqual(set(members), {"README.md", "ROOM.md", "PPSA99541/START-HERE.md"})
        for name in ("README.md", "PPSA99541/START-HERE.md"):
            target = re.search(r"\]\(([^)]+)\)", members[name].decode()).group(1)
            resolved = posixpath.normpath(posixpath.join(posixpath.dirname(name), target))
            self.assertIn(resolved, members)
            self.assertEqual(members[resolved], b"fixture room guide")

    def test_source_publication_corrects_docs_without_changing_program_inputs(self):
        private_notice = (b"Console Shelf source and relink kit (development build)\n"
                          b"This is a private preparation artifact. Publication still requires the full\n"
                          b"hardware and launch acceptance in docs/PROTOTYPE-PLAN.md.")
        original = {"SOURCE-KIT.txt": private_notice,
                    "README.md": b"# Shelf\nThis prototype uses an artwork fallback and does not play video.\n",
                    "source.cpp": b"frozen application source\x00\xff", "objects/main.o": b"frozen object",
                    "libraries/libSDL2.a": b"frozen library", "assets/room/intros/ps2.mpg": b"frozen media"}
        before = original.copy()
        documents = public_source_documents(original, "v0.1.0-alpha.1")
        self.assertEqual(original, before)
        self.assertEqual(set(documents), {"README.md", "SOURCE-KIT.txt"})
        self.assertNotIn(b"does not play video", documents["README.md"])
        self.assertIn(b"historical", documents["README.md"])
        published = {**original, **documents}
        for name in original.keys() - documents.keys():
            self.assertEqual(published[name], before[name])
        manifest = {"files": [{"path": name, **digest(raw)} for name, raw in published.items()]}
        verify_source_members({**published, "SOURCE-KIT-MANIFEST.json": b"fixture metadata"}, manifest)
        with self.assertRaisesRegex(ValueError, "publication notice"):
            public_source_documents({**original, "SOURCE-KIT.txt": b"already published or unexpected"}, "v0.1.0-alpha.1")


if __name__ == "__main__":
    unittest.main()
