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

#include "Common.h"

#include "MapLoader.h"
#include "Test_MapperHelpers.h"

FO_BEGIN_NAMESPACE

using namespace MapperTests;

TEST_CASE("MapperEngineClearsTheStartingUpFlagOnceItRuns")
{
    // The client constructor deliberately does not finish start-up, because the mapper keeps coming up
    // through its own body; construction itself proves it, since a second finish would throw
    auto settings = MakeMapperTestSettings();
    auto mapper = safe_alloc::make_refcounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    CHECK_FALSE(mapper->IsStartingUp());
}

TEST_CASE("MapperMultihexMeshMerge")
{
    auto settings = MakeMapperTestSettings();
    auto mapper = safe_alloc::make_refcounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    hstring tile_a = mapper->Hashes.to_hashed_string(TILE_A);
    hstring tile_b = mapper->Hashes.to_hashed_string(TILE_B);

    REQUIRE(mapper->GetProtoItem(tile_a));
    REQUIRE(mapper->GetProtoItem(tile_b));
    REQUIRE(mapper->GetProtoItem(tile_a)->GetMultihexGeneration() == MultihexGenerationType::SameSibling);

    SECTION("Coalesces an adjacent block into one multihex-mesh item")
    {
        // A horizontal run of four clean TileA tiles
        string body;
        for (int32_t i = 0; i < 4; i++) {
            body += MakeItemBlock(10 + i, TILE_A, 5 + i, 5);
        }

        auto map = mapper->LoadMapFromText("CoalesceMap", "CoalesceMap.fomap", MakeMapText(body));
        REQUIRE(map);

        REQUIRE(CountItemsOfProto(map, tile_a) == 1);

        nptr<const ItemHexView> survivor;
        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == tile_a) {
                survivor = item;
            }
        }
        REQUIRE(survivor);
        CHECK(survivor->IsNonEmptyMultihexMesh());

        auto covered = CollectMeshHexes(survivor);
        REQUIRE(covered.size() == 4);
        CHECK(covered[0] == mpos {5, 5});
        CHECK(covered[1] == mpos {6, 5});
        CHECK(covered[2] == mpos {7, 5});
        CHECK(covered[3] == mpos {8, 5});
    }

    SECTION("Origin is normalized to the hex_less-smallest covered hex and mesh is sorted")
    {
        // Authoring order intentionally does not start at the smallest hex
        string body;
        body += MakeItemBlock(30, TILE_A, 8, 5);
        body += MakeItemBlock(31, TILE_A, 6, 5);
        body += MakeItemBlock(32, TILE_A, 7, 5);
        body += MakeItemBlock(33, TILE_A, 5, 5);

        auto map = mapper->LoadMapFromText("NormalizeMap", "NormalizeMap.fomap", MakeMapText(body));
        REQUIRE(map);
        REQUIRE(CountItemsOfProto(map, tile_a) == 1);

        nptr<const ItemHexView> survivor;
        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == tile_a) {
                survivor = item;
            }
        }
        REQUIRE(survivor);

        auto covered = CollectMeshHexes(survivor);
        REQUIRE(covered.size() == 4);

        // Origin is the smallest covered hex
        CHECK(survivor->GetHex() == covered.front());
        CHECK(survivor->GetHex() == mpos {5, 5});

        // The stored mesh (origin + remaining covered hexes) is sorted by hex_less
        const auto& mesh = survivor->GetMultihexMesh();
        REQUIRE(!mesh.empty());
        CHECK(std::ranges::is_sorted(mesh, HexLess));
        CHECK(HexLess(survivor->GetHex(), mesh.front()));
    }

    SECTION("Physical-only mesh retains its single sprite origin during normalization and save")
    {
        string props = "MultihexGeneration = None\nDrawMultihexMesh = False\nOffset = 7 -9\nMultihexMesh = \"6 5\" \"7 6\"";
        auto map = mapper->LoadMapFromText("PhysicalMesh", "PhysicalMesh.fomap", MakeMapText(MakeItemBlock(50, TILE_A, 8, 5, props)));
        REQUIRE(map);
        REQUIRE(map->GetItems().size() == 1);

        auto item = map->GetItems().front().as_ptr();
        CHECK(item->GetHex() == mpos {8, 5});
        CHECK(item->GetOffset().x == 7);
        CHECK(item->GetOffset().y == -9);
        CHECK(CollectMeshHexes(item) == vector<mpos> {{6, 5}, {8, 5}, {7, 6}});

        size_t extra_merges = mapper->MergeItemsToMultihexMeshes(map);
        CHECK(extra_merges == 0);
        CHECK(item->GetHex() == mpos {8, 5});

        string saved = map->SaveToText("PhysicalMeshSaved");
        auto reloaded = mapper->LoadMapFromText("PhysicalMeshSaved", "PhysicalMeshSaved.fomap", saved);
        REQUIRE(reloaded);
        REQUIRE(reloaded->GetItems().size() == 1);
        CHECK(reloaded->GetItems().front()->GetHex() == mpos {8, 5});
        CHECK(reloaded->GetItems().front()->GetOffset() == item->GetOffset());
        CHECK(CollectMeshHexes(reloaded->GetItems().front().as_ptr()) == CollectMeshHexes(item));
    }

    SECTION("Idempotent: re-running the merge changes nothing")
    {
        string body;
        for (int32_t i = 0; i < 4; i++) {
            body += MakeItemBlock(40 + i, TILE_A, 5 + i, 5);
        }

        auto map = mapper->LoadMapFromText("IdempotentMap", "IdempotentMap.fomap", MakeMapText(body));
        REQUIRE(map);
        REQUIRE(CountItemsOfProto(map, tile_a) == 1);

        nptr<const ItemHexView> survivor;
        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == tile_a) {
                survivor = item;
            }
        }
        REQUIRE(survivor);

        auto origin_before = survivor->GetHex();
        auto covered_before = CollectMeshHexes(survivor);

        // LoadMapFromText already runs the merge twice (the second call asserts idempotency); run it once
        // more directly to lock that the public entry point is a fixed point on an already-merged map
        size_t extra_merges = mapper->MergeItemsToMultihexMeshes(map);
        CHECK(extra_merges == 0);

        REQUIRE(CountItemsOfProto(map, tile_a) == 1);
        CHECK(survivor->GetHex() == origin_before);
        CHECK(CollectMeshHexes(survivor) == covered_before);
    }

    SECTION("Different protos do not merge")
    {
        string body;
        body += MakeItemBlock(50, TILE_A, 5, 5);
        body += MakeItemBlock(51, TILE_B, 6, 5);

        auto map = mapper->LoadMapFromText("CrossProtoMap", "CrossProtoMap.fomap", MakeMapText(body));
        REQUIRE(map);

        CHECK(CountItemsOfProto(map, tile_a) == 1);
        CHECK(CountItemsOfProto(map, tile_b) == 1);

        for (const auto& item : map->GetItems()) {
            CHECK_FALSE(item->IsNonEmptyMultihexMesh());
        }
    }

    SECTION("Tiles with differing modified data are not merged together")
    {
        // Neither tile is clean and their data differs, so they stay apart: merging two non-clean items requires
        // identical data
        string body;
        body += MakeItemBlock(60, TILE_A, 5, 5, "LightDistance = 7");
        body += MakeItemBlock(61, TILE_A, 6, 5, "LightDistance = 9");

        auto map = mapper->LoadMapFromText("ModifiedPairMap", "ModifiedPairMap.fomap", MakeMapText(body));
        REQUIRE(map);

        REQUIRE(CountItemsOfProto(map, tile_a) == 2);

        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == tile_a) {
                CHECK_FALSE(item->IsNonEmptyMultihexMesh());
            }
        }
    }

    SECTION("Current behavior: a clean tile next to a modified tile still coalesces")
    {
        // Locks the surprising rule that a clean source merges into any same-proto target, so one modified tile
        // among clean ones is not kept separate and its authored data is dropped
        string body;
        body += MakeItemBlock(70, TILE_A, 5, 5);
        body += MakeItemBlock(71, TILE_A, 6, 5);
        body += MakeItemBlock(72, TILE_A, 7, 5);
        body += MakeItemBlock(73, TILE_A, 8, 5, "LightDistance = 7");

        auto map = mapper->LoadMapFromText("CleanPlusModifiedMap", "CleanPlusModifiedMap.fomap", MakeMapText(body));
        REQUIRE(map);

        REQUIRE(CountItemsOfProto(map, tile_a) == 1);

        nptr<const ItemHexView> survivor;
        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == tile_a) {
                survivor = item;
            }
        }
        REQUIRE(survivor);
        CHECK(survivor->IsNonEmptyMultihexMesh());
        CHECK(CollectMeshHexes(survivor).size() == 4);
    }

    SECTION("Two disjoint clusters stay separate")
    {
        string body;
        // Cluster 1 near the top-left
        body += MakeItemBlock(70, TILE_A, 3, 3);
        body += MakeItemBlock(71, TILE_A, 4, 3);
        // Cluster 2 far away so the two never touch as neighbors
        body += MakeItemBlock(72, TILE_A, 20, 20);
        body += MakeItemBlock(73, TILE_A, 21, 20);

        auto map = mapper->LoadMapFromText("DisjointMap", "DisjointMap.fomap", MakeMapText(body));
        REQUIRE(map);

        REQUIRE(CountItemsOfProto(map, tile_a) == 2);

        int32_t mesh_items = 0;
        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == tile_a) {
                CHECK(item->IsNonEmptyMultihexMesh());
                CHECK(CollectMeshHexes(item).size() == 2);
                mesh_items++;
            }
        }
        CHECK(mesh_items == 2);
    }

    SECTION("Path-dependent: a clean bridge collapses a modified..modified chain into one clean mesh")
    {
        // A naive flood-fill would merge this line too but pick a different survivor: here clean middle tiles
        // bridge two modified ends and the clean proto data wins, which is exactly what must not change
        string body;
        body += MakeItemBlock(200, TILE_A, 5, 5, "LightDistance = 3");
        body += MakeItemBlock(201, TILE_A, 6, 5);
        body += MakeItemBlock(202, TILE_A, 7, 5);
        body += MakeItemBlock(203, TILE_A, 8, 5, "LightDistance = 9");

        auto map = mapper->LoadMapFromText("ModChainAsc", "ModChainAsc.fomap", MakeMapText(body));
        REQUIRE(map);

        auto survivors = CollectSurvivors(map, tile_a);
        REQUIRE(survivors.size() == 1);
        CHECK(survivors[0].Origin == mpos {5, 5});
        CHECK(survivors[0].LightDistance == 0);
        CHECK(survivors[0].Covered == vector<mpos> {{5, 5}, {6, 5}, {7, 5}, {8, 5}});
    }

    SECTION("Path-dependent: a modified tile bridged by clean neighbors is absorbed (clean - modified - clean)")
    {
        string body;
        body += MakeItemBlock(210, TILE_A, 5, 5);
        body += MakeItemBlock(211, TILE_A, 6, 5, "LightDistance = 4");
        body += MakeItemBlock(212, TILE_A, 7, 5);

        auto map = mapper->LoadMapFromText("CleanModClean", "CleanModClean.fomap", MakeMapText(body));
        REQUIRE(map);

        auto survivors = CollectSurvivors(map, tile_a);
        REQUIRE(survivors.size() == 1);
        CHECK(survivors[0].Origin == mpos {5, 5});
        CHECK(survivors[0].LightDistance == 0);
        CHECK(survivors[0].Covered == vector<mpos> {{5, 5}, {6, 5}, {7, 5}});
    }

    SECTION("Path-dependent: modified..clean..modified collapse is id-order independent")
    {
        // The same chain with the id order reversed, so the per-step merge direction differs: an identical result
        // is what proves the optimization stayed insensitive to authoring order
        string body;
        body += MakeItemBlock(220, TILE_A, 5, 5, "LightDistance = 3");
        body += MakeItemBlock(223, TILE_A, 6, 5);
        body += MakeItemBlock(222, TILE_A, 7, 5);
        body += MakeItemBlock(221, TILE_A, 8, 5, "LightDistance = 9");

        auto map = mapper->LoadMapFromText("ModChainModLowIds", "ModChainModLowIds.fomap", MakeMapText(body));
        REQUIRE(map);

        auto survivors = CollectSurvivors(map, tile_a);
        REQUIRE(survivors.size() == 1);
        CHECK(survivors[0].Origin == mpos {5, 5});
        CHECK(survivors[0].LightDistance == 0);
        CHECK(survivors[0].Covered == vector<mpos> {{5, 5}, {6, 5}, {7, 5}, {8, 5}});
    }

    SECTION("Modified data partitions a run that no clean tile bridges (modified X - X - Y)")
    {
        // Two tiles share authored data and merge while the third differs with no clean tile to bridge it, which
        // a pure proto-adjacency flood-fill would over-merge into one mesh
        string body;
        body += MakeItemBlock(230, TILE_A, 5, 5, "LightDistance = 5");
        body += MakeItemBlock(231, TILE_A, 6, 5, "LightDistance = 5");
        body += MakeItemBlock(232, TILE_A, 7, 5, "LightDistance = 8");

        auto map = mapper->LoadMapFromText("ModXModXModY", "ModXModXModY.fomap", MakeMapText(body));
        REQUIRE(map);

        auto survivors = CollectSurvivors(map, tile_a);
        REQUIRE(survivors.size() == 2);

        CHECK(survivors[0].Origin == mpos {5, 5});
        CHECK(survivors[0].LightDistance == 5);
        CHECK(survivors[0].Covered == vector<mpos> {{5, 5}, {6, 5}});

        CHECK(survivors[1].Origin == mpos {7, 5});
        CHECK(survivors[1].LightDistance == 8);
        CHECK(survivors[1].Covered == vector<mpos> {{7, 5}});
    }

    SECTION("Large solid block coalesces and loads")
    {
        // Solid 24x24 block of clean TileA tiles; pins the result shape for the perf optimization
        constexpr int32_t block = 24;
        constexpr int32_t origin = 3;

        string body;
        int32_t id = 100;
        for (int32_t y = 0; y < block; y++) {
            for (int32_t x = 0; x < block; x++) {
                body += MakeItemBlock(id++, TILE_A, origin + x, origin + y);
            }
        }

        auto map = mapper->LoadMapFromText("LargeBlockMap", "LargeBlockMap.fomap", MakeMapText(body, 64));
        REQUIRE(map);

        // Current behavior: an adjacency-connected block collapses into a single multihex-mesh item
        REQUIRE(CountItemsOfProto(map, tile_a) == 1);

        nptr<const ItemHexView> survivor;
        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == tile_a) {
                survivor = item;
            }
        }
        REQUIRE(survivor);

        auto covered = CollectMeshHexes(survivor);
        CHECK(covered.size() == numeric_cast<size_t>(block) * block);
        CHECK(survivor->GetHex() == mpos {origin, origin});
    }
}

