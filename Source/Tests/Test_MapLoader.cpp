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

TEST_CASE("MapLoader")
{
    SECTION("RejectsOldMapFormat")
    {
        EngineMetadata meta {[] { }};
        HashStorage hashes;

        CHECK_THROWS_AS(MapLoader::Load("LegacyMap", "[Header]\n[Tiles]\n[Objects]\n", meta, hashes, [](ident_t, const ProtoCritter*, const map<string_view, string_view>&) { }, [](ident_t, const ProtoItem*, const map<string_view, string_view>&) { }), MapLoaderException);
    }

    SECTION("RejectsMapsWithoutProtoMapSection")
    {
        EngineMetadata meta {[] { }};
        HashStorage hashes;

        CHECK_THROWS_AS(MapLoader::Load("BrokenMap", "[Critter]\n$Proto = CritterOne\n", meta, hashes, [](ident_t, const ProtoCritter*, const map<string_view, string_view>&) { }, [](ident_t, const ProtoItem*, const map<string_view, string_view>&) { }), MapLoaderException);
    }

    SECTION("MissingProtosAccumulateErrorsAndSkipCallbacks")
    {
        EngineMetadata meta {[] { }};
        HashStorage hashes;
        size_t critter_calls = 0;
        size_t item_calls = 0;

        const string map_buf = "[ProtoMap]\n"
                               "$Name = TestMap\n"
                               "[Critter]\n"
                               "$Id = 1\n"
                               "$Proto = MissingCritter\n"
                               "[Item]\n"
                               "$Id = 2\n"
                               "$Proto = MissingItem\n";

        CHECK_THROWS_AS(MapLoader::Load("MissingProtoMap", map_buf, meta, hashes, [&](ident_t, const ProtoCritter*, const map<string_view, string_view>&) { critter_calls++; }, [&](ident_t, const ProtoItem*, const map<string_view, string_view>&) { item_calls++; }), MapLoaderException);

        CHECK(critter_calls == 0);
        CHECK(item_calls == 0);
    }

    SECTION("MissingProtoFieldAlsoFailsLoad")
    {
        EngineMetadata meta {[] { }};
        HashStorage hashes;

        const string map_buf = "[ProtoMap]\n"
                               "$Name = TestMap\n"
                               "[Critter]\n"
                               "$Id = 1\n"
                               "[Item]\n"
                               "$Id = 2\n";

        CHECK_THROWS_AS(MapLoader::Load("InvalidEntriesMap", map_buf, meta, hashes, [](ident_t, const ProtoCritter*, const map<string_view, string_view>&) { }, [](ident_t, const ProtoItem*, const map<string_view, string_view>&) { }), MapLoaderException);
    }

    SECTION("ValidEntriesLoadAndDuplicateIdsAreReassigned")
    {
        EngineMetadata meta {[] { }};
        InitTestMapLoaderMetadata(meta);
        auto critter_proto = SafeAlloc::MakeRefCounted<ProtoCritter>(meta.Hashes.ToHashedString("TestCritter"), meta.GetPropertyRegistrator("Critter"));
        auto item_proto = SafeAlloc::MakeRefCounted<ProtoItem>(meta.Hashes.ToHashedString("TestItem"), meta.GetPropertyRegistrator("Item"));
        meta.RegisterProto(meta.Hashes.ToHashedString("Critter"), critter_proto);
        meta.RegisterProto(meta.Hashes.ToHashedString("Item"), item_proto);

        HashStorage hashes;
        vector<ident_t> critter_ids;
        vector<ident_t> item_ids;
        vector<string> critter_proto_names;
        vector<string> item_proto_names;

        const string map_buf = "[ProtoMap]\n"
                               "$Name = TestMap\n"
                               "[Critter]\n"
                               "$Id = 0\n"
                               "$Proto = TestCritter\n"
                               "Name = One\n"
                               "[Critter]\n"
                               "$Id = 0\n"
                               "$Proto = TestCritter\n"
                               "Name = Two\n"
                               "[Item]\n"
                               "$Id = 2\n"
                               "$Proto = TestItem\n"
                               "Kind = Alpha\n"
                               "[Item]\n"
                               "$Id = 2\n"
                               "$Proto = TestItem\n"
                               "Kind = Beta\n";

        CHECK_NOTHROW(MapLoader::Load(
            "ValidMap", map_buf, meta, hashes,
            [&](ident_t id, const ProtoCritter* proto, const map<string_view, string_view>& kv) {
                critter_ids.emplace_back(id);
                critter_proto_names.emplace_back(proto->GetProtoId().as_str());
                CHECK(kv.at("$Proto") == "TestCritter");
            },
            [&](ident_t id, const ProtoItem* proto, const map<string_view, string_view>& kv) {
                item_ids.emplace_back(id);
                item_proto_names.emplace_back(proto->GetProtoId().as_str());
                CHECK(kv.at("$Proto") == "TestItem");
            }));

        CHECK(critter_ids == vector<ident_t> {ident_t {1}, ident_t {2}});
        CHECK(item_ids == vector<ident_t> {ident_t {3}, ident_t {4}});
        CHECK(critter_proto_names == vector<string> {"TestCritter", "TestCritter"});
        CHECK(item_proto_names == vector<string> {"TestItem", "TestItem"});
    }
}

FO_END_NAMESPACE
