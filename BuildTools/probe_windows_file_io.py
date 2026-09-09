from __future__ import annotations

import argparse
import hashlib
import json
import ntpath
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


PAYLOAD = "FOnline native long-path probe\n"
NATIVE_SOURCE = r'''
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <system_error>
#include <windows.h>

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2) return 2;
    const std::filesystem::path path{argv[1]};
    errno = 0;
    SetLastError(0);
    std::ifstream stream{path, std::ios::binary};
    const int open_errno = errno;
    const DWORD open_win32_error = GetLastError();
    const bool opened = stream.is_open();
    const std::string content{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
    std::error_code ec;
    std::filesystem::directory_iterator entry{path.has_parent_path() ? path.parent_path() : std::filesystem::path{"."}, ec};
    bool found = false;
    while (!ec && entry != std::filesystem::directory_iterator{}) {
        if (entry->path().filename() == path.filename()) found = true;
        entry.increment(ec);
    }
    std::cout << "{\"ifstream_open\":" << (opened ? "true" : "false")
              << ",\"content_matches\":" << (content == "FOnline native long-path probe\n" ? "true" : "false")
              << ",\"open_errno\":" << open_errno
              << ",\"open_win32_error\":" << open_win32_error
              << ",\"enumeration_completed\":" << (!ec ? "true" : "false")
              << ",\"enumerated_file\":" << (found ? "true" : "false")
              << ",\"enumeration_error\":" << ec.value() << "}\n";
    return 0;
}
'''
CMAKE_SOURCE = '''cmake_minimum_required(VERSION 3.22)
project(FOnlineWindowsFileIOProbe CXX)
if(NOT MSVC)
    message(FATAL_ERROR "The probe requires the native MSVC ABI")
endif()
foreach(runtime static dynamic)
    add_executable(file_io_${runtime} probe.cpp)
    target_compile_features(file_io_${runtime} PRIVATE cxx_std_20)
    target_compile_options(file_io_${runtime} PRIVATE /W4 /WX /utf-8)
endforeach()
foreach(runtime static dynamic)
    add_executable(engine_io_${runtime} engine_probe.cpp)
    target_compile_features(engine_io_${runtime} PRIVATE cxx_std_20)
    target_compile_options(engine_io_${runtime} PRIVATE /W4 /WX /utf-8)
endforeach()
set_property(TARGET engine_io_static PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
set_property(TARGET engine_io_dynamic PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
set_property(TARGET file_io_static PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
set_property(TARGET file_io_dynamic PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
'''


