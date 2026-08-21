from __future__ import annotations

import json
import re
import unittest
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]
GUIDE_PATH = "Docs/en/explanation/rendering/index.md"
RUSSIAN_PATH = "Docs/ru/explanation/rendering/index.md"
LEGACY_PATH = "Docs/FrontendAndRendering.md"


class FrontendRenderingDocumentationTests(unittest.TestCase):
    def _read(self, relative_path: str) -> str:
        return (ENGINE_ROOT / relative_path).read_text(encoding="utf-8")

    def test_application_services_and_render_facade_are_covered(self) -> None:
        header = self._read("Source/Frontend/Application.h")
        guide = self._read(GUIDE_PATH)

        for interface in ("IAppRender", "IAppInput", "IAppAudio", "IAppWindow"):
            self.assertIn(f"class {interface}", header)
            self.assertIn(f"`{interface}`", guide)

        for method in (
            "CreateTexture",
            "CreateDrawBuffer",
            "CreateEffect",
            "CreateOrthoMatrix",
            "SetRenderTarget",
            "SetOrthoDepthRange",
            "ClearRenderTarget",
            "EnableScissor",
            "DisableScissor",
        ):
            self.assertIn(method, header)
            self.assertIn(f"`{method}()`", guide)

    def test_backend_inventory_and_selection_match_live_source(self) -> None:
        header = self._read("Source/Frontend/Rendering.h")
        application = self._read("Source/Frontend/Application.cpp")
        guide = self._read(GUIDE_PATH)
        enum_block = re.search(
            r"enum class RenderType : uint8_t\s*\{(?P<body>.*?)\n\};",
            header,
            re.DOTALL,
        )
        self.assertIsNotNone(enum_block)
        backend_names = re.findall(r"^\s{4}([A-Za-z0-9_]+),$", enum_block.group("body"), re.MULTILINE)
        self.assertEqual(
            backend_names,
            ["Null", "OpenGL", "Direct3D", "Metal", "Vulkan", "SDLGpu"],
        )

        for backend in backend_names:
            self.assertIn(f"| `{backend}` |", guide)

        self.assertIn("else if (Settings.ForceMetal)", application)
        self.assertIn("throw NotImplementedException(FO_LINE_STR);", application)
        self.assertIn("else if (Settings.ForceSDLGpu)", application)
        self.assertIn("Direct Metal is a placeholder", guide)
        self.assertIn("does not retry another backend", guide)

    def test_backend_build_flags_and_settings_are_documented(self) -> None:
        cmake = self._read("BuildTools/cmake/stages/Init.cmake")
        settings = self._read("Source/Common/Settings.inc")
        guide = self._read(GUIDE_PATH)

        self.assertIn("No external Vulkan SDK", cmake)
        self.assertNotIn("find_package(Vulkan)", cmake)
        for option in ("FO_DISABLE_VULKAN", "FO_DISABLE_SDL_GPU"):
            self.assertIn(option, cmake)
            self.assertIn(f"`{option}`", guide)

        for setting in (
            "ForceOpenGL",
            "ForceDirect3D",
            "ForceMetal",
            "ForceVulkan",
            "ForceSDLGpu",
            "SDLGpuDriver",
        ):
            self.assertIn(f"Render, {setting}", settings)
            self.assertIn(f"`Render.{setting}`", guide)

    def test_validation_routes_cover_every_implemented_backend(self) -> None:
        native_tests = self._read("Source/Tests/Test_Rendering.cpp")
        guide = self._read(GUIDE_PATH)

        for marker in (
            'TEST_CASE("NullRenderer")',
            'SECTION("TextureReadWriteAndClear")',
            'SECTION("DrawBufferUploadAndEffectDraw")',
            'SECTION("DepthVariantRequiresBuiltState")',
        ):
            self.assertIn(marker, native_tests)

        for backend in ("Null/headless", "OpenGL/WebGL", "Direct3D", "Vulkan", "SDL_GPU"):
            self.assertIn(backend, guide)
        self.assertIn("zero `[VkLayer/...`", guide)
        self.assertIn("visible map and GUI", guide)

    def test_project_evidence_supports_reusable_practices(self) -> None:
        evidence = json.loads(self._read("BuildTools/ExternalProjectEvidence.json"))
        record = next(
            item
            for item in evidence["records"]
            if item["id"] == "effects-images-and-shaders"
        )
        sources = {(item["snapshot"], item["path"]) for item in record["sources"]}
        self.assertIn(("last-frontier", "LastFrontier.fomain"), sources)
        self.assertIn(("last-frontier", "Resources/Visual/Effects"), sources)
        self.assertIn(("fonline-tla", "TLA.fomain"), sources)
        self.assertIn(("fonline-tla", "Resources"), sources)
        self.assertIn(GUIDE_PATH, record["engine_targets"])

        guide = self._read(GUIDE_PATH)
        self.assertIn("## Embedding-project practices", guide)
        self.assertIn("Keep every `Render.Force*` selector false", guide)
        self.assertIn("Do not copy an older project's render block", guide)
        self.assertIn("advanced-profile overrides", guide)

    def test_russian_mirror_keeps_the_complete_runtime_contract(self) -> None:
        guide = self._read(GUIDE_PATH)
        russian = self._read(RUSSIAN_PATH)

        self.assertGreaterEqual(len(russian.encode("utf-8")), len(guide.encode("utf-8")) * 0.75)
        self.assertEqual(
            len(re.findall(r"^#{2,3} ", russian, re.MULTILINE)),
            len(re.findall(r"^#{2,3} ", guide, re.MULTILINE)),
        )
        for contract in (
            "`AppRender` / `IAppRender`",
            "`VULKAN_FRAMES_IN_FLIGHT = 2`",
            "`TextureAtlasLayout`",
            "`Game.SetResolution`",
            "`Render.ModelDirectDraw`",
            "`SpriteManager::AcquireSceneBackground()`",
            "`Render.DrawWireframe`",
            "`Render.ForceSDLGpu = True`",
        ):
            self.assertIn(contract, russian)

    def test_canonical_russian_and_legacy_routes_are_owned(self) -> None:
        documents = json.loads(self._read("Docs/documentation-manifest.json"))["documents"]
        canonical = documents[GUIDE_PATH]
        legacy = documents[LEGACY_PATH]

        self.assertEqual(
            (canonical["id"], canonical["state"], canonical["disposition"]),
            ("frontend-rendering", "current", "retain"),
        )
        self.assertEqual(
            (legacy["state"], legacy["disposition"], legacy["redirect_to"]),
            ("redirect", "replace", "frontend-rendering"),
        )
        self.assertTrue((ENGINE_ROOT / RUSSIAN_PATH).is_file())

        canonical_text = self._read(GUIDE_PATH)
        legacy_text = self._read(LEGACY_PATH)
        for heading in re.findall(r"^#{2,3} .+$", canonical_text, re.MULTILINE):
            self.assertIn(heading, legacy_text)
        self.assertIn("en/explanation/rendering/", legacy_text)
        self.assertIn("ru/explanation/rendering/", legacy_text)


if __name__ == "__main__":
    unittest.main()
