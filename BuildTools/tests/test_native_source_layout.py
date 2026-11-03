"""Regression tests for check_native_source_layout.

The tests are intentionally fixture based (tmp_path only).  They do not assert
that the current checkout already conforms because the source migration is
performed separately.
"""

from __future__ import annotations

from pathlib import Path
import subprocess
import sys

import pytest


BUILDTOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS))
import check_native_source_layout as layout


CHECKER = BUILDTOOLS / "check_native_source_layout.py"


def header(include: str = "Common.h", body: str = "") -> str:
    text = f'#pragma once\n\n#include "{include}"\n'

    return text + ("\n" + body + "\n" if body else "")


def implementation(include: str = "Unit.h", body: str = "") -> str:
    text = f'#include "{include}"\n'

    return text + ("\n" + body + "\n" if body else "")


def project(tmp_path: Path, files: dict[str, str | bytes]) -> Path:
    for name, payload in files.items():
        path = tmp_path / name
        path.parent.mkdir(parents=True, exist_ok=True)

        if isinstance(payload, bytes):
            path.write_bytes(payload)
        else:
            path.write_bytes(payload.encode("utf-8"))

    return tmp_path


def run_cli(*args: object) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(CHECKER), *(str(arg) for arg in args)],
        capture_output=True,
        text=True,
    )


def ordinary_pair(directory: str = "Source/Common") -> dict[str, str]:
    return {
        f"{directory}/Unit.h": header(),
        f"{directory}/Unit.cpp": implementation("Unit.h", "void Run() {}"),
    }


_CONTROL_CASES = [
    (
        "ordinary-pair",
        ordinary_pair(),
        True,
        None,
    ),
    (
        "cpp-only-good",
        {
            "Source/Common/Unit.cpp": implementation("Common.h", "void Run() {}"),
        },
        True,
        None,
    ),
    (
        "terminal-include-good",
        {
            "Source/Common/Unit.cpp": implementation("Common.h"),
        },
        True,
        None,
    ),
    (
        "common-before-own",
        {
            "Source/Common/Unit.h": header(),
            "Source/Common/Unit.cpp": implementation("Common.h", '#include "Unit.h"'),
        },
        False,
        'first include must be "Unit.h"',
    ),
    (
        "extra-eof",
        {
            "Source/Common/Unit.cpp": '#include "Common.h"\n\nvoid Run() {}\n\n',
        },
        False,
        "exactly one line ending",
    ),
    (
        "wrong-first",
        {
            "Source/Common/Unit.cpp": '#include "Other.h"\n\nvoid Run() {}\n',
        },
        False,
        'first include must be "Common.h"',
    ),
    (
        "missing-separator",
        {
            "Source/Common/Unit.cpp": '#include "Common.h"\nvoid Run() {}\n',
        },
        False,
        "blank line must follow the first include",
    ),
    (
        "continued-comment",
        {
            "Source/Common/Unit.cpp": (
                '// continued \\\n'
                '#include "Fake.h"\n'
                '#include "Common.h"\n'
                '\n'
                'void Run() {}\n'),
        },
        True,
        None,
    ),
    (
        "no-first-separation",
        {
            "Source/Common/Unit.h": '#include "Common.h"\n#include "Other.h"\n',
            "Source/Common/Unit.cpp": implementation("Unit.h"),
        },
        False,
        "blank line must follow the first include",
    ),
    (
        "two-eol-only",
        {
            "Source/Common/Unit.h": '#include "Common.h"\n\n',
            "Source/Common/Unit.cpp": implementation("Unit.h"),
        },
        False,
        "exactly one line ending",
    ),
]


@pytest.mark.parametrize(
    "name,files,valid,fragment",
    _CONTROL_CASES,
    ids=[case[0] for case in _CONTROL_CASES],
)
def test_independent_controls(tmp_path: Path, name: str, files: dict[str, str],
                             valid: bool, fragment: str | None) -> None:
    del name
    root = project(tmp_path, files)
    issues = layout.check_layout(root)

    assert (issues == []) is valid

    if fragment is not None:
        assert any(fragment in issue for issue in issues), issues


def test_ordinary_pair_is_valid(tmp_path: Path) -> None:
    root = project(tmp_path, ordinary_pair())

    assert layout.check_layout(root) == []


