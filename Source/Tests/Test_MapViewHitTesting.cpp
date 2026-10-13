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
#include "MapLightCalculation.h"
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
        auto outside_rect = outside_sprite->GetDrawRect();
        auto outside_pixel = map->MapToScreenPos({outside_rect.x + outside_rect.width / 2, outside_rect.y + outside_rect.height / 2});
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

static auto LightFixtureDigest(const MapLightCalculationResult& result) -> uint64_t;

TEST_CASE("MapLightCalculationDirtyConsumerSubmitsBatch")
{
    int32_t mode = GENERATE(0, 1, 2, 3);
    int32_t source_count = mode == 3 ? 2 : 16;
    GlobalSettings settings(false);
    settings.ApplyDefaultSettings();
    settings.ApplyAutoSettings();
    BakerTests::ApplySelfContainedClientSettings(settings);
    BakerTests::OverrideSetting(settings.Common.Packaged, false);
    BakerTests::OverrideSetting(settings.Baking.BakeOutput, string {});
    BakerTests::OverrideSetting(settings.Client.Multithreading, mode != 0);
    BakerTests::OverrideSetting(settings.Client.MultithreadingMaxWorkers, mode == 1 ? int32_t {0} : int32_t {2});
    BakerTests::OverrideSetting(settings.Client.MultithreadingHeadroomMinCores, int32_t {0});
    auto client = safe_alloc::make_refcounted<ClientEngine>(&settings, MakeMapViewHitTestResources(), &GetApp()->MainWindow);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    if (mode >= 2 && !client->WorkSched.IsParallel()) {
        SKIP("Client workers are unavailable on this platform");
    }
    auto map = safe_alloc::make_refcounted<MapView>(client, ident_t {1001}, client->GetProtoMap(client->Hashes.to_hashed_string("HitTestMap")), isize32 {320, 200});
    auto destroy_map = scope_exit([&map]() noexcept { safe_call([&map] { map->DestroySelf(); }); });
    map->InstantScrollTo(mpos {8, 8});

    for (int32_t i = 0; i < source_count; i++) {
        auto item = map->AddReceivedItem(ident_t {numeric_cast<uint32_t>(2000 + i)}, client->Hashes.to_hashed_string("HitTestFloor"), {8, 8}, {}, false);
        item->SetLightSource(true);
        item->SetLightDistance(int16_t {6});
        item->SetLightIntensity(int8_t {-100});
        item->SetLightColor(ucolor {255, 128, 64, 255});
        map->UpdateItemLightSource(item);
    }

    map->DrawMap();
    REQUIRE(map->GetField({8, 8}).LightSources.size() == numeric_cast<size_t>(source_count));

    for (auto ls : map->GetField({8, 8}).LightSources | std::views::keys) {
        ls->CurIntensity = 100;
        ls->NeedReapply = true;
    }

    auto before = client->WorkSched.GetDiagnostics();
    map->DrawMap();
    auto after = client->WorkSched.GetDiagnostics();
    if (mode == 2) {
        CHECK(after.ParallelBatches > before.ParallelBatches);
        CHECK(after.ParallelItems >= before.ParallelItems + 16);
    }
    else {
        CHECK(after.ParallelBatches == before.ParallelBatches);
    }

    CHECK(map->GetLightData().get()[8 * 20 + 8] == ucolor {200, 100, 50, 0});
    auto source = map->GetField({8, 8}).LightSources.begin()->first;
    MapLightCalculationResult published;
    published.Capacity = source->Capacity;
    published.CenterColor = source->CenterColor;
    published.FanHexes = source->FanHexes;

    for (mpos hex : source->MarkedHexes) {
        published.Marks.emplace_back(hex, map->GetField(hex).LightSources.at(source));
    }

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        CHECK(LightFixtureDigest(published) == 16627265182678125812ULL);
    }

    if (mode == 2) {
        auto old_fan = source->FanHexes;
        auto old_marks = source->MarkedHexes;
        source->Hex = {-1, 8};

        for (auto ls : map->GetField({8, 8}).LightSources | std::views::keys) {
            ls->NeedReapply = true;
        }

        CHECK_THROWS(map->DrawMap());
        CHECK(source->FanHexes == old_fan);
        CHECK(source->MarkedHexes == old_marks);
        CHECK(source->NeedReapply);
        CHECK(map->GetLightData().get()[8 * 20 + 8] == ucolor {200, 100, 50, 0});
        source->Hex = {8, 8};
        REQUIRE_NOTHROW(map->DrawMap());
        CHECK_FALSE(source->NeedReapply);
        CHECK(source->FanHexes == old_fan);
    }

    before = client->WorkSched.GetDiagnostics();

    for (int16_t y = 6; y <= 10; y++) {
        auto wall = map->AddReceivedItem(ident_t {numeric_cast<uint32_t>(3000 + y)}, client->Hashes.to_hashed_string("HitTestWall"), {10, y}, {}, false);
        wall->SetCorner(CornerType::NorthSouth);
        map->RefreshItem(wall);
    }

    after = client->WorkSched.GetDiagnostics();
    CHECK((after.ParallelBatches > before.ParallelBatches) == (mode == 2));
    auto published_digest = [&] {
        MapLightCalculationResult value;
        value.Capacity = source->Capacity;
        value.CenterColor = source->CenterColor;
        value.FanHexes = source->FanHexes;

        for (mpos hex : source->MarkedHexes) {
            value.Marks.emplace_back(hex, map->GetField(hex).LightSources.at(source));
        }

        return LightFixtureDigest(value);
    };

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        CHECK(published_digest() == 12525354448293597504ULL);
    }

    before = client->WorkSched.GetDiagnostics();

    for (int16_t y = 6; y <= 10; y++) {
        map->DestroyItem(map->GetItem(ident_t {numeric_cast<uint32_t>(3000 + y)}));
    }

    // The closed fan need not mark the removed wall cells; explicitly invalidate a still-marked cell
    map->UpdateHexLightSources({8, 8});
    after = client->WorkSched.GetDiagnostics();
    CHECK((after.ParallelBatches > before.ParallelBatches) == (mode == 2));

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        CHECK(published_digest() == 16627265182678125812ULL);
    }

    CHECK(map->GetField({8, 8}).LightSources.size() == numeric_cast<size_t>(source_count));
    CHECK(map->GetLightData().get()[8 * 20 + 8] == ucolor {200, 100, 50, 0});
}