ENGINE_SOURCE = Path(__file__).resolve().parents[1] / "Source/Essentials/DiskFileSystem.cpp"
ENGINE_SIGNATURES = (
    "auto fs_make_path(string_view path)",
    "static auto fs_make_io_path(string_view path, std::error_code& ec, bool force_extended)",
    "auto fs_create_directories(string_view dir)",
    "auto fs_write_file(string_view path, string_view content)",
    "auto fs_open_ifstream(string_view path, std::ios::openmode mode)",
    "auto fs_rename(string_view from_path, string_view to_path)",
    "auto fs_remove_file(string_view path)",
    "auto fs_remove_dir_tree(string_view dir)",
)
ENGINE_HARNESS = r"""
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#define FO_WINDOWS 1
#define FO_STACK_TRACE_ENTRY() ((void)0)
#define FO_NO_STACK_TRACE_ENTRY() ((void)0)
#define ignore_unused(...) ((void)(__VA_ARGS__))
using string_view = std::string_view;
"""
ENGINE_MAIN = r"""
static std::string utf8(const std::filesystem::path& path)
{
    const auto value = path.u8string();
    return {value.begin(), value.end()};
}
int wmain(int argc, wchar_t** argv)
{
    if (argc != 3 && argc != 4) return 2;
    const std::string input = utf8(std::filesystem::path{argv[2]});
    std::error_code ec;
    const auto resolved = fs_make_io_path(input, ec);
    if (std::wstring_view{argv[1]} == L"remove_tree") {
        if (argc != 4 || ec) return 6;
        const std::string child = utf8(std::filesystem::path{argv[3]});
        const bool created_before = fs_write_file(child, "descendant");
        if (!created_before) return 7;
        const auto removed_before = std::filesystem::remove_all(resolved, ec);
        const int before_error = ec.value();
        std::error_code exists_error;
        const bool root_remained = std::filesystem::exists(resolved, exists_error);
        const bool negative_reproduced = before_error != 0 && root_remained && !exists_error;
        const bool created_after = fs_write_file(child, "descendant");
        const bool removed_after = fs_remove_dir_tree(input);
        const bool root_gone = !std::filesystem::exists(resolved, exists_error) && !exists_error;
        const auto resolved_child = fs_make_io_path(child, exists_error);
        const bool child_gone = !exists_error && !std::filesystem::exists(resolved_child, exists_error) && !exists_error;
        std::cout << "{\"root_utf16_length\":" << resolved.native().size()
                  << ",\"child_utf16_length\":" << std::filesystem::path{argv[3]}.native().size()
                  << ",\"unprefixed_root\":" << (!resolved.native().starts_with(LR"(\\?\)") ? "true" : "false")
                  << ",\"before_error\":" << before_error
                  << ",\"before_removed_count\":" << removed_before
                  << ",\"negative_reproduced\":" << (negative_reproduced ? "true" : "false")
                  << ",\"created_after\":" << (created_after ? "true" : "false")
                  << ",\"removed_after\":" << (removed_after ? "true" : "false")
                  << ",\"root_gone\":" << (root_gone ? "true" : "false")
                  << ",\"child_gone\":" << (child_gone ? "true" : "false") << "}\n";
        return created_after && removed_after && root_gone && child_gone ? 0 : 8;
    }
    if (std::wstring_view{argv[1]} == L"directory_names") {
        const std::string literal = utf8(fs_make_io_path(input, ec, true)) + ". ";
        const bool written = !ec && fs_write_file(input + "\\child.bin", "ordinary") && fs_write_file(literal + "\\child.bin", "literal");
        const bool ordinary_removed = fs_remove_dir_tree(input + ". ");
        const bool ordinary_gone = !std::filesystem::exists(resolved, ec) && !ec;
        bool literal_retained = false;
        {
            auto stream = fs_open_ifstream(literal + "\\child.bin", std::ios::binary);
            const std::string payload{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
            literal_retained = stream.is_open() && payload == "literal";
        }
        const bool literal_removed = fs_remove_dir_tree(literal);
        const bool passed = written && ordinary_removed && ordinary_gone && literal_retained && literal_removed;
        std::cout << (passed ? "true" : "false");
        return passed ? 0 : 9;
    }
    if (std::wstring_view{argv[1]} == L"names") {
        const std::string literal = utf8(resolved) + ". ";
        const bool written = fs_write_file(input, "ordinary") && fs_write_file(literal, "literal");
        const auto read = [](const std::string& name) {
            auto stream = fs_open_ifstream(name, std::ios::binary);
            return std::string{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
        };
        const bool matched = written && read(input + ". ") == "ordinary" && read(literal) == "literal";
        const bool removed = fs_remove_file(input) && fs_remove_file(literal);
        std::cout << (matched && removed ? "true" : "false");
        return matched && removed ? 0 : 5;
    }
    if (std::wstring_view{argv[1]} == L"resolve" || std::wstring_view{argv[1]} == L"resolve_recursive") {
        if (ec) return 3;
        const auto output = std::wstring_view{argv[1]} == L"resolve_recursive" ? fs_make_io_path(input, ec, true) : resolved;
        if (ec) return 3;
        std::cout << utf8(output);
        return 0;
    }
    const bool written = fs_write_file(input, "FOnline native long-path probe\n");
    bool read = false;
    {
        auto stream = fs_open_ifstream(input, std::ios::binary);
        const std::string payload{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
        read = stream.is_open() && payload == "FOnline native long-path probe\n";
    }
    const std::string renamed = input + ".renamed";
    const bool moved = fs_rename(input, renamed);
    const auto resolved_renamed = fs_make_io_path(renamed, ec);
    std::filesystem::directory_iterator entry{resolved_renamed.parent_path(), ec};
    bool found = false;
    while (!ec && entry != std::filesystem::directory_iterator{}) {
        if (entry->path().filename() == resolved_renamed.filename()) found = true;
        entry.increment(ec);
    }
    const bool removed = fs_remove_file(renamed);
    std::cout << "{\"written\":" << (written ? "true" : "false")
              << ",\"read_matches\":" << (read ? "true" : "false")
              << ",\"renamed\":" << (moved ? "true" : "false")
              << ",\"enumerated_file\":" << (found ? "true" : "false")
              << ",\"removed\":" << (removed ? "true" : "false") << "}\n";
    return written && read && moved && found && removed && !ec ? 0 : 4;
}
"""


