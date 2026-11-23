from __future__ import annotations

from pathlib import Path
import shutil
import subprocess
import sys

import pytest


ENGINE = Path(__file__).resolve().parents[2]


@pytest.fixture(scope="module", params=["quick", "normal", "immediate"])
def termination_probe(request, tmp_path_factory):
    has_quick_exit = sys.platform != "darwin"
    if request.param == "quick" and not has_quick_exit:
        pytest.skip("Apple targets select normal exit; their CRT has no quick_exit entry point")
    compiler = shutil.which("clang++-20") or shutil.which("clang++")
    if compiler is None:
        pytest.skip("Clang C++ compiler is required for the native termination subprocess probe")

    source = (ENGINE / "Source/Essentials/BasicCore.cpp").read_text(encoding="utf-8")
    header = (ENGINE / "Source/Essentials/BasicCore.h").read_text(encoding="utf-8")
    function = "exit_app_immediately" if request.param == "immediate" else "exit_app"
    declaration = next(line for line in header.splitlines() if line.startswith(f"[[noreturn]] void {function}("))
    start = source.index(f"void {function}(bool success) noexcept\n{{")
    end = source.index("\n}\n", start) + len("\n}\n")
    # Compile the exact canonical declaration/body. Platform macros only select its existing CRT branch
    probe = "\n".join([
        "#include <chrono>", "#include <cstdint>", "#include <cstdio>", "#include <cstdlib>",
        "#include <cstring>", "#include <thread>",
        f"#define HAS_QUICK_EXIT {int(has_quick_exit)}",
        f"#define FO_WEB {int(request.param == 'normal')}", "#define FO_MAC 0", "#define FO_IOS 0", "#define FO_ANDROID 0",
        declaration, source[start:end],
    ]) + r"""
void QuickHandler() { std::puts("QUICK_HANDLER"); std::fflush(stdout); }
void NormalHandler() { std::puts("NORMAL_HANDLER"); std::fflush(stdout); }
struct GlobalGuard
{
    ~GlobalGuard() { std::puts("GLOBAL_DESTRUCTOR"); std::fflush(stdout); }
} Global;
struct LocalGuard
{
    ~LocalGuard() { std::puts("LOCAL_DESTRUCTOR"); std::fflush(stdout); }
};
int main(int argc, char** argv)
{
    if (argc != 2 || std::atexit(NormalHandler) != 0) {
        return 9;
    }
#if HAS_QUICK_EXIT
    if (std::at_quick_exit(QuickHandler) != 0) {
        return 9;
    }
#endif
    LocalGuard guard;

    if (std::strcmp(argv[1], "return") == 0) {
        return 0;
    }

    EXIT_FUNCTION(std::strcmp(argv[1], "success") == 0);
}
""".replace("EXIT_FUNCTION", function)
    directory = tmp_path_factory.mktemp("process-termination")
    path = directory / "probe.cpp"
    path.write_text(probe, encoding="utf-8")
    executable = directory / "probe"
    compiled = subprocess.run(
        [compiler, "-std=c++20", "-Wall", "-Wextra", "-Wunreachable-code", "-Werror",
         str(path), "-o", str(executable)], capture_output=True, text=True, timeout=60, check=False,
    )
    assert compiled.returncode == 0, compiled.stdout + compiled.stderr
    return executable, request.param


@pytest.mark.parametrize(("argument", "exit_code"), [("success", 0), ("failure", 1)])
def test_exit_app_preserves_status_and_crt_cleanup(termination_probe, argument, exit_code):
    executable, mode = termination_probe
    result = subprocess.run([str(executable), argument], capture_output=True, text=True, timeout=10, check=False)
    assert result.returncode == exit_code, result.stdout + result.stderr
    expected = {
        "quick": ["QUICK_HANDLER"],
        "normal": ["NORMAL_HANDLER", "GLOBAL_DESTRUCTOR"],
        "immediate": [],
    }
    assert result.stdout.splitlines() == expected[mode]
    assert result.stderr == ""


def test_exit_app_probe_guards_and_handlers_are_live(termination_probe):
    executable, _ = termination_probe
    result = subprocess.run([str(executable), "return"], capture_output=True, text=True, timeout=10, check=False)
    assert result.returncode == 0, result.stdout + result.stderr
    assert result.stdout.splitlines() == ["LOCAL_DESTRUCTOR", "NORMAL_HANDLER", "GLOBAL_DESTRUCTOR"]
    assert result.stderr == ""
