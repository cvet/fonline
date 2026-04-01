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

#include "AngelScriptScripting.h"
#include "Application.h"
#include "Baker.h"
#include "Client.h"
#include "CritterView.h"
#include "DataSerialization.h"
#include "PlayerView.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

namespace
{
    static auto MakeClientTestSettings() -> GlobalSettings
    {
        auto settings = GlobalSettings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        BakerTests::ApplySelfContainedClientSettings(settings);

        return settings;
    }

    static auto MakeClientScriptBinary(const FileSystem& metadata_resources) -> vector<uint8>
    {
        BakerClientEngine compiler_engine {metadata_resources};

        return BakerTests::CompileInlineScripts(&compiler_engine, "ClientEngineScripts",
            {
                {"Scripts/ClientEngineTest.fos", R"(
namespace ClientEngineTest
{
    int StartCalls = 0;
    int LoopCalls = 0;
    int ManualCalls = 0;

    void ModuleInit()
    {
        Game.OnStart.Subscribe(OnStart);
        Game.OnLoop.Subscribe(OnLoop);
    }

    void OnStart()
    {
        StartCalls++;
    }

    void OnLoop()
    {
        LoopCalls++;
    }

    void UnitTestNoop() {}

    void UnitTestMarkManualCall()
    {
        ManualCalls++;
    }

    int UnitTestGetStartCalls()
    {
        return StartCalls;
    }

    int UnitTestGetLoopCalls()
    {
        return LoopCalls;
    }

    int UnitTestGetManualCalls()
    {
        return ManualCalls;
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

    static auto MakeClientTestResources() -> FileSystem
    {
        const auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();

        auto compiler_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ClientEngineCompilerResources");
        compiler_source->AddFile("Metadata.fometa-client", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_source));

        BakerClientEngine proto_engine {compiler_resources};
        const auto critter_type = proto_engine.Hashes.ToHashedString("Critter");
        const auto proto_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, "UnitTestClientCritter");
        const auto script_blob = MakeClientScriptBinary(compiler_resources);

        auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ClientEngineRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-client", metadata_blob);
        runtime_source->AddFile("ClientEngineTest.fopro-bin-client", proto_blob);
        runtime_source->AddFile("ClientEngineTest.fos-bin-client", script_blob);

        FileSystem resources;
        resources.AddCustomSource(std::move(runtime_source));
        return resources;
    }
}

TEST_CASE("ClientEngineStartsAndRegistersEntities")
{
    auto settings = MakeClientTestSettings();
    auto client = SafeAlloc::MakeRefCounted<ClientEngine>(settings, MakeClientTestResources(), &App->MainWindow);

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    CHECK_FALSE(client->IsConnecting());
    CHECK_FALSE(client->IsConnected());
    CHECK(client->GetCurPlayer() == nullptr);
    CHECK(client->GetCurLocation() == nullptr);
    CHECK(client->GetCurMap() == nullptr);

    const auto critter_pid = client->Hashes.ToHashedString("UnitTestClientCritter");
    const auto* critter_proto = client->GetProtoCritter(critter_pid);
    REQUIRE(critter_proto != nullptr);

    auto player = SafeAlloc::MakeRefCounted<PlayerView>(client.get(), ident_t {1001});
    auto critter = SafeAlloc::MakeRefCounted<CritterView>(client.get(), ident_t {1002}, critter_proto);

    REQUIRE(client->GetEntity(player->GetId()) == player.get());
    REQUIRE(client->GetEntity(critter->GetId()) == critter.get());
    CHECK(critter->GetProtoId() == critter_pid);
    CHECK(critter->GetName() == "UnitTestClientCritter_1002");

    critter->DestroySelf();
    player->DestroySelf();

    CHECK(client->GetEntity(ident_t {1002}) == nullptr);
    CHECK(client->GetEntity(ident_t {1001}) == nullptr);
}

TEST_CASE("ClientEngineScriptModuleInitAndLoopAreCallable")
{
    auto settings = MakeClientTestSettings();
    auto client = SafeAlloc::MakeRefCounted<ClientEngine>(settings, MakeClientTestResources(), &App->MainWindow);

    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });

    const auto get_func_name = [&client](string_view name) { return client->Hashes.ToHashedString(name); };

    int start_calls = 0;
    int loop_calls = 0;
    int manual_calls = 0;

    REQUIRE(client->CallFunc(get_func_name("ClientEngineTest::UnitTestGetStartCalls"), start_calls));
    REQUIRE(client->CallFunc(get_func_name("ClientEngineTest::UnitTestGetLoopCalls"), loop_calls));
    REQUIRE(client->CallFunc(get_func_name("ClientEngineTest::UnitTestGetManualCalls"), manual_calls));

    CHECK(start_calls == 1);
    CHECK(loop_calls == 0);
    CHECK(manual_calls == 0);

    REQUIRE(client->CallFunc(get_func_name("ClientEngineTest::UnitTestMarkManualCall")));
    REQUIRE(client->CallFunc(get_func_name("ClientEngineTest::UnitTestGetManualCalls"), manual_calls));
    CHECK(manual_calls == 1);

    client->MainLoop();
    client->MainLoop();

    REQUIRE(client->CallFunc(get_func_name("ClientEngineTest::UnitTestGetLoopCalls"), loop_calls));
    CHECK(loop_calls >= 2);
}

FO_END_NAMESPACE
