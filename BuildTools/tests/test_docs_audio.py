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
        self.assertEqual(outputs["indexed_extensions"], ["wav", "ogg"])
        self.assertEqual(outputs["baker_extensions"], ["wav", "ogg"])
        self.assertEqual(outputs["decoder_extensions"], ["ogg"])
        self.assertFalse(outputs["missing_suffix_fallback"])
        self.assertEqual(outputs["wav"]["format_tags"], [1, 3])
        self.assertEqual(outputs["wav"]["sample_bits"], [8, 16, 24, 32])
        self.assertEqual(outputs["ogg"]["native_stream_chunk_bytes"], 65536)
        self.assertEqual(outputs["ogg"]["web_stream_chunk_bytes"], 131072)
        self.assertEqual(outputs["mix_volume_range"], [0, 100])
        self.assertTrue(outputs["play_sound_returns_handle"])
        self.assertTrue(outputs["placed_sound_updates"])
        self.assertFalse(outputs["headless_audio_enabled"])
        self.assertTrue(outputs["unsupported_extension_rejected"])
        self.assertTrue(outputs["authored_extension_preserved"])
        self.assertEqual(
            outputs["native_test_files"],
            [
                "Source/Tests/Test_AudioBaker.cpp",
                "Source/Tests/Test_AudioManager.cpp",
            ],
        )
        self.assertEqual(self.model["summary"]["entry_count"], 31)

    def test_audio_uses_the_audio_baker_instead_of_raw_copy(self) -> None:
        outputs = self.model["outputs"]
        self.assertTrue(
            set(outputs["baker_extensions"]).isdisjoint(outputs["raw_copy_extensions"])
        )
        self.assertEqual(
            outputs["audio_settings"],
            {"DisableAudio": False, "SoundVolume": 100, "MusicVolume": 100},
        )

    def test_baker_and_runtime_decoder_boundary_is_explicit(self) -> None:
        baker = (ENGINE_ROOT / "Source/Tools/AudioBaker.cpp").read_text(
            encoding="utf-8"
        )
        manager = (ENGINE_ROOT / "Source/Client/AudioManager.cpp").read_text(
            encoding="utf-8"
        )

        self.assertIn('{"wav"}', baker)
        self.assertIn("ext == NATIVE_EXTENSION", baker)
        self.assertIn("VerifyVorbisStream", baker)
        self.assertIn("ov_open_callbacks", manager)
        self.assertNotIn('ext == "wav"', manager)

    def test_exact_paths_handles_and_placement_are_pinned(self) -> None:
        audio = (ENGINE_ROOT / "Source/Client/AudioManager.cpp").read_text(
            encoding="utf-8"
        )
        scripts = (
            ENGINE_ROOT / "Source/Scripting/ClientGlobalScriptMethods.cpp"
        ).read_text(
            encoding="utf-8"
        )

        self.assertIn("_resources->ReadFile(fname)", audio)
        self.assertIn("auto AudioManager::PlaySound", audio)
        self.assertIn("auto AudioManager::UpdateSound", audio)
        self.assertIn("return Load(fname, true, repeat_time, 1.0f, 0.0f) != 0", audio)
        self.assertIn("Client_Game_UpdateSound", scripts)

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
        self.assertIn("exact baked resource path", guide)
        self.assertIn("`Game.UpdateSound`", guide)
        self.assertIn("`Test_AudioBaker`", guide)
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
            "Runtime formats | <code>.ogg</code>",
            pages["Docs/en/reference/audio/index.md"],
        )
        self.assertIn("audio.format.wav", pages["Docs/en/reference/audio/formats.md"])
        self.assertIn(
            "Source extension and payload separation",
            pages["Docs/en/reference/audio/delivery.md"],
        )
        self.assertIn("Ogg streaming", pages["Docs/en/reference/audio/decoding.md"])
        self.assertIn(
            "Placed sound update",
            pages["Docs/en/reference/audio/playback.md"],
        )
        self.assertIn(
            "Test_AudioBaker and Test_AudioManager",
            pages["Docs/en/reference/audio/validation.md"],
        )
        self.assertIn(
            "Доставка через AudioBaker",
            pages["Docs/ru/reference/audio/delivery.md"],
        )
        self.assertIn(
            "Авторский вход, преобразуемый",
            pages["Docs/ru/reference/audio/formats.md"],
        )
        self.assertIn(
            "Test_AudioBaker и Test_AudioManager",
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
