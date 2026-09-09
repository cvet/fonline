"""Exercise canonical native callback dispatch with a real Linux Mono embedding runtime.

Mono's native stack scan can retain an unrooted object, so profiler handle events also
check explicit ownership at allocation boundaries; this is not a WASM crash reproducer.
"""

from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import sys

import pytest

import test_managed_async_callbacks as managed_callbacks


ENGINE = Path(__file__).resolve().parents[2]
BACKEND = ENGINE / "Source/Scripting/Managed/ManagedScriptBackend.cpp"

MANAGED_PROBE = r'''
using System;
using FOnline;

public enum FirstKind { One = 1 }
public enum SecondKind { Two = 2 }
public delegate int ChangeText(ref string text);

public static class Program
{
    public static int Main() => 0;
    public static Delegate CreateScalars() =>
        (Func<long, long, string, uint, uint, int, FirstKind, SecondKind, int, int, int, int>)
        ((a, b, text, c, d, e, f, g, h, i, j) =>
        {
            if (text != "scope") throw new InvalidOperationException("Callback text was corrupted");
            GC.Collect();
            return checked((int)(a + b + c + d + e + (int)f + (int)g + h + i + j));
        });
    public static Delegate CreateByRef() => (ChangeText)Change;
    private static int Change(ref string text)
    {
        if (text != "scope") throw new InvalidOperationException("By-ref text was corrupted");
        text = "changed";
        GC.Collect();
        return 73;
    }
}
''' + managed_callbacks.PROBE_SOURCE[managed_callbacks.PROBE_SOURCE.index("\nnamespace FOnline\n{"):]

