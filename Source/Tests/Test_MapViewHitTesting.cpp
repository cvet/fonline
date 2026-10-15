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

auto Client_Map_GetItems(ptr<MapView> self) -> vector<ptr<ItemView>>;
auto Client_Map_GetItem(ptr<MapView> self, ident_t item_id) -> nptr<ItemView>;
auto Client_Map_GetItemsOnHex(ptr<MapView> self, mpos hex) -> vector<ptr<ItemView>>;

namespace
{
    auto MakeMapViewHitTestResources() -> FileSystem
    {
        auto metadata = BakerTests::MakeEmptyMetadataBlob();
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
    irect32 rect = wall_sprite->GetDrawRect();
    ipos32 pixel = map->MapToScreenPos({rect.x + rect.width / 2, rect.y + rect.height / 2});
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

    SECTION("BoundedViewFiltersStaticItemsAndScreenQueries")
    {
        ipos32 edge {8, 8};
        GeometryHelper::MoveHexByDirUnsafe(edge, hdir::East);
        GeometryHelper::MoveHexByDirUnsafe(edge, hdir::East);
        mpos edge_hex = map->GetSize().from_raw_pos(edge);
        GeometryHelper::MoveHexByDirUnsafe(edge, hdir::East);
        mpos outside_hex = map->GetSize().from_raw_pos(edge);
        ptr<ItemHexView> outside = map->AddMapperItem(client->Hashes.to_hashed_string("HitTestFloor"), outside_hex, nullptr);
        map->SetViewArea(mpos {8, 8}, 2);
        CHECK(map->IsHexInViewArea(edge_hex));
        CHECK_FALSE(map->IsHexInViewArea(outside_hex));
        CHECK(Client_Map_GetItems(map).size() == 2);
        CHECK_FALSE(Client_Map_GetItem(map, outside->GetId()));
        CHECK(Client_Map_GetItemsOnHex(map, outside_hex).empty());
        map->RebuildMap();
        auto outside_sprite = outside->GetMapSprite();
        irect32 outside_rect = outside_sprite->GetDrawRect();
        ipos32 outside_pixel = map->MapToScreenPos({outside_rect.x + outside_rect.width / 2, outside_rect.y + outside_rect.height / 2});
        CHECK_FALSE(map->GetEntityAtScreen(outside_pixel, 0, false).first);
        mpos hit_hex {};
        CHECK_FALSE(map->GetHexAtScreen(outside_pixel, hit_hex, nullptr));
        map->SetScreenSize(isize32 {1024, 768});
        CHECK_FALSE(map->IsHexInViewArea(outside_hex));
        CHECK(Client_Map_GetItems(map).size() == 2);
        map->SetViewArea(mpos {8, 8}, 0);
        CHECK(Client_Map_GetItems(map).size() == 3);
        CHECK(Client_Map_GetItem(map, outside->GetId()) == outside);
    }

    SECTION("BoundedViewRespectsAuthoredScrollArea")
    {
        map->SetScrollAxialArea(irect32 {0, 0, 38, 20});
        map->SetScrollCheck(true);
        auto check_viewport = [&map](isize32 size) {
            fpos32 viewport_origin = fpos32(GeometryHelper::GetHexPos(map->GetScreenRawHex())) + map->GetScrollOffset();
            fpos32 viewport_end = viewport_origin + fpos32(numeric_cast<float32_t>(size.width), numeric_cast<float32_t>(size.height)) / map->GetSpritesZoom();
            CHECK(viewport_origin.x >= GameSettings::MAP_HEX_WIDTH - 0.01f);
            CHECK(viewport_origin.y >= GameSettings::MAP_HEX_LINE_HEIGHT - 0.01f);
            CHECK(viewport_end.x <= 20 * GameSettings::MAP_HEX_WIDTH + 0.01f);
            CHECK(viewport_end.y <= 21 * GameSettings::MAP_HEX_LINE_HEIGHT + 0.01f);
        };

        for (mpos focus : {mpos {1, 1}, mpos {18, 18}}) {
            CAPTURE(focus);
            map->SetViewArea(focus, 0);
            map->SetScreenSize(isize32 {320, 200});
            map->InstantZoom(1.0f, {0.5f, 0.5f});
            map->InstantScrollTo(focus);
            ipos32 ordinary_focus_pixel = map->MapToScreenPos(map->GetHexMapPos(focus));
            check_viewport({320, 200});
            map->SetViewArea(focus, 2);
            CHECK(map->MapToScreenPos(map->GetHexMapPos(focus)) == ordinary_focus_pixel);
            check_viewport({320, 200});
            map->InstantScrollTo(focus);
            check_viewport({320, 200});
            map->InstantScroll(fpos32 {-1000.0f, -1000.0f});
            check_viewport({320, 200});
            map->InstantScroll(fpos32 {1000.0f, 1000.0f});
            check_viewport({320, 200});
            map->SetScreenSize(isize32 {480, 220});
            check_viewport({480, 220});
            map->InstantZoom(2.0f, {0.5f, 0.5f});
            map->InstantScrollTo(focus);
            check_viewport({480, 220});
            map->InstantZoom(1.0f, {0.5f, 0.5f});
            map->InstantScrollTo(focus);
            check_viewport({480, 220});
            CHECK(map->IsHexInViewArea(focus));
            CHECK_FALSE(map->IsHexInViewArea(mpos {8, 8}));
            CHECK(map->IsScrollCheck());
        }
    }

    SECTION("BoundedViewCanScrollWithoutMovingVisibility")
    {
        map->SetScrollAxialArea(irect32 {-38, 0, 76, 40});
        map->SetScrollCheck(true);
        map->SetViewArea(mpos {8, 8}, 2);
        auto camera_position = [&map]() { return fpos32(GeometryHelper::GetHexPos(map->GetScreenRawHex())) + map->GetScrollOffset(); };
        fpos32 initial_position = camera_position();
        map->ScrollToHex(mpos {12, 12}, {}, 10000, true);
        CHECK(map->IsAutoScrolling());
        map->SetManualScroll(ScrollDirection::Right);

        for (int32_t frame = 0; frame < 10; frame++) {
            coarse_sleep(std::chrono::milliseconds {10});
            client->GameTime.FrameAdvance(false);
            map->Process();
        }

        CHECK_FALSE(map->IsAutoScrolling());
        fpos32 scrolled_position = camera_position();
        CHECK(scrolled_position.x > initial_position.x);
        map->SetManualScroll(ScrollDirection::None);

        for (int32_t frame = 0; frame < 10; frame++) {
            coarse_sleep(std::chrono::milliseconds {10});
            client->GameTime.FrameAdvance(false);
            map->Process();
        }

        CHECK(camera_position() == scrolled_position);
        map->SetScreenSize(isize32 {480, 220});
        CHECK(camera_position() == scrolled_position);
        CHECK(map->IsHexInViewArea(mpos {8, 8}));
        CHECK_FALSE(map->IsHexInViewArea(mpos {12, 12}));
    }
}

FO_END_NAMESPACE
