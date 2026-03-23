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
//

#include "catch_amalgamated.hpp"

#include "EngineBase.h"

FO_BEGIN_NAMESPACE

using MigrationRulesMap = unordered_map<hstring, unordered_map<hstring, unordered_map<hstring, hstring>>>;

static void AddTestMigrationRule(EngineMetadata& meta, string_view target, string_view replacement)
{
    meta.RegisterMigrationRule("Property", "Item", target, replacement);
}

static auto HashTestMigrationToken(EngineMetadata& meta, string_view value) -> hstring
{
    return meta.Hashes.ToHashedString(value);
}

static auto ResolveTestMigrationRule(EngineMetadata& meta, string_view target) -> optional<hstring>
{
    return meta.CheckMigrationRule(HashTestMigrationToken(meta, "Property"), HashTestMigrationToken(meta, "Item"), HashTestMigrationToken(meta, target));
}

TEST_CASE("EngineMetadata")
{
    SECTION("BuiltinProtoEntityTypesUseDedicatedProtoFlag")
    {
        EngineMetadata meta {[] { }};
        meta.RegisterSide(EngineSideKind::ServerSide);
        meta.RegisterEntityType("Map", true, false, true, true, true);

        CHECK_FALSE(meta.IsFixedType("ProtoMap"));
        CHECK_FALSE(meta.IsFixedType("StaticMap"));
        CHECK_FALSE(meta.IsFixedType("AbstractMap"));

        const auto& proto_map_type = meta.GetBaseType("ProtoMap");
        CHECK(proto_map_type.IsEntity);
        CHECK(proto_map_type.IsEntityProto);
        CHECK_FALSE(proto_map_type.IsFixedType);
        CHECK(proto_map_type.Size == sizeof(hstring::hash_t));
    }

    SECTION("MissingMigrationRuleReturnsEmpty")
    {
        EngineMetadata meta {[] { }};

        CHECK_FALSE(ResolveTestMigrationRule(meta, "OldField").has_value());
    }

    SECTION("MigrationRuleChainsResolveToFinalTarget")
    {
        EngineMetadata meta {[] { }};
        AddTestMigrationRule(meta, "OldField", "MiddleField");
        AddTestMigrationRule(meta, "MiddleField", "NewField");

        const optional<hstring> resolved = ResolveTestMigrationRule(meta, "OldField");
        REQUIRE(resolved.has_value());
        CHECK(resolved.value() == HashTestMigrationToken(meta, "NewField"));
    }

    SECTION("DuplicateMigrationRuleTargetRejected")
    {
        EngineMetadata meta {[] { }};
        AddTestMigrationRule(meta, "OldField", "MiddleField");

        CHECK_THROWS_AS(AddTestMigrationRule(meta, "OldField", "NewField"), AssertationException);
    }

    SECTION("SelfReferencingMigrationRuleRejected")
    {
        EngineMetadata meta {[] { }};

        CHECK_THROWS_AS(AddTestMigrationRule(meta, "OldField", "OldField"), AssertationException);
    }

    SECTION("CyclicMigrationRuleChainRejected")
    {
        EngineMetadata meta {[] { }};
        AddTestMigrationRule(meta, "OldField", "MiddleField");
        AddTestMigrationRule(meta, "MiddleField", "NewField");

        CHECK_THROWS_AS(AddTestMigrationRule(meta, "NewField", "OldField"), AssertationException);
    }

    SECTION("BulkMigrationRulesWithCycleRejected")
    {
        EngineMetadata meta {[] { }};
        MigrationRulesMap migration_rules {};
        const hstring rule_name = HashTestMigrationToken(meta, "Property");
        const hstring extra_info = HashTestMigrationToken(meta, "Item");

        migration_rules[rule_name][extra_info].emplace(HashTestMigrationToken(meta, "OldField"), HashTestMigrationToken(meta, "MiddleField"));
        migration_rules[rule_name][extra_info].emplace(HashTestMigrationToken(meta, "MiddleField"), HashTestMigrationToken(meta, "OldField"));

        CHECK_THROWS_AS(meta.RegisterMigrationRules(std::move(migration_rules)), AssertationException);
    }
}

FO_END_NAMESPACE