def test_header_only_file_is_valid(tmp_path: Path) -> None:
    root = project(tmp_path, {"Source/Common/Only.h": header()})

    assert layout.check_layout(root) == []


def test_continued_line_comment_does_not_create_an_include(tmp_path: Path) -> None:
    files = ordinary_pair()
    files["Source/Common/Unit.h"] = '// continued \\\n#include "Fake.h"\n' + header()
    root = project(tmp_path, files)
    assert layout.check_layout(root) == []


def test_continued_comment_in_a_cpp_only_unit_is_masked(tmp_path: Path) -> None:
    root = project(tmp_path, {
        "Source/Common/Unit.cpp": (
            '// continued \\\n'
            '#include "Fake.h"\n'
            '#include "Common.h"\n'
            '\n'
            'void Run() {}\n'),
    })
    assert layout.check_layout(root) == []


def test_standalone_sample_keeps_its_engine_independent_interface(tmp_path: Path) -> None:
    files = {
        "Examples/NativeExtensionSample/SourceExt/NativeExtensionCore.h": (
            '#pragma once\n\n#include <cstdint>\n'),
        "Examples/NativeExtensionSample/SourceExt/NativeExtensionCore.cpp":
            implementation("NativeExtensionCore.h"),
        "Examples/NativeExtensionSample/Tests/NativeExtensionCoreTest.cpp":
            implementation("NativeExtensionCore.h"),
    }
    root = project(tmp_path, files)
    assert layout.check_layout(root, "Examples") == []

    path = root / "Examples/NativeExtensionSample/SourceExt/NativeExtensionCore.h"
    path.write_text(header(), encoding="utf-8")
    assert any("must not include Common.h" in issue for issue in layout.check_layout(root, "Examples"))


def test_standalone_sample_test_does_not_fall_back_to_common(tmp_path: Path) -> None:
    files = {
        "Examples/NativeExtensionSample/SourceExt/NativeExtensionCore.h": (
            '#pragma once\n\n#include <cstdint>\n'),
        "Examples/NativeExtensionSample/Tests/NativeExtensionCoreTest.cpp":
            implementation("Common.h"),
    }
    root = project(tmp_path, files)
    issues = layout.check_layout(root, "Examples")

    assert any('first include must be "NativeExtensionCore.h"' in issue for issue in issues), issues


def test_missing_source_directory_is_an_error(tmp_path: Path) -> None:
    issues = layout.check_layout(tmp_path)

    assert issues
    assert any("does not exist" in issue for issue in issues)


def test_empty_source_directory_is_an_error(tmp_path: Path) -> None:
    (tmp_path / "Source").mkdir()
    issues = layout.check_layout(tmp_path)

    assert issues
    assert any("inventory is empty" in issue for issue in issues)


def test_only_generator_inputs_report_empty_inventory(tmp_path: Path) -> None:
    root = project(tmp_path, {
        "Source/Common/GenericCode.template.cpp": b"",
        "Source/Common/MetadataRegistration.template.cpp": b"",
    })
    issues = layout.check_layout(root)

    assert issues
    assert any("inventory is empty" in issue for issue in issues)


def test_generator_inputs_are_excluded_next_to_a_valid_pair(tmp_path: Path) -> None:
    files = ordinary_pair()
    files["Source/Common/GenericCode.template.cpp"] = b"not native C++"
    files["Source/Common/MetadataRegistration.template.cpp"] = b"not native C++"
    root = project(tmp_path, files)

    assert layout.check_layout(root) == []


def test_non_exact_template_cpp_is_not_exempt(tmp_path: Path) -> None:
    root = project(tmp_path, {
        "Source/Common/Other.template.cpp": implementation("Other.template.h"),
    })
    issues = layout.check_layout(root)

    assert any("Source/Common/Other.template.cpp" in issue
               and 'first include must be "Common.h"' in issue
               for issue in issues), issues


def test_pair_stem_and_case_must_match_exactly(tmp_path: Path) -> None:
    root = project(tmp_path, {
        "Source/Common/Unit.h": header(),
        "Source/Common/unit.cpp": implementation("unit.h"),
    })
    issues = layout.check_layout(root)

    assert any("Source/Common/unit.cpp" in issue
               and 'first include must be "Common.h"' in issue
               for issue in issues), issues


