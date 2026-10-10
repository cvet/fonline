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

#include "MapLoader.h"
#include "Server.h"
#include "StaticMap.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

static auto MakeServerEntityLifetimeSettings() -> GlobalSettings
{
    GlobalSettings settings(false);
    settings.ApplyDefaultSettings();
    settings.ApplyAutoSettings();
    BakerTests::ApplySelfContainedServerSettings(settings);
    BakerTests::OverrideSetting(settings.Server.DbStorage, string {"Memory"});
    BakerTests::OverrideSetting(settings.Server.WriteHealthFile, false);
    BakerTests::OverrideSetting(settings.Server.WorkerThreads, int32_t {1});
    BakerTests::OverrideSetting(settings.Common.Packaged, false);
    BakerTests::OverrideSetting(settings.Baking.BakeOutput, string {});
    return settings;
}

static auto MakeServerEntityLifetimeResources(bool load_map = false, optional<msize> requested_size = std::nullopt) -> FileSystem
{
    vector<uint8_t> metadata = BakerTests::MakeEmptyMetadataBlob();
    auto compiler_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("ServerEntityLifetimeCompiler");
    compiler_source->AddFile("Metadata.fometa-server", metadata);

    FileSystem compiler_resources;
    compiler_resources.AddCustomSource(std::move(compiler_source));
    BakerServerEngine proto_engine {compiler_resources};

    auto source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("ServerEntityLifetimeRuntime");
    source->AddFile("Metadata.fometa-server", metadata);
#if FO_ANGELSCRIPT_SCRIPTING
    source->AddFile("ServerEntityLifetime.fos-bin-server", BakerTests::CompileInlineScripts(&proto_engine, "ServerEntityLifetimeScripts", {{"Scripts/ServerEntityLifetime.fos", "void LifetimeFixtureEntry() {}"}}, [](string_view message) { FAIL(message); }));
#endif
    source->AddFile("LifetimeCritter.fopro-bin-server", BakerTests::MakeSingleProtoResourceBlob<ProtoCritter>(proto_engine, proto_engine.Hashes.to_hashed_string("Critter"), "LifetimeCritter"));
    source->AddFile("LifetimeItem.fopro-bin-server", BakerTests::MakeSingleProtoResourceBlob<ProtoItem>(proto_engine, proto_engine.Hashes.to_hashed_string("Item"), "LifetimeItem"));
    source->AddFile("LifetimeLocation.fopro-bin-server", BakerTests::MakeSingleProtoResourceBlob<ProtoLocation>(proto_engine, proto_engine.Hashes.to_hashed_string("Location"), "LifetimeLocation"));
    vector<pair<string, function<void(ProtoMap&)>>> map_protos;
    constexpr int32_t side = const_numeric_cast<int32_t>(GameSettings::SERVER_MAP_CHUNK_SIDE);
    msize map_size = requested_size.value_or(load_map ? msize {side * 3 + 3, side * 3 + 3} : msize {2, 2});
    map_protos.emplace_back("LifetimeMap", [map_size](ProtoMap& proto) { proto.SetSize(map_size); });
    source->AddFile("LifetimeMap.fopro-bin-server", BakerTests::MakeMultiProtoResourceBlob<ProtoMap>(proto_engine, proto_engine.Hashes.to_hashed_string("Map"), map_protos));

    if (load_map) {
        vector<uint8_t> map_data;
        auto writer = data_writer(map_data);
        writer.write<uint32_t>(BAKED_MAP_FILE_MAGIC);
        writer.write<uint32_t>(BAKED_MAP_FILE_VERSION);
        writer.write<uint32_t>(uint32_t {0}); // Hashes
        writer.write<uint32_t>(uint32_t {0}); // Critters
        writer.write<uint32_t>(uint32_t {0}); // Items
        source->AddFile("LifetimeMap.fomap-bin-server", map_data);
    }

    FileSystem resources;
    resources.AddCustomSource(std::move(source));
    return resources;
}

static auto WaitForServerEntityLifetimeStartup(ptr<ServerEngine> server) -> bool
{
    nanotime deadline = nanotime::now() + std::chrono::seconds {30};

    while (nanotime::now() < deadline) {
        if (server->IsStartingError()) {
            return false;
        }
        if (server->IsStarted()) {
            return true;
        }

        coarse_sleep(std::chrono::milliseconds {10});
    }

    return false;
}

static auto MakeServerEntityLifetimeOwners(ptr<ServerEngine> server, ptr<StaticMap> static_map) -> vector<refcount_ptr<ServerEntity>>
{
    // The caller quiesces the server after complete startup; registrar setup and hash interning cannot race it
    auto critter_proto = server->GetProtoCritter(server->Hashes.to_hashed_string("LifetimeCritter"));
    auto item_proto = server->GetProtoItem(server->Hashes.to_hashed_string("LifetimeItem"));
    auto map_proto = server->GetProtoMap(server->Hashes.to_hashed_string("LifetimeMap"));
    auto location_proto = server->GetProtoLocation(server->Hashes.to_hashed_string("LifetimeLocation"));
    REQUIRE(critter_proto);
    REQUIRE(item_proto);
    REQUIRE(map_proto);
    REQUIRE(location_proto);

    vector<refcount_ptr<ServerEntity>> entities;
    entities.emplace_back(safe_alloc::make_refcounted<Critter>(server, ident_t {1}, critter_proto));
    entities.emplace_back(safe_alloc::make_refcounted<Item>(server, ident_t {2}, item_proto));
    entities.emplace_back(safe_alloc::make_refcounted<Map>(server, ident_t {3}, map_proto, nullptr, static_map));
    entities.emplace_back(safe_alloc::make_refcounted<Location>(server, ident_t {4}, location_proto));

    auto network = NetworkServer::CreateDummyConnection(server->Settings);
    auto connection = safe_alloc::make_unique<ServerConnection>(server->Settings, std::move(network), BakerTests::MakeTestChannelIdentity());
    entities.emplace_back(safe_alloc::make_refcounted<Player>(server, ident_t {5}, std::move(connection)));
    return entities;
}