TEST_CASE("MapperAnyUniqueMeshMerge")
{
    auto settings = MakeMapperTestSettings();
    auto mapper = safe_alloc::make_refcounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    hstring tile_u = mapper->Hashes.to_hashed_string(TILE_U);

    REQUIRE(mapper->GetProtoItem(tile_u));
    REQUIRE(mapper->GetProtoItem(tile_u)->GetMultihexGeneration() == MultihexGenerationType::AnyUnique);

    SECTION("Non-adjacent clean tiles merge into one mesh despite no adjacency")
    {
        string body;
        body += MakeItemBlock(300, TILE_U, 3, 3);
        body += MakeItemBlock(301, TILE_U, 20, 20);
        body += MakeItemBlock(302, TILE_U, 5, 25);

        auto map = mapper->LoadMapFromText("U_NonAdjacentClean", "U_NonAdjacentClean.fomap", MakeMapText(body));
        REQUIRE(map);

        auto survivors = CollectSurvivors(map, tile_u);
        REQUIRE(survivors.size() == 1);
        CHECK(survivors[0].Id == 300); // lowest id wins
        CHECK(survivors[0].Origin == mpos {3, 3});
        CHECK(survivors[0].LightDistance == 0);
        // hex_less is y-major: (3,3) then (20,20) then (5,25)
        CHECK(survivors[0].Covered == vector<mpos> {{3, 3}, {20, 20}, {5, 25}});
    }

    SECTION("Scattered tiles partition by per-item data, each group collapsing into its lowest id")
    {
        string body;
        body += MakeItemBlock(310, TILE_U, 2, 2);
        body += MakeItemBlock(311, TILE_U, 10, 2, "LightDistance = 7");
        body += MakeItemBlock(312, TILE_U, 2, 10);
        body += MakeItemBlock(313, TILE_U, 25, 25, "LightDistance = 7");
        body += MakeItemBlock(314, TILE_U, 18, 4, "LightDistance = 9");

        auto map = mapper->LoadMapFromText("U_MixedScatter", "U_MixedScatter.fomap", MakeMapText(body));
        REQUIRE(map);

        auto survivors = CollectSurvivors(map, tile_u);
        REQUIRE(survivors.size() == 3);

        // Clean group (310 + 312)
        CHECK(survivors[0].Id == 310);
        CHECK(survivors[0].Origin == mpos {2, 2});
        CHECK(survivors[0].LightDistance == 0);
        CHECK(survivors[0].Covered == vector<mpos> {{2, 2}, {2, 10}});

        // LightDistance == 7 group (311 + 313)
        CHECK(survivors[1].Id == 311);
        CHECK(survivors[1].Origin == mpos {10, 2});
        CHECK(survivors[1].LightDistance == 7);
        CHECK(survivors[1].Covered == vector<mpos> {{10, 2}, {25, 25}});

        // Lone LightDistance == 9 tile (314)
        CHECK(survivors[2].Id == 314);
        CHECK(survivors[2].Origin == mpos {18, 4});
        CHECK(survivors[2].LightDistance == 9);
        CHECK(survivors[2].Covered == vector<mpos> {{18, 4}});
    }

    SECTION("Survivor is the lowest-id group member regardless of authoring/position order")
    {
        string body;
        body += MakeItemBlock(330, TILE_U, 5, 5);
        body += MakeItemBlock(320, TILE_U, 6, 5);
        body += MakeItemBlock(325, TILE_U, 7, 5);

        auto map = mapper->LoadMapFromText("U_ShuffledIds", "U_ShuffledIds.fomap", MakeMapText(body));
        REQUIRE(map);

        auto survivors = CollectSurvivors(map, tile_u);
        REQUIRE(survivors.size() == 1);
        CHECK(survivors[0].Id == 320); // lowest id, even though it was authored second and sits at (6,5)
        CHECK(survivors[0].Origin == mpos {5, 5}); // origin normalized to the hex_less-smallest covered hex
        CHECK(survivors[0].LightDistance == 0);
        CHECK(survivors[0].Covered == vector<mpos> {{5, 5}, {6, 5}, {7, 5}});
    }

    SECTION("Different protos and a SameSibling tile never merge into an AnyUnique mesh")
    {
        hstring tile_a = mapper->Hashes.to_hashed_string(TILE_A);

        string body;
        body += MakeItemBlock(340, TILE_U, 3, 3);
        body += MakeItemBlock(341, TILE_A, 4, 3); // SameSibling, adjacent - must not join the AnyUnique mesh
        body += MakeItemBlock(342, TILE_U, 6, 3);

        auto map = mapper->LoadMapFromText("U_CrossStrategy", "U_CrossStrategy.fomap", MakeMapText(body));
        REQUIRE(map);

        auto u_survivors = CollectSurvivors(map, tile_u);
        REQUIRE(u_survivors.size() == 1);
        CHECK(u_survivors[0].Id == 340);
        CHECK(u_survivors[0].Covered == vector<mpos> {{3, 3}, {6, 3}});

        CHECK(CountItemsOfProto(map, tile_a) == 1);
    }

    SECTION("Idempotent: re-running the merge changes nothing")
    {
        string body;
        body += MakeItemBlock(350, TILE_U, 2, 2);
        body += MakeItemBlock(351, TILE_U, 9, 9);
        body += MakeItemBlock(352, TILE_U, 4, 12, "LightDistance = 4");
        body += MakeItemBlock(353, TILE_U, 20, 1, "LightDistance = 4");

        auto map = mapper->LoadMapFromText("U_Idempotent", "U_Idempotent.fomap", MakeMapText(body));
        REQUIRE(map);

        auto before = CollectSurvivors(map, tile_u);
        size_t extra_merges = mapper->MergeItemsToMultihexMeshes(map);
        CHECK(extra_merges == 0);
        CHECK(CollectSurvivors(map, tile_u) == before);
    }
}

TEST_CASE("MapperLoadMapResolvesNameAndPath")
{
    auto settings = MakeMapperTestSettings();

    // Mirror the project's proto-extension order (LastFrontier.fomain), where .foloc precedes .fomap.
    // That ordering is what let a same-stem location file shadow the map file during discovery
    BakerTests::OverrideSetting(settings.Baking.ProtoFileExtensions, vector<string> {"foinfo", "fopro", "foloc", "fomap", "focr", "foitem"});

    auto mapper = safe_alloc::make_refcounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    // A single-map .fomap and a same-stem .foloc live side by side under a subdirectory, mirroring the
    // real content layout. The .foloc must not shadow the .fomap for either lookup form
    auto maps_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("MapperLoadMapTestMaps");
    maps_source->AddFile("Gambell/ShadowedMap.foloc", "[ProtoLocation]\n$Name = ShadowedMap\nMapProtos = ShadowedMap\n");
    maps_source->AddFile("Gambell/ShadowedMap.fomap", MakeMapText(MakeItemBlock(10, TILE_A, 5, 5)));
    mapper->MapsFileSys.AddCustomSource(std::move(maps_source));

    hstring expected_proto = mapper->Hashes.to_hashed_string("ShadowedMap");

    SECTION("Loads by directory-qualified path despite a same-stem location sibling")
    {
        auto map = mapper->LoadMap("Gambell/ShadowedMap");
        REQUIRE(map);
        CHECK(map->GetProtoId() == expected_proto);
    }

    SECTION("Loads by bare declared map name")
    {
        auto map = mapper->LoadMap("ShadowedMap");
        REQUIRE(map);
        CHECK(map->GetProtoId() == expected_proto);
    }
}