TEST_CASE("FogDirtyConsumerSubmitsSectors")
{
    GlobalSettings settings(false);
    settings.ApplyDefaultSettings();
    settings.ApplyAutoSettings();
    BakerTests::ApplySelfContainedClientSettings(settings);
    BakerTests::OverrideSetting(settings.Common.Packaged, false);
    BakerTests::OverrideSetting(settings.Baking.BakeOutput, string {});
    BakerTests::OverrideSetting(settings.Client.Multithreading, true);
    BakerTests::OverrideSetting(settings.View.DisableFog, false);
    BakerTests::OverrideSetting(settings.Client.MultithreadingMaxWorkers, int32_t {2});
    BakerTests::OverrideSetting(settings.Client.MultithreadingHeadroomMinCores, int32_t {0});
    auto client = safe_alloc::make_refcounted<ClientEngine>(&settings, MakeMapViewHitTestResources(), &GetApp()->MainWindow);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    if (!client->WorkSched.IsParallel()) {
        SKIP("Client workers are unavailable on this platform");
    }
    auto map = safe_alloc::make_refcounted<MapView>(client, ident_t {1002}, client->GetProtoMap(client->Hashes.to_hashed_string("HitTestMap")), isize32 {320, 200});
    auto destroy_map = scope_exit([&map]() noexcept { safe_call([&map] { map->DestroySelf(); }); });
    map->InstantScrollTo(mpos {8, 8});
    auto fog = map->AddFog(mpos {8, 8}, DrawOrderType::Light, nullptr).hold_ref();
    fog->Radius = 32;
    fog->Distance = 32;
    fog->Traced = true;
    fog->TransitionDuration = 0;
    auto before = client->WorkSched.GetDiagnostics();
    map->DrawMap();
    auto after = client->WorkSched.GetDiagnostics();
    CHECK_FALSE(fog->Shape.GetPoints().empty());
    CHECK(after.ParallelBatches > before.ParallelBatches);
    CHECK(after.ParallelItems >= before.ParallelItems + 4);
}