TEST_CASE("ServerMapGridSelectionPreservesFieldBehavior", "[server][map][grid]")
{
    string proto_grid = GENERATE(string {"Static"}, string {"Chunked"}, string {"Dynamic"});
    string instance_grid = GENERATE(string {"Static"}, string {"Chunked"}, string {"Dynamic"});
    CAPTURE(proto_grid, instance_grid);

    GlobalSettings settings = MakeServerEntityLifetimeSettings();
    BakerTests::OverrideSetting(settings.Server.ProtoMapGridType, proto_grid);
    BakerTests::OverrideSetting(settings.Server.MapInstanceGridType, instance_grid);
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeServerEntityLifetimeResources(true));
    auto shutdown = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });
    REQUIRE(WaitForServerEntityLifetimeStartup(server));

    bool completed = server->RunInQuiescence(std::chrono::seconds {10}, [&](const ServerQuiescenceState&) {
        hstring map_pid = server->Hashes.to_hashed_string("LifetimeMap");
        auto loc = server->MapMngr.CreateLocation(server->Hashes.to_hashed_string("LifetimeLocation"), vector<hstring> {map_pid, map_pid});
        auto map = loc->GetMapByIndex(0);
        auto other_map = loc->GetMapByIndex(1);
        REQUIRE(map);
        REQUIRE(other_map);
        auto static_map = server->MapMngr.GetStaticMap(map->GetProtoMap());
        CHECK(map->GetStaticMap() == other_map->GetStaticMap());
        constexpr int32_t side = const_numeric_cast<int32_t>(GameSettings::SERVER_MAP_CHUNK_SIDE);
        mpos anchor {side - 1, side - 1};
        static_map->MarkScrollBlocked(anchor);
        ptr<const StaticMap::Field> anchor_field = &static_map->GetField(anchor);

        if constexpr (side > 1) {
            ptr<const StaticMap::Field> nearby_empty = &static_map->GetField({side - 2, side - 1});
            ptr<const StaticMap::Field> distant_empty = &static_map->GetField({side * 2, side * 2});
            CHECK((nearby_empty != distant_empty) == (proto_grid == "Chunked"));
        }

        for (mpos hex : {mpos {side, side - 1}, mpos {side - 1, side}, mpos {side, side}}) {
            static_map->MarkScrollBlocked(hex);
            CHECK_FALSE(map->IsHexMovable(hex));
            CHECK_FALSE(other_map->IsHexMovable(hex));
            CHECK(map->IsHexShootable(hex));
            CHECK(make_ptr(&static_map->GetField(anchor)) == anchor_field);
        }

        for (mpos hex : {mpos {side - 1, side + 2}, mpos {side + 2, side - 1}, mpos {side + 2, side + 2}, mpos {side * 2 + 2, side * 2 + 2}}) {
            CHECK(map->IsHexMovable(hex));
            map->SetHexManualBlock(hex, true, true);
            CHECK_FALSE(map->IsHexMovable(hex));
            CHECK_FALSE(map->IsHexShootable(hex));
            CHECK(other_map->IsHexMovable(hex));
            CHECK(other_map->IsHexShootable(hex));
            map->SetHexManualBlock(hex, false, true);
            CHECK(map->IsHexMovable(hex));
            CHECK(map->IsHexShootable(hex));
        }

        mpos entity_hex {side * 2 + 1, side * 2 + 1};
        auto item = server->ItemMngr.CreateItemOnHex(map, entity_hex, server->Hashes.to_hashed_string("LifetimeItem"), nullptr);
        CHECK(map->GetItem(item->GetId()) == item);
        CHECK(map->GetItemsOnHex(entity_hex).size() == 1);
        CHECK_FALSE(other_map->GetItem(item->GetId()));
        item->SetNoBlock(false);
        item->SetShootThru(false);
        CHECK_FALSE(map->IsHexMovable(entity_hex));
        CHECK_FALSE(map->IsHexShootable(entity_hex));
        CHECK(other_map->IsHexMovable(entity_hex));
        CHECK(other_map->IsHexShootable(entity_hex));
        mpos critter_hex {side * 2 + 2, side * 2 + 1};
        auto cr = server->CrMngr.CreateCritterOnMap(server->Hashes.to_hashed_string("LifetimeCritter"), nullptr, map, critter_hex, mdir {0});
        CHECK(map->GetCritterOnHex(critter_hex, CritterFindType::Any) == cr);
        CHECK_FALSE(other_map->GetCritterOnHex(critter_hex, CritterFindType::Any));
        server->MapMngr.DestroyLocation(loc);
    });
    REQUIRE(completed);
}

TEST_CASE("ServerMapGridTypesRejectInvalidSettings", "[server][map][grid]")
{
    bool prototype_setting = GENERATE(false, true);
    string invalid_type = GENERATE(string {}, string {"static"}, string {"chunked"}, string {"Hash"}, string {"Unknown"});
    CAPTURE(prototype_setting, invalid_type);
    GlobalSettings settings = MakeServerEntityLifetimeSettings();
    CHECK(settings.Server.ProtoMapGridType == "Dynamic");
    CHECK(settings.Server.MapInstanceGridType == "Dynamic");
    if (prototype_setting) {
        BakerTests::OverrideSetting(settings.Server.ProtoMapGridType, invalid_type);
    }
    else {
        BakerTests::OverrideSetting(settings.Server.MapInstanceGridType, invalid_type);
    }
    CHECK_THROWS_AS(StaticMap(msize {2, 2}, invalid_type), SettingsException);
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeServerEntityLifetimeResources());
    auto shutdown = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });
    REQUIRE_FALSE(WaitForServerEntityLifetimeStartup(server));
    CHECK(server->IsStartingError());
}

TEST_CASE("ServerMapInvalidGridConstructionUnwindsEntityOwner", "[server][map][grid][lifetime]")
{
    GlobalSettings settings = MakeServerEntityLifetimeSettings();
    StaticMap static_map {msize {2, 2}, "Dynamic"};
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeServerEntityLifetimeResources());
    auto shutdown = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });
    REQUIRE(WaitForServerEntityLifetimeStartup(server));
    REQUIRE(server->RunInQuiescence(std::chrono::seconds {10}, [&](const ServerQuiescenceState&) {
        auto proto = server->GetProtoMap(server->Hashes.to_hashed_string("LifetimeMap"));
        REQUIRE(proto);
        BakerTests::OverrideSetting(settings.Server.MapInstanceGridType, string {"Unknown"});
        CHECK_THROWS_AS(safe_alloc::make_refcounted<Map>(server, ident_t {}, proto, nullptr, &static_map), SettingsException);
        BakerTests::OverrideSetting(settings.Server.MapInstanceGridType, string {"Dynamic"});
        auto map = safe_alloc::make_refcounted<Map>(server, ident_t {}, proto, nullptr, &static_map);
        auto ctx = SyncContext::GetCurrentOnThisThread();
        REQUIRE(ctx);
        ctx->WidenEntities(vector<ptr<ServerEntity>> {map});
        CHECK(map->GetSize() == msize {2, 2});
    }));
}