TEST_CASE("MapperDrawsEditorPanelsHeadlessly")
{
    auto settings = MakeMapperTestSettings();
    auto mapper = safe_alloc::make_refcounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    mapper->InitIface();

    // The inspector needs an explicit property-list provider, independently of the scripting backend
    mapper->OnInspectorProperties.UnsubscribeAll();
    Entity::EventCallbackData inspector_properties;
    inspector_properties.Callback = [](FuncCallData& call) {
        ptr<void> properties = call.ArgsData[1];

        for (int32_t index = 0; index < 12; index++) {
            call.Accessor->AddArrayElement(properties, &index);
        }

        int32_t separator = -1;
        call.Accessor->AddArrayElement(properties, &separator);
        return Entity::EventResult::ContinueChain;
    };
    inspector_properties.SubscriptionPtr = reinterpret_cast<uintptr_t>(&mapper);
    mapper->OnInspectorProperties.Subscribe(std::move(inspector_properties));
    auto unsubscribe_inspector = scope_exit([&mapper]() noexcept { mapper->OnInspectorProperties.Unsubscribe(reinterpret_cast<uintptr_t>(&mapper)); });

    REQUIRE(ImGui::GetCurrentContext() == nullptr);
    ImGuiExt::Init();

    auto destroy_context = scope_exit([]() noexcept {
        safe_call([] {
            if (ImGui::GetCurrentContext() != nullptr) {
                ImGui::DestroyContext();
            }
        });
    });

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2 {1280.0f, 720.0f};
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = nullptr;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    // Every editor panel is hidden by default, so opt them all in before drawing
    mapper->WorkspaceWindowVisible = true;
    mapper->ContentWindowVisible = true;
    mapper->CritterAnimationsWindowVisible = true;
    mapper->ScriptCallWindowVisible = true;
    mapper->MapListWindowVisible = true;
    mapper->MapWindowVisible = true;
    mapper->HistoryWindowVisible = true;
    mapper->SettingsWindowVisible = true;
    mapper->InspectorVisible = true;

    // A shown map with everything selected gives the map, content and inspector panels real rows to render:
    // the inspector only lists property lines for a selected entity, so without this it draws an empty frame
    string body = MakeItemBlock(10, TILE_A, 5, 5) + MakeItemBlock(11, TILE_B, 7, 7);
    auto map = mapper->LoadMapFromText("PanelMap", "PanelMap.fomap", MakeMapText(body));
    REQUIRE(map);

    mapper->ShowMap(map.as_ptr());
    mapper->SelectAll();
    REQUIRE_FALSE(mapper->SelectedEntities.empty());
    REQUIRE(mapper->GetInspectorEntity());

    // Walk the inspector's property lines and commit an edit so the parse/apply path runs too
    for (int32_t line = 0; line < 4; line++) {
        REQUIRE_NOTHROW(mapper->SelectInspectorPropertyLine(line));
    }

    // Committing an edit runs the text parser and the undo op behind it; the first property rows start at
    // line 3, and a malformed value has to be rejected and rolled back to the initial one
    for (int32_t line = 3; line < 8; line++) {
        mapper->SelectInspectorPropertyLine(line);
        mapper->InspectorSelectedLineValue = "1";
        REQUIRE_NOTHROW(mapper->ApplyInspectorPropertyEdit(mapper->GetInspectorEntity().as_ptr()));
    }

    mapper->SelectInspectorPropertyLine(3);
    mapper->InspectorSelectedLineInitialValue = "0";
    mapper->InspectorSelectedLineValue = "not a value";
    REQUIRE_NOTHROW(mapper->ApplyInspectorPropertyEdit(mapper->GetInspectorEntity().as_ptr()));

    // Structured values go through the token reader, which handles quoting, escapes and nesting
    for (string_view structured_value : {"{ 1 2 3 }", R"("quoted value")", R"({ "a" 1 "b" 2 })", R"("with \"escape\"")", "{ unterminated"}) {
        mapper->SelectInspectorPropertyLine(4);
        mapper->InspectorSelectedLineValue = string {structured_value};
        REQUIRE_NOTHROW(mapper->ApplyInspectorPropertyEdit(mapper->GetInspectorEntity().as_ptr()));
    }

    mapper->SelectInspectorPropertyLine(0);

    // Logging auto-expands tree nodes, so the panel bodies run instead of collapsing to headers
    constexpr int32_t PANEL_FRAMES = 8;
    string drawn_text;

    // Each panel mode shows a different content-window body, so the frames walk through them
    vector<int32_t> panel_modes {MapperEngine::INT_MODE_ITEM, MapperEngine::INT_MODE_TILE, MapperEngine::INT_MODE_CRIT, MapperEngine::INT_MODE_INCONT, MapperEngine::INT_MODE_FAST, MapperEngine::INT_MODE_IGNORE, MapperEngine::INT_MODE_MESS, MapperEngine::INT_MODE_LIST};

    // Collapsing headers opt out of the log auto-expansion, so their bodies only render once their stored
    // state is seeded open; these are the ones the map window carries
    constexpr std::array MAP_WINDOW_HEADER_IDS = {"Visibility", "Selection"};

    for (int32_t frame = 0; frame < PANEL_FRAMES; frame++) {
        mapper->SetActivePanelMode(panel_modes[numeric_cast<size_t>(frame) % panel_modes.size()]);

        ImGui::NewFrame();

        // State storage is per window, so the Controls window has to be found by name once it exists
        if (auto controls_window = make_nptr(ImGui::FindWindowByName("Controls"))) {
            for (string_view header_id : MAP_WINDOW_HEADER_IDS) {
                controls_window->StateStorage.SetInt(controls_window->GetID(header_id.data(), header_id.data() + header_id.size()), 1);
            }
        }

        ImGui::LogToBuffer(12);

        REQUIRE_NOTHROW(mapper->DrawMainPanelImGui());
        REQUIRE_NOTHROW(mapper->DrawWorkspaceWindowImGui());
        REQUIRE_NOTHROW(mapper->DrawContentWindowImGui());
        REQUIRE_NOTHROW(mapper->DrawCritterAnimationsWindowImGui());
        REQUIRE_NOTHROW(mapper->DrawScriptCallWindowImGui());
        REQUIRE_NOTHROW(mapper->DrawMapListWindowImGui());
        REQUIRE_NOTHROW(mapper->DrawMapWindowImGui());
        REQUIRE_NOTHROW(mapper->DrawHistoryWindowImGui());
        REQUIRE_NOTHROW(mapper->DrawSettingsWindowImGui());
        REQUIRE_NOTHROW(mapper->DrawInspectorImGui());
        REQUIRE_NOTHROW(mapper->DrawConsoleImGui());

        drawn_text.assign(ImGui::GetCurrentContext()->LogBuffer.c_str());
        ImGui::LogFinish();
        ImGui::Render();
    }

    CHECK(ImGui::GetFrameCount() == PANEL_FRAMES);
    INFO(drawn_text);
    CHECK_FALSE(drawn_text.empty());

    // The inspector draws an editor only for the line being edited, so walking every line in turn is what
    // reaches each property type's widget
    REQUIRE_FALSE(mapper->ShowProps.empty());

    for (size_t prop_index = 0; prop_index < mapper->ShowProps.size(); prop_index++) {
        int32_t line = 3 + numeric_cast<int32_t>(prop_index);
        INFO(line);

        mapper->SelectInspectorPropertyLine(line);
        mapper->InspectorEditLine = line;
        mapper->InspectorEditBuf = mapper->InspectorSelectedLineValue;

        // Two frames per line: the second one meets the pending focus/caret state the first one queued
        for (int32_t frame = 0; frame < 2; frame++) {
            ImGui::NewFrame();
            ImGui::LogToBuffer(12);
            REQUIRE_NOTHROW(mapper->DrawInspectorImGui());
            ImGui::LogFinish();
            ImGui::Render();
        }
    }

    // Applying an edit to every compatible selected entity is a separate arm of the same walk
    mapper->InspectorApplyToAll = true;
    mapper->SelectInspectorPropertyLine(3);
    mapper->InspectorEditLine = 3;
    mapper->InspectorEditBuf = mapper->InspectorSelectedLineValue;

    ImGui::NewFrame();
    REQUIRE_NOTHROW(mapper->DrawInspectorImGui());
    ImGui::Render();

    mapper->InspectorEditLine = -1;
    mapper->InspectorApplyToAll = false;
}

TEST_CASE("MapperSelectionFollowsLayerVisibility")
{
    // Select-all admits each placed entity by its own kind, so the map carries one of every kind and the layers
    // are switched off one at a time to reach each arm
    auto settings = MakeMapperTestSettings();
    auto mapper = safe_alloc::make_refcounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    mapper->InitIface();

    string body = MakeItemBlock(10, TILE_A, 5, 5);
    body += MakeItemBlock(11, SCENERY_A, 7, 5);
    body += MakeItemBlock(12, WALL_A, 9, 5);
    body += MakeCritterBlock(13, CRITTER_A, 11, 5);

    auto map = mapper->LoadMapFromText("SelectionMap", "SelectionMap.fomap", MakeMapText(body));
    REQUIRE(map);
    mapper->ShowMap(map.as_ptr());

    // Tiles are not authored in map text - the mapper places them, which is also what marks them as roof
    ptr<MapView> map_ptr = map.as_ptr();
    REQUIRE_NOTHROW(ignore_unused(map_ptr->AddMapperTile(mapper->Hashes.to_hashed_string(TILE_A), mpos {6, 8}, 0, false).get()));
    REQUIRE_NOTHROW(ignore_unused(map_ptr->AddMapperTile(mapper->Hashes.to_hashed_string(TILE_B), mpos {8, 8}, 0, true).get()));

    SECTION("EveryLayerContributesItsOwnEntities")
    {
        mapper->SelectAll();
        size_t all_layers = mapper->SelectedEntities.size();
        CHECK(all_layers >= 6);

        // Each layer switched off must cost exactly the entities that belong to it
        mapper->VisibleLayers = exclude_enum(mapper->VisibleLayers, MapLayers::Critters);
        mapper->SelectAll();
        CHECK(mapper->SelectedEntities.size() < all_layers);

        mapper->VisibleLayers = exclude_enum(mapper->VisibleLayers, MapLayers::Scenery, MapLayers::Walls, MapLayers::Tiles, MapLayers::Roof);
        mapper->SelectAll();
        size_t items_only = mapper->SelectedEntities.size();
        CHECK(items_only < all_layers);

        mapper->VisibleLayers = exclude_enum(mapper->VisibleLayers, MapLayers::Items);
        mapper->SelectAll();
        CHECK(mapper->SelectedEntities.empty());

        mapper->VisibleLayers = MapLayers::All;
    }

    SECTION("PerKindSelectionSwitchesGateTheSameWalk")
    {
        mapper->SelectCrittersEnabled = false;
        mapper->SelectSceneryEnabled = false;
        mapper->SelectWallsEnabled = false;
        mapper->SelectTilesEnabled = false;
        mapper->SelectRoofTilesEnabled = false;
        mapper->SelectAll();
        size_t items_only = mapper->SelectedEntities.size();

        mapper->SelectItemsEnabled = false;
        mapper->SelectAll();
        CHECK(mapper->SelectedEntities.empty());

        mapper->SelectItemsEnabled = true;
        mapper->SelectCrittersEnabled = true;
        mapper->SelectSceneryEnabled = true;
        mapper->SelectWallsEnabled = true;
        mapper->SelectTilesEnabled = true;
        mapper->SelectRoofTilesEnabled = true;
        mapper->SelectAll();
        CHECK(mapper->SelectedEntities.size() > items_only);
    }

    SECTION("DeselectingAnAnyUniqueItemRunsTheIncrementalMerge")
    {
        // Dropping an AnyUnique item out of the selection re-merges it through the per-step driver rather than the
        // whole-map coalescer, a path no other fixture reaches
        ptr<MapView> selection_map = mapper->GetCurMap().as_ptr();
        hstring unique_pid = mapper->Hashes.to_hashed_string(TILE_U);

        auto first_unique = selection_map->AddMapperItem(unique_pid, mpos {4, 12}, nullptr);
        auto second_unique = selection_map->AddMapperItem(unique_pid, mpos {16, 18}, nullptr);
        REQUIRE_NOTHROW(ignore_unused(first_unique.get()));
        REQUIRE_NOTHROW(ignore_unused(second_unique.get()));

        mapper->SelectClear();
        mapper->SelectAdd(first_unique);
        mapper->SelectAdd(second_unique);
        REQUIRE(mapper->SelectedEntities.size() == 2);

        // The second one leaving the selection is what offers it to the first one's mesh
        REQUIRE_NOTHROW(mapper->SelectRemove(second_unique, false));
        REQUIRE_NOTHROW(mapper->SelectRemove(first_unique, false));
        CHECK(mapper->SelectedEntities.empty());

        // Same-proto AnyUnique items with identical data collapse into one mesh regardless of distance
        size_t unique_left = 0;

        for (ptr<const ItemHexView> item : selection_map->GetItems()) {
            if (!item->IsDestroyed() && item->GetProtoId() == unique_pid) {
                unique_left++;
            }
        }

        CHECK(unique_left == 1);
    }

    SECTION("RemovingAndReAddingASelectionKeepsBothStoresInStep")
    {
        mapper->SelectAll();
        REQUIRE_FALSE(mapper->SelectedEntities.empty());

        size_t before = mapper->SelectedEntities.size();
        ptr<ClientEntity> first = mapper->SelectedEntities.front().as_ptr();

        mapper->SelectRemove(first, false);
        CHECK(mapper->SelectedEntities.size() == before - 1);

        // Removing an entity that is not selected is ordinary editing, not an error
        REQUIRE_NOTHROW(mapper->SelectRemove(first, false));
        CHECK(mapper->SelectedEntities.size() == before - 1);

        mapper->SelectAdd(first);
        CHECK(mapper->SelectedEntities.size() == before);

        mapper->SelectClear();
        CHECK(mapper->SelectedEntities.empty());
    }
}

TEST_CASE("MapperPanelControlsRunTheirActions")
{
    // Panels draw in every headless frame but nothing is pressed, so the code behind each button needs an
    // explicit press; saving writes real files, hence the private Maps root
    auto maps_dir = std::filesystem::temp_directory_path() / std::format("fo_engine_mapper_controls_test_{}", std::chrono::steady_clock::now().time_since_epoch().count());
    std::error_code remove_error;
    std::filesystem::remove_all(maps_dir, remove_error);
    std::filesystem::create_directories(maps_dir);

    auto cleanup_maps_dir = scope_exit([maps_dir]() noexcept {
        safe_call([maps_dir] {
            std::error_code ignored;
            std::filesystem::remove_all(maps_dir, ignored);
        });
    });

    REQUIRE(fs::write_file((maps_dir / "ReferenceMap.fomap").generic_string(), MakeMapText(MakeItemBlock(10, TILE_A, 5, 5))));

    auto settings = MakeMapperTestSettings();
    BakerTests::OverrideSetting(settings.Baking.ProtoFileExtensions, vector<string> {"fopro", "fomap"});

    // The direct-draw render path the headless fixture uses turns map zoom off, and with it the zoom
    // buttons become no-ops that cannot be told apart from a press that never landed
    BakerTests::OverrideSetting(settings.View.MapZoomEnabled, true);
    auto pack_config = ConfigFile(strex("[ResourcePack]\nName = MapperControlsTestPack\nInputDirs = {}\n", maps_dir.generic_string()).str());
    settings.ApplyConfigFile(pack_config, "");

    auto mapper = safe_alloc::make_refcounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    mapper->InitIface();

    REQUIRE(ImGui::GetCurrentContext() == nullptr);
    ImGuiExt::Init();

    auto destroy_context = scope_exit([]() noexcept {
        safe_call([] {
            if (ImGui::GetCurrentContext() != nullptr) {
                ImGui::DestroyContext();
            }
        });
    });

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2 {1920.0f, 1080.0f};
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = nullptr;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    // Pressing the fullscreen control writes through to the process-global app window
    bool saved_fullscreen = GetApp()->MainWindow.IsFullscreen();
    auto restore_fullscreen = scope_exit([saved_fullscreen]() noexcept { safe_call([saved_fullscreen] { (void)GetApp()->MainWindow.ToggleFullscreen(saved_fullscreen); }); });

    mapper->WorkspaceWindowVisible = true;
    mapper->ContentWindowVisible = true;
    mapper->CritterAnimationsWindowVisible = true;
    mapper->ScriptCallWindowVisible = true;
    mapper->MapListWindowVisible = true;
    mapper->MapWindowVisible = true;
    mapper->HistoryWindowVisible = true;
    mapper->SettingsWindowVisible = true;

    auto map = mapper->LoadMapFromText("ControlsMap", "ControlsMap.fomap", MakeMapText(MakeItemBlock(11, TILE_A, 5, 5) + MakeCritterBlock(12, CRITTER_A, 6, 6)));
    REQUIRE(map);
    mapper->ShowMap(map.as_ptr());
    mapper->SelectAll();

    // The two folded sections of the Controls window hold most of its checkboxes
    constexpr std::array CONTROLS_HEADER_IDS = {"Visibility", "Selection"};

    auto seed_controls_headers = [&] {
        if (auto controls_window = make_nptr(ImGui::FindWindowByName("Controls"))) {
            for (string_view header_id : CONTROLS_HEADER_IDS) {
                controls_window->StateStorage.SetInt(controls_window->GetID(header_id.data(), header_id.data() + header_id.size()), 1);
            }
        }
    };

    // A press is addressed to one panel, so only that panel is drawn around it: a neighbouring window that
    // asks for keyboard focus while it appears would otherwise take over the queued activation
    auto press = [&seed_controls_headers](string_view window_name, string_view control_label, auto&& draw_panel) {
        INFO(window_name);
        INFO(control_label);

        auto draw_frame = [&seed_controls_headers, &draw_panel] {
            ImGui::NewFrame();
            seed_controls_headers();
            REQUIRE_NOTHROW(draw_panel());
            ImGui::Render();
        };

        // ImGui consumes a queued activation when it meets the widget again, so the first frame submits
        // the widget and the second one runs the branch behind it
        draw_frame();
        REQUIRE(ImGuiTestHarness::ActivateItem(window_name, control_label));
        draw_frame();
    };

    // Controls laid out inside a child window belong to the child's id stack, not the panel's
    auto press_child = [&seed_controls_headers](string_view window_name, std::initializer_list<string_view> child_labels, string_view control_label, auto&& draw_panel) {
        INFO(window_name);
        INFO(control_label);

        auto draw_frame = [&seed_controls_headers, &draw_panel] {
            ImGui::NewFrame();
            seed_controls_headers();
            REQUIRE_NOTHROW(draw_panel());
            ImGui::Render();
        };

        draw_frame();
        REQUIRE(ImGuiTestHarness::ActivateChildItem(window_name, child_labels, control_label));
        draw_frame();
    };

    auto draw_controls = [&mapper] { mapper->DrawMapWindowImGui(); };
    auto draw_content = [&mapper] { mapper->DrawContentWindowImGui(); };

    SECTION("TheMapControlsChangeTheShownMap")
    {
        float32_t zoom_before = map->GetSpritesZoomTarget();
        press("Controls", "Zoom in", draw_controls);
        CHECK(map->GetSpritesZoomTarget() > zoom_before);

        press("Controls", "Zoom out", draw_controls);
        press("Controls", "Zoom reset", draw_controls);
        CHECK(map->GetSpritesZoomTarget() == 1.0f);

        bool overlay_before = mapper->MapperHexOverlayVisible;
        press("Controls", "Toggle hex", draw_controls);
        CHECK(mapper->MapperHexOverlayVisible != overlay_before);

        press("Controls", "Time +1h", draw_controls);
        press("Controls", "Time -1h", draw_controls);

        mdir dir_before = mapper->CritterDir;
        press("Controls", strex("Rotate preview dir ({})", dir_before).str(), draw_controls);
        CHECK(mapper->CritterDir != dir_before);

        press("Controls", "Rotate selected critters", draw_controls);

        bool roof_preview_before = mapper->PreviewRoofTiles;
        press("Controls", "Roof preview", draw_controls);
        CHECK(mapper->PreviewRoofTiles != roof_preview_before);

        bool axial_before = mapper->SelectAxialGrid;
        press("Controls", "Axial grid selection", draw_controls);
        CHECK(mapper->SelectAxialGrid != axial_before);

        bool entire_before = mapper->SelectEntireEntity;
        press("Controls", "Select entire entity", draw_controls);
        CHECK(mapper->SelectEntireEntity != entire_before);

        press("Controls", "Scroll check", draw_controls);

        // The folded groups carry the layer toggles the renderer reads
        bool show_items_before = is_enum_set(mapper->VisibleLayers, MapLayers::Items);
        press("Controls", "Items", draw_controls);
        CHECK(is_enum_set(mapper->VisibleLayers, MapLayers::Items) != show_items_before);

        for (string_view layer_label : {"Scenery", "Walls", "Critters", "Tiles", "Roof", "Fast"}) {
            press("Controls", layer_label, draw_controls);
        }
    }

    SECTION("TheToolWindowsRunTheirCommands")
    {
        // The workspace layer buttons rebuild the map, and its tab list is what switches the panel mode
        auto draw_workspace = [&mapper] { mapper->DrawWorkspaceWindowImGui(); };

        bool workspace_items_before = is_enum_set(mapper->VisibleLayers, MapLayers::Items);
        press("Workspace", "Items", draw_workspace);
        CHECK(is_enum_set(mapper->VisibleLayers, MapLayers::Items) != workspace_items_before);

        for (string_view layer_button : {"Scenery", "Walls", "Critters", "Tiles", "Roof", "Fast"}) {
            INFO(layer_button);
            press("Workspace", layer_button, draw_workspace);
        }

        press("Critter Animations", "Play pair", [&mapper] { mapper->DrawCritterAnimationsWindowImGui(); });
        press("Critter Animations", "Play sequence", [&mapper] { mapper->DrawCritterAnimationsWindowImGui(); });
        press("Script Call", "Run script", [&mapper] { mapper->DrawScriptCallWindowImGui(); });
        press("Workspace", "Open Content", [&mapper] { mapper->DrawWorkspaceWindowImGui(); });

        press("Settings", "Reset layout", [&mapper] { mapper->DrawSettingsWindowImGui(); });
        CHECK(mapper->ResetImGuiSettingsRequested);

        press("Settings", "Fullscreen", [&mapper] { mapper->DrawSettingsWindowImGui(); });
        press("Settings", "Fullscreen", [&mapper] { mapper->DrawSettingsWindowImGui(); });
    }

    SECTION("TheMapCommandsLoadSaveAndUnload")
    {
        mapper->SetActivePanelMode(MapperEngine::INT_MODE_LIST);

        // Picking another loaded map out of the browser list is what switches the shown map
        auto other_map = mapper->LoadMapFromText("OtherControlsMap", "OtherControlsMap.fomap", MakeMapText(MakeItemBlock(13, TILE_A, 4, 4)));
        REQUIRE(other_map);
        // The list labels a map by its own name and marks the current one with a leading asterisk
        string other_map_label = string {other_map->GetName()};
        press_child("Content", {"##LoadedMaps"}, other_map_label, draw_content);
        CHECK(mapper->GetCurMap() == other_map.as_ptr());

        // Picking the already-current map is the branch that must not reload it
        press_child("Content", {"##LoadedMaps"}, strex("* {}", other_map_label).str(), draw_content);
        CHECK(mapper->GetCurMap() == other_map.as_ptr());

        // An empty name input makes load and save-as no-ops, which is the branch a user hits first
        press("Content", "Load", draw_content);
        press("Content", "Save As", draw_content);

        // Saving resolves both the file name and the container from the loaded map files, so the result
        // may be a new .fomap or a splice into an existing one - either way the root grows
        auto maps_root_bytes = [&maps_dir] {
            uintmax_t total = 0;

            for (const auto& entry : std::filesystem::directory_iterator {maps_dir}) {
                if (entry.is_regular_file()) {
                    total += entry.file_size();
                }
            }

            return total;
        };

        uintmax_t bytes_before_save = maps_root_bytes();
        press("Content", "Save current", draw_content);
        CHECK(maps_root_bytes() > bytes_before_save);

        press("Content", "Resave all", draw_content);
        press("Content", "New map", draw_content);
        press("Content", "Unload current", draw_content);
    }

    SECTION("TheMessageLogCanBeClearedAndCopied")
    {
        mapper->SetActivePanelMode(MapperEngine::INT_MODE_MESS);
        mapper->AddMess("Control sweep message");
        REQUIRE_FALSE(mapper->MessBox.empty());

        press("Content", "Copy all", draw_content);
        CHECK_FALSE(mapper->MessBox.empty());

        press("Content", "Clear", draw_content);
        CHECK(mapper->MessBox.empty());
    }
}

TEST_CASE("MapperEditorOperations")
{
    auto settings = MakeMapperTestSettings();
    auto mapper = safe_alloc::make_refcounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    mapper->InitIface();

    string body = MakeItemBlock(10, TILE_A, 5, 5) + MakeItemBlock(11, TILE_B, 7, 7) + MakeItemBlock(12, TILE_A, 9, 9);
    auto map = mapper->LoadMapFromText("EditorMap", "EditorMap.fomap", MakeMapText(body));
    REQUIRE(map);

    mapper->ShowMap(map.as_ptr());
    REQUIRE(mapper->GetCurMap());

    SECTION("PanelModesAndCursorModesSwitch")
    {
        for (int32_t mode = MapperEngine::INT_MODE_CUSTOM0; mode < MapperEngine::INT_MODE_COUNT; mode++) {
            REQUIRE_NOTHROW(mapper->SetActivePanelMode(mode));
        }

        REQUIRE_NOTHROW(mapper->SetCurMode(MapperEngine::CUR_MODE_DEFAULT));
        REQUIRE_NOTHROW(mapper->SetCurMode(MapperEngine::CUR_MODE_MOVE_SELECTION));
        REQUIRE_NOTHROW(mapper->SetCurMode(MapperEngine::CUR_MODE_PLACE_OBJECT));
        REQUIRE_NOTHROW(mapper->SetCurMode(MapperEngine::CUR_MODE_DEFAULT));

        REQUIRE_NOTHROW(mapper->ChangeZoom(1.5f));
        REQUIRE_NOTHROW(mapper->ChangeZoom(0.5f));
        REQUIRE_NOTHROW(mapper->ChangeZoom(1.0f));
    }

    SECTION("SelectionBufferRoundTripAndUndo")
    {
        size_t initial_items = map->GetItems().size();
        REQUIRE(initial_items >= 3);

        mapper->SelectAll();
        REQUIRE_NOTHROW(mapper->BufferCopy());
        REQUIRE_NOTHROW(mapper->BufferPaste());

        // Pasting the whole selection at least preserves what was there
        CHECK(map->GetItems().size() >= initial_items);

        mapper->SelectAll();
        REQUIRE_NOTHROW(mapper->BufferCut());

        if (mapper->CanUndo()) {
            CHECK_FALSE(mapper->GetUndoLabel().empty());
            CHECK(mapper->ExecuteUndo());
            CHECK(mapper->CanRedo());
            CHECK_FALSE(mapper->GetRedoLabel().empty());

            // Redo used to escape with "Lookup failed in vec" here, because undo clears the selection and
            // the replayed delete then failed to find its entity in the selection vector
            CHECK(mapper->ExecuteRedo());
            CHECK(mapper->CanUndo());
        }
    }

    SECTION("ConsoleAcceptsCommands")
    {
        // The console is the mapper's scripted command surface; an unknown command must not throw
        mapper->ConsoleStr = "~help";
        REQUIRE_NOTHROW(mapper->ConsoleSubmitCommand());

        mapper->ConsoleStr = "";
        REQUIRE_NOTHROW(mapper->ConsoleSubmitCommand());

        mapper->ConsoleStr = "totally unknown mapper command";
        REQUIRE_NOTHROW(mapper->ConsoleSubmitCommand());
    }

    SECTION("MapLifecycleUnloadsCleanly")
    {
        REQUIRE_NOTHROW(mapper->UnloadMap(map.as_ptr()));
        CHECK_FALSE(mapper->CanUndo());
    }
}

TEST_CASE("MapperViewerAndParticleEditorPanelsDrawHeadlessly")
{
    auto settings = MakeMapperTestSettings();

    // The particle preview sub-editor initialises only when a preview effect is configured and a map is
    // shown; without both it returns immediately and none of its windows ever draw
    BakerTests::OverrideSetting(settings.Mapper.ParticlePreviewEffect, string {"Particles/MapperEditorTest.spk"});

    // Every particle renderer draws a wireframe overlay on top of its geometry when this is on
    BakerTests::OverrideSetting(settings.Render.DrawWireframe, true);
    auto mapper = safe_alloc::make_refcounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    mapper->InitIface();

    REQUIRE(ImGui::GetCurrentContext() == nullptr);
    ImGuiExt::Init();

    auto destroy_context = scope_exit([]() noexcept {
        safe_call([] {
            if (ImGui::GetCurrentContext() != nullptr) {
                ImGui::DestroyContext();
            }
        });
    });

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2 {1280.0f, 720.0f};
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = nullptr;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    // The viewer selects a critter only through a list click or its persisted "last critter" setting, so the
    // setting is seeded here and restored afterwards to leave the user store untouched
    string saved_selected_proto;

    string saved_selected_particle;

    {
        SettingsStorage viewer_settings {"AnimationViewer"};
        saved_selected_proto = viewer_settings.GetString("SelectedProto");
        viewer_settings.SetString("SelectedProto", CRITTER_A);
    }

    {
        SettingsStorage particle_settings {"ParticleViewer"};
        saved_selected_particle = particle_settings.GetString("SelectedPath");
        particle_settings.SetString("SelectedPath", "Particles/MapperEditorTest.spk");
    }

    auto restore_viewer_settings = scope_exit([&saved_selected_proto, &saved_selected_particle]() noexcept {
        safe_call([&saved_selected_proto] {
            SettingsStorage viewer_settings {"AnimationViewer"};
            viewer_settings.SetString("SelectedProto", saved_selected_proto);
        });

        safe_call([&saved_selected_particle] {
            SettingsStorage particle_settings {"ParticleViewer"};
            particle_settings.SetString("SelectedPath", saved_selected_particle);
        });
    });

    AnimationViewer animation_viewer {mapper.as_ptr(), &mapper->SprMngr, &mapper->ResMngr, &mapper->CritterFrames, &mapper->GameTime};
    ParticleViewer particle_viewer {mapper.as_ptr(), &mapper->SprMngr, &mapper->ResMngr};
    ParticleEditorManager particle_editor {mapper.as_ptr()};

    auto preview_map = mapper->LoadMapFromText("PreviewMap", "PreviewMap.fomap", MakeMapText(MakeItemBlock(30, TILE_A, 5, 5)));
    REQUIRE(preview_map);
    mapper->ShowMap(preview_map.as_ptr());

    REQUIRE_NOTHROW(particle_editor.Initialize());

    auto editor_shutdown = scope_exit([&particle_editor]() noexcept { safe_call([&particle_editor] { particle_editor.Shutdown(); }); });

    CHECK_FALSE(animation_viewer.IsVisible());
    animation_viewer.SetVisible(true);
    animation_viewer.SetFillViewport(true);
    CHECK(animation_viewer.IsVisible());

    particle_viewer.SetVisible(true);
    particle_viewer.SetFillViewport(false);
    CHECK(particle_viewer.IsVisible());

    // Initialize opened the preview window and placed the configured effect, so the frames below draw the
    // populated panel; resetting the layout first would close it and leave only the early return covered
    REQUIRE_NOTHROW(particle_editor.OnFocusGained());

    constexpr int32_t VIEWER_FRAMES = 3;

    for (int32_t frame = 0; frame < VIEWER_FRAMES; frame++) {
        ImGui::NewFrame();
        ImGui::LogToBuffer(12);

        REQUIRE_NOTHROW(animation_viewer.Draw());
        REQUIRE_NOTHROW(particle_viewer.Draw());
        REQUIRE_NOTHROW(particle_editor.DrawMenuItems());
        REQUIRE_NOTHROW(particle_editor.DrawWindows());

        ImGui::LogFinish();
        ImGui::Render();
    }

#if FO_SPARK_PARTICLES || FO_EFFEKSEER_PARTICLES
    // Nothing below the resource list runs until a control is pressed, so each press is queued against
    // the drawn window and consumed by the frame that follows it
    constexpr std::array PRESSED_CONTROLS = {"Refresh", "Mouse position", "View center", "Play", "Restart", "Remove"};

    for (string_view control : PRESSED_CONTROLS) {
        INFO(control);
        REQUIRE(ImGuiTestHarness::ActivateItem("Particle Preview", control));

        ImGui::NewFrame();
        REQUIRE_NOTHROW(particle_editor.DrawWindows());
        ImGui::Render();
    }
#endif

    // The viewer lays its controls out in child windows, so each press is addressed to the owning child
    for (string_view toggle : {"Direct draw", "Root", "Name level", "Draw rect", "View rect"}) {
        INFO(toggle);
        REQUIRE(ImGuiTestHarness::ActivateChildItem("Animation Viewer", {"Preview"}, toggle));

        ImGui::NewFrame();
        REQUIRE_NOTHROW(animation_viewer.Draw());
        ImGui::Render();
    }

    for (string_view control : {"Loop", "Idle"}) {
        INFO(control);
        REQUIRE(ImGuiTestHarness::ActivateChildItem("Animation Viewer", {"RightColumn", "Animations"}, control));

        ImGui::NewFrame();
        REQUIRE_NOTHROW(animation_viewer.Draw());
        ImGui::Render();
    }

    // Picking a critter from the list is what loads a preview sprite in the first place
    REQUIRE(ImGuiTestHarness::ActivateChildItem("Animation Viewer", {"Critters"}, CRITTER_A));

    ImGui::NewFrame();
    REQUIRE_NOTHROW(animation_viewer.Draw());
    ImGui::Render();

#if FO_SPARK_PARTICLES
    // The SPARK browser sits behind a menu item, and DrawMenuItems draws into ImGui's implicit window
    // when no real menu bar hosts it - so the item is addressable there
    REQUIRE(ImGuiTestHarness::ActivateItem("Debug##Default", "SPARK particle editor"));

    {
        for (int32_t frame = 0; frame < 3; frame++) {
            ImGui::NewFrame();
            ImGui::LogToBuffer(12);
            REQUIRE_NOTHROW(particle_editor.DrawMenuItems());
            REQUIRE_NOTHROW(particle_editor.DrawWindows());
            ImGui::LogFinish();
            ImGui::Render();
        }

        // Refreshing the list and picking the one asset in it is what opens an editor window
        for (string_view browser_control : {"Refresh", "Particles/MapperEditorTest.spark"}) {
            INFO(browser_control);
            REQUIRE(ImGuiTestHarness::ActivateItem("SPARK Particle Editor", browser_control));

            ImGui::NewFrame();
            ImGui::LogToBuffer(12);
            REQUIRE_NOTHROW(particle_editor.DrawMenuItems());
            REQUIRE_NOTHROW(particle_editor.DrawWindows());
            ImGui::LogFinish();
            ImGui::Render();
        }

        for (int32_t frame = 0; frame < 2; frame++) {
            ImGui::NewFrame();
            ImGui::LogToBuffer(12);
            REQUIRE_NOTHROW(particle_editor.DrawMenuItems());
            REQUIRE_NOTHROW(particle_editor.DrawWindows());
            ImGui::LogFinish();
            ImGui::Render();
        }
    }
#endif

    // Switching away from the previewed map and then unloading it must take the placed sprite with it
    auto second_map = mapper->LoadMapFromText("PreviewMapB", "PreviewMapB.fomap", MakeMapText(MakeItemBlock(31, TILE_A, 6, 6)));
    REQUIRE(second_map);
    REQUIRE_NOTHROW(particle_editor.OnCurrentMapChanging(second_map.as_ptr()));
    mapper->ShowMap(second_map.as_ptr());
    REQUIRE_NOTHROW(particle_editor.OnMapUnloading(preview_map.as_ptr()));
    mapper->UnloadMap(preview_map.as_ptr());

    // Closing the panel through the layout reset must tear the preview down instead of leaving it placed
    REQUIRE_NOTHROW(particle_editor.ResetLayout());

    // Hidden windows must be just as safe to drive as visible ones
    animation_viewer.SetVisible(false);
    particle_viewer.SetVisible(false);

    ImGui::NewFrame();
    REQUIRE_NOTHROW(animation_viewer.Draw());
    REQUIRE_NOTHROW(particle_viewer.Draw());
    REQUIRE_NOTHROW(particle_editor.DrawMenuItems());
    REQUIRE_NOTHROW(particle_editor.DrawWindows());
    ImGui::Render();

    REQUIRE_NOTHROW(animation_viewer.SaveSettings());
    REQUIRE_NOTHROW(particle_viewer.SaveSettings());

#if FO_SPARK_PARTICLES || FO_EFFEKSEER_PARTICLES
    CHECK(ImGui::GetFrameCount() >= VIEWER_FRAMES + numeric_cast<int32_t>(PRESSED_CONTROLS.size()) + 9);
#else
    CHECK(ImGui::GetFrameCount() >= VIEWER_FRAMES + 9);
#endif
}

#if (FO_SPARK_PARTICLES)
TEST_CASE("SparkParticleEditorDrawsHeadlessly")
{
    auto settings = MakeMapperTestSettings();

    string asset_path = "Particles/MapperEditorTest.spark";

    auto raw_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("SparkEditorRawResources");
    raw_source->AddFile(asset_path, vector<uint8_t>(MAPPER_TEST_SPARK_ASSET.begin(), MAPPER_TEST_SPARK_ASSET.end()));

    FileSystem raw_resources;
    raw_resources.AddCustomSource(std::move(raw_source));

    // The editor previews the *baked* particle, so the raw asset is run through the real baker first
    BakerTests::TestRig rig;
    rig.AddSourceFile(asset_path, MAPPER_TEST_SPARK_ASSET, 10);

    ParticleBaker baker(rig.MakeContext());
    baker.BakeFiles(rig.GetAllSourceFiles(), "");

    string baked_path = strex(asset_path).change_file_extension("spk").str();
    REQUIRE(rig.Outputs.contains(baked_path));

    // The SPARK renderer resolves the effect named in the asset plus the whole atlas-variant family the
    // particle runtime registers up front, so all of them are baked into the fixture
    BakerTests::TestRig effect_rig;

    for (string_view effect_name : {"Effects/Particles_ColorAdd.fofx", "Effects/Particles_ColorAddAtlas.fofx", "Effects/Particles_ColorMulAtlas.fofx", "Effects/Particles_ColorSubAtlas.fofx", "Effects/Particles_DistortionAtlas.fofx", "Effects/Particles_DistortionAddAtlas.fofx"}) {
        effect_rig.AddSourceFile(effect_name, MAPPER_TEST_PARTICLE_EFFECT, 10);
    }

    EffectBaker effect_baker(effect_rig.MakeContext());
    effect_baker.BakeFiles(effect_rig.GetAllSourceFiles(), "");
    REQUIRE(effect_rig.Outputs.contains("Effects/Particles_ColorAdd.fofx"));

    auto baked_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("SparkEditorBakedResources");
    baked_source->AddFile(baked_path, rig.Outputs.at(baked_path));

    for (const auto& [output_path, output_data] : effect_rig.Outputs) {
        baked_source->AddFile(output_path, output_data);
    }

    // The renderer resolves its texture relative to the particle asset directory
    baked_source->AddFile("Particles/TestParticle.png", BakerTests::MakeMinimalBakedSprite(4, 4));

    FileSystem baked_resources;
    baked_resources.AddCustomSource(std::move(baked_source));

    REQUIRE(ImGui::GetCurrentContext() == nullptr);
    ImGuiExt::Init();

    auto destroy_context = scope_exit([]() noexcept {
        safe_call([] {
            if (ImGui::GetCurrentContext() != nullptr) {
                ImGui::DestroyContext();
            }
        });
    });

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2 {1280.0f, 720.0f};
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = nullptr;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    int32_t saved_calls = 0;
    SparkParticleEditor editor {asset_path, &settings, &raw_resources, &baked_resources, [&saved_calls](string_view) { saved_calls++; }};

    CHECK(editor.GetAssetPath() == asset_path);
    CHECK_FALSE(editor.IsChanged());

    editor.BringToFront();

    // The window carries a stable "###" id suffix, which is what ImGui matches a window by
    string editor_window_title = strex("###SparkParticleEditor_{}", asset_path).str();

    // Logging auto-expands the object tree so the per-node inspectors run instead of staying collapsed
    constexpr int32_t EDITOR_FRAMES = 4;

    for (int32_t frame = 0; frame < EDITOR_FRAMES; frame++) {
        ImGui::NewFrame();
        ImGui::LogToBuffer(12);

        bool keep_open = true;
        REQUIRE_NOTHROW(keep_open = editor.Draw());
        CHECK(keep_open);

        ImGui::LogFinish();
        ImGui::Render();
    }

    // The editor's own controls live in a child window and only run when something is pressed
    constexpr std::array INFO_CONTROLS = {"Adding mode", "Removing mode", "Naming mode", "Auto replay", "Respawn"};

    for (string_view control : INFO_CONTROLS) {
        INFO(control);
        REQUIRE(ImGuiTestHarness::ActivateChildItem(editor_window_title, {"Info"}, control));

        ImGui::NewFrame();
        ImGui::LogToBuffer(12);
        REQUIRE_NOTHROW((void)editor.Draw());
        ImGui::LogFinish();
        ImGui::Render();
    }

    editor.Hide();

    ImGui::NewFrame();
    REQUIRE_NOTHROW((void)editor.Draw());
    ImGui::Render();

    // Nothing above edits the asset, so the editor must not have written anything back
    CHECK(saved_calls == 0);
}
#endif

TEST_CASE("MapperProcessesInputEventsAndDrawsFrame")
{
    auto settings = MakeMapperTestSettings();
    auto mapper = safe_alloc::make_refcounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    // The key sweep below presses F11, which dumps the atlases, so the directories it writes are cleared
    // once the case is done
    set<string> tex_dumps_before = TexDumpArtifacts::CollectDumpDirs();

    auto remove_tex_dumps = scope_exit([&tex_dumps_before]() noexcept { safe_call([&tex_dumps_before] { TexDumpArtifacts::RemoveNewDumpDirs(tex_dumps_before); }); });

    mapper->InitIface();

    string body = MakeItemBlock(10, TILE_A, 5, 5) + MakeItemBlock(11, TILE_B, 7, 7);
    auto map = mapper->LoadMapFromText("InputMap", "InputMap.fomap", MakeMapText(body));
    REQUIRE(map);
    mapper->ShowMap(map.as_ptr());

    REQUIRE(ImGui::GetCurrentContext() == nullptr);
    ImGuiExt::Init();

    auto destroy_context = scope_exit([]() noexcept {
        safe_call([] {
            if (ImGui::GetCurrentContext() != nullptr) {
                ImGui::DestroyContext();
            }
        });
    });

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2 {1280.0f, 720.0f};
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = nullptr;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    auto make_key_event = [](InputEvent::EventType type, KeyCode code, string_view text = {}) {
        InputEvent ev;
        ev.Type = type;

        if (type == InputEvent::EventType::KeyDownEvent) {
            ev.KeyDown.Code = code;
            ev.KeyDown.Text = string {text};
        }
        else {
            ev.KeyUp.Code = code;
        }

        return ev;
    };

    SECTION("MouseAndWheelEventsAreProcessed")
    {
        ImGui::NewFrame();

        InputEvent move;
        move.Type = InputEvent::EventType::MouseMoveEvent;
        move.MouseMove.MouseX = 100;
        move.MouseMove.MouseY = 100;
        move.MouseMove.DeltaX = 4;
        move.MouseMove.DeltaY = 4;
        REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(move));

        for (MouseButton button : {MouseButton::Left, MouseButton::Right, MouseButton::Middle}) {
            InputEvent down;
            down.Type = InputEvent::EventType::MouseDownEvent;
            down.MouseDown.Button = button;
            REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(down));

            InputEvent up;
            up.Type = InputEvent::EventType::MouseUpEvent;
            up.MouseUp.Button = button;
            REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(up));
        }

        InputEvent wheel;
        wheel.Type = InputEvent::EventType::MouseWheelEvent;
        wheel.MouseWheel.Delta = 3;
        REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(wheel));
        wheel.MouseWheel.Delta = -3;
        REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(wheel));

        REQUIRE_NOTHROW(mapper->ProcessRightMouseInertia());

        ImGui::Render();
    }

    SECTION("SelectingAndDraggingOverTheMapDrivesTheEditorCursor")
    {
        auto mouse_move = [](int32_t x, int32_t y, int32_t dx, int32_t dy) {
            InputEvent ev;
            ev.Type = InputEvent::EventType::MouseMoveEvent;
            ev.MouseMove.MouseX = x;
            ev.MouseMove.MouseY = y;
            ev.MouseMove.DeltaX = dx;
            ev.MouseMove.DeltaY = dy;
            return ev;
        };

        auto mouse_button = [](InputEvent::EventType type, MouseButton button) {
            InputEvent ev;
            ev.Type = type;

            if (type == InputEvent::EventType::MouseDownEvent) {
                ev.MouseDown.Button = button;
            }
            else {
                ev.MouseUp.Button = button;
            }

            return ev;
        };

        // Press over the map, drag across it and release: this is the rubber-band selection path, which is
        // where the cursor modes, the hit tests and the selection move live
        for (int32_t mode : {MapperEngine::INT_MODE_ITEM, MapperEngine::INT_MODE_TILE, MapperEngine::INT_MODE_CRIT}) {
            mapper->SetActivePanelMode(mode);

            ImGui::NewFrame();
            REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(mouse_move(300, 300, 0, 0)));
            REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(mouse_button(InputEvent::EventType::MouseDownEvent, MouseButton::Left)));

            for (int32_t step = 1; step <= 4; step++) {
                REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(mouse_move(300 + step * 20, 300 + step * 15, 20, 15)));
                REQUIRE_NOTHROW(mapper->DrawMapperFrame());
            }

            REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(mouse_button(InputEvent::EventType::MouseUpEvent, MouseButton::Left)));
            REQUIRE_NOTHROW(mapper->DrawMapperFrame());
            ImGui::Render();
        }

        // With everything selected, pressing on a selected entity switches the drag into selection-move
        // instead of rubber-banding, so the press has to land on a hex that actually holds one
        mapper->SelectAll();
        REQUIRE_FALSE(mapper->SelectedEntities.empty());

        // Rather than deriving the screen point from a hex, ask the map itself where something is: the
        // press has to land on a hit-testable entity for the drag to become a selection move
        ptr<MapView> cur_map = mapper->GetCurMap().as_ptr();
        cur_map->InstantScrollTo(mpos {5, 5});
        cur_map->RebuildMap();

        ipos32 hex_screen_pos {};
        bool found_entity = false;

        for (int32_t y = 0; y < 720 && !found_entity; y += 8) {
            for (int32_t x = 0; x < 1280 && !found_entity; x += 8) {
                if (cur_map->GetEntityAtScreen(ipos32 {x, y}, 0, false).first) {
                    hex_screen_pos = ipos32 {x, y};
                    found_entity = true;
                }
            }
        }

        CHECK(found_entity);

        ImGui::NewFrame();
        mapper->SetCurMode(MapperEngine::CUR_MODE_MOVE_SELECTION);
        REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(mouse_move(hex_screen_pos.x, hex_screen_pos.y, 0, 0)));
        REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(mouse_button(InputEvent::EventType::MouseDownEvent, MouseButton::Left)));

        for (int32_t step = 1; step <= 3; step++) {
            REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(mouse_move(hex_screen_pos.x + step * 24, hex_screen_pos.y + step * 18, 24, 18)));
            REQUIRE_NOTHROW(mapper->DrawMapperFrame());
        }

        REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(mouse_button(InputEvent::EventType::MouseUpEvent, MouseButton::Left)));
        REQUIRE_NOTHROW(mapper->DrawMapperFrame());
        ImGui::Render();

        // Placing mode drops a new object under the cursor
        ImGui::NewFrame();
        mapper->SetCurMode(MapperEngine::CUR_MODE_PLACE_OBJECT);
        REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(mouse_move(360, 340, 0, 0)));
        REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(mouse_button(InputEvent::EventType::MouseDownEvent, MouseButton::Left)));
        REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(mouse_button(InputEvent::EventType::MouseUpEvent, MouseButton::Left)));
        REQUIRE_NOTHROW(mapper->DrawMapperFrame());
        ImGui::Render();

        mapper->SetCurMode(MapperEngine::CUR_MODE_DEFAULT);
    }

    SECTION("FullEditorFrameRenders")
    {
        // DrawMapperFrame drives the whole editor frame: map draw, interface events and every panel
        for (int32_t frame = 0; frame < 2; frame++) {
            ImGui::NewFrame();
            REQUIRE_NOTHROW(mapper->DrawMapperFrame());
            ImGui::Render();
        }
    }

    SECTION("FrameRendersAcrossZoomAndScrollStates")
    {
        // A denser map plus zoom/scroll changes walks far more of the map-view draw path than a
        // single static frame does
        string dense;
        for (int32_t i = 0; i < 12; i++) {
            dense += MakeItemBlock(100 + i, TILE_A, 4 + i, 4 + (i % 3));
            dense += MakeItemBlock(200 + i, TILE_B, 6 + i, 8 + (i % 4));
        }

        auto dense_map = mapper->LoadMapFromText("DenseMap", "DenseMap.fomap", MakeMapText(dense));
        REQUIRE(dense_map);
        mapper->ShowMap(dense_map.as_ptr());

        vector<float32_t> zooms {1.0f, 2.0f, 0.5f, 1.0f};

        for (float32_t zoom : zooms) {
            REQUIRE_NOTHROW(mapper->ChangeZoom(zoom));

            InputEvent scroll;
            scroll.Type = InputEvent::EventType::MouseWheelEvent;
            scroll.MouseWheel.Delta = zoom > 1.0f ? 2 : -2;
            REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(scroll));

            ImGui::NewFrame();
            REQUIRE_NOTHROW(mapper->DrawMapperFrame());
            ImGui::Render();
        }

        // Selecting everything makes the frame draw selection decoration on every item too
        mapper->SelectAll();

        ImGui::NewFrame();
        REQUIRE_NOTHROW(mapper->DrawMapperFrame());
        ImGui::Render();

        mapper->UnloadMap(dense_map.as_ptr());
    }

    SECTION("KeyboardHotkeysAreProcessed")
    {
        ImGui::NewFrame();

        // The editor's hotkey tables are wide switch statements, so the whole common key range is walked
        vector<KeyCode> keys {
            KeyCode::Escape,
            KeyCode::Delete,
            KeyCode::Tab,
            KeyCode::Space,
            KeyCode::Up,
            KeyCode::Down,
            KeyCode::Left,
            KeyCode::Right,
            KeyCode::A,
            KeyCode::B,
            KeyCode::C,
            KeyCode::D,
            KeyCode::E,
            KeyCode::F,
            KeyCode::G,
            KeyCode::H,
            KeyCode::L,
            KeyCode::M,
            KeyCode::S,
            KeyCode::V,
            KeyCode::X,
            KeyCode::Z,
            KeyCode::C1,
            KeyCode::C2,
            KeyCode::C3,
            KeyCode::C0,
        };

        for (KeyCode key : keys) {
            REQUIRE_NOTHROW(mapper->HandlePrimaryMapperHotkeys(key, false));
            REQUIRE_NOTHROW(mapper->HandleShiftMapperHotkeys(key, false));
            REQUIRE_NOTHROW(mapper->HandleCtrlMapperHotkeys(key, false));
            REQUIRE_NOTHROW(mapper->UpdateArrowScrollKeys(key, KeyCode::None));
            REQUIRE_NOTHROW(mapper->UpdateArrowScrollKeys(KeyCode::None, key));
        }

        // The function keys drive the panel toggles, the view modes and the window state, and F8/F11/F12
        // reach through to the process-global app window, so the original state is put back afterwards
        bool saved_fullscreen = GetApp()->MainWindow.IsFullscreen();

        auto restore_fullscreen = scope_exit([saved_fullscreen]() noexcept { safe_call([saved_fullscreen] { (void)GetApp()->MainWindow.ToggleFullscreen(saved_fullscreen); }); });

        // With an entity selected and the inspector up, F9, Delete and Escape take their other branches.
        // This runs before the sweep below, which toggles the layer visibility a selection depends on
        mapper->SelectAll();
        REQUIRE_FALSE(mapper->SelectedEntities.empty());
        mapper->InspectorVisible = true;
        REQUIRE_NOTHROW(mapper->HandlePrimaryMapperHotkeys(KeyCode::F9, false));
        REQUIRE_NOTHROW(mapper->HandlePrimaryMapperHotkeys(KeyCode::F9, false));
        REQUIRE_NOTHROW(mapper->HandlePrimaryMapperHotkeys(KeyCode::Escape, false));
        REQUIRE_NOTHROW(mapper->HandlePrimaryMapperHotkeys(KeyCode::Delete, false));

        for (KeyCode function_key : {KeyCode::F1, KeyCode::F2, KeyCode::F3, KeyCode::F4, KeyCode::F5, KeyCode::F6, KeyCode::F7, KeyCode::F8, KeyCode::F9, KeyCode::F10, KeyCode::F11, KeyCode::F12, KeyCode::Add, KeyCode::Subtract}) {
            REQUIRE_NOTHROW(mapper->HandlePrimaryMapperHotkeys(function_key, false));
        }

        // The modifier state comes only from SDL, and a simulated key event does not set it, so these tables stay
        // unreachable from a test
        for (KeyCode shift_key : {KeyCode::F7, KeyCode::F11, KeyCode::C0, KeyCode::Numpad0, KeyCode::Tab}) {
            REQUIRE_NOTHROW(mapper->HandleShiftMapperHotkeys(shift_key, false));
        }

        for (KeyCode ctrl_key : {KeyCode::A, KeyCode::C, KeyCode::X, KeyCode::V, KeyCode::Z, KeyCode::Y, KeyCode::D, KeyCode::B}) {
            REQUIRE_NOTHROW(mapper->HandleCtrlMapperHotkeys(ctrl_key, false));
        }

        // Blocked hotkeys must take the early-out path instead of acting
        REQUIRE_NOTHROW(mapper->HandlePrimaryMapperHotkeys(KeyCode::Delete, true));
        REQUIRE_NOTHROW(mapper->HandleShiftMapperHotkeys(KeyCode::Delete, true));
        REQUIRE_NOTHROW(mapper->HandleCtrlMapperHotkeys(KeyCode::Z, true));

        REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(make_key_event(InputEvent::EventType::KeyDownEvent, KeyCode::A, "a")));
        REQUIRE_NOTHROW(mapper->ProcessMapperInputEvent(make_key_event(InputEvent::EventType::KeyUpEvent, KeyCode::A)));

        REQUIRE_NOTHROW(mapper->HandleMapperConsoleKeyDown(KeyCode::A, "a"));
        REQUIRE_NOTHROW(mapper->HandleMapperConsoleKeyDown(KeyCode::Back, {}));
        REQUIRE_NOTHROW(mapper->HandleMapperConsoleKeyDown(KeyCode::Return, {}));

        ImGui::Render();
    }
}