def test_pair_must_be_in_the_same_directory(tmp_path: Path) -> None:
    root = project(tmp_path, {
        "Source/Common/Unit.cpp": implementation("Unit.h"),
        "Source/Other/Unit.h": header(),
    })
    issues = layout.check_layout(root)

    assert any("Source/Common/Unit.cpp" in issue
               and 'first include must be "Common.h"' in issue
               for issue in issues), issues


def test_app_cpp_without_a_header_requires_common(tmp_path: Path) -> None:
    good = project(tmp_path / "good", {
        "Source/Applications/BootApp.cpp": implementation("Common.h"),
    })
    bad = project(tmp_path / "bad", {
        "Source/Applications/BootApp.cpp": implementation("Other.h"),
    })

    assert layout.check_layout(good) == []
    assert any('first include must be "Common.h"' in issue
               for issue in layout.check_layout(bad))


@pytest.mark.parametrize("path", [
    "Source/Applications/Boot.cpp",
    "Source/Applications/Sub/BootApp.cpp",
    "Source/Helpers/Boot.cpp",
    "Source/Common/BootApp.cpp",
    "Source/Essentials/Foo.cpp",
])
def test_cpp_only_units_are_valid_in_varied_directories(tmp_path: Path, path: str) -> None:
    root = project(tmp_path, {path: implementation("Common.h")})

    assert layout.check_layout(root) == []


def test_app_cpp_with_a_matching_header_prefers_its_own_header(tmp_path: Path) -> None:
    root = project(tmp_path, {
        "Source/Applications/BootApp.h": header(),
        "Source/Applications/BootApp.cpp": implementation("Common.h"),
    })
    issues = layout.check_layout(root)

    assert any('first include must be "BootApp.h"' in issue for issue in issues), issues


@pytest.mark.parametrize("path", [
    "Source/Common/Test_Foo.cpp",
    "Source/Tests/Helper.cpp",
    "Source/Foo/Tests/Helper.cpp",
])
def test_test_units_without_headers_use_common(tmp_path: Path, path: str) -> None:
    root = project(tmp_path, {path: implementation("Common.h")})

    assert layout.check_layout(root) == []


def test_nested_test_helper_is_a_cpp_only_unit(tmp_path: Path) -> None:
    good = project(tmp_path / "good", {
        "Source/Tests/Nested/Helper.cpp": implementation("Common.h"),
    })
    bad = project(tmp_path / "bad", {
        "Source/Tests/Nested/Helper.cpp": implementation("Other.h"),
    })

    assert layout.check_layout(good) == []
    assert any('first include must be "Common.h"' in issue
               for issue in layout.check_layout(bad))


@pytest.mark.parametrize("path", [
    "Source/Applications/BootApp.cpp",
    "Source/Common/Test_Foo.cpp",
    "Source/Tests/Helper.cpp",
    "Source/Essentials/Foo.cpp",
])
def test_cpp_only_units_require_common_first(tmp_path: Path, path: str) -> None:
    root = project(tmp_path, {path: implementation("Other.h")})
    issues = layout.check_layout(root)

    assert any('first include must be "Common.h"' in issue for issue in issues), issues


def test_cpp_with_a_header_uses_that_header(tmp_path: Path) -> None:
    good = project(tmp_path / "good", {
        "Source/Tests/Test_Foo.h": header(),
        "Source/Tests/Test_Foo.cpp": implementation("Test_Foo.h"),
    })
    bad = project(tmp_path / "bad", {
        "Source/Tests/Test_Foo.h": header(),
        "Source/Tests/Test_Foo.cpp": implementation("Common.h"),
    })

    assert layout.check_layout(good) == []
    assert any('first include must be "Test_Foo.h"' in issue
               for issue in layout.check_layout(bad))


@pytest.mark.parametrize("content", [
    '#include "Other.h"\n',
    '#include <Common.h>\n',
    '#include COMMON_HEADER\n',
    '#pragma once\n',
])
def test_header_requires_common_as_the_first_quoted_include(tmp_path: Path,
                                                            content: str) -> None:
    root = project(tmp_path, {"Source/Common/Unit.h": content})
    issues = layout.check_layout(root)

    assert any('first include must be "Common.h"' in issue for issue in issues), issues


