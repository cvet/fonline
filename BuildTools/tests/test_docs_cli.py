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
import docs_description_translations  # noqa: E402
import docs_localization  # noqa: E402


FIXTURE_PARSER = """
import argparse

def create_parser():
    parser = argparse.ArgumentParser(description="Fixture tools")
    subparsers = parser.add_subparsers(dest="command", required=True)
    inspect_parser = subparsers.add_parser("inspect", help="inspect a fixture")
    inspect_parser.add_argument("path", help="fixture path")
    inspect_parser.add_argument("--format", choices=["text", "json"], default="text")
    return parser
"""


def _write_fixture(root: Path) -> None:
    source = root / docs_cli.DEFAULT_SOURCE
    source.parent.mkdir(parents=True, exist_ok=True)
    source.write_text(textwrap.dedent(FIXTURE_PARSER).lstrip(), encoding="utf-8")
    catalog = root / docs_description_translations.DEFAULT_CATALOG
    catalog.parent.mkdir(parents=True, exist_ok=True)
    catalog.write_text(
        json.dumps(
            {
                "schema_version": docs_description_translations.SCHEMA_VERSION,
                "source_locale": "en",
                "target_locale": "ru",
                "enforcement": "registered-translations-current",
                "domains": {},
            }
        )
        + "\n",
        encoding="utf-8",
    )


