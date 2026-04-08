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
#include "Baker.h"
#include "DataSerialization.h"
#include "Movement.h"
#include "Server.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

namespace
{
    constexpr msize SERVER_TEST_MAP_SIZE {200, 200};
    constexpr mpos SERVER_TEST_MOVE_START_HEX {20, 20};
    constexpr uint16 SERVER_TEST_MOVE_SPEED = 100;
    const vector<uint8> SERVER_TEST_MOVE_STEPS {0, 0, 1, 1, 2};
    const vector<uint16> SERVER_TEST_MOVE_CONTROL_STEPS {2, 4, 5};

    static auto MakeServerTestSettings() -> GlobalSettings
    {
        auto settings = GlobalSettings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        BakerTests::ApplySelfContainedServerSettings(settings);

        return settings;
    }

    static auto MakeEmptyMapBlob() -> vector<uint8>
    {
        vector<uint8> map_data;
        auto writer = DataWriter(map_data);
        writer.Write<uint32>(uint32 {0});
        writer.Write<uint32>(uint32 {0});
        writer.Write<uint32>(uint32 {0});
        return map_data;
    }

    static auto MakeMapProtoBlob(BakerServerEngine& proto_engine, hstring type_name, string_view proto_name, msize map_size) -> vector<uint8>
    {
        vector<uint8> props_data;
        set<hstring> str_hashes;

        ProtoMap proto {proto_engine.Hashes.ToHashedString(proto_name), proto_engine.GetPropertyRegistrator(type_name)};
        proto.SetSize(map_size);
        proto.GetProperties().StoreAllData(props_data, str_hashes);

        vector<uint8> protos_data;
        auto writer = DataWriter(protos_data);

        writer.Write<uint32>(uint32 {0});
        ignore_unused(str_hashes);
        writer.Write<uint32>(uint32 {1});
        writer.Write<uint32>(uint32 {1});
        writer.Write<uint16>(numeric_cast<uint16>(type_name.as_str().length()));
        writer.WritePtr(type_name.as_str().data(), type_name.as_str().length());
        writer.Write<uint16>(numeric_cast<uint16>(proto_name.length()));
        writer.WritePtr(proto_name.data(), proto_name.length());
        writer.Write<uint32>(numeric_cast<uint32>(props_data.size()));
        writer.WritePtr(props_data.data(), props_data.size());

        return protos_data;
    }

