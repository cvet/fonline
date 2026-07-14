//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "catch_amalgamated.hpp"

#if FO_ANGELSCRIPT_SCRIPTING
#include "AngelScriptScripting.h"
#include "Baker.h"
#include "Server.h"
#include "Test_BakerHelpers.h"

#include <angelscript.h>
#endif

FO_BEGIN_NAMESPACE

#if FO_ANGELSCRIPT_SCRIPTING

// Regression coverage for every script CALL shape that establishes a frame, with 8-byte locals and
// argument blocks of varying DWORD parity at each level — so any frame-base or argument-slot
// alignment regression trips a hard `alignment` runtime error under the UBSan leg, and any
// value/aliasing regression fails the exact checksums on every build:
//   - free function calls and a deep chain with different argument parity per level;
//   - class methods (`this` + small argument = odd combined block) and constructors with arguments;
//   - virtual override through a base handle, interface method, funcdef call and a method delegate;
//   - by-value parameters of every width (8-byte value types included), const in-references read
//     from the caller's 8-byte locals, and out-references written back across frames;
//   - a script call into a registered engine method returning `vector<ptr<T>>`, pinning the
//     generated native-call cast's return-type spelling under `-fsanitize=function`.
// Storage-layout coverage (class member offsets, globals, arrays) lives in
// Test_AngelScriptAlignment.cpp.
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

    // Deep call chain with varying argument-block parity at every level, 8-byte locals at each.
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
    // the constructor itself takes a small argument and initializes 8-byte members.
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
    // 8-byte locals of the caller.
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
    // into argument slots, the callee reads them from its frame.
    int64 SumMixedParams(int8 a, int64 b, int16 c, double d, bool e, ident id, timespan ts)
    {
        return a + b + c + int64(d) + (e ? 1 : 0) + ts.seconds;
    }

    int64 UseMixedParams()
    {
        return SumMixedParams(1, 10, 2, 3.5, true, ident(), timespan(5, 3));
    }

    // Script call into a registered engine method returning vector<ptr<T>>: pins the generated
    // native-call cast's return-type spelling under -fsanitize=function.
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
                    const auto message_str = string(message);

                    if (message_str.find("error") != string::npos || message_str.find("Error") != string::npos || message_str.find("fatal") != string::npos || message_str.find("Fatal") != string::npos) {
                        throw ScriptSystemException(message_str);
                    }
                });
        }

        static auto MakeResources() -> FileSystem
        {
            const auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();

            auto compiler_resources_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("CallTestCompilerResources");
            compiler_resources_source->AddFile("Metadata.fometa-server", metadata_blob);

            FileSystem compiler_resources;
            compiler_resources.AddCustomSource(std::move(compiler_resources_source));

            const auto script_blob = MakeScriptBinary(compiler_resources);

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

    const auto startup_error = CallTestRig::WaitForStart(server);
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    const auto fn = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    const auto call_and_check = [&](string_view func_name, int64_t expected) {
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
    // test-side server lock held the sync context may legitimately hold entities.
    {
        auto func = server->FindFunc<int64_t>(fn("CallTest::CallVectorReturningApi"));
        REQUIRE(func);
        REQUIRE(func.Call());
        CHECK(func.GetResult() >= 0);
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
            NativeDataProvider::WriteHandleSlot(call.RetData.as_ptr(), returned_obj);
        };
        func_desc.ReturnValueCleaner = [&release_count, returned_obj](ptr<void> ret_data) {
            const nptr<void> stored_obj = NativeDataProvider::ReadHandleSlot(ret_data);

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

TEST_CASE("VoidScriptFuncDoesNotStoreReturnCleaner")
{
    CHECK(sizeof(ScriptFunc<void>) < sizeof(ScriptFunc<nptr<void>>));

    ScriptFuncDesc func_desc;
    func_desc.ReturnValueCleaner = [](ptr<void>) { FO_UNREACHABLE_PLACE(); };

    ScriptFunc<void> func {&func_desc};
    CHECK(func);
}

#endif

FO_END_NAMESPACE
