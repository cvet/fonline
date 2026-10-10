"""Exercise boxed collection events through the complete native Managed backend.

The configured unit build supplies compile flags and native libraries; both
managed assemblies are built from Engine sources. No game bake or public test
hook is required. The negative control removes writeback only in a temporary
copy and runs the identical behavioral test.
"""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys

import pytest

import test_managed_async_callbacks as callbacks

ENGINE = Path(__file__).resolve().parents[2]
BACKEND = ENGINE / "Source/Scripting/Managed/ManagedScriptBackend.cpp"
TEST_NAME = "Actual boxed managed collection event writeback"

MANAGED_PROBE = r'''

using System;
using System.Collections.Generic;
using FOnline;

public delegate void MutableList(ref List<int> values);
public delegate void MutableDictionary(ref Dictionary<string, int> values);

public static class Program
{
    public static int Main() => 0;
    public static Delegate ReplaceList() => (MutableList)((ref List<int> values) =>
    {
        values = new List<int> { 42, -1, 73 };
        GC.Collect();
    });
    public static Delegate EditList() => (MutableList)((ref List<int> values) =>
    {
        values[0] += 1;
        values.Add(99);
        GC.Collect();
    });
    public static Delegate ClearList() => (MutableList)((ref List<int> values) => values.Clear());
    public static Delegate ReplaceDictionary() => (MutableDictionary)((ref Dictionary<string, int> values) =>
    {
        values = new Dictionary<string, int> { ["z"] = 30, ["a"] = 10, ["m"] = 20 };
        GC.Collect();
    });
    public static Delegate EditDictionary() => (MutableDictionary)((ref Dictionary<string, int> values) =>
    {
        values["a"] += 1;
        values.Remove("m");
        values["b"] = 40;
        GC.Collect();
    });
    public static Delegate ClearDictionary() => (MutableDictionary)((ref Dictionary<string, int> values) => values.Clear());
}
''' + callbacks.PROBE_SOURCE[callbacks.PROBE_SOURCE.index("\nnamespace FOnline\n{"):]

