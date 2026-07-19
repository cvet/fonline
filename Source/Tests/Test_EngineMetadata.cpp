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

#include "AnimationInfo.h"
#include "EngineBase.h"
#include "Test_BakerHelpers.h"
#include "TextPack.h"

FO_BEGIN_NAMESPACE

using MigrationRulesMap = unordered_map<hstring, unordered_map<hstring, unordered_map<hstring, hstring>>>;

static auto MakeSpriteAnimationInfoResources() -> FileSystem
{
    FO_STACK_TRACE_ENTRY();

    SpriteInfoFileEntry entry {
        .SourcePath = "Art/Test.png",
        .ResourcePath = "Art/Test.png",
        .Info =
            {
                .FrameCount = 2,
                .Duration = std::chrono::milliseconds {75},
                .Directions =
                    {
                        SpriteDirInfo {
                            .Frames =
                                {
                                    SpriteFrameInfo {.Offset = {-3, 4}, .Size = {2, 1}, .NextOffset = {5, -6}},
                                    SpriteFrameInfo {.SharedFrameIndex = 0, .Offset = {-3, 4}, .Size = {2, 1}, .NextOffset = {5, -6}},
                                },
                        },
                    },
            },
    };

    auto source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("SpriteInfoTestPack");
    source->AddFile("SpriteInfo/TestPack.foinfo", WriteSpriteInfoFile({entry}));

    FileSystem resources;
    resources.AddCustomSource(std::move(source));
    return resources;
}

#if FO_ENABLE_3D

static constexpr string_view VALID_ANIMATION_DURATIONS = R"(StateAnimations = 1
ActionAnimations = 3
DurationsMs = 500
)";

static constexpr string_view VALID_ANIMATION_BOUNDS = R"(BoundsStateAnimations = 1
BoundsActionAnimations = 1
BoundsMinX = -1
BoundsMinY = -1
BoundsMinZ = 0
BoundsMaxX = 1
BoundsMaxY = 1
BoundsMaxZ = 2
)";

static constexpr string_view VALID_MODEL_ANIMATION_INFO = R"([Critters/Test.fo3d]
BoundsVersion = 2
ModelBoundsMinX = -2
ModelBoundsMinY = -1
ModelBoundsMinZ = 0
ModelBoundsMaxX = 2
ModelBoundsMaxY = 1
ModelBoundsMaxZ = 4
ViewBoundsMinX = -1
ViewBoundsMinY = -0.5
ViewBoundsMinZ = 0
ViewBoundsMaxX = 1
ViewBoundsMaxY = 0.5
ViewBoundsMaxZ = 3
StateAnimations = 1 1
ActionAnimations = 3 5
DurationsMs = 500 250
BoundsStateAnimations = 1
BoundsActionAnimations = 1
BoundsMinX = -1.5
BoundsMinY = -0.75
BoundsMinZ = 0.25
BoundsMaxX = 1.5
BoundsMaxY = 0.75
BoundsMaxZ = 3.5

[Critters/Static.fo3d]
BoundsVersion = 2
ModelBoundsMinX = -4
ModelBoundsMinY = -3
ModelBoundsMinZ = -2
ModelBoundsMaxX = 4
ModelBoundsMaxY = 3
ModelBoundsMaxZ = 2
ViewBoundsMinX = -2
ViewBoundsMinY = -1
ViewBoundsMinZ = -0.5
ViewBoundsMaxX = 2
ViewBoundsMaxY = 1
ViewBoundsMaxZ = 1.5

)";

static auto MakeModelAnimationInfoResources(string_view content) -> FileSystem
{
    FO_STACK_TRACE_ENTRY();

    auto source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("AnimationInfoTestPack");
    source->AddFile("ModelAnimationInfo.foinfo", content);

    FileSystem resources;
    resources.AddCustomSource(std::move(source));
    return resources;
}