def test_blank_line_is_required_after_the_first_header_include(tmp_path: Path) -> None:
    root = project(tmp_path, {
        "Source/Common/Unit.h": '#include "Common.h"\n#include "Other.h"\n',
    })
    issues = layout.check_layout(root)

    assert any("blank line must follow the first include" in issue for issue in issues), issues


def test_blank_line_is_required_after_the_first_cpp_include(tmp_path: Path) -> None:
    root = project(tmp_path, {
        "Source/Common/Unit.h": header(),
        "Source/Common/Unit.cpp": '#include "Unit.h"\nvoid Run() {}\n',
    })
    issues = layout.check_layout(root)

    assert any("blank line must follow the first include" in issue for issue in issues), issues


def test_cpp_only_body_requires_a_real_blank_separator(tmp_path: Path) -> None:
    good = project(tmp_path / "good", {
        "Source/Common/Unit.cpp": '#include "Common.h"\n\nvoid Run() {}\n',
    })
    bad = project(tmp_path / "bad", {
        "Source/Common/Unit.cpp": '#include "Common.h"\nvoid Run() {}\n',
    })

    assert layout.check_layout(good) == []
    assert any("blank line must follow the first include" in issue
               for issue in layout.check_layout(bad))


def test_empty_cpp_anchor_ends_directly_after_the_include(tmp_path: Path) -> None:
    root = project(tmp_path, {
        "Source/Common/Unit.h": header(),
        "Source/Common/Unit.cpp": implementation("Unit.h"),
    })

    assert layout.check_layout(root) == []


@pytest.mark.parametrize("payload", [
    b'#include "Common.h"\n',
    b'#include "Common.h"\r\n',
])
def test_cpp_only_terminal_include_accepts_lf_and_crlf(tmp_path: Path,
                                                       payload: bytes) -> None:
    root = project(tmp_path, {"Source/Common/Unit.cpp": payload})

    assert layout.check_layout(root) == []


def test_crlf_cpp_only_with_body_is_valid(tmp_path: Path) -> None:
    root = project(tmp_path, {
        "Source/Common/Unit.cpp": b'#include "Common.h"\r\n\r\nvoid Run() {}\r\n',
    })

    assert layout.check_layout(root) == []


def test_crlf_cpp_only_missing_separator_is_invalid(tmp_path: Path) -> None:
    root = project(tmp_path, {
        "Source/Common/Unit.cpp": b'#include "Common.h"\r\nvoid Run() {}\r\n',
    })
    issues = layout.check_layout(root)

    assert any("blank line must follow the first include" in issue for issue in issues), issues


def test_fake_includes_in_comments_and_strings_are_ignored(tmp_path: Path) -> None:
    content = (
        '// #include "Common.h"\n'
        '/* #include "Common.h" */\n'
        'const char* Fake = "#include \\"Common.h\\"";\n'
        'const char* Raw = R"(\n'
        '#include "Common.h"\n'
        ')";\n'
        '\n'
        '#include "Common.h"\n'
    )
    root = project(tmp_path, {"Source/Common/Unit.h": content})

    assert layout.check_layout(root) == []


def test_fake_includes_do_not_satisfy_the_first_include(tmp_path: Path) -> None:
    content = (
        '// #include "Common.h"\n'
        '/* #include "Common.h" */\n'
        'const char* Fake = "#include \\"Common.h\\"";\n'
        'const char* Raw = R"(\n'
        '#include "Common.h"\n'
        ')";\n'
    )
    root = project(tmp_path, {"Source/Common/Unit.h": content})
    issues = layout.check_layout(root)

    assert any('first include must be "Common.h"' in issue for issue in issues), issues


def test_preprocessor_conditions_are_not_evaluated(tmp_path: Path) -> None:
    content = (
        '#if 0\n'
        '#include "Common.h"\n'
        '\n'
        '#endif\n'
        '#include "Other.h"\n'
    )
    root = project(tmp_path, {"Source/Common/Unit.h": content})

    assert layout.check_layout(root) == []


@pytest.mark.parametrize("path,content", [
    ("Source/Common/Common.h", '#pragma once\n\n#include "Other.h"\n'),
    ("Source/Common/ImGuiExt/ImGuiConfig.h",
     '#include "Other.h"\n#include "Second.h"\n'),
    ("Source/Essentials/Foo.h", '#include "Other.h"\n'),
])
def test_bootstrap_headers_do_not_require_common_first(tmp_path: Path,
                                                       path: str,
                                                       content: str) -> None:
    root = project(tmp_path, {path: content})

    assert layout.check_layout(root) == []


