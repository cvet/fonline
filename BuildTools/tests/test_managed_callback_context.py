"""Check the canonical callback scope with the compiled engine's synchronization primitives."""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

import pytest


ENGINE = Path(__file__).resolve().parents[2]
BACKEND = ENGINE / "Source/Scripting/Managed/ManagedScriptBackend.cpp"
SERVER = ENGINE / "Source/Server/Server.cpp"

PREFIX = r'''
#include "Common.h"
#include "EntitySync.h"
#include "catch_amalgamated.hpp"

FO_BEGIN_NAMESPACE
namespace ManagedCallbackContextProbe
{
class EngineMetadata
{
public:
    virtual ~EngineMetadata() = default;
};
class BaseEngine : public EngineMetadata
{
public:
    virtual auto RunScriptContext(const function<void()>& callback) -> timespan = 0;
};
class ServerEngine : public BaseEngine
{
public:
    auto RunScriptContext(const function<void()>& callback) -> timespan override;
};
class ManagedScriptBackend
{
public:
    explicit ManagedScriptBackend(ptr<BaseEngine> engine) : _engine(engine) { }
    auto GetMetadata() -> nptr<EngineMetadata> { return _engine; }
private:
    ptr<BaseEngine> _engine;
};
struct ComplexTypeDesc { };
struct FuncCallData
{
    function<void()> Body;
};
static void DispatchManagedCallbackInContext(ptr<ManagedScriptBackend>, uint32_t, const ComplexTypeDesc&, const vector<ComplexTypeDesc>&, FuncCallData& call)
{
    FO_STACK_TRACE_ENTRY();

    call.Body();
}
'''

SUFFIX = r'''
}
TEST_CASE("ManagedCallbackPreservesCallerSyncContext")
{
    using namespace ManagedCallbackContextProbe;
    ManagedCallbackContextProbe::ServerEngine server;
    ManagedScriptBackend backend {&server};
    EntityLock caller_lock;
    EntityLock callback_lock;
    ScopedSyncContext caller;
    caller.GetContext().LockSingleton(&caller_lock);
    auto caller_context = SyncContext::GetCurrentOnThisThread();
    bool should_throw = false;
    bool replace_cover = false;

    SECTION("ReturnAfterRelease") { }
    SECTION("ReturnAfterReplacingCover") { replace_cover = true; }
    SECTION("ThrowAfterReplacingCover") { replace_cover = true; should_throw = true; }

    ManagedCallbackContextProbe::FuncCallData call {[&] {
        auto inner = SyncContext::GetCurrentOnThisThread();
        REQUIRE(inner);
        CHECK(inner != caller_context);
        inner->Release();
        CHECK(caller_lock.IsLockedByCurrentThread());
        if (replace_cover) {
            inner->SyncEntities({});
            inner->LockSingleton(&callback_lock);
            CHECK(callback_lock.IsLockedByCurrentThread());
            CHECK(caller_lock.IsLockedByCurrentThread());
        }

        if (should_throw) {
            throw std::runtime_error("Requested callback failure");
        }
    }};

    if (should_throw) {
        CHECK_THROWS_WITH(DispatchManagedCallback(&backend, 0, {}, {}, call), "Requested callback failure");
    }
    else {
        CHECK_NOTHROW(DispatchManagedCallback(&backend, 0, {}, {}, call));
    }

    CHECK(SyncContext::GetCurrentOnThisThread() == caller_context);
    CHECK(caller_lock.IsLockedByCurrentThread());
    CHECK_FALSE(callback_lock.IsLockedByCurrentThread());
    caller.GetContext().Release();
    CHECK_FALSE(caller_lock.IsLockedByCurrentThread());
}
FO_END_NAMESPACE
'''


def function_source(source: str, declaration: str) -> str:
    start = source.index(declaration + "\n{")
    return source[start:source.index("\n}\n", start) + len("\n}\n")]


