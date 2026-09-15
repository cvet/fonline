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

#include "DataSerialization.h"
#include "EngineBase.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

static void InitProtoTestMetadata(EngineMetadata& meta)
{
    meta.RegisterSide(EngineSideKind::ServerSide);
    meta.RegisterEntityType("Item", true, false, true, true, true);
    meta.RegisterEntityType("Critter", true, false, true, true, true);
    meta.RegisterEntityType("Map", true, false, true, true, true);
    meta.RegisterEntityType("Location", true, false, true, true, true);
}

static auto GetTestRegistrar(EngineMetadata& meta, hstring type_name) -> ptr<const PropertyRegistrar>
{
    auto registrar = meta.GetPropertyRegistrar(type_name);
    REQUIRE(static_cast<bool>(registrar));
    return registrar;
}

template<typename TActual, typename TExpected>
static auto IsSameProtoPtr(const TActual& actual, const TExpected& expected) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return actual == expected;
}

TEST_CASE("ProtoManager")
{
    SECTION("BuiltInProtoLookupsAcceptEntityAndProtoTypeNames")
    {
        EngineMetadata meta {[] { }};
        InitProtoTestMetadata(meta);

        hstring item_type = meta.Hashes.to_hashed_string("Item");
        hstring proto_item_type = meta.Hashes.to_hashed_string("ProtoItem");
        hstring critter_type = meta.Hashes.to_hashed_string("Critter");
        hstring proto_critter_type = meta.Hashes.to_hashed_string("ProtoCritter");
        hstring knife_pid = meta.Hashes.to_hashed_string("Knife");
        hstring raider_pid = meta.Hashes.to_hashed_string("Raider");

        auto item_proto = safe_alloc::make_refcounted<ProtoItem>(knife_pid, GetTestRegistrar(meta, item_type));
        auto critter_proto = safe_alloc::make_refcounted<ProtoCritter>(raider_pid, GetTestRegistrar(meta, critter_type));
        meta.RegisterProto(item_type, item_proto);
        meta.RegisterProto(critter_type, critter_proto);

        REQUIRE(IsSameProtoPtr(meta.GetProtoItem(knife_pid), item_proto.get()));
        REQUIRE(IsSameProtoPtr(meta.GetProtoCritter(raider_pid), critter_proto.get()));

        CHECK(IsSameProtoPtr(meta.GetProtoEntity(item_type, knife_pid), item_proto.get()));
        CHECK(IsSameProtoPtr(meta.GetProtoEntity(proto_item_type, knife_pid), item_proto.get()));
        CHECK(IsSameProtoPtr(meta.GetProtoEntity(critter_type, raider_pid), critter_proto.get()));
        CHECK(IsSameProtoPtr(meta.GetProtoEntity(proto_critter_type, raider_pid), critter_proto.get()));

        const auto& item_protos = meta.GetProtoEntities(proto_item_type);
        const auto& critter_protos = meta.GetProtoEntities(proto_critter_type);

        REQUIRE(item_protos.size() == 1);
        REQUIRE(critter_protos.size() == 1);
        CHECK(IsSameProtoPtr(item_protos.at(knife_pid).get(), item_proto.get()));
        CHECK(IsSameProtoPtr(critter_protos.at(raider_pid).get(), critter_proto.get()));
        CHECK(IsSameProtoPtr(meta.GetProtoItems().at(knife_pid), item_proto.get()));
        CHECK(IsSameProtoPtr(meta.GetProtoCritters().at(raider_pid), critter_proto.get()));
    }

    SECTION("MigrationRulesRedirectProtoLookups")
    {
        EngineMetadata meta {[] { }};
        InitProtoTestMetadata(meta);

        hstring item_type = meta.Hashes.to_hashed_string("Item");
        hstring proto_item_type = meta.Hashes.to_hashed_string("ProtoItem");
        hstring knife_pid = meta.Hashes.to_hashed_string("Knife");
        hstring legacy_pid = meta.Hashes.to_hashed_string("LegacyKnife");
        hstring map_type = meta.Hashes.to_hashed_string("Map");
        hstring proto_map_type = meta.Hashes.to_hashed_string("ProtoMap");
        hstring location_type = meta.Hashes.to_hashed_string("Location");
        hstring proto_location_type = meta.Hashes.to_hashed_string("ProtoLocation");
        hstring rest_stop_day_pid = meta.Hashes.to_hashed_string("RestStop_Day");
        hstring rest_stop_day_time_pid = meta.Hashes.to_hashed_string("RestStop_DayTime");

        auto item_proto = safe_alloc::make_refcounted<ProtoItem>(knife_pid, GetTestRegistrar(meta, item_type));
        auto map_proto = safe_alloc::make_refcounted<ProtoMap>(rest_stop_day_time_pid, GetTestRegistrar(meta, map_type));
        auto location_proto = safe_alloc::make_refcounted<ProtoLocation>(rest_stop_day_time_pid, GetTestRegistrar(meta, location_type));
        meta.RegisterProto(item_type, item_proto);
        meta.RegisterProto(map_type, map_proto);
        meta.RegisterProto(location_type, location_proto);
        meta.RegisterMigrationRule("Proto", "Item", "LegacyKnife", "Knife");
        meta.RegisterMigrationRule("Proto", "Map", "RestStop_Day", "RestStop_DayTime");
        meta.RegisterMigrationRule("Proto", "Location", "RestStop_Day", "RestStop_DayTime");

        CHECK(IsSameProtoPtr(meta.GetProtoItem(legacy_pid), item_proto.get()));
        CHECK(IsSameProtoPtr(meta.GetProtoEntity(item_type, legacy_pid), item_proto.get()));
        CHECK(IsSameProtoPtr(meta.GetProtoEntity(proto_item_type, legacy_pid), item_proto.get()));
        CHECK(IsSameProtoPtr(meta.GetProtoEntity(map_type, rest_stop_day_pid), map_proto.get()));
        CHECK(IsSameProtoPtr(meta.GetProtoEntity(proto_map_type, rest_stop_day_pid), map_proto.get()));
        CHECK(IsSameProtoPtr(meta.GetProtoEntity(location_type, rest_stop_day_pid), location_proto.get()));
        CHECK(IsSameProtoPtr(meta.GetProtoEntity(proto_location_type, rest_stop_day_pid), location_proto.get()));
    }

    SECTION("MigrationRuleDeletionTokenResolvesToPresentEmptyValue")
    {
        EngineMetadata meta {[] { }};
        InitProtoTestMetadata(meta);

        hstring proto_rule = meta.Hashes.to_hashed_string("Proto");
        hstring item_type = meta.Hashes.to_hashed_string("Item");
        hstring removed_pid = meta.Hashes.to_hashed_string("RemovedKnife");

        meta.RegisterMigrationRule("Proto", "Item", "RemovedKnife", "__remove__");

        // An engaged empty result distinguishes an intentional deletion from no migration rule
        auto resolved = meta.CheckMigrationRule(proto_rule, item_type, removed_pid);
        CHECK(resolved.has_value());
        CHECK_FALSE(static_cast<bool>(resolved.value()));
        CHECK_FALSE(static_cast<bool>(meta.GetProtoItem(removed_pid)));
    }

    SECTION("UnknownTypeCollectionReturnsEmptyReference")
    {
        EngineMetadata meta {[] { }};
        InitProtoTestMetadata(meta);

        hstring map_type = meta.Hashes.to_hashed_string("Map");

        CHECK_FALSE(static_cast<bool>(meta.GetProtoEntity(map_type, meta.Hashes.to_hashed_string("Missing"))));
        CHECK(meta.GetProtoEntities(map_type).empty());
    }

    SECTION("LoadFromResourcesRegistersBuiltInProtoData")
    {
        EngineMetadata meta {[] { }};
        InitProtoTestMetadata(meta);

        auto source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("ProtoTestPack");
        source->AddFile("test.fopro-bin-server", BakerTests::MakeSingleProtoResourceBlob<ProtoItem>(meta, meta.Hashes.to_hashed_string("Item"), "LoadedKnife"));

        FileSystem resources;
        resources.AddCustomSource(std::move(source));
        meta.RegisterProtos(resources);

        hstring loaded_pid = meta.Hashes.to_hashed_string("LoadedKnife");
        hstring item_type = meta.Hashes.to_hashed_string("Item");
        hstring proto_item_type = meta.Hashes.to_hashed_string("ProtoItem");

        REQUIRE(static_cast<bool>(meta.GetProtoItem(loaded_pid)));
        CHECK(meta.GetProtoItem(loaded_pid)->GetName() == string_view {"LoadedKnife"});
        CHECK(meta.GetProtoItem(loaded_pid)->GetTypeName() == item_type);
        CHECK(IsSameProtoPtr(meta.GetProtoEntity(item_type, loaded_pid), meta.GetProtoItem(loaded_pid)));
        CHECK(IsSameProtoPtr(meta.GetProtoEntity(proto_item_type, loaded_pid), meta.GetProtoItem(loaded_pid)));
        CHECK(meta.GetProtoItems().contains(loaded_pid));
    }
}

FO_END_NAMESPACE