class DocumentationCliTests(unittest.TestCase):
    def test_current_cli_model_has_stable_shape_ids_and_exact_help(self) -> None:
        model = docs_cli.generate_cli_model(ENGINE_ROOT)

        self.assertEqual(model["schema_version"], 1)
        self.assertEqual(model["generated_by"], "BuildTools/docs_cli.py")
        self.assertEqual(model["program"], "buildtools.py")
        self.assertEqual(model["summary"]["command_count"], 13)
        self.assertEqual(model["summary"]["command_argument_count"], 25)
        self.assertEqual(model["commands"][0]["id"], "cli.buildtools.command.env")
        self.assertEqual(model["commands"][-1]["name"], "prepare-host-workspace")
        self.assertIn("build-auxiliary", {command["name"] for command in model["commands"]})
        self.assertIn("repair-checkout-case", {command["name"] for command in model["commands"]})
        self.assertNotIn("\n", model["usage"])
        prepare_workspace = next(
            command for command in model["commands"] if command["name"] == "prepare-host-workspace"
        )
        parts = next(
            argument for argument in prepare_workspace["arguments"] if argument["destination"] == "features"
        )
        self.assertFalse(parts["required"])

        identities = [
            entry["id"]
            for command in model["commands"]
            for entry in [command, *command["arguments"]]
        ]
        self.assertEqual(len(identities), len(set(identities)))
        self.assertTrue(
            all(argument["description"] for command in model["commands"] for argument in command["arguments"])
        )

        environment = dict(os.environ)
        environment["COLUMNS"] = str(docs_cli.HELP_COLUMNS)
        result = subprocess.run(
            [sys.executable, str(ENGINE_ROOT / docs_cli.DEFAULT_SOURCE), "--help"],
            cwd=ENGINE_ROOT,
            env=environment,
            capture_output=True,
            check=True,
            text=True,
        )
        self.assertEqual(result.stdout.replace("\r\n", "\n"), model["help_output"])
        for command in model["commands"]:
            with self.subTest(command=command["name"]):
                result = subprocess.run(
                    [sys.executable, str(ENGINE_ROOT / docs_cli.DEFAULT_SOURCE), command["name"], "--help"],
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
            self.assertEqual(docs_cli.generate_cli_model(ENGINE_ROOT), model)
        finally:
            if previous_columns is None:
                os.environ.pop("COLUMNS", None)
            else:
                os.environ["COLUMNS"] = previous_columns

    def test_parser_validation_rejects_missing_factory_and_nested_commands(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source = root / docs_cli.DEFAULT_SOURCE
            source.parent.mkdir(parents=True)
            source.write_text("VALUE = 1\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "must expose create_parser"):
                docs_cli.generate_cli_model(root)

            source.write_text(
                "import argparse\n"
                "def create_parser():\n"
                "    parser = argparse.ArgumentParser()\n"
                "    first = parser.add_subparsers(dest='command', required=True)\n"
                "    nested = first.add_parser('nested').add_subparsers(dest='child', required=True)\n"
                "    nested.add_parser('leaf')\n"
                "    return parser\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "Nested subcommands are not supported"):
                docs_cli.generate_cli_model(root)

    def test_reference_pages_are_deterministic_escaped_and_cover_every_id(self) -> None:
        model = docs_cli.generate_cli_model(ENGINE_ROOT)
        pages = docs_cli.generate_reference_pages(model)

        self.assertEqual(tuple(sorted(pages)), tuple(sorted(docs_cli.OUTPUT_PATHS)))
        self.assertEqual(pages, docs_cli.generate_reference_pages(copy.deepcopy(model)))
        commands_page = pages[f"{docs_cli.DEFAULT_OUTPUT_DIR}/commands.md"]
        for command in model["commands"]:
            self.assertIn(command["id"], commands_page)
            self.assertIn(docs_cli._anchor("entry", command["id"]), commands_page)
            for argument in command["arguments"]:
                self.assertIn(argument["id"], commands_page)

        escaped_model = copy.deepcopy(model)
        escaped_model["commands"][0]["arguments"][0]["description"] = "A | B {shape} <unsafe>"
        escaped_page = docs_cli.generate_reference_pages(escaped_model)[
            f"{docs_cli.DEFAULT_OUTPUT_DIR}/commands.md"
        ]
        self.assertIn("A &#124; B &#123;shape&#125; &lt;unsafe&gt;", escaped_page)

        for filename, _, _ in docs_cli.PAGE_DEFINITIONS:
            canonical = pages[f"{docs_cli.DEFAULT_OUTPUT_DIR}/{filename}"]
            legacy = pages[f"{docs_cli.LEGACY_OUTPUT_DIR}/{filename}"]
            self.assertIn(f"../../en/reference/buildtools/{filename}", legacy)
            self.assertIn(f"../../ru/reference/buildtools/{filename}", legacy)
            for heading in re.findall(r"^(#{2,3} .+)$", canonical, flags=re.MULTILINE):
                self.assertIn(heading, legacy)
            for anchor in re.findall(r'<a id="([^"]+)"></a>', canonical):
                self.assertIn(f'<a id="{anchor}"></a>', legacy)

        localized_pages = docs_cli.render_reference_pages(ENGINE_ROOT)
        english_commands = localized_pages[
            f"{docs_cli.DEFAULT_OUTPUT_DIR}/commands.md"
        ]
        russian_commands = localized_pages[
            f"{docs_cli.RUSSIAN_OUTPUT_DIR}/commands.md"
        ]
        self.assertIn("Определить окружение BuildTools.", russian_commands)
        self.assertIn("Синтаксис вывода окружения.", russian_commands)
        self.assertNotIn("resolve BuildTools environment\n\nStable ID", russian_commands)
        self.assertEqual(
            re.findall(r"```text\n(.*?)```", english_commands, flags=re.DOTALL),
            re.findall(r"```text\n(.*?)```", russian_commands, flags=re.DOTALL),
        )
        self.assertIn(
            docs_localization.translation_metadata_line(
                "generated-cli-commands",
                f"{docs_cli.DEFAULT_OUTPUT_DIR}/commands.md",
                docs_localization.normalized_sha256(english_commands),
            ),
            russian_commands,
        )

    def test_cli_write_check_and_stale_detection(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            _write_fixture(root)
            output = io.StringIO()
            with redirect_stdout(output), redirect_stderr(output):
                self.assertEqual(docs_cli.main(["--root", str(root), "--write"]), 0)
                self.assertEqual(docs_cli.main(["--root", str(root), "--check"]), 0)

                stale_page = root / docs_cli.OUTPUT_PATHS[0]
                stale_page.write_text(stale_page.read_text(encoding="utf-8") + "stale\n", encoding="utf-8")
                self.assertEqual(docs_cli.main(["--root", str(root), "--check"]), 1)
            self.assertIn("missing or stale", output.getvalue())


if __name__ == "__main__":
    unittest.main()
