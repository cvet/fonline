//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "catch_amalgamated.hpp"

#if FO_ANGELSCRIPT_SCRIPTING
#include "AngelScriptScripting.h"
#include "Baker.h"
#include "Server.h"
#include "Test_BakerHelpers.h"

#include <angelscript.h>
#include <as_scriptengine.h>
#endif

FO_BEGIN_NAMESPACE

#if FO_ANGELSCRIPT_SCRIPTING

// Exercise every call-frame shape with 8-byte locals and both argument-slot parities.
// Exact checksums pin values and aliasing while UBSan pins alignment
namespace
{
    struct CallTestRig
    {
        static auto MakeSettings() -> GlobalSettings
        {
            auto settings = GlobalSettings(false);

            settings.ApplyDefaultSettings();
            settings.ApplyAutoSettings();

            BakerTests::ApplySelfContainedServerSettings(settings);

            return settings;
        }

        static auto MakeScriptBinary(const FileSystem& metadata_resources) -> vector<uint8_t>
        {
            BakerServerEngine compiler_engine {metadata_resources};

            return BakerTests::CompileInlineScripts(&compiler_engine, "CallTestScripts",
                {
                    {"Scripts/CallTest.fos",
                        R"(
namespace CallTest
{
    funcdef int64 UnaryOp(int8);

    // Vary argument-block parity across a deep call chain with 8-byte locals
    int64 Chain3(int8 a, int16 b, int8 c)
    {
        int64 wide = 1;
        nanotime nt;

        return wide + a + b + c;
    }

    int64 Chain2(int8 a)
    {
        int64 wide = 2;
        ident id;

        return wide + Chain3(a, 2, 3) + a;
    }

    int64 Chain1()
    {
        int64 wide = 3;
        timespan ts;

        return wide + Chain2(1);
    }

    // Class method with `this` + one small argument (odd combined block) and 8-byte locals inside;
    // the constructor itself takes a small argument and initializes 8-byte members
    class OpHost
    {
        int8 Bias;
        ident HostId;
        timespan HostTs;

        OpHost(int8 b)
        {
            Bias = b;
            HostId = ident();
            HostTs = timespan(8, 3);
        }

        int64 Op(int8 x)
        {
            int64 wide = 30;
            double real = 1.5;

            return wide + int64(real) + x + Bias;
        }
    }

    int64 UseMethodCall()
    {
        OpHost host = OpHost(2);

        return host.Op(3) + host.HostTs.seconds;
    }

    int64 CallThroughDelegate()
    {
        OpHost host = OpHost(1);
        UnaryOp op = UnaryOp(host.Op);

        return op(1);
    }

    interface IWide
    {
        int64 Compute(int8 pad);
    }

    class WideImpl : IWide
    {
        int64 Compute(int8 pad)
        {
            int64 wide = 50;
            timespan ts = timespan(2, 3);

            return wide + pad + ts.seconds;
        }
    }

    int64 CallThroughInterface()
    {
        IWide it = WideImpl();

        return it.Compute(1);
    }

    class BaseVirt
    {
        int64 Compute(int8 pad)
        {
            return 1;
        }
    }

    class DerivedVirt : BaseVirt
    {
        int64 Compute(int8 pad) override
        {
            int64 wide = 60;
            double real = 2.5;

            return wide + int64(real) + pad;
        }
    }

    int64 CallVirtual()
    {
        BaseVirt b = DerivedVirt();

        return b.Compute(1);
    }

    // Reference parameters: out-references written by the callee, const in-references read from
    // 8-byte locals of the caller
    void WriteRefs(int64 &out wide, timespan &out ts, double &out real)
    {
        wide = 70;
        ts = timespan(3, 3);
        real = 2.5;
    }

    int64 UseRefParams()
    {
        int8 pad = 1;
        int64 wide = 0;
        timespan ts;
        double real = 0.0;

        WriteRefs(wide, ts, real);

        return wide + ts.seconds + int64(real) + pad;
    }

    int64 ReadConstRefs(const int64 &in wide, const ident &in id, const timespan &in ts)
    {
        return wide + ts.seconds;
    }

    int64 UseConstRefs()
    {
        int64 wide = 20;
        ident id;
        timespan ts = timespan(4, 3);

        return ReadConstRefs(wide, id, ts);
    }

    // By-value parameters of every width, 8-byte value types included: the caller constructs them
    // into argument slots, the callee reads them from its frame
    int64 SumMixedParams(int8 a, int64 b, int16 c, double d, bool e, ident id, timespan ts)
    {
        return a + b + c + int64(d) + (e ? 1 : 0) + ts.seconds;
    }

    int64 UseMixedParams()
    {
        return SumMixedParams(1, 10, 2, 3.5, true, ident(), timespan(5, 3));
    }

    // Script call into a registered engine method returning vector<ptr<T>>: pins the generated
    // native-call cast's return-type spelling under -fsanitize=function
    int64 CallVectorReturningApi()
    {
        Entity[] held = Game.GetHeldSyncEntities();

        return int64(held.length());
    }

    int[] ReturnIntArray()
    {
        int[] values = {4, 5, 6};
        return values;
    }

    dict<int, int> ReturnIntDict()
    {
        dict<int, int> values = {};
        values[2] = 7;
        values[3] = 9;
        return values;
    }
}
)"},
                },
                [](string_view message) {
                    string message_str = string(message);

                    if (message_str.find("error") != string::npos || message_str.find("Error") != string::npos || message_str.find("fatal") != string::npos || message_str.find("Fatal") != string::npos) {
                        throw ScriptSystemException(message_str);
                    }
                });
        }