static void MeasureServerMapGridMemory(string_view grid_type)
{
    constexpr int32_t side = 600;
    constexpr int32_t map_count = 16;
    constexpr int32_t items_per_map = 1024;
    constexpr int32_t critters_per_map = 256;
    msize map_size {side, side};
    GlobalSettings settings = MakeServerEntityLifetimeSettings();
    BakerTests::OverrideSetting(settings.Server.ProtoMapGridType, string {"Dynamic"});
    BakerTests::OverrideSetting(settings.Server.MapInstanceGridType, string {grid_type});
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeServerEntityLifetimeResources(true, map_size));
    bool shutdown_done = false;
    auto shutdown = scope_exit([&server, &shutdown_done]() noexcept {
        if (!shutdown_done) {
            safe_call([&server] { server->Shutdown(); });
        }
    });
    REQUIRE(WaitForServerEntityLifetimeStartup(server));
    if (platform::get_process_private_memory_usage() == 0) {
        SKIP("Process-private-memory measurement is unavailable on this platform");
    }

    auto measure = [&](string_view stage) {
        array<size_t, 5> private_samples {};
        array<size_t, 5> allocator_samples {};
        for (size_t index = 0; index < private_samples.size(); index++) {
            coarse_sleep(std::chrono::milliseconds {100});
            private_samples[index] = platform::get_process_private_memory_usage();
            allocator_samples[index] = memory::get_in_use_bytes();
        }
        std::sort(private_samples.begin(), private_samples.end());
        std::sort(allocator_samples.begin(), allocator_samples.end());
        WARN(strex("SERVER_GRID_MEMORY_CSV,{},{},{},{}", grid_type, stage, private_samples[2], allocator_samples[2]).str());
    };
    auto hex_for_index = [&](int32_t index) -> mpos {
        int32_t cell = numeric_cast<int32_t>(numeric_cast<int64_t>(index) * 104729 % (side * side));
        return map_size.from_raw_pos(cell % side, cell / side);
    };

    measure("baseline");
    REQUIRE(server->RunInQuiescence(std::chrono::seconds {120}, [&](const ServerQuiescenceState&) {
        hstring map_pid = server->Hashes.to_hashed_string("LifetimeMap");
        auto location = server->MapMngr.CreateLocation(server->Hashes.to_hashed_string("LifetimeLocation"), vector<hstring>(map_count, map_pid));
        vector<ptr<Map>> maps;
        for (int32_t index = 0; index < map_count; index++) {
            auto map = location->GetMapByIndex(index);
            REQUIRE(map);
            maps.emplace_back(map);
        }
        measure("empty_maps");

        for (auto map : maps) {
            for (int32_t index = 0; index < items_per_map; index++) {
                auto item = server->ItemMngr.CreateItemOnHex(map, hex_for_index(index), server->Hashes.to_hashed_string("LifetimeItem"), nullptr);
                item->SetNoBlock(true);
                item->SetShootThru(true);
            }
            for (int32_t index = 0; index < critters_per_map; index++) {
                (void)server->CrMngr.CreateCritterOnMap(server->Hashes.to_hashed_string("LifetimeCritter"), nullptr, map, hex_for_index(items_per_map + index), mdir {0});
            }
            CHECK(map->GetItems().size() == items_per_map);
            CHECK(map->GetCritters().size() == critters_per_map);
        }
        measure("populated_1280_fields");

        // Toggle then clear the same fields to measure retained storage after activity, with no extra live entities
        for (int32_t percent : {10, 90}) {
            int32_t touched = side * side * percent / 100;
            for (auto map : maps) {
                for (int32_t index = 0; index < touched; index++) {
                    mpos hex = hex_for_index(index);
                    map->SetHexManualBlock(hex, true, true);
                    map->SetHexManualBlock(hex, false, true);
                }
                CHECK(map->GetItems().size() == items_per_map);
                CHECK(map->GetCritters().size() == critters_per_map);
                CHECK(map->IsHexMovable(hex_for_index(touched - 1)));
            }
            measure(percent == 10 ? "touched_10_percent" : "touched_90_percent");
        }
        server->MapMngr.DestroyLocation(location);
    }));
    measure("after_destroy_and_cover_release");
    server->Shutdown();
    shutdown_done = true;
    measure("after_shutdown");
}

TEST_CASE("ServerMapGridMemoryDynamic", "[.][server][map][grid][memory]")
{
    MeasureServerMapGridMemory("Dynamic");
}

TEST_CASE("ServerMapGridMemoryChunked", "[.][server][map][grid][memory]")
{
    MeasureServerMapGridMemory("Chunked");
}

