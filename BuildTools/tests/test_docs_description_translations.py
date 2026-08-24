from __future__ import annotations

import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
ENGINE_ROOT = BUILDTOOLS_DIR.parent
sys.path.insert(0, str(BUILDTOOLS_DIR))

import docs_description_translations  # noqa: E402
import docs_localization  # noqa: E402


def _catalog(entries: dict[str, object] | None = None) -> dict[str, object]:
    return {
        "schema_version": docs_description_translations.SCHEMA_VERSION,
        "source_locale": "en",
        "target_locale": "ru",
        "enforcement": "registered-translations-current",
        "domains": (
            {"package": {"entries": entries}}
            if entries is not None
            else {}
        ),
    }


def _write_catalog(root: Path, catalog: dict[str, object]) -> None:
    path = root / docs_description_translations.DEFAULT_CATALOG
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(catalog, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


class DocumentationDescriptionTranslationTests(unittest.TestCase):
    def test_current_inventory_is_stable_and_all_domains_are_complete(self) -> None:
        first = docs_description_translations.generate_status(ENGINE_ROOT)
        second = docs_description_translations.generate_status(ENGINE_ROOT)

        self.assertEqual(first, second)
        self.assertEqual(first["summary"]["domain_count"], 20)
        self.assertEqual(first["enforcement"], "complete")
        self.assertEqual(first["summary"]["entry_count"], 4948)
        self.assertEqual(first["summary"]["current_count"], 4948)
        self.assertEqual(first["summary"]["missing_count"], 0)
        self.assertTrue(first["summary"]["complete"])
        for domain, count in (
            ("ai-control-protocol", 134),
            ("api", 2497),
            ("audio", 103),
            ("cli", 42),
            ("cmake", 64),
            ("effect-format", 157),
            ("font-format", 187),
            ("gui-runtime", 199),
            ("helper-cli", 125),
            ("image-format", 154),
            ("map-format", 207),
            ("model-format", 138),
            ("native-extension", 44),
            ("package", 62),
            ("particle-format", 339),
            ("prototype-format", 188),
            ("public-examples", 13),
            ("support-matrix", 76),
            ("text-format", 113),
            ("video", 106),
        ):
            with self.subTest(domain=domain):
                status = first["domains"][domain]
                self.assertEqual(status["entry_count"], count)
                self.assertEqual(status["current_count"], count)
                self.assertTrue(status["complete"])
                self.assertTrue(
                    all("@index=" not in entry["locator"] for entry in status["entries"])
                )
        self.assertEqual(
            sum(
                1
                for entry in first["domains"]["api"]["entries"]
                if "translation_source_locator" in entry
            ),
            133,
        )

    def test_application_uses_copy_and_translates_strings_and_lists(self) -> None:
        package = json.loads(
            (ENGINE_ROOT / "Docs/generated/package.json").read_text(encoding="utf-8")
        )
        original = copy.deepcopy(package)
        translated_package = docs_description_translations.apply_translations(
            ENGINE_ROOT,
            "package",
            package,
        )
        self.assertEqual(package, original)
        self.assertEqual(
            translated_package["declaration"]["description"],
            "Объявляет именованную цель пакета и содержимое одного или нескольких бинарных файлов.",
        )

        support = json.loads(
            (ENGINE_ROOT / "Docs/generated/support-matrix.json").read_text(encoding="utf-8")
        )
        translated_support = docs_description_translations.apply_translations(
            ENGINE_ROOT,
            "support-matrix",
            support,
        )
        windows = next(
            entry
            for entry in translated_support["platforms"]
            if entry["id"] == "windows-x64-msvc"
        )
        self.assertEqual(windows["compiler"], "MSVC 19.44 или новее")
        self.assertIn("настольный и headless-клиент", windows["applications"])

        examples = json.loads(
            (ENGINE_ROOT / "Docs/generated/public-examples.json").read_text(
                encoding="utf-8"
            )
        )
        translated_examples = docs_description_translations.apply_translations(
            ENGINE_ROOT,
            "public-examples",
            examples,
        )
        self.assertEqual(
            translated_examples["program"]["owners"]["documentation"],
            "Ответственные за документацию",
        )

    def test_list_reordering_does_not_change_locators(self) -> None:
        model = {
            "entries": [
                {"id": "second", "description": "Second description."},
                {"id": "first", "description": "First description."},
            ]
        }
        reordered = {"entries": list(reversed(model["entries"]))}
        expected = {
            "/entries/@id=first/description": "First description.",
            "/entries/@id=second/description": "Second description.",
        }
        self.assertEqual(
            docs_description_translations.inventory_model("package", model),
            expected,
        )
        self.assertEqual(
            docs_description_translations.inventory_model("package", reordered),
            expected,
        )

    def test_exact_source_translation_memory_reuses_only_unambiguous_reviewed_text(self) -> None:
        model = {
            "entries": [
                {"id": "owner", "description": "Shared contract."},
                {"id": "reuse", "description": "Shared contract."},
            ]
        }
        source_hash = docs_localization.normalized_sha256("Shared contract.")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            _write_catalog(
                root,
                _catalog(
                    {
                        "/entries/@id=owner/description": {
                            "source_sha256": source_hash,
                            "translation": "Общий контракт.",
                        }
                    }
                ),
            )
            translated = docs_description_translations.apply_translations(
                root,
                "package",
                model,
            )
            self.assertEqual(
                [entry["description"] for entry in translated["entries"]],
                ["Общий контракт.", "Общий контракт."],
            )

            ambiguous_model = {
                "entries": model["entries"]
                + [{"id": "other", "description": "Shared contract."}]
            }
            _write_catalog(
                root,
                _catalog(
                    {
                        "/entries/@id=owner/description": {
                            "source_sha256": source_hash,
                            "translation": "Общий контракт.",
                        },
                        "/entries/@id=other/description": {
                            "source_sha256": source_hash,
                            "translation": "Совместный контракт.",
                        },
                    }
                ),
            )
            translated = docs_description_translations.apply_translations(
                root,
                "package",
                ambiguous_model,
            )
            reuse = next(entry for entry in translated["entries"] if entry["id"] == "reuse")
            self.assertEqual(reuse["description"], "Shared contract.")

    def test_stale_unknown_and_code_damaging_entries_fail(self) -> None:
        model = {"description": "Use `ExactName` here."}
        locator = "/description"
        source_hash = docs_localization.normalized_sha256(model["description"])
        cases = (
            (
                {locator: {"source_sha256": "0" * 64, "translation": "Используйте `ExactName`."}},
                "stale generated-description translation",
            ),
            (
                {"/unknown": {"source_sha256": source_hash, "translation": "Перевод."}},
                "unknown locators",
            ),
            (
                {locator: {"source_sha256": source_hash, "translation": "Используйте ExactName."}},
                "preserve inline code spans",
            ),
        )
        for entries, error in cases:
            with self.subTest(error=error), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                _write_catalog(root, _catalog(entries))
                with self.assertRaisesRegex(ValueError, error):
                    docs_description_translations.apply_translations(
                        root,
                        "package",
                        model,
                    )

    def test_duplicate_catalog_keys_fail(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            path = root / docs_description_translations.DEFAULT_CATALOG
            path.parent.mkdir(parents=True)
            path.write_text(
                '{"schema_version":1,"source_locale":"en","target_locale":"ru",'
                '"enforcement":"registered-translations-current","domains":{},"domains":{}}\n',
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "duplicate JSON key: domains"):
                docs_description_translations.load_catalog(root)

    def test_write_check_and_complete_enforcement(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            _write_catalog(root, _catalog())
            for model_path in docs_description_translations.MODEL_PATHS.values():
                path = root / model_path
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("{}\n", encoding="utf-8")

            self.assertEqual(
                docs_description_translations.main(
                    ["--root", str(root), "--write"]
                ),
                0,
            )
            self.assertEqual(
                docs_description_translations.main(
                    ["--root", str(root), "--check"]
                ),
                0,
            )
            output = root / docs_description_translations.DEFAULT_OUTPUT
            output.write_text("stale\n", encoding="utf-8")
            self.assertEqual(
                docs_description_translations.main(
                    ["--root", str(root), "--check"]
                ),
                1,
            )

            package_path = root / docs_description_translations.MODEL_PATHS["package"]
            package_path.write_text(
                json.dumps({"description": "Untranslated."}) + "\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "translations are incomplete"):
                docs_description_translations.generate_status(
                    root,
                    enforce_complete=True,
                )


if __name__ == "__main__":
    unittest.main()
