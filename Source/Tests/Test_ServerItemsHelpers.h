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

#pragma once

#include "Common.h"

#include "catch_amalgamated.hpp"

#include "AngelScriptScripting.h"
#include "Baker.h"
#include "DataSerialization.h"
#include "Server.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

namespace ServerItemsTests
{
    inline auto MakeSettings() -> GlobalSettings
    {
        auto settings = GlobalSettings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        BakerTests::ApplySelfContainedServerSettings(settings);

        return settings;
    }

#if FO_ANGELSCRIPT_SCRIPTING
    inline auto MakeScriptBinary(const FileSystem& metadata_resources) -> vector<uint8_t>
    {
        BakerServerEngine compiler_engine {metadata_resources};

        return BakerTests::CompileInlineScripts(&compiler_engine, "ServerItemsScripts",
            {
                {"Scripts/ServerItemsTest.fos", R"(
namespace ServerItemsTest
{
    int ItemInitCalls = 0;
    int64 LastItemId = 0;

    [[ModuleInit]]
    void InitServerItemsTest()
    {
        Game.OnInit.Subscribe(OnInit);
        Game.OnItemInit.Subscribe(OnItemInit);
    }

    [[Event]]
    void OnInit()
    {
    }

    [[Event]]
    void OnItemInit(Item item, bool firstTime)
    {
        ItemInitCalls++;
        LastItemId = item.Id.value;
    }

    int GetItemInitCalls()
    {
        return ItemInitCalls;
    }

    int64 GetLastItemId()
    {
        return LastItemId;
    }

    int64 GetCritterItemCount(Critter cr)
    {
        array<Item> items = cr.GetItems();
        return items.length();
    }

    bool TestCritterIsAlive(Critter cr)
    {
        return cr.IsAlive();
    }

    bool TestCritterIsDead(Critter cr)
    {
        return cr.IsDead();
    }

    string GetCritterName(Critter cr)
    {
        return cr.Name;
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
#endif

    inline auto MakeResources() -> FileSystem
    {
        auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();

        auto compiler_resources_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("ServerItemsCompilerResources");
        compiler_resources_source->AddFile("Metadata.fometa-server", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_resources_source));

        BakerServerEngine proto_engine {compiler_resources};
        hstring critter_type = proto_engine.Hashes.to_hashed_string("Critter");
        hstring item_type = proto_engine.Hashes.to_hashed_string("Item");
        auto critter_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, "TestCritter");
        auto item_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoItem>(proto_engine, item_type, "TestItem");
#if FO_ANGELSCRIPT_SCRIPTING
        auto script_blob = MakeScriptBinary(compiler_resources);
#endif

        auto runtime_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("ServerItemsRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-server", metadata_blob);
        runtime_source->AddFile("ServerItemsCritter.fopro-bin-server", critter_blob);
        runtime_source->AddFile("ServerItemsItem.fopro-bin-server", item_blob);
#if FO_ANGELSCRIPT_SCRIPTING
        runtime_source->AddFile("ServerItemsTest.fos-bin-server", script_blob);
#endif

        FileSystem resources;
        resources.AddCustomSource(std::move(runtime_source));

        return resources;
    }

    inline auto WaitForStart(ptr<ServerEngine> server) -> string
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

    inline auto CreateLoggedPlayer(ptr<ServerEngine> server, string_view name) -> ptr<Player>
    {
        shared_ptr<NetworkServerConnection> net_connection = NetworkServer::CreateDummyConnection(server->Settings, NetworkServer::DummyConnectionState::Connected);
        auto not_logged_in_player = server->CreateNotLoggedInPlayer(std::move(net_connection));

        not_logged_in_player->SetName(name);
        not_logged_in_player->SetLastControlledCritterId(ident_t {1});
        auto player = server->LoginPlayerToNewRecord(not_logged_in_player);

        return player;
    }

    inline auto MakeServerEngine(GlobalSettings& settings) -> refcount_ptr<ServerEngine>
    {
        return safe_alloc::make_refcounted<ServerEngine>(&settings, MakeResources());
    }
}

FO_END_NAMESPACE

