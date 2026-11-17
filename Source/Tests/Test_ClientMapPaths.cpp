// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "catch_amalgamated.hpp"

#include "Application.h"
#include "Client.h"
#include "CritterHexView.h"
#include "ItemHexView.h"
#include "MapView.h"
#include "ScriptSystem.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

FO_SCRIPT_API vector<mdir> Client_Map_GetPath(ptr<MapView> self, ptr<CritterView> cr, mpos toHex, int32_t cut);
FO_SCRIPT_API vector<mdir> Client_Map_GetPath(ptr<MapView> self, mpos fromHex, mpos toHex, int32_t cut);
FO_SCRIPT_API vector<mdir> Client_Map_GetPath(ptr<MapView> self, mpos fromHex, mpos toHex, int32_t cut, ScriptFunc<bool, ptr<ItemView>> gagCallback);
FO_SCRIPT_API int32_t Client_Map_GetPathLength(ptr<MapView> self, mpos fromHex, mpos toHex, int32_t cut);
FO_SCRIPT_API int32_t Client_Map_GetPathLength(ptr<MapView> self, ptr<CritterView> cr, mpos toHex, int32_t cut);

static auto MakeClientMapPathResources() -> FileSystem
{
    auto metadata {BakerTests::MakeEmptyMetadataBlob()};
    auto compiler_source {safe_alloc::make_unique<BakerTests::MemoryDataSource>("ClientMapPathCompiler")};
    compiler_source->AddFile("Metadata.fometa-client", metadata);
    FileSystem compiler_resources {};
    compiler_resources.AddCustomSource(std::move(compiler_source));
    BakerClientEngine compiler {compiler_resources};
    auto source {safe_alloc::make_unique<BakerTests::MemoryDataSource>("ClientMapPathRuntime")};
    source->AddFile("Metadata.fometa-client", metadata);
#if FO_ANGELSCRIPT_SCRIPTING
    source->AddFile("ClientMapPath.fos-bin-client", BakerTests::CompileInlineScripts(&compiler, "ClientMapPathScripts", {{"Scripts/Path.fos", "void PathFixtureEntry() {}"}}, [](string_view message) { FAIL(message); }));
#endif
    vector<pair<string, function<void(ProtoMap&)>>> maps {};
    maps.emplace_back("PathMap", [](ProtoMap& proto) { proto.SetSize(msize {40, 40}); });
    source->AddFile("PathMap.fopro-bin-client", BakerTests::MakeMultiProtoResourceBlob<ProtoMap>(compiler, compiler.Hashes.to_hashed_string("Map"), maps));
    vector<pair<string, function<void(ProtoItem&)>>> items {};
    items.emplace_back("PathWall", [&compiler](ProtoItem& proto) {
        proto.SetIsWall(true);
        proto.SetPicMap(compiler.Hashes.to_hashed_string("Path.png"));
    });
    items.emplace_back("PathGag", [&compiler](ProtoItem& proto) {
        proto.SetIsGag(true);
        proto.SetNoBlock(false);
        proto.SetShootThru(true);
        proto.SetPicMap(compiler.Hashes.to_hashed_string("Path.png"));
    });
    items.emplace_back("PathDecoration", [&compiler](ProtoItem& proto) {
        proto.SetNoBlock(true);
        proto.SetShootThru(true);
        proto.SetPicMap(compiler.Hashes.to_hashed_string("Path.png"));
    });
    source->AddFile("PathWall.fopro-bin-client", BakerTests::MakeMultiProtoResourceBlob<ProtoItem>(compiler, compiler.Hashes.to_hashed_string("Item"), items));
    source->AddFile("Path.png", BakerTests::MakeMinimalBakedSprite(2, 2));
    FileSystem resources {};
    resources.AddCustomSource(std::move(source));
    return resources;
}

