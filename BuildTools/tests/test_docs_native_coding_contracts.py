from __future__ import annotations

import json
import re
import unittest
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]
DOC_ROOT = "Docs/en/contributing/coding-contracts"


class DocumentationNativeCodingContractsTests(unittest.TestCase):
    def _read(self, relative_path: str) -> str:
        return (ENGINE_ROOT / relative_path).read_text(encoding="utf-8")

    def test_exception_safety_matches_lifecycle_convergence_contract(self) -> None:
        guide = self._read(f"{DOC_ROOT}/exception-safety.md")
        server_sources = "\n".join(
            path.read_text(encoding="utf-8")
            for path in (ENGINE_ROOT / "Source/Server").glob("*.cpp")
        )

        loop_marker = (
            "for (size_t prev_deps = std::numeric_limits<size_t>::max()"
        )
        self.assertEqual(server_sources.count(loop_marker), 8)
        self.assertIn("Eight such loops exist", guide)
        for marker in (
            "Source/Tests/Test_EntityLifecycle.cpp",
            "Source/Tests/Test_ServerMapOperations.cpp",
            "FO_STRONG_ASSERT",
            "scope_fail",
            "copy_hold_ref",
            "Reads and queries do not take `NOT_DESTROYING`",
        ):
            self.assertIn(marker, guide)
        self.assertNotIn("Tools/ExceptionSafetyAudit", guide)
        self.assertNotIn("Tools/AllocatorAudit", guide)

    def test_local_variable_rules_keep_analyzers_project_owned(self) -> None:
        guide = self._read(f"{DOC_ROOT}/local-variables.md")
        for marker in (
            "FO_REDUNDANT_CONST_SUPPRESS",
            "FO_USE_AFTER_MOVE_SUPPRESS",
            "Clang 20+",
            "embedding project owns analyzer implementation",
            "compile-database generation",
        ):
            self.assertIn(marker, guide)
        for project_tool in (
            "Tools/ExplicitLocalTypes",
            "Tools/LocalVariableValidator",
        ):
            self.assertNotIn(project_tool, guide)

    def test_nullability_matches_compiler_runtime_and_boundary_checks(self) -> None:
        guide = self._read(f"{DOC_ROOT}/nullability.md")
        compiler = self._read(
            "ThirdParty/AngelScript/sdk/angelscript/source/as_compiler.cpp"
        )
        context = self._read(
            "ThirdParty/AngelScript/sdk/angelscript/source/as_context.cpp"
        )
        backend = self._read(
            "Source/Scripting/AngelScript/AngelScriptBackend.cpp"
        )
        codegen = self._read("BuildTools/codegen.py")

        for marker in (
            "Dereference of nullable handle",
            "Redundant null comparison",
            "Redundant '?'",
        ):
            self.assertIn(marker, compiler)
            self.assertIn(marker, guide)
        self.assertIn("asBC_RefCpyChk", context)
        self.assertIn("asBC_RefCpyChk", guide)
        self.assertIn("asEP_DISALLOW_NULLABLE_TO_NON_NULLABLE", backend)
        self.assertIn("asEP_DISALLOW_NULLABLE_TO_NON_NULLABLE", guide)
        for marker in ("CheckArgNotNull", "CheckReturnNotNull"):
            self.assertIn(marker, codegen)
            self.assertIn(marker, guide)
        self.assertIn("Treat every client-originated payload as untrusted", guide)
        self.assertIn("complements rather than replaces", guide)
        self.assertNotIn("tokens truncated", guide)

    def test_smart_pointer_vocabulary_is_source_backed_and_project_neutral(self) -> None:
        guide = self._read(f"{DOC_ROOT}/smart-pointers.md")
        header = self._read("Source/Essentials/SmartPointers.h")
        memory = self._read("Source/Essentials/MemorySystem.h")

        for marker in (
            "class ptr",
            "class nptr",
            "class unique_ptr",
            "class unique_nptr",
            "class refcount_ptr",
            "class refcount_nptr",
            "make_ptr",
            "make_nptr",
            "take_not_null",
            "hold_ref",
        ):
            self.assertIn(marker, header)
            self.assertIn(marker.replace("class ", ""), guide)
        self.assertIn("MakeShared", memory)
        self.assertIn("SafeAlloc::MakeShared", guide)
        self.assertIn("does not currently ship a whole-tree", guide)
        self.assertNotIn("Tools/SmartPointerAudit", guide)
        self.assertIn("Do not reintroduce the removed permissive `FO_STRICT_*`", guide)

    def test_thread_safety_macro_inventory_and_clang_gate_are_exact(self) -> None:
        guide = self._read(f"{DOC_ROOT}/thread-safety-analysis.md")
        threading = self._read("Source/Essentials/Threading.h")
        cmake = self._read("BuildTools/cmake/stages/Init.cmake")

        source_macros = set(re.findall(r"^#define (FO_TSA_[A-Z_]+)", threading, re.M))
        source_macros.remove("FO_TSA_ATTR")
        vocabulary = guide.split("## Macro vocabulary", 1)[1].split("\n## ", 1)[0]
        documented_macros = set(re.findall(r"FO_TSA_[A-Z_]+", vocabulary))
        self.assertEqual(documented_macros, source_macros)
        for marker in (
            "fo::mutex",
            "fo::shared_mutex",
            "fo::scoped_lock",
            "fo::shared_lock",
            "fo::unique_lock",
        ):
            self.assertIn(marker, guide)
        self.assertIn("-Wthread-safety -Werror=thread-safety", cmake)
        self.assertIn("/clang:-Wthread-safety /clang:-Werror=thread-safety", cmake)

    def test_engine_source_comments_name_no_documentation_page(self) -> None:
        # A comment outlives the page it names: the docs move, the path stays, and the reader follows it into
        # nothing. Engine comments point at code only, and the guide is reached from Docs/en/index.md
        source_root = ENGINE_ROOT / "Source"
        suffixes = {".cpp", ".h", ".hpp", ".inc", ".fos"}
        comment = re.compile(r"(?://|/\*|^\s*\*)")
        page = re.compile(r"[\w./-]+\.md\b")

        offenders: list[str] = []
        for path in sorted(source_root.rglob("*")):
            if path.suffix not in suffixes or not path.is_file():
                continue
            for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
                match = comment.search(line)
                if match is not None and page.search(line, match.end()) is not None:
                    offenders.append(f"{path.relative_to(ENGINE_ROOT).as_posix()}:{number}")

        self.assertEqual([], offenders)

    def test_manifest_owns_canonical_and_legacy_routes(self) -> None:
        manifest = json.loads(
            self._read("Docs/documentation-manifest.json")
        )["documents"]
        document_ids = (
            "exception-safety",
            "local-variables",
            "nullability",
            "smart-pointers",
            "thread-safety-analysis",
        )

        for document_id in document_ids:
            canonical_path = f"{DOC_ROOT}/{document_id}.md"
            legacy_name = "".join(part.title() for part in document_id.split("-"))
            legacy_path = f"Docs/{legacy_name}.md"
            canonical = manifest[canonical_path]
            legacy = manifest[legacy_path]
            self.assertEqual(canonical["id"], document_id)
            self.assertEqual(canonical["state"], "current")
            self.assertEqual(canonical["disposition"], "retain")
            self.assertEqual(legacy["state"], "redirect")
            self.assertEqual(legacy["redirect_to"], document_id)
            self.assertEqual(legacy["target"], canonical_path)


if __name__ == "__main__":
    unittest.main()