def engine_probe_source() -> tuple[str, dict[str, object]]:
    source = ENGINE_SOURCE.read_text(encoding="utf-8")
    declaration = next(line for line in source.splitlines() if line.startswith("static auto fs_make_io_path(") and line.endswith(";"))
    functions: list[str] = []
    for signature in ENGINE_SIGNATURES:
        start = source.rindex(signature)
        end = source.index("\n}\n", start) + len("\n}\n")
        function = source[start:end]
        if "\n{" not in function or ";" in function.split("\n", 1)[0]:
            raise ValueError(f"Expected an out-of-line canonical definition: {signature}")
        functions.append(function)
    native = ENGINE_HARNESS + declaration + "\n" + "\n".join(functions) + ENGINE_MAIN
    return native, {
        "source_file": "Source/Essentials/DiskFileSystem.cpp",
        "source_sha256": hashlib.sha256(ENGINE_SOURCE.read_bytes()).hexdigest(),
        "extracted_functions": {signature: hashlib.sha256(function.encode()).hexdigest() for signature, function in zip(ENGINE_SIGNATURES, functions)},
        "extracted_declaration_sha256": hashlib.sha256(declaration.encode()).hexdigest(),
        "harness_aliases": {"string_view": "std::string_view", "FO_WINDOWS": "1", "stack_trace_macros": "no-op", "ignore_unused": "void expression (marks the argument used)"},
        "proof_scope": "Exact path conversion and create/write/open/rename/remove function bodies, including recursive removal and its canonical helper declaration. STL directory enumeration uses that conversion; the negative control uses remove_all with the ordinary two-argument conversion. Full engine linkage, allocator and visitor dispatch remain native unit-test responsibilities.",
        "native_source_sha256": hashlib.sha256(native.encode()).hexdigest(),
    }


def probe_engine_paths(binaries: dict[str, Path], base: Path) -> dict[str, object]:
    path = make_case_path(base / "Юникод", 320).with_name("данные.bin")
    cases: list[dict[str, object]] = []
    semantics: list[dict[str, object]] = []
    recursive_removal: list[dict[str, object]] = []
    forms = [("absolute", str(path)), ("relative", os.path.relpath(path, base)), ("extended", extended_path(str(path)))]
    long_unc = "\\\\server\\share\\" + "nested\\" * 45 + "file.bin"
    expected_forms = [
        ("ordinary_trailing_dot_space", str(path) + ". ", extended_path(str(path))),
        ("extended_trailing_dot_space", extended_path(str(path)) + ". ", extended_path(str(path)) + ". "),
        ("device_passthrough", "\\\\.\\" + str(path), "\\\\.\\" + str(path)),
        ("unc_lexical", long_unc, extended_path(long_unc)),
        ("extended_unc_passthrough", extended_path(long_unc), extended_path(long_unc)),
    ]
    for runtime, executable in binaries.items():
        for form, argument in forms:
            result = subprocess.run([str(executable), "exercise", argument], cwd=base, text=True, encoding="utf-8", capture_output=True)
            case = {"runtime": runtime, "form": form, "path": argument, "absolute_utf16_length": utf16_length(str(path)), "exit_code": result.returncode}
            case.update(json.loads(result.stdout) if result.stdout.startswith("{") else {"diagnostic": result.stdout + result.stderr})
            cases.append(case)
        result = subprocess.run([str(executable), "names", str(path)], cwd=base, text=True, encoding="utf-8", capture_output=True)
        semantics.append({"runtime": runtime, "case": "ordinary_and_literal_names_are_distinct_files", "exit_code": result.returncode, "matches": result.returncode == 0 and result.stdout == "true"})
        directory = base / f"directory-names-{runtime}"
        result = subprocess.run([str(executable), "directory_names", str(directory)], cwd=base, text=True, encoding="utf-8", capture_output=True)
        semantics.append({"runtime": runtime, "case": "ordinary_and_literal_directory_removal_are_distinct", "exit_code": result.returncode, "matches": result.returncode == 0 and result.stdout == "true"})
        for form in ("absolute", "relative"):
            root = base / f"recursive-{runtime}-{form}"
            if utf16_length(str(root)) >= 248:
                raise ValueError(f"Recursive-removal control requires a short root: {root}")
            child = make_case_path(root / "Юникод", 360)
            argument = str(root) if form == "absolute" else os.path.relpath(root, base)
            result = subprocess.run([str(executable), "remove_tree", argument, str(child)], cwd=base, text=True, encoding="utf-8", capture_output=True)
            case = {"runtime": runtime, "form": form, "root": argument, "child": str(child), "absolute_root_utf16_length": utf16_length(str(root)), "exit_code": result.returncode}
            case.update(json.loads(result.stdout) if result.stdout.startswith("{") else {"diagnostic": result.stdout + result.stderr})
            recursive_removal.append(case)
        for name, argument, expected in expected_forms:
            result = subprocess.run([str(executable), "resolve", argument], cwd=base, text=True, encoding="utf-8", capture_output=True)
            semantics.append({"runtime": runtime, "case": name, "input": argument, "expected": expected, "actual": result.stdout, "exit_code": result.returncode, "matches": result.returncode == 0 and result.stdout == expected})
        recursive_forms = [("short_unc_recursive", r"\\server\share\short", r"\\?\UNC\server\share\short")]
        recursive_forms.extend(expected_forms)
        for name, argument, expected in recursive_forms:
            result = subprocess.run([str(executable), "resolve_recursive", argument], cwd=base, text=True, encoding="utf-8", capture_output=True)
            semantics.append({"runtime": runtime, "case": name + "_recursive", "input": argument, "expected": expected, "actual": result.stdout, "exit_code": result.returncode, "matches": result.returncode == 0 and result.stdout == expected})
    return {"cases": cases, "semantics": semantics, "recursive_removal": recursive_removal, "recursive_negative_reproduced": all(case.get("negative_reproduced", False) for case in recursive_removal), "unc_io_tested": False, "passed": all(case["exit_code"] == 0 for case in cases + recursive_removal) and all(case["matches"] for case in semantics)}