TEST_CASE("ClientCoordinatePathLengthIsZeroWhenAlreadyAtTarget")
{
    GlobalSettings settings {false};
    settings.SetupDefaultSettings();
    settings.ApplyAutoSettings({});
    BakerTests::ApplySelfContainedClientSettings(settings);
    BakerTests::OverrideSetting(settings.Common.Packaged, false);
    BakerTests::OverrideSetting(settings.Baking.BakeOutput, string {});
    auto client {safe_alloc::make_refcounted<ClientEngine>(&settings, MakeClientMapPathResources(), &GetApp()->MainWindow)};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto map {safe_alloc::make_refcounted<MapView>(client, ident_t {1000}, client->GetProtoMap(client->Hashes.to_hashed_string("PathMap")), isize32 {320, 200})};
    auto destroy_map {scope_exit([&map]() noexcept { safe_call([&map] { map->DestroySelf(); }); })};
    mpos here {20, 20};

    for (int32_t cut : {-1, 0, 1}) {
        CAPTURE(cut);
        CHECK(Client_Map_GetPath(map, here, here, cut).empty());
        CHECK(Client_Map_GetPathLength(map, here, here, cut) == 0);
    }

    mpos adjacent {here};
    REQUIRE(GeometryHelper::MoveHexByDir(adjacent, hdir::NorthEast, map->GetSize()));
    CHECK(Client_Map_GetPath(map, here, adjacent, 0).size() == 1);
    CHECK(Client_Map_GetPathLength(map, here, adjacent, 0) == 1);
    CHECK(Client_Map_GetPathLength(map, here, adjacent, 1) == 0);
}

TEST_CASE("ClientCritterPathsKeepExactCenterAndCutNearMultihexBody")
{
    GlobalSettings settings {false};
    settings.SetupDefaultSettings();
    settings.ApplyAutoSettings({});
    BakerTests::ApplySelfContainedClientSettings(settings);
    BakerTests::OverrideSetting(settings.Common.Packaged, false);
    BakerTests::OverrideSetting(settings.Baking.BakeOutput, string {});
    auto client {safe_alloc::make_refcounted<ClientEngine>(&settings, MakeClientMapPathResources(), &GetApp()->MainWindow)};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto map {safe_alloc::make_refcounted<MapView>(client, ident_t {1001}, client->GetProtoMap(client->Hashes.to_hashed_string("PathMap")), isize32 {320, 200})};
    auto destroy_map {scope_exit([&map]() noexcept { safe_call([&map] { map->DestroySelf(); }); })};
    ProtoCritter proto {client->Hashes.to_hashed_string("PathBody"), client->GetPropertyRegistrar("Critter")};
    proto.SetMultihex(2);
    auto cr {safe_alloc::make_refcounted<CritterHexView>(map, ident_t {1002}, &proto)};
    auto destroy_cr {scope_exit([&cr]() noexcept { safe_call([&cr] { cr->DestroySelf(); }); })};
    cr->SetHex(mpos {20, 20});

    SECTION("ExactTargetsInsideAndOutsideTheBodyRadius")
    {
        for (int32_t distance : {0, 1, 2, 3, 5}) {
            mpos target {cr->GetHex()};
            for (int32_t step = 0; step < distance; step++) {
                bool moved {GeometryHelper::MoveHexByDir(target, hdir::NorthEast, map->GetSize())};
                REQUIRE(moved);
            }
            auto path {Client_Map_GetPath(map, cr, target, 0)};
            CHECK(path.size() == numeric_cast<size_t>(distance));
            CHECK(Client_Map_GetPathLength(map, cr, target, 0) == distance);
            mpos endpoint {cr->GetHex()};

            for (mdir dir : path) {
                bool moved {GeometryHelper::MoveHexByDir(endpoint, dir, map->GetSize())};
                REQUIRE(moved);
            }

            CHECK(endpoint == target);
        }
    }

    SECTION("PositiveCutIsOnlyTheRequestedCenterDistance")
    {
        mpos target {cr->GetHex()};
        for (int32_t step = 0; step < 3; step++) {
            bool moved {GeometryHelper::MoveHexByDir(target, hdir::NorthEast, map->GetSize())};
            REQUIRE(moved);
        }
        auto path {Client_Map_GetPath(map, cr, target, 1)};
        CHECK(path.size() == 2);
        CHECK(Client_Map_GetPathLength(map, cr, target, 1) == 2);
        CHECK(Client_Map_GetPath(map, cr, target, 3).empty());
        CHECK(Client_Map_GetPathLength(map, cr, target, 3) == 0);
    }

    SECTION("NearbyBlockedTargetHasNoPathOrLength")
    {
        mpos target {cr->GetHex()};
        bool moved {GeometryHelper::MoveHexByDir(target, hdir::NorthEast, map->GetSize())};
        REQUIRE(moved);
        map->EnableMapperMode();
        map->AddMapperItem(client->Hashes.to_hashed_string("PathWall"), target, nullptr);
        CHECK(Client_Map_GetPath(map, cr, target, 0).empty());
        CHECK(Client_Map_GetPathLength(map, cr, target, 0) == 0);
    }
}