TEST_CASE("ServerMapGridOperationsCost", "[.][server][map][grid][benchmark]")
{
    constexpr int32_t side = 600;
    constexpr msize map_size {side, side};
    constexpr int32_t samples = 5;
    vector<uint8_t> blocked(numeric_cast<size_t>(side * side), 0);
    random_generator random {20261009};
    auto cell = [&](int32_t x, int32_t y) -> uint8_t& { return blocked[numeric_cast<size_t>(y * side + x)]; };
    auto outline = [&](int32_t x, int32_t y, int32_t width, int32_t height) {
        for (int32_t dx = 0; dx < width; dx++) {
            cell(x + dx, y) = 1;
            cell(x + dx, y + height - 1) = 1;
        }
        for (int32_t dy = 0; dy < height; dy++) {
            cell(x, y + dy) = 1;
            cell(x + width - 1, y + dy) = 1;
        }
    };
    for (int32_t i = 0; i < side * side / 33; i++) {
        cell(random.next_between(1, side - 2), random.next_between(1, side - 2)) = 1;
    }
    for (int32_t i = 0; i < 900; i++) {
        int32_t width = random.next_between(6, 18);
        int32_t height = random.next_between(6, 18);
        int32_t x = random.next_between(1, side - width - 1);
        int32_t y = random.next_between(1, side - height - 1);
        outline(x, y, width, height);
        int32_t door_x = random.next_between(x + 1, x + width - 3);
        cell(door_x, y) = 0;
        cell(door_x + 1, y) = 0;
    }
    vector<mpos> sealed_centers;
    for (int32_t i = 0; i < 16; i++) {
        int32_t x = 20 + (i % 4) * 150;
        int32_t y = 20 + (i / 4) * 150;
        for (int32_t dy = 0; dy < 11; dy++) {
            for (int32_t dx = 0; dx < 11; dx++) {
                cell(x + dx, y + dy) = 0;
            }
        }
        outline(x, y, 11, 11);
        sealed_centers.emplace_back(map_size.from_raw_pos(x + 5, y + 5));
    }
    auto pick_open = [&]() -> mpos {
        for (;;) {
            mpos hex = map_size.from_raw_pos(random.next_between(2, side - 3), random.next_between(2, side - 3));
            if (cell(hex.x, hex.y) == 0) {
                return hex;
            }
        }
    };
    auto pick_near = [&](mpos from, int32_t min_distance, int32_t max_distance) -> mpos {
        for (;;) {
            ipos32 raw {from.x + random.next_between(-max_distance, max_distance), from.y + random.next_between(-max_distance, max_distance)};
            if (map_size.is_valid_pos(raw)) {
                mpos hex = map_size.from_raw_pos(raw);
                int32_t distance = GeometryHelper::GetDistance(from, hex);
                if (distance >= min_distance && distance <= max_distance && cell(hex.x, hex.y) == 0) {
                    return hex;
                }
            }
        }
    };
    struct Route
    {
        mpos From;
        mpos To;
        vector<mpos> Targets {};
    };
    vector<vector<Route>> routes(5);
    for (size_t scenario = 0; scenario < 4; scenario++) {
        int32_t count = scenario == 2 ? 40 : 200;
        int32_t minimum = scenario == 1 ? 20 : scenario == 2 ? 100 : 3;
        int32_t maximum = scenario == 1 ? 60 : scenario == 2 ? 300 : 15;
        for (int32_t i = 0; i < count; i++) {
            mpos from = pick_open();
            mpos to = pick_near(from, minimum, maximum);
            Route route {.From = from, .To = to};
            for (int32_t direction = 0; direction < GameSettings::MAP_DIR_COUNT; direction++) {
                mpos target = to;
                if (GeometryHelper::MoveHexByDir(target, hdir {direction}, map_size)) {
                    route.Targets.emplace_back(target);
                }
            }
            routes[scenario].emplace_back(std::move(route));
        }
    }
    for (mpos center : sealed_centers) {
        routes[4].emplace_back(Route {.From = pick_near(center, 30, 80), .To = center});
    }
    vector<mpos> scattered;
    vector<mpos> clustered;
    vector<mpos> writable;
    vector<mpos> critter_hexes;
    vector<mpos> item_hexes;
    for (int32_t i = 0; i < 8192; i++) {
        scattered.emplace_back(map_size.from_raw_pos(random.next_between(0, side - 1), random.next_between(0, side - 1)));
        clustered.emplace_back(map_size.from_raw_pos(250 + i % 96, 250 + (i / 96) % 64));
        if (i < 1024) {
            writable.emplace_back(pick_open());
            item_hexes.emplace_back(pick_open());
        }
        if (i < 256) {
            critter_hexes.emplace_back(pick_open());
        }
    }
    auto mix = [](uint64_t hash, uint64_t value) -> uint64_t { return (hash ^ value) * 1099511628211ull; };
    auto hash_hex = [&](uint64_t hash, mpos hex) -> uint64_t { return mix(hash, numeric_cast<uint64_t>(hex.y * side + hex.x)); };
    auto hash_path = [&](uint64_t hash, const FindPathOutput& output) -> uint64_t {
        hash = mix(hash, numeric_cast<uint64_t>(static_cast<uint8_t>(output.Result)));
        hash = hash_hex(hash, output.NewToHex);
        hash = mix(hash, numeric_cast<uint64_t>(static_cast<uint16_t>(output.EndHexOffset.x)));
        hash = mix(hash, numeric_cast<uint64_t>(static_cast<uint16_t>(output.EndHexOffset.y)));
        for (mdir step : output.Steps) {
            hash = mix(hash, numeric_cast<uint64_t>(step.angle()));
        }
        for (uint16_t step : output.ControlSteps) {
            hash = mix(hash, step);
        }
        return hash;
    };
    vector<int32_t> iterations;
    vector<uint64_t> reference_hashes;
    vector<string> report;
    const std::array<string_view, 3> modes {"dense", "chunked", "hash"};
    const array<string, 3> grid_types {"Static", "Chunked", "Dynamic"};
    for (int32_t sample = 0; sample < samples; sample++) {
        for (int32_t order = 0; order < 9; order++) {
            // Rotate and reverse the matrix so host load and cache history cannot always favour one mode
            int32_t combination = (sample * 4 + (sample % 2 == 0 ? order : 8 - order)) % 9;
            int32_t proto_mode = combination / 3;
            int32_t instance_mode = combination % 3;
            CAPTURE(sample, proto_mode, instance_mode);
            GlobalSettings settings = MakeServerEntityLifetimeSettings();
            BakerTests::OverrideSetting(settings.Server.ProtoMapGridType, grid_types[numeric_cast<size_t>(proto_mode)]);
            BakerTests::OverrideSetting(settings.Server.MapInstanceGridType, grid_types[numeric_cast<size_t>(instance_mode)]);
            BakerTests::OverrideSetting(settings.Geometry.MapFreeMovement, true);
            BakerTests::OverrideSetting(settings.Geometry.MaxPathFindLength, int32_t {500});
            auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeServerEntityLifetimeResources(true, map_size));
            auto shutdown = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });
            REQUIRE(WaitForServerEntityLifetimeStartup(server));
            REQUIRE(server->RunInQuiescence(std::chrono::seconds {30}, [&](const ServerQuiescenceState&) {
                auto location = server->MapMngr.CreateLocation(server->Hashes.to_hashed_string("LifetimeLocation"), vector<hstring> {server->Hashes.to_hashed_string("LifetimeMap")});
                auto map = location->GetMapByIndex(0);
                REQUIRE(map);
                auto static_map = server->MapMngr.GetStaticMap(map->GetProtoMap());
                size_t wall_count = 0;
                for (int32_t y = 0; y < side; y++) {
                    for (int32_t x = 0; x < side; x++) {
                        if (cell(x, y) != 0) {
                            mpos hex = map_size.from_raw_pos(x, y);
                            static_map->MarkScrollBlocked(hex);
                            if (wall_count++ % 32 == 0) {
                                map->SetHexManualBlock(hex, true, true);
                            }
                        }
                    }
                }
                for (mpos hex : item_hexes) {
                    auto item = server->ItemMngr.CreateItemOnHex(map, hex, server->Hashes.to_hashed_string("LifetimeItem"), nullptr);
                    item->SetNoBlock(true);
                    item->SetShootThru(true);
                }
                for (mpos hex : critter_hexes) {
                    (void)server->CrMngr.CreateCritterOnMap(server->Hashes.to_hashed_string("LifetimeCritter"), nullptr, map, hex, mdir {0});
                }
                REQUIRE(map->GetItems().size() == item_hexes.size());
                REQUIRE(map->GetCritters().size() == critter_hexes.size());
                vector<Route> reachable_routes;
                for (size_t index = 0; index < 16; index++) {
                    const Route& route = routes[0][index];
                    Route reachable_route {.From = route.From, .To = route.To};
                    for (mpos target : route.Targets) {
                        auto path = server->MapMngr.FindPath(map, nullptr, route.From, target, 0, 0);
                        if (path.Result == FindPathOutput::ResultType::Ok || path.Result == FindPathOutput::ResultType::AlreadyHere) {
                            reachable_route.Targets.emplace_back(target);
                        }
                    }
                    if (!reachable_route.Targets.empty()) {
                        reachable_routes.emplace_back(std::move(reachable_route));
                    }
                }
                REQUIRE_FALSE(reachable_routes.empty());
                if (sample == 0 && order == 0) {
                    WARN(strex("Server grid fixture: {} walls, {} items, {} critters, {} reachable requests", wall_count, map->GetItems().size(), map->GetCritters().size(), reachable_routes.size()).str());
                    for (size_t index = 0; index < routes.size(); index++) {
                        size_t ok = 0;
                        size_t no_way = 0;
                        size_t busy = 0;
                        size_t too_far = 0;
                        for (const Route& route : routes[index]) {
                            auto path = server->MapMngr.FindPath(map, nullptr, route.From, route.To, index == 3 ? 1 : 0, 0);
                            ok += path.Result == FindPathOutput::ResultType::Ok ? 1 : 0;
                            no_way += path.Result == FindPathOutput::ResultType::NoWay ? 1 : 0;
                            busy += path.Result == FindPathOutput::ResultType::HexBusy ? 1 : 0;
                            too_far += path.Result == FindPathOutput::ResultType::TooFar ? 1 : 0;
                        }
                        WARN(strex("Server grid paths {}: {} requests, {} ok, {} no way, {} busy, {} too far", index, routes[index].size(), ok, no_way, busy, too_far).str());
                    }
                }
                struct Workload
                {
                    string Name;
                    size_t Operations;
                    function<uint64_t()> Run;
                };
                vector<Workload> workloads;
                const std::array<string_view, 5> names {"path_near", "path_medium", "path_long", "path_multihex", "path_sealed"};
                for (size_t index = 0; index < routes.size(); index++) {
                    workloads.emplace_back(Workload {string {names[index]}, routes[index].size(), [&, index] {
                                                         uint64_t hash = 14695981039346656037ull;
                                                         for (const Route& route : routes[index]) {
                                                             hash = hash_path(hash, server->MapMngr.FindPath(map, nullptr, route.From, route.To, index == 3 ? 1 : 0, 0));
                                                         }
                                                         return hash;
                                                     }});
                }
                workloads.emplace_back(Workload {"path_to_any", routes[0].size(), [&] {
                                                     uint64_t hash = 14695981039346656037ull;
                                                     for (const Route& route : routes[0]) {
                                                         hash = hash_path(hash, server->MapMngr.FindPathToAny(map, nullptr, route.From, route.Targets, 0));
                                                     }
                                                     return hash;
                                                 }});
                workloads.emplace_back(Workload {"reachable_near", reachable_routes.size(), [&] {
                                                     uint64_t hash = 14695981039346656037ull;
                                                     for (const Route& route : reachable_routes) {
                                                         auto hexes = server->MapMngr.FindReachableHexes(map, route.From, route.Targets);
                                                         hash = mix(hash, hexes.size());
                                                         for (mpos hex : hexes) {
                                                             hash = hash_hex(hash, hex);
                                                         }
                                                     }
                                                     return hash;
                                                 }});
                workloads.emplace_back(Workload {"reachable_sealed", 1, [&] {
                                                     auto hexes = server->MapMngr.FindReachableHexes(map, routes[4][0].From, const_span<mpos> {sealed_centers.data(), 1});
                                                     uint64_t hash = mix(14695981039346656037ull, hexes.size());
                                                     for (mpos hex : hexes) {
                                                         hash = hash_hex(hash, hex);
                                                     }
                                                     return hash;
                                                 }});
                workloads.emplace_back(Workload {"trace", routes[2].size(), [&] {
                                                     uint64_t hash = 14695981039346656037ull;
                                                     for (const Route& route : routes[2]) {
                                                         auto trace = server->MapMngr.TracePath(map, route.From, route.To, 0, 0.0f, nullptr, CritterFindType::Any, true, true);
                                                         hash = mix(hash, static_cast<uint64_t>(trace.FullyTraced) | (static_cast<uint64_t>(trace.HasLastMovable) << 1) | (static_cast<uint64_t>(trace.IsCritterFound) << 2));
                                                         hash = hash_hex(hash_hex(hash_hex(hash, trace.Block), trace.PreBlock), trace.LastMovable);
                                                         hash = mix(hash, trace.Critters.size());
                                                     }
                                                     return hash;
                                                 }});
                for (bool local : {true, false}) {
                    workloads.emplace_back(Workload {local ? "fields_clustered" : "fields_scattered", scattered.size(), [&, local] {
                                                         uint64_t hash = 14695981039346656037ull;
                                                         for (mpos hex : local ? clustered : scattered) {
                                                             hash = mix(hash, static_cast<uint64_t>(map->IsHexMovable(hex)) | (static_cast<uint64_t>(map->IsHexShootable(hex)) << 1) | (static_cast<uint64_t>(map->HasLivingCritter(hex, nullptr)) << 2));
                                                         }
                                                         return hash;
                                                     }});
                }
                workloads.emplace_back(Workload {"multihex_checks", writable.size(), [&] {
                                                     uint64_t hash = 14695981039346656037ull;
                                                     for (mpos hex : writable) {
                                                         hash = mix(hash, map->IsHexesMovable(hex, 2));
                                                     }
                                                     return hash;
                                                 }});
                workloads.emplace_back(Workload {"items_on_hex", item_hexes.size(), [&] {
                                                     uint64_t hash = 14695981039346656037ull;
                                                     for (mpos hex : item_hexes) {
                                                         hash = mix(hash, map->GetItemsOnHex(hex).size());
                                                     }
                                                     return hash;
                                                 }});
                workloads.emplace_back(Workload {"manual_block_toggle", writable.size(), [&] {
                                                     uint64_t hash = 14695981039346656037ull;
                                                     for (mpos hex : writable) {
                                                         map->SetHexManualBlock(hex, true, true);
                                                         hash = mix(hash, static_cast<uint64_t>(map->IsHexMovable(hex)) | (static_cast<uint64_t>(map->IsHexShootable(hex)) << 1));
                                                         map->SetHexManualBlock(hex, false, true);
                                                     }
                                                     return hash;
                                                 }});
                if (iterations.empty()) {
                    iterations.resize(workloads.size());
                    reference_hashes.resize(workloads.size());
                }
                for (size_t index = 0; index < workloads.size(); index++) {
                    const auto& workload = workloads[index];
                    auto warm_started = std::chrono::steady_clock::now();
                    uint64_t warm_hash = workload.Run();
                    double warm_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - warm_started).count();
                    if (sample == 0 && order == 0) {
                        iterations[index] = iround<int32_t>(std::clamp(std::ceil(200.0 / std::max(warm_ms, 0.01)), 1.0, 4096.0));
                        reference_hashes[index] = warm_hash;
                    }
                    REQUIRE(warm_hash == reference_hashes[index]);
                    uint64_t expected_hash = 14695981039346656037ull;
                    for (int32_t iteration = 0; iteration < iterations[index]; iteration++) {
                        expected_hash = mix(expected_hash, warm_hash);
                    }
                    uint64_t hash = 14695981039346656037ull;
                    auto cpu_before = platform::get_cpu_usage_snapshot();
                    auto started = std::chrono::steady_clock::now();
                    for (int32_t iteration = 0; iteration < iterations[index]; iteration++) {
                        hash = mix(hash, workload.Run());
                    }
                    double wall_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
                    auto cpu_after = platform::get_cpu_usage_snapshot();
                    double cpu_ms = numeric_cast<double>(cpu_after.process_time_ns - cpu_before.process_time_ns) / 1000000.0;
                    REQUIRE(hash == expected_hash);
                    size_t operations = workload.Operations * numeric_cast<size_t>(iterations[index]);
                    report.emplace_back(strex("SERVER_GRID_CSV,{},{},{},{},{},{},{:.6f},{:.6f},{:016x}", sample, modes[numeric_cast<size_t>(proto_mode)], modes[numeric_cast<size_t>(instance_mode)], workload.Name, iterations[index], operations, wall_ms, cpu_ms, hash).str());
                }
                server->MapMngr.DestroyLocation(location);
            }));
            WARN(strex("Server grid sample {} prototype {} instance {} complete", sample, modes[numeric_cast<size_t>(proto_mode)], modes[numeric_cast<size_t>(instance_mode)]).str());
            for (const string& line : report) {
                WARN(line);
            }
            report.clear();
        }
    }
}