TEST_CASE("MapViewItemHitTestingUsesActiveItemSprites")
{
    GlobalSettings settings = MakeMapperTestSettings();
    refcount_ptr<MapperEngine> mapper = safe_alloc::make_refcounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    mapper->InitIface();

    nptr<MapView> map = mapper->LoadMapFromText("ItemHitMap", "ItemHitMap.fomap", MakeMapText(MakeItemBlock(10, TILE_A, 8, 8)));
    REQUIRE(map);
    mapper->ShowMap(map.as_ptr());

    ptr<MapView> map_ptr = map.as_ptr();
    map_ptr->InstantScrollTo(mpos {8, 8});
    map_ptr->RebuildMap();

    span<refcount_ptr<ItemHexView>> items = map_ptr->GetItems();
    REQUIRE(items.size() == 1);
    ptr<ItemHexView> item = items.front().as_ptr();
    REQUIRE(item->IsMapSpriteVisible());

    irect32 first_rect = item->GetMapSprite()->GetDrawRect();
    ipos32 first_pos = map_ptr->MapToScreenPos({first_rect.x + first_rect.width / 2, first_rect.y + first_rect.height / 2});
    bool item_egg = false;
    pair<nptr<ItemHexView>, nptr<const MapSprite>> first_hit = map_ptr->GetItemAtScreen(first_pos, item_egg, 0, false);
    CHECK(first_hit.first == item);
    CHECK_FALSE(item_egg);

    map_ptr->MoveItem(item, mpos {9, 8});
    REQUIRE(item->IsMapSpriteVisible());

    irect32 moved_rect = item->GetMapSprite()->GetDrawRect();
    ipos32 moved_pos = map_ptr->MapToScreenPos({moved_rect.x + moved_rect.width / 2, moved_rect.y + moved_rect.height / 2});
    pair<nptr<ItemHexView>, nptr<const MapSprite>> moved_hit = map_ptr->GetItemAtScreen(moved_pos, item_egg, 0, false);
    CHECK(moved_hit.first == item);

    map_ptr->DestroyItem(item);
    pair<nptr<ItemHexView>, nptr<const MapSprite>> removed_hit = map_ptr->GetItemAtScreen(moved_pos, item_egg, 0, false);
    CHECK_FALSE(removed_hit.first);
}