static auto MakeModelAnimationInfoDocument(string_view animation_fields, string_view bounds_version = "2", string_view model_bounds_max_x = "2") -> string
{
    FO_STACK_TRACE_ENTRY();

    return strex(R"([Critters/Test.fo3d]
BoundsVersion = {}
ModelBoundsMinX = -2
ModelBoundsMinY = -1
ModelBoundsMinZ = 0
ModelBoundsMaxX = {}
ModelBoundsMaxY = 1
ModelBoundsMaxZ = 4
ViewBoundsMinX = -1
ViewBoundsMinY = -0.5
ViewBoundsMinZ = 0
ViewBoundsMaxX = 1
ViewBoundsMaxY = 0.5
ViewBoundsMaxZ = 3
{}
)",
        bounds_version, model_bounds_max_x, animation_fields);
}

static void CheckModelAnimationInfoRejected(string_view content)
{
    FO_STACK_TRACE_ENTRY();

    EngineMetadata meta {[] { }};
    FileSystem resources = MakeModelAnimationInfoResources(content);
    CHECK_THROWS_AS(meta.RegisterAnimationInfo(resources), VerificationException);
}

#endif

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

    SECTION("ValueTypeLayoutMatchesNativeTextPackKey")
    {
        EngineMetadata meta {[] { }};
        meta.RegisterValueType("TextPackName");
        meta.RegisterValueType("TextPackKey");
        meta.RegisterValueTypeLayout("TextPackName", {{"Name", "hstring"}});
        meta.RegisterValueTypeLayout("TextPackKey", {{"Collection", "TextPackName"}, {"Key1", "hstring"}, {"Key2", "hstring"}, {"Key3", "hstring"}});

        const BaseTypeDesc& hstring_type = meta.GetBaseType("hstring");
        CHECK(hstring_type.Size == sizeof(hstring::hash_t));
        CHECK(hstring_type.Size == sizeof(hstring));

        const BaseTypeDesc& text_key_type = meta.GetBaseType("TextPackKey");
        REQUIRE(text_key_type.StructLayout);
        CHECK(text_key_type.Size == sizeof(hstring::hash_t) * 4);
        CHECK(text_key_type.Size == sizeof(TextPackKey));

        const auto& fields = text_key_type.StructLayout->Fields;
        REQUIRE(fields.size() == 4);
        CHECK(fields[0].Offset == 0);
        CHECK(fields[1].Offset == sizeof(hstring::hash_t));
        CHECK(fields[2].Offset == sizeof(hstring::hash_t) * 2);
        CHECK(fields[3].Offset == sizeof(hstring::hash_t) * 3);
        CHECK(fields[0].Offset == offsetof(TextPackKey, Collection));
        CHECK(fields[1].Offset == offsetof(TextPackKey, Key1));
        CHECK(fields[2].Offset == offsetof(TextPackKey, Key2));
        CHECK(fields[3].Offset == offsetof(TextPackKey, Key3));
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

        CHECK_THROWS_AS(AddTestMigrationRule(meta, "OldField", "NewField"), VerificationException);
    }

    SECTION("SelfReferencingMigrationRuleRejected")
    {
        EngineMetadata meta {[] { }};

        CHECK_THROWS_AS(AddTestMigrationRule(meta, "OldField", "OldField"), VerificationException);
    }

    SECTION("CyclicMigrationRuleChainRejected")
    {
        EngineMetadata meta {[] { }};
        AddTestMigrationRule(meta, "OldField", "MiddleField");
        AddTestMigrationRule(meta, "MiddleField", "NewField");

        CHECK_THROWS_AS(AddTestMigrationRule(meta, "NewField", "OldField"), VerificationException);
    }

    SECTION("BulkMigrationRulesWithCycleRejected")
    {
        EngineMetadata meta {[] { }};
        MigrationRulesMap migration_rules {};
        const hstring rule_name = HashTestMigrationToken(meta, "Property");
        const hstring extra_info = HashTestMigrationToken(meta, "Item");

        migration_rules[rule_name][extra_info].emplace(HashTestMigrationToken(meta, "OldField"), HashTestMigrationToken(meta, "MiddleField"));
        migration_rules[rule_name][extra_info].emplace(HashTestMigrationToken(meta, "MiddleField"), HashTestMigrationToken(meta, "OldField"));

        CHECK_THROWS_AS(meta.RegisterMigrationRules(std::move(migration_rules)), VerificationException);
    }
}

