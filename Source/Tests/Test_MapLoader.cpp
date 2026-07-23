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

static auto GetTestMapLoaderRegistrator(EngineMetadata& meta, string_view type_name) -> ptr<const PropertyRegistrator>
{
    auto registrator = meta.GetPropertyRegistrator(type_name);
    REQUIRE(static_cast<bool>(registrator));
    return registrator;
}

TEST_CASE("MapLoader")
{
    SECTION("RejectsMapsWithoutProtoMapSection")
    {
        EngineMetadata meta {[] { }};
        HashStorage hashes {};

        CHECK_THROWS_AS(MapLoader::Load("LegacyMap", "LegacyMap.fomap", "[Header]\n[Tiles]\n[Objects]\n", meta, hashes, [](ident_t, ptr<const ProtoCritter>, ptr<const map<string_view, string_view>>) { }, [](ident_t, ptr<const ProtoItem>, ptr<const map<string_view, string_view>>) { }), MapLoaderException);
        CHECK_THROWS_AS(MapLoader::Load("BrokenMap", "BrokenMap.fomap", "[$Name/Critter]\n$Proto = CritterOne\n", meta, hashes, [](ident_t, ptr<const ProtoCritter>, ptr<const map<string_view, string_view>>) { }, [](ident_t, ptr<const ProtoItem>, ptr<const map<string_view, string_view>>) { }), MapLoaderException);
    }

    SECTION("RejectsBareContentSections")
    {
        EngineMetadata meta {[] { }};
        HashStorage hashes {};

        const string map_buf = "[ProtoMap]\n"
                               "$Name = TestMap\n"
                               "[Critter]\n"
                               "$Id = 1\n"
                               "$Proto = CritterOne\n";

        CHECK_THROWS_AS(MapLoader::Load("TestMap", "TestMap.fomap", map_buf, meta, hashes, [](ident_t, ptr<const ProtoCritter>, ptr<const map<string_view, string_view>>) { }, [](ident_t, ptr<const ProtoItem>, ptr<const map<string_view, string_view>>) { }), MapLoaderException);

        const string bare_slash_map_buf = "[ProtoMap]\n"
                                          "$Name = TestMap\n"
                                          "[/Item]\n"
                                          "$Id = 1\n"
                                          "$Proto = ItemOne\n";

        CHECK_THROWS_AS(MapLoader::Load("TestMap", "TestMap.fomap", bare_slash_map_buf, meta, hashes, [](ident_t, ptr<const ProtoCritter>, ptr<const map<string_view, string_view>>) { }, [](ident_t, ptr<const ProtoItem>, ptr<const map<string_view, string_view>>) { }), MapLoaderException);
    }

    SECTION("RejectsNestedSectionsAddressedToUndeclaredMap")
    {
        EngineMetadata meta {[] { }};
        HashStorage hashes {};

        const string map_buf = "[ProtoMap]\n"
                               "$Name = TestMap\n"
                               "[OtherMap/Item]\n"
                               "$Id = 1\n"
                               "$Proto = ItemOne\n";

        CHECK_THROWS_AS(MapLoader::Load("TestMap", "TestMap.fomap", map_buf, meta, hashes, [](ident_t, ptr<const ProtoCritter>, ptr<const map<string_view, string_view>>) { }, [](ident_t, ptr<const ProtoItem>, ptr<const map<string_view, string_view>>) { }), MapLoaderException);
    }

    SECTION("RejectsUnknownNestedSectionType")
    {
        EngineMetadata meta {[] { }};
        HashStorage hashes {};

        const string map_buf = "[ProtoMap]\n"
                               "$Name = TestMap\n"
                               "[$Name/Tile]\n"
                               "$Id = 1\n";

        CHECK_THROWS_AS(MapLoader::Load("TestMap", "TestMap.fomap", map_buf, meta, hashes, [](ident_t, ptr<const ProtoCritter>, ptr<const map<string_view, string_view>>) { }, [](ident_t, ptr<const ProtoItem>, ptr<const map<string_view, string_view>>) { }), MapLoaderException);
    }

    SECTION("RejectsLoadOfMapThatIsNotDeclared")
    {
        EngineMetadata meta {[] { }};
        HashStorage hashes {};

        const string map_buf = "[ProtoMap]\n"
                               "$Name = TestMap\n";

        CHECK_THROWS_AS(MapLoader::Load("AnotherMap", "AnotherMap.fomap", map_buf, meta, hashes, [](ident_t, ptr<const ProtoCritter>, ptr<const map<string_view, string_view>>) { }, [](ident_t, ptr<const ProtoItem>, ptr<const map<string_view, string_view>>) { }), MapLoaderException);
    }

    SECTION("AnonymousAnchorResolvesToFileStemNotRequestedName")
    {
        EngineMetadata meta {[] { }};
        InitTestMapLoaderMetadata(meta);
        auto item_proto = SafeAlloc::MakeRefCounted<ProtoItem>(meta.Hashes.ToHashedString("TestItem"), GetTestMapLoaderRegistrator(meta, "Item"));
        meta.RegisterProto(meta.Hashes.ToHashedString("Item"), item_proto);

        HashStorage hashes {};

        const string map_buf = "[ProtoMap]\n"
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
        const auto load_map = [&](string_view map_name) {
            loaded_kinds.clear();
            CHECK_NOTHROW(MapLoader::Load(
                map_name, "Maps/Zones.fomap", map_buf, meta, hashes, [](ident_t, ptr<const ProtoCritter>, ptr<const map<string_view, string_view>>) { },
                [&](ident_t, ptr<const ProtoItem>, ptr<const map<string_view, string_view>> kv) { loaded_kinds.emplace_back(kv->at("Kind")); }));
        };

        load_map("Zones");
        CHECK(loaded_kinds == vector<string> {"FromStemMap"});

        load_map("ZoneB");
        CHECK(loaded_kinds == vector<string> {"FromZoneB"});
    }

    SECTION("MissingProtosAccumulateErrorsAndSkipCallbacks")
    {
        EngineMetadata meta {[] { }};
        HashStorage hashes {};
        size_t critter_calls = 0;
        size_t item_calls = 0;

        const string map_buf = "[ProtoMap]\n"
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
        HashStorage hashes {};

        const string map_buf = "[ProtoMap]\n"
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
        auto critter_proto = SafeAlloc::MakeRefCounted<ProtoCritter>(meta.Hashes.ToHashedString("TestCritter"), GetTestMapLoaderRegistrator(meta, "Critter"));
        auto item_proto = SafeAlloc::MakeRefCounted<ProtoItem>(meta.Hashes.ToHashedString("TestItem"), GetTestMapLoaderRegistrator(meta, "Item"));
        meta.RegisterProto(meta.Hashes.ToHashedString("Critter"), critter_proto);
        meta.RegisterProto(meta.Hashes.ToHashedString("Item"), item_proto);

        HashStorage hashes {};
        vector<ident_t> critter_ids;
        vector<ident_t> item_ids;
        vector<string> critter_proto_names;
        vector<string> item_proto_names;

        const string map_buf = "[ProtoMap]\n"
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
            "TestMap", map_buf, meta, hashes,
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
        auto item_proto = SafeAlloc::MakeRefCounted<ProtoItem>(meta.Hashes.ToHashedString("TestItem"), GetTestMapLoaderRegistrator(meta, "Item"));
        meta.RegisterProto(meta.Hashes.ToHashedString("Item"), item_proto);

        HashStorage hashes {};

        const string map_buf = "[ProtoMap]\n"
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
        const auto load_map = [&](string_view map_name) {
            loaded_kinds.clear();
            CHECK_NOTHROW(MapLoader::Load(
                map_name, map_buf, meta, hashes, [](ident_t, ptr<const ProtoCritter>, ptr<const map<string_view, string_view>>) { },
                [&](ident_t, ptr<const ProtoItem>, ptr<const map<string_view, string_view>> kv) { loaded_kinds.emplace_back(kv->at("Kind")); }));
        };

        load_map("MapOne");
        CHECK(loaded_kinds == vector<string> {"FromOne"});

        load_map("MapTwo");
        CHECK(loaded_kinds == vector<string> {"FromTwo", "FromTwoAsWell"});
    }

    SECTION("EnumerateMapsResolvesAnchorNames")
    {
        const string multi_buf = "[ProtoMap]\n"
                                 "$Name = MapOne\n"
                                 "[$Name/Item]\n"
                                 "$Id = 1\n"
                                 "[ProtoMap]\n"
                                 "$Name = MapTwo\n";

        CHECK(MapLoader::EnumerateMaps("Multi.fomap", multi_buf) == vector<string> {"MapOne", "MapTwo"});

        const string anonymous_buf = "[ProtoMap]\n"
                                     "Outside = True\n";

        CHECK(MapLoader::EnumerateMaps("Maps/StemMap.fomap", anonymous_buf) == vector<string> {"StemMap"});

        const string anonymous_multi_buf = "[ProtoMap]\n"
                                           "$Name = MapOne\n"
                                           "[ProtoMap]\n"
                                           "Outside = True\n";

        CHECK(MapLoader::EnumerateMaps("Multi.fomap", anonymous_multi_buf) == vector<string> {"MapOne", "Multi"});

        const string colliding_anonymous_buf = "[ProtoMap]\n"
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
        auto item_proto = SafeAlloc::MakeRefCounted<ProtoItem>(meta.Hashes.ToHashedString("TestItem"), GetTestMapLoaderRegistrator(meta, "Item"));
        meta.RegisterProto(meta.Hashes.ToHashedString("Item"), item_proto);

        HashStorage hashes {};
        size_t item_calls = 0;

        const string map_buf = "[ProtoMap]\n"
                               "Outside = True\n"
                               "[$Name/Item]\n"
                               "$Id = 1\n"
                               "$Proto = TestItem\n";

        CHECK_NOTHROW(MapLoader::Load("StemMap", "StemMap.fomap", map_buf, meta, hashes, [](ident_t, ptr<const ProtoCritter>, ptr<const map<string_view, string_view>>) { }, [&](ident_t, ptr<const ProtoItem>, ptr<const map<string_view, string_view>>) { item_calls++; }));

        CHECK(item_calls == 1);
    }
}

FO_END_NAMESPACE