def render_probe(backend_source: str, server_source: str, *, without_callback_scope: bool = False) -> str:
    declaration = "static void DispatchManagedCallback(ptr<ManagedScriptBackend> backend, uint32_t handler_handle, const ComplexTypeDesc& ret, const vector<ComplexTypeDesc>& args, FuncCallData& call)"
    wrapper = function_source(backend_source, declaration)
    if without_callback_scope:
        boundary = "engine->RunScriptContext([&] { DispatchManagedCallbackInContext(backend, handler_handle, ret, args, call); });"
        assert wrapper.count(boundary) == 1
        wrapper = wrapper.replace(boundary, "DispatchManagedCallbackInContext(backend, handler_handle, ret, args, call);")
    run_context = function_source(server_source, "auto ServerEngine::RunScriptContext(const function<void()>& callback) -> timespan")
    return PREFIX + run_context + wrapper + SUFFIX


def sha(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def build_context_probe(build: Path, output: Path, *, without_callback_scope: bool = False) -> Path:
    commands = json.loads((build / "compile_commands.json").read_text(encoding="utf-8"))
    entry = next(item for item in commands if item["file"].endswith("/Tests/Test_EntitySync.cpp"))
    compile_args = entry.get("arguments") or shlex.split(entry["command"])
    original_object = compile_args[compile_args.index("-o") + 1]
    target_dir = Path(original_object).parts[:2]
    backend_source = BACKEND.read_text(encoding="utf-8")
    server_source = SERVER.read_text(encoding="utf-8")
    source = output / "callback-context.cpp"
    source.write_text(render_probe(backend_source, server_source, without_callback_scope=without_callback_scope), encoding="utf-8")
    obj = output / "callback-context.cpp.o"
    compile_args[compile_args.index("-o") + 1] = str(obj)
    compile_args[compile_args.index("-c") + 1] = str(source)
    compile_args += ["-Werror"]
    compiled = subprocess.run(compile_args, cwd=entry["directory"], capture_output=True, text=True, timeout=120)
    (output / "compile.log").write_text(compiled.stdout + compiled.stderr, encoding="utf-8")
    assert compiled.returncode == 0, compiled.stdout + compiled.stderr
    link_file = build.joinpath(*target_dir, "link.txt")
    link_args = shlex.split(link_file.read_text(encoding="utf-8"))
    link_args = [arg for arg in link_args if not arg.endswith(".o") or arg.endswith("/Applications/TestingApp.cpp.o")]
    executable = output / "callback-context"
    link_args[link_args.index("-o") + 1] = str(executable)
    link_args.insert(link_args.index("-o"), str(obj))
    inputs = []
    for argument in link_args:
        if argument.endswith((".a", ".o")):
            path = Path(argument)
            path = path if path.is_absolute() else build / path
            inputs.append({"path": str(path), "sha256": sha(path)})
    linked = subprocess.run(link_args, cwd=build, capture_output=True, text=True, timeout=120)
    (output / "link.log").write_text(linked.stdout + linked.stderr, encoding="utf-8")
    manifest = {"compile": compile_args, "link": link_args, "link_inputs": inputs,
                "backend_sha256": hashlib.sha256(backend_source.encode()).hexdigest(),
                "server_sha256": hashlib.sha256(server_source.encode()).hexdigest(),
                "probe_sha256": sha(source),
                "limit": "Exact callback wrapper and ServerEngine method on a fixture host; compiled real SyncContext and EntityLock, no complete running server"}
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    assert linked.returncode == 0, linked.stdout + linked.stderr
    assert not linked.stdout and not linked.stderr
    assert all(sha(Path(item["path"])) == item["sha256"] for item in inputs), "Native link inputs changed during probe build"
    return executable


@pytest.fixture(scope="module")
def callback_context_probe(tmp_path_factory):
    configured = os.environ.get("FO_MANAGED_CALLBACK_BUILD")
    if sys.platform != "linux" or not configured:
        pytest.skip("FO_MANAGED_CALLBACK_BUILD must select an existing Linux Makefiles unit-test build")
    build = Path(configured).resolve()
    output = tmp_path_factory.mktemp("managed-callback-context")
    return build_context_probe(build, output)


def test_callback_preserves_caller_cover(callback_context_probe):
    result = subprocess.run([str(callback_context_probe), "ManagedCallbackPreservesCallerSyncContext", "--reporter", "compact"],
                            cwd=callback_context_probe.parent, capture_output=True, text=True, timeout=30)
    (callback_context_probe.parent / "test.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    assert result.returncode == 0, result.stdout + result.stderr
    assert "All tests passed" in result.stdout
