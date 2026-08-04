from __future__ import annotations

import contextlib
import io
import json
import re
import sys
import unittest
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
ENGINE_ROOT = BUILDTOOLS_DIR.parent
sys.path.insert(0, str(BUILDTOOLS_DIR))

import docs_ai_control_protocol  # noqa: E402
import docs_localization  # noqa: E402


class AiControlProtocolDocumentationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.model = docs_ai_control_protocol.generate_ai_control_protocol_model(
            ENGINE_ROOT
        )

    def test_model_is_deterministic_and_source_backed(self) -> None:
        second = docs_ai_control_protocol.generate_ai_control_protocol_model(
            ENGINE_ROOT
        )

        self.assertEqual(self.model, second)
        self.assertEqual(self.model["schema_version"], 1)
        self.assertEqual(self.model["scope"]["stability"], "experimental")
        self.assertEqual(self.model["summary"]["entry_count"], 49)
        self.assertEqual(self.model["summary"]["method_count"], 6)
        self.assertEqual(self.model["summary"]["command_field_count"], 11)
        self.assertEqual(
            self.model["outputs"]["methods"],
            ["auth", "ping", "status", "observe", "events", "act"],
        )
        self.assertEqual(self.model["outputs"]["max_line_bytes"], 1024 * 1024)

        identities = [
            entry["id"]
            for collection in docs_ai_control_protocol.COLLECTION_KINDS
            for entry in self.model[collection]
        ]
        self.assertEqual(len(identities), len(set(identities)))

    def test_every_manifest_anchor_exists_in_engine_sources(self) -> None:
        for collection in docs_ai_control_protocol.COLLECTION_KINDS:
            for entry in self.model[collection]:
                for source in entry["source"]:
                    source_text = (ENGINE_ROOT / source["path"]).read_text(
                        encoding="utf-8", errors="replace"
                    )
                    for anchor in source["anchors"]:
                        self.assertIn(anchor, source_text)

    def test_generated_pages_cover_protocol_security_and_boundaries(self) -> None:
        pages = docs_ai_control_protocol.render_reference_pages(ENGINE_ROOT)

        self.assertEqual(set(pages), set(docs_ai_control_protocol.OUTPUT_PATHS))
        self.assertEqual(
            pages,
            docs_ai_control_protocol.render_reference_pages(ENGINE_ROOT),
        )
        self.assertIn(
            "ai-control-protocol.wire.ndjson",
            pages["Docs/en/reference/ai-control-protocol/wire.md"],
        )
        self.assertIn(
            "ai-control-protocol.method.act",
            pages["Docs/en/reference/ai-control-protocol/methods.md"],
        )
        self.assertIn(
            "compile the listener",
            pages["Docs/en/reference/ai-control-protocol/security.md"],
        )
        self.assertIn(
            "MCP adapter boundary",
            pages[
                "Docs/en/reference/ai-control-protocol/integration-validation.md"
            ],
        )
        self.assertIn(
            "Сгенерированный справочник протокола AiControl",
            pages["Docs/ru/reference/ai-control-protocol/index.md"],
        )
        self.assertIn(
            "Очередь команд заполнена",
            pages["Docs/ru/reference/ai-control-protocol/wire.md"],
        )
        self.assertNotIn(
            "The request cannot be interpreted safely.",
            pages["Docs/ru/reference/ai-control-protocol/wire.md"],
        )

    def test_russian_pages_pin_english_hashes_and_preserve_commands(self) -> None:
        pages = docs_ai_control_protocol.render_reference_pages(ENGINE_ROOT)
        for (_, document_id, _), english_path, russian_path in zip(
            docs_ai_control_protocol.PAGE_DEFINITIONS,
            docs_ai_control_protocol.CANONICAL_OUTPUT_PATHS,
            docs_ai_control_protocol.RUSSIAN_OUTPUT_PATHS,
            strict=True,
        ):
            english = pages[english_path]
            russian = pages[russian_path]
            self.assertIn(
                docs_localization.translation_metadata_line(
                    document_id,
                    english_path,
                    docs_localization.normalized_sha256(english),
                ),
                russian,
            )
            self.assertEqual(
                re.findall(r"```[^\n]*\n(.*?)```", english, re.DOTALL),
                re.findall(r"```[^\n]*\n(.*?)```", russian, re.DOTALL),
            )

    def test_human_guide_routes_protocol_without_promoting_game_schema(self) -> None:
        guide = (ENGINE_ROOT / "Docs/en/how-to/ai-control-protocol.md").read_text(
            encoding="utf-8"
        )

        for heading in (
            "## What the Engine owns",
            "## What the project owns",
            "## Wire protocol",
            "## Command lifecycle",
            "## Security boundary",
            "## Native project integration",
            "## MCP adapter integration",
            "## Validation",
            "## Maintenance",
        ):
            self.assertIn(heading, guide)
        self.assertIn("not a FOnline runtime proof", guide)
        self.assertIn("server authority", guide)
        self.assertIn("Last Frontier", guide)
        self.assertIn("TLA", guide)

    def test_legacy_generated_routes_preserve_headings_and_entry_anchors(self) -> None:
        pages = docs_ai_control_protocol.generate_reference_pages(self.model)

        for filename, _, _ in docs_ai_control_protocol.PAGE_DEFINITIONS:
            canonical = pages[f"Docs/en/reference/ai-control-protocol/{filename}"]
            legacy = pages[f"Docs/generated/ai-control-protocol/{filename}"]
            for heading in re.findall(r"^#{2,3} .+$", canonical, re.MULTILINE):
                self.assertIn(heading, legacy)
            for anchor in re.findall(r'<a id="([^"]+)"></a>', canonical):
                self.assertIn(f'<a id="{anchor}"></a>', legacy)
                self.assertIn(
                    f"../../en/reference/ai-control-protocol/{filename}#{anchor}",
                    legacy,
                )

    def test_manifest_owns_canonical_and_legacy_routes(self) -> None:
        documents = json.loads(
            (ENGINE_ROOT / "Docs/documentation-manifest.json").read_text(
                encoding="utf-8"
            )
        )["documents"]
        guide = documents["Docs/en/how-to/ai-control-protocol.md"]
        legacy_guide = documents["Docs/AiControlProtocol.md"]
        self.assertEqual(
            (guide["id"], guide["state"], guide["disposition"]),
            ("ai-control-protocol-guide", "current", "retain"),
        )
        self.assertEqual(
            (
                legacy_guide["id"],
                legacy_guide["state"],
                legacy_guide["disposition"],
                legacy_guide["redirect_to"],
            ),
            (
                "legacy-ai-control-protocol-guide-route",
                "redirect",
                "replace",
                "ai-control-protocol-guide",
            ),
        )
        for filename, document_id, _ in docs_ai_control_protocol.PAGE_DEFINITIONS:
            canonical = documents[
                f"Docs/en/reference/ai-control-protocol/{filename}"
            ]
            legacy = documents[f"Docs/generated/ai-control-protocol/{filename}"]
            self.assertEqual(
                (canonical["id"], canonical["state"], canonical["disposition"]),
                (document_id, "current", "retain"),
            )
            self.assertEqual(
                (legacy["state"], legacy["disposition"], legacy["redirect_to"]),
                ("redirect", "replace", document_id),
            )

    def test_generated_outputs_are_current_and_ci_routes_the_domain(self) -> None:
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(
            io.StringIO()
        ):
            result = docs_ai_control_protocol.main(
                ["--root", str(ENGINE_ROOT), "--check"]
            )
        self.assertEqual(result, 0)

        workflow = (ENGINE_ROOT / ".github/workflows/validate.yml").read_text(
            encoding="utf-8"
        )
        manifest = json.loads(
            (ENGINE_ROOT / "Docs/documentation-manifest.json").read_text(
                encoding="utf-8"
            )
        )
        contract_diff = (ENGINE_ROOT / "BuildTools/docs_contract_diff.py").read_text(
            encoding="utf-8"
        )
        document_ids = {document["id"] for document in manifest["documents"].values()}

        self.assertIn("BuildTools/tests/test_ai_control_protocol.py", workflow)
        self.assertIn("BuildTools/tests/test_docs_ai_control_protocol.py", workflow)
        self.assertIn("BuildTools/docs_ai_control_protocol.py --check", workflow)
        self.assertIn("ai-control-protocol-guide", document_ids)
        self.assertIn("ai-control-sample-readme", document_ids)
        self.assertIn("generated-ai-control-protocol-index", document_ids)
        self.assertIn('"ai-control-protocol"', contract_diff)
        generated_paths = manifest["generated_artifacts"][
            "ai_control_protocol_reference"
        ]["paths"]
        self.assertTrue(
            set(docs_ai_control_protocol.RUSSIAN_OUTPUT_PATHS).issubset(
                generated_paths
            )
        )


if __name__ == "__main__":
    unittest.main()