NATIVE_PREFIX = r'''
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/appdomain.h>
#include <mono/metadata/class.h>
#include <mono/metadata/mono-config.h>
#include <mono/metadata/mono-gc.h>
#include <mono/metadata/object.h>
#include <mono/metadata/profiler.h>
#include <mono/metadata/threads.h>
#include <mono/utils/mono-dl-fallback.h>
#include <mono/utils/mono-publib.h>

#define FO_STACK_TRACE_ENTRY()
#define FO_NO_STACK_TRACE_ENTRY()
#define FO_VERIFY_AND_THROW(condition, ...) do { if (!(condition)) throw ScriptSystemException("Managed thread attachment failed"); } while (false)
using ScriptSystemException = std::runtime_error;
template<class F> void safe_call(F&& action) noexcept { try { action(); } catch (...) { } }
template<class T> using vector = std::vector<T>;
template<class T> class ptr
{
public:
    ptr(T* value) : value_(value) { }
    T* get() const { return value_; }
    T* operator->() const { return value_; }
private:
    T* value_;
};
template<class F> class scope_exit
{
public:
    explicit scope_exit(F action) : action_(std::move(action)) { }
    ~scope_exit() { action_(); }
private:
    F action_;
};
struct ComplexTypeDesc
{
    MonoClass* Class {};
    bool IsMutable {};
    bool IsString {};
    explicit operator bool() const { return Class != nullptr; }
};
struct FuncCallData
{
    vector<void*> ArgsData;
    ptr<void> Accessor {nullptr};
    int32_t Result {};
};
struct _MonoProfiler
{
    std::mutex Lock;
    std::map<uint32_t, std::pair<MonoGCHandleType, MonoClass*>> Handles;
};
static void HandleCreated(MonoProfiler* profiler, uint32_t handle, MonoGCHandleType type, MonoObject* value)
{
    std::lock_guard guard(profiler->Lock);
    profiler->Handles[handle] = {type, mono_object_get_class(value)};
}
static void HandleDeleted(MonoProfiler* profiler, uint32_t handle, MonoGCHandleType)
{
    std::lock_guard guard(profiler->Lock);
    profiler->Handles.erase(handle);
}
struct ManagedScriptBackend
{
    MonoDomain* Domain;
    MonoImage* Image;
    MonoProfiler* Profiler;
    int32_t BoxCalls {};
    int32_t Collections {};
    int32_t ThrowAt {-1};
    bool CheckRoots {true};
    MonoDomain* GetDomain() { return Domain; }
    auto SnapshotHandles()
    {
        std::lock_guard guard(Profiler->Lock);
        return Profiler->Handles;
    }
    bool HasRoot(MonoClass* klass)
    {
        std::lock_guard guard(Profiler->Lock);
        for (auto [handle, info] : Profiler->Handles) {
            (void)handle;
            if (info.first == MONO_GC_HANDLE_NORMAL && info.second == klass) return true;
        }
        return false;
    }
};
struct ActiveBackendScope { explicit ActiveBackendScope(ptr<ManagedScriptBackend>) { } };
static MonoDomain* GetDomainOrThrow(MonoDomain* domain) { return domain; }
static MonoClass* FindFOnlineClass(ptr<ManagedScriptBackend> backend, const char* name)
{
    return mono_class_from_name(backend->Image, "FOnline", name);
}
static void ThrowIfManagedException(MonoObject* exception, const char* message)
{
    if (exception != nullptr) throw ScriptSystemException(message);
}
static MonoObject* BoxNativeCallValue(ptr<ManagedScriptBackend> backend, const ComplexTypeDesc& type, void* data, void*)
{
    if (backend->CheckRoots && !backend->HasRoot(mono_array_class_get(mono_get_object_class(), 1))) {
        throw ScriptSystemException("Callback argument array has no explicit strong GC root during boxing");
    }
    if (backend->BoxCalls++ == backend->ThrowAt) throw ScriptSystemException("Requested boxing failure");
    mono_gc_collect(mono_gc_max_generation());
    backend->Collections++;
    return type.IsString ? reinterpret_cast<MonoObject*>(mono_string_new(backend->Domain, static_cast<std::string*>(data)->c_str())) :
                           mono_value_box(backend->Domain, type.Class, data);
}
static void CopyManagedCallbackByRefArg(ptr<ManagedScriptBackend> backend, const ComplexTypeDesc&, MonoObject* value, ptr<void> data)
{
    if (backend->CheckRoots && !backend->HasRoot(mono_get_int32_class())) {
        throw ScriptSystemException("Callback return value has no explicit strong GC root during by-ref copy-back");
    }
    uint32_t value_handle = mono_gchandle_new(value, false);
    mono_gc_collect(mono_gc_max_generation());
    backend->Collections++;
    char* text = mono_string_to_utf8(reinterpret_cast<MonoString*>(mono_gchandle_get_target(value_handle)));
    *static_cast<std::string*>(data.get()) = text;
    mono_free(text);
    mono_gchandle_free(value_handle);
}
static void CopyManagedCallbackReturnValue(ptr<ManagedScriptBackend>, const ComplexTypeDesc&, MonoObject* value, FuncCallData& call)
{
    call.Result = *static_cast<int32_t*>(mono_object_unbox(value));
}
'''