        static auto MakeResources() -> FileSystem
        {
            auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();

            auto compiler_resources_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("CallTestCompilerResources");
            compiler_resources_source->AddFile("Metadata.fometa-server", metadata_blob);

            FileSystem compiler_resources;
            compiler_resources.AddCustomSource(std::move(compiler_resources_source));

            auto script_blob = MakeScriptBinary(compiler_resources);

            auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("CallTestRuntimeResources");
            runtime_source->AddFile("Metadata.fometa-server", metadata_blob);
            runtime_source->AddFile("CallTest.fos-bin-server", script_blob);

            FileSystem resources;
            resources.AddCustomSource(std::move(runtime_source));

            return resources;
        }

        static auto WaitForStart(ptr<ServerEngine> server) -> string
        {
            for (int32_t i = 0; i < 6000; i++) {
                if (server->IsStarted()) {
                    return {};
                }
                if (server->IsStartingError()) {
                    return "ServerEngine startup failed";
                }

                std::this_thread::sleep_for(std::chrono::milliseconds {10});
            }

            return "ServerEngine startup timed out";
        }

        static auto MakeServerEngine(GlobalSettings& settings) -> refcount_ptr<ServerEngine> { return SafeAlloc::MakeRefCounted<ServerEngine>(&settings, MakeResources()); }
    };
}

