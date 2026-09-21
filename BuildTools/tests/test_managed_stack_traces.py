"""Exercise exception descriptions and the canonical native-entry exception storage."""

import shutil
import subprocess

import pytest

import test_managed_async_callbacks as callbacks


MANAGED_PROBE = r'''
using System;
using System.Reflection;
using System.Runtime.CompilerServices;
using FOnline;

internal static class Program
{
    private static int Main(string[] args)
    {
        var native = new NativeCallException(new string('x', 5));
        Exception exception = args[0] switch {
            "native" => native,
            "reflection" => new TargetInvocationException(new AggregateException(native)),
            "wrapped" => new InvalidOperationException("operation context", native),
            "aggregate" => new AggregateException(Capture(First), Capture(Second)),
            _ => throw new ArgumentException("Unknown probe")
        };
        object?[] description = Native.DescribeException(exception);
        Console.WriteLine(description[0]);
        if (args[0] == "native" || args[0] == "reflection") {
            return ReferenceEquals(description[1], native.Message) ? 0 : 1;
        }
        if (description[1] != null) return 2;
        if (args[0] == "wrapped") {
            return ((string)description[0]!).Contains("operation context", StringComparison.Ordinal) ? 0 : 3;
        }
        long[] frames = (long[])description[2]!;
        long first = typeof(Program).GetMethod(nameof(First), BindingFlags.Static | BindingFlags.NonPublic)!.MethodHandle.Value.ToInt64();
        long second = typeof(Program).GetMethod(nameof(Second), BindingFlags.Static | BindingFlags.NonPublic)!.MethodHandle.Value.ToInt64();
        bool foundFirst = false, foundSecond = false;
        for (int i = 0; i < frames.Length; i += 2) {
            foundFirst |= frames[i] == first;
            foundSecond |= frames[i] == second;
        }
        return foundFirst && foundSecond ? 0 : 4;
    }

    private static Exception Capture(Action action)
    {
        try { action(); }
        catch (Exception ex) { return ex; }
        throw new InvalidOperationException("Probe must throw");
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static void First() => throw new InvalidOperationException("first failure");
    [MethodImpl(MethodImplOptions.NoInlining)]
    private static void Second() => throw new ArgumentException("second failure");
}
''' + callbacks.PROBE_SOURCE[callbacks.PROBE_SOURCE.index("\nnamespace FOnline\n{"):]


@pytest.fixture(scope="module")
def description_probe(tmp_path_factory):
    dotnet = shutil.which("dotnet")
    if dotnet is None:
        pytest.skip("dotnet SDK is required")
    return callbacks.build_probe(dotnet, tmp_path_factory.mktemp("managed-exception-description"), MANAGED_PROBE)


@pytest.mark.parametrize("kind", ["native", "reflection", "wrapped", "aggregate"])
def test_exception_description_preserves_causes(description_probe, kind):
    dotnet, output = description_probe
    result = subprocess.run([dotnet, str(output / "bin/Debug/net10.0/Probe.dll"), kind],
                            capture_output=True, text=True, timeout=20)
    assert result.returncode == 0, result.stdout + result.stderr


NATIVE_FIXTURE = r'''
#include <array>
#include <cassert>
#include <cstdint>
#include <exception>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#define FO_NO_INLINE
#define FO_NO_STACK_TRACE_ENTRY()
#define FO_STACK_TRACE_ENTRY()
#define FO_STRONG_ASSERT(condition, message) assert(condition)
template<class T> using nptr = T*;
template<class T> using vector = std::vector<T>;
template<class A, class B> using pair = std::pair<A, B>;
struct MonoObject {};
struct MonoString : MonoObject { std::string Text; };
struct MonoMethod {};
struct MonoDomain {};
namespace stack_trace {
using native_frame_address = uintptr_t;
constexpr size_t MAX_NATIVE_FRAMES = 128;
struct script_layer {
    std::array<native_frame_address, MAX_NATIVE_FRAMES> birth_native_frames {};
    uint32_t birth_native_frame_count {};
    bool birth_native_truncated {};
};
void capture_native_frames(std::array<native_frame_address, MAX_NATIVE_FRAMES>&, uint32_t&, bool&, uint32_t) {}
}
std::map<uint32_t, MonoObject*> Roots;
uint32_t NextRoot = 0;
uint32_t mono_gchandle_new(MonoObject* object, int) { Roots[++NextRoot] = object; return NextRoot; }
uint32_t NewManagedGcHandle(MonoObject* object, int pinned) { return mono_gchandle_new(object, pinned); }
MonoObject* mono_gchandle_get_target(uint32_t handle) { return Roots.at(handle); }
void mono_gchandle_free(uint32_t handle) { assert(Roots.erase(handle) == 1); }
void AppendRuntimeNativeFrames(MonoDomain*, std::span<const stack_trace::native_frame_address>, stack_trace::script_layer&) {}
'''