NATIVE_PROBE = r'''
#include "catch_amalgamated.hpp"
#include "Test_BakerHelpers.h"
#include "ManagedScripting.h"
#include "ImGuiStuff.h"
#include __BACKEND__


FO_BEGIN_NAMESPACE

class CollectionProbeMetadata final : public BaseEngine
{
public:
    explicit CollectionProbeMetadata(ptr<GlobalSettings> settings) :
        BaseEngine(settings, FileSystem {}, [this] { RegisterServerStubMetadata(this, nullptr); })
    {
    }

    auto RunScriptContext(const function<void()>& callback) -> timespan override
    {
        ContextEntries++;
        return BaseEngine::RunScriptContext(callback);
    }

    int32_t ContextEntries {};
};

static auto ReadCollectionProbeAssembly(const std::filesystem::path& path) -> vector<uint8_t>
{
    std::ifstream stream {path, std::ios::binary};
    REQUIRE(stream.is_open());
    return vector<uint8_t> {std::istreambuf_iterator<char> {stream}, std::istreambuf_iterator<char> {}};
}

static auto MakeCollectionSubscription(ptr<ManagedScriptBackend> backend, const char* factory_name,
    const ComplexTypeDesc& type) -> shared_ptr<ManagedEventSubscription>
{
    MonoDomain* domain = GetDomainOrThrow(backend->GetDomain());
    ManagedThreadAttachment attachment {domain};
    REQUIRE(backend->GetImages().size() == 1);
    MonoImage* image = backend->GetImages().front().reinterpret_as<MonoImage>().get();
    MonoClass* program = mono_class_from_name(image, "", "Program");
    REQUIRE(program != nullptr);
    MonoMethod* factory = mono_class_get_method_from_name(program, factory_name, 0);
    REQUIRE(factory != nullptr);
    MonoObject* exception = nullptr;
    MonoObject* handler = mono_runtime_invoke(factory, nullptr, nullptr, &exception);
    REQUIRE(exception == nullptr);
    REQUIRE(handler != nullptr);
    auto subscription = safe_alloc::make_shared<ManagedEventSubscription>();
    subscription->Backend = backend;
    subscription->Domain = domain;
    subscription->Args.emplace_back(type);
    subscription->Handler = NewManagedGcHandle(handler, false);
    REQUIRE(subscription->Handler != 0);
    return subscription;
}

TEST_CASE("Actual boxed managed collection event writeback")
{
    GlobalSettings settings {false};
    CollectionProbeMetadata metadata {&settings};
    FileSystem resources;
    auto source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("Scripts");
    string assembly_dir = MakeManagedAssemblyResourceDir("Server");
    source->AddFile(assembly_dir + "/Probe.Server.dll", ReadCollectionProbeAssembly(
        __ASSEMBLY__));
    source->AddFile(assembly_dir + "/FOnline.ManagedHost.dll", ReadCollectionProbeAssembly(
        __HOST__));
    resources.AddCustomSource(std::move(source));
    InitManagedScripting(&metadata, &resources,
        __CACHE__);
    auto backend = metadata.GetBackend<ManagedScriptBackend>(ScriptSystemBackend::MANAGED_BACKEND_INDEX);
    REQUIRE(backend);
    auto cleanup = scope_exit([&]() noexcept { metadata.ShutdownBackends(); });

    ComplexTypeDesc list_type {.Kind = ComplexTypeKind::Array, .BaseType = metadata.GetBaseType("int32"), .IsMutable = true};
    ComplexTypeDesc dict_type {.Kind = ComplexTypeKind::Dict, .BaseType = metadata.GetBaseType("int32"),
        .KeyType = metadata.GetBaseType("string"), .IsMutable = true};
    auto accessor = make_ptr(&NativeDataProvider::NATIVE_DATA_ACCESSOR);

    SECTION("List replacement, next subscriber edit, and clear")
    {
        vector<int32_t> values {1, 2};
        NativeDataProvider::ArrayDataProxy proxy {values};
        array<ptr<void>, 1> args {make_ptr(&proxy).void_cast()};
        FuncCallData call {.Accessor = accessor, .ArgsData = args};
        auto replace = MakeCollectionSubscription(backend, "ReplaceList", list_type);
        CHECK(DispatchManagedEvent(replace, call) == Entity::EventResult::ContinueChain);
        CHECK(values == vector<int32_t> {42, -1, 73});
        auto edit = MakeCollectionSubscription(backend, "EditList", list_type);
        CHECK(DispatchManagedEvent(edit, call) == Entity::EventResult::ContinueChain);
        CHECK(values == vector<int32_t> {43, -1, 73, 99});
        REQUIRE(proxy.Size() == 4);
        CHECK(proxy.Get(3).get() == &values[3]);
        auto clear = MakeCollectionSubscription(backend, "ClearList", list_type);
        CHECK(DispatchManagedEvent(clear, call) == Entity::EventResult::ContinueChain);
        CHECK(values.empty());
        CHECK(proxy.Size() == 0);
        CHECK(metadata.ContextEntries == 3);
    }

    SECTION("Dictionary replacement, next subscriber edit, native order, and clear")
    {
        map<string, int32_t> values {{"original", 1}};
        NativeDataProvider::DictDataProxy proxy {values};
        array<ptr<void>, 1> args {make_ptr(&proxy).void_cast()};
        FuncCallData call {.Accessor = accessor, .ArgsData = args};
        auto replace = MakeCollectionSubscription(backend, "ReplaceDictionary", dict_type);
        CHECK(DispatchManagedEvent(replace, call) == Entity::EventResult::ContinueChain);
        CHECK(values == map<string, int32_t> {{"a", 10}, {"m", 20}, {"z", 30}});
        auto edit = MakeCollectionSubscription(backend, "EditDictionary", dict_type);
        CHECK(DispatchManagedEvent(edit, call) == Entity::EventResult::ContinueChain);
        CHECK(values == map<string, int32_t> {{"a", 11}, {"b", 40}, {"z", 30}});
        REQUIRE(proxy.Size() == 3);
        CHECK(*proxy.Get(0).first.reinterpret_as<string>() == "a");
        CHECK(*proxy.Get(1).first.reinterpret_as<string>() == "b");
        CHECK(*proxy.Get(2).first.reinterpret_as<string>() == "z");
        auto clear = MakeCollectionSubscription(backend, "ClearDictionary", dict_type);
        CHECK(DispatchManagedEvent(clear, call) == Entity::EventResult::ContinueChain);
        CHECK(values.empty());
        CHECK(proxy.Size() == 0);
        CHECK(metadata.ContextEntries == 3);
    }

    SECTION("A const list rejects caller writeback")
    {
        const vector<int32_t> values {1, 2};
        NativeDataProvider::ArrayDataProxy proxy {values};
        array<ptr<void>, 1> args {make_ptr(&proxy).void_cast()};
        FuncCallData call {.Accessor = accessor, .ArgsData = args};
        auto replace = MakeCollectionSubscription(backend, "ReplaceList", list_type);
        CHECK_THROWS_AS(DispatchManagedEvent(replace, call), InvalidCallException);
        CHECK(values == vector<int32_t> {1, 2});
        CHECK(proxy.Size() == 2);
        CHECK(metadata.ContextEntries == 1);
    }
}

FO_END_NAMESPACE
'''


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run_logged(args: list[str], cwd: Path, name: str, log_dir: Path, **kwargs):
    result = subprocess.run(args, cwd=cwd, capture_output=True, text=True,
                            timeout=600, **kwargs)
    (log_dir / (name + ".log")).write_text(result.stdout + result.stderr, encoding="utf-8")
    return result