TEST_CASE("MapViewLightingAndViewportOperations")
{
    auto settings = MakeMapperTestSettings();
    auto mapper = safe_alloc::make_refcounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    mapper->InitIface();

    // A light-emitting item drives the light fan machinery: tracing, marking the fan ends and cleaning it up
    // on every rebuild. Without a light source those paths never run, whatever else the map contains
    string light_props = "LightSource = true\nLightIntensity = 50\nLightDistance = 6\nLightFlags = 0\nLightColor = 0xFFFFFFFF";
    string body = MakeItemBlock(10, TILE_A, 8, 8, light_props) + MakeItemBlock(11, TILE_B, 12, 12, light_props) + MakeItemBlock(12, TILE_A, 5, 5);

    // A map critter exercises the critter map view alongside the item paths. Its light properties are
    // client-scoped, so they cannot be authored in the map text and are switched on at runtime below
    body += MakeCritterBlock(20, CRITTER_A, 6, 6);
    body += MakeCritterBlock(21, CRITTER_A, 10, 10);

    auto map = mapper->LoadMapFromText("LightMap", "LightMap.fomap", MakeMapText(body));
    REQUIRE(map);
    mapper->ShowMap(map.as_ptr());

    ptr<MapView> map_ptr = map.as_ptr();

    for (auto& map_critter : map_ptr->GetCritters()) {
        map_critter->SetLightSource(true);
        map_critter->SetLightIntensity(int8_t {50});
        map_critter->SetLightDistance(int16_t {6});
    }

    map_ptr->RebuildMap();

    SECTION("CritterViewsAnimateAndMoveOverFrames")
    {
        // A critter view only advances its animation while the map is processed, so nothing under the
        // per-frame animation walk runs in a test that just builds the map and looks at it
        auto critters = map_ptr->GetCritters();
        REQUIRE_FALSE(critters.empty());

        ptr<CritterHexView> critter = critters.front().as_ptr();

        auto process_frames = [&map_ptr](int32_t count) {
            for (int32_t frame = 0; frame < count; frame++) {
                REQUIRE_NOTHROW(map_ptr->Process());
            }
        };

        // Queued animations play one after another, and appending adds to the queue instead of replacing
        REQUIRE_NOTHROW(critter->AppendAnim(static_cast<CritterStateAnim>(1), static_cast<CritterActionAnim>(1), nullptr));
        REQUIRE_NOTHROW(critter->AppendAnim(static_cast<CritterStateAnim>(1), static_cast<CritterActionAnim>(3), nullptr));
        process_frames(12);

        // The action path picks its own animation for each kind of action
        for (CritterAction action : {CritterAction::Refresh, CritterAction::MoveItem, CritterAction::Knockout, CritterAction::StandUp, CritterAction::Dead, CritterAction::Respawn}) {
            REQUIRE_NOTHROW(critter->Action(action, 0, nullptr, true));
            process_frames(3);
        }

        // Direction changes and the view refresh rebuild the sprite the animation walks over
        for (int32_t dir = 0; dir < 6; dir++) {
            REQUIRE_NOTHROW(critter->ChangeLookDir(mdir {numeric_cast<uint8_t>(dir)}));
            REQUIRE_NOTHROW(critter->ChangeMoveDir(mdir {numeric_cast<uint8_t>(dir)}));
            process_frames(2);
        }

        REQUIRE_NOTHROW(critter->RefreshView(false));
        REQUIRE_NOTHROW(critter->RefreshView(true));
#if FO_ENABLE_3D
        REQUIRE_NOTHROW(critter->RefreshModel());
#endif
        process_frames(4);

        ignore_unused(critter->IsAnimPlaying());
        ignore_unused(critter->IsMoving());

        ipos32 name_pos {};
        ignore_unused(critter->GetNameTextPos(name_pos));
        ignore_unused(critter->GetViewRect());
    }

    SECTION("LightSourcesSurviveRebuildsAndMoves")
    {
        REQUIRE_NOTHROW(map_ptr->RebuildMap());

        auto items = map_ptr->GetItems();
        REQUIRE_FALSE(items.empty());

        // Moving a light source retraces its fan from the new hex
        for (auto& item : items) {
            if (item->GetLightSource()) {
                REQUIRE_NOTHROW(map_ptr->MoveItem(item.as_ptr(), mpos {9, 9}));
                break;
            }
        }

        REQUIRE_NOTHROW(map_ptr->RebuildMap());
    }

    SECTION("CritterViewsProcessAnimationAndVisibility")
    {
        auto critters = map_ptr->GetCritters();
        REQUIRE_FALSE(critters.empty());

        for (auto& cr : critters) {
            ptr<CritterHexView> cr_ptr = cr.as_ptr();

            REQUIRE_NOTHROW(cr_ptr->RefreshView());
            REQUIRE_NOTHROW(cr_ptr->ChangeDir(mdir {2}));
            REQUIRE_NOTHROW(cr_ptr->StopAnim());
            REQUIRE_NOTHROW(cr_ptr->Process());
        }

        REQUIRE_NOTHROW(map_ptr->Process());
        REQUIRE_NOTHROW(map_ptr->RebuildMap());
    }

    SECTION("ScrollingAndZoomingRetraceTheVisibleArea")
    {
        REQUIRE_NOTHROW(map_ptr->ChangeZoom(1.5f, fpos32 {}));
        REQUIRE_NOTHROW(map_ptr->ChangeZoom(0.5f, fpos32 {}));
        REQUIRE_NOTHROW(map_ptr->ChangeZoom(1.0f, fpos32 {}));

        REQUIRE_NOTHROW(map_ptr->InstantScrollTo(mpos {12, 12}));
        REQUIRE_NOTHROW(map_ptr->InstantScrollTo(mpos {2, 2}));

        // Auto-scroll and auto-zoom advance from the frame delta, so the pumping has to let real time pass
        // between frames or the animation steps are all zero-length and never run
        REQUIRE_NOTHROW(map_ptr->ScrollToHex(mpos {8, 8}, ipos16 {}, 5, false));

        for (int32_t i = 0; i < 8; i++) {
            mapper->GameTime.FrameAdvance(true);
            REQUIRE_NOTHROW(map_ptr->Process());
            std::this_thread::sleep_for(std::chrono::milliseconds {12});
        }

        REQUIRE_NOTHROW(map_ptr->ApplyScrollOffset(ipos32 {20, 20}, 5, false));

        for (int32_t i = 0; i < 8; i++) {
            mapper->GameTime.FrameAdvance(true);
            REQUIRE_NOTHROW(map_ptr->Process());
            std::this_thread::sleep_for(std::chrono::milliseconds {12});
        }

        REQUIRE_NOTHROW(map_ptr->SetScreenSize(isize32 {800, 600}));
        REQUIRE_NOTHROW(map_ptr->SetScreenSize(isize32 {1024, 768}));
    }

    SECTION("TheMapDrawsWithLightsAndFog")
    {
        REQUIRE(ImGui::GetCurrentContext() == nullptr);
        ImGuiExt::Init();

        auto destroy_context = scope_exit([]() noexcept {
            safe_call([] {
                if (ImGui::GetCurrentContext() != nullptr) {
                    ImGui::DestroyContext();
                }
            });
        });

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2 {1280.0f, 720.0f};
        io.DeltaTime = 1.0f / 60.0f;
        io.IniFilename = nullptr;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

        map_ptr->AddFog(mpos {8, 8}, DrawOrderType::Light, nullptr);

        for (int32_t frame = 0; frame < 3; frame++) {
            ImGui::NewFrame();
            REQUIRE_NOTHROW(map_ptr->Process());
            REQUIRE_NOTHROW(map_ptr->DrawMap());
            ImGui::Render();
        }
    }
}