static auto ClientMapPathAllowGag(ptr<ItemView> item) -> bool
{
    return item->GetIsGag();
}

static auto ClientMapPathRefuseGag(ptr<ItemView> item) -> bool
{
    ignore_unused(item);

    return false;
}

TEST_CASE("ClientGagPathRejectsMapDestroyedByCallback")
{
    GlobalSettings settings {false};
    settings.SetupDefaultSettings();
    settings.ApplyAutoSettings({});
    BakerTests::ApplySelfContainedClientSettings(settings);
    BakerTests::OverrideSetting(settings.Common.Packaged, false);
    BakerTests::OverrideSetting(settings.Baking.BakeOutput, string {});
    auto client {safe_alloc::make_refcounted<ClientEngine>(&settings, MakeClientMapPathResources(), &GetApp()->MainWindow)};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto map {safe_alloc::make_refcounted<MapView>(client, ident_t {1004}, client->GetProtoMap(client->Hashes.to_hashed_string("PathMap")), isize32 {320, 200})};
    auto destroy_map {scope_exit([&map]() noexcept {
        if (!map->IsDestroyed()) {
            safe_call([&map] { map->DestroySelf(); });
        }
    })};
    map->EnableMapperMode();
    mpos gap {20, 20};
    map->AddMapperItem(client->Hashes.to_hashed_string("PathGag"), gap, nullptr);
    bool called {false};
    function<bool(ptr<const ItemHexView>)> callback {[&](ptr<const ItemHexView> item) {
        ignore_unused(item);
        called = true;
        map->DestroySelf();
        return true;
    }};

    CHECK_THROWS_AS(map->CheckGagItem(gap, callback), VerificationException);
    CHECK(called);
    CHECK(map->IsDestroyed());
}

TEST_CASE("ClientGagPathSkipsItemRemovedOrMovedByCallback")
{
    bool move_item {GENERATE(false, true)};
    CAPTURE(move_item);
    GlobalSettings settings {false};
    settings.SetupDefaultSettings();
    settings.ApplyAutoSettings({});
    BakerTests::ApplySelfContainedClientSettings(settings);
    BakerTests::OverrideSetting(settings.Common.Packaged, false);
    BakerTests::OverrideSetting(settings.Baking.BakeOutput, string {});
    auto client {safe_alloc::make_refcounted<ClientEngine>(&settings, MakeClientMapPathResources(), &GetApp()->MainWindow)};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto map {safe_alloc::make_refcounted<MapView>(client, ident_t {1005}, client->GetProtoMap(client->Hashes.to_hashed_string("PathMap")), isize32 {320, 200})};
    auto destroy_map {scope_exit([&map]() noexcept { safe_call([&map] { map->DestroySelf(); }); })};
    map->EnableMapperMode();
    mpos gap {20, 20};
    auto first {map->AddMapperItem(client->Hashes.to_hashed_string("PathGag"), gap, nullptr)};
    auto second {map->AddMapperItem(client->Hashes.to_hashed_string("PathGag"), gap, nullptr)};
    auto second_holder {second.hold_ref()};
    int32_t calls {0};
    function<bool(ptr<const ItemHexView>)> callback {[&](ptr<const ItemHexView> item) {
        calls++;

        if (item == first) {
            if (move_item) {
                map->MoveItem(second, mpos {21, 20});
            }
            else {
                map->DestroyItem(second);
            }
        }

        return true;
    }};

    CHECK(map->CheckGagItem(gap, callback));
    CHECK(calls == 1);
    CHECK(second_holder->IsDestroyed() != move_item);

    if (move_item) {
        mpos target {21, 20};
        CHECK(second_holder->GetHex() == target);
    }
}