NATIVE_MAIN = r'''
static void* LoadShim(const char* name, int, char**, void*)
{
    if (name == nullptr || std::strstr(name, "System.") == nullptr) return nullptr;
    return reinterpret_cast<void*>(1);
}
static void* FindShim(void*, const char* name, char**, void*) { return dlsym(RTLD_DEFAULT, name); }
static void* CloseShim(void*, void*) { return nullptr; }
int main(int argc, char** argv)
{
    if (argc != 4) return 90;
    setenv("DOTNET_SYSTEM_GLOBALIZATION_INVARIANT", "1", 1);
    setenv("MONO_THREADS_SUSPEND", "preemptive", 1);
    std::string runtime = argv[1];
    mono_set_dirs((runtime + "/lib").c_str(), (runtime + "/etc").c_str());
    mono_set_assemblies_path((runtime + "/lib/netcoreapp").c_str());
    mono_config_parse(nullptr);
    (void)mono_dl_fallback_register(LoadShim, FindShim, CloseShim, nullptr);
    if (std::strcmp(argv[3], "runtime-init") == 0) {
        int32_t status = 0;
        std::thread worker([&] {
            MonoDomain* worker_domain = mono_jit_init_version("CallbackRootProbe", "v4.0.30319");
            if (worker_domain == nullptr || mono_thread_current() == nullptr) {
                status = 98;
                return;
            }

            uint32_t flag_handle = 0;
            {
                ManagedThreadAttachment managed_thread {worker_domain, ManagedThreadAttachmentMode::AdoptExisting};
                MonoArray* flag = mono_array_new(worker_domain, mono_get_boolean_class(), 1);
                flag_handle = mono_gchandle_new(reinterpret_cast<MonoObject*>(flag), false);
                mono_gc_collect(mono_gc_max_generation());
                flag = reinterpret_cast<MonoArray*>(mono_gchandle_get_target(flag_handle));
                mono_array_set(flag, uint8_t, 0, 1);
            }

            if (mono_thread_current() != nullptr) {
                status = 97;
                return;
            }

            ReleaseManagedGcHandle(worker_domain, flag_handle);
            if (flag_handle != 0 || mono_thread_current() != nullptr) {
                status = 94;
                return;
            }

            {
                ManagedThreadAttachment managed_thread {worker_domain};
                if (mono_thread_current() == nullptr) status = 96;
            }

            if (mono_thread_current() != nullptr) status = 95;
        });
        worker.join();
        std::printf("RUNTIME_INIT status=%d detached=%d\n", status, status == 0);
        return status;
    }

    MonoDomain* domain = mono_jit_init_version("CallbackRootProbe", "v4.0.30319");
    MonoAssembly* assembly = mono_domain_assembly_open(domain, argv[2]);
    if (assembly == nullptr) return 91;
    MonoImage* image = mono_assembly_get_image(assembly);
    MonoClass* program = mono_class_from_name(image, "", "Program");
    bool by_ref = std::strcmp(argv[3], "by-ref") == 0;
    bool external_thread = std::strcmp(argv[3], "external-thread") == 0 || std::strcmp(argv[3], "external-thread-throw") == 0;
    bool adopt_existing_throw = std::strcmp(argv[3], "adopt-existing-throw") == 0;
    bool adopt_existing = std::strcmp(argv[3], "adopt-existing") == 0 || adopt_existing_throw;
    MonoMethod* factory = mono_class_get_method_from_name(program, by_ref ? "CreateByRef" : "CreateScalars", 0);
    MonoObject* exception = nullptr;
    MonoObject* handler = mono_runtime_invoke(factory, nullptr, nullptr, &exception);
    if (exception != nullptr || handler == nullptr) return 92;
    MonoProfiler profiler;
    MonoProfilerHandle profiler_handle = mono_profiler_create(&profiler);
    mono_profiler_set_gc_handle_created_callback(profiler_handle, HandleCreated);
    mono_profiler_set_gc_handle_deleted_callback(profiler_handle, HandleDeleted);
    uint32_t handler_handle = mono_gchandle_new(handler, false);
    ManagedScriptBackend backend {domain, image, &profiler};
    auto initial_handles = backend.SnapshotHandles();
    if (std::strcmp(argv[3], "throw-boxing") == 0 || std::strcmp(argv[3], "external-thread-throw") == 0 ||
        std::strcmp(argv[3], "adopt-existing-throw") == 0) backend.ThrowAt = 4;
    if (std::strcmp(argv[3], "observe-unrooted") == 0) backend.CheckRoots = false;
    std::string text = "scope";
    int64_t a = 1, b = 2;
    uint32_t c = 3, d = 4;
    int32_t e = 5, f = 1, g = 2, h = 6, i = 7, j = 8;
    ComplexTypeDesc long_type {mono_get_int64_class()}, uint_type {mono_get_uint32_class()}, int_type {mono_get_int32_class()};
    ComplexTypeDesc text_type {mono_get_string_class(), by_ref, true};
    vector<ComplexTypeDesc> args = by_ref ? vector<ComplexTypeDesc>{text_type} : vector<ComplexTypeDesc>{
        long_type, long_type, text_type, uint_type, uint_type, int_type,
        {mono_class_from_name(image, "", "FirstKind")}, {mono_class_from_name(image, "", "SecondKind")}, int_type, int_type, int_type};
    FuncCallData call;
    call.ArgsData = by_ref ? vector<void*>{&text} : vector<void*>{&a, &b, &text, &c, &d, &e, &f, &g, &h, &i, &j};
    int32_t status = 0;
    auto dispatch = [&] {
        DispatchManagedCallbackInContext(&backend, handler_handle, int_type, args, call);
        if (backend.ThrowAt >= 0) status = 93;
        if (call.Result != (by_ref ? 73 : 39) || text != (by_ref ? "changed" : "scope")) status = 94;
    };
    auto handle_exception = [&](const std::exception& error) {
        std::printf("EXCEPTION %s\n", error.what());
        if (backend.ThrowAt < 0 || std::strcmp(error.what(), "Requested boxing failure") != 0) status = 95;
    };
    auto invoke = [&] {
        try {
            dispatch();
        }
        catch (const std::exception& error) {
            handle_exception(error);
        }
    };
    bool worker_detached = !external_thread && !adopt_existing;
    if (external_thread || adopt_existing) {
        std::thread worker([&] {
            if (adopt_existing) {
                MonoThread* implicit_attachment = mono_thread_attach(domain);
                if (implicit_attachment == nullptr) {
                    status = 98;
                    return;
                }

                try {
                    ManagedThreadAttachment managed_thread {domain, ManagedThreadAttachmentMode::AdoptExisting};
                    if (adopt_existing_throw) {
                        dispatch();
                    }
                    else {
                        invoke();
                    }
                }
                catch (const std::exception& error) {
                    handle_exception(error);
                }
            }
            else {
                invoke();
            }

            worker_detached = mono_thread_current() == nullptr;
        });
        worker.join();
        if (!worker_detached) status = 97;
    }
    else {
        invoke();
    }
    auto callback_handles = backend.SnapshotHandles();
    if (external_thread || adopt_existing) {
        // Mono may retain its own weak Thread handle until a later registry sweep; it is not a callback root.
        std::erase_if(callback_handles, [](const auto& entry) { return std::strcmp(mono_class_get_name(entry.second.second), "Thread") == 0; });
    }
    if (callback_handles != initial_handles) {
        std::printf("LEAK handles=%zu\n", callback_handles.size());
        for (auto [handle, info] : callback_handles) {
            std::printf("HANDLE id=%u type=%d class=%s\n", handle, static_cast<int>(info.first), mono_class_get_name(info.second));
        }
        status = 96;
    }
    mono_gchandle_free(handler_handle);
    mono_profiler_set_gc_handle_created_callback(profiler_handle, nullptr);
    mono_profiler_set_gc_handle_deleted_callback(profiler_handle, nullptr);
    std::printf("RESULT status=%d boxes=%d collections=%d return=%d text=%s detached=%d\n", status, backend.BoxCalls, backend.Collections, call.Result, text.c_str(), worker_detached);
    return status;
}
'''


