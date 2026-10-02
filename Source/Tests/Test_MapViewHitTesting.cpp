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

#include "Application.h"
#include "Client.h"
#include "MapView.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

namespace
{
    auto MakeMapViewHitTestResources() -> FileSystem
    {
        const auto metadata = BakerTests::MakeEmptyMetadataBlob();
        auto compiler_source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("MapViewHitTestCompiler");
        compiler_source->AddFile("Metadata.fometa-client", metadata);
        FileSystem compiler_resources;
        compiler_resources.AddCustomSource(std::move(compiler_source));
        BakerClientEngine proto_engine {compiler_resources};
        auto source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("MapViewHitTestRuntime");
        source->AddFile("Metadata.fometa-client", metadata);
#if FO_ANGELSCRIPT_SCRIPTING
        source->AddFile("MapViewHitTest.fos-bin-client", BakerTests::CompileInlineScripts(&proto_engine, "MapViewHitTestScripts", {{"Scripts/MapViewHitTest.fos", "void HitTestFixtureEntry() {}"}}, [](string_view message) { FAIL(message); }));
#endif
        vector<pair<string, function<void(ProtoMap&)>>> maps;
        maps.emplace_back("HitTestMap", [](ProtoMap& proto) { proto.SetSize(msize {20, 20}); });
        source->AddFile("HitTestMap.fopro-bin-client", BakerTests::MakeMultiProtoResourceBlob<ProtoMap>(proto_engine, proto_engine.Hashes.to_hashed_string("Map"), maps));
        vector<pair<string, function<void(ProtoItem&)>>> items;
        items.emplace_back("HitTestWall", [&proto_engine](ProtoItem& proto) {
            proto.SetIsWall(true);
            proto.SetPicMap(proto_engine.Hashes.to_hashed_string("HitTest.png"));
        });
        items.emplace_back("HitTestFloor", [&proto_engine](ProtoItem& proto) {
            proto.SetIsTile(true);
            proto.SetPicMap(proto_engine.Hashes.to_hashed_string("HitTest.png"));
        });
        source->AddFile("HitTestItems.fopro-bin-client", BakerTests::MakeMultiProtoResourceBlob<ProtoItem>(proto_engine, proto_engine.Hashes.to_hashed_string("Item"), items));
        source->AddFile("HitTest.png", BakerTests::MakeMinimalBakedSprite(32, 16));
        FileSystem resources;
        resources.AddCustomSource(std::move(source));
        return resources;
    }
}

TEST_CASE("MapViewItemHitTestingCanSelectTransparentEggOccluders")
{
    GlobalSettings settings(false);
    settings.ApplyDefaultSettings();
    settings.ApplyAutoSettings();
    BakerTests::ApplySelfContainedClientSettings(settings);
    BakerTests::OverrideSetting(settings.Common.Packaged, false);
    BakerTests::OverrideSetting(settings.Baking.BakeOutput, string {});
    auto client = safe_alloc::make_refcounted<ClientEngine>(&settings, MakeMapViewHitTestResources(), &GetApp()->MainWindow);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto map = safe_alloc::make_refcounted<MapView>(client, ident_t {1001}, client->GetProtoMap(client->Hashes.to_hashed_string("HitTestMap")), isize32 {320, 200});
    auto destroy_map = scope_exit([&map]() noexcept { safe_call([&map] { map->DestroySelf(); }); });
    map->EnableMapperMode();
    ptr<ItemHexView> floor = map->AddMapperItem(client->Hashes.to_hashed_string("HitTestFloor"), mpos {8, 8}, nullptr);
    ptr<ItemHexView> wall = map->AddMapperItem(client->Hashes.to_hashed_string("HitTestWall"), mpos {8, 8}, nullptr);
    map->InstantScrollTo(mpos {8, 8});
    map->RebuildMap();
    REQUIRE(wall->IsMapSpriteVisible());
    REQUIRE(floor->IsMapSpriteVisible());

    ptr<MapSprite> wall_sprite = wall->GetMapSprite();
    wall_sprite->SetEggAppearence(EggAppearenceType::Always);
    wall_sprite->SetEggStructure(true);
    const irect32 rect = wall_sprite->GetDrawRect();
    const ipos32 pixel = map->MapToScreenPos({rect.x + rect.width / 2, rect.y + rect.height / 2});
    bool item_egg = false;
    CHECK(map->GetItemAtScreen(pixel, item_egg, 0, true).first == wall);

    map->SetTransparentEgg(TransparentEggSlot::Secondary, mpos {8, 8}, ipos32 {0, 0}, isize32 {128, 128}, TransparentEggTarget::Structure, false);
    CHECK(map->GetItemAtScreen(pixel, item_egg, 0, true).first == floor);
    CHECK_FALSE(item_egg);
    CHECK(map->GetEntityAtScreen(pixel, 0, true).first == floor);
    CHECK(map->GetItemAtScreen(pixel, item_egg, 0, true, true).first == wall);
    CHECK_FALSE(item_egg);
    CHECK(map->GetEntityAtScreen(pixel, 0, true, true).first == wall);
    CHECK_FALSE(map->GetEntityAtScreen(ipos32 {-10000, -10000}, 0, true, true).first);
    map->ClearTransparentEgg(TransparentEggSlot::Secondary);
    CHECK(map->GetEntityAtScreen(pixel, 0, true).first == wall);
}

FO_END_NAMESPACE
