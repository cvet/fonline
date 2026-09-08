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
set_property(TARGET file_io_static PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
set_property(TARGET file_io_dynamic PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
'''


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
        (source / "CMakeLists.txt").write_text(CMAKE_SOURCE, encoding="utf-8")
        build = work / "build"
        run_checked(["cmake", "-S", str(source), "-B", str(build)], work, artifacts / "configure.log")
        run_checked(["cmake", "--build", str(build), "--config", "Release", "--parallel", "2"], work, artifacts / "build.log")
        binaries: dict[str, Path] = {}
        manifests: dict[str, object] = {}
        for runtime in ("static", "dynamic"):
            candidates = list(build.rglob(f"file_io_{runtime}.exe"))
            if len(candidates) != 1:
                raise RuntimeError(f"Expected exactly one {runtime} probe executable: {candidates}")
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
        finally:
            for kind, base in roots:
                shutil.rmtree(extended_path(str(base)))
        report: dict[str, object] = {"platform": sys.platform, "python": sys.version, "cwd_utf16_length": utf16_length(str(roots[0][1])),
            "script_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            "source_sha256": hashlib.sha256(NATIVE_SOURCE.encode()).hexdigest(), "cmake_sha256": hashlib.sha256(CMAKE_SOURCE.encode()).hexdigest(),
            "manifests": manifests, "unc_tested": unc_root is not None, "cases": cases}
        output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(report, indent=2))
        if not all(case["content_matches"] and case["enumerated_file"] for case in cases if case["form"] == "extended_absolute"):
            raise RuntimeError("Extended-path control failed; inspect the factual JSON before drawing a conclusion")
        if not all(case["content_matches"] for case in cases if str(case["path"]).endswith("control.pdb")):
            raise RuntimeError("Short-path control failed; inspect the factual JSON")
        return report


def main() -> None:
    parser = argparse.ArgumentParser(description="Compare native MSVC ifstream and directory enumeration at Windows path-length boundaries")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--unc-root", type=Path, help="Optional existing writable UNC directory; no share is contacted by default")
    args = parser.parse_args()
    probe(args.output, args.unc_root)


if __name__ == "__main__":
    main()
