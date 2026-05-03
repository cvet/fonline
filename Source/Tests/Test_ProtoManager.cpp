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

TEST_CASE("ProtoManager")
{
    SECTION("BuiltInProtoLookupsAcceptEntityAndProtoTypeNames")
    {
        EngineMetadata meta {[] { }};
        InitProtoTestMetadata(meta);

        const hstring item_type = meta.Hashes.ToHashedString("Item");
        const hstring proto_item_type = meta.Hashes.ToHashedString("ProtoItem");
        const hstring critter_type = meta.Hashes.ToHashedString("Critter");
        const hstring proto_critter_type = meta.Hashes.ToHashedString("ProtoCritter");
        const hstring knife_pid = meta.Hashes.ToHashedString("Knife");
        const hstring raider_pid = meta.Hashes.ToHashedString("Raider");

        auto item_proto = SafeAlloc::MakeRefCounted<ProtoItem>(knife_pid, meta.GetPropertyRegistrator(item_type));
        auto critter_proto = SafeAlloc::MakeRefCounted<ProtoCritter>(raider_pid, meta.GetPropertyRegistrator(critter_type));
        meta.RegisterProto(item_type, item_proto);
        meta.RegisterProto(critter_type, critter_proto);

        REQUIRE(meta.GetProtoItem(knife_pid) == item_proto.get());
        REQUIRE(meta.GetProtoCritter(raider_pid) == critter_proto.get());

        CHECK(meta.GetProtoEntity(item_type, knife_pid) == item_proto.get());
        CHECK(meta.GetProtoEntity(proto_item_type, knife_pid) == item_proto.get());
        CHECK(meta.GetProtoEntity(critter_type, raider_pid) == critter_proto.get());
        CHECK(meta.GetProtoEntity(proto_critter_type, raider_pid) == critter_proto.get());

        const auto& item_protos = meta.GetProtoEntities(proto_item_type);
        const auto& critter_protos = meta.GetProtoEntities(proto_critter_type);

        REQUIRE(item_protos.size() == 1);
        REQUIRE(critter_protos.size() == 1);
        CHECK(item_protos.at(knife_pid).get() == item_proto.get());
        CHECK(critter_protos.at(raider_pid).get() == critter_proto.get());
        CHECK(meta.GetProtoItems().at(knife_pid) == item_proto.get());
        CHECK(meta.GetProtoCritters().at(raider_pid) == critter_proto.get());
    }

    SECTION("MigrationRulesRedirectProtoLookups")
    {
        EngineMetadata meta {[] { }};
        InitProtoTestMetadata(meta);

        const hstring item_type = meta.Hashes.ToHashedString("Item");
        const hstring proto_item_type = meta.Hashes.ToHashedString("ProtoItem");
        const hstring knife_pid = meta.Hashes.ToHashedString("Knife");
        const hstring legacy_pid = meta.Hashes.ToHashedString("LegacyKnife");
        const hstring map_type = meta.Hashes.ToHashedString("Map");
        const hstring proto_map_type = meta.Hashes.ToHashedString("ProtoMap");
        const hstring location_type = meta.Hashes.ToHashedString("Location");
        const hstring proto_location_type = meta.Hashes.ToHashedString("ProtoLocation");
        const hstring rest_stop_day_pid = meta.Hashes.ToHashedString("RestStop_Day");
        const hstring rest_stop_day_time_pid = meta.Hashes.ToHashedString("RestStop_DayTime");

        auto item_proto = SafeAlloc::MakeRefCounted<ProtoItem>(knife_pid, meta.GetPropertyRegistrator(item_type));
        auto map_proto = SafeAlloc::MakeRefCounted<ProtoMap>(rest_stop_day_time_pid, meta.GetPropertyRegistrator(map_type));
        auto location_proto = SafeAlloc::MakeRefCounted<ProtoLocation>(rest_stop_day_time_pid, meta.GetPropertyRegistrator(location_type));
        meta.RegisterProto(item_type, item_proto);
        meta.RegisterProto(map_type, map_proto);
        meta.RegisterProto(location_type, location_proto);
        meta.RegisterMigrationRule("Proto", "Item", "LegacyKnife", "Knife");
        meta.RegisterMigrationRule("Proto", "Map", "RestStop_Day", "RestStop_DayTime");
        meta.RegisterMigrationRule("Proto", "Location", "RestStop_Day", "RestStop_DayTime");

        CHECK(meta.GetProtoItem(legacy_pid) == item_proto.get());
        CHECK(meta.GetProtoEntity(item_type, legacy_pid) == item_proto.get());
        CHECK(meta.GetProtoEntity(proto_item_type, legacy_pid) == item_proto.get());
        CHECK(meta.GetProtoEntity(map_type, rest_stop_day_pid) == map_proto.get());
        CHECK(meta.GetProtoEntity(proto_map_type, rest_stop_day_pid) == map_proto.get());
        CHECK(meta.GetProtoEntity(location_type, rest_stop_day_pid) == location_proto.get());
        CHECK(meta.GetProtoEntity(proto_location_type, rest_stop_day_pid) == location_proto.get());
    }

    SECTION("UnknownTypeCollectionReturnsEmptyReference")
    {
        EngineMetadata meta {[] { }};
        InitProtoTestMetadata(meta);

        const hstring map_type = meta.Hashes.ToHashedString("Map");

        CHECK(meta.GetProtoEntity(map_type, meta.Hashes.ToHashedString("Missing")) == nullptr);
        CHECK(meta.GetProtoEntities(map_type).empty());
    }

    SECTION("LoadFromResourcesRegistersBuiltInProtoData")
    {
        EngineMetadata meta {[] { }};
        InitProtoTestMetadata(meta);

        auto source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("ProtoTestPack");
        source->AddFile("test.fopro-bin-server", BakerTests::MakeSingleProtoResourceBlob<ProtoItem>(meta, meta.Hashes.ToHashedString("Item"), "LoadedKnife"));

        FileSystem resources;
        resources.AddCustomSource(std::move(source));
        meta.RegisterProtos(resources);

        const hstring loaded_pid = meta.Hashes.ToHashedString("LoadedKnife");
        const hstring item_type = meta.Hashes.ToHashedString("Item");
        const hstring proto_item_type = meta.Hashes.ToHashedString("ProtoItem");

        REQUIRE(meta.GetProtoItem(loaded_pid) != nullptr);
        CHECK(meta.GetProtoItem(loaded_pid)->GetName() == string_view {"LoadedKnife"});
        CHECK(meta.GetProtoItem(loaded_pid)->GetTypeName() == item_type);
        CHECK(meta.GetProtoEntity(item_type, loaded_pid) == meta.GetProtoItem(loaded_pid));
        CHECK(meta.GetProtoEntity(proto_item_type, loaded_pid) == meta.GetProtoItem(loaded_pid));
        CHECK(meta.GetProtoItems().contains(loaded_pid));
    }
}

FO_END_NAMESPACE