TEST_CASE("ClientGagPathHandlesObstacleAddedByCallback")
{
    bool add_wall {GENERATE(false, true)};
    CAPTURE(add_wall);
    GlobalSettings settings {false};
    settings.SetupDefaultSettings();
    settings.ApplyAutoSettings({});
    BakerTests::ApplySelfContainedClientSettings(settings);
    BakerTests::OverrideSetting(settings.Common.Packaged, false);
    BakerTests::OverrideSetting(settings.Baking.BakeOutput, string {});
    auto client {safe_alloc::make_refcounted<ClientEngine>(&settings, MakeClientMapPathResources(), &GetApp()->MainWindow)};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto map {safe_alloc::make_refcounted<MapView>(client, ident_t {1006}, client->GetProtoMap(client->Hashes.to_hashed_string("PathMap")), isize32 {320, 200})};
    auto destroy_map {scope_exit([&map]() noexcept { safe_call([&map] { map->DestroySelf(); }); })};
    map->EnableMapperMode();
    mpos gap {20, 20};
    map->AddMapperItem(client->Hashes.to_hashed_string("PathGag"), gap, nullptr);
    bool called {false};
    function<bool(ptr<const ItemHexView>)> callback {[&](ptr<const ItemHexView> item) {
        ignore_unused(item);
        called = true;
        map->AddMapperItem(client->Hashes.to_hashed_string(add_wall ? "PathWall" : "PathGag"), gap, nullptr);
        return true;
    }};

    if (add_wall) {
        CHECK_FALSE(map->CheckGagItem(gap, callback));
    }
    else {
        CHECK_THROWS_AS(map->CheckGagItem(gap, callback), VerificationException);
    }
    CHECK(called);
}

TEST_CASE("ClientGagPathHandlesItemBecomingObstacle")
{
    bool already_skipped {GENERATE(false, true)};
    CAPTURE(already_skipped);
    GlobalSettings settings {false};
    settings.SetupDefaultSettings();
    settings.ApplyAutoSettings({});
    BakerTests::ApplySelfContainedClientSettings(settings);
    BakerTests::OverrideSetting(settings.Common.Packaged, false);
    BakerTests::OverrideSetting(settings.Baking.BakeOutput, string {});
    auto client {safe_alloc::make_refcounted<ClientEngine>(&settings, MakeClientMapPathResources(), &GetApp()->MainWindow)};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto map {safe_alloc::make_refcounted<MapView>(client, ident_t {1007}, client->GetProtoMap(client->Hashes.to_hashed_string("PathMap")), isize32 {320, 200})};
    auto destroy_map {scope_exit([&map]() noexcept { safe_call([&map] { map->DestroySelf(); }); })};
    map->EnableMapperMode();
    mpos gap {20, 20};
    auto first {map->AddMapperItem(client->Hashes.to_hashed_string(already_skipped ? "PathDecoration" : "PathGag"), gap, nullptr)};
    auto second {map->AddMapperItem(client->Hashes.to_hashed_string(already_skipped ? "PathGag" : "PathDecoration"), gap, nullptr)};
    auto decoration {already_skipped ? first : second};
    auto original_gag {already_skipped ? second : first};
    int32_t calls {0};
    function<bool(ptr<const ItemHexView>)> callback {[&](ptr<const ItemHexView> item) {
        calls++;

        if (item == original_gag) {
            decoration->SetIsGag(true);
            decoration->SetNoBlock(false);
            map->RecacheHexFlags(gap);
            return true;
        }

        return false;
    }};

    if (already_skipped) {
        CHECK_THROWS_AS(map->CheckGagItem(gap, callback), VerificationException);
    }
    else {
        CHECK_FALSE(map->CheckGagItem(gap, callback));
    }

    CHECK(calls == (already_skipped ? 1 : 2));
}

