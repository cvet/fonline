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

import docs_effect_format  # noqa: E402
import docs_localization  # noqa: E402


class EffectFormatDocumentationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.model = docs_effect_format.generate_effect_format_model(ENGINE_ROOT)

    def test_model_is_deterministic_and_derives_live_limits(self) -> None:
        second = docs_effect_format.generate_effect_format_model(ENGINE_ROOT)

        self.assertEqual(self.model, second)
        self.assertEqual(
            self.model["schema_version"], docs_effect_format.SCHEMA_VERSION
        )
        limits = {
            entry["option"]: entry for entry in self.model["compile_limits"]
        }
        self.assertEqual(limits["FO_EFFECT_SCRIPT_VALUES"]["default"], "16")
        self.assertEqual(limits["FO_EFFECT_MAX_PASSES"]["default"], "6")
        self.assertEqual(limits["FO_MODEL_MAX_TEXTURES"]["default"], "8")
        self.assertEqual(limits["FO_MODEL_MAX_BONES"]["default"], "54")

        identities = [
            entry["id"]
            for collection in ("compile_limits", *docs_effect_format.COLLECTION_KINDS)
            for entry in self.model[collection]
        ]
        self.assertEqual(len(identities), len(set(identities)))
        self.assertTrue(
            all(
                docs_effect_format.ENTRY_ID_PATTERN.fullmatch(identity)
                for identity in identities
            )
        )

    def test_format_contract_preserves_pass_binding_and_cache_boundaries(self) -> None:
        sections = {entry["id"]: entry for entry in self.model["sections"]}
        options = {entry["id"]: entry for entry in self.model["effect_options"]}
        resources = {entry["id"]: entry for entry in self.model["resources"]}
        runtime = {entry["id"]: entry for entry in self.model["runtime_rules"]}

        self.assertIn(
            "falls back",
            sections["effect-format.section.vertex"]["requirement"],
        )
        self.assertEqual(
            options["effect-format.option.blend-func"]["default"],
            "SrcAlpha InvSrcAlpha",
        )
        self.assertEqual(
            options["effect-format.option.depth-variants"]["default"],
            "False",
        )
        self.assertEqual(
            options["effect-format.option.cull-variants"]["default"],
            "False",
        )
        self.assertIn(
            "descriptor set 0",
            resources["effect-format.resource.native-descriptor-sets"][
                "requirement"
            ],
        )
        self.assertIn(
            "BackgroundTex",
            resources["effect-format.resource.samplers"]["requirement"],
        )
        self.assertIn(
            "ParticleSamplingBuf",
            resources["effect-format.resource.general-buffers"]["requirement"],
        )
        self.assertIn(
            "SetEffect does not clear",
            runtime["effect-format.runtime.script-value-lifetime"]["requirement"],
        )
        self.assertIn(
            "cache key omits EffectUsage",
            runtime["effect-format.runtime.first-usage"]["rationale"],
        )

    def test_outputs_cover_native_sdl_and_cross_compiled_flavors(self) -> None:
        outputs = self.model["outputs"]

        self.assertEqual(
            outputs["stage_flavors"],
            [
                "spv",
                "spv_sdl",
                "glsl",
                "glsl_es",
                "hlsl",
                "msl_mac",
                "msl_ios",
            ],
        )
        self.assertEqual(
            outputs["native_descriptor_sets"],
            {"uniform_buffers": 0, "samplers": 1},
        )
        self.assertEqual(outputs["sdl_gpu_stage_limits"]["samplers"], 16)
        self.assertEqual(outputs["sdl_gpu_stage_limits"]["uniform_buffers"], 4)

    def test_manifest_entries_keep_live_source_anchors(self) -> None:
        for collection in ("compile_limits", *docs_effect_format.COLLECTION_KINDS):
            for entry in self.model[collection]:
                for source in entry["source"]:
                    source_text = (ENGINE_ROOT / source["path"]).read_text(
                        encoding="utf-8"
                    )
                    for anchor in source["anchors"]:
                        self.assertIn(anchor, source_text)

    def test_human_guide_covers_authoring_runtime_and_project_boundaries(self) -> None:
        guide = (
            ENGINE_ROOT / "Docs/en/how-to/content/effect-format.md"
        ).read_text(encoding="utf-8")

        for heading in (
            "## File structure",
            "## Render state",
            "## Descriptor and binding contract",
            "## Built-in uniform buffers",
            "## ScriptValueBuf ownership and lifetime",
            "## Runtime loading and cache identity",
            "## Validation workflow",
        ):
            self.assertIn(heading, guide)
        self.assertIn("There is no `.fofx` include directive", guide)
        self.assertIn("`DepthVariants`", guide)
        self.assertIn("`CullVariants`", guide)
        self.assertIn("`BackgroundTex`", guide)
        self.assertIn("`ParticleSamplingBuf`", guide)
        self.assertIn("SetEffect does not reset cached values", guide)
        self.assertIn("The cache key does not include `EffectUsage`", guide)

    def test_missing_required_output_flavor_is_rejected(self) -> None:
        manifest = json.loads(
            (ENGINE_ROOT / docs_effect_format.DEFAULT_MANIFEST).read_text(
                encoding="utf-8"
            )
        )
        manifest["outputs"] = copy.deepcopy(manifest["outputs"])
        manifest["outputs"]["stage_flavors"].remove("spv_sdl")

        with tempfile.TemporaryDirectory() as temporary_directory:
            manifest_path = Path(temporary_directory) / "EffectFormatInterface.json"
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            with self.assertRaisesRegex(
                ValueError, "outputs.stage_flavors must be"
            ):
                docs_effect_format.generate_effect_format_model(
                    ENGINE_ROOT, str(manifest_path)
                )

    def test_generated_pages_and_checked_outputs_are_current(self) -> None:
        pages = docs_effect_format.render_reference_pages(ENGINE_ROOT)

        self.assertEqual(set(pages), set(docs_effect_format.OUTPUT_PATHS))
        self.assertIn(
            "VertexShader PassN",
            pages["Docs/en/reference/effect-format/syntax.md"],
        )
        self.assertIn(
            "SrcAlpha InvSrcAlpha",
            pages["Docs/en/reference/effect-format/render-state.md"],
        )
        self.assertIn(
            "FO_EFFECT_SCRIPT_VALUES",
            pages["Docs/en/reference/effect-format/resources.md"],
        )
        self.assertIn(
            "spv_sdl",
            pages["Docs/en/reference/effect-format/baking.md"],
        )
        self.assertIn(
            "SetEffect does not clear",
            pages["Docs/en/reference/effect-format/runtime.md"],
        )
        self.assertIn(
            "Сгенерированный справочник формата эффектов",
            pages["Docs/ru/reference/effect-format/index.md"],
        )
        self.assertIn(
            "Сохранение ScriptValue",
            pages["Docs/ru/reference/effect-format/runtime.md"],
        )
        self.assertNotIn(
            "Required and optional sections plus pass fallback.",
            pages["Docs/ru/reference/effect-format/index.md"],
        )
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(
            io.StringIO()
        ):
            result = docs_effect_format.main(
                ["--root", str(ENGINE_ROOT), "--check"]
            )
        self.assertEqual(result, 0)

    def test_russian_pages_pin_english_hashes_and_preserve_commands(self) -> None:
        pages = docs_effect_format.render_reference_pages(ENGINE_ROOT)
        for (_, document_id, _), english_path, russian_path in zip(
            docs_effect_format.PAGE_DEFINITIONS,
            docs_effect_format.CANONICAL_OUTPUT_PATHS,
            docs_effect_format.RUSSIAN_OUTPUT_PATHS,
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
        pages = docs_effect_format.generate_reference_pages(self.model)

        for filename, _, _ in docs_effect_format.PAGE_DEFINITIONS:
            canonical = pages[f"Docs/en/reference/effect-format/{filename}"]
            legacy = pages[f"Docs/generated/effect-format/{filename}"]
            for heading in re.findall(r"^#{2,3} .+$", canonical, re.MULTILINE):
                self.assertIn(heading, legacy)
            for anchor in re.findall(r'<a id="([^"]+)"></a>', canonical):
                self.assertIn(f'<a id="{anchor}"></a>', legacy)
                self.assertIn(
                    f"../../en/reference/effect-format/{filename}#{anchor}",
                    legacy,
                )

    def test_manifest_owns_canonical_and_legacy_routes(self) -> None:
        documents = json.loads(
            (ENGINE_ROOT / "Docs/documentation-manifest.json").read_text(
                encoding="utf-8"
            )
        )["documents"]
        guide = documents["Docs/en/how-to/content/effect-format.md"]
        legacy_guide = documents["Docs/EffectFormat.md"]
        self.assertEqual(
            (guide["id"], guide["state"], guide["disposition"]),
            ("effect-format-guide", "current", "retain"),
        )
        self.assertEqual(
            (
                legacy_guide["id"],
                legacy_guide["state"],
                legacy_guide["disposition"],
                legacy_guide["redirect_to"],
            ),
            (
                "legacy-effect-format-guide-route",
                "redirect",
                "replace",
                "effect-format-guide",
            ),
        )
        for filename, document_id, _ in docs_effect_format.PAGE_DEFINITIONS:
            canonical = documents[f"Docs/en/reference/effect-format/{filename}"]
            legacy = documents[f"Docs/generated/effect-format/{filename}"]
            self.assertEqual(
                (canonical["id"], canonical["state"], canonical["disposition"]),
                (document_id, "current", "retain"),
            )
            self.assertEqual(
                (legacy["state"], legacy["disposition"], legacy["redirect_to"]),
                ("redirect", "replace", document_id),
            )
        generated_paths = json.loads(
            (ENGINE_ROOT / "Docs/documentation-manifest.json").read_text(
                encoding="utf-8"
            )
        )["generated_artifacts"]["effect_format_reference"]["paths"]
        self.assertTrue(
            set(docs_effect_format.RUSSIAN_OUTPUT_PATHS).issubset(generated_paths)
        )


if __name__ == "__main__":
    unittest.main()
