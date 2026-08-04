from __future__ import annotations

import json
import unittest
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]


class DocumentationProjectDependenciesTests(unittest.TestCase):
    def test_guide_owns_the_complete_project_dependency_boundary(self) -> None:
        guide = (
            ENGINE_ROOT / "Docs/en/how-to/native-extensions/project-dependencies.md"
        ).read_text(encoding="utf-8")

        for heading in (
            "## Choose The Owner First",
            "## Select A Delivery Model",
            "## Keep A Dependency Record",
            "## Integrate At The Project Boundary",
            "## Route To The Narrowest Role",
            "## Control Package Discovery",
            "## Define The Platform Contract",
            "## Package Runtime Payloads",
            "## Respect ABI, Allocation, And Lifetime",
            "## Review License And Supply-Chain Risk",
            "## Update Workflow",
            "## Validation Matrix",
            "## Failure Routing",
        ):
            self.assertIn(heading, guide)

        for contract in (
            "AddProjectLibraries",
            "FO_MAPPER_LIBS",
            "NotFoundFindPackage",
            "PassThroughFindPackage",
            "requested, compiled, and initialized at runtime",
            "runtime file hashes",
            "allocator hooks",
            "exact Engine compatibility range",
            "Do not use an unpinned branch",
        ):
            self.assertIn(contract, guide)

    def test_public_helper_routes_all_supported_roles_and_fixture_exercises_it(self) -> None:
        build_helpers = (ENGINE_ROOT / "BuildTools/cmake/helpers/Build.cmake").read_text(encoding="utf-8")
        state = (ENGINE_ROOT / "BuildTools/cmake/helpers/State.cmake").read_text(encoding="utf-8")
        core_libs = (ENGINE_ROOT / "BuildTools/cmake/stages/CoreLibs.cmake").read_text(encoding="utf-8")
        project_interface = json.loads(
            (ENGINE_ROOT / "BuildTools/cmake/ProjectInterface.json").read_text(encoding="utf-8")
        )
        fixture_cmake = (ENGINE_ROOT / "Examples/MinimalProject/CMakeLists.txt").read_text(encoding="utf-8")
        fixture_source = (ENGINE_ROOT / "Examples/MinimalProject/StarterServerExtension.cpp").read_text(
            encoding="utf-8"
        )

        for marker in (
            "macro(AddProjectLibraries)",
            "PROJECT_LIBRARIES_ROLES",
            "PROJECT_LIBRARIES_LIBRARIES",
            "unknown project library role",
            "must run before BuildCoreLibraries",
            "AppendList(FO_${role}_LIBS",
        ):
            self.assertIn(marker, build_helpers)
        self.assertIn("FO_MAPPER_LIBS", state)
        self.assertIn("${FO_MAPPER_LIBS}", core_libs)

        helper = next(entry for entry in project_interface["helpers"] if entry["name"] == "AddProjectLibraries")
        self.assertEqual(helper["allowed_roles"], ["COMMON", "SERVER", "CLIENT", "MAPPER", "BAKER"])
        self.assertIn("ROLES <role>", helper["signature"])
        self.assertIn("LIBRARIES <target-or-library>", helper["signature"])

        self.assertIn("add_library(StarterProjectDependency INTERFACE)", fixture_cmake)
        self.assertIn("AddProjectLibraries(ROLES SERVER LIBRARIES StarterProjectDependency)", fixture_cmake)
        self.assertIn("FO_STARTER_PROJECT_DEPENDENCY", fixture_source)

    def test_manifest_evaluation_evidence_and_ci_register_the_guide(self) -> None:
        manifest = json.loads((ENGINE_ROOT / "Docs/documentation-manifest.json").read_text(encoding="utf-8"))
        evaluation = json.loads((ENGINE_ROOT / "Docs/ai-evaluation.json").read_text(encoding="utf-8"))
        evidence = json.loads((ENGINE_ROOT / "BuildTools/ExternalProjectEvidence.json").read_text(encoding="utf-8"))
        workflow = (ENGINE_ROOT / ".github/workflows/validate.yml").read_text(encoding="utf-8")

        entry = manifest["documents"][
            "Docs/en/how-to/native-extensions/project-dependencies.md"
        ]
        self.assertEqual(entry["id"], "project-local-dependencies")
        self.assertEqual(entry["state"], "current")
        self.assertEqual(entry["classification"]["visibility"], "public")
        self.assertTrue(entry["classification"]["human"])
        self.assertEqual(entry["classification"]["translation"], "required")

        task = next(task for task in evaluation["tasks"] if task["id"] == "architecture-project-dependencies")
        self.assertEqual(task["primary_document_id"], "project-local-dependencies")
        self.assertEqual(len(task["retrieval_checks"]), 3)

        record = next(record for record in evidence["records"] if record["id"] == "native-extensions-and-dependencies")
        self.assertIn(
            "Docs/en/how-to/native-extensions/project-dependencies.md",
            record["engine_targets"],
        )
        self.assertIn("python3 BuildTools/tests/test_docs_project_dependencies.py", workflow)


if __name__ == "__main__":
    unittest.main()