def without_collection_writeback(source: str) -> str:
    declaration = ("static void WriteBackManagedEventArg(ptr<ManagedScriptBackend> backend, "
                   "const ComplexTypeDesc& type, MonoObject* value, ptr<void> dst, ptr<const DataAccessor> accessor)")
    definition = source.index(declaration + "\n{")
    start = source.index("    if (type.Kind == ComplexTypeKind::Array || type.Kind == ComplexTypeKind::Dict)", definition)
    end = source.index("    if (type.Kind != ComplexTypeKind::Simple)", start)
    return source[:start] + source[end:]


@pytest.fixture(scope="module")
def collection_inputs(tmp_path_factory):
    configured = os.environ.get("FO_MANAGED_EVENT_BUILD")
    if not configured:
        pytest.skip("FO_MANAGED_EVENT_BUILD must select an existing Managed native unit build")
    build = Path(configured).resolve()
    configured_runtime = os.environ.get("FO_MANAGED_EVENT_RUNTIME")
    assert configured_runtime, "FO_MANAGED_EVENT_RUNTIME must select the prepared host runtime"
    runtime = Path(configured_runtime).resolve()
    dotnet = shutil.which("dotnet")
    ninja = shutil.which("ninja")
    assert dotnet and ninja, "dotnet SDK and Ninja are required for this native probe"
    assert (runtime / "runtime.manifest").is_file(), "Select the prepared target runtime"
    assert (runtime / "lib/netcoreapp/System.Private.CoreLib.dll").is_file()
    config = os.environ.get("FO_MANAGED_EVENT_CONFIG", "RelWithDebInfo")
    commands = json.loads((build / "compile_commands.json").read_text(encoding="utf-8"))
    entries = [item for item in commands if item["file"].endswith("/Tests/Test_EngineMetadata.cpp")]
    ninja_file = build / ("build-" + config + ".ninja")
    if ninja_file.exists():
        entries = [item for item in entries if config in str(item.get("arguments", item.get("command")))]
        ninja_args = [ninja, "-f", ninja_file.name]
    else:
        assert (build / "build.ninja").is_file(), "Select a Ninja native unit build"
        ninja_args = [ninja]
    assert len(entries) == 1, "Select exactly one unit-test build configuration"
    entry = entries[0]
    compile_args = entry.get("arguments") or shlex.split(entry["command"])
    assert "-DFO_MANAGED_SCRIPTING=1" in compile_args, "The unit build must enable Managed scripting"
    assert "-o" in compile_args and "-c" in compile_args, "This probe needs GCC/Clang compile commands"
    original_object = compile_args[compile_args.index("-o") + 1]
    target_dir = next(part for part in Path(original_object).parts if part.endswith("_UnitTests.dir"))
    target = target_dir.removesuffix(".dir")
    result = subprocess.run(ninja_args + ["-t", "commands", target], cwd=build,
                            capture_output=True, text=True, timeout=30, check=True)
    links = []
    for line in result.stdout.splitlines():
        args = shlex.split(line)
        if "-c" in args or "-o" not in args or Path(args[args.index("-o") + 1]).name != target:
            continue
        compiler = next(i for i, arg in enumerate(args) if Path(arg).name in ("g++", "c++", "clang++"))
        end = next((i for i in range(compiler + 1, len(args)) if args[i] == "&&"), len(args))
        links.append(args[compiler:end])
    assert len(links) == 1, "The configured native unit link command must be unambiguous"
    link_args = [arg for arg in links[0] if not (arg.endswith(".o") and target_dir in Path(arg).parts)
                 or arg.endswith("/Applications/TestingApp.cpp.o")]
    output = tmp_path_factory.mktemp("managed-event-collections")
    managed = output / "managed"
    managed.mkdir()
    for name in ("ScriptStaticCleanup.cs", "EntityWrapperTracker.cs"):
        shutil.copyfile(callbacks.CORE / name, managed / name)
    project = callbacks.PROJECT.replace("<TargetFramework>", "<AssemblyName>Probe.Server</AssemblyName><TargetFramework>")
    callbacks.build_probe(dotnet, managed, MANAGED_PROBE, project_source=project)
    host = output / "host"
    host.mkdir()
    shutil.copyfile(ENGINE / "Source/Scripting/Managed/ManagedHost/ManagedLoadContextHost.cs", host / "ManagedLoadContextHost.cs")
    (host / "Host.csproj").write_text(callbacks.PROJECT.replace("<OutputType>Exe</OutputType>", "<OutputType>Library</OutputType><AssemblyName>FOnline.ManagedHost</AssemblyName>"), encoding="utf-8")
    built = run_logged([dotnet, "build", "Host.csproj", "--nologo", "-v:minimal"], host, "host", output)
    assert built.returncode == 0, built.stdout + built.stderr
    paths = [BACKEND, ENGINE / "Source/Common/ScriptSystem.h",
             ENGINE / "Source/Scripting/Managed/ManagedHost/ManagedLoadContextHost.cs"]
    paths += list(callbacks.CORE.glob("*.cs"))
    inputs = {str(path): sha(path) for path in paths}
    for arg in link_args:
        if not arg.startswith("-") and arg.endswith((".a", ".o", ".so", ".dylib")):
            path = Path(arg)
            path = path if path.is_absolute() else build / path
            inputs[str(path)] = sha(path)
    return {"build": build, "runtime": runtime, "compile": compile_args, "link": link_args,
            "entry": entry, "output": output, "inputs": inputs,
            "assembly": managed / "bin/Debug/net10.0/Probe.Server.dll",
            "host": host / "bin/Debug/net10.0/FOnline.ManagedHost.dll"}


