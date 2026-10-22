from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys

import pytest


BUILDTOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS))
import check_native_modules as modules


def project(tmp_path: Path, files: dict[str, str], registered: list[str] | None = None,
            exceptions: dict | None = None) -> Path:
    for name, text in files.items():
        path = tmp_path / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    inventory = tmp_path / "BuildTools/cmake/stages/EngineSources.cmake"
    inventory.parent.mkdir(parents=True, exist_ok=True)
    paths = list(files) if registered is None else registered
    inventory.write_text("AppendList(SOURCES\n" + "".join(
        f'    "${{FO_ENGINE_ROOT}}/{name}"\n' for name in paths) + ")\n", encoding="utf-8")
    (tmp_path / "BuildTools/NativeModuleExceptions.json").write_text(json.dumps(
        exceptions or {"Version": 1, "HeaderOnly": {}, "ImplementationOnly": {}}), encoding="utf-8")
    return tmp_path


def test_checkout_conforms() -> None:
    assert modules.check_modules(BUILDTOOLS.parent) == []


def test_complete_registered_pair_is_valid(tmp_path: Path) -> None:
    root = project(tmp_path, {"Source/Common/Example.h": "#pragma once\n",
                              "Source/Common/Example.cpp": '#include "Example.h"\n'})
    assert modules.check_modules(root) == []


@pytest.mark.parametrize("path,text", [
    ("Source/Common/ResourceDefaults.h", "#pragma once\ninline constexpr int Value = 1;\n"),
    ("Source/Common/NewHelpers.h", "#pragma once\n"),
    ("Source/Tools/NewApp.cpp", "void Run() {}\n"),
    ("Source/Tests/Test_Container/Module.cpp", "void Run() {}\n"),
])
def test_new_unpaired_module_is_rejected(tmp_path: Path, path: str, text: str) -> None:
    root = project(tmp_path, {path: text})
    assert any(path in issue for issue in modules.check_modules(root))


@pytest.mark.parametrize("missing", ["Source/Common/Example.h", "Source/Common/Example.cpp"])
def test_both_pair_members_must_be_registered(tmp_path: Path, missing: str) -> None:
    files = {"Source/Common/Example.h": "#pragma once\n", "Source/Common/Example.cpp": ""}
    root = project(tmp_path, files, [name for name in files if name != missing])
    assert any(missing in issue for issue in modules.check_modules(root))


def test_comments_do_not_register_a_module(tmp_path: Path) -> None:
    root = project(tmp_path, {"Source/Common/Example.h": "", "Source/Common/Example.cpp": ""}, [])
    inventory = root / "BuildTools/cmake/stages/EngineSources.cmake"
    inventory.write_text('# "${FO_ENGINE_ROOT}/Source/Common/Example.h"\n'
                         '# "${FO_ENGINE_ROOT}/Source/Common/Example.cpp"\n', encoding="utf-8")
    assert modules.check_modules(root)


def test_case_mismatch_does_not_form_a_pair(tmp_path: Path) -> None:
    root = project(tmp_path, {"Source/Common/Example.h": "", "Source/Common/example.cpp": ""})
    assert modules.check_modules(root)


def test_reviewed_header_and_implementation_exceptions_are_valid(tmp_path: Path) -> None:
    root = project(tmp_path, {"Source/Common/Templates.h": "", "Source/Applications/Boot.cpp": "",
                              "Source/Tests/Test_Example.cpp": ""}, exceptions={
        "Version": 1,
        "HeaderOnly": {"Source/Common/Templates.h": "Template utility, no independent subsystem."},
        "ImplementationOnly": {"Source/Applications/Boot.cpp": "Application entry point."},
    })
    assert modules.check_modules(root) == []


@pytest.mark.parametrize("path,reason", [
    ("Source/Common/Absent.h", "Removed helper."),
    ("Source/Common/Example.h", ""),
    ("Source/Common/*.h", "All headers."),
])
def test_invalid_or_stale_exceptions_are_rejected(tmp_path: Path, path: str, reason: str) -> None:
    root = project(tmp_path, {"Source/Common/Example.h": "", "Source/Common/Example.cpp": ""},
                   exceptions={"Version": 1, "HeaderOnly": {path: reason}, "ImplementationOnly": {}})
    assert modules.check_modules(root)


def test_angelscript_sources_use_their_real_owner_alias(tmp_path: Path) -> None:
    paths = {"Source/Scripting/AngelScript/Example.h": "", "Source/Scripting/AngelScript/Example.cpp": ""}
    root = project(tmp_path, paths, [])
    owner = root / "BuildTools/cmake/stages/CoreLibs.cmake"
    owner.write_text('SetValue(FO_ANGELSCRIPT_SCRIPTING_DIR "${FO_ENGINE_ROOT}/Source/Scripting/AngelScript")\n'
                     'SetValue(SOURCES "${FO_ANGELSCRIPT_SCRIPTING_DIR}/Example.h"\n'
                     ' "${FO_ANGELSCRIPT_SCRIPTING_DIR}/Example.cpp")\n', encoding="utf-8")
    assert modules.check_modules(root) == []


def test_cli_reports_a_bad_module_and_fails(tmp_path: Path) -> None:
    root = project(tmp_path, {"Source/Common/ResourceDefaults.h": "inline constexpr int Value = 1;\n"})
    result = subprocess.run([sys.executable, str(BUILDTOOLS / "check_native_modules.py"), "--root", str(root)],
                            capture_output=True, text=True)
    assert result.returncode == 1
    assert "Source/Common/ResourceDefaults.h" in result.stdout + result.stderr