def extended_path(path: str) -> str:
    normalized = path.replace("/", "\\")
    if normalized.startswith("\\\\?\\"):
        return normalized
    if normalized.startswith("\\\\"):
        return "\\\\?\\UNC\\" + normalized[2:]
    drive, tail = ntpath.splitdrive(normalized)
    if len(drive) != 2 or drive[1] != ":" or not tail.startswith("\\"):
        raise ValueError(f"An absolute Windows drive or UNC path is required: {path}")
    return "\\\\?\\" + normalized


def utf16_length(value: str) -> int:
    return len(value.encode("utf-16-le")) // 2


def make_case_path(base: Path, length: int) -> Path:
    path = base
    filename = "LastFrontier.dll.pdb"
    remaining = length - utf16_length(str(path)) - 1 - len(filename)
    if remaining < 2:
        raise ValueError(f"Probe root is too long for the {length}-character boundary: {base}")
    while remaining > 61:
        path /= "d" * 40
        remaining -= 41
    path /= "d" * (remaining - 1)
    return path / filename


def run_checked(command: list[str], cwd: Path, log_path: Path) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, cwd=cwd, text=True, capture_output=True)
    log_path.write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode != 0:
        raise RuntimeError(f"Command failed ({result.returncode}), see {log_path}: {command}")
    return result


def inspect_manifest(executable: Path, build: Path, output: Path) -> dict[str, object]:
    cache = (build / "CMakeCache.txt").read_text(encoding="utf-8")
    mt = next((line.split("=", 1)[1] for line in cache.splitlines() if line.startswith("CMAKE_MT:FILEPATH=")), "")
    if not mt or not Path(mt).is_file():
        return {"inspected": False, "reason": "CMAKE_MT executable unavailable"}
    result = subprocess.run([mt, f"-inputresource:{executable};#1", f"-out:{output}"], text=True, capture_output=True)
    if result.returncode != 0:
        return {"inspected": False, "exit_code": result.returncode, "diagnostic": result.stdout + result.stderr}
    document = ET.parse(output)
    aware = any(node.tag.rsplit("}", 1)[-1] == "longPathAware" and (node.text or "").strip().lower() == "true" for node in document.iter())
    return {"inspected": True, "long_path_aware": aware, "sha256": hashlib.sha256(output.read_bytes()).hexdigest()}