def build_native_probe(inputs, *, negative: bool):
    output = inputs["output"] / ("without-writeback" if negative else "production")
    output.mkdir()
    backend = BACKEND
    if negative:
        backend = output / "ManagedScriptBackend.cpp"
        backend.write_text(without_collection_writeback(BACKEND.read_text(encoding="utf-8")), encoding="utf-8")
    source_text = NATIVE_PROBE
    for key, path in (("__BACKEND__", backend), ("__ASSEMBLY__", inputs["assembly"]),
                      ("__HOST__", inputs["host"]), ("__CACHE__", output / "Cache")):
        source_text = source_text.replace(key, json.dumps(path.as_posix()))
    source = output / "probe.cpp"
    source.write_text(source_text, encoding="utf-8")
    obj = output / "probe.cpp.o"
    compile_args = list(inputs["compile"])
    compile_args[compile_args.index("-o") + 1] = str(obj)
    compile_args[compile_args.index("-c") + 1] = str(source)
    if "-MF" in compile_args:
        compile_args[compile_args.index("-MF") + 1] = str(output / "probe.cpp.d")
    if "-MT" in compile_args:
        compile_args[compile_args.index("-MT") + 1] = str(obj)
    compile_args += ["-I" + str(ENGINE / "Source/Tests"), "-Werror"]
    compiled = run_logged(compile_args, Path(inputs["entry"]["directory"]), "compile", output)
    assert compiled.returncode == 0, compiled.stdout + compiled.stderr
    binary = output / "collection-probe"
    link_args = list(inputs["link"])
    link_args[link_args.index("-o") + 1] = str(binary)
    link_args.insert(link_args.index("-o"), str(obj))
    linked = run_logged(link_args, inputs["build"], "link", output)
    assert linked.returncode == 0, linked.stdout + linked.stderr
    assert not re.search(r"\bwarning:", compiled.stdout + compiled.stderr + linked.stdout + linked.stderr)
    manifest = {"negative_control": negative, "compile": compile_args, "link": link_args,
                "source_sha256": sha(source), "backend_sha256": sha(backend), "inputs": inputs["inputs"],
                "assembly_sha256": sha(inputs["assembly"]), "host_sha256": sha(inputs["host"]),
                "limit": "Complete outer BaseEngine event entry; int lists and string/int dictionaries, not entity payloads or ServerEngine synchronization"}
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    assert all(sha(Path(path)) == digest for path, digest in inputs["inputs"].items())
    env = os.environ.copy()
    env["FO_MANAGED_RUNTIME"] = str(inputs["runtime"])
    variable = "DYLD_LIBRARY_PATH" if sys.platform == "darwin" else "LD_LIBRARY_PATH"
    env[variable] = str(inputs["runtime"] / "lib") + (os.pathsep + env[variable] if env.get(variable) else "")
    result = run_logged([str(binary), TEST_NAME, "--reporter", "compact"], output, "runtime", output,
                        env=env, preexec_fn=callbacks.disable_core_dump if os.name == "posix" else None)
    assert all(sha(Path(path)) == digest for path, digest in inputs["inputs"].items())
    (output / "result.json").write_text(json.dumps({"exit_code": result.returncode,
        "source_unchanged": True, "runtime_log_sha256": sha(output / "runtime.log")}, indent=2) + "\n", encoding="utf-8")
    assert "VerificationException:" not in result.stdout + result.stderr
    assert "Managed runtime directory not found" not in result.stdout + result.stderr
    return result


def test_boxed_collection_events_write_back_to_the_native_caller(collection_inputs):
    result = build_native_probe(collection_inputs, negative=False)
    assert result.returncode == 0, result.stdout + result.stderr
    assert "All tests passed" in result.stdout
    assert "ScriptSystemException:" not in result.stdout + result.stderr


def test_without_collection_writeback_the_same_event_test_fails(collection_inputs):
    result = build_native_probe(collection_inputs, negative=True)
    assert result.returncode == 42, result.stdout + result.stderr
    assert "Managed mutable event argument type is not supported" in result.stdout + result.stderr
    assert re.search(r"test cases:\s+1\s*\|\s*0 passed\s*\|\s*1 failed", result.stdout)
