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
#include "ConfigFile.h"
#include "MapView.h"
#include "Mapper.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

// These tests lock the CURRENT behavior of MapperEngine::MergeItemsToMultihexMeshes (the multihex-mesh
// coalescence run at map load) so that a later O(N) optimization is guarded against regressions. They
// construct a real, headless MapperEngine over self-contained synthetic resources: NullRenderer stubs
// the GPU, minimal baked font sprites satisfy the mapper interface init, and two item protos carry
// MultihexGeneration = SameSibling so their clean tiles coalesce.

namespace
{
    constexpr auto TILE_A = "MapperMergeTileA"; // SameSibling (spatial, adjacency-based merge)
    constexpr auto TILE_B = "MapperMergeTileB"; // SameSibling
    constexpr auto TILE_U = "MapperMergeTileU"; // AnyUnique (position-independent same-proto+same-data merge)

    static auto MakeMapperTestSettings() -> GlobalSettings
    {
        auto settings = GlobalSettings(false);

        settings.ApplyDefaultSettings();
        settings.ApplyAutoSettings();

        BakerTests::ApplySelfContainedClientSettings(settings);

        // The MapperEngine ctor reads GetResourcePacks() to seed the map file system. The maps in these
        // tests are supplied directly via LoadMapFromText, so a single named pack with no input dirs is
        // enough to keep construction from throwing "No information about resource packs found".
        auto pack_config = ConfigFile("TestPacks.fomain", "[ResourcePack]\nName = MapperMergeTestPack\n");
        settings.ApplyConfigFile(pack_config, "");

        return settings;
    }