// Frozen legacy fixtures pin every cell's mask/color and the ordered fan, independently of the new kernel
static auto LightFixtureDigest(const MapLightCalculationResult& result) -> uint64_t
{
    vector<ucolor> colors(400);
    vector<uint8_t> marked(400);

    for (const auto& mark : result.Marks) {
        size_t index = numeric_cast<size_t>(mark.Hex.y * 20 + mark.Hex.x);
        auto& color = colors[index];

        if (!marked[index] || mark.Color.comp.r > color.comp.r || mark.Color.comp.g > color.comp.g || mark.Color.comp.b > color.comp.b) {
            color = mark.Color;
        }

        marked[index] = 1;
    }

    uint64_t digest = 14695981039346656037ULL;
    auto add = [&digest](uint64_t value) { digest = (digest ^ value) * 1099511628211ULL; };
    auto add_color = [&add](ucolor color) {
        add(color.comp.r);
        add(color.comp.g);
        add(color.comp.b);
        add(color.comp.a);
    };
    add(numeric_cast<uint64_t>(result.Capacity));
    add_color(result.CenterColor);

    for (size_t i = 0; i < colors.size(); i++) {
        add(marked[i]);
        add_color(colors[i]);
    }

    add(result.FanHexes.size());

    for (const auto& [hex, alpha, offset] : result.FanHexes) {
        add(numeric_cast<uint64_t>(hex.x));
        add(numeric_cast<uint64_t>(hex.y));
        add(alpha);
        add(offset ? 1 : 0);
    }

    return digest;
}

TEST_CASE("MapLightCalculationLegacyObstacleAndCapacityOracles")
{
    int32_t scenario = GENERATE(0, 1, 2, 3, 4, 5, 6, 7);
    GlobalSettings settings(false);
    settings.ApplyDefaultSettings();
    settings.ApplyAutoSettings();
    BakerTests::ApplySelfContainedClientSettings(settings);
    BakerTests::OverrideSetting(settings.Common.Packaged, false);
    BakerTests::OverrideSetting(settings.Baking.BakeOutput, string {});
    auto client = safe_alloc::make_refcounted<ClientEngine>(&settings, MakeMapViewHitTestResources(), &GetApp()->MainWindow);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto map = safe_alloc::make_refcounted<MapView>(client, ident_t {1003}, client->GetProtoMap(client->Hashes.to_hashed_string("HitTestMap")), isize32 {320, 200});
    auto destroy_map = scope_exit([&map]() noexcept { safe_call([&map] { map->DestroySelf(); }); });
    mpos origin = scenario == 3 ? mpos {0, 0} : mpos {8, 8};
    map->InstantScrollTo(origin);
    map->SetMapDayLightCapacity(40);
    map->SetGlobalDayLightCapacity(70);

    if (scenario == 1 || scenario == 2) {
        for (int16_t y = 6; y <= 10; y++) {
            auto wall = map->AddReceivedItem(ident_t {numeric_cast<uint32_t>(3000 + y)}, client->Hashes.to_hashed_string("HitTestWall"), {10, y}, {}, false);
            wall->SetCorner(scenario == 1 ? CornerType::NorthSouth : CornerType::EastWest);
            wall->SetLightThru(scenario == 2);
            map->RefreshItem(wall);
        }
    }

    auto item = map->AddReceivedItem(ident_t {2000}, client->Hashes.to_hashed_string("HitTestFloor"), origin, {}, false);
    item->SetLightSource(true);
    item->SetLightDistance(int16_t {6});
    item->SetLightIntensity(scenario >= 4 ? int8_t {100} : int8_t {-100});
    item->SetLightColor(ucolor {255, 128, 64, 255});
    uint16_t flags = scenario == 4 ? 1 : scenario == 5 ? 3 : scenario == 6 ? 2 : scenario == 7 ? 0x54 : 0;
    item->SetLightFlags(flags);
    map->UpdateItemLightSource(item);
    REQUIRE(map->GetField(origin).LightSources.size() == 1);
    auto ls = map->GetField(origin).LightSources.begin()->first;
    ls->CurIntensity = 100;
    ls->NeedReapply = true;
    map->DrawMap();

    MapLightCalculationResult legacy;
    legacy.Capacity = ls->Capacity;
    legacy.CenterColor = ls->CenterColor;
    legacy.FanHexes = ls->FanHexes;

    for (mpos hex : ls->MarkedHexes) {
        legacy.Marks.emplace_back(hex, map->GetField(hex).LightSources.at(ls));
    }

    constexpr array<uint64_t, 8> expected_digests {16627265182678125812ULL, 12525354448293597504ULL, 1286257406146885428ULL, 15114296361840258567ULL, 3333029952730632869ULL, 793512007040746606ULL, 14944741294266904326ULL, 14347835127108210628ULL};
    uint64_t legacy_digest = LightFixtureDigest(legacy);

    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        CHECK(legacy_digest == expected_digests[numeric_cast<size_t>(scenario)]);
    }

    vector<MapLightObstacle> obstacles(400);

    for (int16_t y = 0; y < 20; y++) {
        for (int16_t x = 0; x < 20; x++) {
            const auto& field = map->GetField({x, y});
            obstacles[numeric_cast<size_t>(y * 20 + x)] = {field.LightBlocked, field.HasWall, field.HasTransparentWall, field.Corner};
        }
    }

    MapLightCalculationInput input {.MapSize = {20, 20}, .Hex = origin, .Color = item->GetLightColor(), .Distance = 6, .Intensity = item->GetLightIntensity(), .CurIntensity = 100, .Global = (flags & 1) != 0, .Inverse = (flags & 2) != 0, .StopDirections = numeric_cast<uint16_t>(flags >> 2), .MapCapacity = 40, .GlobalCapacity = 70};
    auto calculated = CalculateMapLight(input, {{0, 0}, {20, 20}, obstacles});
    if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
        CHECK(LightFixtureDigest(calculated) == expected_digests[numeric_cast<size_t>(scenario)]);
    }
    else {
        CHECK(calculated.CenterColor == legacy.CenterColor);
        CHECK(calculated.FanHexes == legacy.FanHexes);
    }
}