def probe(output: Path, unc_root: Path | None) -> dict[str, object]:
    if os.name != "nt":
        raise RuntimeError("This diagnostic must execute on Windows")
    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    artifacts = output.parent / (output.stem + "-artifacts")
    artifacts.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="fo-file-io-") as temporary:
        work = Path(temporary).resolve()
        source = work / "source"
        source.mkdir()
        (source / "probe.cpp").write_text(NATIVE_SOURCE, encoding="utf-8")
        engine_native, engine_manifest = engine_probe_source()
        (source / "engine_probe.cpp").write_text(engine_native, encoding="utf-8")
        (artifacts / "engine_probe.cpp").write_text(engine_native, encoding="utf-8")
        (source / "CMakeLists.txt").write_text(CMAKE_SOURCE, encoding="utf-8")
        build = work / "build"
        run_checked(["cmake", "-S", str(source), "-B", str(build)], work, artifacts / "configure.log")
        run_checked(["cmake", "--build", str(build), "--config", "Release", "--parallel", "2"], work, artifacts / "build.log")
        binaries: dict[str, Path] = {}
        engine_binaries: dict[str, Path] = {}
        manifests: dict[str, object] = {}
        for runtime in ("static", "dynamic"):
            candidates = list(build.rglob(f"file_io_{runtime}.exe"))
            if len(candidates) != 1:
                raise RuntimeError(f"Expected exactly one {runtime} probe executable: {candidates}")
            engine_candidates = list(build.rglob(f"engine_io_{runtime}.exe"))
            if len(engine_candidates) != 1:
                raise RuntimeError(f"Expected one canonical engine probe executable: {engine_candidates}")
            engine_binaries[runtime] = engine_candidates[0]
            shutil.copy2(engine_candidates[0], artifacts / engine_candidates[0].name)
            binaries[runtime] = candidates[0]
            shutil.copy2(candidates[0], artifacts / candidates[0].name)
            manifests[runtime] = inspect_manifest(candidates[0], build, artifacts / f"{runtime}.manifest")
        roots = [("drive", work / "files")]
        if unc_root is not None:
            unc = str(unc_root)
            if not unc.startswith("\\\\") or unc.startswith("\\\\?\\"):
                raise ValueError("--unc-root must name an ordinary writable UNC directory")
            roots.append(("unc", Path(tempfile.mkdtemp(prefix="fo-file-io-", dir=unc))))
        cases: list[dict[str, object]] = []
        try:
            for kind, base in roots:
                Path(extended_path(str(base))).mkdir(parents=True, exist_ok=True)
                control = base / "control.pdb"
                Path(extended_path(str(control))).write_bytes(PAYLOAD.encode())
                paths = [control] + [make_case_path(base, length) for length in (259, 260, 262, 320)]
                for path in paths:
                    prefixed = extended_path(str(path))
                    Path(prefixed).parent.mkdir(parents=True, exist_ok=True)
                    Path(prefixed).write_bytes(PAYLOAD.encode())
                    inputs = [("absolute", str(path)), ("relative", os.path.relpath(path, base)), ("extended_absolute", prefixed)]
                    for runtime, executable in binaries.items():
                        for form, argument in inputs:
                            result = subprocess.run([str(executable), argument], cwd=base, text=True, capture_output=True)
                            if result.returncode != 0:
                                raise RuntimeError(f"Native probe failed: {result.returncode}: {result.stdout}{result.stderr}")
                            cases.append({"root_kind": kind, "runtime": runtime, "form": form, "path": argument,
                                "path_utf16_length": utf16_length(argument), "absolute_utf16_length": utf16_length(str(path)),
                                **json.loads(result.stdout)})
            engine_results = probe_engine_paths(engine_binaries, roots[0][1])
        finally:
            for kind, base in roots:
                shutil.rmtree(extended_path(str(base)))
        report: dict[str, object] = {"platform": sys.platform, "python": sys.version, "cwd_utf16_length": utf16_length(str(roots[0][1])),
            "script_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            "source_sha256": hashlib.sha256(NATIVE_SOURCE.encode()).hexdigest(), "cmake_sha256": hashlib.sha256(CMAKE_SOURCE.encode()).hexdigest(),
            "manifests": manifests, "unc_tested": unc_root is not None, "cases": cases,
            "engine_manifest": engine_manifest, "engine_results": engine_results}
        output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(report, indent=2))
        if not all(case["content_matches"] and case["enumerated_file"] for case in cases if case["form"] == "extended_absolute"):
            raise RuntimeError("Extended-path control failed; inspect the factual JSON before drawing a conclusion")
        if not all(case["content_matches"] for case in cases if str(case["path"]).endswith("control.pdb")):
            raise RuntimeError("Short-path control failed; inspect the factual JSON")
        if not engine_results["passed"]:
            raise RuntimeError("Canonical engine path regression failed; inspect the factual JSON")
        return report


def main() -> None:
    parser = argparse.ArgumentParser(description="Compare native MSVC ifstream and directory enumeration at Windows path-length boundaries")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--unc-root", type=Path, help="Optional existing writable UNC directory; no share is contacted by default")
    args = parser.parse_args()
    probe(args.output, args.unc_root)


if __name__ == "__main__":
    main()
