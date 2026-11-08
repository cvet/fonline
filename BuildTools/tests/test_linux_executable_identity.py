from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import sys
import textwrap

import pytest


SOURCE = Path(__file__).resolve().parents[2] / "Source/Essentials/Posix.cpp"
BASIC_SOURCE = SOURCE.with_name("BasicCore.cpp")


@pytest.mark.skipif(sys.platform != "linux", reason="Linux executable identity integration")
@pytest.mark.parametrize("rename", [False, True])
def test_posix_executable_identity_through_private_loader_and_runtime(tmp_path: Path, rename: bool) -> None:
    compiler = shutil.which("c++")
    if compiler is None:
        pytest.skip("C++ compiler is required for the native identity fixture")
    loader = next((p for p in [Path("/lib64/ld-linux-x86-64.so.2"), Path("/lib/ld-linux-aarch64.so.1")] if p.is_file()), None)
    if loader is None:
        pytest.skip("A glibc loader is required for the direct-loader fixture")
    source = SOURCE.read_text(encoding="utf-8")
    start = source.index("auto posix::get_executable_path() noexcept -> optional<string>")
    end = source.index("auto posix::fork_into_background()", start)
    # Compile the actual owner function. The oracle is the executable created below,
    # independent of readlink/auxv/loader implementation.
    method = source[start:end]
    basic = BASIC_SOURCE.read_text(encoding="utf-8")
    helper_start = basic.index("auto details::linux_executable_path() noexcept")
    helper_end = basic.index("\n}\n", helper_start) + 3
    helper = basic[helper_start:helper_end]
    header = BASIC_SOURCE.with_suffix(".h").read_text(encoding="utf-8")
    declaration_start = header.index("    [[nodiscard]] auto linux_executable_path()")
    declaration_end = header.index(";", declaration_start) + 1
    declaration = header[declaration_start:declaration_end]
    stack_trace = SOURCE.with_name("StackTrace.cpp").read_text(encoding="utf-8")
    symbolization_start = stack_trace.index("static auto linux_symbolization_path() noexcept")
    symbolization_end = stack_trace.index("\n}\n", symbolization_start) + 3
    symbolization = stack_trace[symbolization_start:symbolization_end]
    module_source = textwrap.dedent(r'''
        #include <cstdio>
        #include <cstdlib>
        #include <optional>
        #include <string>
        #include <sys/auxv.h>
        #include <unistd.h>
        #define FO_LINUX 1
        #define FO_MAC 0
        using string = std::string;
        template<typename T> using optional = std::optional<T>;
        template<typename T> struct Borrow {
            T* value;
            T* get() { return value; }
            T& operator[](size_t index) { return value[index]; }
            explicit operator bool() const { return value != nullptr; }
        };
        template<typename T> Borrow<T> make_ptr(T* value) { return {value}; }
        template<typename T> Borrow<T> make_nptr(T* value) { return {value}; }
        namespace posix { auto get_executable_path() noexcept -> optional<string>; }
    ''') + "namespace details {\n" + declaration + "\n}\n" + helper + method + symbolization + textwrap.dedent(r'''
        extern "C" const char* identity() {
            static string path = posix::get_executable_path().value_or("");
            return path.c_str();
        }
        extern "C" const char* symbolization_identity() {
            static std::string path = linux_symbolization_path().value_or("");
            return path.c_str();
        }
    ''')
    module_file = tmp_path / "identity.cpp"
    module_file.write_text(module_source, encoding="utf-8")
    runtime = tmp_path / "identity.so"
    main = tmp_path / "real application"
    driver = tmp_path / "main.cpp"
    driver.write_text(textwrap.dedent(r'''
        #include <cstdio>
        #include <dlfcn.h>
        #include <string>
        int main(int argc, char** argv) {
            if (argc > 2) {
                std::string moved = std::string(argv[0]) + ".moved";
                if (std::rename(argv[0], moved.c_str())) return 6;
                FILE* replacement = std::fopen(argv[0], "wb");
                if (!replacement) return 7;
                std::fputs("replacement", replacement);
                std::fclose(replacement);
            }
            void* module = dlopen(argv[1], RTLD_NOW);
            if (!module) { std::fprintf(stderr, "%s", dlerror()); return 2; }
            auto identity = reinterpret_cast<const char* (*)()>(dlsym(module, "identity"));
            if (!identity) return 3;
            std::puts(identity());
            auto symbolization = reinterpret_cast<const char* (*)()>(dlsym(module, "symbolization_identity"));
            if (!symbolization) return 4;
            std::puts(symbolization());
            return 0;
        }
    '''), encoding="utf-8")
    for command in [
        [compiler, "-std=c++20", "-shared", "-fPIC", str(module_file), "-o", str(runtime)],
        [compiler, "-std=c++20", str(driver), "-o", str(main), "-ldl"],
    ]:
        result = subprocess.run(command, capture_output=True, text=True, timeout=60)
        assert result.returncode == 0, result.stdout + result.stderr
    unrelated = tmp_path / "unrelated cwd"
    unrelated.mkdir()
    for index, prefix in enumerate([[], [str(loader)]]):
        active = tmp_path / ("active application " + str(index))
        shutil.copy2(main, active)
        extra = ["rename"] if rename else []
        result = subprocess.run([*prefix, str(active), str(runtime), *extra], cwd=unrelated, capture_output=True, text=True, timeout=15)
        assert result.returncode == 0, result.stderr
        expected = Path(str(active) + ".moved") if rename else active
        lines = result.stdout.splitlines()
        assert Path(lines[0]) == expected.resolve()
        assert lines[1] == (str(expected.resolve()) if prefix else "")