NATIVE_MAIN = r'''
int main()
{
    MonoString first, second, forged, moved;
    first.Text = second.Text = forged.Text = moved.Text = "same error";
    auto firstError = std::make_exception_ptr(std::runtime_error("first throw site"));
    auto secondError = std::make_exception_ptr(std::runtime_error("second throw site"));
    assert(CurrentScriptEntry == nullptr);
    {
        ManagedScriptEntryScope outer {nullptr};
        outer.SetCrossedNativeException(firstError, &first);
        outer.SetCrossedNativeException(secondError, &second);
        assert(outer.FindCrossedNativeException(&first) == firstError);
        assert(outer.FindCrossedNativeException(&second) == secondError);
        assert(!outer.FindCrossedNativeException(&forged));
        assert(!outer.FindCrossedNativeException(nullptr));
        {
            ManagedScriptEntryScope inner {nullptr};
            assert(inner.FindCrossedNativeException(&first) == firstError);
            inner.SetCrossedNativeException(secondError, &forged);
            assert(Roots.size() == 3);
        }
        assert(Roots.size() == 2);
        // Model a moving collection: the handle target changes while the native record stays in place
        for (auto& [handle, target] : Roots) if (target == &first) target = &moved;
        assert(outer.FindCrossedNativeException(&moved) == firstError);
        assert(!outer.FindCrossedNativeException(&first));
        assert(outer.FindCrossedNativeException(&second) == secondError);
        outer.Leave();
        assert(ManagedScriptEntryScope::GetInnermostRunning() == nullptr);
        assert(outer.FindCrossedNativeException(&moved) == firstError);
    }
    assert(Roots.empty());
    assert(CurrentScriptEntry == nullptr);
}
'''


def test_native_errors_keep_identity_and_release_roots(tmp_path):
    cmake = shutil.which("cmake")
    if cmake is None:
        pytest.skip("CMake and a native compiler are required")
    backend = (callbacks.ENGINE / "Source/Scripting/Managed/ManagedScriptBackend.cpp").read_text(encoding="utf-8")
    declarations = backend[backend.index("class ManagedScriptEntryScope final"):backend.index("// A managed exception reduced")]
    methods = backend[backend.index("ManagedScriptEntryScope::ManagedScriptEntryScope("):backend.index("// === Native ABI: logging")]
    (tmp_path / "probe.cpp").write_text(NATIVE_FIXTURE + declarations + methods + NATIVE_MAIN, encoding="utf-8")
    (tmp_path / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.20)\nproject(ManagedExceptionStorage LANGUAGES CXX)\n"
        "add_executable(probe probe.cpp)\ntarget_compile_features(probe PRIVATE cxx_std_20)\n", encoding="utf-8")
    build = tmp_path / "build"
    for command in ([cmake, "-S", str(tmp_path), "-B", str(build), "-DCMAKE_BUILD_TYPE=Debug"],
                    [cmake, "--build", str(build), "--config", "Debug"]):
        result = subprocess.run(command, capture_output=True, text=True, timeout=120)
        assert result.returncode == 0, result.stdout + result.stderr
    executable = build / "Debug/probe.exe" if (build / "Debug/probe.exe").exists() else build / "probe"
    result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=20)
    assert result.returncode == 0, result.stdout + result.stderr
