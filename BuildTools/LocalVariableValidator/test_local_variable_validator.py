from __future__ import annotations

import json
import os
import shutil
import tempfile
import unittest
from pathlib import Path

import local_variable_validator as validator


FIXTURE = r'''
#include <utility>

struct Value
{
    Value();
    Value(Value&&);
};

template<typename T>
struct Wrapper
{
    T* value {};
};

void Consume(Value);
void Read(const Value&);

void Validate()
{
    const int redundant = 1;

    int value = 1;
    const int* pointee_const = &value;
    int* const const_pointer = &value;
    const Wrapper<const int> wrapped_const {};
    Wrapper<const int> const east_wrapped_const {};
    Wrapper<const int> wrappers[1] {};
    for (const Wrapper<const int> loop_wrapped_const : wrappers) {
        (void)loop_wrapped_const;
    }
    const int const_array[] = {1, 2};
    constexpr int constexpr_value = 1;

    const int intentional_const = 1; // FO_REDUNDANT_CONST_SUPPRESS: const selects a deliberate API contract

    Value moved;
    Consume(std::move(moved));
    Read(moved);

    Value inspected_after_move;
    Consume(std::move(inspected_after_move));
    Read(inspected_after_move); // FO_USE_AFTER_MOVE_SUPPRESS: test verifies the moved-from object contract

    (void)redundant;
    (void)pointee_const;
    (void)const_pointer;
    (void)wrapped_const;
    (void)east_wrapped_const;
    (void)const_array;
    (void)constexpr_value;
    (void)intentional_const;
}
'''


class LocalVariableValidatorIntegrationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.clang_query = validator.discover_tool(None, "FO_CLANG_QUERY", ("clang-query-20", "clang-query"))
        cls.clang_tidy = validator.discover_tool(None, "FO_CLANG_TIDY", ("clang-tidy-20", "clang-tidy"))
        compiler = None
        for name in ("clang++-20", "clang++"):
            sibling = cls.clang_query.parent / (f"{name}.exe" if os.name == "nt" else name)
            if sibling.is_file():
                compiler = sibling
                break
            found = shutil.which(name)
            if found:
                compiler = Path(found)
                break
        if compiler is None:
            raise unittest.SkipTest("clang++ matching clang-query is unavailable")
        cls.compiler = compiler

    def test_redundant_const_and_use_after_move(self) -> None:
        with tempfile.TemporaryDirectory(prefix="local_variable_fixture_") as temp_text:
            temp = Path(temp_text)
            source = temp / "fixture.cpp"
            source.write_text(FIXTURE, encoding="utf-8")
            vendored_source = temp / "vendor" / "fixture.cpp"
            vendored_source.parent.mkdir()
            vendored_source.write_text("void VendorCase() { const int vendored = 1; }\n", encoding="utf-8")
            command = f'"{self.compiler}" -std=c++20 -c "{source}"'
            vendored_command = f'"{self.compiler}" -std=c++20 -c "{vendored_source}"'
            database = [
                {"directory": str(temp), "command": command, "file": str(source)},
                {"directory": str(temp), "command": vendored_command, "file": str(vendored_source)},
            ]
            (temp / "compile_commands.json").write_text(json.dumps(database), encoding="utf-8")

            result = validator.analyze(
                source_root=temp,
                database_dir=temp,
                clang_query_path=str(self.clang_query),
                clang_tidy_path=str(self.clang_tidy),
                source_pattern="local_variable_fixture_",
                timeout=120,
                batch_size=1,
                jobs=1,
                checks="all",
                unit_pattern=r"^fixture\.cpp$",
            )

        self.assertEqual([unit.canonical_path for unit in result.units], ["fixture.cpp"])
        by_variable = {diagnostic.variable: diagnostic for diagnostic in result.diagnostics}
        self.assertEqual(by_variable["redundant"].code, "fo-redundant-local-const")
        self.assertEqual(by_variable["const_pointer"].code, "fo-redundant-local-const")
        self.assertEqual(by_variable["moved"].code, "fo-use-after-move")
        self.assertTrue(by_variable["intentional_const"].suppressed)
        self.assertTrue(by_variable["inspected_after_move"].suppressed)
        self.assertNotIn("pointee_const", by_variable)
        self.assertNotIn("const_array", by_variable)
        self.assertNotIn("constexpr_value", by_variable)
        self.assertNotIn("vendored", by_variable)

    def test_multiline_ast_range_uses_declarator_location(self) -> None:
        with tempfile.TemporaryDirectory(prefix="local_variable_location_") as temp_text:
            temp = Path(temp_text)
            source = temp / "fixture.cpp"
            source.write_text("const int value = 1;\n", encoding="utf-8")
            mapper = validator.SourcePathMapper(temp)
            location = validator._declarator_location_from_node_line(
                f"VarDecl 0x1 <{source}:1:1, line:3:2> col:11 value 'const int'",
                mapper,
            )
        self.assertIsNotNone(location)
        assert location is not None
        self.assertEqual((location.path, location.line, location.column), ("fixture.cpp", 1, 11))

    def test_apply_removes_only_top_level_const(self) -> None:
        with tempfile.TemporaryDirectory(prefix="local_variable_apply_") as temp_text:
            temp = Path(temp_text)
            source = temp / "fixture.cpp"
            source.write_text(FIXTURE, encoding="utf-8")
            command = f'"{self.compiler}" -std=c++20 -c "{source}"'
            database = [{"directory": str(temp), "command": command, "file": str(source)}]
            (temp / "compile_commands.json").write_text(json.dumps(database), encoding="utf-8")

            result = validator.analyze(
                source_root=temp,
                database_dir=temp,
                clang_query_path=str(self.clang_query),
                clang_tidy_path=str(self.clang_tidy),
                source_pattern="local_variable_apply_",
                timeout=120,
                batch_size=1,
                jobs=1,
                checks="redundant-local-const",
            )
            applied = validator.apply_redundant_const_fixes(result.diagnostics)
            rewritten = source.read_text(encoding="utf-8")

        self.assertEqual(applied, 5)
        self.assertIn("int redundant = 1;", rewritten)
        self.assertIn("int* const_pointer = &value;", rewritten)
        self.assertIn("Wrapper<const int> wrapped_const {};", rewritten)
        self.assertIn("Wrapper<const int> east_wrapped_const {};", rewritten)
        self.assertIn("for (Wrapper<const int> loop_wrapped_const : wrappers)", rewritten)
        self.assertIn("const int* pointee_const = &value;", rewritten)
        self.assertIn("const int const_array[] = {1, 2};", rewritten)
        self.assertIn("constexpr int constexpr_value = 1;", rewritten)
        self.assertIn("const int intentional_const = 1;", rewritten)

    def test_compilation_errors_are_detected_in_clang_output(self) -> None:
        self.assertTrue(validator._has_compilation_error("source.cpp:1:2: error: invalid declaration\n"))
        self.assertTrue(validator._has_compilation_error("source.cpp(1,2): error: invalid declaration\n"))
        self.assertFalse(validator._has_compilation_error("source.cpp:1:2: warning: ignored\n"))

    def test_const_location_falls_back_when_ast_uses_declaration_start_for_name(self) -> None:
        with tempfile.TemporaryDirectory(prefix="local_variable_location_fallback_") as temp_text:
            source = Path(temp_text) / "source.cpp"
            source.write_text("    const bool value = true;\n", encoding="utf-8")
            start = validator.SourceLocation("source.cpp", 1, 5, str(source))
            node = validator.NodeBinding(
                "top_level_const_local",
                "VarDecl",
                "0x1",
                "",
                "",
                start,
                start,
                "value",
                "const bool",
            )
            location = validator._find_top_level_const_location(node)

        self.assertIsNotNone(location)
        assert location is not None
        self.assertEqual((location.line, location.column), (1, 5))

    def test_invalid_suppression_does_not_hide_diagnostic(self) -> None:
        with tempfile.TemporaryDirectory(prefix="local_variable_suppression_") as temp_text:
            source = Path(temp_text) / "source.cpp"
            source.write_text("const int value = 1; // FO_REDUNDANT_CONST_SUPPRESS: no\n", encoding="utf-8")
            location = validator.SourceLocation("source.cpp", 1, 11, str(source))
            diagnostic = validator.Diagnostic(
                "fo-redundant-local-const",
                "warning",
                "test",
                location,
                variable="value",
            )
            diagnostics = validator.apply_suppressions([diagnostic])
        self.assertFalse(diagnostic.suppressed)
        self.assertTrue(any(item.code == "fo-invalid-suppression" for item in diagnostics))


if __name__ == "__main__":
    unittest.main()
