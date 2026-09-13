from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import uuid

import pytest


ENGINE_ROOT = Path(__file__).resolve().parents[2]


@pytest.fixture(scope="module")
def search_probe(tmp_path_factory: pytest.TempPathFactory) -> tuple[Path, dict[str, Path], str]:
    compiler = shutil.which("c++") or shutil.which("clang++")
    if compiler is None:
        pytest.skip("A C++20 compiler is required for the filesystem search probe")
    work = tmp_path_factory.mktemp("config-search")
    name = f"fonline-config-probe-{uuid.uuid4().hex}.fomain"
    source = (ENGINE_ROOT / "Source/Frontend/ApplicationInit.cpp").read_text(encoding="utf-8")
    start = source.index("            auto dir = std::filesystem::current_path();", source.index("auto LoadAppSettings("))
    opening = source.index("{", source.index("while (true)", start))
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    loop = source[start:end]
    preamble = r'''
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
using string = std::string;
class AppInitException : public std::runtime_error {
public:
    AppInitException(const char* text, const char* config) : std::runtime_error(string(text) + ": " + config) {}
};
class StringFormat {
public:
    explicit StringFormat(const string& text) : value(text) {}
    string normalize_path_slashes() {
        std::replace(value.begin(), value.end(), '\\', '/');
        return value;
    }
private:
    string value;
};
StringFormat strex(const char*, string value) { return StringFormat(value); }
string fs_path_to_string(const std::filesystem::path& path) { return path.string(); }
bool fs_exists(const string& path) { return std::filesystem::exists(path); }
bool fs_is_dir(const string& path) { return std::filesystem::is_directory(path); }
int main() {
    try {
        string config_to_apply, config_to_apply_dir;
'''
    ending = r'''
        if (config_to_apply != FO_MAIN_CONFIG) return 4;
        std::cout << config_to_apply_dir << '\n';
        return 0;
    } catch (const AppInitException& error) {
        std::cerr << error.what() << '\n';
        return 3;
    }
}
'''
    programs = {}
    for variant in ("before", "after"):
        body = loop.replace("dir.has_parent_path() && dir.parent_path() != dir", "dir.has_parent_path()") if variant == "before" else loop
        probe = work / f"{variant}.cpp"
        probe.write_text(f'#define FO_MAIN_CONFIG "{name}"\n' + preamble + body + ending, encoding="utf-8")
        executable = work / (variant + (".exe" if os.name == "nt" else ""))
        result = subprocess.run([compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror", str(probe), "-o", str(executable)], capture_output=True, text=True)
        assert result.returncode == 0 and not result.stderr, result.stdout + result.stderr
        programs[variant] = executable
    return work, programs, name


def test_missing_config_terminates_at_filesystem_root(search_probe) -> None:
    work, programs, name = search_probe
    filesystem_root = Path(work.anchor)
    assert not (filesystem_root / name).exists()
    with pytest.raises(subprocess.TimeoutExpired):
        subprocess.run([str(programs["before"])], cwd=filesystem_root, capture_output=True, timeout=1, check=False)
    after = subprocess.run([str(programs["after"])], cwd=filesystem_root, capture_output=True, text=True, timeout=2, check=False)
    assert after.returncode == 3
    assert after.stderr.strip() == f"Config file not found: {name}"


def test_nearest_parent_config_wins_over_a_farther_config(search_probe) -> None:
    work, programs, name = search_probe
    nearest = work / "parent/nearest"
    child = nearest / "child/grandchild"
    child.mkdir(parents=True)
    (work / name).write_text("farther", encoding="utf-8")
    (nearest / name).write_text("nearest", encoding="utf-8")
    (child / name).mkdir()
    for executable in programs.values():
        result = subprocess.run([str(executable)], cwd=child, capture_output=True, text=True, timeout=2, check=False)
        assert result.returncode == 0 and not result.stderr
        assert result.stdout.strip() == nearest.as_posix()