// A native owner may outlive Shutdown and be released from another thread, but never outlive the engine itself:
// every entity borrows the engine's registrars, protos and interned hashes, so ~ServerEngine asserts the count
TEST_CASE("ServerEntityOwnersReleasedAfterShutdownOnAnotherThread", "[server][entity][lifetime]")
{
    GlobalSettings settings = MakeServerEntityLifetimeSettings();
    StaticMap static_map {msize {2, 2}, "Dynamic"};
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeServerEntityLifetimeResources());
    bool shutdown_done = false;
    auto shutdown_guard = scope_exit([&]() noexcept {
        if (!shutdown_done) {
            safe_call([&server] { server->Shutdown(); });
        }
    });

    REQUIRE(WaitForServerEntityLifetimeStartup(server));
    REQUIRE_FALSE(server->IsStartingError());

    vector<refcount_ptr<ServerEntity>> retained;
    REQUIRE(server->RunInQuiescence(std::chrono::seconds {10}, [&](const ServerQuiescenceState&) { retained = MakeServerEntityLifetimeOwners(server, &static_map); }));
    REQUIRE(retained.size() == 5);

    for (const auto& entity : retained) {
        REQUIRE(entity->GetRefCount() == 1);
    }

    REQUIRE_NOTHROW(server->Shutdown());
    shutdown_done = true;
    REQUIRE(server->IsShutdownInProgress());

    std::atomic_size_t released {0};
    std::thread::id releasing_thread;
    std::thread::id owning_thread = std::this_thread::get_id();
    std::jthread finalizer([entities = std::move(retained), &released, &releasing_thread]() mutable {
        releasing_thread = std::this_thread::get_id();

        while (!entities.empty()) {
            entities.pop_back();
            released.fetch_add(1, std::memory_order_relaxed);
        }
    });
    finalizer.join();

    CHECK(releasing_thread != owning_thread);
    CHECK(released.load(std::memory_order_relaxed) == 5);

    // The engine outlives its last entity, so nothing above dereferenced a freed registrar
    CHECK(server->GetRefCount() == 1);
}