@pytest.mark.parametrize("path", [
    "Source/Common/Common.h",
    "Source/Common/ImGuiExt/ImGuiConfig.h",
    "Source/Essentials/Foo.h",
])
@pytest.mark.parametrize("content", [
    '#include "Common.h"\n',
    '#include <Common.h>\n',
    '#include "Common/Common.h"\n',
])
def test_bootstrap_headers_must_not_include_common(tmp_path: Path, path: str,
                                                   content: str) -> None:
    root = project(tmp_path, {path: content})
    issues = layout.check_layout(root)

    assert any("bootstrap header must not include Common.h" in issue for issue in issues), issues


def test_essentials_subdirectories_are_not_bootstrap(tmp_path: Path) -> None:
    root = project(tmp_path, {
        "Source/Essentials/Detail/Detail.h": '#include "Other.h"\n',
    })
    issues = layout.check_layout(root)

    assert any('first include must be "Common.h"' in issue for issue in issues), issues


def test_bootstrap_headers_still_obey_eof_rules(tmp_path: Path) -> None:
    root = project(tmp_path, {"Source/Essentials/Foo.h": '#include "Other.h"\n\n'})
    issues = layout.check_layout(root)

    assert any("exactly one line ending" in issue for issue in issues), issues


@pytest.mark.parametrize("name,payload,valid", [
    ("valid-lf", b'#include "Common.h"\n', True),
    ("valid-crlf", b'#include "Common.h"\r\n', True),
    ("two-eol", b'#include "Common.h"\n\n', False),
    ("extra-eof", b'#include "Common.h"\n\n\n', False),
    ("extra-crlf", b'#include "Common.h"\r\n\r\n', False),
    ("no-eol", b'#include "Common.h"', False),
    ("bare-cr", b'#include "Common.h"\r', False),
    ("trailing-spaces", b'#include "Common.h"   \n', False),
    ("trailing-tab", b'#include "Common.h"\t\n', False),
    ("space-only-line", b'#include "Common.h"\n  \n', False),
    ("extra-eol-after-body", b'#include "Common.h"\n\nvoid Foo();\n\n', False),
    ("trailing-spaces-after-body", b'#include "Common.h"\n\nvoid Foo();  \n', False),
    ("empty-file", b"", False),
    ("only-eol", b"\n", False),
    ("whitespace-only-content", b"  \n", False),
])
def test_exact_eof_contract(tmp_path: Path, name: str, payload: bytes,
                            valid: bool) -> None:
    del name
    root = project(tmp_path, {"Source/Common/Unit.h": payload})

    assert (layout.check_layout(root) == []) is valid


def test_eof_is_enforced_for_test_units(tmp_path: Path) -> None:
    root = project(tmp_path, {"Source/Tests/Helper.cpp": '#include "Common.h"\n\n'})
    issues = layout.check_layout(root)

    assert any("exactly one line ending" in issue for issue in issues), issues


def test_utf8_bom_is_accepted(tmp_path: Path) -> None:
    payload = b"\xef\xbb\xbf" + b'#include "Common.h"\n'
    root = project(tmp_path, {"Source/Common/Unit.h": payload})

    assert layout.check_layout(root) == []


def test_invalid_utf8_is_reported(tmp_path: Path) -> None:
    root = project(tmp_path, {"Source/Common/Unit.h": b"\xff\xfe\x00"})
    issues = layout.check_layout(root)

    assert any("UTF-8" in issue for issue in issues), issues


def test_crlf_pair_is_valid(tmp_path: Path) -> None:
    root = project(tmp_path, {
        "Source/Common/Unit.h": (
            b'#pragma once\r\n\r\n#include "Common.h"\r\n'),
        "Source/Common/Unit.cpp": b'#include "Unit.h"\r\n',
    })

    assert layout.check_layout(root) == []


@pytest.mark.parametrize("source_dir", [
    "",
    "../Source",
    "Source/../Elsewhere",
    "/absolute",
    "C:\\absolute",
])
def test_source_dir_must_be_root_relative(tmp_path: Path, source_dir: str) -> None:
    root = project(tmp_path, {"Source/Common/Unit.h": header()})
    issues = layout.check_layout(root, source_dir=source_dir)

    assert issues
    assert any("root-relative" in issue for issue in issues), issues