    static auto MakeScriptBinary(const FileSystem& metadata_resources) -> vector<uint8>
    {
        BakerServerEngine compiler_engine {metadata_resources};

        return BakerTests::CompileInlineScripts(&compiler_engine, "ServerEngineScripts",
            {
                {"Scripts/ServerEngineTest.fos", R"(
namespace ServerEngineTest
{
    int InitCalls = 0;
    int CritterInitCalls = 0;
    int64 LastCritterId = 0;
    bool LastCritterFirstTime = false;
    int ManualCalls = 0;

    void ModuleInit()
    {
        Game.OnInit.Subscribe(OnInit);
        Game.OnCritterInit.Subscribe(OnCritterInit);
    }

    bool OnInit()
    {
        InitCalls++;
        return true;
    }

    void OnCritterInit(Critter cr, bool firstTime)
    {
        CritterInitCalls++;
        LastCritterId = cr.Id.value;
        LastCritterFirstTime = firstTime;
    }

    void UnitTestNoop() {}

    void UnitTestMarkManualCall()
    {
        ManualCalls++;
    }

    int UnitTestGetInitCalls()
    {
        return InitCalls;
    }

    int UnitTestGetCritterInitCalls()
    {
        return CritterInitCalls;
    }

    int64 UnitTestGetLastCritterId()
    {
        return LastCritterId;
    }

    bool UnitTestGetLastCritterFirstTime()
    {
        return LastCritterFirstTime;
    }

    int UnitTestGetManualCalls()
    {
        return ManualCalls;
    }

    int UnitTestSumArray(int[] values)
    {
        int total = 0;

        for (int i = 0; i < values.length(); i++) {
            total += values[i];
        }

        return total;
    }

    void UnitTestMutateArray(int[] & values)
    {
        if (values.length() > 0) {
            values[0] += 10;
        }

        values.insertLast(77);
    }

    int UnitTestSumDict(dict<int, int> values)
    {
        int total = 0;

        for (int i = 0; i < values.length(); i++) {
            total += values.getKey(i);
            total += values.getValue(i);
        }

        return total;
    }

    int64 UnitTestGetCritterIdValue(Critter cr)
    {
        return cr !is null ? cr.Id.value : 0;
    }

    bool UnitTestMatchesHash(hstring value)
    {
        return value == "UnitTestHash".hstr();
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

    static auto MakeServerTestResources() -> FileSystem
    {
        const auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();

        auto compiler_resources_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ServerEngineCompilerResources");
        compiler_resources_source->AddFile("Metadata.fometa-server", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_resources_source));

        BakerServerEngine proto_engine {compiler_resources};
        const auto critter_type = proto_engine.Hashes.ToHashedString("Critter");
        const auto location_type = proto_engine.Hashes.ToHashedString("Location");
        const auto map_type = proto_engine.Hashes.ToHashedString("Map");
        const auto proto_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, critter_type, "UnitTestRat");
        const auto location_blob = BakerTests::MakeSingleProtoResourceBlob<ProtoLocation>(proto_engine, location_type, "UnitTestLocation");
        const auto map_blob = MakeMapProtoBlob(proto_engine, map_type, "UnitTestMap", SERVER_TEST_MAP_SIZE);
        const auto fomap_blob = MakeEmptyMapBlob();
        const auto script_blob = MakeScriptBinary(compiler_resources);

        auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ServerEngineRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-server", metadata_blob);
        runtime_source->AddFile("ServerEngineTest.fopro-bin-server", proto_blob);
        runtime_source->AddFile("UnitTestLocation.fopro-bin-server", location_blob);
        runtime_source->AddFile("UnitTestMap.fopro-bin-server", map_blob);
        runtime_source->AddFile("UnitTestMap.fomap-bin-server", fomap_blob);
        runtime_source->AddFile("ServerEngineTest.fos-bin-server", script_blob);

        FileSystem resources;
        resources.AddCustomSource(std::move(runtime_source));

        return resources;
    }

    static auto WaitForServerStart(ServerEngine* server) -> string
    {
        FO_RUNTIME_ASSERT(server);

        for (int32 i = 0; i < 6000; i++) {
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

    static auto MakeServerMovementContext(msize map_size, mpos start_hex, nanotime start_time) -> refcount_ptr<MovingContext>
    {
        return SafeAlloc::MakeRefCounted<MovingContext>(map_size, SERVER_TEST_MOVE_SPEED, SERVER_TEST_MOVE_STEPS, SERVER_TEST_MOVE_CONTROL_STEPS, start_time, timespan {}, start_hex, ipos16 {}, ipos16 {});
    }

    static auto WaitForUnlockedServerCondition(ServerEngine* server, bool& locked, const function<bool()>& condition, std::chrono::milliseconds timeout = std::chrono::milliseconds {1000}) -> bool
    {
        FO_RUNTIME_ASSERT(server);
        FO_RUNTIME_ASSERT(locked);

        server->Unlock();
        locked = false;

        const auto deadline = std::chrono::steady_clock::now() + timeout;

        while (std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds {5});

            if (!server->Lock(timespan {std::chrono::seconds {10}})) {
                continue;
            }

            locked = true;

            if (condition()) {
                return true;
            }

            server->Unlock();
            locked = false;
        }

        if (!locked) {
            if (!server->Lock(timespan {std::chrono::seconds {10}})) {
                return false;
            }

            locked = true;
        }

        return condition();
    }
}

TEST_CASE("ServerEngineStartsAndCreatesCritter")
{
    auto settings = MakeServerTestSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, MakeServerTestResources());

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForServerStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    const auto critter_pid = server->Hashes.ToHashedString("UnitTestRat");
    REQUIRE(server->GetProtoCritter(critter_pid) != nullptr);

    const auto critter_count = server->EntityMngr.GetCrittersCount();
    const auto entity_count = server->EntityMngr.GetEntitiesCount();

    auto* cr = server->CreateCritter(critter_pid, false);
    REQUIRE(cr != nullptr);

    const auto cr_id = cr->GetId();
    CHECK(cr->GetProtoId() == critter_pid);
    CHECK(server->EntityMngr.GetCritter(cr_id) == cr);
    CHECK(server->EntityMngr.GetCrittersCount() == critter_count + 1);
    CHECK(server->EntityMngr.GetEntitiesCount() > entity_count);

    const auto player_id = server->MakePlayerId("UnitTestPlayer");
    CHECK(player_id != ident_t {});
    CHECK(player_id == server->MakePlayerId("UnitTestPlayer"));

    server->CrMngr.DestroyCritter(cr);

    CHECK(server->EntityMngr.GetCritter(cr_id) == nullptr);
    CHECK(server->EntityMngr.GetCrittersCount() == critter_count);
}

TEST_CASE("ServerEngineHandlesPlayerCritterUnloadAndMissingProto")
{
    auto settings = MakeServerTestSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, MakeServerTestResources());

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForServerStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    SECTION("PlayerControlledCritterCanBeUnloaded")
    {
        const auto critter_pid = server->Hashes.ToHashedString("UnitTestRat");
        const auto critter_count = server->EntityMngr.GetCrittersCount();

        auto* cr = server->CreateCritter(critter_pid, true);
        REQUIRE(cr != nullptr);

        const auto cr_id = cr->GetId();
        CHECK(cr->GetControlledByPlayer());
        CHECK(server->EntityMngr.GetCritter(cr_id) == cr);

        server->UnloadCritter(cr);

        CHECK(server->EntityMngr.GetCritter(cr_id) == nullptr);
        CHECK(server->EntityMngr.GetCrittersCount() == critter_count);
    }

    SECTION("MissingProtoThrows")
    {
        const auto missing_pid = server->Hashes.ToHashedString("MissingUnitTestCritter");

        CHECK_THROWS(server->CreateCritter(missing_pid, false));
        CHECK_THROWS(server->CreateCritter(missing_pid, true));
    }
}

TEST_CASE("ServerEngineScriptModuleInitAndEventsAreCallable")
{
    auto settings = MakeServerTestSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, MakeServerTestResources());

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForServerStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    const auto get_func_name = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    int init_calls = 0;
    REQUIRE(server->CallFunc(get_func_name("ServerEngineTest::UnitTestGetInitCalls"), init_calls));
    CHECK(init_calls == 1);

    int manual_calls = 0;
    REQUIRE(server->CallFunc(get_func_name("ServerEngineTest::UnitTestGetManualCalls"), manual_calls));
    CHECK(manual_calls == 0);

    REQUIRE(server->CallFunc(get_func_name("ServerEngineTest::UnitTestMarkManualCall")));
    REQUIRE(server->CallFunc(get_func_name("ServerEngineTest::UnitTestGetManualCalls"), manual_calls));
    CHECK(manual_calls == 1);

    const auto critter_pid = server->Hashes.ToHashedString("UnitTestRat");
    auto* cr = server->CreateCritter(critter_pid, false);
    REQUIRE(cr != nullptr);

    int critter_init_calls = 0;
    int64 last_critter_id = 0;
    bool last_first_time = false;

    REQUIRE(server->CallFunc(get_func_name("ServerEngineTest::UnitTestGetCritterInitCalls"), critter_init_calls));
    REQUIRE(server->CallFunc(get_func_name("ServerEngineTest::UnitTestGetLastCritterId"), last_critter_id));
    REQUIRE(server->CallFunc(get_func_name("ServerEngineTest::UnitTestGetLastCritterFirstTime"), last_first_time));

    CHECK(critter_init_calls >= 1);
    CHECK(last_critter_id == cr->GetId().underlying_value());
    CHECK(last_first_time);

    server->CrMngr.DestroyCritter(cr);
}

TEST_CASE("ServerEngineScriptCallsMarshalContainersAndEntities")
{
    auto settings = MakeServerTestSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, MakeServerTestResources());

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForServerStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });

    const auto get_func_name = [&server](string_view name) { return server->Hashes.ToHashedString(name); };

    vector<int32> values {1, 2, 3};
    auto sum_array_func = server->FindFunc<int32, vector<int32>>(get_func_name("ServerEngineTest::UnitTestSumArray"));
    REQUIRE(sum_array_func);
    REQUIRE(sum_array_func.Call(values));
    CHECK(sum_array_func.GetResult() == 6);

    auto mutate_array_func = server->FindFunc<void, vector<int32>&>(get_func_name("ServerEngineTest::UnitTestMutateArray"));
    REQUIRE(mutate_array_func);
    REQUIRE(mutate_array_func.Call(values));
    CHECK(values == vector<int32> {11, 2, 3, 77});

    const auto critter_pid = server->Hashes.ToHashedString("UnitTestRat");
    auto* cr = server->CreateCritter(critter_pid, false);
    REQUIRE(cr != nullptr);

    auto critter_id_func = server->FindFunc<int64, Critter*>(get_func_name("ServerEngineTest::UnitTestGetCritterIdValue"));
    REQUIRE(critter_id_func);
    REQUIRE(critter_id_func.Call(cr));
    CHECK(critter_id_func.GetResult() == cr->GetId().underlying_value());

    auto matches_hash_func = server->FindFunc<bool, hstring>(get_func_name("ServerEngineTest::UnitTestMatchesHash"));
    REQUIRE(matches_hash_func);
    REQUIRE(matches_hash_func.Call(server->Hashes.ToHashedString("UnitTestHash")));
    CHECK(matches_hash_func.GetResult());

    server->CrMngr.DestroyCritter(cr);
}

