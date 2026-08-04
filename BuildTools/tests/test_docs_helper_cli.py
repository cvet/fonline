from __future__ import annotations

import copy
import io
import json
import os
import re
import subprocess
import sys
import tempfile
import textwrap
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ENGINE_ROOT / "BuildTools"))

import docs_cli  # noqa: E402
import docs_helper_cli  # noqa: E402
import docs_localization  # noqa: E402


FIXTURE_PARSER = """
import argparse

def create_parser():
    parser = argparse.ArgumentParser(prog="fixture-tool.py", description="Fixture helper")
    parser.add_argument("--value", default="default", help="fixture value")
    return parser
"""


def _fixture_manifest() -> dict[str, object]:
    return {
        "schema_version": 1,
        "description": "Fixture helper CLI manifest.",
        "scope": {
            "surface": "helper-cli",
            "stability": "internal",
            "since": None,
            "support_note": "Pin a fixture revision.",
            "included": ["fixture parser"],
            "excluded": ["fixture libraries"],
        },
        "discovery": {
            "root": "BuildTools",
            "excluded_directories": ["tests"],
            "excluded_name_prefixes": ["docs_"],
            "excluded_parser_sources": [],
        },
        "helpers": [
            {
                "id": "helper-cli.fixture-tool",
                "name": "Fixture tool",
                "source": "BuildTools/fixture-tool.py",
                "factory": "create_parser",
                "program": "fixture-tool.py",
                "owner": "quality",
                "audiences": ["engine-contributor"],
                "invocation_owner": "fixture test",
                "description": "Exercise helper CLI generation.",
            }
        ],
    }


