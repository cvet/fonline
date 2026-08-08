from __future__ import annotations

import unittest
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]
ANGELSCRIPT_CMAKE = (
    ENGINE_ROOT
    / "ThirdParty"
    / "AngelScript"
    / "sdk"
    / "angelscript"
    / "projects"
    / "cmake"
    / "CMakeLists.txt"
)


class AngelScriptCMakeTests(unittest.TestCase):
    def test_assembler_sources_follow_discovered_compilers(self) -> None:
        source = ANGELSCRIPT_CMAKE.read_text(encoding="utf-8")

        self.assertNotIn("CMAKE_ASM_MASM_COMPILER_WORKS", source)
        self.assertNotIn("CMAKE_ASM_COMPILER_WORKS", source)
        self.assertIn("if(CMAKE_ASM_MASM_COMPILER)", source)
        self.assertGreaterEqual(source.count("if(CMAKE_ASM_COMPILER)"), 2)
        self.assertIn("../../source/as_callfunc_x64_msvc_asm.asm", source)
        self.assertNotIn("message(FATAL ERROR", source)
        self.assertGreaterEqual(source.count("(FOnline Patch)"), 3)


if __name__ == "__main__":
    unittest.main()