def test_dot_source_dir_checks_root_files(tmp_path: Path) -> None:
    root = project(tmp_path, {
        "Unit.h": header(),
        "Unit.cpp": implementation("Unit.h"),
    })

    assert layout.check_layout(root, source_dir=".") == []


def test_custom_source_directory_is_supported(tmp_path: Path) -> None:
    root = project(tmp_path, ordinary_pair("Native/Common"))

    assert layout.check_layout(root, source_dir="Native") == []
    assert any("does not exist" in issue for issue in layout.check_layout(root))


def test_custom_source_dir_diagnostics_are_root_relative(tmp_path: Path) -> None:
    root = project(tmp_path, {
        "Native/Common/Broken.cpp": implementation("Broken.h"),
    })
    issues = layout.check_layout(root, source_dir="Native")

    assert issues
    assert any("Native/Common/Broken.cpp" in issue for issue in issues), issues
    assert all(str(tmp_path) not in issue for issue in issues), issues


def test_source_dir_subtree_keeps_root_relative_bootstrap_rules(tmp_path: Path) -> None:
    root = project(tmp_path, {
        "Source/Essentials/Foo.h": '#include "Other.h"\n',
        "Source/Common/Common.h": '#pragma once\n\n#include "Other.h"\n',
    })

    assert layout.check_layout(root, source_dir="Source/Essentials") == []
    assert layout.check_layout(root, source_dir="Source/Common") == []


def test_source_path_must_be_a_directory(tmp_path: Path) -> None:
    root = project(tmp_path, {"Source": b"not a directory"})
    issues = layout.check_layout(root)

    assert any("not a directory" in issue for issue in issues), issues


def test_non_native_files_are_ignored(tmp_path: Path) -> None:
    root = project(tmp_path, {
        "Source/Common/Unit.h": header(),
        "Source/Common/README.md": b'#include "bad"\n',
        "Source/Common/data.json": b"{",
    })

    assert layout.check_layout(root) == []


def test_native_extensions_are_case_sensitive(tmp_path: Path) -> None:
    root = project(tmp_path, {
        "Source/Common/Unit.H": b'#include "Other.h"\n',
        "Source/Common/Unit.CPP": b'#include "Other.h"\n',
    })
    issues = layout.check_layout(root)

    assert any("inventory is empty" in issue for issue in issues), issues


def test_default_source_dir_ignores_top_level_vendor_trees(tmp_path: Path) -> None:
    files = ordinary_pair()
    files["SourceExt/Vendor/Bad.h"] = '#include "Other.h"\n'
    root = project(tmp_path, files)

    assert layout.check_layout(root) == []


def test_shallow_limits_to_direct_children(tmp_path: Path) -> None:
    files = {
        "Source/Unit.h": header(),
        "Source/Unit.cpp": implementation("Unit.h"),
        "Source/Vendor/Native/Bad.h": '#include "Other.h"\n',
    }
    root = project(tmp_path, files)

    assert layout.check_layout(root, shallow=True) == []

    deep_issues = layout.check_layout(root, shallow=False)
    assert any("Source/Vendor/Native/Bad.h" in issue for issue in deep_issues), deep_issues


def test_shallow_source_dir_excludes_nested_vendors(tmp_path: Path) -> None:
    files = {
        "Source/Vendor/Ok.h": header(),
        "Source/Vendor/ThirdParty/Bad.h": '#include "Other.h"\n',
    }
    root = project(tmp_path, files)

    assert layout.check_layout(root, source_dir="Source/Vendor", shallow=True) == []

    deep_issues = layout.check_layout(root, source_dir="Source/Vendor", shallow=False)
    assert any("Source/Vendor/ThirdParty/Bad.h" in issue for issue in deep_issues), deep_issues


def test_shallow_scope_with_only_nested_files_is_empty_inventory(tmp_path: Path) -> None:
    root = project(tmp_path, {"Source/Vendor/Native/Bad.h": '#include "Other.h"\n'})
    issues = layout.check_layout(root, shallow=True)

    assert issues
    assert any("inventory is empty" in issue for issue in issues), issues


