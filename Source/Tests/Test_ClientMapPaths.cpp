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

#include "catch_amalgamated.hpp"

#include "Application.h"
#include "Client.h"
#include "CritterHexView.h"
#include "MapView.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

FO_SCRIPT_API vector<mdir> Client_Map_GetPath(ptr<MapView> self, ptr<CritterView> cr, mpos toHex, int32_t cut);
FO_SCRIPT_API int32_t Client_Map_GetPathLength(ptr<MapView> self, ptr<CritterView> cr, mpos toHex, int32_t cut);

static auto MakeClientMapPathResources() -> FileSystem
{
    auto metadata = BakerTests::MakeEmptyMetadataBlob();
    auto compiler_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("ClientMapPathCompiler");
    compiler_source->AddFile("Metadata.fometa-client", metadata);
    FileSystem compiler_resources;
    compiler_resources.AddCustomSource(std::move(compiler_source));
    BakerClientEngine compiler {compiler_resources};
    auto source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("ClientMapPathRuntime");
    source->AddFile("Metadata.fometa-client", metadata);
#if FO_ANGELSCRIPT_SCRIPTING
    source->AddFile("ClientMapPath.fos-bin-client", BakerTests::CompileInlineScripts(&compiler, "ClientMapPathScripts", {{"Scripts/Path.fos", "void PathFixtureEntry() {}"}}, [](string_view message) { FAIL(message); }));
#endif
    vector<pair<string, function<void(ProtoMap&)>>> maps;
    maps.emplace_back("PathMap", [](ProtoMap& proto) { proto.SetSize(msize {40, 40}); });
    source->AddFile("PathMap.fopro-bin-client", BakerTests::MakeMultiProtoResourceBlob<ProtoMap>(compiler, compiler.Hashes.to_hashed_string("Map"), maps));
    vector<pair<string, function<void(ProtoItem&)>>> items;
    items.emplace_back("PathWall", [&compiler](ProtoItem& proto) {
        proto.SetIsWall(true);
        proto.SetPicMap(compiler.Hashes.to_hashed_string("Path.png"));
    });
    source->AddFile("PathWall.fopro-bin-client", BakerTests::MakeMultiProtoResourceBlob<ProtoItem>(compiler, compiler.Hashes.to_hashed_string("Item"), items));
    source->AddFile("Path.png", BakerTests::MakeMinimalBakedSprite(2, 2));
    FileSystem resources;
    resources.AddCustomSource(std::move(source));
    return resources;
}

TEST_CASE("ClientCritterPathsKeepExactCenterAndCutNearMultihexBody")
{
    GlobalSettings settings(false);
    settings.ApplyDefaultSettings();
    settings.ApplyAutoSettings();
    BakerTests::ApplySelfContainedClientSettings(settings);
    BakerTests::OverrideSetting(settings.Common.Packaged, false);
    BakerTests::OverrideSetting(settings.Baking.BakeOutput, string {});
    auto client = safe_alloc::make_refcounted<ClientEngine>(&settings, MakeClientMapPathResources(), &GetApp()->MainWindow);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto map = safe_alloc::make_refcounted<MapView>(client, ident_t {1001}, client->GetProtoMap(client->Hashes.to_hashed_string("PathMap")), isize32 {320, 200});
    auto destroy_map = scope_exit([&map]() noexcept { safe_call([&map] { map->DestroySelf(); }); });
    ProtoCritter proto {client->Hashes.to_hashed_string("PathBody"), client->GetPropertyRegistrar("Critter")};
    proto.SetMultihex(2);
    auto cr = safe_alloc::make_refcounted<CritterHexView>(map, ident_t {1002}, &proto);
    auto destroy_cr = scope_exit([&cr]() noexcept { safe_call([&cr] { cr->DestroySelf(); }); });
    cr->SetHex(mpos {20, 20});

    SECTION("ExactTargetsInsideAndOutsideTheBodyRadius")
    {
        for (int32_t distance : {0, 1, 2, 3, 5}) {
            mpos target = cr->GetHex();
            for (int32_t step = 0; step < distance; step++) {
                bool moved = GeometryHelper::MoveHexByDir(target, hdir::NorthEast, map->GetSize());
                REQUIRE(moved);
            }
            auto path = Client_Map_GetPath(map, cr, target, 0);
            CHECK(path.size() == numeric_cast<size_t>(distance));
            CHECK(Client_Map_GetPathLength(map, cr, target, 0) == distance);
            mpos endpoint = cr->GetHex();

            for (mdir dir : path) {
                bool moved = GeometryHelper::MoveHexByDir(endpoint, dir, map->GetSize());
                REQUIRE(moved);
            }

            CHECK(endpoint == target);
        }
    }

    SECTION("PositiveCutIsOnlyTheRequestedCenterDistance")
    {
        mpos target = cr->GetHex();
        for (int32_t step = 0; step < 3; step++) {
            bool moved = GeometryHelper::MoveHexByDir(target, hdir::NorthEast, map->GetSize());
            REQUIRE(moved);
        }
        auto path = Client_Map_GetPath(map, cr, target, 1);
        CHECK(path.size() == 2);
        CHECK(Client_Map_GetPathLength(map, cr, target, 1) == 2);
        CHECK(Client_Map_GetPath(map, cr, target, 3).empty());
        CHECK(Client_Map_GetPathLength(map, cr, target, 3) == 0);
    }

    SECTION("NearbyBlockedTargetHasNoPathOrLength")
    {
        mpos target = cr->GetHex();
        bool moved = GeometryHelper::MoveHexByDir(target, hdir::NorthEast, map->GetSize());
        REQUIRE(moved);
        map->EnableMapperMode();
        map->AddMapperItem(client->Hashes.to_hashed_string("PathWall"), target, nullptr);
        CHECK(Client_Map_GetPath(map, cr, target, 0).empty());
        CHECK(Client_Map_GetPathLength(map, cr, target, 0) == 0);
    }
}

FO_END_NAMESPACE