TEST_CASE("MapperSavesMapsToADiskMapsRoot")
{
    // Saving resolves the on-disk Maps root from an existing map container, so the fixture needs a real
    // directory with a reference .fomap in it - a memory-only resource set can never reach this path
    auto maps_dir = std::filesystem::temp_directory_path() / std::format("fo_engine_mapper_save_test_{}", std::chrono::steady_clock::now().time_since_epoch().count());
    std::error_code remove_error;
    std::filesystem::remove_all(maps_dir, remove_error);
    std::filesystem::create_directories(maps_dir);

    auto cleanup_maps_dir = scope_exit([maps_dir]() noexcept {
        safe_call([maps_dir] {
            std::error_code ignored;
            std::filesystem::remove_all(maps_dir, ignored);
        });
    });

    string reference_map = MakeMapText(MakeItemBlock(10, TILE_A, 5, 5));
    REQUIRE(fs::write_file((maps_dir / "ReferenceMap.fomap").generic_string(), reference_map));

    auto settings = MakeMapperTestSettings();
    BakerTests::OverrideSetting(settings.Baking.ProtoFileExtensions, vector<string> {"fopro", "fomap"});
    auto pack_config = ConfigFile(strex("[ResourcePack]\nName = MapperSaveTestPack\nInputDirs = {}\n", maps_dir.generic_string()).str());
    settings.ApplyConfigFile(pack_config, "");

    auto mapper = safe_alloc::make_refcounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    mapper->InitIface();

    auto map = mapper->LoadMapFromText("SaveMap", "SaveMap.fomap", MakeMapText(MakeItemBlock(11, TILE_B, 6, 6)));
    REQUIRE(map);
    mapper->ShowMap(map.as_ptr());

    SECTION("SavingIntoASubDirectoryWritesTheFile")
    {
        REQUIRE_NOTHROW(mapper->SaveMapToDir(map.as_ptr(), "Generated", "SavedByTest"));
        CHECK(std::filesystem::exists(maps_dir / "Generated" / "SavedByTest.fomap"));

        // Re-saving the same name overwrites in place instead of appending a second container
        REQUIRE_NOTHROW(mapper->SaveMapToDir(map.as_ptr(), "Generated", "SavedByTest"));
        CHECK(std::filesystem::exists(maps_dir / "Generated" / "SavedByTest.fomap"));
    }

    SECTION("SavingUnderTheReferenceContainerSplicesIntoIt")
    {
        // SaveMap falls back to the first source file's directory when the map has no container of its
        // own, which in this fixture is the working directory - so the sandboxed writer is used instead
        REQUIRE_NOTHROW(mapper->SaveMapToDir(map.as_ptr(), "", "SaveMap"));
        CHECK(std::filesystem::exists(maps_dir / "SaveMap.fomap"));
    }

    SECTION("TheMapListWindowEnumeratesTheDiskMapsRoot")
    {
        REQUIRE(ImGui::GetCurrentContext() == nullptr);
        ImGuiExt::Init();

        auto destroy_context = scope_exit([]() noexcept {
            safe_call([] {
                if (ImGui::GetCurrentContext() != nullptr) {
                    ImGui::DestroyContext();
                }
            });
        });

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2 {1280.0f, 720.0f};
        io.DeltaTime = 1.0f / 60.0f;
        io.IniFilename = nullptr;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

        mapper->MapListWindowVisible = true;

        for (int32_t frame = 0; frame < 3; frame++) {
            ImGui::NewFrame();
            ImGui::LogToBuffer(12);
            REQUIRE_NOTHROW(mapper->DrawMapListWindowImGui());
            ImGui::LogFinish();
            ImGui::Render();
        }
    }

    SECTION("SavingAnUnloadedMapIsRejected")
    {
        auto other = mapper->LoadMapFromText("OtherMap", "OtherMap.fomap", MakeMapText(MakeItemBlock(12, TILE_A, 7, 7)));
        REQUIRE(other);

        // The engine's only owning reference lives in LoadedMaps, so unloading destroys the view outright. Hold
        // an own reference across it, or the rejection below reads freed memory instead of exercising the guard
        ptr<MapView> other_view = other.as_ptr();
        refcount_ptr<MapView> unloaded = refcount_ptr<MapView>::from_addref(other_view.get());

        mapper->UnloadMap(other_view);

        CHECK_THROWS(mapper->SaveMapToDir(unloaded.as_ptr(), "Generated", "NotLoaded"));
    }
}

TEST_CASE("MapperConsoleCommands")
{
    auto settings = MakeMapperTestSettings();
    auto mapper = safe_alloc::make_refcounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    mapper->InitIface();

    SECTION("EmptyAndUnknownInputIsIgnored")
    {
        REQUIRE_NOTHROW(mapper->ParseCommand(""));
        REQUIRE_NOTHROW(mapper->ParseCommand("no leading sigil"));
        REQUIRE_NOTHROW(mapper->ParseCommand("*"));
        REQUIRE_NOTHROW(mapper->ParseCommand("*unknown-subcommand"));
    }

    SECTION("MapLifecycleCommandsRunAgainstAFreshMap")
    {
        REQUIRE_NOTHROW(mapper->ParseCommand("*new"));
        REQUIRE(mapper->GetCurMap());

        REQUIRE_NOTHROW(mapper->ParseCommand("*size 40 40"));
        REQUIRE_NOTHROW(mapper->ParseCommand("*reverse-light"));
        REQUIRE_NOTHROW(mapper->ParseCommand("*merge-items"));
        REQUIRE_NOTHROW(mapper->ParseCommand("*break-items"));
        REQUIRE_NOTHROW(mapper->ParseCommand("*unload"));
    }

    SECTION("LoadAndSaveCommandsReportTheirOutcome")
    {
        // Both sigils validate the name before touching the filesystem
        REQUIRE_NOTHROW(mapper->ParseCommand("~"));
        REQUIRE_NOTHROW(mapper->ParseCommand("~NoSuchMapName"));
        REQUIRE_NOTHROW(mapper->ParseCommand("^"));
        REQUIRE_NOTHROW(mapper->ParseCommand("^SomeName"));
    }

    SECTION("ScriptAndAnimationCommandsHandleAMissingTarget")
    {
        REQUIRE_NOTHROW(mapper->ParseCommand("#"));
        REQUIRE_NOTHROW(mapper->ParseCommand("#NoSuchFunction argument"));
        REQUIRE_NOTHROW(mapper->ParseCommand("@"));
        REQUIRE_NOTHROW(mapper->ParseCommand("@1 2"));
    }

    SECTION("AnimationCommandsWalkTheCrittersOfALoadedMap")
    {
        auto map = mapper->LoadMapFromText("ConsoleMap", "ConsoleMap.fomap", MakeMapText(MakeItemBlock(10, TILE_A, 5, 5)));
        REQUIRE(map);
        mapper->ShowMap(map.as_ptr());

        REQUIRE_NOTHROW(mapper->ParseCommand("@1 2"));
        REQUIRE_NOTHROW(mapper->ParseCommand("@"));
    }

    SECTION("ConsoleTypingReachesTheParser")
    {
        // The console edit buffer accumulates characters and submits on Return
        mapper->HandleMapperConsoleKeyDown(KeyCode::Return, {});

        for (char ch : string {"*new"}) {
            mapper->HandleMapperConsoleKeyDown(KeyCode::A, string {ch});
        }

        REQUIRE_NOTHROW(mapper->HandleMapperConsoleKeyDown(KeyCode::Return, {}));
    }
}

TEST_CASE("MapperDynamicItemAuthoringRejectsUndeliveredOverrides")
{
    auto settings = MakeMapperTestSettings();
    auto mapper = safe_alloc::make_refcounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);
    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });
    auto map = mapper->LoadMapFromText("DynamicAuthoring", "DynamicAuthoring.fomap", MakeMapText(MakeItemBlock(10, SCENERY_A, 8, 8)));
    REQUIRE(map);
    mapper->ShowMap(map.as_ptr());
    auto item = map->GetItems().front().as_ptr();
    REQUIRE_FALSE(item->GetStatic());

    SECTION("Constants and persistent NoSync data reject zero false and empty overrides atomically")
    {
        for (auto [name, value] : {pair<string_view, string_view> {"AuthoringNumber", "0"}, {"AuthoringFlag", "False"}, {"AuthoringText", ""}, {"AuthoringList", ""}, {"AuthoringPrivate", "1"}, {"DrawFlatten", "True"}, {"AuthoringClient", "0"}}) {
            auto prop = item->GetProperties()->GetRegistrar()->FindProperty(name);
            REQUIRE(prop);
            string before = item->GetProperties()->SavePropertyToText(prop);
            CHECK_FALSE(mapper->ApplyEntityPropertyText(item, prop, value));
            CHECK(item->GetProperties()->SavePropertyToText(prop) == before);
            CHECK_FALSE(mapper->IsMapDirty(map));
        }
    }

    SECTION("Equal prototype values are accepted without a dirty change")
    {
        for (auto name : {"AuthoringNumber", "AuthoringClient"}) {
            auto prop = item->GetProperties()->GetRegistrar()->FindProperty(name);
            REQUIRE(prop);
            CHECK(mapper->ApplyEntityPropertyText(item, prop, "20"));
            CHECK_FALSE(mapper->IsMapDirty(map));
        }
    }

    SECTION("Static placements retain client-only configuration overrides")
    {
        auto static_map = mapper->LoadMapFromText("StaticClient", "StaticClient.fomap", MakeMapText(MakeItemBlock(20, SCENERY_A, 12, 12, "Static = True\nAuthoringClient = 0")));
        REQUIRE(static_map);
        auto static_item = static_map->GetItems().front().as_ptr();
        auto prop = static_item->GetProperties()->GetRegistrar()->FindProperty("AuthoringClient");
        REQUIRE(prop);
        CHECK(static_item->GetProperties()->GetValueFast<int32_t>(prop) == 0);
        CHECK_NOTHROW(static_map->SaveToText("StaticClient"));
    }

    SECTION("Existing public owner and server channels remain authorable")
    {
        for (auto name : {"LightDistance", "AuthoringOwner", "AuthoringServer"}) {
            auto prop = item->GetProperties()->GetRegistrar()->FindProperty(name);
            REQUIRE(prop);
            CHECK(mapper->ApplyEntityPropertyText(item, prop, "3"));
            CHECK(item->GetProperties()->SavePropertyToText(prop) == "3");
        }
        CHECK_NOTHROW(map->SaveToText("AllowedChannels"));
    }

    SECTION("Static transition checks all overrides before committing")
    {
        auto static_map = mapper->LoadMapFromText("StaticAuthoring", "StaticAuthoring.fomap", MakeMapText(MakeItemBlock(20, TILE_A, 12, 12, "DrawFlatten = True")));
        REQUIRE(static_map);
        mapper->ShowMap(static_map.as_ptr());
        auto static_item = static_map->GetItems().front().as_ptr();
        CHECK_FALSE(mapper->ApplyEntityPropertyText(static_item, static_item->GetPropertyStatic(), "False"));
        CHECK(static_item->GetStatic());
        CHECK(static_item->GetDrawFlatten());
        CHECK_FALSE(mapper->IsMapDirty(static_map));
    }

    SECTION("A rejected dynamic sibling leaves the primary static item unchanged")
    {
        string body = MakeItemBlock(20, TILE_A, 12, 12) + MakeItemBlock(21, TILE_A, 13, 12, "Static = False");
        auto mixed_map = mapper->LoadMapFromText("MixedAuthoring", "MixedAuthoring.fomap", MakeMapText(body));
        REQUIRE(mixed_map);
        mapper->ShowMap(mixed_map.as_ptr());
        auto primary = mixed_map->GetItem(ident_t {20});
        auto sibling = mixed_map->GetItem(ident_t {21});
        REQUIRE(primary);
        REQUIRE(sibling);
        auto prop = primary->GetProperties()->GetRegistrar()->FindProperty("AuthoringNumber");
        REQUIRE(prop);
        CHECK_FALSE(mapper->ApplyEntityPropertyText(primary, prop, "30"));
        CHECK(primary->GetProperties()->GetValueFast<int32_t>(prop) == 0);
        CHECK(sibling->GetProperties()->GetValueFast<int32_t>(prop) == 0);
        CHECK_FALSE(mapper->IsMapDirty(mixed_map));
    }

    SECTION("Manual source text cannot load an undelivered override")
    {
        CHECK_FALSE(mapper->LoadMapFromText("InvalidDynamic", "InvalidDynamic.fomap", MakeMapText(MakeItemBlock(30, SCENERY_A, 10, 10, "AuthoringNumber = 0"))));
    }

    SECTION("Save and clone creation reject direct raw property bypasses")
    {
        auto prop = item->GetProperties()->GetRegistrar()->FindProperty("AuthoringList");
        REQUIRE(prop);
        item->GetPropertiesForEdit()->ApplyPropertyFromText(prop, "");
        CHECK_THROWS_AS(map->SaveToText("InvalidRaw"), MapLoaderException);
        size_t before_count = map->GetItems().size();
        CHECK_THROWS_AS(map->AddMapperItem(item->GetProtoId(), mpos {10, 10}, item->GetProperties()), MapLoaderException);
        CHECK(map->GetItems().size() == before_count);

        auto cr = map->AddMapperCritter(mapper->Hashes.to_hashed_string(CRITTER_A), mpos {12, 12}, mdir {}, nullptr);
        auto proto = mapper->GetProtoItem(item->GetProtoId());
        REQUIRE(proto);
        CHECK_THROWS_AS(cr->AddMapperInvItem(ident_t {101}, proto, CritterItemSlot::Inventory, item->GetProperties()), MapLoaderException);
        CHECK(cr->GetInvItems().empty());
        CHECK_THROWS_AS(item->AddMapperInnerItem(ident_t {102}, proto, any_t {}, item->GetProperties()), MapLoaderException);
        CHECK(item->GetInnerItems().empty());
    }
}

