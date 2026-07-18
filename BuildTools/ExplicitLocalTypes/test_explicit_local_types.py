from __future__ import annotations

import json
import os
import shutil
import tempfile
import unittest
from pathlib import Path

import explicit_local_types as explicit_types


FIXTURE = r'''
using int16_t = short;
using int32_t = int;
using float32_t = float;
using float64_t = double;

namespace qualified
{
enum namespaced_kind
{
    namespaced_value,
};

struct simple_record
{
};

auto GetRecord() -> simple_record;
}

namespace fo
{
struct engine_type
{
};

auto GetEngineType() -> engine_type;

void EngineScope()
{
    auto engine_local = GetEngineType();
}
}

enum simple_kind
{
    simple_value,
};

struct CamelCase
{
};

struct pair_data
{
    int first;
    int second;
};

struct owner
{
    struct nested_type
    {
    };

    static auto GetNested() -> nested_type;
};

template<typename T>
struct container
{
};

template<int Size>
struct matrix
{
};

using mat44 = matrix<4>;
using other_mat = matrix<4>;

using count_t = int;

auto GetSimpleKind() -> simple_kind;
auto GetAliasCount() -> count_t;
auto Translate(const mat44&) -> matrix<4>;
auto SelectMatrix(const mat44&, const other_mat&) -> matrix<4>;
auto BuildMatrix() -> matrix<4>;

template<typename T>
void Instantiated(T value)
{
    auto inconsistent = value;
}

void ValidateCases()
{
    auto integer = 1;
    auto short_integer = static_cast<short>(1);
    auto floating = 1.0F;
    auto double_floating = 1.0;
    auto platform_long = static_cast<long>(1);
    auto kind = GetSimpleKind();
    auto camel = CamelCase {};
    auto templated = container<int> {};
    auto namespaced = qualified::namespaced_value;
    auto alias = GetAliasCount();
    auto alias_backed_matrix = Translate(mat44 {});
    auto ambiguous_matrix_alias = SelectMatrix(mat44 {}, other_mat {});
    auto unspelled_matrix_alias = BuildMatrix();
    auto namespace_record = qualified::GetRecord();
    auto nested = owner::GetNested();
    auto outside_fo = fo::GetEngineType();

    int values[2] {};
    auto decayed = values;
    auto* pointer = &integer;
    const int const_integer = 2;
    auto& const_reference = const_integer;
    const auto& authored_const_reference = integer;
    const auto top_level_const_pointer = &integer;
    auto [first, second] = pair_data {};

    Instantiated(1);
    Instantiated(1.0F);
}
'''


class ExplicitLocalTypesIntegrationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.clang_query = explicit_types.clang_support.discover_tool(
            None,
            "FO_CLANG_QUERY",
            ("clang-query-20", "clang-query"),
        )
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

    def test_only_exact_simple_types_are_rewritten(self) -> None:
        with tempfile.TemporaryDirectory(prefix="explicit_local_types_") as temp_text:
            project = Path(temp_text)
            engine = project / "Engine"
            source_root = engine / "Source"
            source_root.mkdir(parents=True)
            source = source_root / "fixture.cpp"
            source.write_text(FIXTURE, encoding="utf-8")
            vendored_source = source_root / "vendor" / "fixture.cpp"
            vendored_source.parent.mkdir()
            vendored_source.write_text("void VendorCase() { auto vendored = 1; }\n", encoding="utf-8")
            command = f'"{self.compiler}" -std=c++20 -c "{source}"'
            vendored_command = f'"{self.compiler}" -std=c++20 -c "{vendored_source}"'
            database = [
                {"directory": str(project), "command": command, "file": str(source)},
                {"directory": str(project), "command": vendored_command, "file": str(vendored_source)},
            ]
            (project / "compile_commands.json").write_text(json.dumps(database), encoding="utf-8")

            result = explicit_types.run_inventory(
                engine_root=engine,
                database_dir=project,
                source_pattern="explicit_local_types_",
                jobs=1,
                batch_size=1,
                timeout=120,
                clang_query_path=str(self.clang_query),
                unit_pattern=r"^Source/fixture\.cpp$",
            )

            self.assertEqual([unit.canonical_path for unit in result.units], ["Source/fixture.cpp"])
            replacements = {candidate.declarations: candidate.replacement for candidate in result.candidates}
            self.assertEqual(replacements[("integer",)], "int32_t")
            self.assertEqual(replacements[("short_integer",)], "int16_t")
            self.assertEqual(replacements[("floating",)], "float32_t")
            self.assertEqual(replacements[("double_floating",)], "float64_t")
            self.assertEqual(replacements[("kind",)], "simple_kind")
            self.assertEqual(replacements[("pointer",)], "int32_t")
            self.assertEqual(replacements[("const_reference",)], "const int32_t")
            self.assertEqual(replacements[("authored_const_reference",)], "int32_t")
            self.assertEqual(replacements[("alias",)], "count_t")
            self.assertEqual(replacements[("alias_backed_matrix",)], "mat44")
            self.assertEqual(replacements[("engine_local",)], "engine_type")
            self.assertNotIn(("inconsistent",), replacements)
            self.assertNotIn(("vendored",), replacements)

            for excluded in (
                "camel",
                "ambiguous_matrix_alias",
                "unspelled_matrix_alias",
                "platform_long",
                "templated",
                "namespaced",
                "decayed",
                "namespace_record",
                "nested",
                "outside_fo",
                "top_level_const_pointer",
                "first",
                "second",
            ):
                self.assertNotIn((excluded,), replacements)

            applied = explicit_types.apply_candidates(result, engine)
            rewritten = source.read_text(encoding="utf-8")

        self.assertEqual(applied, 11)
        self.assertIn("int32_t integer = 1;", rewritten)
        self.assertIn("int16_t short_integer = static_cast<short>(1);", rewritten)
        self.assertIn("float32_t floating = 1.0F;", rewritten)
        self.assertIn("float64_t double_floating = 1.0;", rewritten)
        self.assertIn("auto platform_long = static_cast<long>(1);", rewritten)
        self.assertIn("simple_kind kind = GetSimpleKind();", rewritten)
        self.assertIn("int32_t* pointer = &integer;", rewritten)
        self.assertIn("const int32_t& const_reference = const_integer;", rewritten)
        self.assertIn("const int32_t& authored_const_reference = integer;", rewritten)
        self.assertIn("count_t alias = GetAliasCount();", rewritten)
        self.assertIn("mat44 alias_backed_matrix = Translate(mat44 {});", rewritten)
        self.assertIn("auto ambiguous_matrix_alias = SelectMatrix(mat44 {}, other_mat {});", rewritten)
        self.assertIn("auto unspelled_matrix_alias = BuildMatrix();", rewritten)
        self.assertIn("auto inconsistent = value;", rewritten)
        self.assertIn("auto decayed = values;", rewritten)
        self.assertIn("const auto top_level_const_pointer = &integer;", rewritten)

    def test_type_shape_accepts_only_one_simple_base_name(self) -> None:
        self.assertEqual(explicit_types.parse_type_shape("const simple_type *").base, "simple_type")
        self.assertIsNone(explicit_types.parse_type_shape("CamelCase"))
        self.assertIsNone(explicit_types.parse_type_shape("std::string"))
        self.assertIsNone(explicit_types.parse_type_shape("container<int>"))
        self.assertIsNone(explicit_types.parse_type_shape("unsigned int"))


if __name__ == "__main__":
    unittest.main()