TEST_CASE("EngineMetadataSpriteAnimationInfo")
{
    SECTION("ParsesSpritePayloadWithout3DDependency")
    {
        EngineMetadata meta {[] { }};
        FileSystem resources = MakeSpriteAnimationInfoResources();
        meta.RegisterAnimationInfo(resources);

        const auto info = meta.GetAnimationInfo(meta.Hashes.ToHashedString("Art/Test.png"));
        REQUIRE(static_cast<bool>(info));
        REQUIRE(info->Sprite.has_value());
        const SpriteInfo& sprite_info = *info->Sprite;
        CHECK(sprite_info.FrameCount == 2);
        CHECK(sprite_info.Duration == std::chrono::milliseconds {75});
        REQUIRE(sprite_info.Directions.size() == 1);
        REQUIRE(sprite_info.Directions.front().Frames.size() == 2);
        CHECK(sprite_info.Directions.front().Frames.front().Offset == ipos32 {-3, 4});
        REQUIRE(sprite_info.Directions.front().Frames.back().SharedFrameIndex.has_value());
        CHECK(*sprite_info.Directions.front().Frames.back().SharedFrameIndex == 0);
        CHECK_FALSE(static_cast<bool>(meta.GetAnimationInfo(meta.Hashes.ToHashedString("Art/Missing.png"))));
    }

    SECTION("RejectsUnsupportedSpriteInfoVersion")
    {
        FileSystem resources = MakeSpriteAnimationInfoResources();
        const File info_file = resources.ReadFile("SpriteInfo/TestPack.foinfo");
        string invalid_info = info_file.GetStr();
        invalid_info = strex(invalid_info).replace("InfoVersion = 1", "InfoVersion = 2").str();

        auto source = SafeAlloc::MakeUnique<BakerTests::MemoryDataSource>("InvalidSpriteInfoTestPack");
        source->AddFile("SpriteInfo/TestPack.foinfo", invalid_info);
        FileSystem invalid_resources;
        invalid_resources.AddCustomSource(std::move(source));

        EngineMetadata meta {[] { }};
        CHECK_THROWS_AS(meta.RegisterAnimationInfo(invalid_resources), VerificationException);
    }
}

#if FO_ENABLE_3D