TEST_CASE("MapperDynamicItemsNeverCoalesceIntoMesh")
{
    auto settings = MakeMapperTestSettings();
    auto mapper = safe_alloc::make_refcounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);
    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    for (auto proto : {TILE_A, TILE_U}) {
        string body = MakeItemBlock(10, proto, 8, 8, "Static = False") + MakeItemBlock(11, proto, 9, 8, "Static = False") + MakeItemBlock(12, proto, 10, 8);
        auto map = mapper->LoadMapFromText("DynamicMerge", "DynamicMerge.fomap", MakeMapText(body));
        REQUIRE(map);
        CHECK(map->GetItems().size() == 3);
        CHECK(mapper->MergeItemsToMultihexMeshes(map.as_ptr()) == 0);

        for (const auto& item : map->GetItems()) {
            CHECK(item->GetMultihexMesh().empty());
        }

        CHECK_NOTHROW(map->SaveToText("DynamicMergeSaved"));
        mapper->UnloadMap(map.as_ptr());
    }
}

TEST_CASE("AnimationViewerRequestsTheConfiguredOpaquePreviewPair")
{
    auto preview_action = static_cast<CritterActionAnim>(GENERATE(509, 600));
    auto preview_state = static_cast<CritterStateAnim>(401);
    auto settings = MakeMapperTestSettings();
    BakerTests::OverrideSetting(settings.Animation.PreviewStateAnim, preview_state);
    BakerTests::OverrideSetting(settings.Animation.PreviewActionAnim, preview_action);
    auto resources = MakeMapperTestResources("Policy.fofrm");
    auto source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("PreviewPolicyResources");
    source->AddFile("Policy.fofrm", BakerTests::MakeMultiFrameBakedSprite(5, 2, 2, 100));
    resources.AddCustomSource(std::move(source));
    auto mapper = safe_alloc::make_refcounted<MapperEngine>(&settings, std::move(resources), &GetApp()->MainWindow);
    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });
    mapper->InitIface();
    auto sprite = mapper->ResMngr.LoadSprite("Policy.fofrm", AtlasType::MapSprites).dyn_cast<SpriteSheet>();
    REQUIRE(sprite);
    uint32_t sprite_id = mapper->AnimAdd(sprite);
    bool requested_preview = false;
    Entity::EventCallbackData resolution;
    resolution.Callback = [&](FuncCallData& call) {
        auto state = *cast_from_void<CritterStateAnim*>(call.ArgsData[1]);
        auto action = *cast_from_void<CritterActionAnim*>(call.ArgsData[2]);
        requested_preview |= state == preview_state && action == preview_action;

        if (state == preview_state && action == static_cast<CritterActionAnim>(509)) {
            *cast_from_void<uint32_t*>(call.ArgsData[3]) = sprite_id;
        }

        return Entity::EventResult::ContinueChain;
    };
    mapper->OnCritterAnimationSprite.Subscribe(std::move(resolution));

    SettingsStorage viewer_settings {"AnimationViewer"};
    string previous_proto = viewer_settings.GetString("SelectedProto");
    viewer_settings.SetString("SelectedProto", CRITTER_A);
    auto restore_settings = scope_exit([&]() noexcept { safe_call([&] { viewer_settings.SetString("SelectedProto", previous_proto); }); });
    REQUIRE(ImGui::GetCurrentContext() == nullptr);
    ImGuiExt::Init();
    auto destroy_context = scope_exit([]() noexcept { ImGui::DestroyContext(); });
    auto& io = ImGui::GetIO();
    io.DisplaySize = ImVec2 {1280.0f, 720.0f};
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = nullptr;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    AnimationViewer viewer {mapper.as_ptr(), &mapper->SprMngr, &mapper->ResMngr, &mapper->CritterFrames, &mapper->GameTime};
    viewer.SetVisible(true);
    viewer.SetFillViewport(true);

    for (int32_t frame = 0; frame < 3; frame++) {
        ImGui::NewFrame();
        REQUIRE_NOTHROW(viewer.Draw());
        ImGui::Render();
    }

    CHECK(requested_preview);
    CHECK(io.MetricsRenderVertices > 0);
}

TEST_CASE("CritterAnimationPreservesProjectSelectionWhileMoving")
{
    auto settings = MakeMapperTestSettings();
    auto resources = MakeMapperTestResources();
    auto source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("AnimationPolicyResources");
    source->AddFile("Policy.fofrm", BakerTests::MakeMultiFrameBakedSprite(5, 2, 2, 100, ipos16 {2, 0}));
    resources.AddCustomSource(std::move(source));
    auto mapper = safe_alloc::make_refcounted<MapperEngine>(&settings, std::move(resources), &GetApp()->MainWindow);
    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });
    mapper->InitIface();
    auto map = mapper->LoadMapFromText("AnimationPolicyMap", "AnimationPolicyMap.fomap", MakeMapText(MakeCritterBlock(20, CRITTER_A, 6, 6)));
    REQUIRE(map);
    mapper->ShowMap(map);
    auto critter = map->GetCritters().front();
    CritterActionAnim observed {};
    auto selected_action = static_cast<CritterActionAnim>(509);
    auto sprite = mapper->ResMngr.LoadSprite("Policy.fofrm", AtlasType::MapSprites).dyn_cast<SpriteSheet>();
    REQUIRE(sprite);

    for (int32_t index = 0; index < sprite->GetFramesCount(); index++) {
        sprite->SetFrame(index, sprite->GetSprite(index)->MakeCopy(), ipos32 {40 + index, -17}, sprite->GetSpriteMotion()[index]);
    }

    uint32_t spr_id = mapper->AnimAdd(sprite);
    auto flags = CritterAnimationFlags::None;
    float32_t normalized_time = 0.0f;
    Entity::EventCallbackData selection;
    selection.Callback = [&flags, &normalized_time, &selected_action](FuncCallData& call) {
        *cast_from_void<CritterStateAnim*>(call.ArgsData[1]) = static_cast<CritterStateAnim>(401);
        *cast_from_void<CritterActionAnim*>(call.ArgsData[2]) = selected_action;
        *cast_from_void<CritterAnimationFlags*>(call.ArgsData[6]) = flags;
        *cast_from_void<float32_t*>(call.ArgsData[7]) = normalized_time;
        return Entity::EventResult::ContinueChain;
    };
    mapper->OnCritterAnimationInit.Subscribe(std::move(selection));
    Entity::EventCallbackData resolution;
    resolution.Callback = [&observed, &spr_id](FuncCallData& call) {
        observed = *cast_from_void<CritterActionAnim*>(call.ArgsData[2]);
        *cast_from_void<uint32_t*>(call.ArgsData[3]) = spr_id;
        return Entity::EventResult::ContinueChain;
    };
    mapper->OnCritterAnimationSprite.Subscribe(std::move(resolution));
    critter->SetMoving(safe_alloc::make_refcounted<MovingContext>(map->GetSize(), uint16_t {60}, vector<mdir> {mdir {0}}, vector<uint16_t> {1}, mapper->GameTime.GetFrameTime(), timespan {}, critter->GetHex(), ipos16 {}, ipos16 {}));
    critter->RefreshView();
    CHECK(observed == static_cast<CritterActionAnim>(509));

    SECTION("FreezeIsIndependentOfLifeStateAndSurvivesProcessingAndDirectionChanges")
    {
        critter->StopMoving();
        flags = CritterAnimationFlags::Freeze;
        normalized_time = 0.5f;
        critter->RefreshView();
        auto frames = mapper->CritterFrames.GetCritterAnimFrames(critter->GetModelName(), static_cast<CritterStateAnim>(401), static_cast<CritterActionAnim>(509), critter->GetDir());
        REQUIRE(frames);
        CHECK(critter->GetMapSprite()->GetSprite() == frames->GetSprite(2));
        critter->Process();
        CHECK(critter->GetMapSprite()->GetSprite() == frames->GetSprite(2));
        normalized_time = 0.75f;
        critter->ChangeLookDir(mdir {1});
        critter->Process();
        CHECK(critter->GetMapSprite()->GetSprite() == frames->GetSprite(3));
        critter->SetCondition(CritterCondition::Dead);
        flags = CritterAnimationFlags::None;
        normalized_time = 0.0f;
        critter->RefreshView();
        CHECK(critter->GetMapSprite()->GetSprite() == frames->GetSprite(0));
    }

    SECTION("DrawOffsetAnchorsSpriteRootWithoutTouchingCumulativeMotion")
    {
        critter->StopMoving();
        flags = CritterAnimationFlags::Freeze;
        normalized_time = 0.5f;
        critter->RefreshView();
        auto frames = mapper->CritterFrames.GetCritterAnimFrames(critter->GetModelName(), static_cast<CritterStateAnim>(401), static_cast<CritterActionAnim>(509), critter->GetDir());
        REQUIRE(frames);
        CHECK(critter->GetMapSprite()->GetSprite() == frames->GetSprite(2));
        CHECK(critter->GetMapSprite()->GetSpriteRootOffset() == ipos32 {-41, 19});
        CHECK(*critter->GetMapSprite()->GetPSprOffset() == ipos32 {6, 0});
    }

    SECTION("FrameOffsetsAnchorBeforeTheFirstProcessAtCurrentDisplacement")
    {
        critter->SetHexOffset(ipos16 {5, 0});
        flags = CritterAnimationFlags::Movement;
        critter->RefreshView();
        CHECK(*critter->GetMapSprite()->GetPSprOffset() == ipos32 {7, 0});
        CHECK(critter->GetHexOffset() == ipos16 {5, 0});
        CHECK(critter->GetMapSprite()->GetSpriteRootOffset() == ipos32 {-39, 19});
    }

    SECTION("AnimationOffsetCompensatesInterpolationWithoutChangingTheSelectedFrame")
    {
        flags = CritterAnimationFlags::Movement;
        critter->RefreshView();
        auto frame = critter->GetMapSprite()->GetSprite();
        CHECK(*critter->GetMapSprite()->GetPSprOffset() == ipos32 {2, 0});
        critter->SetHexOffset(ipos16 {1, 0});
        critter->RefreshView();
        CHECK(critter->GetMapSprite()->GetSprite() == frame);
        CHECK(*critter->GetMapSprite()->GetPSprOffset() == ipos32 {2, 0});
        CHECK(critter->GetHexOffset() == ipos16 {1, 0});
        critter->AddExtraOffs({3, -4});
        CHECK(*critter->GetMapSprite()->GetPSprOffset() == ipos32 {5, -4});
    }

    SECTION("FrameAlignmentProjectsBothAxesAndFloorsNegativeCycles")
    {
        auto aligned = sprite->MakeCopy().dyn_cast<SpriteSheet>();
        REQUIRE(aligned);
        const ipos32 deltas[] = {{2, 1}, {2, 1}, {4, 2}, {}, {}};

        for (int32_t index = 0; index < aligned->GetFramesCount(); index++) {
            aligned->SetFrame(index, aligned->GetSprite(index)->MakeCopy(), aligned->GetSpriteOffset(index), deltas[index]);
        }

        spr_id = mapper->AnimAdd(aligned);
        selected_action = static_cast<CritterActionAnim>(512);
        flags = CritterAnimationFlags::Movement;
        critter->RefreshView();
        auto frames = mapper->CritterFrames.GetCritterAnimFrames(critter->GetModelName(), static_cast<CritterStateAnim>(401), selected_action, critter->GetDir());
        REQUIRE(frames);

        // T=(8,4), cumulative projections 20,40,80; expected positions include complete cycles
        const tuple<ipos16, int32_t, ipos32> cases[] = {
            {{3, 1}, 0, {2, 1}},
            {{3, 2}, 1, {4, 2}},
            {{4, 2}, 1, {4, 2}},
            {{7, 4}, 2, {8, 4}},
            {{11, 5}, 0, {10, 5}},
            {{-1, 0}, 2, {0, 0}},
            {{-1, 2}, 0, {2, 1}},
            {{-8, -4}, 0, {-6, -3}},
            {{-9, -4}, 2, {-8, -4}},
        };

        for (const auto& [position, frame, offset] : cases) {
            critter->SetHexOffset(position);
            critter->RefreshView();
            CHECK(critter->GetMapSprite()->GetSprite() == frames->GetSprite(frame));
            CHECK(*critter->GetMapSprite()->GetPSprOffset() == offset);
            CHECK(critter->GetHexOffset() == position);
        }
    }

    SECTION("FrameAlignmentKeepsTheFirstEquidistantPose")
    {
        flags = CritterAnimationFlags::Movement;
        critter->RefreshView();
        critter->SetHexOffset(ipos16 {3, 0});
        critter->RefreshView();
        // Positions 2 and 4 are equally close; retain the first pose, not the later one
        CHECK(*critter->GetMapSprite()->GetPSprOffset() == ipos32 {2, 0});
        CHECK(critter->GetMapSprite()->GetSpriteRootOffset() == ipos32 {-39, 19});
    }

    SECTION("SheetReplacementRoundsSignedHalfPhaseAwayFromZero")
    {
        flags = CritterAnimationFlags::Movement;
        critter->RefreshView();
        critter->SetHexOffset(ipos16 {5, 0});
        critter->RefreshView();
        auto turned = sprite->MakeCopy().dyn_cast<SpriteSheet>();
        REQUIRE(turned);

        for (int32_t index = 0; index < turned->GetFramesCount(); index++) {
            turned->SetFrame(index, turned->GetSprite(index)->MakeCopy(), turned->GetSpriteOffset(index), index == 0 ? ipos32 {-5, 7} : ipos32 {});
        }

        spr_id = mapper->AnimAdd(turned);
        selected_action = static_cast<CritterActionAnim>(513);
        critter->RefreshView();
        // Half of (-5,7) rounds to (-3,4): anchor=(8,-4), first cumulative=(-5,7).
        CHECK(*critter->GetMapSprite()->GetPSprOffset() == ipos32 {3, 3});
        CHECK(critter->GetHexOffset() == ipos16 {5, 0});
    }

    SECTION("SheetReplacementPreservesNegativeAndFractionalPhase")
    {
        const auto [position, expected] = GENERATE((tuple<ipos16, ipos32> {ipos16 {4, 2}, ipos32 {2, 5}}), (tuple<ipos16, ipos32> {ipos16 {-4, -2}, ipos32 {-6, 1}}), (tuple<ipos16, ipos32> {ipos16 {2, 1}, ipos32 {-2, 6}}));
        auto source_frames = sprite->MakeCopy().dyn_cast<SpriteSheet>();
        REQUIRE(source_frames);
        const ipos32 deltas[] = {{2, 1}, {2, 1}, {4, 2}, {}, {}};

        for (int32_t index = 0; index < source_frames->GetFramesCount(); index++) {
            source_frames->SetFrame(index, source_frames->GetSprite(index)->MakeCopy(), source_frames->GetSpriteOffset(index), deltas[index]);
        }

        spr_id = mapper->AnimAdd(source_frames);
        selected_action = static_cast<CritterActionAnim>(516);
        flags = CritterAnimationFlags::Movement;
        critter->RefreshView();
        critter->SetHexOffset(position);
        critter->RefreshView();
        auto target_frames = sprite->MakeCopy().dyn_cast<SpriteSheet>();
        REQUIRE(target_frames);

        for (int32_t index = 0; index < target_frames->GetFramesCount(); index++) {
            target_frames->SetFrame(index, target_frames->GetSprite(index)->MakeCopy(), target_frames->GetSpriteOffset(index), index == 0 ? ipos32 {-5, 7} : ipos32 {});
        }

        spr_id = mapper->AnimAdd(target_frames);
        selected_action = static_cast<CritterActionAnim>(517);
        critter->RefreshView();
        // T=(8,4): positive/negative halves map to(-3,4), one quarter maps to(-1,2).
        CHECK(*critter->GetMapSprite()->GetPSprOffset() == expected);
        CHECK(critter->GetHexOffset() == position);
    }

    SECTION("SheetReplacementScalesPhaseWithoutOverflowingTheNumerator")
    {
        auto long_cycle = sprite->MakeCopy().dyn_cast<SpriteSheet>();
        REQUIRE(long_cycle);

        for (int32_t index = 0; index < long_cycle->GetFramesCount(); index++) {
            long_cycle->SetFrame(index, long_cycle->GetSprite(index)->MakeCopy(), long_cycle->GetSpriteOffset(index), index == 0 ? ipos32 {1000000000, 0} : ipos32 {});
        }

        spr_id = mapper->AnimAdd(long_cycle);
        selected_action = static_cast<CritterActionAnim>(514);
        flags = CritterAnimationFlags::Movement;
        critter->RefreshView();
        critter->SetHexOffset(ipos16 {32767, 0});
        critter->RefreshView();
        auto turned = sprite->MakeCopy().dyn_cast<SpriteSheet>();
        REQUIRE(turned);

        for (int32_t index = 0; index < turned->GetFramesCount(); index++) {
            turned->SetFrame(index, turned->GetSprite(index)->MakeCopy(), turned->GetSpriteOffset(index), index == 0 ? ipos32 {-2000000001, 2000000001} : ipos32 {});
        }

        spr_id = mapper->AnimAdd(turned);
        selected_action = static_cast<CritterActionAnim>(515);
        critter->RefreshView();
        // 32767/1e9 of the new cycle rounds to (-65534,65534); the naive numerator exceeds int64_t
        CHECK(*critter->GetMapSprite()->GetPSprOffset() == ipos32 {-1999901700, 1999934467});
        CHECK(critter->GetHexOffset() == ipos16 {32767, 0});
    }

    SECTION("MotionReplacementTransfersPhaseBeforeTheNextProcess")
    {
        flags = CritterAnimationFlags::Movement;
        critter->RefreshView();
        critter->SetHexOffset(ipos16 {5, 0});
        critter->RefreshView();
        auto turned = sprite->MakeCopy().dyn_cast<SpriteSheet>();
        REQUIRE(turned);

        for (int32_t index = 0; index < turned->GetFramesCount(); index++) {
            turned->SetFrame(index, turned->GetSprite(index)->MakeCopy(), turned->GetSpriteOffset(index), ipos32 {0, 4});
        }

        spr_id = mapper->AnimAdd(turned);
        selected_action = static_cast<CritterActionAnim>(510);
        critter->ChangeLookDir(mdir {1});
        // Half of (10,0) becomes half of (0,20): anchor=(5,-10), closest cumulative=(0,8).
        CHECK(*critter->GetMapSprite()->GetPSprOffset() == ipos32 {5, -2});
        CHECK(critter->GetHexOffset() == ipos16 {5, 0});
        CHECK(critter->GetMapSprite()->GetSpriteRootOffset() == ipos32 {-40, 19});
    }

    SECTION("ZeroTotalMotionAndFrozenPoseUseSeparateOffsetPolicies")
    {
        auto zero = sprite->MakeCopy().dyn_cast<SpriteSheet>();
        REQUIRE(zero);

        for (int32_t index = 0; index < zero->GetFramesCount(); index++) {
            ipos32 motion = index == 0 ? ipos32 {4, -2} : index == 1 ? ipos32 {-4, 2} : ipos32 {};
            zero->SetFrame(index, zero->GetSprite(index)->MakeCopy(), zero->GetSpriteOffset(index), motion);
        }

        spr_id = mapper->AnimAdd(zero);
        selected_action = static_cast<CritterActionAnim>(511);
        critter->SetHexOffset(ipos16 {5, 3});
        flags = CritterAnimationFlags::Movement;
        critter->RefreshView();
        CHECK(*critter->GetMapSprite()->GetPSprOffset() == ipos32 {5, 3});
        flags = CritterAnimationFlags::Freeze;
        critter->RefreshView();
        CHECK(*critter->GetMapSprite()->GetPSprOffset() == ipos32 {9, 1});
    }

    SECTION("FrozenMotionTakesPrecedenceOverMovementAlignment")
    {
        flags = combine_enum(CritterAnimationFlags::Movement, CritterAnimationFlags::Freeze);
        normalized_time = 0.5f;
        critter->SetHexOffset(ipos16 {5, 0});
        critter->RefreshView();
        CHECK(*critter->GetMapSprite()->GetPSprOffset() == ipos32 {11, 0});
        CHECK(critter->GetMapSprite()->GetSpriteRootOffset() == ipos32 {-41, 19});
    }

    SECTION("StoppedMovementFlagDoesNotApplyThePreviousMotionCorrection")
    {
        flags = CritterAnimationFlags::Movement;
        critter->RefreshView();
        CHECK(*critter->GetMapSprite()->GetPSprOffset() == ipos32 {2, 0});
        critter->StopMoving();
        critter->RefreshView();
        CHECK(*critter->GetMapSprite()->GetPSprOffset() == ipos32(critter->GetHexOffset()));
    }