TEST_CASE("MapLightCalculationOverlapRemovalAndDoorRecovery")
{
    GlobalSettings settings(false);
    settings.ApplyDefaultSettings();
    settings.ApplyAutoSettings();
    BakerTests::ApplySelfContainedClientSettings(settings);
    BakerTests::OverrideSetting(settings.Common.Packaged, false);
    BakerTests::OverrideSetting(settings.Baking.BakeOutput, string {});
    auto client = safe_alloc::make_refcounted<ClientEngine>(&settings, MakeMapViewHitTestResources(), &GetApp()->MainWindow);
    auto shutdown = scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); });
    auto map = safe_alloc::make_refcounted<MapView>(client, ident_t {1004}, client->GetProtoMap(client->Hashes.to_hashed_string("HitTestMap")), isize32 {320, 200});
    auto destroy_map = scope_exit([&map]() noexcept { safe_call([&map] { map->DestroySelf(); }); });
    map->InstantScrollTo({8, 8});

    for (uint32_t id : {2000U, 2001U}) {
        auto item = map->AddReceivedItem(ident_t {id}, client->Hashes.to_hashed_string("HitTestFloor"), {8, 8}, {}, false);
        item->SetLightSource(true);
        item->SetLightDistance(int16_t {6});
        item->SetLightIntensity(int8_t {-100});
        item->SetLightColor(id == 2000 ? ucolor {255, 0, 0, 255} : ucolor {0, 0, 255, 255});
        map->UpdateItemLightSource(item);
    }

    for (auto ls : map->GetField({8, 8}).LightSources | std::views::keys) {
        ls->CurIntensity = 100;
        ls->NeedReapply = true;
    }

    map->DrawMap();
    CHECK(map->GetLightData().get()[8 * 20 + 8] == ucolor {200, 0, 200, 0});
    auto blue = map->GetItem(ident_t {2001});
    map->DestroyItem(blue);

    for (auto ls : map->GetField({8, 8}).LightSources | std::views::keys) {
        if (ls->Finishing) {
            ls->CurIntensity = 0;
        }
    }

    map->DrawMap();
    CHECK(map->GetField({8, 8}).LightSources.size() == 1);
    CHECK(map->GetLightData().get()[8 * 20 + 8] == ucolor {200, 0, 0, 0});
    auto source = map->GetField({8, 8}).LightSources.begin()->first;
    auto before = source->FanHexes;
    auto door = map->AddReceivedItem(ident_t {3000}, client->Hashes.to_hashed_string("HitTestWall"), {10, 8}, {}, false);
    source->NeedReapply = true;
    map->DrawMap();
    CHECK(source->FanHexes != before);
    door->SetLightThru(true);
    door->SetIsWall(false);
    map->RefreshItem(door);
    source->NeedReapply = true;
    map->DrawMap();
    CHECK(source->FanHexes == before);
}

FO_END_NAMESPACE

