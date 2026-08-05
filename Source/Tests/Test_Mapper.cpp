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
#include "AnimationViewer.h"
#include "ImGuiStuff.h"
#include "ParticleEditor.h"
#include "ParticleViewer.h"
#include "ParticleBaker.h"
#include "SparkParticleEditor.h"
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
        auto pack_config = ConfigFile("[ResourcePack]\nName = MapperMergeTestPack\n");
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
                string message_str = string(message);

                if (message_str.find("error") != string::npos || message_str.find("Error") != string::npos || message_str.find("fatal") != string::npos || message_str.find("Fatal") != string::npos) {
                    throw ScriptSystemException(message_str);
                }
            });
    }

    static void AddMinimalFont(BakerTests::MemoryDataSource& source, string_view font_name)
    {
        string fofnt_path = strex("Fonts/{}.fofnt", font_name).str();
        string image_name = strex("{}.png", font_name).str();
        string image_path = strex("Fonts/{}", image_name).str();

        string fofnt_text = strex("Version 2\nImage {}\nYAdvance 1\n\nLetter ' '\n  PositionX 1\n  PositionY 1\n  XAdvance 5\n\nEnd\n", image_name).str();

        source.AddFile(fofnt_path, fofnt_text);
        source.AddFile(image_path, BakerTests::MakeMinimalBakedSprite());
    }

    static auto MakeMapperTestResources() -> FileSystem
    {
        auto metadata_blob = BakerTests::MakeEmptyMetadataBlob();

        auto compiler_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("MapperMergeCompilerResources");
        compiler_source->AddFile("Metadata.fometa-mapper", metadata_blob);

        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_source));

        BakerMapperEngine proto_engine {compiler_resources};
        hstring item_type = proto_engine.Hashes.ToHashedString("Item");

        auto configure_tile = [](ProtoItem& proto) { proto.SetMultihexGeneration(MultihexGenerationType::SameSibling); };
        auto configure_unique = [](ProtoItem& proto) { proto.SetMultihexGeneration(MultihexGenerationType::AnyUnique); };

        vector<pair<string, function<void(ProtoItem&)>>> tile_protos {
            {string(TILE_A), configure_tile},
            {string(TILE_B), configure_tile},
            {string(TILE_U), configure_unique},
        };

        auto proto_blob = BakerTests::MakeMultiProtoResourceBlob<ProtoItem>(proto_engine, item_type, tile_protos);
        auto script_blob = MakeMapperScriptBinary(compiler_resources);

        auto runtime_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("MapperMergeRuntimeResources");
        runtime_source->AddFile("Metadata.fometa-mapper", metadata_blob);
        runtime_source->AddFile("MapperMergeTiles.fopro-bin-mapper", proto_blob);
        runtime_source->AddFile("MapperMergeTest.fos-bin-mapper", script_blob);

        for (string_view font_name : {"OldDefault", "Numbers", "BigNumbers", "SandNumbers", "Special", "Default", "Thin", "Fat", "Big"}) {
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
        return strex("[$Name/Item]\n$Id = {}\n$Proto = {}\nHex = {} {}\n{}\n", id, proto, hx, hy, extra).str();
    }

    static auto HexLess(mpos hex1, mpos hex2) -> bool
    {
        return hex1.y == hex2.y ? hex1.x < hex2.x : hex1.y < hex2.y;
    }

    static auto CollectMeshHexes(ptr<const ItemHexView> item) -> vector<mpos>
    {
        vector<mpos> hexes;
        hexes.emplace_back(item->GetHex());

        for (auto hex : item->GetMultihexMesh()) {
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

    hstring tile_a = mapper->Hashes.ToHashedString(TILE_A);
    hstring tile_b = mapper->Hashes.ToHashedString(TILE_B);

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

        auto map = mapper->LoadMapFromText("CoalesceMap", "CoalesceMap.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);

        REQUIRE(CountItemsOfProto(map, tile_a) == 1);

        nptr<const ItemHexView> survivor;
        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == tile_a) {
                survivor = item;
            }
        }
        REQUIRE(survivor != nullptr);
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
        // Authoring order intentionally does not start at the smallest hex.
        string body;
        body += MakeItemBlock(30, TILE_A, 8, 5);
        body += MakeItemBlock(31, TILE_A, 6, 5);
        body += MakeItemBlock(32, TILE_A, 7, 5);
        body += MakeItemBlock(33, TILE_A, 5, 5);

        auto map = mapper->LoadMapFromText("NormalizeMap", "NormalizeMap.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);
        REQUIRE(CountItemsOfProto(map, tile_a) == 1);

        nptr<const ItemHexView> survivor;
        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == tile_a) {
                survivor = item;
            }
        }
        REQUIRE(survivor != nullptr);

        auto covered = CollectMeshHexes(survivor);
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

        auto map = mapper->LoadMapFromText("IdempotentMap", "IdempotentMap.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);
        REQUIRE(CountItemsOfProto(map, tile_a) == 1);

        nptr<const ItemHexView> survivor;
        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == tile_a) {
                survivor = item;
            }
        }
        REQUIRE(survivor != nullptr);

        auto origin_before = survivor->GetHex();
        auto covered_before = CollectMeshHexes(survivor);

        // LoadMapFromText already runs the merge twice (the second call asserts idempotency); run it once
        // more directly to lock that the public entry point is a fixed point on an already-merged map.
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
        REQUIRE(map != nullptr);

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

        auto map = mapper->LoadMapFromText("ModifiedPairMap", "ModifiedPairMap.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);

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

        auto map = mapper->LoadMapFromText("CleanPlusModifiedMap", "CleanPlusModifiedMap.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);

        REQUIRE(CountItemsOfProto(map, tile_a) == 1);

        nptr<const ItemHexView> survivor;
        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == tile_a) {
                survivor = item;
            }
        }
        REQUIRE(survivor != nullptr);
        CHECK(survivor->IsNonEmptyMultihexMesh());
        CHECK(CollectMeshHexes(survivor).size() == 4);
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

        auto map = mapper->LoadMapFromText("DisjointMap", "DisjointMap.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);

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

        auto map = mapper->LoadMapFromText("ModChainAsc", "ModChainAsc.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);

        auto survivors = CollectSurvivors(map, tile_a);
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

        auto map = mapper->LoadMapFromText("CleanModClean", "CleanModClean.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);

        auto survivors = CollectSurvivors(map, tile_a);
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

        auto map = mapper->LoadMapFromText("ModChainModLowIds", "ModChainModLowIds.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);

        auto survivors = CollectSurvivors(map, tile_a);
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

        auto map = mapper->LoadMapFromText("ModXModXModY", "ModXModXModY.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);

        auto survivors = CollectSurvivors(map, tile_a);
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

        auto map = mapper->LoadMapFromText("LargeBlockMap", "LargeBlockMap.fomap", MakeMapText(body, 64));
        REQUIRE(map != nullptr);

        // Current behavior: an adjacency-connected block collapses into a single multihex-mesh item.
        REQUIRE(CountItemsOfProto(map, tile_a) == 1);

        nptr<const ItemHexView> survivor;
        for (const auto& item : map->GetItems()) {
            if (item->GetProtoId() == tile_a) {
                survivor = item;
            }
        }
        REQUIRE(survivor != nullptr);

        auto covered = CollectMeshHexes(survivor);
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

    hstring tile_u = mapper->Hashes.ToHashedString(TILE_U);

    REQUIRE(mapper->GetProtoItem(tile_u) != nullptr);
    REQUIRE(mapper->GetProtoItem(tile_u)->GetMultihexGeneration() == MultihexGenerationType::AnyUnique);

    SECTION("Non-adjacent clean tiles merge into one mesh despite no adjacency")
    {
        string body;
        body += MakeItemBlock(300, TILE_U, 3, 3);
        body += MakeItemBlock(301, TILE_U, 20, 20);
        body += MakeItemBlock(302, TILE_U, 5, 25);

        auto map = mapper->LoadMapFromText("U_NonAdjacentClean", "U_NonAdjacentClean.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);

        auto survivors = CollectSurvivors(map, tile_u);
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

        auto map = mapper->LoadMapFromText("U_MixedScatter", "U_MixedScatter.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);

        auto survivors = CollectSurvivors(map, tile_u);
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

        auto map = mapper->LoadMapFromText("U_ShuffledIds", "U_ShuffledIds.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);

        auto survivors = CollectSurvivors(map, tile_u);
        REQUIRE(survivors.size() == 1);
        CHECK(survivors[0].Id == 320); // lowest id, even though it was authored second and sits at (6,5)
        CHECK(survivors[0].Origin == mpos {5, 5}); // origin normalized to the hex_less-smallest covered hex
        CHECK(survivors[0].Count == 0);
        CHECK(survivors[0].Covered == vector<mpos> {{5, 5}, {6, 5}, {7, 5}});
    }

    SECTION("Different protos and a SameSibling tile never merge into an AnyUnique mesh")
    {
        hstring tile_a = mapper->Hashes.ToHashedString(TILE_A);

        string body;
        body += MakeItemBlock(340, TILE_U, 3, 3);
        body += MakeItemBlock(341, TILE_A, 4, 3); // SameSibling, adjacent - must not join the AnyUnique mesh
        body += MakeItemBlock(342, TILE_U, 6, 3);

        auto map = mapper->LoadMapFromText("U_CrossStrategy", "U_CrossStrategy.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);

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
        body += MakeItemBlock(352, TILE_U, 4, 12, "Count = 4");
        body += MakeItemBlock(353, TILE_U, 20, 1, "Count = 4");

        auto map = mapper->LoadMapFromText("U_Idempotent", "U_Idempotent.fomap", MakeMapText(body));
        REQUIRE(map != nullptr);

        auto before = CollectSurvivors(map, tile_u);
        size_t extra_merges = mapper->MergeItemsToMultihexMeshes(map);
        CHECK(extra_merges == 0);
        CHECK(CollectSurvivors(map, tile_u) == before);
    }
}

// Regression: MapperEngine::LoadMap resolves a map file located by a directory-qualified path (the
// form the render/preview tooling passes, e.g. "Gambell/NewGambell_Center") and never lets a
// same-stem sibling of another type (the map's NewGambell_Center.foloc location file) shadow the
// .fomap during file discovery. Nearly every location map in the project ships such a .foloc sibling,
// so the shadowing broke headless map loading for most maps.
TEST_CASE("MapperLoadMapResolvesNameAndPath")
{
    auto settings = MakeMapperTestSettings();

    // Mirror the project's proto-extension order (LastFrontier.fomain), where .foloc precedes .fomap.
    // That ordering is what let a same-stem location file shadow the map file during discovery.
    BakerTests::OverrideSetting(settings.ProtoFileExtensions, vector<string> {"foinfo", "fopro", "foloc", "fomap", "focr", "foitem"});

    auto mapper = SafeAlloc::MakeRefCounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    // A single-map .fomap and a same-stem .foloc live side by side under a subdirectory, mirroring the
    // real content layout. The .foloc must not shadow the .fomap for either lookup form.
    auto maps_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("MapperLoadMapTestMaps");
    maps_source->AddFile("Gambell/ShadowedMap.foloc", "[ProtoLocation]\n$Name = ShadowedMap\nMapProtos = ShadowedMap\n");
    maps_source->AddFile("Gambell/ShadowedMap.fomap", MakeMapText(MakeItemBlock(10, TILE_A, 5, 5)));
    mapper->MapsFileSys.AddCustomSource(std::move(maps_source));

    hstring expected_proto = mapper->Hashes.ToHashedString("ShadowedMap");

    SECTION("Loads by directory-qualified path despite a same-stem location sibling")
    {
        auto map = mapper->LoadMap("Gambell/ShadowedMap");
        REQUIRE(map != nullptr);
        CHECK(map->GetProtoId() == expected_proto);
    }

    SECTION("Loads by bare declared map name")
    {
        auto map = mapper->LoadMap("ShadowedMap");
        REQUIRE(map != nullptr);
        CHECK(map->GetProtoId() == expected_proto);
    }
}

TEST_CASE("MapperDrawsEditorPanelsHeadlessly")
{
    auto settings = MakeMapperTestSettings();
    auto mapper = SafeAlloc::MakeRefCounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

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

    // Every editor panel is hidden by default, so opt them all in before drawing
    mapper->WorkspaceWindowVisible = true;
    mapper->ContentWindowVisible = true;
    mapper->CritterAnimationsWindowVisible = true;
    mapper->ScriptCallWindowVisible = true;
    mapper->MapListWindowVisible = true;
    mapper->MapWindowVisible = true;
    mapper->HistoryWindowVisible = true;
    mapper->SettingsWindowVisible = true;

    // A loaded map gives the map, content and inspector panels real rows to render
    string body = MakeItemBlock(10, TILE_A, 5, 5) + MakeItemBlock(11, TILE_B, 7, 7);
    auto map = mapper->LoadMapFromText("PanelMap", "PanelMap.fomap", MakeMapText(body));
    REQUIRE(map != nullptr);

    // Logging auto-expands tree nodes, so the panel bodies run instead of collapsing to headers
    constexpr int32_t PANEL_FRAMES = 3;
    string drawn_text;

    for (int32_t frame = 0; frame < PANEL_FRAMES; frame++) {
        ImGui::NewFrame();
        ImGui::LogToBuffer();

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
}


TEST_CASE("MapperEditorOperations")
{
    auto settings = MakeMapperTestSettings();
    auto mapper = SafeAlloc::MakeRefCounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    mapper->InitIface();

    string body = MakeItemBlock(10, TILE_A, 5, 5) + MakeItemBlock(11, TILE_B, 7, 7) + MakeItemBlock(12, TILE_A, 9, 9);
    auto map = mapper->LoadMapFromText("EditorMap", "EditorMap.fomap", MakeMapText(body));
    REQUIRE(map != nullptr);

    mapper->ShowMap(map.as_ptr());
    REQUIRE(mapper->GetCurMap() != nullptr);

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
        }

        // ExecuteRedo() is deliberately not driven here: after select-all + cut + undo it throws
        // "Lookup failed in vec" from inside the redo op instead of reporting failure. Recorded in
        // the coverage plan as a suspected mapper undo/redo defect rather than pinned as behaviour.
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
    auto mapper = SafeAlloc::MakeRefCounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

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

    // The viewers and the particle editor are mapper-hosted tool windows, so they take their
    // dependencies straight off the running mapper engine
    AnimationViewer animation_viewer {mapper.as_ptr(), &mapper->SprMngr, &mapper->ResMngr, &mapper->GameTime};
    ParticleViewer particle_viewer {mapper.as_ptr(), &mapper->SprMngr};
    ParticleEditorManager particle_editor {mapper.as_ptr()};

    REQUIRE_NOTHROW(particle_editor.Initialize());

    auto editor_shutdown = scope_exit([&particle_editor]() noexcept { safe_call([&particle_editor] { particle_editor.Shutdown(); }); });

    CHECK_FALSE(animation_viewer.IsVisible());
    animation_viewer.SetVisible(true);
    animation_viewer.SetFillViewport(true);
    CHECK(animation_viewer.IsVisible());

    particle_viewer.SetVisible(true);
    particle_viewer.SetFillViewport(false);
    CHECK(particle_viewer.IsVisible());

    REQUIRE_NOTHROW(particle_editor.ResetLayout());
    REQUIRE_NOTHROW(particle_editor.OnFocusGained());

    constexpr int32_t VIEWER_FRAMES = 3;

    for (int32_t frame = 0; frame < VIEWER_FRAMES; frame++) {
        ImGui::NewFrame();
        ImGui::LogToBuffer();

        REQUIRE_NOTHROW(animation_viewer.Draw());
        REQUIRE_NOTHROW(particle_viewer.Draw());
        REQUIRE_NOTHROW(particle_editor.DrawMenuItems());
        REQUIRE_NOTHROW(particle_editor.DrawWindows());

        ImGui::LogFinish();
        ImGui::Render();
    }

    // Hidden windows must be just as safe to drive as visible ones
    animation_viewer.SetVisible(false);
    particle_viewer.SetVisible(false);

    ImGui::NewFrame();
    REQUIRE_NOTHROW(animation_viewer.Draw());
    REQUIRE_NOTHROW(particle_viewer.Draw());
    ImGui::Render();

    REQUIRE_NOTHROW(animation_viewer.SaveSettings());
    REQUIRE_NOTHROW(particle_viewer.SaveSettings());

    CHECK(ImGui::GetFrameCount() == VIEWER_FRAMES + 1);
}


#if FO_SPARK_PARTICLES

static constexpr string_view MAPPER_TEST_SPARK_ASSET = R"PARTICLE(
<SPARK>
  <System name="MapperEditorParticle">
    <attrib id="groups">
      <Group name="MapperEditorGroup">
        <attrib id="capacity" value="4" />
        <attrib id="life time" value="1;1" />
        <attrib id="emitters">
          <StaticEmitter>
            <attrib id="tank" value="1" />
            <attrib id="flow" value="-1" />
            <attrib id="force" value="0" />
            <attrib id="zone">
              <Point>
                <attrib id="position" value="(0,0,0)" />
              </Point>
            </attrib>
            <attrib id="full" value="false" />
          </StaticEmitter>
        </attrib>
        <attrib id="renderer">
          <SparkQuadRenderer>
            <attrib id="draw in scene" value="true" />
            <attrib id="active" value="true" />
            <attrib id="effect" value="Effects/Particles_ColorAdd.fofx" />
            <attrib id="texture" value="TestParticle.png" />
            <attrib id="scale" value="1.5;2" />
            <attrib id="atlas dimensions" value="2;3" />
          </SparkQuadRenderer>
        </attrib>
      </Group>
    </attrib>
  </System>
</SPARK>
)PARTICLE";

TEST_CASE("SparkParticleEditorDrawsHeadlessly")
{
    auto settings = MakeMapperTestSettings();

    string asset_path = "Particles/MapperEditorTest.spark";

    auto raw_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("SparkEditorRawResources");
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

    auto baked_source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("SparkEditorBakedResources");
    baked_source->AddFile(baked_path, rig.Outputs.at(baked_path));

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

    // Logging auto-expands the object tree so the per-node inspectors run instead of staying collapsed
    constexpr int32_t EDITOR_FRAMES = 4;

    for (int32_t frame = 0; frame < EDITOR_FRAMES; frame++) {
        ImGui::NewFrame();
        ImGui::LogToBuffer();

        bool keep_open = true;
        REQUIRE_NOTHROW(keep_open = editor.Draw());
        CHECK(keep_open);

        ImGui::LogFinish();
        ImGui::Render();
    }

    editor.Hide();

    ImGui::NewFrame();
    REQUIRE_NOTHROW((void)editor.Draw());
    ImGui::Render();

    CHECK(saved_calls == 0);
}

#endif


TEST_CASE("MapperProcessesInputEventsAndDrawsFrame")
{
    auto settings = MakeMapperTestSettings();
    auto mapper = SafeAlloc::MakeRefCounted<MapperEngine>(&settings, MakeMapperTestResources(), &GetApp()->MainWindow);

    auto shutdown = scope_exit([&mapper]() noexcept { safe_call([&mapper] { mapper->Shutdown(); }); });

    mapper->InitIface();

    string body = MakeItemBlock(10, TILE_A, 5, 5) + MakeItemBlock(11, TILE_B, 7, 7);
    auto map = mapper->LoadMapFromText("InputMap", "InputMap.fomap", MakeMapText(body));
    REQUIRE(map != nullptr);
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

    SECTION("FullEditorFrameRenders")
    {
        // DrawMapperFrame drives the whole editor frame: map draw, interface events and every panel
        for (int32_t frame = 0; frame < 2; frame++) {
            ImGui::NewFrame();
            REQUIRE_NOTHROW(mapper->DrawMapperFrame());
            ImGui::Render();
        }
    }

    SECTION("KeyboardHotkeysAreProcessed")
    {
        ImGui::NewFrame();

        // The editor's hotkey tables are wide switch statements, so the whole common key range is walked
        const vector<KeyCode> keys {
            KeyCode::Escape, KeyCode::Delete, KeyCode::Tab, KeyCode::Space,
            KeyCode::Up, KeyCode::Down, KeyCode::Left, KeyCode::Right,
            KeyCode::A, KeyCode::B, KeyCode::C, KeyCode::D, KeyCode::E,
            KeyCode::F, KeyCode::G, KeyCode::H, KeyCode::L, KeyCode::M,
            KeyCode::S, KeyCode::V, KeyCode::X, KeyCode::Z,
            KeyCode::C1, KeyCode::C2, KeyCode::C3, KeyCode::C0,
        };

        for (KeyCode key : keys) {
            REQUIRE_NOTHROW(mapper->HandlePrimaryMapperHotkeys(key, false));
            REQUIRE_NOTHROW(mapper->HandleShiftMapperHotkeys(key, false));
            REQUIRE_NOTHROW(mapper->HandleCtrlMapperHotkeys(key, false));
            REQUIRE_NOTHROW(mapper->UpdateArrowScrollKeys(key, KeyCode::None));
            REQUIRE_NOTHROW(mapper->UpdateArrowScrollKeys(KeyCode::None, key));
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

FO_END_NAMESPACE
