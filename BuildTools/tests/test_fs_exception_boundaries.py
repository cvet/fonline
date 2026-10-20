from pathlib import Path
import shutil
import subprocess

import pytest


SOURCE = Path(__file__).resolve().parents[2] / "Source/Essentials/DiskFileSystem.cpp"


@pytest.fixture(scope="module")
def parent_path_probe(tmp_path_factory: pytest.TempPathFactory) -> Path:
    if not shutil.which("cmake"):
        pytest.skip("CMake and a C++ compiler are required")
    folder = tmp_path_factory.mktemp("fs-exception-boundary")
    source = SOURCE.read_text(encoding="utf-8")
    start = source.rindex("static auto fs_parent_path(")
    end = source.index("\n}\n", start) + len("\n}\n")
    # Keep the canonical body; substitute only its input type to inject failures.
    function = source[start:end].replace("const std::filesystem::path& path", "const FaultyPath& path", 1)
    (folder / "probe.cpp").write_text(
        r'''
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <new>
#include <stdexcept>
#include <system_error>

class FaultyPath {
public:
    explicit FaultyPath(int mode) : mode_(mode) {}
    auto parent_path() const -> std::filesystem::path {
        switch (mode_) {
        case 1: throw std::bad_alloc{};
        case 2: throw std::length_error{"injected length failure"};
        case 3: throw std::runtime_error{"injected unexpected exception"};
        case 4: throw 42;
        default: return "parent";
        }
    }
private:
    int mode_;
};
''' + function + r'''
int main(int argc, char** argv) {
    std::set_terminate([] { std::_Exit(70); });
    if (argc != 2) return 2;
    const int mode = std::atoi(argv[1]);
    std::error_code ec = std::make_error_code(std::errc::permission_denied);
    const auto result = fs_parent_path(FaultyPath{mode}, ec);
    if (mode == 0) return !ec && result == "parent" ? 0 : 3;
    const auto expected = mode == 1 ? std::errc::not_enough_memory
        : mode == 2 ? std::errc::filename_too_long : std::errc::io_error;
    return result.empty() && ec == expected ? 0 : 4;
}
''', encoding="utf-8",
    )
    (folder / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.22)\n"
        "project(FsExceptionBoundary CXX)\n"
        "add_executable(fs_exception_probe probe.cpp)\n"
        "target_compile_features(fs_exception_probe PRIVATE cxx_std_20)\n",
        encoding="utf-8",
    )
    build = folder / "build"
    for command in (
        ["cmake", "-S", str(folder), "-B", str(build)],
        ["cmake", "--build", str(build), "--config", "Release"],
    ):
        result = subprocess.run(command, capture_output=True, text=True, timeout=180)
        assert result.returncode == 0, result.stdout + result.stderr
    executables = [path for path in build.rglob("fs_exception_probe*") if path.is_file() and path.name in ("fs_exception_probe", "fs_exception_probe.exe")]
    assert len(executables) == 1, executables
    return executables[0]


@pytest.mark.parametrize("mode", [0, 1, 2, 3, 4], ids=["success", "allocation", "length", "unexpected-standard", "non-standard"])
def test_parent_path_preserves_failure_contract(parent_path_probe: Path, mode: int) -> None:
    result = subprocess.run([str(parent_path_probe), str(mode)], capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, f"mode={mode}, exit={result.returncode}: {result.stdout}{result.stderr}"