TEST_CASE("ClientGagPathChecksEveryItemInDenseCell", "[.][performance]")
{
    GlobalSettings settings {false};
    settings.SetupDefaultSettings();
    settings.ApplyAutoSettings({});
    BakerTests::ApplySelfContainedClientSettings(settings);
    BakerTests::OverrideSetting(settings.Common.Packaged, false);
    BakerTests::OverrideSetting(settings.Baking.BakeOutput, string {});
    auto client {safe_alloc::make_refcounted<ClientEngine>(&settings, MakeClientMapPathResources(), &GetApp()->MainWindow)};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto map {safe_alloc::make_refcounted<MapView>(client, ident_t {1008}, client->GetProtoMap(client->Hashes.to_hashed_string("PathMap")), isize32 {320, 200})};
    auto destroy_map {scope_exit([&map]() noexcept { safe_call([&map] { map->DestroySelf(); }); })};
    map->EnableMapperMode();
    mpos gap {20, 20};
    int32_t item_count {GENERATE(128, 256, 512)};
    CAPTURE(item_count);

    for (int32_t i {0}; i < item_count; i++) {
        map->AddMapperItem(client->Hashes.to_hashed_string("PathGag"), gap, nullptr);
    }

    int32_t calls {0};
    function<bool(ptr<const ItemHexView>)> callback {[&](ptr<const ItemHexView> item) {
        ignore_unused(item);
        calls++;
        return true;
    }};
    auto started {std::chrono::steady_clock::now()};
    bool traversable {map->CheckGagItem(gap, callback)};
    auto elapsed {std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started)};

    CHECK(traversable);
    CHECK(calls == item_count);
    logging::write("Dense gag-cell check: {} items in {} us", item_count, elapsed.count());
}

