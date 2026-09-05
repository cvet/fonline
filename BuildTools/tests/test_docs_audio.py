from __future__ import annotations

import contextlib
import copy
import io
import json
import re
import sys
import tempfile
import unittest
from pathlib import Path


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
ENGINE_ROOT = BUILDTOOLS_DIR.parent
sys.path.insert(0, str(BUILDTOOLS_DIR))

import docs_audio  # noqa: E402
import docs_localization  # noqa: E402


class AudioDocumentationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.model = docs_audio.generate_audio_model(ENGINE_ROOT)

    def test_model_is_deterministic_and_derives_live_contract(self) -> None:
        second = docs_audio.generate_audio_model(ENGINE_ROOT)

        self.assertEqual(self.model, second)
        self.assertEqual(self.model["schema_version"], docs_audio.SCHEMA_VERSION)
        outputs = self.model["outputs"]
        self.assertEqual(outputs["indexed_extensions"], ["wav", "acm", "ogg"])
        self.assertEqual(outputs["decoder_extensions"], ["wav", "acm", "ogg"])
        self.assertEqual(outputs["default_extension"], "acm")
        self.assertEqual(outputs["wav"]["sample_bits"], [8, 16])
        self.assertEqual(outputs["acm"]["sample_rate"], 22050)
        self.assertEqual(outputs["ogg"]["native_stream_chunk_bytes"], 65536)
        self.assertEqual(outputs["ogg"]["web_stream_chunk_bytes"], 131072)
        self.assertEqual(outputs["mix_volume_range"], [0, 100])
        self.assertFalse(outputs["headless_audio_enabled"])
        self.assertFalse(outputs["unsupported_extension_rejected"])
        self.assertEqual(outputs["native_test_files"], [])
        self.assertEqual(self.model["summary"]["entry_count"], 32)

    def test_runtime_formats_are_present_in_raw_copy_defaults(self) -> None:
        outputs = self.model["outputs"]
        self.assertLessEqual(
            set(outputs["decoder_extensions"]), set(outputs["raw_copy_extensions"])
        )
        self.assertEqual(
            outputs["audio_settings"],
            {"DisableAudio": False, "SoundVolume": 100, "MusicVolume": 100},
        )

    def test_unsupported_extension_limitation_is_explicit(self) -> None:
        source = (ENGINE_ROOT / "Source/Client/SoundManager.cpp").read_text(
            encoding="utf-8"
        )

        load_begin = source.index("auto SoundManager::Load(")
        load_end = source.index("auto SoundManager::LoadWav", load_begin)
        load = source[load_begin:load_end]
        enqueue = load.index("_playingSounds.emplace_back")
        self.assertNotIn("Unsupported sound format", load)
        self.assertIn("if (ext == \"wav\"", load[:enqueue])
        self.assertIn("if (ext == \"acm\"", load[:enqueue])
        self.assertIn("if (ext == \"ogg\"", load[:enqueue])

    def test_effect_identity_variants_and_music_path_are_pinned(self) -> None:
        sound = (ENGINE_ROOT / "Source/Client/SoundManager.cpp").read_text(
            encoding="utf-8"
        )
        resources = (ENGINE_ROOT / "Source/Client/ResourceManager.cpp").read_text(
            encoding="utf-8"
        )

        self.assertIn('{"wav", "acm", "ogg"}', resources)
        self.assertIn("erase_file_extension().lower()", resources)
        self.assertIn("erase_file_extension().lower()", sound)
        self.assertIn("sound_name, count + 1", sound)
        self.assertIn("_randomGenerator.next_between(1, count)", sound)
        self.assertIn("return Load(fname, true, repeat_time)", sound)

    def test_manifest_entries_keep_live_source_anchors(self) -> None:
        for collection in docs_audio.COLLECTION_KINDS:
            for entry in self.model[collection]:
                for source in entry["source"]:
                    source_text = (ENGINE_ROOT / source["path"]).read_text(
                        encoding="utf-8", errors="replace"
                    )
                    for anchor in source["anchors"]:
                        self.assertIn(anchor, source_text)

    def test_human_guide_covers_authoring_runtime_and_boundaries(self) -> None:
        guide = (
            ENGINE_ROOT / "Docs/en/how-to/content/audio.md"
        ).read_text(encoding="utf-8")

        for heading in (
            "## Supported resources",
            "## Delivering audio",
            "## Playing effects",
            "### Numbered variants",
            "## Playing music",
            "## Repeat timing",
            "## Format details",
            "## Device conversion and mixing",
            "## Disabled and headless behavior",
            "## Recommended project practice",
            "## Diagnostics",
            "## Validation workflow",
            "## Project boundary",
            "## Maintenance",
        ):
            self.assertIn(heading, guide)
        self.assertIn("not a resource-existence check", guide)
        self.assertIn("first missing number", guide)
        self.assertIn("no focused native `SoundManager`", guide)
        self.assertIn("licenses", guide)

    def test_changed_derived_manifest_values_are_rejected(self) -> None:
        manifest = json.loads(
            (ENGINE_ROOT / docs_audio.DEFAULT_MANIFEST).read_text(encoding="utf-8")
        )
        manifest["outputs"] = copy.deepcopy(manifest["outputs"])
        manifest["outputs"]["ogg"]["native_stream_chunk_bytes"] = 32768

        with tempfile.TemporaryDirectory() as temporary_directory:
            manifest_path = Path(temporary_directory) / "AudioInterface.json"
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            with self.assertRaisesRegex(
                ValueError, "outputs.ogg must match the live source"
            ):
                docs_audio.generate_audio_model(ENGINE_ROOT, str(manifest_path))

    def test_generated_pages_and_checked_outputs_are_current(self) -> None:
        pages = docs_audio.render_reference_pages(ENGINE_ROOT)

        self.assertEqual(set(pages), set(docs_audio.OUTPUT_PATHS))
        self.assertIn(
            "Runtime formats | <code>.wav</code>, <code>.acm</code>, "
            "<code>.ogg</code>",
            pages["Docs/en/reference/audio/index.md"],
        )
        self.assertIn("audio.format.wav", pages["Docs/en/reference/audio/formats.md"])
        self.assertIn(
            "Duplicate-stem precedence", pages["Docs/en/reference/audio/delivery.md"]
        )
        self.assertIn("Ogg streaming", pages["Docs/en/reference/audio/decoding.md"])
        self.assertIn(
            "Contiguous numbered variants",
            pages["Docs/en/reference/audio/playback.md"],
        )
        self.assertIn(
            "no focused native decoder/playback fixture",
            pages["Docs/en/reference/audio/validation.md"],
        )
        self.assertIn(
            "Доставка raw copy",
            pages["Docs/ru/reference/audio/delivery.md"],
        )
        self.assertIn(
            "Устаревшие эффекты и музыка, полностью декодируемые",
            pages["Docs/ru/reference/audio/formats.md"],
        )
        self.assertIn(
            "Сейчас нет сфокусированного нативного fixture",
            pages["Docs/ru/reference/audio/validation.md"],
        )
        self.assertNotIn(
            "Raw-copy delivery",
            pages["Docs/ru/reference/audio/delivery.md"],
        )
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(
            io.StringIO()
        ):
            result = docs_audio.main(["--root", str(ENGINE_ROOT), "--check"])
        self.assertEqual(result, 0)

    def test_russian_pages_pin_english_hashes_and_preserve_commands(self) -> None:
        pages = docs_audio.render_reference_pages(ENGINE_ROOT)
        for (filename, document_id, _), english_path, russian_path in zip(
            docs_audio.PAGE_DEFINITIONS,
            docs_audio.CANONICAL_OUTPUT_PATHS,
            docs_audio.RUSSIAN_OUTPUT_PATHS,
            strict=True,
        ):
            del filename
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

    def test_legacy_generated_routes_preserve_headings_and_entry_anchors(self) -> None:
        pages = docs_audio.generate_reference_pages(self.model)
        for canonical_path, legacy_path in zip(
            docs_audio.CANONICAL_OUTPUT_PATHS,
            docs_audio.LEGACY_OUTPUT_PATHS,
            strict=True,
        ):
            canonical = pages[canonical_path]
            legacy = pages[legacy_path]
            self.assertEqual(
                re.findall(r'<a id="([^"]+)"></a>', canonical),
                re.findall(r'<a id="([^"]+)"></a>', legacy),
            )
            for heading in re.findall(r"^#{2,3} .+$", canonical, re.MULTILINE):
                self.assertIn(heading, legacy)
            self.assertIn("../../en/reference/audio/", legacy)
            self.assertIn("../../ru/reference/audio/", legacy)

    def test_ci_manifest_and_contract_diff_route_the_domain(self) -> None:
        workflow = (ENGINE_ROOT / ".github/workflows/validate.yml").read_text(
            encoding="utf-8"
        )
        manifest = json.loads(
            (ENGINE_ROOT / "Docs/documentation-manifest.json").read_text(
                encoding="utf-8"
            )
        )
        contract_diff = (
            ENGINE_ROOT / "BuildTools/docs_contract_diff.py"
        ).read_text(encoding="utf-8")
        validate = (ENGINE_ROOT / "BuildTools/docs_validate.py").read_text(
            encoding="utf-8"
        )

        self.assertIn("BuildTools/tests/test_docs_audio.py", workflow)
        self.assertIn("BuildTools/docs_audio.py --check", workflow)
        document_ids = {
            document["id"] for document in manifest["documents"].values()
        }
        self.assertIn("audio-guide", document_ids)
        self.assertIn("generated-audio-index", document_ids)
        guide = manifest["documents"]["Docs/en/how-to/content/audio.md"]
        self.assertEqual(guide["disposition"], "retain")
        legacy_guide = manifest["documents"]["Docs/Audio.md"]
        self.assertEqual(legacy_guide["state"], "redirect")
        self.assertEqual(legacy_guide["redirect_to"], "audio-guide")
        for canonical_path, legacy_path in zip(
            docs_audio.CANONICAL_OUTPUT_PATHS,
            docs_audio.LEGACY_OUTPUT_PATHS,
            strict=True,
        ):
            canonical = manifest["documents"][canonical_path]
            legacy = manifest["documents"][legacy_path]
            self.assertEqual(canonical["state"], "current")
            self.assertEqual(canonical["disposition"], "retain")
            self.assertEqual(legacy["state"], "redirect")
            self.assertEqual(legacy["redirect_to"], canonical["id"])
        generated_paths = manifest["generated_artifacts"]["audio_reference"]["paths"]
        self.assertTrue(set(docs_audio.RUSSIAN_OUTPUT_PATHS).issubset(generated_paths))
        self.assertIn('"audio"', contract_diff)
        self.assertIn("docs_audio", validate)


if __name__ == "__main__":
    unittest.main()