TEST_CASE("AngelScriptCallShapes")
{
    auto settings = CallTestRig::MakeSettings();
    auto server = CallTestRig::MakeServerEngine(settings);

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    string startup_error = CallTestRig::WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    auto fn = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    auto call_and_check = [&](string_view func_name, int64_t expected) {
        INFO(func_name);
        auto func = server->FindFunc<int64_t>(fn(func_name));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == expected);
    };

    call_and_check("CallTest::Chain1", 13);
    call_and_check("CallTest::UseMethodCall", 44);
    call_and_check("CallTest::CallThroughDelegate", 33);
    call_and_check("CallTest::CallThroughInterface", 53);
    call_and_check("CallTest::CallVirtual", 63);
    call_and_check("CallTest::UseRefParams", 76);
    call_and_check("CallTest::UseConstRefs", 24);
    call_and_check("CallTest::UseMixedParams", 22);

    {
        auto func = server->FindFunc<vector<int32_t>>(fn("CallTest::ReturnIntArray"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == vector<int32_t> {4, 5, 6});
    }

    {
        auto func = server->FindFunc<map<int32_t, int32_t>>(fn("CallTest::ReturnIntDict"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() == map<int32_t, int32_t> {{2, 7}, {3, 9}});
    }

    // The vector<ptr<T>>-returning call is about the generated cast, not the value: with the
    // test-side server lock held the sync context may legitimately hold entities
    {
        auto func = server->FindFunc<int64_t>(fn("CallTest::CallVectorReturningApi"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() >= 0);
    }
}

TEST_CASE("AngelScriptTypeIdsAreLazilyAssignedAcrossThreads")
{
    nptr<AngelScript::asIScriptEngine> as_engine = AngelScript::asCreateScriptEngine(ANGELSCRIPT_VERSION);
    REQUIRE(as_engine);
    auto release_engine = scope_exit([&as_engine]() noexcept {
        safe_call([&as_engine] {
            if (as_engine) {
                as_engine->ShutDownAndRelease();
            }
        });
    });
    nptr<AngelScript::asCScriptEngine> engine = as_engine.dyn_cast<AngelScript::asCScriptEngine>();
    REQUIRE(engine);

    constexpr size_t THREAD_COUNT = 16;
    constexpr size_t TYPE_COUNT = 128;

    vector<AngelScript::asCDataType> probe_types;
    probe_types.reserve(TYPE_COUNT);

    for (size_t type_index = 0; type_index < TYPE_COUNT; type_index++) {
        const string type_name = strex("ConcurrentTypeIdProbe{}", type_index);
        REQUIRE(as_engine->RegisterObjectType(type_name.c_str(), 0, AngelScript::asOBJ_REF | AngelScript::asOBJ_NOCOUNT) >= 0);
        nptr<AngelScript::asITypeInfo> type_info = as_engine->GetTypeInfoByName(type_name.c_str());
        REQUIRE(type_info);
        nptr<AngelScript::asCTypeInfo> concrete_type_info = type_info.dyn_cast<AngelScript::asCTypeInfo>();
        REQUIRE(concrete_type_info);
        probe_types.emplace_back(AngelScript::asCDataType::CreateType(concrete_type_info.get_no_const(), false));
    }

    vector<vector<int32_t>> results(THREAD_COUNT, vector<int32_t>(TYPE_COUNT, -1));
    std::atomic<size_t> ready_count = 0;
    std::atomic<size_t> completed_count = 0;
    std::atomic<size_t> phase = 0;
    vector<thread> workers;
    workers.reserve(THREAD_COUNT);

    for (size_t thread_index = 0; thread_index < THREAD_COUNT; thread_index++) {
        workers.emplace_back(run_thread("AngelScriptTypeIdProbe", [&, thread_index] {
            for (size_t type_index = 0; type_index < TYPE_COUNT; type_index++) {
                ready_count.fetch_add(1, std::memory_order_release);

                while (phase.load(std::memory_order_acquire) <= type_index) {
                    std::this_thread::yield();
                }

                results[thread_index][type_index] = engine->GetTypeIdFromDataType(probe_types[type_index]);
                completed_count.fetch_add(1, std::memory_order_release);
            }
        }));
    }

    for (size_t type_index = 0; type_index < TYPE_COUNT; type_index++) {
        const size_t expected_count = (type_index + 1) * THREAD_COUNT;

        while (ready_count.load(std::memory_order_acquire) < expected_count) {
            std::this_thread::yield();
        }

        phase.store(type_index + 1, std::memory_order_release);

        while (completed_count.load(std::memory_order_acquire) < expected_count) {
            std::this_thread::yield();
        }
    }

    for (thread& worker : workers) {
        worker.join();
    }

    for (size_t type_index = 0; type_index < TYPE_COUNT; type_index++) {
        const int32_t expected_type_id = results[0][type_index];
        REQUIRE(expected_type_id > AngelScript::asTYPEID_DOUBLE);
        REQUIRE(as_engine->GetTypeInfoById(expected_type_id));

        for (size_t thread_index = 1; thread_index < THREAD_COUNT; thread_index++) {
            CHECK(results[thread_index][type_index] == expected_type_id);
        }
    }
}

TEST_CASE("ScriptFuncCleansStoredReturnHandle")
{
    ScriptFunc<nptr<void>> empty_func {unique_del_nptr<ScriptFuncDesc> {}};
    CHECK_FALSE(empty_func);

    int32_t release_count = 0;
    nptr<void> returned_obj = std::addressof(release_count);

    ScriptFunc<nptr<void>> moved;

    {
        ScriptFunc<nptr<void>> func;
        ScriptFuncDesc func_desc;
        func_desc.Call = [returned_obj](FuncCallData& call) {
            FO_VERIFY_AND_THROW(call.RetData, "Missing return storage");
            NativeDataProvider::WriteHandleSlot(call.RetData, returned_obj);
        };
        func_desc.ReturnValueCleaner = [&release_count, returned_obj](ptr<void> ret_data) {
            nptr<void> stored_obj = NativeDataProvider::ReadHandleSlot(ret_data);

            if (stored_obj == returned_obj) {
                release_count++;
                NativeDataProvider::WriteHandleSlot(ret_data, nullptr);
            }
        };

        func = ScriptFunc<nptr<void>> {&func_desc};
        REQUIRE(func.Call());
        CHECK(func.GetResult() == returned_obj);
        CHECK(release_count == 0);

        moved = std::move(func);
    }

    CHECK(release_count == 0);

    ScriptFunc<nptr<void>> moved_again {std::move(moved)};
    CHECK(release_count == 0);

    moved_again = ScriptFunc<nptr<void>> {};
    CHECK(release_count == 1);
}

TEST_CASE("VoidScriptFuncDoesNotRetainReturnCleanerAcrossDeferredLifetime")
{
    CHECK(sizeof(ScriptFunc<void>) < sizeof(ScriptFunc<nptr<void>>));

    constexpr size_t CALLBACK_COUNT = 64;

    int32_t call_count = 0;
    int32_t failed_call_count = 0;
    auto cleanup_token = SafeAlloc::MakeShared<int32_t>(1);
    weak_ptr<int32_t> cleanup_token_weak = cleanup_token;

    ScriptFuncDesc func_desc;
    func_desc.Call = [&call_count](FuncCallData& call) {
        ignore_unused(call);
        call_count++;
    };
    func_desc.ReturnValueCleaner = [cleanup_token](ptr<void> ret_data) {
        ignore_unused(cleanup_token, ret_data);
        FO_UNREACHABLE_PLACE();
    };

    vector<function<void()>> deferred_callbacks;
    deferred_callbacks.reserve(CALLBACK_COUNT);

    for (size_t i = 0; i < CALLBACK_COUNT; i++) {
        ScriptFunc<void> callback_func;
        callback_func = ScriptFunc<void> {&func_desc};

        auto stored_func = SafeAlloc::MakeShared<ScriptFunc<void>>(std::move(callback_func));
        deferred_callbacks.emplace_back([stored_func, &failed_call_count]() mutable {
            if (!stored_func->Call()) {
                failed_call_count++;
            }
        });
    }

    func_desc.ReturnValueCleaner = {};
    cleanup_token.reset();

    CHECK_FALSE(cleanup_token_weak.lock());

    for (auto& callback : deferred_callbacks) {
        callback();
    }

    CHECK(failed_call_count == 0);
    CHECK(call_count == CALLBACK_COUNT);

    deferred_callbacks.clear();
    CHECK_FALSE(cleanup_token_weak.lock());
}

#endif

FO_END_NAMESPACE