TEST_CASE("ServerEngineProcessesOverdueMovementByHex")
{
    auto settings = MakeServerTestSettings();
    auto server = SafeAlloc::MakeRefCounted<ServerEngine>(settings, MakeServerTestResources());

    auto shutdown = scope_exit([&server]() noexcept {
        safe_call([&server] {
            if (server->IsStarted()) {
                server->Shutdown();
            }
        });
    });

    const auto startup_error = WaitForServerStart(server.get());
    INFO(startup_error);
    REQUIRE(startup_error.empty());

    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    bool locked = true;
    auto unlock = scope_exit([&server, &locked]() noexcept {
        safe_call([&server, &locked] {
            if (locked) {
                server->Unlock();
            }
        });
    });

    const auto critter_pid = server->Hashes.ToHashedString("UnitTestRat");
    const auto location_pid = server->Hashes.ToHashedString("UnitTestLocation");
    const auto map_pid = server->Hashes.ToHashedString("UnitTestMap");

    SECTION("CompletesWholeRoute")
    {
        vector<hstring> map_pids {map_pid};
        auto* loc = server->MapMngr.CreateLocation(location_pid, map_pids);
        REQUIRE(loc != nullptr);

        auto destroy_loc = scope_exit([&server, &loc]() noexcept {
            safe_call([&server, &loc] {
                if (loc != nullptr && !loc->IsDestroyed()) {
                    server->MapMngr.DestroyLocation(loc);
                }
            });
        });

        auto* map = loc->GetMapByIndex(0);
        REQUIRE(map != nullptr);

        auto* cr = server->CreateCritter(critter_pid, false);
        REQUIRE(cr != nullptr);

        server->MapMngr.TransferToMap(cr, map, SERVER_TEST_MOVE_START_HEX, 0, std::nullopt);

        const auto template_moving = MakeServerMovementContext(map->GetSize(), cr->GetHex(), server->GameTime.GetFrameTime());
        const auto path_hexes = template_moving->EvaluatePathHexes(cr->GetHex());
        REQUIRE(path_hexes.size() == SERVER_TEST_MOVE_STEPS.size() + 1);

        const auto overdue_time = timespan {std::chrono::milliseconds {iround<int32>(template_moving->GetWholeTime()) + 100}};
        auto moving = MakeServerMovementContext(map->GetSize(), cr->GetHex(), server->GameTime.GetFrameTime() - overdue_time);

        server->StartCritterMoving(cr, moving, nullptr);
        REQUIRE(cr->IsMoving());

        REQUIRE(WaitForUnlockedServerCondition(server.get(), locked, [&cr] { return !cr->IsMoving(); }));

        CHECK_FALSE(cr->IsMoving());
        CHECK(cr->GetMovingState() == MovingState::Success);
        CHECK(cr->GetHex() == path_hexes.back());

        const auto* finished_moving = cr->GetMovingContext();
        REQUIRE(finished_moving != nullptr);
        CHECK(finished_moving->GetCompleteReason() == MovingState::Success);
    }

    SECTION("StopsAtFirstBlockedHexWithoutTeleport")
    {
        vector<hstring> map_pids {map_pid};
        auto* loc = server->MapMngr.CreateLocation(location_pid, map_pids);
        REQUIRE(loc != nullptr);

        auto destroy_loc = scope_exit([&server, &loc]() noexcept {
            safe_call([&server, &loc] {
                if (loc != nullptr && !loc->IsDestroyed()) {
                    server->MapMngr.DestroyLocation(loc);
                }
            });
        });

        auto* map = loc->GetMapByIndex(0);
        REQUIRE(map != nullptr);

        auto* cr = server->CreateCritter(critter_pid, false);
        REQUIRE(cr != nullptr);

        server->MapMngr.TransferToMap(cr, map, SERVER_TEST_MOVE_START_HEX, 0, std::nullopt);

        const auto template_moving = MakeServerMovementContext(map->GetSize(), cr->GetHex(), server->GameTime.GetFrameTime());
        const auto path_hexes = template_moving->EvaluatePathHexes(cr->GetHex());
        REQUIRE(path_hexes.size() == SERVER_TEST_MOVE_STEPS.size() + 1);

        const auto expected_stop_hex = path_hexes[1];
        const auto blocked_hex = path_hexes[2];

        map->SetHexManualBlock(blocked_hex, true, true);
        auto unblock_hex = scope_exit([map, blocked_hex]() noexcept {
            safe_call([map, blocked_hex] {
                if (!map->IsDestroyed()) {
                    map->SetHexManualBlock(blocked_hex, false, false);
                }
            });
        });

        const auto overdue_time = timespan {std::chrono::milliseconds {iround<int32>(template_moving->GetWholeTime()) + 100}};
        auto moving = MakeServerMovementContext(map->GetSize(), cr->GetHex(), server->GameTime.GetFrameTime() - overdue_time);

        server->StartCritterMoving(cr, moving, nullptr);
        REQUIRE(cr->IsMoving());

        REQUIRE(WaitForUnlockedServerCondition(server.get(), locked, [&cr] { return !cr->IsMoving(); }));

        CHECK_FALSE(cr->IsMoving());
        CHECK(cr->GetMovingState() == MovingState::HexBusy);
        CHECK(cr->GetHex() == expected_stop_hex);

        const auto* finished_moving = cr->GetMovingContext();
        REQUIRE(finished_moving != nullptr);
        CHECK(finished_moving->GetCompleteReason() == MovingState::HexBusy);
        CHECK(finished_moving->GetPreBlockHex() == expected_stop_hex);
        CHECK(finished_moving->GetBlockHex() == blocked_hex);
    }
}

FO_END_NAMESPACE