// Baked map files author static item ids from one upward and the client indexes them together with runtime
// items, so a generated world has to start above them exactly as a restored one does
TEST_CASE("ServerGeneratedWorldDrawsEntityIdsAboveTheConfiguredStart", "[server][entity][lifetime]")
{
    GlobalSettings settings = MakeServerEntityLifetimeSettings();
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeServerEntityLifetimeResources());
    auto shutdown_guard = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });

    REQUIRE(WaitForServerEntityLifetimeStartup(server));
    REQUIRE(server->Lock(timespan {std::chrono::seconds {10}}));

    auto unlock = scope_exit([&server]() noexcept { safe_call([&server] { server->Unlock(); }); });
    ptr<Critter> cr = server->CreateCritter(server->Hashes.to_hashed_string("LifetimeCritter"), false);

    CHECK(cr->GetId().underlying_value() > settings.Server.EntityStartId);
    CHECK(server->GetLastEntityId().underlying_value() >= cr->GetId().underlying_value());
}

TEST_CASE("ServerEntityOwnersReleaseBeforeShutdown", "[server][entity][lifetime]")
{
    GlobalSettings settings = MakeServerEntityLifetimeSettings();
    StaticMap static_map {msize {2, 2}, "Dynamic"};
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeServerEntityLifetimeResources());
    bool shutdown_done = false;
    auto shutdown_guard = scope_exit([&]() noexcept {
        if (!shutdown_done) {
            safe_call([&server] { server->Shutdown(); });
        }
    });

    REQUIRE(WaitForServerEntityLifetimeStartup(server));
    REQUIRE_FALSE(server->IsShutdownInProgress());
    REQUIRE(server->RunInQuiescence(std::chrono::seconds {10}, [&](const ServerQuiescenceState&) {
        vector<refcount_ptr<ServerEntity>> entities = MakeServerEntityLifetimeOwners(server, &static_map);
        REQUIRE(entities.size() == 5);

        for (const auto& entity : entities) {
            REQUIRE(entity->GetRefCount() == 1);
        }

        // Ordinary destruction still executes the existing empty-association checks while the engine is alive
        entities.clear();
        CHECK(entities.empty());
    }));
    CHECK_FALSE(server->IsShutdownInProgress());
    REQUIRE_NOTHROW(server->Shutdown());
    shutdown_done = true;
    CHECK(server->IsShutdownInProgress());
}

