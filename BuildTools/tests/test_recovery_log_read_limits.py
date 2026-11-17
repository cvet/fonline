from pathlib import Path
import shutil
import subprocess

import pytest


SOURCE = Path(__file__).resolve().parents[2] / "Source/Server/DataBase.cpp"


def test_recovery_log_read_checks_size_before_allocating(tmp_path: Path) -> None:
    if not shutil.which("cmake"):
        pytest.skip("CMake and a C++ compiler are required")

    source = SOURCE.read_text(encoding="utf-8")
    start = source.index("auto DataBaseImpl::RecoveryLogHandle::Read() noexcept")
    end = source.index("\nauto DataBaseImpl::RecoveryLogHandle::Truncate()", start)
    method = source[start:end]
    (tmp_path / "probe.cpp").write_text(
        r'''
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
using std::int64_t;
using std::uint64_t;
using std::size_t;
using std::optional;
class string {
public:
    static constexpr auto max_size() noexcept -> size_t { return 1024; }
    void resize(size_t n) { if (n > max_size()) throw std::length_error("oversized"); _data.resize(n); }
    auto size() const noexcept -> size_t { return _data.size(); }
    auto data() noexcept -> char* { return _data.data(); }
private:
    std::string _data {};
};
template<typename T> auto make_ptr(T* p) -> T* { return p; }
template<typename T, typename U> auto numeric_cast(U value) -> T {
    if (!std::in_range<T>(value)) throw std::range_error("numeric conversion");
    return static_cast<T>(value);
}
namespace osfile {
    int reads {};
    int64_t file_size {};
    auto seek_file_end(int) -> int64_t { return file_size; }
    auto seek_file_begin(int) -> bool { return true; }
    auto read_file_chunk(int, char*, size_t size) -> int64_t { ++reads; return static_cast<int64_t>(size); }
}
class DataBaseImpl {
public:
    class RecoveryLogHandle {
    public:
        auto Read() noexcept -> optional<string>;
    private:
        int _fd {1};
    };
};
''' + method + r'''
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    int mode {std::atoi(argv[1])};
    osfile::file_size = mode == 0 ? 0 : mode == 1 ? 16 : mode == 2 ? 1025 : mode == 3 ? std::numeric_limits<int64_t>::max() : int64_t {4294967296};
    DataBaseImpl::RecoveryLogHandle log {};
    auto result {log.Read()};
    if (mode <= 1) return !result.has_value() || result->size() != static_cast<size_t>(osfile::file_size) || osfile::reads != mode ? 1 : 0;
    return result.has_value() || osfile::reads != 0 ? 1 : 0;
}
''', encoding="utf-8")
    (tmp_path / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.22)\n"
        "project(RecoveryReadLimit CXX)\n"
        "add_executable(recovery_read_limit probe.cpp)\n"
        "target_compile_features(recovery_read_limit PRIVATE cxx_std_20)\n",
        encoding="utf-8",
    )
    build = tmp_path / "build"
    for command in (["cmake", "-S", str(tmp_path), "-B", str(build)],
                    ["cmake", "--build", str(build), "--config", "Release"]):
        result = subprocess.run(command, capture_output=True, text=True, timeout=180)
        assert result.returncode == 0, result.stdout + result.stderr
    candidates = [p for p in build.rglob("recovery_read_limit*") if p.is_file()
                  and p.name in ("recovery_read_limit", "recovery_read_limit.exe")]
    assert len(candidates) == 1
    for mode in range(5):
        result = subprocess.run([str(candidates[0]), str(mode)], capture_output=True, text=True, timeout=10)
        assert result.returncode == 0, f"mode={mode}, exit={result.returncode}: {result.stdout}{result.stderr}"