def _write_fixture(root: Path) -> None:
    build_tools = root / "BuildTools"
    build_tools.mkdir(parents=True)
    (build_tools / "fixture-tool.py").write_text(textwrap.dedent(FIXTURE_PARSER).lstrip(), encoding="utf-8")
    (root / docs_helper_cli.DEFAULT_MANIFEST).write_text(
        json.dumps(_fixture_manifest(), indent=2) + "\n",
        encoding="utf-8",
    )
    docs = root / "Docs"
    docs.mkdir()
    (docs / "description-translations.ru.json").write_text(
        json.dumps(
            {
                "schema_version": 1,
                "source_locale": "en",
                "target_locale": "ru",
                "enforcement": "registered-translations-current",
                "domains": {},
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )


class DocumentationHelperCliTests(unittest.TestCase):
    def test_current_model_has_complete_inventory_stable_ids_and_exact_help(self) -> None:
        model = docs_helper_cli.generate_helper_cli_model(ENGINE_ROOT)

        self.assertEqual(model["schema_version"], 1)
        self.assertEqual(model["generated_by"], "BuildTools/docs_helper_cli.py")
        self.assertEqual(model["summary"], {
            "helper_count": 9,
            "command_count": 16,
            "global_argument_count": 26,
            "command_argument_count": 48,
        })
        self.assertEqual(model["helpers"][0]["id"], "helper-cli.codegen")
        self.assertEqual(model["helpers"][-1]["id"], "helper-cli.createmsi")
        self.assertIn("helper-cli.windows7-import-check", [helper["id"] for helper in model["helpers"]])
        self.assertIn("helper-cli.ai-control-client", [helper["id"] for helper in model["helpers"]])

        identities = [
            entry["id"]
            for helper in model["helpers"]
            for entry in [
                helper,
                *helper["global_arguments"],
                *[
                    nested
                    for command in helper["commands"]
                    for nested in [command, *command["arguments"]]
                ],
            ]
        ]
        self.assertEqual(len(identities), len(set(identities)))
        self.assertTrue(
            all(
                argument["description"]
                for helper in model["helpers"]
                for argument in [
                    *helper["global_arguments"],
                    *[
                        argument
                        for command in helper["commands"]
                        for argument in command["arguments"]
                    ],
                ]
            )
        )

        environment = dict(os.environ)
        environment["COLUMNS"] = "80"
        for helper in model["helpers"]:
            with self.subTest(helper=helper["id"]):
                result = subprocess.run(
                    [sys.executable, str(ENGINE_ROOT / helper["source"]), "--help"],
                    cwd=ENGINE_ROOT,
                    env=environment,
                    capture_output=True,
                    check=True,
                    text=True,
                )
                self.assertEqual(result.stdout.replace("\r\n", "\n"), helper["help_output"])
            for command in helper["commands"]:
                with self.subTest(helper=helper["id"], command=command["name"]):
                    result = subprocess.run(
                        [sys.executable, str(ENGINE_ROOT / helper["source"]), command["name"], "--help"],
                        cwd=ENGINE_ROOT,
                        env=environment,
                        capture_output=True,
                        check=True,
                        text=True,
                    )
                    self.assertEqual(result.stdout.replace("\r\n", "\n"), command["help_output"])

        previous_columns = os.environ.get("COLUMNS")
        try:
            os.environ["COLUMNS"] = "140"
            self.assertEqual(docs_helper_cli.generate_helper_cli_model(ENGINE_ROOT), model)
        finally:
            if previous_columns is None:
                os.environ.pop("COLUMNS", None)
            else:
                os.environ["COLUMNS"] = previous_columns

    def test_manifest_discovery_rejects_undocumented_and_duplicate_parsers(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            _write_fixture(root)
            undocumented = root / "BuildTools" / "undocumented.py"
            undocumented.write_text(textwrap.dedent(FIXTURE_PARSER).lstrip(), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "undocumented parser sources"):
                docs_helper_cli.generate_helper_cli_model(root)

            undocumented.unlink()
            manifest_path = root / docs_helper_cli.DEFAULT_MANIFEST
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            manifest["helpers"].append(copy.deepcopy(manifest["helpers"][0]))
            manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "duplicate helper CLI id"):
                docs_helper_cli.generate_helper_cli_model(root)

    def test_reference_pages_are_deterministic_escaped_and_cover_every_id(self) -> None:
        model = docs_helper_cli.generate_helper_cli_model(ENGINE_ROOT)
        pages = docs_helper_cli.generate_reference_pages(model)

        self.assertEqual(tuple(sorted(pages)), tuple(sorted(docs_helper_cli.OUTPUT_PATHS)))
        self.assertEqual(pages, docs_helper_cli.generate_reference_pages(copy.deepcopy(model)))
        commands_page = pages[f"{docs_helper_cli.DEFAULT_OUTPUT_DIR}/commands.md"]
        for helper in model["helpers"]:
            self.assertIn(helper["id"], commands_page)
            for argument in helper["global_arguments"]:
                self.assertIn(argument["id"], commands_page)
            for command in helper["commands"]:
                self.assertIn(command["id"], commands_page)
                for argument in command["arguments"]:
                    self.assertIn(argument["id"], commands_page)

        escaped_model = copy.deepcopy(model)
        escaped_model["helpers"][0]["global_arguments"][0]["description"] = "A | B {shape} <unsafe>"
        escaped_page = docs_helper_cli.generate_reference_pages(escaped_model)[
            f"{docs_helper_cli.DEFAULT_OUTPUT_DIR}/commands.md"
        ]
        self.assertIn("A &#124; B &#123;shape&#125; &lt;unsafe&gt;", escaped_page)

        for filename, _, _ in docs_helper_cli.PAGE_DEFINITIONS:
            canonical = pages[f"{docs_helper_cli.DEFAULT_OUTPUT_DIR}/{filename}"]
            legacy = pages[f"{docs_helper_cli.LEGACY_OUTPUT_DIR}/{filename}"]
            self.assertIn(f"../../en/reference/helper-cli/{filename}", legacy)
            self.assertIn(f"../../ru/reference/helper-cli/{filename}", legacy)
            for heading in re.findall(r"^(#{2,3} .+)$", canonical, flags=re.MULTILINE):
                self.assertIn(heading, legacy)
            for anchor in re.findall(r'<a id="([^"]+)"></a>', canonical):
                self.assertIn(f'<a id="{anchor}"></a>', legacy)

    def test_russian_pages_translate_model_prose_and_preserve_exact_help(self) -> None:
        pages = docs_helper_cli.render_reference_pages(ENGINE_ROOT)
        self.assertIn(
            "Сгенерированный справочник вспомогательных CLI",
            pages["Docs/ru/reference/helper-cli/index.md"],
        )
        self.assertIn(
            "исходники встроенных ресурсов",
            pages["Docs/ru/reference/helper-cli/commands.md"],
        )
        self.assertNotIn(
            "Generate engine configuration, metadata registration",
            pages["Docs/ru/reference/helper-cli/commands.md"],
        )
        for (_, document_id, _), english_path, russian_path in zip(
            docs_helper_cli.PAGE_DEFINITIONS,
            docs_helper_cli.CANONICAL_OUTPUT_PATHS,
            docs_helper_cli.RUSSIAN_OUTPUT_PATHS,
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

        manifest = json.loads(
            (ENGINE_ROOT / "Docs/documentation-manifest.json").read_text(
                encoding="utf-8"
            )
        )
        generated_paths = manifest["generated_artifacts"]["helper_cli_reference"][
            "paths"
        ]
        self.assertTrue(
            set(docs_helper_cli.RUSSIAN_OUTPUT_PATHS).issubset(generated_paths)
        )

    def test_write_check_and_stale_detection(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            _write_fixture(root)
            output = io.StringIO()
            with redirect_stdout(output), redirect_stderr(output):
                self.assertEqual(docs_helper_cli.main(["--root", str(root), "--write"]), 0)
                self.assertEqual(docs_helper_cli.main(["--root", str(root), "--check"]), 0)

                stale_page = root / docs_helper_cli.OUTPUT_PATHS[0]
                stale_page.write_text(stale_page.read_text(encoding="utf-8") + "stale\n", encoding="utf-8")
                self.assertEqual(docs_helper_cli.main(["--root", str(root), "--check"]), 1)
            self.assertIn("missing or stale", output.getvalue())


if __name__ == "__main__":
    unittest.main()