TEST_CASE("EngineMetadataModelAnimationInfo")
{
    SECTION("ParsesCompleteModelAndKeepsDurationAndBoundsDomainsIndependent")
    {
        EngineMetadata meta {[] { }};
        FileSystem resources = MakeModelAnimationInfoResources(VALID_MODEL_ANIMATION_INFO);
        meta.RegisterAnimationInfo(resources);

        const hstring model_name = meta.Hashes.ToHashedString("Critters/Test.fo3d");
        const auto info = meta.GetAnimationInfo(model_name);
        REQUIRE(static_cast<bool>(info));
        REQUIRE(info->Model.has_value());
        const ModelAnimationInfo& model_info = *info->Model;

        CHECK(model_info.ModelBounds.Min.x == -2.0f);
        CHECK(model_info.ModelBounds.Min.y == -1.0f);
        CHECK(model_info.ModelBounds.Min.z == 0.0f);
        CHECK(model_info.ModelBounds.Max.x == 2.0f);
        CHECK(model_info.ModelBounds.Max.y == 1.0f);
        CHECK(model_info.ModelBounds.Max.z == 4.0f);
        CHECK(model_info.ViewBounds.Min.x == -1.0f);
        CHECK(model_info.ViewBounds.Min.y == -0.5f);
        CHECK(model_info.ViewBounds.Min.z == 0.0f);
        CHECK(model_info.ViewBounds.Max.x == 1.0f);
        CHECK(model_info.ViewBounds.Max.y == 0.5f);
        CHECK(model_info.ViewBounds.Max.z == 3.0f);

        const auto walk_key = std::make_pair(CritterStateAnim::Unarmed, CritterActionAnim::Walk);
        const auto run_key = std::make_pair(CritterStateAnim::Unarmed, CritterActionAnim::Run);
        const auto idle_key = std::make_pair(CritterStateAnim::Unarmed, CritterActionAnim::Idle);
        const auto walk_duration = model_info.AnimationDurations.find(walk_key);
        const auto run_duration = model_info.AnimationDurations.find(run_key);
        REQUIRE(walk_duration != model_info.AnimationDurations.end());
        REQUIRE(run_duration != model_info.AnimationDurations.end());
        CHECK(walk_duration->second.milliseconds() == 500);
        CHECK(run_duration->second.milliseconds() == 250);

        CHECK_FALSE(model_info.AnimationBounds.contains(walk_key));
        CHECK_FALSE(model_info.AnimationBounds.contains(run_key));
        CHECK_FALSE(model_info.AnimationDurations.contains(idle_key));

        const auto idle_bounds = model_info.AnimationBounds.find(idle_key);
        REQUIRE(idle_bounds != model_info.AnimationBounds.end());
        CHECK(idle_bounds->second.Min.x == -1.5f);
        CHECK(idle_bounds->second.Min.y == -0.75f);
        CHECK(idle_bounds->second.Min.z == 0.25f);
        CHECK(idle_bounds->second.Max.x == 1.5f);
        CHECK(idle_bounds->second.Max.y == 0.75f);
        CHECK(idle_bounds->second.Max.z == 3.5f);
    }

    SECTION("AcceptsStaticModelAndReturnsNullForMissingModel")
    {
        EngineMetadata meta {[] { }};
        FileSystem resources = MakeModelAnimationInfoResources(VALID_MODEL_ANIMATION_INFO);
        meta.RegisterAnimationInfo(resources);

        const auto static_info = meta.GetAnimationInfo(meta.Hashes.ToHashedString("Critters/Static.fo3d"));
        REQUIRE(static_cast<bool>(static_info));
        REQUIRE(static_info->Model.has_value());
        const ModelAnimationInfo& static_model_info = *static_info->Model;
        CHECK(static_model_info.AnimationDurations.empty());
        CHECK(static_model_info.AnimationBounds.empty());
        CHECK(static_model_info.ModelBounds.Min.x == -4.0f);
        CHECK(static_model_info.ViewBounds.Max.z == 1.5f);

        CHECK_FALSE(static_cast<bool>(meta.GetAnimationInfo(meta.Hashes.ToHashedString("Critters/Missing.fo3d"))));
    }

    SECTION("MissingResourceLeavesLookupEmpty")
    {
        EngineMetadata meta {[] { }};
        FileSystem resources;
        CHECK_NOTHROW(meta.RegisterAnimationInfo(resources));
        CHECK_FALSE(static_cast<bool>(meta.GetAnimationInfo(meta.Hashes.ToHashedString("Critters/Test.fo3d"))));
    }

    SECTION("RejectsPresentEmptyResource")
    {
        CheckModelAnimationInfoRejected({});
    }

    SECTION("RejectsUnsupportedBoundsVersion")
    {
        CheckModelAnimationInfoRejected(MakeModelAnimationInfoDocument({}, "1"));
    }

    SECTION("RejectsInvalidAggregateBounds")
    {
        CheckModelAnimationInfoRejected(MakeModelAnimationInfoDocument({}, "2", "-3"));
    }

    SECTION("RejectsMismatchedDurationArrays")
    {
        CheckModelAnimationInfoRejected(MakeModelAnimationInfoDocument(strex("{}{}", R"(StateAnimations = 1 1
ActionAnimations = 3
DurationsMs = 500 250
)",
            VALID_ANIMATION_BOUNDS)));
    }

    SECTION("RejectsNonPositiveAnimationDuration")
    {
        CheckModelAnimationInfoRejected(MakeModelAnimationInfoDocument(strex("{}{}", R"(StateAnimations = 1
ActionAnimations = 3
DurationsMs = 0
)",
            VALID_ANIMATION_BOUNDS)));
    }

    SECTION("RejectsDuplicateDurationPair")
    {
        CheckModelAnimationInfoRejected(MakeModelAnimationInfoDocument(strex("{}{}", R"(StateAnimations = 1 1
ActionAnimations = 3 3
DurationsMs = 500 250
)",
            VALID_ANIMATION_BOUNDS)));
    }

    SECTION("RejectsPartialAnimationBoundsArrays")
    {
        CheckModelAnimationInfoRejected(MakeModelAnimationInfoDocument(strex("{}{}", VALID_ANIMATION_DURATIONS, R"(BoundsStateAnimations = 1
BoundsActionAnimations = 1
BoundsMinX = -1
)")));
    }

    SECTION("RejectsDuplicateAnimationBoundsPair")
    {
        CheckModelAnimationInfoRejected(MakeModelAnimationInfoDocument(strex("{}{}", VALID_ANIMATION_DURATIONS, R"(BoundsStateAnimations = 1 1
BoundsActionAnimations = 1 1
BoundsMinX = -1 -1
BoundsMinY = -1 -1
BoundsMinZ = 0 0
BoundsMaxX = 1 1
BoundsMaxY = 1 1
BoundsMaxZ = 2 2
)")));
    }

    SECTION("AcceptsAnimationDurationsWithoutAnimationBounds")
    {
        EngineMetadata meta {[] { }};
        FileSystem resources = MakeModelAnimationInfoResources(MakeModelAnimationInfoDocument(VALID_ANIMATION_DURATIONS));
        meta.RegisterAnimationInfo(resources);

        const auto info = meta.GetAnimationInfo(meta.Hashes.ToHashedString("Critters/Test.fo3d"));
        REQUIRE(static_cast<bool>(info));
        REQUIRE(info->Model.has_value());
        const ModelAnimationInfo& model_info = *info->Model;
        REQUIRE(model_info.AnimationDurations.size() == 1);
        CHECK(model_info.AnimationDurations.at({CritterStateAnim::Unarmed, CritterActionAnim::Walk}).milliseconds() == 500);
        CHECK(model_info.AnimationBounds.empty());
    }

    SECTION("AcceptsAnimationBoundsWithoutAnimationDurations")
    {
        EngineMetadata meta {[] { }};
        FileSystem resources = MakeModelAnimationInfoResources(MakeModelAnimationInfoDocument(VALID_ANIMATION_BOUNDS));
        meta.RegisterAnimationInfo(resources);

        const auto info = meta.GetAnimationInfo(meta.Hashes.ToHashedString("Critters/Test.fo3d"));
        REQUIRE(static_cast<bool>(info));
        REQUIRE(info->Model.has_value());
        const ModelAnimationInfo& model_info = *info->Model;
        CHECK(model_info.AnimationDurations.empty());
        REQUIRE(model_info.AnimationBounds.size() == 1);

        const ModelBounds3D& bounds = model_info.AnimationBounds.at({CritterStateAnim::Unarmed, CritterActionAnim::Idle});
        CHECK(bounds.Min.x == -1.0f);
        CHECK(bounds.Min.y == -1.0f);
        CHECK(bounds.Min.z == 0.0f);
        CHECK(bounds.Max.x == 1.0f);
        CHECK(bounds.Max.y == 1.0f);
        CHECK(bounds.Max.z == 2.0f);
    }

    SECTION("RejectsEntriesOutsideModelSection")
    {
        CheckModelAnimationInfoRejected(strex("Unexpected = value\n{}", MakeModelAnimationInfoDocument({})));
    }
}

#endif

FO_END_NAMESPACE