def test_diagnostics_are_sorted_relative_and_deduplicated(tmp_path: Path) -> None:
    root = project(tmp_path, {
        "Source/Common/Bad1.h": '#pragma once\n',
        "Source/Common/Bad2.h": '#include "Other.h"\n',
        "Source/Common/Missing.cpp": implementation("Missing.h"),
    })
    issues = layout.check_layout(root)

    assert issues
    assert issues == sorted(issues)
    assert all(issue.startswith("Source/") for issue in issues), issues
    assert all(str(tmp_path) not in issue for issue in issues), issues
    assert all("\\" not in issue for issue in issues), issues


def test_include_diagnostics_carry_the_source_line(tmp_path: Path) -> None:
    root = project(tmp_path, {
        "Source/Common/Unit.h": '#pragma once\n\n#include "Other.h"\n',
    })
    issues = layout.check_layout(root)

    assert any(issue.startswith("Source/Common/Unit.h:3:") for issue in issues), issues


def test_cli_accepts_a_clean_project(tmp_path: Path) -> None:
    root = project(tmp_path, ordinary_pair())
    result = run_cli("--root", root)

    assert result.returncode == 0
    assert result.stdout == ""


def test_cli_rejects_a_bad_project_with_a_relative_path(tmp_path: Path) -> None:
    root = project(tmp_path, {
        "Source/Common/Orphan.cpp": implementation("Orphan.h"),
    })
    result = run_cli("--root", root)
    output = result.stdout + result.stderr

    assert result.returncode == 1
    assert "Source/Common/Orphan.cpp" in output
    assert str(tmp_path) not in output


def test_cli_shallow_skips_nested_vendor_files(tmp_path: Path) -> None:
    root = project(tmp_path, {
        "Source/Unit.h": header(),
        "Source/Unit.cpp": implementation("Unit.h"),
        "Source/Vendor/Native/Bad.h": '#include "Other.h"\n',
    })

    assert run_cli("--root", root, "--shallow").returncode == 0
    assert run_cli("--root", root).returncode == 1


def test_cli_accepts_a_custom_source_dir(tmp_path: Path) -> None:
    root = project(tmp_path, ordinary_pair("Native/Common"))
    result = run_cli("--root", root, "--source-dir", "Native")

    assert result.returncode == 0
    assert result.stdout == ""


def test_check_is_read_only(tmp_path: Path) -> None:
    root = project(tmp_path, ordinary_pair())
    before = {
        path.relative_to(root).as_posix(): path.read_bytes()
        for path in root.rglob("*")
        if path.is_file()
    }

    layout.check_layout(root)

    after = {
        path.relative_to(root).as_posix(): path.read_bytes()
        for path in root.rglob("*")
        if path.is_file()
    }
    assert after == before


def test_symlinks_outside_the_root_are_not_followed(tmp_path: Path) -> None:
    outside = tmp_path.parent / f"{tmp_path.name}-outside"
    outside.mkdir(exist_ok=True)
    (outside / "Outside.h").write_bytes(b'#include "Other.h"\n')

    root = project(tmp_path, ordinary_pair())
    link = tmp_path / "Source/Common/Outside.h"

    try:
        link.symlink_to(outside / "Outside.h")
    except (OSError, NotImplementedError):
        pytest.skip("file symlinks are unavailable")

    assert layout.check_layout(root) == []


def test_symlinks_inside_the_root_are_checked(tmp_path: Path) -> None:
    root = project(tmp_path, {
        "Source/Common/Real.h": header(),
        "Source/Common/Unit.cpp": implementation("Unit.h"),
    })
    link = tmp_path / "Source/Common/Unit.h"

    try:
        link.symlink_to(tmp_path / "Source/Common/Real.h")
    except (OSError, NotImplementedError):
        pytest.skip("file symlinks are unavailable")

    assert layout.check_layout(root) == []


def test_source_directory_symlink_outside_the_root_is_rejected(tmp_path: Path) -> None:
    outside = tmp_path.parent / f"{tmp_path.name}-outside-source"
    outside.mkdir(exist_ok=True)
    (outside / "Outside.h").write_bytes(b'#include "Other.h"\n')

    try:
        (tmp_path / "Source").symlink_to(outside, target_is_directory=True)
    except (OSError, NotImplementedError):
        pytest.skip("directory symlinks are unavailable")

    issues = layout.check_layout(tmp_path)

    assert issues
    assert any("resolves outside the root" in issue for issue in issues), issues