TEST_CASE("ClientGagPathWithCutUsesCallbackAcrossBlockedBarrier")
{
    GlobalSettings settings {false};
    settings.SetupDefaultSettings();
    settings.ApplyAutoSettings({});
    BakerTests::ApplySelfContainedClientSettings(settings);
    BakerTests::OverrideSetting(settings.Common.Packaged, false);
    BakerTests::OverrideSetting(settings.Baking.BakeOutput, string {});
    auto client {safe_alloc::make_refcounted<ClientEngine>(&settings, MakeClientMapPathResources(), &GetApp()->MainWindow)};
    auto shutdown {scope_exit([&client]() noexcept { safe_call([&client] { client->Shutdown(); }); })};
    auto map {safe_alloc::make_refcounted<MapView>(client, ident_t {1003}, client->GetProtoMap(client->Hashes.to_hashed_string("PathMap")), isize32 {320, 200})};
    auto destroy_map {scope_exit([&map]() noexcept {
        if (!map->IsDestroyed()) {
            safe_call([&map] { map->DestroySelf(); });
        }
    })};
    map->EnableMapperMode();
    mpos from {10, 20};
    mpos to {30, 20};
    mpos gap {20, 20};
    int32_t direct_distance {GeometryHelper::GetDistance(from, to)};
    REQUIRE(direct_distance == 20);

    auto wall {map->AddMapperItem(client->Hashes.to_hashed_string("PathWall"), mpos {20, 0}, nullptr)};

    for (int32_t y = 1; y < 40; y++) {
        if (y == 20) {
            continue;
        }

        map->AddMapperItem(client->Hashes.to_hashed_string("PathWall"), map->GetSize().from_raw_pos(20, y), nullptr);
    }

    auto gag {map->AddMapperItem(client->Hashes.to_hashed_string("PathGag"), gap, nullptr)};
    ScriptFuncDesc callback_desc {};
    callback_desc.Call = NativeDataCaller::NativeCall<&ClientMapPathAllowGag>;
    ScriptFunc<bool, ptr<ItemView>> callback {&callback_desc};
    bool called {callback.Call(gag)};
    REQUIRE(called);
    CHECK(callback.GetResult());
    called = callback.Call(wall);
    REQUIRE(called);
    CHECK_FALSE(callback.GetResult());

    CHECK(Client_Map_GetPath(map, from, to, 0).empty());

    SECTION("DestroyedMapStopsTheActualPathSearch")
    {
        bool invoked {false};
        function<bool(ptr<const ItemHexView>)> destroy_callback {[&](ptr<const ItemHexView> item) {
            ignore_unused(item);
            invoked = true;
            map->DestroySelf();
            return true;
        }};
        mpos destination {to};

        CHECK_THROWS_AS(map->FindPath(nullptr, from, destination, 0, {}, destroy_callback), VerificationException);
        CHECK(invoked);
        CHECK(map->IsDestroyed());
    }

    SECTION("TrueCallbackCutZeroReachesExactTarget")
    {
        auto exact {Client_Map_GetPath(map, from, to, 0, ScriptFunc<bool, ptr<ItemView>> {&callback_desc})};
        REQUIRE_FALSE(exact.empty());
        CHECK(exact.size() == numeric_cast<size_t>(direct_distance));
        mpos endpoint {from};

        for (mdir dir : exact) {
            bool moved {GeometryHelper::MoveHexByDir(endpoint, dir, map->GetSize())};
            REQUIRE(moved);
        }

        CHECK(endpoint == to);
    }

    SECTION("TrueCallbackCutOneStopsExactlyOneHexShort")
    {
        auto shortened {Client_Map_GetPath(map, from, to, 1, ScriptFunc<bool, ptr<ItemView>> {&callback_desc})};
        REQUIRE_FALSE(shortened.empty());
        CHECK(shortened.size() == numeric_cast<size_t>(direct_distance - 1));
        mpos endpoint {from};

        for (mdir dir : shortened) {
            bool moved {GeometryHelper::MoveHexByDir(endpoint, dir, map->GetSize())};
            REQUIRE(moved);
        }

        CHECK(GeometryHelper::GetDistance(endpoint, to) == 1);
    }

    SECTION("RefusingCallbackCannotCrossTheGagGap")
    {
        ScriptFuncDesc refuse_desc {};
        refuse_desc.Call = NativeDataCaller::NativeCall<&ClientMapPathRefuseGag>;
        ScriptFunc<bool, ptr<ItemView>> refuse {&refuse_desc};
        bool called_refuse {refuse.Call(gag)};
        REQUIRE(called_refuse);
        CHECK_FALSE(refuse.GetResult());

        CHECK(Client_Map_GetPath(map, from, to, 0, ScriptFunc<bool, ptr<ItemView>> {&refuse_desc}).empty());
        CHECK(Client_Map_GetPath(map, from, to, 1, ScriptFunc<bool, ptr<ItemView>> {&refuse_desc}).empty());
    }

    SECTION("SolidNonGagReplacementAtGapBlocksTrueCallback")
    {
        // DestroyItem drops the registry and field entry, so the borrowed gag is not used past this point
        map->DestroyItem(gag);
        CHECK(map->GetItemsOnHex(gap).empty());

        auto solid {map->AddMapperItem(client->Hashes.to_hashed_string("PathWall"), gap, nullptr)};
        CHECK(map->GetItemsOnHex(gap).size() == numeric_cast<size_t>(1));
        CHECK_FALSE(solid->GetIsGag());

        bool called_solid {callback.Call(solid)};
        REQUIRE(called_solid);
        CHECK_FALSE(callback.GetResult());

        CHECK(Client_Map_GetPath(map, from, to, 0, ScriptFunc<bool, ptr<ItemView>> {&callback_desc}).empty());
        CHECK(Client_Map_GetPath(map, from, to, 1, ScriptFunc<bool, ptr<ItemView>> {&callback_desc}).empty());
    }

    SECTION("CutCoveringTheWholeRouteReturnsEmpty")
    {
        CHECK(Client_Map_GetPath(map, from, to, direct_distance + 1, ScriptFunc<bool, ptr<ItemView>> {&callback_desc}).empty());
    }

    SECTION("NegativeCutNormalizesToExactTarget")
    {
        auto negative {Client_Map_GetPath(map, from, to, -1, ScriptFunc<bool, ptr<ItemView>> {&callback_desc})};
        REQUIRE_FALSE(negative.empty());
        CHECK(negative.size() == numeric_cast<size_t>(direct_distance));
        mpos endpoint {from};

        for (mdir dir : negative) {
            bool moved {GeometryHelper::MoveHexByDir(endpoint, dir, map->GetSize())};
            REQUIRE(moved);
        }

        CHECK(endpoint == to);
    }

    SECTION("InvalidEndpointsAndNullCallbackAreRejected")
    {
        CHECK_THROWS_AS(Client_Map_GetPath(map, mpos {-1, 20}, to, 0, ScriptFunc<bool, ptr<ItemView>> {&callback_desc}), ScriptException);
        CHECK_THROWS_AS(Client_Map_GetPath(map, from, mpos {30, 40}, 0, ScriptFunc<bool, ptr<ItemView>> {&callback_desc}), ScriptException);
        CHECK_THROWS_AS(Client_Map_GetPath(map, from, to, 0, ScriptFunc<bool, ptr<ItemView>> {}), ScriptException);
    }
}

FO_END_NAMESPACE