// A released map goes straight to the job queued for it, so a widen that let go of the map would come back only after
// that job had finished; the queued waiter below takes the map exactly when this context lets it go
TEST_CASE("ServerSyncWidenOfCoveredEntityKeepsHeldLocks", "[server][sync]")
{
    GlobalSettings settings = MakeServerEntityLifetimeSettings();
    StaticMap static_map {msize {2, 2}, "Dynamic"};
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeServerEntityLifetimeResources());
    auto shutdown_guard = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });

    REQUIRE(WaitForServerEntityLifetimeStartup(server));

    // Unregistered owners are enough: the widen reads only each entity's own lock and its parent chain
    refcount_nptr<Map> map;
    refcount_nptr<Critter> holder;
    refcount_nptr<Critter> neighbour;

    REQUIRE(server->RunInQuiescence(std::chrono::seconds {10}, [&](const ServerQuiescenceState&) {
        auto critter_proto = server->GetProtoCritter(server->Hashes.to_hashed_string("LifetimeCritter"));
        auto map_proto = server->GetProtoMap(server->Hashes.to_hashed_string("LifetimeMap"));
        REQUIRE(critter_proto);
        REQUIRE(map_proto);
        map = safe_alloc::make_refcounted<Map>(server, ident_t {1}, map_proto, nullptr, &static_map);
        holder = safe_alloc::make_refcounted<Critter>(server, ident_t {2}, critter_proto);
        holder->SetParent(map);
        neighbour = safe_alloc::make_refcounted<Critter>(server, ident_t {3}, critter_proto);
        neighbour->SetParent(map);
    }));

    auto map_lock = map->GetEntityLock();
    REQUIRE(static_cast<bool>(map_lock));

    SyncContext ctx;
    ctx.Activate();
    auto deactivate = scope_exit([&ctx]() noexcept {
        ctx.Release();
        ctx.Deactivate();
    });

    vector<ptr<ServerEntity>> held {holder, map};
    ctx.SyncEntities(held);
    REQUIRE(map_lock->IsLockedByCurrentThread());

    std::atomic_bool waiter_took_map {false};

    std::thread waiter([&]() {
        SyncContext waiter_ctx;
        waiter_ctx.Activate();
        vector<ptr<ServerEntity>> wanted {map};
        waiter_ctx.SyncEntities(wanted);
        waiter_took_map.store(true);
        waiter_ctx.Release();
        waiter_ctx.Deactivate();
    });
    auto join_waiter = scope_exit([&ctx, &waiter]() noexcept {
        ctx.Release();
        waiter.join();
    });

    while (map_lock->WaiterCount() == 0) {
        coarse_sleep(std::chrono::milliseconds {1});
    }

    // Both entry points: the native widen and a full request that keeps every held entity
    vector<ptr<ServerEntity>> extras {neighbour};
    ctx.WidenEntities(extras);

    CHECK_FALSE(waiter_took_map.load());
    CHECK(map_lock->IsLockedByCurrentThread());
    CHECK(neighbour->GetEntityLock()->IsLockedByCurrentThread());
    CHECK(ctx.GetHeldEntities().size() == 3);

    vector<ptr<ServerEntity>> same_cover {holder, map, neighbour};
    ctx.SyncEntities(same_cover);

    CHECK_FALSE(waiter_took_map.load());
    CHECK(ctx.GetHeldEntities().size() == 3);

    // Only letting the cover go hands the map on
    ctx.Release();
    waiter.join();
    join_waiter.release();
    CHECK(waiter_took_map.load());
}

// A finish handler runs on the thread destroying its subject, so a widen inside it keeps that subject held
TEST_CASE("ServerSyncWidenKeepsHeldEntityBeingDestroyed", "[server][sync]")
{
    GlobalSettings settings = MakeServerEntityLifetimeSettings();
    StaticMap static_map {msize {2, 2}, "Dynamic"};
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeServerEntityLifetimeResources());
    auto shutdown_guard = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });

    REQUIRE(WaitForServerEntityLifetimeStartup(server));

    refcount_nptr<Map> map;
    refcount_nptr<Critter> dying;
    refcount_nptr<Critter> neighbour;

    REQUIRE(server->RunInQuiescence(std::chrono::seconds {10}, [&](const ServerQuiescenceState&) {
        auto critter_proto = server->GetProtoCritter(server->Hashes.to_hashed_string("LifetimeCritter"));
        auto map_proto = server->GetProtoMap(server->Hashes.to_hashed_string("LifetimeMap"));
        REQUIRE(critter_proto);
        REQUIRE(map_proto);
        map = safe_alloc::make_refcounted<Map>(server, ident_t {1}, map_proto, nullptr, &static_map);
        dying = safe_alloc::make_refcounted<Critter>(server, ident_t {2}, critter_proto);
        dying->SetParent(map);
        neighbour = safe_alloc::make_refcounted<Critter>(server, ident_t {3}, critter_proto);
        neighbour->SetParent(map);
    }));

    SyncContext ctx;
    ctx.Activate();
    auto deactivate = scope_exit([&ctx]() noexcept {
        ctx.Release();
        ctx.Deactivate();
    });

    vector<ptr<ServerEntity>> held {dying};
    ctx.SyncEntities(held);
    dying->MarkAsDestroying();

    vector<ptr<ServerEntity>> extras {neighbour};
    ctx.WidenEntities(extras);

    CHECK(dying->GetEntityLock()->IsLockedByCurrentThread());
    CHECK(neighbour->GetEntityLock()->IsLockedByCurrentThread());
    CHECK(ctx.GetHeldEntities().size() == 2);

    // Destruction completes once the handler returns, and only then does a widen let the entity go
    dying->MarkAsDestroyed();
    ctx.WidenEntities({});

    CHECK_FALSE(dying->GetEntityLock()->IsLockedByCurrentThread());
    CHECK(ctx.GetHeldEntities().size() == 1);
}