#if FO_ENABLE_3D
    SECTION("ModelRefreshClearsMotionBeforeAReplacementFrame")
    {
        critter->StopMoving();
        flags = CritterAnimationFlags::Freeze;
        normalized_time = 0.5f;
        critter->RefreshView();
        critter->AddExtraOffs({3, -4});
        auto mspr = critter->GetMapSprite();
        CHECK(*mspr->GetPSprOffset() == ipos32 {9, -4});
        critter->RefreshModel();
        CHECK(*mspr->GetPSprOffset() == ipos32 {3, -4});
    }
#endif

    SECTION("MovementFlagAlignsFramesWithPathForAnArbitraryAction")
    {
        flags = CritterAnimationFlags::Movement;
        critter->RefreshView();
        critter->SetHexOffset(ipos16 {5, 0});
        critter->RefreshView();
        auto driven_offset = *critter->GetMapSprite()->GetPSprOffset();
        flags = CritterAnimationFlags::None;
        critter->RefreshView();
        auto timed_offset = *critter->GetMapSprite()->GetPSprOffset();
        CHECK(driven_offset == ipos32 {4, 0});
        CHECK(timed_offset == ipos32 {7, 0});
    }

    SECTION("QueuedActionKeepsSelectedFrameUntilStopped")
    {
        critter->StopMoving();
        flags = CritterAnimationFlags::Freeze;
        normalized_time = 0.5f;
        critter->AppendAnim(static_cast<CritterStateAnim>(401), static_cast<CritterActionAnim>(509));
        critter->Process();
        CHECK(critter->IsAnimPlaying());
        flags = CritterAnimationFlags::None;
        normalized_time = 0.0f;
        critter->StopAnim();
        CHECK_FALSE(critter->IsAnimPlaying());
        auto frames = mapper->CritterFrames.GetCritterAnimFrames(critter->GetModelName(), static_cast<CritterStateAnim>(401), static_cast<CritterActionAnim>(509), critter->GetDir());
        REQUIRE(frames);
        CHECK(critter->GetMapSprite()->GetSprite() == frames->GetSprite(0));
    }

    SECTION("InvalidSelectionDoesNotLeaveAnUninitializedQueuedAnimation")
    {
        normalized_time = 2.0f;
        CHECK_THROWS(critter->AppendAnim(static_cast<CritterStateAnim>(401), static_cast<CritterActionAnim>(509)));
        REQUIRE_FALSE(critter->IsAnimPlaying());
        normalized_time = 0.0f;
        CHECK_NOTHROW(critter->Process());
    }

    SECTION("MapperAppliesDrawFlattenWithoutExplicitReapply")
    {
        critter->SetDrawFlatten(true);
        CHECK(critter->GetMapSprite()->GetDrawOrder() == DrawOrderType::FlatCritter);
        critter->SetDrawFlatten(false);
        CHECK(critter->GetMapSprite()->GetDrawOrder() == DrawOrderType::Critter);
    }

    SECTION("MapperPropertyTextEditInvokesPostSetterWithCommittedValue")
    {
        bool observed_dead = false;
        critter->GetPropertyCondition()->AddPostSetter([&](nptr<Entity>, ptr<const Property>) { observed_dead = critter->GetCondition() == CritterCondition::Dead; });
        CHECK(mapper->ApplyEntityPropertyText(critter, critter->GetPropertyCondition(), "Dead"));
        CHECK(observed_dead);
    }
}

TEST_CASE("MapperPropertyTextEditHandlesReentrantPostSetters")
{
    auto settings = MakeMapperTestSettings();
    auto mapper = safe_alloc::make_refcounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);
    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });
    auto body = MakeItemBlock(20, TILE_A, 12, 12, "LightDistance = 7") + MakeItemBlock(21, TILE_A, 13, 12, "LightDistance = 9") + MakeCritterBlock(30, CRITTER_A, 8, 8);
    auto map = mapper->LoadMapFromText("PropertyCallbacks", "PropertyCallbacks.fomap", MakeMapText(body));
    REQUIRE(map);
    mapper->ShowMap(map);
    REQUIRE_FALSE(mapper->IsMapDirty(map));

    SECTION("SwitchingTheCurrentMapKeepsTheEditOnItsOriginalMap")
    {
        auto other_map = mapper->LoadMapFromText("OtherPropertyCallbacks", "OtherPropertyCallbacks.fomap", MakeMapText(""));
        REQUIRE(other_map);
        auto critter = map->GetCritters().front();
        bool callback_ran = false;
        critter->GetPropertyCondition()->AddPostSetter([&](nptr<Entity>, ptr<const Property>) {
            callback_ran = true;
            CHECK(mapper->IsMapDirty(map));
            mapper->ShowMap(other_map);
        });
        CHECK(mapper->ApplyEntityPropertyText(critter, critter->GetPropertyCondition(), "Dead"));
        CHECK(callback_ran);
        CHECK(mapper->GetCurMap() == other_map);
        CHECK(mapper->IsMapDirty(map));
        CHECK_FALSE(mapper->IsMapDirty(other_map));
    }

    SECTION("UnloadingTheEditedMapDoesNotLeaveDeadTargetsOrDirtyEntries")
    {
        // Retain the map while checking its terminal state after the callback unloads it
        auto held_map = map.hold_ref();
        ptr<CritterHexView> critter = map->GetCritters().front();
        auto prop = critter->GetPropertyCondition();
        bool callback_ran = false;
        prop->AddPostSetter([&](nptr<Entity>, ptr<const Property>) {
            callback_ran = true;
            CHECK(mapper->IsMapDirty(map));
            mapper->UnloadMap(map);
        });
        CHECK(mapper->ApplyEntityPropertyText(critter, prop, "Dead"));
        CHECK(callback_ran);
        CHECK(held_map->IsDestroyed());
        CHECK_FALSE(mapper->IsMapDirty(held_map));
        CHECK_FALSE(mapper->GetCurMap());
    }

    SECTION("DestroyingAnotherPreparedSiblingSkipsItsLaterCommit")
    {
        auto primary = map->GetItem(ident_t {20});
        auto sibling = map->GetItem(ident_t {21});
        REQUIRE(primary);
        REQUIRE(sibling);
        auto prop = primary->GetProperties()->GetRegistrar()->FindProperty("AuthoringNumber");
        REQUIRE(prop);
        bool callback_ran = false;
        prop->AddPostSetter([&](nptr<Entity> entity, ptr<const Property>) {
            if (entity == primary) {
                callback_ran = true;
                CHECK(mapper->IsMapDirty(map));
                mapper->DeleteEntity(sibling);
            }
        });
        CHECK_NOTHROW(mapper->ApplyEntityPropertyText(primary, prop, "30"));
        CHECK(callback_ran);
        CHECK(primary->GetProperties()->SavePropertyToText(prop) == "30");
        CHECK_FALSE(map->GetItem(ident_t {21}));
    }

    SECTION("InspectorHistoryStaysWithTheOriginalMapAfterCallbackSwitchesMaps")
    {
        auto other_map = mapper->LoadMapFromText("OtherInspectorMap", "OtherInspectorMap.fomap", MakeMapText(""));
        REQUIRE(other_map);
        auto primary = map->GetItem(ident_t {20});
        REQUIRE(primary);
        auto prop = primary->GetProperties()->GetRegistrar()->FindProperty("AuthoringNumber");
        REQUIRE(prop);
        mapper->ShowProps = {prop};
        mapper->InspectorSelectedLine = 3;
        mapper->InspectorSelectedLineInitialValue = "0";
        mapper->InspectorSelectedLineValue = "30";
        prop->AddPostSetter([&](nptr<Entity> entity, ptr<const Property>) {
            if (entity == primary && primary->GetProperties()->SavePropertyToText(prop) == "30") {
                mapper->ShowMap(other_map);
            }
        });
        REQUIRE_NOTHROW(mapper->ApplyInspectorPropertyEdit(primary));
        CHECK(mapper->GetCurMap() == other_map);
        CHECK_FALSE(mapper->CanUndo());
        mapper->ShowMap(map);
        REQUIRE(mapper->CanUndo());
        REQUIRE(mapper->ExecuteUndo());
        CHECK(primary->GetProperties()->SavePropertyToText(prop) == "0");
        REQUIRE(mapper->ExecuteRedo());
        CHECK(mapper->GetCurMap() == other_map);
        CHECK_FALSE(mapper->CanUndo());
    }

    SECTION("InspectorEditCanUnloadItsMapDuringPostSetter")
    {
        auto held_map = map.hold_ref();
        ptr<CritterHexView> critter = map->GetCritters().front();
        auto prop = critter->GetPropertyCondition();
        mapper->ShowProps = {prop};
        mapper->InspectorSelectedLine = 3;
        mapper->InspectorSelectedLineInitialValue = "Alive";
        mapper->InspectorSelectedLineValue = "Dead";
        prop->AddPostSetter([&](nptr<Entity>, ptr<const Property>) { mapper->UnloadMap(map); });
        CHECK_NOTHROW(mapper->ApplyInspectorPropertyEdit(critter));
        CHECK(held_map->IsDestroyed());
        CHECK_FALSE(mapper->GetCurMap());
        CHECK_FALSE(mapper->IsMapDirty(held_map));
    }

    SECTION("UndoCanUnloadTheEditedMapFromItsPostSetter")
    {
        auto held_map = map.hold_ref();
        auto primary = map->GetItem(ident_t {20});
        REQUIRE(primary);
        auto prop = primary->GetProperties()->GetRegistrar()->FindProperty("AuthoringNumber");
        REQUIRE(prop);
        mapper->ShowProps = {prop};
        mapper->InspectorSelectedLine = 3;
        mapper->InspectorSelectedLineInitialValue = "0";
        mapper->InspectorSelectedLineValue = "30";
        mapper->ApplyInspectorPropertyEdit(primary);
        REQUIRE(mapper->CanUndo());
        prop->AddPostSetter([&](nptr<Entity>, ptr<const Property>) { mapper->UnloadMap(map); });
        CHECK_FALSE(mapper->ExecuteUndo());
        CHECK(held_map->IsDestroyed());
        CHECK_FALSE(mapper->GetCurMap());
        CHECK_FALSE(mapper->IsMapDirty(held_map));
    }

    SECTION("RedoCanUnloadTheEditedMapFromItsPostSetter")
    {
        auto held_map = map.hold_ref();
        auto primary = map->GetItem(ident_t {20});
        REQUIRE(primary);
        auto prop = primary->GetProperties()->GetRegistrar()->FindProperty("AuthoringNumber");
        REQUIRE(prop);
        mapper->ShowProps = {prop};
        mapper->InspectorSelectedLine = 3;
        mapper->InspectorSelectedLineInitialValue = "0";
        mapper->InspectorSelectedLineValue = "30";
        mapper->ApplyInspectorPropertyEdit(primary);
        REQUIRE(mapper->ExecuteUndo());
        REQUIRE(mapper->CanRedo());
        prop->AddPostSetter([&](nptr<Entity>, ptr<const Property>) { mapper->UnloadMap(map); });
        CHECK_FALSE(mapper->ExecuteRedo());
        CHECK(held_map->IsDestroyed());
        CHECK_FALSE(mapper->GetCurMap());
        CHECK_FALSE(mapper->IsMapDirty(held_map));
    }
}

FO_END_NAMESPACE