def extract_function(source: str, declaration: str) -> str:
    start = source.index(declaration + "\n{")
    end = source.index("\n}\n", start) + len("\n}\n")
    return source[start:end]


def build_native_probe(output: Path, runtime: Path, compiler: str, backend_source: str) -> Path:
    declaration = "static void DispatchManagedCallbackInContext(ptr<ManagedScriptBackend> backend, uint32_t handler_handle, const ComplexTypeDesc& ret, const vector<ComplexTypeDesc>& args, FuncCallData& call)"
    attachment_start = backend_source.index("enum class ManagedThreadAttachmentMode\n{")
    attachment_class = backend_source.index("class ManagedThreadAttachment final\n{", attachment_start)
    attachment_end = backend_source.index("\n};", attachment_class) + len("\n};")
    release_handle = extract_function(backend_source, "static void ReleaseManagedGcHandle(MonoDomain* domain, uint32_t& handle) noexcept")
    root_start = backend_source.index("struct ManagedObjectRoot\n{")
    root_end = backend_source.index("\n};", root_start) + len("\n};")
    source = (NATIVE_PREFIX + backend_source[attachment_start:attachment_end] + "\n" + release_handle +
              backend_source[root_start:root_end] + "\n" + extract_function(backend_source, declaration) + NATIVE_MAIN)
    path = output / "callback.cpp"
    path.write_text(source, encoding="utf-8")
    executable = output / "callback"
    lib = runtime / "lib"
    result = subprocess.run([
        compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror", "-rdynamic", str(path),
        "-I" + str(runtime / "include/mono-2.0"), "-L" + str(lib), "-Wl,-rpath," + str(lib),
        "-lcoreclr", "-Wl,--whole-archive", str(lib / "libSystem.Native.a"), str(lib / "libSystem.Globalization.Native.a"),
        "-Wl,--no-whole-archive", "-lminipal", "-ldl", "-lpthread", "-o", str(executable),
    ], capture_output=True, text=True, timeout=120)
    (output / "native-build.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    assert result.returncode == 0, result.stdout + result.stderr
    return executable


@pytest.fixture(scope="module")
def mono_callback_probe(tmp_path_factory):
    configured = os.environ.get("FO_MANAGED_CALLBACK_RUNTIME")
    if sys.platform != "linux" or not configured:
        pytest.skip("Linux Mono embedding runtime must be selected with FO_MANAGED_CALLBACK_RUNTIME")
    runtime = Path(configured).resolve()
    assert (runtime / "lib/libcoreclr.so").is_file(), runtime
    compiler = shutil.which("clang++-20") or shutil.which("clang++")
    dotnet = shutil.which("dotnet")
    if compiler is None or dotnet is None:
        pytest.skip("Clang and dotnet SDK are required for the actual Mono callback probe")
    output = tmp_path_factory.mktemp("managed-callback-gc")
    managed_callbacks.build_probe(dotnet, output, MANAGED_PROBE)
    executable = build_native_probe(output, runtime, compiler, BACKEND.read_text(encoding="utf-8"))
    return executable, runtime, output / "bin/Debug/net10.0/Probe.dll"


@pytest.mark.parametrize(("mode", "expected"), [
    ("scalars", "boxes=11 collections=11 return=39 text=scope detached=1"),
    ("by-ref", "boxes=1 collections=2 return=73 text=changed detached=1"),
    ("throw-boxing", "boxes=5 collections=4 return=0 text=scope detached=1"),
    ("external-thread", "boxes=11 collections=11 return=39 text=scope detached=1"),
    ("external-thread-throw", "boxes=5 collections=4 return=0 text=scope detached=1"),
    ("adopt-existing", "boxes=11 collections=11 return=39 text=scope detached=1"),
    ("adopt-existing-throw", "boxes=5 collections=4 return=0 text=scope detached=1"),
])
def test_callback_roots_survive_collection_and_release_on_exit(mono_callback_probe, mode, expected):
    executable, runtime, assembly = mono_callback_probe
    result = subprocess.run([str(executable), str(runtime), str(assembly), mode],
                            capture_output=True, text=True, timeout=30,
                            preexec_fn=managed_callbacks.disable_core_dump)
    (executable.parent / (mode + ".log")).write_text(result.stdout + result.stderr, encoding="utf-8")
    assert result.returncode == 0, result.stdout + result.stderr
    assert "RESULT status=0 " + expected in result.stdout
    assert "LEAK" not in result.stdout


def test_runtime_initialization_attachment_is_adopted_and_reusable(mono_callback_probe):
    executable, runtime, assembly = mono_callback_probe
    result = subprocess.run([str(executable), str(runtime), str(assembly), "runtime-init"],
                            capture_output=True, text=True, timeout=30,
                            preexec_fn=managed_callbacks.disable_core_dump)
    (executable.parent / "runtime-init.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    assert result.returncode == 0, result.stdout + result.stderr
    assert "RUNTIME_INIT status=0 detached=1" in result.stdout


def test_web_runtime_initialization_preserves_interpreter_thread_attachment():
    source = BACKEND.read_text(encoding="utf-8")
    assignment = "attachment_mode = ManagedThreadAttachmentMode::AdoptExisting;"
    assignment_pos = source.index(assignment)
    guard_start = source.rfind("#if !FO_WEB", 0, assignment_pos)

    assert guard_start != -1
    guard_end = source.index("#endif", guard_start)
    assert guard_start < assignment_pos < guard_end
