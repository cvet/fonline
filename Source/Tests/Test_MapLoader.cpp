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

#include "EngineBase.h"
#include "EntityProtos.h"
#include "MapLoader.h"

FO_BEGIN_NAMESPACE

static void InitTestMapLoaderMetadata(EngineMetadata& meta)
{
    meta.RegisterSide(EngineSideKind::ServerSide);
    meta.RegisterEntityType("Critter", true, false, true, true, true);
    meta.RegisterEntityType("Item", true, false, true, true, true);
}

static auto GetTestMapLoaderRegistrar(EngineMetadata& meta, string_view type_name) -> ptr<const PropertyRegistrar>
{
    auto registrar = meta.GetPropertyRegistrar(type_name);
    REQUIRE(static_cast<bool>(registrar));
    return registrar;
}

TEST_CASE("MapLoader")
{
    SECTION("RejectsMapsWithoutProtoMapSection")
    {
        EngineMetadata meta {[] { }};
        hash_storage hashes {};

        CHECK_THROWS_AS(MapLoader::Load("LegacyMap", "LegacyMap.fomap", "[Header]\n[Tiles]\n[Objects]\n", meta, hashes, [](ident_t, ptr<const ProtoCritter>, ptr<const map<string_view, string_view>>) { }, [](ident_t, ptr<const ProtoItem>, ptr<const map<string_view, string_view>>) { }), MapLoaderException);
        CHECK_THROWS_AS(MapLoader::Load("BrokenMap", "BrokenMap.fomap", "[$Name/Critter]\n$Proto = CritterOne\n", meta, hashes, [](ident_t, ptr<const ProtoCritter>, ptr<const map<string_view, string_view>>) { }, [](ident_t, ptr<const ProtoItem>, ptr<const map<string_view, string_view>>) { }), MapLoaderException);
    }

    SECTION("RejectsBareContentSections")
    {
        EngineMetadata meta {[] { }};
        hash_storage hashes {};

        string map_buf = "[ProtoMap]\n"
                         "$Name = TestMap\n"
                         "[Critter]\n"
                         "$Id = 1\n"
                         "$Proto = CritterOne\n";

        CHECK_THROWS_AS(MapLoader::Load("TestMap", "TestMap.fomap", map_buf, meta, hashes, [](ident_t, ptr<const ProtoCritter>, ptr<const map<string_view, string_view>>) { }, [](ident_t, ptr<const ProtoItem>, ptr<const map<string_view, string_view>>) { }), MapLoaderException);

        string bare_slash_map_buf = "[ProtoMap]\n"
                                    "$Name = TestMap\n"
                                    "[/Item]\n"
                                    "$Id = 1\n"
                                    "$Proto = ItemOne\n";

        CHECK_THROWS_AS(MapLoader::Load("TestMap", "TestMap.fomap", bare_slash_map_buf, meta, hashes, [](ident_t, ptr<const ProtoCritter>, ptr<const map<string_view, string_view>>) { }, [](ident_t, ptr<const ProtoItem>, ptr<const map<string_view, string_view>>) { }), MapLoaderException);
    }

    SECTION("RejectsNestedSectionsAddressedToUndeclaredMap")
    {
        EngineMetadata meta {[] { }};
        hash_storage hashes {};

        string map_buf = "[ProtoMap]\n"
                         "$Name = TestMap\n"
                         "[OtherMap/Item]\n"
                         "$Id = 1\n"
                         "$Proto = ItemOne\n";

        CHECK_THROWS_AS(MapLoader::Load("TestMap", "TestMap.fomap", map_buf, meta, hashes, [](ident_t, ptr<const ProtoCritter>, ptr<const map<string_view, string_view>>) { }, [](ident_t, ptr<const ProtoItem>, ptr<const map<string_view, string_view>>) { }), MapLoaderException);
    }

    SECTION("RejectsUnknownNestedSectionType")
    {
        EngineMetadata meta {[] { }};
        hash_storage hashes {};

        string map_buf = "[ProtoMap]\n"
                         "$Name = TestMap\n"
                         "[$Name/Tile]\n"
                         "$Id = 1\n";

        CHECK_THROWS_AS(MapLoader::Load("TestMap", "TestMap.fomap", map_buf, meta, hashes, [](ident_t, ptr<const ProtoCritter>, ptr<const map<string_view, string_view>>) { }, [](ident_t, ptr<const ProtoItem>, ptr<const map<string_view, string_view>>) { }), MapLoaderException);
    }

    SECTION("RejectsLoadOfMapThatIsNotDeclared")
    {
        EngineMetadata meta {[] { }};
        hash_storage hashes {};

        string map_buf = "[ProtoMap]\n"
                         "$Name = TestMap\n";

        CHECK_THROWS_AS(MapLoader::Load("AnotherMap", "AnotherMap.fomap", map_buf, meta, hashes, [](ident_t, ptr<const ProtoCritter>, ptr<const map<string_view, string_view>>) { }, [](ident_t, ptr<const ProtoItem>, ptr<const map<string_view, string_view>>) { }), MapLoaderException);
    }

    SECTION("AnonymousAnchorResolvesToFileStemNotRequestedName")
    {
        EngineMetadata meta {[] { }};
        InitTestMapLoaderMetadata(meta);
        auto item_proto = safe_alloc::make_refcounted<ProtoItem>(meta.Hashes.to_hashed_string("TestItem"), GetTestMapLoaderRegistrar(meta, "Item"));
        meta.RegisterProto(meta.Hashes.to_hashed_string("Item"), item_proto);

        hash_storage hashes {};

        string map_buf = "[ProtoMap]\n"
                         "Outside = True\n"
                         "[$Name/Item]\n"
                         "$Id = 1\n"
                         "$Proto = TestItem\n"
                         "Kind = FromStemMap\n"
                         "[ProtoMap]\n"
                         "$Name = ZoneB\n"
                         "[$Name/Item]\n"
                         "$Id = 1\n"
                         "$Proto = TestItem\n"
                         "Kind = FromZoneB\n";

        vector<string> loaded_kinds;
        auto load_map = [&](string_view map_name) {
            loaded_kinds.clear();
            CHECK_NOTHROW(MapLoader::Load(map_name, "Maps/Zones.fomap", map_buf, meta, hashes, [](ident_t, ptr<const ProtoCritter>, ptr<const map<string_view, string_view>>) {}, [&](ident_t, ptr<const ProtoItem>, ptr<const map<string_view, string_view>> kv) { loaded_kinds.emplace_back(kv->at("Kind")); }));
        };

        load_map("Zones");
        CHECK(loaded_kinds == vector<string> {"FromStemMap"});

        load_map("ZoneB");
        CHECK(loaded_kinds == vector<string> {"FromZoneB"});
    }

    SECTION("MissingProtosAccumulateErrorsAndSkipCallbacks")
    {
        EngineMetadata meta {[] { }};
        hash_storage hashes {};
        size_t critter_calls = 0;
        size_t item_calls = 0;

        string map_buf = "[ProtoMap]\n"
                         "$Name = TestMap\n"
                         "[$Name/Critter]\n"
                         "$Id = 1\n"
                         "$Proto = MissingCritter\n"
                         "[$Name/Item]\n"
                         "$Id = 2\n"
                         "$Proto = MissingItem\n";

        CHECK_THROWS_AS(MapLoader::Load("TestMap", "TestMap.fomap", map_buf, meta, hashes, [&](ident_t, ptr<const ProtoCritter>, ptr<const map<string_view, string_view>>) { critter_calls++; }, [&](ident_t, ptr<const ProtoItem>, ptr<const map<string_view, string_view>>) { item_calls++; }), MapLoaderException);

        CHECK(critter_calls == 0);
        CHECK(item_calls == 0);
    }

    SECTION("MissingProtoFieldAlsoFailsLoad")
    {
        EngineMetadata meta {[] { }};
        hash_storage hashes {};

        string map_buf = "[ProtoMap]\n"
                         "$Name = TestMap\n"
                         "[$Name/Critter]\n"
                         "$Id = 1\n"
                         "[$Name/Item]\n"
                         "$Id = 2\n";

        CHECK_THROWS_AS(MapLoader::Load("TestMap", "TestMap.fomap", map_buf, meta, hashes, [](ident_t, ptr<const ProtoCritter>, ptr<const map<string_view, string_view>>) { }, [](ident_t, ptr<const ProtoItem>, ptr<const map<string_view, string_view>>) { }), MapLoaderException);
    }

    SECTION("ValidEntriesLoadAndDuplicateIdsAreReassigned")
    {
        EngineMetadata meta {[] { }};
        InitTestMapLoaderMetadata(meta);
        auto critter_proto = safe_alloc::make_refcounted<ProtoCritter>(meta.Hashes.to_hashed_string("TestCritter"), GetTestMapLoaderRegistrar(meta, "Critter"));
        auto item_proto = safe_alloc::make_refcounted<ProtoItem>(meta.Hashes.to_hashed_string("TestItem"), GetTestMapLoaderRegistrar(meta, "Item"));
        meta.RegisterProto(meta.Hashes.to_hashed_string("Critter"), critter_proto);
        meta.RegisterProto(meta.Hashes.to_hashed_string("Item"), item_proto);

        hash_storage hashes {};
        vector<ident_t> critter_ids;
        vector<ident_t> item_ids;
        vector<string> critter_proto_names;
        vector<string> item_proto_names;

        string map_buf = "[ProtoMap]\n"
                         "$Name = TestMap\n"
                         "[$Name/Critter]\n"
                         "$Id = 0\n"
                         "$Proto = TestCritter\n"
                         "Name = One\n"
                         "[$Name/Critter]\n"
                         "$Id = 0\n"
                         "$Proto = TestCritter\n"
                         "Name = Two\n"
                         "[$Name/Item]\n"
                         "$Id = 2\n"
                         "$Proto = TestItem\n"
                         "Kind = Alpha\n"
                         "[TestMap/Item]\n"
                         "$Id = 2\n"
                         "$Proto = TestItem\n"
                         "Kind = Beta\n";

        CHECK_NOTHROW(MapLoader::Load(
            "TestMap", "TestMap.fomap", map_buf, meta, hashes,
            [&](ident_t id, ptr<const ProtoCritter> proto, ptr<const map<string_view, string_view>> kv) {
                critter_ids.emplace_back(id);
                critter_proto_names.emplace_back(proto->GetProtoId().as_str());
                CHECK(kv->at("$Proto") == "TestCritter");
            },
            [&](ident_t id, ptr<const ProtoItem> proto, ptr<const map<string_view, string_view>> kv) {
                item_ids.emplace_back(id);
                item_proto_names.emplace_back(proto->GetProtoId().as_str());
                CHECK(kv->at("$Proto") == "TestItem");
            }));

        CHECK(critter_ids == vector<ident_t> {ident_t {1}, ident_t {2}});
        CHECK(item_ids == vector<ident_t> {ident_t {3}, ident_t {4}});
        CHECK(critter_proto_names == vector<string> {"TestCritter", "TestCritter"});
        CHECK(item_proto_names == vector<string> {"TestItem", "TestItem"});
    }

    SECTION("MultiMapFileLoadsEachMapSeparately")
    {
        EngineMetadata meta {[] { }};
        InitTestMapLoaderMetadata(meta);
        auto item_proto = safe_alloc::make_refcounted<ProtoItem>(meta.Hashes.to_hashed_string("TestItem"), GetTestMapLoaderRegistrar(meta, "Item"));
        meta.RegisterProto(meta.Hashes.to_hashed_string("Item"), item_proto);

        hash_storage hashes {};

        string map_buf = "[ProtoMap]\n"
                         "$Name = MapOne\n"
                         "[$Name/Item]\n"
                         "$Id = 1\n"
                         "$Proto = TestItem\n"
                         "Kind = FromOne\n"
                         "[ProtoMap]\n"
                         "$Name = MapTwo\n"
                         "[$Name/Item]\n"
                         "$Id = 1\n"
                         "$Proto = TestItem\n"
                         "Kind = FromTwo\n"
                         "[$Name/Item]\n"
                         "$Id = 2\n"
                         "$Proto = TestItem\n"
                         "Kind = FromTwoAsWell\n";

        vector<string> loaded_kinds;
        auto load_map = [&](string_view map_name) {
            loaded_kinds.clear();
            CHECK_NOTHROW(MapLoader::Load(map_name, "Multi.fomap", map_buf, meta, hashes, [](ident_t, ptr<const ProtoCritter>, ptr<const map<string_view, string_view>>) {}, [&](ident_t, ptr<const ProtoItem>, ptr<const map<string_view, string_view>> kv) { loaded_kinds.emplace_back(kv->at("Kind")); }));
        };

        load_map("MapOne");
        CHECK(loaded_kinds == vector<string> {"FromOne"});

        load_map("MapTwo");
        CHECK(loaded_kinds == vector<string> {"FromTwo", "FromTwoAsWell"});
    }

    SECTION("EnumerateMapsResolvesAnchorNames")
    {
        string multi_buf = "[ProtoMap]\n"
                           "$Name = MapOne\n"
                           "[$Name/Item]\n"
                           "$Id = 1\n"
                           "[ProtoMap]\n"
                           "$Name = MapTwo\n";

        CHECK(MapLoader::EnumerateMaps("Multi.fomap", multi_buf) == vector<string> {"MapOne", "MapTwo"});

        string anonymous_buf = "[ProtoMap]\n"
                               "Outside = True\n";

        CHECK(MapLoader::EnumerateMaps("Maps/StemMap.fomap", anonymous_buf) == vector<string> {"StemMap"});

        string anonymous_multi_buf = "[ProtoMap]\n"
                                     "$Name = MapOne\n"
                                     "[ProtoMap]\n"
                                     "Outside = True\n";

        CHECK(MapLoader::EnumerateMaps("Multi.fomap", anonymous_multi_buf) == vector<string> {"MapOne", "Multi"});

        string colliding_anonymous_buf = "[ProtoMap]\n"
                                         "Outside = True\n"
                                         "[ProtoMap]\n"
                                         "Outside = False\n";

        // Both anchors resolve to the stem; the id enumerates once and the duplicate
        // itself is reported by the generic proto collision check on bake
        CHECK(MapLoader::EnumerateMaps("Collide.fomap", colliding_anonymous_buf) == vector<string> {"Collide"});

        // No [ProtoMap] anchors -> not a map container; EnumerateMaps doubles as the detector
        CHECK(MapLoader::EnumerateMaps("Empty.fomap", string("NoSections = True\n")).empty());
        CHECK(MapLoader::EnumerateMaps("Location.foloc", string("[ProtoLocation]\n$Name = SomeLoc\n")).empty());
    }

    SECTION("AnonymousAnchorBindsNestedContentThroughFileStem")
    {
        EngineMetadata meta {[] { }};
        InitTestMapLoaderMetadata(meta);
        auto item_proto = safe_alloc::make_refcounted<ProtoItem>(meta.Hashes.to_hashed_string("TestItem"), GetTestMapLoaderRegistrar(meta, "Item"));
        meta.RegisterProto(meta.Hashes.to_hashed_string("Item"), item_proto);

        hash_storage hashes {};
        size_t item_calls = 0;

        string map_buf = "[ProtoMap]\n"
                         "Outside = True\n"
                         "[$Name/Item]\n"
                         "$Id = 1\n"
                         "$Proto = TestItem\n";

        CHECK_NOTHROW(MapLoader::Load("StemMap", "StemMap.fomap", map_buf, meta, hashes, [](ident_t, ptr<const ProtoCritter>, ptr<const map<string_view, string_view>>) {}, [&](ident_t, ptr<const ProtoItem>, ptr<const map<string_view, string_view>>) { item_calls++; }));

        CHECK(item_calls == 1);
    }

    SECTION("RejectsBakedMapFileThatIsNotCurrentFormat")
    {
        auto make_baked_map_header = [](uint32_t magic, uint32_t version) {
            vector<uint8_t> data;
            auto writer = data_writer(data);
            writer.write<uint32_t>(magic);
            writer.write<uint32_t>(version);
            writer.write<uint32_t>(uint32_t {7});
            return data;
        };

        vector<uint8_t> current = make_baked_map_header(BAKED_MAP_FILE_MAGIC, BAKED_MAP_FILE_VERSION);
        auto current_reader = data_reader {current};
        CHECK_NOTHROW(MapLoader::ReadBakedFileHeader(current_reader, "TestMap"));
        CHECK(current_reader.read<uint32_t>() == 7);

        vector<uint8_t> future = make_baked_map_header(BAKED_MAP_FILE_MAGIC, BAKED_MAP_FILE_VERSION + 1);
        auto future_reader = data_reader {future};
        CHECK_THROWS_AS(MapLoader::ReadBakedFileHeader(future_reader, "TestMap"), MapLoaderException);

        // The pre-header layout opened with the hash count, so a stale resource file is caught by the magic
        vector<uint8_t> headerless;
        auto headerless_writer = data_writer(headerless);
        headerless_writer.write<uint32_t>(uint32_t {2});
        headerless_writer.write<uint32_t>(uint32_t {4});
        headerless_writer.write_string_bytes("Item");
        auto headerless_reader = data_reader {headerless};
        CHECK_THROWS_AS(MapLoader::ReadBakedFileHeader(headerless_reader, "TestMap"), MapLoaderException);

        vector<uint8_t> truncated;
        auto truncated_reader = data_reader {truncated};
        CHECK_THROWS(MapLoader::ReadBakedFileHeader(truncated_reader, "TestMap"));
    }
}

FO_END_NAMESPACE