    static auto MakeMapperScriptBinary(const FileSystem& metadata_resources) -> vector<uint8_t>
    {
        BakerMapperEngine compiler_engine {metadata_resources};

        return BakerTests::CompileInlineScripts(&compiler_engine, "MapperMergeScripts",
            {
                {"Scripts/MapperMergeTest.fos", R"(
namespace MapperMergeTest
{
    [[ModuleInit]]
    void InitMapperMergeTest()
    {
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

    static void AddMinimalFont(BakerTests::MemoryDataSource& source, string_view font_name)
    {
        const auto fofnt_path = strex("Fonts/{}.fofnt", font_name).str();
        const auto image_name = strex("{}.png", font_name).str();
        const auto image_path = strex("Fonts/{}", image_name).str();

        const auto fofnt_text = strex("Version 2\nImage {}\nYAdvance 1\n\nLetter ' '\n  PositionX 1\n  PositionY 1\n  XAdvance 5\n\nEnd\n", image_name).str();

        source.AddFile(fofnt_path, fofnt_text);
        source.AddFile(image_path, BakerTests::MakeMinimalBakedSprite());
    }

    static auto MakeMapperTestResources() -> FileSystem
    {
        const auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();

        auto compiler_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("MapperMergeCompilerResources");
        compiler_source->AddFile("Metadata.fometa-mapper", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_source));

        BakerMapperEngine proto_engine {compiler_resources};
        const auto item_type = proto_engine.Hashes.ToHashedString("Item");

        const auto configure_tile = [](ProtoItem& proto) { proto.SetMultihexGeneration(MultihexGenerationType::SameSibling); };
        const auto configure_unique = [](ProtoItem& proto) { proto.SetMultihexGeneration(MultihexGenerationType::AnyUnique); };

        const vector<pair<string, function<void(ProtoItem&)>>> tile_protos {
            {string(TILE_A), configure_tile},
            {string(TILE_B), configure_tile},
            {string(TILE_U), configure_unique},
        };

        const auto proto_blob = BakerTests::MakeMultiProtoResourceBlob<ProtoItem>(proto_engine, item_type, tile_protos);
        const auto script_blob = MakeMapperScriptBinary(compiler_resources);

        auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("MapperMergeRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-mapper", metadata_blob);
        runtime_source->AddFile("MapperMergeTiles.fopro-bin-mapper", proto_blob);
        runtime_source->AddFile("MapperMergeTest.fos-bin-mapper", script_blob);

        for (const auto* font_name : {"OldDefault", "Numbers", "BigNumbers", "SandNumbers", "Special", "Default", "Thin", "Fat", "Big"}) {
            AddMinimalFont(*runtime_source, font_name);
        }

        FileSystem resources;
        resources.AddCustomSource(std::move(runtime_source));
        return resources;
    }

    static auto MakeMapText(string_view body, int32_t size = 32) -> string
    {
        return strex("[ProtoMap]\nSize = {} {}\nWorkHex = 0 0\n\n{}", size, size, body).str();
    }

    static auto MakeItemBlock(int32_t id, string_view proto, int32_t hx, int32_t hy, string_view extra = "") -> string
    {
        return strex("[Item]\n$Id = {}\n$Proto = {}\nHex = {} {}\n{}\n", id, proto, hx, hy, extra).str();
    }

    static auto HexLess(mpos hex1, mpos hex2) -> bool
    {
        return hex1.y == hex2.y ? hex1.x < hex2.x : hex1.y < hex2.y;
    }

    static auto CollectMeshHexes(ptr<const ItemHexView> item) -> vector<mpos>
    {
        vector<mpos> hexes;
        hexes.emplace_back(item->GetHex());

        for (const auto hex : item->GetMultihexMesh()) {
            hexes.emplace_back(hex);
        }

        std::ranges::sort(hexes, HexLess);
        return hexes;
    }

    static auto CountItemsOfProto(ptr<MapView> map, hstring proto_id) -> int32_t
    {
        int32_t count = 0;

        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == proto_id) {
                count++;
            }
        }

        return count;
    }

    // A surviving item described independently of authoring/merge order: its serialized id, normalized origin,
    // the full hex_less-sorted set of hexes it covers, and the per-item Count value that survived the merge (the
    // merge survivor keeps ITS OWN data, so Count is a fingerprint of which item won a path-dependent race; the
    // id pins WHICH item became the survivor, which matters for AnyUnique where the survivor is the lowest id).
    struct SurvivorDesc
    {
        int64_t Id;
        mpos Origin;
        vector<mpos> Covered;
        int32_t Count;

        auto operator==(const SurvivorDesc& other) const -> bool { return Id == other.Id && Origin == other.Origin && Covered == other.Covered && Count == other.Count; }
    };

    static auto CollectSurvivors(ptr<MapView> map, hstring proto_id) -> vector<SurvivorDesc>
    {
        vector<SurvivorDesc> survivors;

        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == proto_id) {
                survivors.emplace_back(SurvivorDesc {item->GetId().underlying_value(), item->GetHex(), CollectMeshHexes(item), item->GetCount()});
            }
        }

        std::ranges::sort(survivors, [](const SurvivorDesc& a, const SurvivorDesc& b) { return HexLess(a.Origin, b.Origin); });
        return survivors;
    }
}

TEST_CASE("MapperMultihexMeshMerge")
{
    auto settings = MakeMapperTestSettings();
    auto mapper = SafeAlloc::MakeRefCounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    const auto tile_a = mapper->Hashes.ToHashedString(TILE_A);
    const auto tile_b = mapper->Hashes.ToHashedString(TILE_B);

    REQUIRE(mapper->GetProtoItem(tile_a) != nullptr);
    REQUIRE(mapper->GetProtoItem(tile_b) != nullptr);
    REQUIRE(mapper->GetProtoItem(tile_a)->GetMultihexGeneration() == MultihexGenerationType::SameSibling);

    SECTION("Coalesces an adjacent block into one multihex-mesh item")
    {
        // A horizontal run of four clean TileA tiles.
        string body;
        for (int32_t i = 0; i < 4; i++) {
            body += MakeItemBlock(10 + i, TILE_A, 5 + i, 5);
        }

        auto nullable_map = mapper->LoadMapFromText("CoalesceMap", MakeMapText(body));
        REQUIRE(nullable_map != nullptr);
        auto map = nullable_map.as_ptr();

        REQUIRE(CountItemsOfProto(map, tile_a) == 1);

        nptr<const ItemHexView> survivor;
        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == tile_a) {
                survivor = item;
            }
        }
        REQUIRE(survivor != nullptr);
        CHECK(survivor->IsNonEmptyMultihexMesh());

