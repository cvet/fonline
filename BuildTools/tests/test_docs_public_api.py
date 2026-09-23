from __future__ import annotations

import contextlib
import io
import json
import re
import shutil
import sys
import tempfile
import unittest
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
ENGINE_ROOT = BUILDTOOLS_DIR.parent
sys.path.insert(0, str(BUILDTOOLS_DIR))

import docs_contract_diff  # noqa: E402
import docs_public_api  # noqa: E402


class PublicApiDocumentationTests(unittest.TestCase):
    def test_render_uses_every_live_contract_model(self) -> None:
        rendered = docs_public_api.render_public_api(ENGINE_ROOT)
        self.assertNotIn("Placeholder route", rendered)
        self.assertIn("does not define annotation spelling", rendered)
        self.assertIn("do not invent either", rendered)
        for domain in docs_contract_diff.DOMAIN_ORDER:
            self.assertEqual(
                rendered.count(f"| {docs_public_api.DOMAIN_TITLES[domain]} |"),
                1,
            )
            model_path = (
                Path("Docs/generated") / docs_contract_diff.MODEL_FILES[domain]
            ).as_posix()
            index_path = docs_public_api.SOURCE_REFERENCES[domain]
            model_link = docs_public_api._relative_target(
                docs_public_api.DEFAULT_OUTPUT,
                model_path,
            )
            index_link = docs_public_api._relative_target(
                docs_public_api.DEFAULT_OUTPUT,
                index_path,
            )
            self.assertEqual(rendered.count(f"]({model_link})"), 1)
            self.assertEqual(rendered.count(f"]({index_link})"), 1)

    def test_locale_set_and_legacy_route_are_complete(self) -> None:
        pages = docs_public_api.generate_public_api_pages(ENGINE_ROOT)
        self.assertEqual(set(pages), set(docs_public_api.OUTPUT_PATHS))

        english = pages[docs_public_api.DEFAULT_OUTPUT]
        russian = pages[docs_public_api.RUSSIAN_OUTPUT]
        legacy = pages[docs_public_api.LEGACY_OUTPUT]
        self.assertIn("locale: en", english)
        self.assertIn("locale: ru", russian)
        self.assertIn("docs-translation:", russian)
        self.assertIn("# Индекс публичных контрактов FOnline", russian)
        self.assertIn("| Нативный script API |", russian)
        self.assertIn("../script-api/index.md", russian)
        self.assertIn(f"]({docs_public_api.DEFAULT_OUTPUT})", legacy)
        self.assertIn(f"]({docs_public_api.RUSSIAN_OUTPUT})", legacy)
        for heading in re.findall(r"^(#{2,3} .+)$", english, flags=re.MULTILINE):
            self.assertIn(heading, legacy)
        for anchor in re.findall(r'<a id="([^"]+)"></a>', english):
            self.assertIn(f'<a id="{anchor}"></a>', legacy)

    def test_native_status_comes_from_live_api_model(self) -> None:
        api_model = json.loads(
            (ENGINE_ROOT / "Docs/generated/api.json").read_text(encoding="utf-8")
        )
        summary = api_model["summary"]
        rendered = docs_public_api.render_public_api(ENGINE_ROOT)
        for field in (
            "symbol_count",
            "described_symbol_count",
            "missing_description_count",
            "explicit_contract_symbol_count",
            "default_contract_symbol_count",
        ):
            self.assertIn(f"**{summary[field]}**", rendered)

    def test_checked_in_output_is_current(self) -> None:
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(
            io.StringIO()
        ):
            result = docs_public_api.main(["--root", str(ENGINE_ROOT), "--check"])
        self.assertEqual(result, 0)

    def test_write_check_and_stale_detection(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            shutil.copytree(ENGINE_ROOT / "Docs/generated", root / "Docs/generated")
            for reference_path in docs_public_api.SOURCE_REFERENCES.values():
                if reference_path.startswith("Docs/generated/"):
                    continue
                target = root / reference_path
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(ENGINE_ROOT / reference_path, target)
            output = "Workspace/PUBLIC_API.md"
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(
                io.StringIO()
            ):
                self.assertEqual(
                    docs_public_api.main(
                        ["--root", str(root), "--output", output, "--write"]
                    ),
                    0,
                )
                self.assertEqual(
                    docs_public_api.main(
                        ["--root", str(root), "--output", output, "--check"]
                    ),
                    0,
                )
                (root / output).write_text("stale\n", encoding="utf-8")
                self.assertEqual(
                    docs_public_api.main(
                        ["--root", str(root), "--output", output, "--check"]
                    ),
                    1,
                )

    def test_invalid_model_schema_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            shutil.copytree(ENGINE_ROOT / "Docs/generated", root / "Docs/generated")
            for reference_path in docs_public_api.SOURCE_REFERENCES.values():
                if reference_path.startswith("Docs/generated/"):
                    continue
                target = root / reference_path
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(ENGINE_ROOT / reference_path, target)
            model_path = root / "Docs/generated/cmake.json"
            model = json.loads(model_path.read_text(encoding="utf-8"))
            model["schema_version"] = -1
            model_path.write_text(json.dumps(model), encoding="utf-8")
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(
                io.StringIO()
            ):
                result = docs_public_api.main(
                    ["--root", str(root), "--output", "out.md", "--write"]
                )
            self.assertEqual(result, 1)

    def test_ci_checks_generator_and_test(self) -> None:
        workflow = (ENGINE_ROOT / ".github/workflows/validate.yml").read_text(
            encoding="utf-8"
        )
        self.assertIn("BuildTools/tests/test_docs_public_api.py", workflow)
        self.assertIn("BuildTools/docs_public_api.py --check", workflow)


if __name__ == "__main__":
    unittest.main()