TEST_CASE("ServerSyncYieldHandsTheWholeThreadCoverToWaitersAndTakesItBack", "[server][sync]")
{
    GlobalSettings settings = MakeServerEntityLifetimeSettings();
    StaticMap static_map {msize {2, 2}, "Dynamic"};
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeServerEntityLifetimeResources());
    auto shutdown_guard = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });

    REQUIRE(WaitForServerEntityLifetimeStartup(server));

    refcount_nptr<Map> map;
    refcount_nptr<Critter> holder;
    refcount_nptr<Critter> neighbour;

    REQUIRE(server->RunInQuiescence(std::chrono::seconds {10}, [&](const ServerQuiescenceState&) {
        auto critter_proto = server->GetProtoCritter(server->Hashes.to_hashed_string("LifetimeCritter"));
        auto map_proto = server->GetProtoMap(server->Hashes.to_hashed_string("LifetimeMap"));
        REQUIRE(critter_proto);
        REQUIRE(map_proto);
        map = safe_alloc::make_refcounted<Map>(server, ident_t {1}, map_proto, nullptr, &static_map);
        holder = safe_alloc::make_refcounted<Critter>(server, ident_t {2}, critter_proto);
        holder->SetParent(map);
        neighbour = safe_alloc::make_refcounted<Critter>(server, ident_t {3}, critter_proto);
        neighbour->SetParent(map);
    }));

    auto map_lock = map->GetEntityLock();
    auto neighbour_lock = neighbour->GetEntityLock();
    REQUIRE(static_cast<bool>(map_lock));
    REQUIRE(static_cast<bool>(neighbour_lock));

    // The outer context stands for the job's own cover, which a nested Sync cannot give away
    SyncContext outer;
    outer.Activate();
    auto deactivate_outer = scope_exit([&outer]() noexcept {
        outer.Release();
        outer.Deactivate();
    });

    vector<ptr<ServerEntity>> job_cover {holder, map};
    outer.SyncEntities(job_cover);

    SyncContext nested;
    nested.Activate();
    auto deactivate_nested = scope_exit([&nested]() noexcept {
        nested.Release();
        nested.Deactivate();
    });

    vector<ptr<ServerEntity>> nested_cover {neighbour};
    nested.SyncEntities(nested_cover);

    int32_t map_recursion = map_lock->GetExclusiveRecursionForCurrentThread();
    REQUIRE(map_recursion > 0);
    REQUIRE(neighbour_lock->IsLockedByCurrentThread());

    std::atomic_bool waiter_took_map {false};

    std::thread waiter([&]() {
        SyncContext waiter_ctx;
        waiter_ctx.Activate();
        vector<ptr<ServerEntity>> wanted {map};
        waiter_ctx.SyncEntities(wanted);
        waiter_took_map.store(true);
        waiter_ctx.Release();
        waiter_ctx.Deactivate();
    });
    auto join_waiter = scope_exit([&outer, &nested, &waiter]() noexcept {
        nested.Release();
        outer.Release();
        waiter.join();
    });

    while (map_lock->WaiterCount() == 0) {
        coarse_sleep(std::chrono::milliseconds {1});
    }

    // A re-sync of the same cover keeps the map (the widen test pins that); the yield alone hands it on
    nested.YieldLocks();

    CHECK(waiter_took_map.load());
    CHECK(map_lock->GetExclusiveRecursionForCurrentThread() == map_recursion);
    CHECK(holder->GetEntityLock()->IsLockedByCurrentThread());
    CHECK(neighbour_lock->IsLockedByCurrentThread());
    CHECK(outer.GetHeldEntities().size() == 2);
    CHECK(nested.GetHeldEntities().size() == 1);

    waiter.join();
    join_waiter.release();
}

TEST_CASE("ServerSyncYieldRefusesWhileTheSingletonIsHeld", "[server][sync]")
{
    GlobalSettings settings = MakeServerEntityLifetimeSettings();
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeServerEntityLifetimeResources());
    auto shutdown_guard = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });

    REQUIRE(WaitForServerEntityLifetimeStartup(server));

    SyncContext ctx;
    ctx.Activate();
    auto deactivate = scope_exit([&ctx]() noexcept {
        ctx.Release();
        ctx.Deactivate();
    });

    // Nothing held is nothing to hand on
    CHECK_NOTHROW(ctx.YieldLocks());

    ctx.LockSingleton(server->GetEntityLock());
    CHECK_THROWS_AS(ctx.YieldLocks(), EntitySyncException);
    CHECK(server->GetEntityLock()->IsLockedByCurrentThread());
}

TEST_CASE("ServerSyncRetainedCoverRefreshesReparentedAncestors", "[server][sync]")
{
    GlobalSettings settings = MakeServerEntityLifetimeSettings();
    StaticMap static_map {msize {2, 2}, "Dynamic"};
    auto server = safe_alloc::make_refcounted<ServerEngine>(&settings, MakeServerEntityLifetimeResources());
    auto shutdown_guard = scope_exit([&server]() noexcept { safe_call([&server] { server->Shutdown(); }); });

    REQUIRE(WaitForServerEntityLifetimeStartup(server));

    refcount_nptr<Map> old_map;
    refcount_nptr<Map> new_map;
    refcount_nptr<Critter> cr;

    REQUIRE(server->RunInQuiescence(std::chrono::seconds {10}, [&](const ServerQuiescenceState&) {
        auto critter_proto = server->GetProtoCritter(server->Hashes.to_hashed_string("LifetimeCritter"));
        auto map_proto = server->GetProtoMap(server->Hashes.to_hashed_string("LifetimeMap"));
        REQUIRE(critter_proto);
        REQUIRE(map_proto);
        old_map = safe_alloc::make_refcounted<Map>(server, ident_t {1}, map_proto, nullptr, &static_map);
        new_map = safe_alloc::make_refcounted<Map>(server, ident_t {2}, map_proto, nullptr, &static_map);
        cr = safe_alloc::make_refcounted<Critter>(server, ident_t {3}, critter_proto);
        cr->SetParent(old_map);
    }));

    SyncContext ctx;
    ctx.Activate();
    auto deactivate = scope_exit([&ctx]() noexcept {
        ctx.Release();
        ctx.Deactivate();
    });

    ctx.SyncEntity(cr);
    REQUIRE(old_map->GetEntityLock()->GetDescendantHoldCountForCurrentThread() == 1);

    // A nested transfer changes the parent while the outer context still records the old ancestor mark
    {
        SyncContext transfer_ctx;
        transfer_ctx.Activate();
        auto release_transfer = scope_exit([&transfer_ctx]() noexcept {
            transfer_ctx.Release();
            transfer_ctx.Deactivate();
        });
        vector<ptr<ServerEntity>> transfer {cr, old_map, new_map};
        transfer_ctx.SyncEntities(transfer);
        cr->SetParent(new_map);
    }

    SECTION("ReplaceSameCover")
    {
        ctx.SyncEntity(cr);
    }
    SECTION("WidenSameCover")
    {
        vector<ptr<ServerEntity>> extras {cr};
        ctx.WidenEntities(extras);
    }
    SECTION("WidenNoExtras")
    {
        ctx.WidenEntities({});
    }

    CHECK(cr->GetEntityLock()->IsLockedByCurrentThread());
    CHECK(new_map->GetEntityLock()->GetDescendantHoldCountForCurrentThread() == 1);
    CHECK(old_map->GetEntityLock()->GetDescendantHoldCountForCurrentThread() == 0);

    // Probe from another thread without blocking: the new map must exclude a foreign exclusive acquisition
    std::atomic_bool took_map {false};
    std::thread probe([&]() {
        if (new_map->GetEntityLock()->TryAcquire()) {
            took_map.store(true);
            new_map->GetEntityLock()->Release();
        }
    });
    probe.join();
    CHECK_FALSE(took_map.load());
}

FO_END_NAMESPACE