        const auto covered = CollectMeshHexes(survivor.as_ptr());
        REQUIRE(covered.size() == 4);
        CHECK(covered[0] == mpos {5, 5});
        CHECK(covered[1] == mpos {6, 5});
        CHECK(covered[2] == mpos {7, 5});
        CHECK(covered[3] == mpos {8, 5});
    }

    SECTION("Origin is normalized to the hex_less-smallest covered hex and mesh is sorted")
    {
        // Authoring order intentionally does not start at the smallest hex.
        string body;
        body += MakeItemBlock(30, TILE_A, 8, 5);
        body += MakeItemBlock(31, TILE_A, 6, 5);
        body += MakeItemBlock(32, TILE_A, 7, 5);
        body += MakeItemBlock(33, TILE_A, 5, 5);

        auto nullable_map = mapper->LoadMapFromText("NormalizeMap", MakeMapText(body));
        REQUIRE(nullable_map != nullptr);
        auto map = nullable_map.as_ptr();
        REQUIRE(CountItemsOfProto(map, tile_a) == 1);

        nptr<const ItemHexView> survivor;
        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == tile_a) {
                survivor = item;
            }
        }
        REQUIRE(survivor != nullptr);

        const auto covered = CollectMeshHexes(survivor.as_ptr());
        REQUIRE(covered.size() == 4);

        // Origin is the smallest covered hex.
        CHECK(survivor->GetHex() == covered.front());
        CHECK(survivor->GetHex() == mpos {5, 5});

        // The stored mesh (origin + remaining covered hexes) is sorted by hex_less.
        const auto& mesh = survivor->GetMultihexMesh();
        REQUIRE(!mesh.empty());
        CHECK(std::ranges::is_sorted(mesh, HexLess));
        CHECK(HexLess(survivor->GetHex(), mesh.front()));
    }

    SECTION("Idempotent: re-running the merge changes nothing")
    {
        string body;
        for (int32_t i = 0; i < 4; i++) {
            body += MakeItemBlock(40 + i, TILE_A, 5 + i, 5);
        }

        auto nullable_map = mapper->LoadMapFromText("IdempotentMap", MakeMapText(body));
        REQUIRE(nullable_map != nullptr);
        auto map = nullable_map.as_ptr();
        REQUIRE(CountItemsOfProto(map, tile_a) == 1);

        nptr<const ItemHexView> survivor;
        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == tile_a) {
                survivor = item;
            }
        }
        REQUIRE(survivor != nullptr);

        const auto origin_before = survivor->GetHex();
        const auto covered_before = CollectMeshHexes(survivor.as_ptr());

        // LoadMapFromText already runs the merge twice (the second call asserts idempotency); run it once
        // more directly to lock that the public entry point is a fixed point on an already-merged map.
        const auto extra_merges = mapper->MergeItemsToMultihexMeshes(map);
        CHECK(extra_merges == 0);

        REQUIRE(CountItemsOfProto(map, tile_a) == 1);
        CHECK(survivor->GetHex() == origin_before);
        CHECK(CollectMeshHexes(survivor.as_ptr()) == covered_before);
    }

    SECTION("Different protos do not merge")
    {
        string body;
        body += MakeItemBlock(50, TILE_A, 5, 5);
        body += MakeItemBlock(51, TILE_B, 6, 5);

        auto nullable_map = mapper->LoadMapFromText("CrossProtoMap", MakeMapText(body));
        REQUIRE(nullable_map != nullptr);
        auto map = nullable_map.as_ptr();

        CHECK(CountItemsOfProto(map, tile_a) == 1);
        CHECK(CountItemsOfProto(map, tile_b) == 1);

        for (const auto& item : map->GetItems()) {
            CHECK_FALSE(item->IsNonEmptyMultihexMesh());
        }
    }

    SECTION("Tiles with differing modified data are not merged together")
    {
        // Two adjacent TileA tiles carrying DIFFERENT authored Count values. Neither is clean (equal to
        // the proto) and their per-item data differs, so CompareMultihexItemForMerge keeps them apart:
        // a merge between two non-clean items requires identical data.
        string body;
        body += MakeItemBlock(60, TILE_A, 5, 5, "Count = 7");
        body += MakeItemBlock(61, TILE_A, 6, 5, "Count = 9");

        auto nullable_map = mapper->LoadMapFromText("ModifiedPairMap", MakeMapText(body));
        REQUIRE(nullable_map != nullptr);
        auto map = nullable_map.as_ptr();

        REQUIRE(CountItemsOfProto(map, tile_a) == 2);

        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == tile_a) {
                CHECK_FALSE(item->IsNonEmptyMultihexMesh());
            }
        }
    }

    SECTION("Current behavior: a clean tile next to a modified tile still coalesces")
    {
        // Locks the surprising current rule: the merge has a dedicated "first merge to modified items"
        // pass and CompareMultihexItemForMerge allows a clean source to merge into any same-proto target
        // (allow_clean_merge). So one modified tile adjacent to clean tiles is NOT kept separate - the
        // whole run collapses into a single multihex-mesh item and the modified per-item data is dropped.
        string body;
        body += MakeItemBlock(70, TILE_A, 5, 5);
        body += MakeItemBlock(71, TILE_A, 6, 5);
        body += MakeItemBlock(72, TILE_A, 7, 5);
        body += MakeItemBlock(73, TILE_A, 8, 5, "Count = 7");

        auto nullable_map = mapper->LoadMapFromText("CleanPlusModifiedMap", MakeMapText(body));
        REQUIRE(nullable_map != nullptr);
        auto map = nullable_map.as_ptr();

        REQUIRE(CountItemsOfProto(map, tile_a) == 1);

        nptr<const ItemHexView> survivor;
        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == tile_a) {
                survivor = item;
            }
        }
        REQUIRE(survivor != nullptr);
        CHECK(survivor->IsNonEmptyMultihexMesh());
        CHECK(CollectMeshHexes(survivor.as_ptr()).size() == 4);
    }

    SECTION("Two disjoint clusters stay separate")
    {
        string body;
        // Cluster 1 near the top-left.
        body += MakeItemBlock(70, TILE_A, 3, 3);
        body += MakeItemBlock(71, TILE_A, 4, 3);
        // Cluster 2 far away so the two never touch as neighbors.
        body += MakeItemBlock(72, TILE_A, 20, 20);
        body += MakeItemBlock(73, TILE_A, 21, 20);

        auto nullable_map = mapper->LoadMapFromText("DisjointMap", MakeMapText(body));
        REQUIRE(nullable_map != nullptr);
        auto map = nullable_map.as_ptr();

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
        // ADVERSARIAL guard for the O(N) optimization. A naive static connected-component flood-fill of
        // "~-connected same-proto tiles" would also merge this whole line, but it would NOT reproduce the
        // exact PATH-DEPENDENT survivor: because the merge runs a dedicated "modified items first" pass and
        // CompareMultihexItemForMerge(allow_clean_merge) lets a CLEAN source merge into ANY same-proto
        // target, the two modified end tiles (Count 3 and Count 9) are bridged by the clean middle tiles and
        // the WHOLE run collapses into a single multihex mesh whose surviving data is the CLEAN proto data
        // (Count == 0, the proto default) - both authored Count values are dropped. This pins that exact
        // result so the incremental candidate-collection optimization cannot quietly change the survivor.
        string body;
        body += MakeItemBlock(200, TILE_A, 5, 5, "Count = 3");
        body += MakeItemBlock(201, TILE_A, 6, 5);
        body += MakeItemBlock(202, TILE_A, 7, 5);
        body += MakeItemBlock(203, TILE_A, 8, 5, "Count = 9");

        auto nullable_map = mapper->LoadMapFromText("ModChainAsc", MakeMapText(body));
        REQUIRE(nullable_map != nullptr);
        auto map = nullable_map.as_ptr();

        const auto survivors = CollectSurvivors(map, tile_a);
        REQUIRE(survivors.size() == 1);
        CHECK(survivors[0].Origin == mpos {5, 5});
        CHECK(survivors[0].Count == 0);
        CHECK(survivors[0].Covered == vector<mpos> {{5, 5}, {6, 5}, {7, 5}, {8, 5}});
    }

    SECTION("Path-dependent: a modified tile bridged by clean neighbors is absorbed (clean - modified - clean)")
    {
        string body;
        body += MakeItemBlock(210, TILE_A, 5, 5);
        body += MakeItemBlock(211, TILE_A, 6, 5, "Count = 4");
        body += MakeItemBlock(212, TILE_A, 7, 5);

        auto nullable_map = mapper->LoadMapFromText("CleanModClean", MakeMapText(body));
        REQUIRE(nullable_map != nullptr);
        auto map = nullable_map.as_ptr();

        const auto survivors = CollectSurvivors(map, tile_a);
        REQUIRE(survivors.size() == 1);
        CHECK(survivors[0].Origin == mpos {5, 5});
        CHECK(survivors[0].Count == 0);
        CHECK(survivors[0].Covered == vector<mpos> {{5, 5}, {6, 5}, {7, 5}});
    }

    SECTION("Path-dependent: modified..clean..modified collapse is id-order independent")
    {
        // Same chain as above but the modified end tiles own the LOWEST ids and the clean bridge owns the
        // HIGHEST ids, so the per-step best-by-id merge direction differs from the ascending-id case. The
        // collapse and the clean survivor data must match regardless: a single mesh of all four hexes with
        // Count == 0. This catches an optimization that accidentally became sensitive to id authoring order.
        string body;
        body += MakeItemBlock(220, TILE_A, 5, 5, "Count = 3");
        body += MakeItemBlock(223, TILE_A, 6, 5);
        body += MakeItemBlock(222, TILE_A, 7, 5);
        body += MakeItemBlock(221, TILE_A, 8, 5, "Count = 9");

        auto nullable_map = mapper->LoadMapFromText("ModChainModLowIds", MakeMapText(body));
        REQUIRE(nullable_map != nullptr);
        auto map = nullable_map.as_ptr();

        const auto survivors = CollectSurvivors(map, tile_a);
        REQUIRE(survivors.size() == 1);
        CHECK(survivors[0].Origin == mpos {5, 5});
        CHECK(survivors[0].Count == 0);
        CHECK(survivors[0].Covered == vector<mpos> {{5, 5}, {6, 5}, {7, 5}, {8, 5}});
    }

    SECTION("Modified data partitions a run that no clean tile bridges (modified X - X - Y)")
    {
        // modified(X) - modified(X) - modified(Y): the first two share authored data and merge; the third
        // carries different data with no clean tile to bridge it, so it stays separate. A pure
        // proto-adjacency flood-fill would over-merge all three into one mesh - this pins the data-aware
        // partition: two survivors, a mesh of {(5,5),(6,5)} keeping Count 5 and a single {(7,5)} keeping
        // Count 8.
        string body;
        body += MakeItemBlock(230, TILE_A, 5, 5, "Count = 5");
        body += MakeItemBlock(231, TILE_A, 6, 5, "Count = 5");
        body += MakeItemBlock(232, TILE_A, 7, 5, "Count = 8");

        auto nullable_map = mapper->LoadMapFromText("ModXModXModY", MakeMapText(body));
        REQUIRE(nullable_map != nullptr);
        auto map = nullable_map.as_ptr();

        const auto survivors = CollectSurvivors(map, tile_a);
        REQUIRE(survivors.size() == 2);

        CHECK(survivors[0].Origin == mpos {5, 5});
        CHECK(survivors[0].Count == 5);
        CHECK(survivors[0].Covered == vector<mpos> {{5, 5}, {6, 5}});

        CHECK(survivors[1].Origin == mpos {7, 5});
        CHECK(survivors[1].Count == 8);
        CHECK(survivors[1].Covered == vector<mpos> {{7, 5}});
    }

    SECTION("Large solid block coalesces and loads")
    {
        // Solid 24x24 block of clean TileA tiles; pins the result shape for the perf optimization.
        constexpr int32_t block = 24;
        constexpr int32_t origin = 3;

        string body;
        int32_t id = 100;
        for (int32_t y = 0; y < block; y++) {
            for (int32_t x = 0; x < block; x++) {
                body += MakeItemBlock(id++, TILE_A, origin + x, origin + y);
            }
        }

        auto nullable_map = mapper->LoadMapFromText("LargeBlockMap", MakeMapText(body, 64));
        REQUIRE(nullable_map != nullptr);
        auto map = nullable_map.as_ptr();

        // Current behavior: an adjacency-connected block collapses into a single multihex-mesh item.
        REQUIRE(CountItemsOfProto(map, tile_a) == 1);

        nptr<const ItemHexView> survivor;
        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == tile_a) {
                survivor = item;
            }
        }
        REQUIRE(survivor != nullptr);

        const auto covered = CollectMeshHexes(survivor.as_ptr());
        CHECK(covered.size() == numeric_cast<size_t>(block) * block);
        CHECK(survivor->GetHex() == mpos {origin, origin});
    }
}

