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

import docs_video  # noqa: E402
import docs_localization  # noqa: E402


class VideoDocumentationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.model = docs_video.generate_video_model(ENGINE_ROOT)

    def test_model_is_deterministic_and_derives_live_contract(self) -> None:
        second = docs_video.generate_video_model(ENGINE_ROOT)

        self.assertEqual(self.model, second)
        self.assertEqual(self.model["schema_version"], docs_video.SCHEMA_VERSION)
        outputs = self.model["outputs"]
        self.assertEqual(outputs["resource_extension"], "ogv")
        self.assertEqual(outputs["container"], "Ogg")
        self.assertEqual(outputs["codec"], "Theora")
        self.assertEqual(outputs["read_chunk_bytes"], 1024)
        self.assertEqual(outputs["max_logical_streams"], 10)
        self.assertEqual(
            outputs["pixel_formats"],
            ["TH_PF_420", "TH_PF_422", "TH_PF_444"],
        )
        self.assertEqual(outputs["output_channels"], 4)
        self.assertEqual(outputs["output_alpha"], 255)
        self.assertEqual(outputs["native_test_files"], [])
        self.assertEqual(self.model["summary"]["entry_count"], 34)

    def test_delivery_and_decoder_boundaries_are_explicit(self) -> None:
        outputs = self.model["outputs"]

        self.assertTrue(outputs["whole_resource_buffered"])
        self.assertFalse(outputs["container_audio_decoded"])
        self.assertEqual(outputs["fullscreen_path_separator"], "|")
        self.assertEqual(outputs["runtime_side"], "client")
        delivery_ids = {
            entry["id"] for entry in self.model["delivery_rules"]
        }
        decoding_ids = {
            entry["id"] for entry in self.model["decoding_rules"]
        }
        self.assertIn("video.delivery.memory-budget", delivery_ids)
        self.assertIn("video.decoding.no-container-audio", decoding_ids)

    def test_fullscreen_semantics_are_pinned(self) -> None:
        source = (ENGINE_ROOT / "Source/Client/Client.cpp").read_text(
            encoding="utf-8"
        )

        replace = source.index("_videoQueue.clear()")
        load = source.index("Resources.ReadFile(names.front())", replace)
        self.assertLess(replace, load)
        self.assertIn("if (_video && enqueue)", source)
        self.assertIn("SndMngr.StopMusic()", source)
        self.assertIn("OnRenderIface.Fire()", source)
        self.assertIn("SprMngr.DrawTexture(_video->Tex, false)", source)
        self.assertTrue(self.model["outputs"]["fullscreen_stretches_to_target"])

    def test_embedded_semantics_are_pinned(self) -> None:
        source = (
            ENGINE_ROOT / "Source/Scripting/ClientGlobalScriptMethods.cpp"
        ).read_text(encoding="utf-8")

        self.assertIn('throw ScriptException("Video file not found"', source)
        self.assertIn("only in RenderIface event", source)
        self.assertIn("if (size.width > 0 && size.height > 0)", source)
        self.assertIn(
            "irect32 r = {pos.x, pos.y, size.width, size.height}", source
        )
        self.assertIn("video->PlaybackResources.reset()", source)
        self.assertIn("video->Stopped = true", source)

    def test_manifest_entries_keep_live_source_anchors(self) -> None:
        for collection in docs_video.COLLECTION_KINDS:
            for entry in self.model[collection]:
                for source in entry["source"]:
                    source_text = (ENGINE_ROOT / source["path"]).read_text(
                        encoding="utf-8", errors="replace"
                    )
                    for anchor in source["anchors"]:
                        self.assertIn(anchor, source_text)

    def test_human_guide_covers_authoring_runtime_and_boundaries(self) -> None:
        guide = (
            ENGINE_ROOT / "Docs/en/how-to/content/video.md"
        ).read_text(encoding="utf-8")

        for heading in (
            "## Contract status",
            "## Delivering video",
            "## Authoring requirements",
            "## Memory and performance",
            "## Fullscreen playback",
            "### Queue and replacement",
            "### Interruption",
            "### Separate music",
            "### Drawing and aspect",
            "## Embedded playback",
            "## Looping",
            "## Diagnostics",
            "## Validation workflow",
            "## Project boundary",
            "## Maintenance",
        ):
            self.assertIn(heading, guide)
        self.assertIn("does not decode audio", guide)
        self.assertIn("not stream", guide)
        self.assertIn("experimental", guide)
        self.assertIn("no focused native video", guide)
        self.assertIn("subtitles", guide)
        self.assertIn("provenance", guide)

    def test_changed_derived_manifest_values_are_rejected(self) -> None:
        manifest = json.loads(
            (ENGINE_ROOT / docs_video.DEFAULT_MANIFEST).read_text(encoding="utf-8")
        )
        manifest["outputs"] = copy.deepcopy(manifest["outputs"])
        manifest["outputs"]["read_chunk_bytes"] = 2048

        with tempfile.TemporaryDirectory() as temporary_directory:
            manifest_path = Path(temporary_directory) / "VideoInterface.json"
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            with self.assertRaisesRegex(
                ValueError, "outputs.read_chunk_bytes must match the live source"
            ):
                docs_video.generate_video_model(
                    ENGINE_ROOT, str(manifest_path)
                )

    def test_generated_pages_and_checked_outputs_are_current(self) -> None:
        pages = docs_video.render_reference_pages(ENGINE_ROOT)

        self.assertEqual(set(pages), set(docs_video.OUTPUT_PATHS))
        self.assertIn(
            ".ogv / Ogg / Theora",
            pages["Docs/en/reference/video/index.md"],
        )
        self.assertIn(
            "video.format.theora", pages["Docs/en/reference/video/formats.md"]
        )
        self.assertIn(
            "Whole-resource memory budget",
            pages["Docs/en/reference/video/delivery.md"],
        )
        self.assertIn(
            "No container-audio decode",
            pages["Docs/en/reference/video/decoding.md"],
        )
        self.assertIn(
            "Completion stops music",
            pages["Docs/en/reference/video/fullscreen.md"],
        )
        self.assertIn(
            "RenderIface-only drawing",
            pages["Docs/en/reference/video/embedded.md"],
        )
        self.assertIn(
            "no focused native video decoder/playback fixture",
            pages["Docs/en/reference/video/validation.md"],
        )
        self.assertIn(
            "Приёмка встраиваемого проекта",
            pages["Docs/ru/reference/video/validation.md"],
        )
        self.assertIn(
            "Текущий CPU-декодируемый тракт Ogg/Theora",
            pages["Docs/ru/reference/video/index.md"],
        )
        self.assertNotIn(
            "Whole-resource memory budget",
            pages["Docs/ru/reference/video/delivery.md"],
        )
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(
            io.StringIO()
        ):
            result = docs_video.main(["--root", str(ENGINE_ROOT), "--check"])
        self.assertEqual(result, 0)

    def test_russian_pages_pin_english_hashes_and_preserve_commands(self) -> None:
        pages = docs_video.render_reference_pages(ENGINE_ROOT)
        for (_, document_id, _), english_path, russian_path in zip(
            docs_video.PAGE_DEFINITIONS,
            docs_video.CANONICAL_OUTPUT_PATHS,
            docs_video.RUSSIAN_OUTPUT_PATHS,
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

    def test_legacy_generated_routes_preserve_headings_and_entry_anchors(self) -> None:
        pages = docs_video.generate_reference_pages(self.model)
        for canonical_path, legacy_path in zip(
            docs_video.CANONICAL_OUTPUT_PATHS,
            docs_video.LEGACY_OUTPUT_PATHS,
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
            self.assertIn("../../en/reference/video/", legacy)
            self.assertIn("../../ru/reference/video/", legacy)

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

        self.assertIn("BuildTools/tests/test_docs_video.py", workflow)
        self.assertIn("BuildTools/docs_video.py --check", workflow)
        document_ids = {
            document["id"] for document in manifest["documents"].values()
        }
        self.assertIn("video-guide", document_ids)
        self.assertIn("generated-video-index", document_ids)
        guide = manifest["documents"]["Docs/en/how-to/content/video.md"]
        self.assertEqual(guide["disposition"], "retain")
        legacy_guide = manifest["documents"]["Docs/Video.md"]
        self.assertEqual(legacy_guide["state"], "redirect")
        self.assertEqual(legacy_guide["redirect_to"], "video-guide")
        for canonical_path, legacy_path in zip(
            docs_video.CANONICAL_OUTPUT_PATHS,
            docs_video.LEGACY_OUTPUT_PATHS,
            strict=True,
        ):
            canonical = manifest["documents"][canonical_path]
            legacy = manifest["documents"][legacy_path]
            self.assertEqual(canonical["state"], "current")
            self.assertEqual(canonical["disposition"], "retain")
            self.assertEqual(legacy["state"], "redirect")
            self.assertEqual(legacy["redirect_to"], canonical["id"])
        generated_paths = manifest["generated_artifacts"]["video_reference"]["paths"]
        self.assertTrue(set(docs_video.RUSSIAN_OUTPUT_PATHS).issubset(generated_paths))
        self.assertIn('"video"', contract_diff)
        self.assertIn("docs_video", validate)


if __name__ == "__main__":
    unittest.main()