@pytest.mark.parametrize("equals", ["", "="])
def test_cmake_block_comments_do_not_register_modules(tmp_path: Path, equals: str) -> None:
    root = project(tmp_path, {"Source/Common/Example.h": "", "Source/Common/Example.cpp": ""}, [])
    inventory = root / "BuildTools/cmake/stages/EngineSources.cmake"
    inventory.write_text(f"#[{equals}[\nAppendList(SOURCES\n"
                         ' "${FO_ENGINE_ROOT}/Source/Common/Example.h"\n'
                         ' "${FO_ENGINE_ROOT}/Source/Common/Example.cpp")\n'
                         f"]{equals}]\n", encoding="utf-8")
    assert modules.check_modules(root)


def test_diagnostic_literals_are_not_build_registration(tmp_path: Path) -> None:
    root = project(tmp_path, {"Source/Common/Example.h": "", "Source/Common/Example.cpp": ""}, [])
    inventory = root / "BuildTools/cmake/stages/EngineSources.cmake"
    inventory.write_text('message("${FO_ENGINE_ROOT}/Source/Common/Example.h")\n'
                         'message("${FO_ENGINE_ROOT}/Source/Common/Example.cpp")\n', encoding="utf-8")
    assert modules.check_modules(root)


def test_boolean_is_not_an_exception_schema_version(tmp_path: Path) -> None:
    root = project(tmp_path, {"Source/Common/Example.h": "", "Source/Common/Example.cpp": ""},
                   exceptions={"Version": True, "HeaderOnly": {}, "ImplementationOnly": {}})
    assert modules.check_modules(root)


def test_duplicate_exception_json_keys_are_rejected(tmp_path: Path) -> None:
    root = project(tmp_path, {"Source/Common/Example.h": "", "Source/Common/Example.cpp": ""})
    (root / "BuildTools/NativeModuleExceptions.json").write_text(
        '{"Version":1,"Version":1,"HeaderOnly":{},"ImplementationOnly":{}}', encoding="utf-8")
    assert modules.check_modules(root)


def test_missing_source_directory_is_not_a_clean_checkout(tmp_path: Path) -> None:
    root = project(tmp_path, {})
    assert modules.check_modules(root)


def test_empty_source_directory_is_not_a_clean_checkout(tmp_path: Path) -> None:
    root = project(tmp_path, {})
    (root / "Source").mkdir()
    assert modules.check_modules(root)


def test_dot_segments_are_not_exact_exception_paths(tmp_path: Path) -> None:
    root = project(tmp_path, {"Source/Common/Example.h": "", "Source/Common/Example.cpp": ""},
                   exceptions={"Version": 1, "HeaderOnly": {"Source/Common/./Example.h": "Utility."},
                               "ImplementationOnly": {}})
    assert modules.check_modules(root)


def test_cli_accepts_a_clean_project(tmp_path: Path) -> None:
    root = project(tmp_path, {"Source/Common/Example.h": "", "Source/Common/Example.cpp": ""})
    result = subprocess.run([sys.executable, str(BUILDTOOLS / "check_native_modules.py"), "--root", str(root)],
                            capture_output=True, text=True)
    assert result.returncode == 0
    assert result.stdout == ""


def test_malformed_exception_json_is_reported(tmp_path: Path) -> None:
    root = project(tmp_path, {"Source/Common/Example.h": "", "Source/Common/Example.cpp": ""})
    (root / "BuildTools/NativeModuleExceptions.json").write_text("{ not json", encoding="utf-8")
    assert modules.check_modules(root)


def test_cli_fails_on_malformed_exception_configuration(tmp_path: Path) -> None:
    root = project(tmp_path, {"Source/Common/Example.h": "", "Source/Common/Example.cpp": ""})
    (root / "BuildTools/NativeModuleExceptions.json").write_text("[]", encoding="utf-8")
    result = subprocess.run([sys.executable, str(BUILDTOOLS / "check_native_modules.py"), "--root", str(root)],
                            capture_output=True, text=True)
    assert result.returncode == 1
    assert "NativeModuleExceptions.json" in result.stdout + result.stderr


def test_wrong_exception_version_is_rejected(tmp_path: Path) -> None:
    root = project(tmp_path, {"Source/Common/Example.h": "", "Source/Common/Example.cpp": ""}, exceptions={
        "Version": 2, "HeaderOnly": {}, "ImplementationOnly": {}})
    assert any("version" in issue for issue in modules.check_modules(root))


def test_exception_section_must_match_the_file_extension(tmp_path: Path) -> None:
    root = project(tmp_path, {"Source/Common/Templates.h": ""}, exceptions={
        "Version": 1,
        "HeaderOnly": {},
        "ImplementationOnly": {"Source/Common/Templates.h": "Header listed as an implementation."}})
    assert any("Templates.h" in issue for issue in modules.check_modules(root))


def test_exception_for_a_paired_module_is_rejected(tmp_path: Path) -> None:
    root = project(tmp_path, {"Source/Common/Example.h": "", "Source/Common/Example.cpp": ""}, exceptions={
        "Version": 1,
        "HeaderOnly": {"Source/Common/Example.h": "Would hide a complete module pair."},
        "ImplementationOnly": {}})
    assert any("Example.h" in issue for issue in modules.check_modules(root))
