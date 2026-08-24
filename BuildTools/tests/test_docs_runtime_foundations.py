from __future__ import annotations

import json
import unittest
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]


class DocumentationRuntimeFoundationsTests(unittest.TestCase):
    def test_map_guide_matches_loader_and_pathfinding_surface(self) -> None:
        guide = (ENGINE_ROOT / "Docs/en/explanation/maps-and-movement.md").read_text(
            encoding="utf-8"
        )
        loader = (ENGINE_ROOT / "Source/Common/MapLoader.h").read_text(
            encoding="utf-8"
        )

        for marker in (
            "string_view file_name,",
            "MapLoader::EnumerateMaps(file_name, buf)",
            "Source/Tests/Test_Geometry.cpp",
            "Source/Tests/Test_PathFinding.cpp",
            "PathFinding::EvaluateFreeMovementEndOffset()",
            "MapManager::FindPathToAny()",
        ):
            self.assertIn(marker, guide)
        for marker in (
            "static void Load(string_view name, string_view file_name",
            "static auto EnumerateMaps(string_view file_name, const string& buf)",
        ):
            self.assertIn(marker, loader)

    def test_networking_guide_matches_current_hardening_controls(self) -> None:
        guide = (
            ENGINE_ROOT / "Docs/en/explanation/authority-and-networking/index.md"
        ).read_text(encoding="utf-8")
        settings = (ENGINE_ROOT / "Source/Common/Settings.inc").read_text(
            encoding="utf-8"
        )
        udp = (ENGINE_ROOT / "Source/Common/NetworkUdp.h").read_text(
            encoding="utf-8"
        )

        for marker in (
            "Length-before-allocation rule",
            "Maximum message size",
            "Per-pass message budget",
            "UDP reorder window",
            "Pre-handshake parse failures",
            "MaxReorderAhead",
            "NetMessage::UnresolvedHash",
            "Async transport connection lifetime & threading",
        ):
            self.assertIn(marker, guide)
        for marker in (
            "MaxMessageSize",
            "MaxMessagesPerProcessPass",
            "MaxUdpReorderAhead",
            "LoginTimeout",
        ):
            self.assertIn(marker, settings)
        self.assertIn("uint32_t MaxReorderAhead", udp)

    def test_persistence_guide_matches_oplog_path_contract(self) -> None:
        guide = (ENGINE_ROOT / "Docs/en/explanation/persistence/index.md").read_text(
            encoding="utf-8"
        )
        database = (ENGINE_ROOT / "Source/Server/DataBase.cpp").read_text(
            encoding="utf-8"
        )

        for marker in (
            "must be non-empty",
            "replacing `.oplog` with `-committed.oplog`",
            "Source/Tests/Test_DataBase.cpp",
        ):
            self.assertIn(marker, guide)
        self.assertIn('throw DataBaseException("Empty oplog path in settings")', database)
        self.assertIn('replace(".oplog", "-committed.oplog")', database)

    def test_manifest_owns_canonical_and_legacy_routes(self) -> None:
        manifest = json.loads(
            (ENGINE_ROOT / "Docs/documentation-manifest.json").read_text(
                encoding="utf-8"
            )
        )["documents"]
        pairs = (
            (
                "Docs/en/explanation/maps-and-movement.md",
                "Docs/MapsMovementGeometry.md",
                "maps-movement-geometry",
            ),
            (
                "Docs/en/explanation/authority-and-networking/index.md",
                "Docs/Networking.md",
                "networking",
            ),
            (
                "Docs/en/explanation/persistence/index.md",
                "Docs/Persistence.md",
                "persistence",
            ),
        )

        for canonical_path, legacy_path, document_id in pairs:
            canonical = manifest[canonical_path]
            legacy = manifest[legacy_path]
            self.assertEqual(canonical["id"], document_id)
            self.assertEqual(canonical["state"], "current")
            self.assertEqual(canonical["disposition"], "retain")
            self.assertEqual(legacy["state"], "redirect")
            self.assertEqual(legacy["redirect_to"], document_id)
            self.assertEqual(legacy["target"], canonical_path)


if __name__ == "__main__":
    unittest.main()
