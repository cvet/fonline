from __future__ import annotations

import json
import re
import unittest
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]
BUILD_GUIDE = "Docs/en/how-to/build/index.md"
EMBEDDING_GUIDE = "Docs/en/how-to/build/embedding-project.md"
GENERATED_GUIDE = "Docs/en/how-to/build/generated-content.md"
CONFIG_GUIDE = "Docs/en/how-to/build/project-configuration.md"


class BuildFoundationsDocumentationTests(unittest.TestCase):
    def _read(self, relative_path: str) -> str:
        return (ENGINE_ROOT / relative_path).read_text(encoding="utf-8")

    def test_project_generation_stages_match_the_strict_dispatcher(self) -> None:
        interface = json.loads(self._read("BuildTools/cmake/ProjectInterface.json"))
        entrypoints = [stage["entrypoint"] for stage in interface["stages"]]
        self.assertEqual(
            entrypoints,
            [
                "StartProjectGeneration",
                "RegisterProjectOptions",
                "AddThirdPartyLibraries",
                "RegisterEngineSources",
                "SetupCodeGeneration",
                "BuildCoreLibraries",
                "BuildApplications",
                "SetupScriptsAndBaking",
                "BuildPackages",
                "FinalizeProjectGeneration",
            ],
        )
        applications = next(stage for stage in interface["stages"] if stage["name"] == "Applications")
        self.assertIn("viewer", applications["description"])
        self.assertNotIn("editor", applications["description"])

        dispatcher = self._read("BuildTools/Init.cmake")
        for marker in (
            "each stage must run exactly once",
            "invoked before",
            "must call every stage in order",
        ):
            self.assertIn(marker, dispatcher)

        guide = self._read(BUILD_GUIDE)
        self.assertIn("BuildTools/Init.cmake", guide)
        self.assertIn("BuildTools Pipeline", guide)

    def test_generated_workflow_keeps_codegen_and_docs_dependency_order(self) -> None:
        cmake = self._read("BuildTools/cmake/stages/ScriptsAndBaking.cmake")
        self.assertIn(
            'SetValue(foMainConfigArgs -ApplyConfig "${CMAKE_CURRENT_SOURCE_DIR}/${FO_MAIN_CONFIG}" -ApplySubConfig "NONE")',
            cmake,
        )
        for target in ("CompileAngelScript", "BakeResources", "ForceBakeResources"):
            block = re.search(
                rf"AddCommandTarget\({target}\b(?P<body>.*?)(?=\nAddCommandTarget\(|\Z)",
                cmake,
                re.DOTALL,
            )
            self.assertIsNotNone(block, target)
            self.assertIn("DEPENDS ForceCodeGeneration", block.group("body"), target)

        guide = self._read(GENERATED_GUIDE)
        commands = (
            "docs_snippets.py --write --external",
            "docs_localization.py --write",
            "docs_site.py --write",
            "docs_ai_eval.py --write",
            "docs_ai_delivery.py --write",
            "python BuildTools/docs_validate.py",
        )
        positions = [guide.index(command) for command in commands]
        self.assertEqual(positions, sorted(positions))
        self.assertIn("-ApplyConfig <project .fomain> -ApplySubConfig NONE", guide)

    def test_resource_pack_and_subconfig_contracts_are_documented(self) -> None:
        settings_header = self._read("Source/Common/Settings.h")
        resource_pack = re.search(
            r"struct ResourcePackInfo\s*\{(?P<body>.*?)\n\};",
            settings_header,
            re.DOTALL,
        )
        self.assertIsNotNone(resource_pack)
        fields = re.findall(
            r"(?:string|vector<string>|bool)\s+(\w+)\s*\{\};",
            resource_pack.group("body"),
        )
        self.assertEqual(
            fields,
            [
                "Name",
                "InputDirs",
                "InputFiles",
                "IncludePatterns",
                "ExcludePatterns",
                "ServerOnly",
                "ClientOnly",
                "MapperOnly",
                "Bakers",
            ],
        )

        guide = self._read(CONFIG_GUIDE)
        for field in fields:
            self.assertIn(f"`{field}`", guide)
        self.assertIn("`RecursiveInput` appears in older project files", guide)
        self.assertIn("Render.RenderDebug = True", guide)
        self.assertNotIn("Render.Debug = True", guide)
        for behavior in (
            "String values append with a space",
            "vectors append elements",
            "numeric values add",
            "booleans use logical OR",
            "enums use bitwise OR",
        ):
            self.assertIn(behavior, guide)

        settings_source = self._read("Source/Common/Settings.cpp")
        self.assertIn("later parents override earlier", settings_source)
        self.assertIn("section's own settings (below) override all parents", settings_source)
        self.assertIn("pack_info.ServerOnly", settings_source)
        self.assertIn("pack_info.ClientOnly", settings_source)
        self.assertIn("pack_info.MapperOnly", settings_source)

    def test_embedding_boundary_matches_the_minimal_project(self) -> None:
        project = self._read("Examples/MinimalProject/CMakeLists.txt")
        self.assertIn("add_library(StarterProjectDependency INTERFACE)", project)
        self.assertIn(
            "AddProjectLibraries(ROLES SERVER LIBRARIES StarterProjectDependency)",
            project,
        )
        self.assertIn("if(COMMAND AddProjectLibraries)", project)
        self.assertIn("list(APPEND FO_SERVER_LIBS StarterProjectDependency)", project)

        stock_dialogs = [
            path
            for root in (ENGINE_ROOT / "Source", ENGINE_ROOT / "BuildTools")
            for path in root.rglob("*.fodlg")
        ]
        self.assertEqual(stock_dialogs, [])

        guide = self._read(EMBEDDING_GUIDE)
        self.assertIn("does not currently ship", guide)
        self.assertIn("a built-in dialog-tree schema", guide)
        self.assertIn("project-owned", guide)
        self.assertIn("server-only `INTERFACE` dependency", guide)

    def test_canonical_and_legacy_build_routes_are_explicit(self) -> None:
        manifest = json.loads(self._read("Docs/documentation-manifest.json"))
        routes = (
            (BUILD_GUIDE, "build-workflow", "Docs/BuildWorkflow.md", "legacy-build-workflow-route"),
            (EMBEDDING_GUIDE, "embedding-project", "Docs/EmbeddingProject.md", "legacy-embedding-project-route"),
            (GENERATED_GUIDE, "generated-content-workflow", "Docs/GeneratedContentWorkflow.md", "legacy-generated-content-workflow-route"),
            (CONFIG_GUIDE, "project-configuration", "Docs/ProjectConfiguration.md", "legacy-project-configuration-route"),
        )
        for canonical_path, document_id, legacy_path, legacy_id in routes:
            canonical = manifest["documents"][canonical_path]
            legacy = manifest["documents"][legacy_path]
            self.assertEqual(canonical["id"], document_id)
            self.assertEqual(canonical["state"], "current")
            self.assertEqual(canonical["disposition"], "retain")
            self.assertEqual(canonical["classification"]["translation"], "required")
            self.assertEqual(legacy["id"], legacy_id)
            self.assertEqual(legacy["state"], "redirect")
            self.assertEqual(legacy["disposition"], "replace")
            self.assertEqual(legacy["redirect_to"], document_id)
            self.assertFalse(legacy["classification"]["human"])

            legacy_text = self._read(legacy_path)
            self.assertIn("> Legacy route.", legacy_text)
            self.assertIn(canonical_path.removeprefix("Docs/"), legacy_text)
            russian_path = canonical_path.replace("Docs/en/", "Docs/ru/").removeprefix("Docs/")
            self.assertIn(russian_path, legacy_text)

        evidence = self._read("BuildTools/ExternalProjectEvidence.json")
        for old_path in ("BuildWorkflow.md", "EmbeddingProject.md", "GeneratedContentWorkflow.md", "ProjectConfiguration.md"):
            self.assertNotIn(f"Docs/{old_path}", evidence)
        for canonical_path, _, _, _ in routes:
            self.assertIn(canonical_path, evidence)


if __name__ == "__main__":
    unittest.main()