// AnyUnique is the second multihex-mesh strategy (used by floor tiles/walls in real maps, and the dominant cost
// of the 1000x1000 map load). Unlike SameSibling it is NOT spatial: it merges EVERY same-proto item that has the
// same per-item data (ignoring Hex and MultihexMesh) into one mesh regardless of position, collapsing each
// (proto, data) group into its LOWEST-id member. These sections lock that behavior so the O(N) optimization of
// the AnyUnique coalescence stays behavior-identical.
TEST_CASE("MapperAnyUniqueMeshMerge")
{
    auto settings = MakeMapperTestSettings();
    auto mapper = SafeAlloc::MakeRefCounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    const auto tile_u = mapper->Hashes.ToHashedString(TILE_U);

    REQUIRE(mapper->GetProtoItem(tile_u) != nullptr);
    REQUIRE(mapper->GetProtoItem(tile_u)->GetMultihexGeneration() == MultihexGenerationType::AnyUnique);

    SECTION("Non-adjacent clean tiles merge into one mesh despite no adjacency")
    {
        string body;
        body += MakeItemBlock(300, TILE_U, 3, 3);
        body += MakeItemBlock(301, TILE_U, 20, 20);
        body += MakeItemBlock(302, TILE_U, 5, 25);

        auto nullable_map = mapper->LoadMapFromText("U_NonAdjacentClean", MakeMapText(body));
        REQUIRE(nullable_map != nullptr);
        auto map = nullable_map.as_ptr();

        const auto survivors = CollectSurvivors(map, tile_u);
        REQUIRE(survivors.size() == 1);
        CHECK(survivors[0].Id == 300); // lowest id wins
        CHECK(survivors[0].Origin == mpos {3, 3});
        CHECK(survivors[0].Count == 0);
        // hex_less is y-major: (3,3) then (20,20) then (5,25).
        CHECK(survivors[0].Covered == vector<mpos> {{3, 3}, {20, 20}, {5, 25}});
    }

    SECTION("Scattered tiles partition by per-item data, each group collapsing into its lowest id")
    {
        string body;
        body += MakeItemBlock(310, TILE_U, 2, 2);
        body += MakeItemBlock(311, TILE_U, 10, 2, "Count = 7");
        body += MakeItemBlock(312, TILE_U, 2, 10);
        body += MakeItemBlock(313, TILE_U, 25, 25, "Count = 7");
        body += MakeItemBlock(314, TILE_U, 18, 4, "Count = 9");

        auto nullable_map = mapper->LoadMapFromText("U_MixedScatter", MakeMapText(body));
        REQUIRE(nullable_map != nullptr);
        auto map = nullable_map.as_ptr();

        const auto survivors = CollectSurvivors(map, tile_u);
        REQUIRE(survivors.size() == 3);

        // Clean group (310 + 312).
        CHECK(survivors[0].Id == 310);
        CHECK(survivors[0].Origin == mpos {2, 2});
        CHECK(survivors[0].Count == 0);
        CHECK(survivors[0].Covered == vector<mpos> {{2, 2}, {2, 10}});

        // Count == 7 group (311 + 313).
        CHECK(survivors[1].Id == 311);
        CHECK(survivors[1].Origin == mpos {10, 2});
        CHECK(survivors[1].Count == 7);
        CHECK(survivors[1].Covered == vector<mpos> {{10, 2}, {25, 25}});

        // Lone Count == 9 tile (314).
        CHECK(survivors[2].Id == 314);
        CHECK(survivors[2].Origin == mpos {18, 4});
        CHECK(survivors[2].Count == 9);
        CHECK(survivors[2].Covered == vector<mpos> {{18, 4}});
    }

    SECTION("Survivor is the lowest-id group member regardless of authoring/position order")
    {
        string body;
        body += MakeItemBlock(330, TILE_U, 5, 5);
        body += MakeItemBlock(320, TILE_U, 6, 5);
        body += MakeItemBlock(325, TILE_U, 7, 5);

        auto nullable_map = mapper->LoadMapFromText("U_ShuffledIds", MakeMapText(body));
        REQUIRE(nullable_map != nullptr);
        auto map = nullable_map.as_ptr();

        const auto survivors = CollectSurvivors(map, tile_u);
        REQUIRE(survivors.size() == 1);
        CHECK(survivors[0].Id == 320); // lowest id, even though it was authored second and sits at (6,5)
        CHECK(survivors[0].Origin == mpos {5, 5}); // origin normalized to the hex_less-smallest covered hex
        CHECK(survivors[0].Count == 0);
        CHECK(survivors[0].Covered == vector<mpos> {{5, 5}, {6, 5}, {7, 5}});
    }

    SECTION("Different protos and a SameSibling tile never merge into an AnyUnique mesh")
    {
        const auto tile_a = mapper->Hashes.ToHashedString(TILE_A);

        string body;
        body += MakeItemBlock(340, TILE_U, 3, 3);
        body += MakeItemBlock(341, TILE_A, 4, 3); // SameSibling, adjacent - must not join the AnyUnique mesh
        body += MakeItemBlock(342, TILE_U, 6, 3);

        auto nullable_map = mapper->LoadMapFromText("U_CrossStrategy", MakeMapText(body));
        REQUIRE(nullable_map != nullptr);
        auto map = nullable_map.as_ptr();

        const auto u_survivors = CollectSurvivors(map, tile_u);
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
        body += MakeItemBlock(352, TILE_U, 4, 12, "Count = 4");
        body += MakeItemBlock(353, TILE_U, 20, 1, "Count = 4");

        auto nullable_map = mapper->LoadMapFromText("U_Idempotent", MakeMapText(body));
        REQUIRE(nullable_map != nullptr);
        auto map = nullable_map.as_ptr();

        const auto before = CollectSurvivors(map, tile_u);
        const auto extra_merges = mapper->MergeItemsToMultihexMeshes(map);
        CHECK(extra_merges == 0);
        CHECK(CollectSurvivors(map, tile_u) == before);
    }
}

FO_END_NAMESPACE
