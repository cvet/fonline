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
#include "CritterFramesCache.h"
#include "DefaultSprites.h"
#include "EffectManager.h"
#include "MapSprite.h"
#include "ModelAnimation.h"
#include "ResourceManager.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

namespace
{
    static auto MakeSpriteSheetTestSettings() -> GlobalSettings
    {
        GlobalSettings settings {false};
        settings.ApplyDefaultSettings();
        BakerTests::ApplySelfContainedClientSettings(settings);
        return settings;
    }

    // Every direction has distinct anchors and two-axis displacement. The shared record carries its own
    // placement/motion, deliberately different from the pixel-owner frame it references
    static auto MakeSpriteSheetTestResource(bool shared_frame) -> vector<uint8_t>
    {
        vector<uint8_t> data;
        data_writer writer {data};
        writer.write<uint8_t>(SPRITE_RESOURCE_MAGIC);
        writer.write<uint8_t>(SPRITE_RESOURCE_VERSION);
        writer.write<uint8_t>(numeric_cast<uint8_t>(GameSettings::MAP_DIR_COUNT));

        for (int32_t dir = 0; dir < GameSettings::MAP_DIR_COUNT; dir++) {
            writer.write<uint16_t>(uint16_t {3});
            writer.write<uint32_t>(uint32_t {120});

            for (int32_t frame = 0; frame < 3; frame++) {
                bool shared = shared_frame && frame == 1;
                int32_t source = shared ? 0 : frame;
                int32_t placement_x = dir + source;
                int32_t placement_y = -dir - source;
                int32_t motion_x = (dir + 1) * (source + 1);
                int32_t motion_y = -(dir + 1) * (source + 1);

                if (shared) {
                    placement_x = 100 + dir;
                    placement_y = -100 - dir;
                    motion_x = 50 + dir;
                    motion_y = -50 - dir;
                }

                writer.write<uint32_t>(uint32_t {40});
                writer.write<int32_t>(placement_x);
                writer.write<int32_t>(placement_y);
                writer.write<int16_t>(numeric_cast<int16_t>(motion_x));
                writer.write<int16_t>(numeric_cast<int16_t>(motion_y));
                writer.write<uint8_t>(shared ? uint8_t {1} : uint8_t {0});

                if (shared) {
                    writer.write<uint16_t>(uint16_t {0});
                    continue;
                }

                writer.write<uint16_t>(uint16_t {5});
                writer.write<uint16_t>(uint16_t {7});

                for (size_t pixel = 0; pixel < 35; pixel++) {
                    writer.write<ucolor>(ucolor {255, 255, 255, 255});
                }

                writer.write<uint8_t>(static_cast<uint8_t>(SpriteMeshKind::Quad));
            }
        }

        writer.write<uint8_t>(SPRITE_RESOURCE_MAGIC);
        return data;
    }

    class RecordingSpriteSheetFactory final : public DefaultSpriteFactory
    {
    public:
        using DefaultSpriteFactory::DefaultSpriteFactory;

        shared_ptr<Sprite> Prototype {};
        size_t LoadCount {};
        size_t CleanupCount {};
        size_t InvalidationCount {};
        size_t RetryCount {};
        function<void()> OnUpdate {};

        void Update() override
        {
            if (OnUpdate) {
                OnUpdate();
            }
        }

        void ClenupCache() override { CleanupCount++; }
        void InvalidateResource(hstring) override { InvalidationCount++; }
        void RetryFailedLoads() override { RetryCount++; }

        auto LoadSprite(hstring path, AtlasType atlas_type) -> shared_ptr<Sprite> override
        {
            LoadCount++;
            Prototype = DefaultSpriteFactory::LoadSprite(path, atlas_type);
            return Prototype;
        }
    };

    static auto MakeTimelineTestResource(const vector<vector<uint32_t>>& timelines) -> vector<uint8_t>
    {
        vector<uint8_t> data;
        data_writer writer {data};
        writer.write<uint8_t>(SPRITE_RESOURCE_MAGIC);
        writer.write<uint8_t>(SPRITE_RESOURCE_VERSION);
        writer.write<uint8_t>(numeric_cast<uint8_t>(timelines.size()));

        for (size_t dir = 0; dir < timelines.size(); dir++) {
            uint64_t total = std::accumulate(timelines[dir].begin(), timelines[dir].end(), uint64_t {});
            writer.write<uint16_t>(numeric_cast<uint16_t>(timelines[dir].size()));
            writer.write<uint32_t>(numeric_cast<uint32_t>(total));

            for (size_t frame = 0; frame < timelines[dir].size(); frame++) {
                writer.write<uint32_t>(timelines[dir][frame]);
                writer.write<int32_t>(numeric_cast<int32_t>(frame));
                writer.write<int32_t>(numeric_cast<int32_t>(dir));
                writer.write<int16_t>(int16_t {1});
                writer.write<int16_t>(int16_t {-1});
                writer.write<uint8_t>(uint8_t {0});
                writer.write<uint16_t>(uint16_t {1});
                writer.write<uint16_t>(uint16_t {1});
                writer.write<ucolor>(ucolor {numeric_cast<uint8_t>(frame + 1), 0, 0, 255});
                writer.write<uint8_t>(static_cast<uint8_t>(SpriteMeshKind::Quad));
            }
        }

        writer.write<uint8_t>(SPRITE_RESOURCE_MAGIC);

        return data;
    }

    class UpdatingTestSprite final : public Sprite
    {
    public:
        using Sprite::Sprite;
        using Sprite::StartUpdate;

        function<void()> OnUpdate {};

        auto Update() -> bool override
        {
            OnUpdate();
            return true;
        }

        auto FillData(ptr<RenderDrawBuffer> dbuf, const frect32& pos, const tuple<ucolor, ucolor>& colors) const -> size_t override
        {
            ignore_unused(dbuf, pos, colors);
            return 0;
        }
    };

    struct SpriteSheetFixture
    {
        GlobalSettings Settings {MakeSpriteSheetTestSettings()};
        unique_ptr<IAppWindow> Window {GetAppWindowStub(Settings)};
        FileSystem Resources {};
        GameTimer Timer {&Settings};
        hash_storage Hashes {};
        EffectManager Effects {&Settings, &Resources, Window->GetRender()};
        SpriteManager Sprites {&Settings, Window, &Timer, &Effects, &Hashes};
        ResourceManager Assets {&Resources, &Sprites};
        nptr<RecordingSpriteSheetFactory> Factory {};

        explicit SpriteSheetFixture(bool shared_frame = false) { InitializeResources(MakeSpriteSheetTestResource(shared_frame)); }

        explicit SpriteSheetFixture(const vector<vector<uint32_t>>& timelines) { InitializeResources(MakeTimelineTestResource(timelines)); }

        void InitializeResources(vector<uint8_t> sheet)
        {
            auto source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("SpriteSheetTest");
            source->AddFile("Sheet.fofrm", sheet);
            source->AddFile("art/critters/Testaa.fofrm", sheet);
            source->AddFile("art/critters/Testab.fofrm", std::move(sheet));
            source->AddFile("Quad.png", BakerTests::MakeMinimalBakedSprite(5, 7));

            for (string_view name : {"ImGui_Default", "2D_Default", "2D_NoDepth", "Primitive_Default", "Primitive_Light", "Primitive_Fog", "Flush_RenderTarget", "Flush_Primitive", "Flush_Map", "Flush_Light", "Flush_Fog", "3D_Skinned"}) {
                source->AddFile(strex("Effects/{}.fofx", name).str(), "[Effect]\nPasses = 1\n");
                source->AddFile(strex("Effects/{}.fofx-1-info", name).str(), "[EffectInfo]\n");
            }

            Resources.AddCustomSource(std::move(source));
            Effects.LoadDefaultEffects();
            auto factory = safe_alloc::make_unique<RecordingSpriteSheetFactory>(&Sprites, &Assets);
            Factory = factory.as_nptr();
            Assets.RegisterSpriteFactory(std::move(factory));
        }
    };

    class SpriteSheetTestResolver final : public AnimationResolver
    {
    public:
        shared_ptr<SpriteSheet> Frames {};
        int32_t Calls {};

        auto ResolveCritterAnimationSprite(hstring, CritterStateAnim, CritterActionAnim) -> shared_ptr<SpriteSheet> override
        {
            Calls++;

            if (!Frames) {
                return nullptr;
            }

            auto sprite = Frames->MakeCopy();
            return sprite.dyn_cast<SpriteSheet>();
        }

        auto ResolveCritterAnimationSubstitute(hstring, CritterStateAnim, CritterActionAnim, hstring&, CritterStateAnim&, CritterActionAnim&) -> bool override { return false; }
    };
}

template<typename T>
concept sprite_resource_loader = requires(T& manager, hstring path) { manager.LoadSprite(path, AtlasType::MapSprites); };

TEST_CASE("SpriteManagerDoesNotOwnResourceLoading", "[sprite]")
{
    CHECK_FALSE(sprite_resource_loader<SpriteManager>);
}

TEST_CASE("ResourceManagerReturnsIndependentSheetsFromTheFirstLoad", "[sprite]")
{
    SpriteSheetFixture fixture;
    auto first = fixture.Assets.LoadSprite("Sheet.fofrm", AtlasType::MapSprites).dyn_cast<SpriteSheet>();
    REQUIRE(first);
    CHECK(first != fixture.Factory->Prototype);
    auto second = fixture.Assets.LoadSprite("Sheet.fofrm", AtlasType::MapSprites).dyn_cast<SpriteSheet>();
    REQUIRE(second);
    CHECK(first != second);
    CHECK(first->GetSize() == isize32 {5, 7});
    CHECK(second->GetSize() == isize32 {5, 7});

    first->SetTime(1.0f);
    first->SetDirection(hdir {1});
    first->Play({}, true, true);
    CHECK(first->IsPlaying());
    CHECK_FALSE(second->IsPlaying());
    CHECK(second->GetTime() == 0.0f);
    CHECK(second->GetCurrentSprite() == second->GetSprite(0));

    auto third = fixture.Assets.LoadSprite("Sheet.fofrm", AtlasType::MapSprites).dyn_cast<SpriteSheet>();
    REQUIRE(third);
    CHECK(third->GetTime() == 0.0f);
    CHECK_FALSE(third->IsPlaying());
    CHECK(third->GetSize() == isize32 {5, 7});
    CHECK(third->GetOffset() == ipos32 {});
    CHECK(third->GetFramesCount() == 3);
    CHECK(third->GetDuration() == std::chrono::milliseconds {120});
    CHECK(fixture.Factory->LoadCount == 1);

    auto other_atlas = fixture.Assets.LoadSprite("Sheet.fofrm", AtlasType::IfaceSprites).dyn_cast<SpriteSheet>();
    REQUIRE(other_atlas);
    CHECK(other_atlas->GetSprite(0)->GetBatchTexture() != first->GetSprite(0)->GetBatchTexture());
    // AtlasSprite retains its shared pixel/geometry owner; sheet playback and frame tables are independent
    auto quad = fixture.Assets.LoadSprite("Quad.png", AtlasType::MapSprites);
    REQUIRE(quad);
    CHECK(quad == fixture.Assets.LoadSprite("Quad.png", AtlasType::MapSprites));
}

TEST_CASE("CritterFramesCacheCachesProjectOwnedDirectionalSequences", "[sprite]")
{
    SpriteSheetFixture fixture {true};
    SpriteSheetTestResolver resolver;
    resolver.Frames = fixture.Assets.LoadSprite("Sheet.fofrm", AtlasType::MapSprites).dyn_cast<SpriteSheet>();
    REQUIRE(resolver.Frames);
    CritterFramesCache resources {&resolver};
    hstring model = fixture.Hashes.to_hashed_string("art/critters/ProjectOwned");
    CritterStateAnim state = static_cast<CritterStateAnim>(1);
    CritterActionAnim action = static_cast<CritterActionAnim>(1);

    for (int32_t dir = 0; dir < GameSettings::MAP_DIR_COUNT; dir++) {
        auto frames = resources.GetCritterAnimFrames(model, state, action, hdir {dir});
        REQUIRE(frames);
        CHECK(frames->GetFramesCount() == 3);
        CHECK(frames->GetSpriteMotion()[2] == ipos32 {(dir + 1) * 3, -(dir + 1) * 3});
        CHECK(frames->GetStateAnimation() == state);
        CHECK(frames->GetActionAnimation() == action);
        CHECK(frames != resolver.Frames->GetDirection(hdir {dir}));
        CHECK(resolver.Frames->GetDirection(hdir {dir})->GetStateAnimation() == CritterStateAnim {});
    }

    CHECK(resolver.Calls == 1);
    resources.CleanupCritterFrames();
    REQUIRE(resources.GetCritterAnimFrames(model, state, action, hdir {0}));
    CHECK(resolver.Calls == 2);

    resolver.Frames = nullptr;
    resources.CleanupCritterFrames();
    CHECK_FALSE(resources.GetCritterAnimFrames(model, state, action, hdir {0}));
    CHECK_FALSE(resources.GetCritterAnimFrames(model, state, action, hdir {1}));
    CHECK(resolver.Calls == 3);
}

TEST_CASE("SpriteSheetSharedFrameMetadataIsRecordSpecific", "[sprite]")
{
    SpriteSheetFixture fixture {true};
    auto sheet = fixture.Assets.LoadSprite("Sheet.fofrm", AtlasType::MapSprites).dyn_cast<SpriteSheet>();
    REQUIRE(sheet);
    auto copy = sheet->MakeCopy().dyn_cast<SpriteSheet>();
    REQUIRE(copy);

    for (int32_t dir = 0; dir < 2; dir++) {
        auto source_direction = sheet->GetDirection(hdir {numeric_cast<int8_t>(dir)});
        auto copy_direction = copy->GetDirection(hdir {numeric_cast<int8_t>(dir)});
        REQUIRE(source_direction);
        REQUIRE(copy_direction);

        // The shared frame reuses the reference pixel owner, but not its sheet placement or motion
        CHECK(source_direction->GetSprite(1) == source_direction->GetSprite(0));
        CHECK(copy_direction->GetSprite(1) == copy_direction->GetSprite(0));
        CHECK(source_direction->GetSprite(1)->GetOffset() == ipos32 {dir, -dir});
        CHECK(source_direction->GetSpriteOffset(0) == ipos32 {dir, -dir});
        CHECK(source_direction->GetSpriteOffset(1) == ipos32 {100 + dir, -100 - dir});
        CHECK(source_direction->GetSpriteMotion()[0] == ipos32 {dir + 1, -dir - 1});
        CHECK(source_direction->GetSpriteMotion()[1] == ipos32 {50 + dir, -50 - dir});
        CHECK(copy_direction->GetSpriteOffset(0) == ipos32 {dir, -dir});
        CHECK(copy_direction->GetSpriteOffset(1) == ipos32 {100 + dir, -100 - dir});
        CHECK(copy_direction->GetSpriteMotion()[0] == ipos32 {dir + 1, -dir - 1});
        CHECK(copy_direction->GetSpriteMotion()[1] == ipos32 {50 + dir, -50 - dir});
    }

    sheet->SetDirection(hdir {1});
    sheet->SetTime(0.5f);
    CHECK(sheet->GetCurrentSprite() == sheet->GetSprite(1));
    CHECK(sheet->GetOffset() == ipos32 {101, -101});

    auto copy_direction_zero = copy->GetDirection(hdir {0});
    auto copy_direction_one = copy->GetDirection(hdir {1});
    REQUIRE(copy_direction_zero);
    REQUIRE(copy_direction_one);
    copy_direction_one->SetFrame(1, copy_direction_one->GetSprite(0)->MakeCopy(), ipos32 {300, -301}, ipos32 {70, -71});
    CHECK(copy_direction_one->GetSpriteOffset(1) == ipos32 {300, -301});
    CHECK(copy_direction_one->GetSpriteMotion()[1] == ipos32 {70, -71});
    CHECK(copy_direction_one->GetSprite(1)->GetOffset() == ipos32 {1, -1});
    CHECK(copy_direction_zero->GetSpriteOffset(1) == ipos32 {100, -100});
    CHECK(sheet->GetDirection(hdir {1})->GetSpriteOffset(1) == ipos32 {101, -101});

    copy->SetDirection(hdir {1});
    copy->SetTime(0.5f);
    CHECK(copy->GetCurrentSprite() == copy->GetSprite(1));
    CHECK(copy->GetOffset() == ipos32 {300, -301});
    CHECK(copy->GetSpriteOffset(1) == ipos32 {300, -301});
    CHECK(copy->GetSpriteMotion()[1] == ipos32 {70, -71});
}

TEST_CASE("SpriteSheetSharedCroppedFramesKeepLogicalAnchors", "[sprite]")
{
    SpriteSheetFixture fixture;
    vector<uint8_t> data;
    data_writer writer {data};
    writer.write<uint8_t>(SPRITE_RESOURCE_MAGIC);
    writer.write<uint8_t>(SPRITE_RESOURCE_VERSION);
    writer.write<uint8_t>(uint8_t {1});
    writer.write<uint16_t>(uint16_t {3});
    writer.write<uint32_t>(uint32_t {300});

    for (int32_t frame = 0; frame < 3; frame++) {
        writer.write<uint32_t>(uint32_t {100});
        writer.write<int32_t>(7 + frame * 10);
        writer.write<int32_t>(-11 - frame * 10);
        writer.write<int16_t>(numeric_cast<int16_t>(frame + 1));
        writer.write<int16_t>(numeric_cast<int16_t>(-frame - 1));
        writer.write<uint8_t>(frame == 0 ? uint8_t {0} : uint8_t {1});

        if (frame > 0) {
            writer.write<uint16_t>(numeric_cast<uint16_t>(frame - 1));
            continue;
        }

        writer.write<uint16_t>(uint16_t {5});
        writer.write<uint16_t>(uint16_t {7});

        for (size_t pixel = 0; pixel < 35; pixel++) {
            writer.write<ucolor>(ucolor {255, 255, 255, 255});
        }

        writer.write<uint8_t>(static_cast<uint8_t>(SpriteMeshKind::Mesh));
        writer.write<uint16_t>(uint16_t {4});
        writer.write<uint32_t>(uint32_t {6});
        writer.write<uint16_t>(uint16_t {10});
        writer.write<uint16_t>(uint16_t {14});
        writer.write<int32_t>(int32_t {1});
        writer.write<int32_t>(int32_t {2});

        for (ipos16 vertex : array<ipos16, 4> {ipos16 {0, 0}, ipos16 {5, 0}, ipos16 {5, 7}, ipos16 {0, 7}}) {
            writer.write<uint16_t>(numeric_cast<uint16_t>(vertex.x));
            writer.write<uint16_t>(numeric_cast<uint16_t>(vertex.y));
        }

        for (uint16_t index : array<uint16_t, 6> {0, 1, 2, 0, 2, 3}) {
            writer.write<uint16_t>(index);
        }
    }

    writer.write<uint8_t>(SPRITE_RESOURCE_MAGIC);
    auto source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("SharedCroppedSprite");
    source->AddFile("SharedCropped.fofrm", std::move(data));
    fixture.Resources.AddCustomSource(std::move(source));
    auto sheet = fixture.Assets.LoadSprite("SharedCropped.fofrm", AtlasType::MapSprites).dyn_cast<SpriteSheet>();
    REQUIRE(sheet);
    auto copy = sheet->MakeCopy().dyn_cast<SpriteSheet>();
    REQUIRE(copy);

    for (auto candidate : {sheet, copy}) {
        CHECK(candidate->GetSize() == isize32 {10, 14});
        CHECK(candidate->GetSpriteOffset(0) == ipos32 {9, -6});
        CHECK(candidate->GetSpriteOffset(1) == ipos32 {19, -16});
        CHECK(candidate->GetSpriteOffset(2) == ipos32 {29, -26});
        CHECK(candidate->GetSprite(2) == candidate->GetSprite(0));
        CHECK(candidate->GetSpriteMotion()[2] == ipos32 {3, -3});
        candidate->SetTime(1.0f);
        CHECK(candidate->GetOffset() == ipos32 {29, -26});
    }
}

TEST_CASE("SpriteSheetUntimedSequencesKeepManualFrameSelection", "[sprite]")
{
    SpriteSheetFixture fixture;
    auto source = fixture.Assets.LoadSprite("Sheet.fofrm", AtlasType::MapSprites).dyn_cast<SpriteSheet>();
    REQUIRE(source);
    SpriteSheet sheet {&fixture.Sprites, 3, timespan::zero, 1};

    for (int32_t frame = 0; frame < 3; frame++) {
        sheet.SetFrame(frame, source->GetSprite(frame)->MakeCopy(), source->GetSpriteOffset(frame), source->GetSpriteMotion()[frame]);
    }

    CHECK(sheet.GetDuration() == timespan::zero);
    sheet.SetTime(0.5f);
    CHECK(sheet.GetCurrentSprite() == sheet.GetSprite(1));
    CHECK(sheet.GetOffset() == ipos32 {1, -1});
    sheet.PlayDefault();
    CHECK_FALSE(sheet.IsPlaying());
    CHECK_FALSE(sheet.AdvanceTime(std::chrono::hours {24}));
    CHECK(sheet.GetCurrentSprite() == sheet.GetSprite(1));
    sheet.SetTime(1.0f);
    CHECK(sheet.GetCurrentSprite() == sheet.GetSprite(2));

    auto copy = sheet.MakeCopy().dyn_cast<SpriteSheet>();
    REQUIRE(copy);
    CHECK(copy->GetDuration() == timespan::zero);
    CHECK(copy->GetCurrentSprite() == copy->GetSprite(0));
    CHECK_FALSE(copy->IsPlaying());
    sheet.Prewarm();
    CHECK(std::isfinite(sheet.GetTime()));
    CHECK(sheet.GetTime() >= 0.0f);
    CHECK(sheet.GetTime() <= 1.0f);
}

TEST_CASE("SpriteSheetFrameAssignmentKeepsMetadataAndChecksBounds", "[sprite]")
{
    SpriteSheetFixture fixture;
    auto source = fixture.Assets.LoadSprite("Sheet.fofrm", AtlasType::MapSprites).dyn_cast<SpriteSheet>();
    REQUIRE(source);
    auto atlas = source->GetSprite(2).dyn_cast<const AtlasSprite>();
    REQUIRE(atlas);
    auto atlas_owner = atlas->shared_from_this().cast_no_const();
    SpriteSheet output {&fixture.Sprites, 1, std::chrono::milliseconds {120}, 1};
    output.SetFrame(0, atlas_owner, ipos32 {42, -17}, ipos32 {5, -9});
    CHECK(output.GetSprite(0) == atlas);
    CHECK(output.GetSpriteOffset(0) == ipos32 {42, -17});
    CHECK(output.GetSize() == isize32 {5, 7});
    CHECK(output.GetOffset() == ipos32 {42, -17});
    CHECK(output.GetSpriteMotion()[0] == ipos32 {5, -9});
    CHECK(source->GetSprite(2)->GetOffset() == ipos32 {2, -2});

    SpriteSheet other {&fixture.Sprites, 1, std::chrono::milliseconds {60}, 1};
    other.SetFrame(0, atlas_owner, ipos32 {7, -3}, ipos32 {1, -1});
    CHECK(other.GetSprite(0) == atlas);
    CHECK(other.GetOffset() == ipos32 {7, -3});
    CHECK(other.GetSpriteOffset(0) == ipos32 {7, -3});
    CHECK(output.GetSpriteOffset(0) == ipos32 {42, -17});
    CHECK(output.GetSpriteMotion()[0] == ipos32 {5, -9});

    auto copied = output.MakeCopy().dyn_cast<SpriteSheet>();
    REQUIRE(copied);
    CHECK(copied->GetSprite(0) == atlas);
    CHECK(copied->GetSpriteOffset(0) == ipos32 {42, -17});
    CHECK(copied->GetSpriteMotion()[0] == ipos32 {5, -9});
    copied->SetFrame(0, atlas_owner, ipos32 {1, 1}, ipos32 {2, 2});
    CHECK(copied->GetOffset() == ipos32 {1, 1});
    CHECK(copied->GetSpriteMotion()[0] == ipos32 {2, 2});
    CHECK(output.GetSpriteMotion()[0] == ipos32 {5, -9});
    CHECK(output.GetOffset() == ipos32 {42, -17});
    CHECK(output.GetSpriteOffset(0) == ipos32 {42, -17});

    CHECK(source->GetSpriteOffset(2) == ipos32 {2, -2});
    source->SetDirection(hdir {1});
    source->SetTime(1.0f);
    CHECK(source->GetOffset() == ipos32 {3, -3});
    CHECK(source->GetCurrentSprite() == source->GetDirection(hdir {1})->GetSprite(2));
    source->GetDirection(hdir {1})->SetFrame(0, source->GetDirection(hdir {1})->GetSprite(0)->MakeCopy(), ipos32 {9, -9}, ipos32 {2, -2});
    CHECK(source->GetDirection(hdir {1})->GetSpriteOffset(2) == ipos32 {3, -3});
    CHECK(source->GetSpriteOffset(2) == ipos32 {3, -3});
    CHECK(source->GetDirection(hdir {0})->GetSpriteOffset(2) == ipos32 {2, -2});
    source->SetTime(0.0f);
    CHECK(source->GetOffset() == ipos32 {9, -9});

    CHECK_THROWS(output.SetFrame(-1, source->GetSprite(0)->MakeCopy(), {}, {}));
    CHECK_THROWS(output.SetFrame(1, source->GetSprite(0)->MakeCopy(), {}, {}));
    CHECK_THROWS(output.SetFrame(0, nullptr, {}, {}));
    CHECK(output.GetOffset() == ipos32 {42, -17});
    SpriteSheet short_duration {&fixture.Sprites, 2, std::chrono::milliseconds {1}, 1};
    short_duration.SetFrame(0, atlas_owner, {}, {});
    short_duration.SetFrame(1, atlas_owner, {}, {});
    short_duration.Play({}, true, false);
    CHECK(short_duration.Update());

    // The original frame and its metadata survive the source sheet and the resource cache
    auto weak_atlas = atlas->weak_from_this();
    source = nullptr;
    fixture.Factory->Prototype = nullptr;
    atlas_owner = nullptr;
    fixture.Assets.InvalidateSpriteResource("Sheet.fofrm");
    REQUIRE(weak_atlas.lock());
    CHECK(output.GetSprite(0) == atlas);
    CHECK(atlas->GetOffset() == ipos32 {2, -2});
    CHECK(output.GetSpriteOffset(0) == ipos32 {42, -17});
    CHECK(output.GetOffset() == ipos32 {42, -17});
}

TEST_CASE("MapSpriteDrawOffsetOverrideSteersOnlyTheSpriteRoot", "[sprite]")
{
    SpriteSheetFixture fixture;
    auto source = fixture.Assets.LoadSprite("Sheet.fofrm", AtlasType::MapSprites).dyn_cast<SpriteSheet>();
    REQUIRE(source);
    auto frame = source->GetSprite(2);
    CHECK(frame->GetOffset() == ipos32 {2, -2});
    CHECK(frame->GetSize() == isize32 {5, 7});

    optional<ipos32> draw_offset {};
    ipos32 hex_offset {10, 20};
    ipos32 extra_hex_offset {1, 2};
    ipos32 spr_offset {3, 4};
    ipos32 root_offset {5, 6};
    uint8_t alpha {0xFF};
    RenderEffect* effect = nullptr;
    bool valid = false;
    bool extra_valid = false;

    MapSpriteList sprites;
    auto mspr = sprites.AddSprite(DrawOrderType::Critter, mpos {1, 1}, hex_offset, &extra_hex_offset, frame, nullptr, &spr_offset, &root_offset, &alpha, &effect, &valid, 0);
    REQUIRE(valid);
    mspr->SetSpriteOffsetOverride(&draw_offset);

    // Without an override the sprite root follows the frame's own offset
    CHECK(mspr->GetSpriteRootOffset() == ipos32 {0, 9});
    auto draw_root = mspr->GetDrawRootPos();
    auto map_root = mspr->GetMapRootOffset();

    draw_offset = ipos32 {42, -17};
    CHECK(mspr->GetSpriteRootOffset() == ipos32 {-40, 24});
    CHECK(mspr->GetDrawRootPos() == draw_root);
    CHECK(mspr->GetMapRootOffset() == map_root);

    draw_offset = ipos32 {7, -3};
    CHECK(mspr->GetSpriteRootOffset() == ipos32 {-5, 10});

    draw_offset.reset();
    CHECK(mspr->GetSpriteRootOffset() == ipos32 {0, 9});

    // Pooled reuse drops the previous sprite's draw-offset borrow
    draw_offset = ipos32 {42, -17};
    mspr->Invalidate();
    CHECK_FALSE(valid);
    auto reused = sprites.AddSprite(DrawOrderType::Critter, mpos {1, 1}, hex_offset, &extra_hex_offset, frame, nullptr, &spr_offset, &root_offset, &alpha, &effect, &valid, 0);
    REQUIRE(valid);
    CHECK(reused->GetSpriteRootOffset() == ipos32 {0, 9});

    // Both sprites borrow the same long-lived optional and follow its current value
    auto extra = sprites.AddSprite(DrawOrderType::Critter, mpos {1, 1}, hex_offset, &extra_hex_offset, frame, nullptr, &spr_offset, &root_offset, &alpha, &effect, &extra_valid, 0);
    REQUIRE(extra_valid);
    extra->SetSpriteOffsetOverride(&draw_offset);
    reused->SetSpriteOffsetOverride(&draw_offset);
    CHECK(reused->GetSpriteRootOffset() == ipos32 {-40, 24});
    CHECK(extra->GetSpriteRootOffset() == ipos32 {-40, 24});
    draw_offset = ipos32 {7, -3};
    CHECK(reused->GetSpriteRootOffset() == ipos32 {-5, 10});
    CHECK(extra->GetSpriteRootOffset() == ipos32 {-5, 10});
    draw_offset.reset();
    CHECK(reused->GetSpriteRootOffset() == ipos32 {0, 9});
    CHECK(extra->GetSpriteRootOffset() == ipos32 {0, 9});
}

TEST_CASE("ResourceManagerInvalidatesEveryAtlasAndRetainsLiveSheets", "[sprite]")
{
    SpriteSheetFixture fixture;
    auto first = fixture.Assets.LoadSprite("Sheet.fofrm", AtlasType::MapSprites);
    auto other = fixture.Assets.LoadSprite("Sheet.fofrm", AtlasType::IfaceSprites);
    REQUIRE(first);
    REQUIRE(other);
    CHECK(fixture.Factory->LoadCount == 2);
    REQUIRE(fixture.Assets.LoadSprite("Sheet.fofrm", AtlasType::MapSprites));
    CHECK(fixture.Factory->LoadCount == 2);
    fixture.Assets.InvalidateSpriteResource("Sheet.fofrm");
    CHECK(fixture.Factory->InvalidationCount == 1);
    REQUIRE(fixture.Assets.LoadSprite("Sheet.fofrm", AtlasType::MapSprites));
    REQUIRE(fixture.Assets.LoadSprite("Sheet.fofrm", AtlasType::IfaceSprites));
    CHECK(fixture.Factory->LoadCount == 4);
    CHECK(first->GetSize() == isize32 {5, 7});
    CHECK(other->GetSize() == isize32 {5, 7});
}

TEST_CASE("ResourceManagerSuppressesMissingLoadsUntilExplicitRetry", "[sprite]")
{
    SpriteSheetFixture fixture;
    CHECK_FALSE(fixture.Assets.LoadSprite("Missing.png", AtlasType::MapSprites, true));
    CHECK_FALSE(fixture.Assets.LoadSprite("Missing.png", AtlasType::IfaceSprites, true));
    CHECK(fixture.Factory->LoadCount == 1);
    fixture.Assets.CleanupSpriteCache();
    CHECK_FALSE(fixture.Assets.LoadSprite("Missing.png", AtlasType::MapSprites, true));
    CHECK(fixture.Factory->LoadCount == 1);
    fixture.Assets.ForgetFailedSprite("Missing.png");
    CHECK_FALSE(fixture.Assets.LoadSprite("Missing.png", AtlasType::MapSprites, true));
    CHECK(fixture.Factory->LoadCount == 2);
    fixture.Assets.InvalidateSpriteResource("Missing.png");
    CHECK_FALSE(fixture.Assets.LoadSprite("Missing.png", AtlasType::MapSprites, true));
    CHECK(fixture.Factory->LoadCount == 3);
    fixture.Assets.RetryFailedSpriteLoads();
    CHECK(fixture.Factory->RetryCount == 1);
}

TEST_CASE("ResourceManagerLowMemoryReleasesUnusedFramesAndKeepsLiveSheets", "[sprite]")
{
    SpriteSheetFixture fixture;
    auto unused = fixture.Assets.LoadSprite("Sheet.fofrm", AtlasType::MapSprites).dyn_cast<SpriteSheet>();
    REQUIRE(unused);
    auto frame = fixture.Factory->Prototype.dyn_cast<SpriteSheet>()->GetSprite(0)->weak_from_this();
    unused = nullptr;
    auto active = fixture.Assets.LoadSprite("Sheet.fofrm", AtlasType::IfaceSprites).dyn_cast<SpriteSheet>();
    REQUIRE(active);
    fixture.Factory->Prototype = nullptr;
    REQUIRE(frame.lock());
    EventDispatcher<> {fixture.Window->GetOnLowMemory()}();
    CHECK(fixture.Factory->CleanupCount == 1);
    CHECK_FALSE(frame.lock());
    CHECK(active->GetSprite(0)->GetSize() == isize32 {5, 7});
}

TEST_CASE("ResourceManagerUpdatesFactoriesBeforeSpriteInstances", "[sprite]")
{
    SpriteSheetFixture fixture;
    int32_t phase = 0;
    fixture.Factory->OnUpdate = [&phase] { phase = 1; };
    auto sprite = safe_alloc::make_shared<UpdatingTestSprite>(&fixture.Sprites, isize32 {1, 1}, ipos32 {});
    sprite->OnUpdate = [&phase] {
        CHECK(phase == 1);
        phase = 2;
    };
    sprite->StartUpdate();
    fixture.Sprites.BeginScene();
    fixture.Sprites.EndScene();
    CHECK(phase == 2);
}

TEST_CASE("ResourceManagerFactoryFailurePreservesSceneRollback", "[sprite]")
{
    SpriteSheetFixture fixture;
    fixture.Factory->OnUpdate = [] { throw InvalidCallException("Factory update failed"); };
    CHECK_THROWS_AS(fixture.Sprites.BeginScene(), InvalidCallException);
    CHECK(fixture.Sprites.GetRtMngr().GetRenderTargetStack().empty());
    fixture.Factory->OnUpdate = nullptr;
    fixture.Sprites.BeginScene();
    fixture.Sprites.EndScene();
}

TEST_CASE("ResourceManagerDetachesCallbacksAtShutdownAndDestruction", "[sprite]")
{
    SpriteSheetFixture fixture;
    size_t updates = 0;
    {
        ResourceManager resources {&fixture.Resources, &fixture.Sprites};
        auto factory = safe_alloc::make_unique<RecordingSpriteSheetFactory>(&fixture.Sprites, &resources);
        factory->OnUpdate = [&updates] { updates++; };
        resources.RegisterSpriteFactory(std::move(factory));
        fixture.Sprites.BeginScene();
        fixture.Sprites.EndScene();
        CHECK(updates == 1);

        SECTION("DestructorDetaches")
        {
            EventDispatcher<> {fixture.Window->GetOnLowMemory()}();
        }

        SECTION("ExplicitShutdownIsIdempotent")
        {
            resources.UnsubscribeEvents();
            resources.UnsubscribeEvents();
            fixture.Sprites.BeginScene();
            fixture.Sprites.EndScene();
            CHECK(updates == 1);
        }
    }

    fixture.Sprites.BeginScene();
    fixture.Sprites.EndScene();
    EventDispatcher<> {fixture.Window->GetOnLowMemory()}();
    CHECK(updates == 1);
}

TEST_CASE("SpriteSheetVariableTimelines", "[sprite]")
{
    SECTION("DistributesNativeNanosecondDurationsExactly")
    {
        SpriteSheetFixture fixture;
        SpriteSheet sheet {&fixture.Sprites, 3, std::chrono::nanoseconds {10}, 1};
        const_span<timespan> durations = sheet.GetFrameDurations();
        REQUIRE(durations.size() == 3);
        CHECK(durations[0] == std::chrono::nanoseconds {3});
        CHECK(durations[1] == std::chrono::nanoseconds {3});
        CHECK(durations[2] == std::chrono::nanoseconds {4});
        CHECK(sheet.GetDuration() == std::chrono::nanoseconds {10});
    }

    SECTION("DistributesMaximumDurationWithoutIntermediateOverflow")
    {
        SpriteSheetFixture fixture;
        timespan maximum {std::chrono::milliseconds {std::numeric_limits<uint32_t>::max()}};
        SpriteSheet sheet {&fixture.Sprites, 65535, maximum, 1};
        const_span<timespan> durations = sheet.GetFrameDurations();
        REQUIRE(durations.size() == 65535);
        CHECK(std::ranges::all_of(durations, [](timespan duration) { return duration == std::chrono::milliseconds {65537}; }));
        CHECK(std::accumulate(durations.begin(), durations.end(), timespan::zero) == maximum);
        CHECK(sheet.GetDuration() == maximum);
        CHECK(sheet.GetFrameIndexAtPhase(1.0) == 65534);
    }

    SECTION("RejectsInvalidNativeDurationsBeforeAccumulation")
    {
        SpriteSheetFixture fixture;
        timespan invalid = GENERATE(timespan::zero, timespan {std::chrono::nanoseconds {-1}});
        CHECK_THROWS(SpriteSheet(&fixture.Sprites, vector<vector<timespan>> {{invalid, std::chrono::nanoseconds {1}}}));
        CHECK_THROWS(SpriteSheet(&fixture.Sprites, 3, std::chrono::nanoseconds {2}, 1));
        timespan maximum {std::chrono::milliseconds {std::numeric_limits<uint32_t>::max()}};
        CHECK_THROWS(SpriteSheet(&fixture.Sprites, vector<vector<timespan>> {{maximum, std::chrono::nanoseconds {1}}}));
    }

    SECTION("AccumulatesSubMillisecondElapsedTime")
    {
        SpriteSheetFixture fixture {vector<vector<uint32_t>> {{1, 1}}};
        auto sheet = fixture.Assets.LoadSprite("Sheet.fofrm", AtlasType::MapSprites).dyn_cast<SpriteSheet>();
        REQUIRE(sheet);
        sheet->Play({}, true, false);
        CHECK(sheet->AdvanceTime(std::chrono::microseconds {500}));
        CHECK(sheet->GetTime() == Catch::Approx(0.25f));
        CHECK(sheet->GetCurrentSprite() == sheet->GetSprite(0));
        CHECK(sheet->AdvanceTime(std::chrono::microseconds {500}));
        CHECK(sheet->GetCurrentSprite() == sheet->GetSprite(1));
    }

    SECTION("PlaysUnequalFrameDurations")
    {
        SpriteSheetFixture fixture {vector<vector<uint32_t>> {{100, 300, 100}}};
        auto sheet = fixture.Assets.LoadSprite("Sheet.fofrm", AtlasType::MapSprites).dyn_cast<SpriteSheet>();
        REQUIRE(sheet);
        CHECK(sheet->GetDuration() == std::chrono::milliseconds {500});
        sheet->Play({}, true, false);
        CHECK(sheet->GetCurrentSprite() == sheet->GetSprite(0));
        CHECK(sheet->AdvanceTime(std::chrono::milliseconds {99}));
        CHECK(sheet->GetCurrentSprite() == sheet->GetSprite(0));
        CHECK(sheet->AdvanceTime(std::chrono::milliseconds {1}));
        CHECK(sheet->GetCurrentSprite() == sheet->GetSprite(1));
        CHECK(sheet->AdvanceTime(std::chrono::milliseconds {299}));
        CHECK(sheet->GetCurrentSprite() == sheet->GetSprite(1));
        CHECK(sheet->AdvanceTime(std::chrono::milliseconds {1}));
        CHECK(sheet->GetCurrentSprite() == sheet->GetSprite(2));
        CHECK(sheet->AdvanceTime(std::chrono::milliseconds {100}));
        CHECK(sheet->GetCurrentSprite() == sheet->GetSprite(0));
    }

    SECTION("QueriesTimelineWithoutMutatingPlayback")
    {
        SpriteSheetFixture fixture {vector<vector<uint32_t>> {{100, 300, 100}}};
        auto sheet = fixture.Assets.LoadSprite("Sheet.fofrm", AtlasType::MapSprites).dyn_cast<SpriteSheet>();
        REQUIRE(sheet);
        CHECK(sheet->GetFrameIndexAtPhase(0.199) == 0);
        CHECK(sheet->GetFrameIndexAtPhase(0.2) == 1);
        CHECK(sheet->GetFrameIndexAtPhase(0.799) == 1);
        CHECK(sheet->GetFrameIndexAtPhase(0.8) == 2);
        CHECK(sheet->GetFrameIndexAtPhase(1.0) == 2);
        CHECK(sheet->GetFrameIndexAtPhase(0.2, true) == 0);
        CHECK(sheet->GetFrameIndexAtPhase(0.8, true) == 1);
        CHECK(sheet->GetTime() == 0.0f);
        CHECK_THROWS(sheet->GetFrameIndexAtPhase(std::numeric_limits<float64_t>::quiet_NaN()));
    }

    SECTION("SwitchesUnequalDirectionCountsByPhase")
    {
        vector<vector<uint32_t>> timelines(numeric_cast<size_t>(GameSettings::MAP_DIR_COUNT), {100, 100});
        timelines[0] = {100, 300, 100};
        SpriteSheetFixture fixture {timelines};
        auto sheet = fixture.Assets.LoadSprite("Sheet.fofrm", AtlasType::MapSprites).dyn_cast<SpriteSheet>();
        REQUIRE(sheet);
        sheet->SetTime(0.9f);
        CHECK(sheet->GetCurrentSprite() == sheet->GetDirection(hdir {0})->GetSprite(2));
        sheet->SetDirection(hdir {1});
        CHECK(sheet->GetTime() == 0.9f);
        CHECK(sheet->GetFramesCount() == 2);
        CHECK(sheet->GetDuration() == std::chrono::milliseconds {200});
        CHECK(sheet->GetCurrentSprite() == sheet->GetDirection(hdir {1})->GetSprite(1));
        CHECK(sheet->GetDirection(hdir {0})->GetFramesCount() == 3);
        sheet->SetDirection(hdir {0});
        CHECK(sheet->GetFramesCount() == 3);
        CHECK(sheet->GetCurrentSprite() == sheet->GetSprite(2));
        auto copy = sheet->MakeCopy().dyn_cast<SpriteSheet>();
        REQUIRE(copy);
        CHECK(copy->GetTime() == 0.0f);
        CHECK(copy->GetDirection(hdir {1})->GetFramesCount() == 2);
    }

    SECTION("ReverseCompletesAtZeroAndLoopingNeverSelectsNegativeFrames")
    {
        SpriteSheetFixture fixture {vector<vector<uint32_t>> {{100, 300, 100}}};
        auto sheet = fixture.Assets.LoadSprite("Sheet.fofrm", AtlasType::MapSprites).dyn_cast<SpriteSheet>();
        REQUIRE(sheet);
        sheet->SetTime(1.0f);
        sheet->Play({}, false, true);
        CHECK(sheet->AdvanceTime(std::chrono::milliseconds {100}));
        CHECK(sheet->GetCurrentSprite() == sheet->GetSprite(1));
        CHECK_FALSE(sheet->AdvanceTime(std::chrono::milliseconds {400}));
        CHECK_FALSE(sheet->IsPlaying());
        CHECK(sheet->GetTime() == 0.0f);
        sheet->Play({}, true, true);
        CHECK(sheet->AdvanceTime(std::chrono::milliseconds {100}));
        CHECK(sheet->GetCurrentSprite() == sheet->GetSprite(1));
    }

    SECTION("ShortAndUint32MaximumDurationsNeverDivideByZero")
    {
        SpriteSheetFixture fixture {vector<vector<uint32_t>> {{1, 1, 1}}};
        auto sheet = fixture.Assets.LoadSprite("Sheet.fofrm", AtlasType::MapSprites).dyn_cast<SpriteSheet>();
        REQUIRE(sheet);
        sheet->Play({}, false, false);
        CHECK(sheet->AdvanceTime(std::chrono::milliseconds {1}));
        CHECK(sheet->GetCurrentSprite() == sheet->GetSprite(1));
        CHECK_FALSE(sheet->AdvanceTime(std::chrono::milliseconds {2}));
        CHECK(sheet->GetCurrentSprite() == sheet->GetSprite(2));
        SpriteSheetFixture long_fixture {vector<vector<uint32_t>> {{4294967294u, 1u}}};
        auto long_sheet = long_fixture.Assets.LoadSprite("Sheet.fofrm", AtlasType::MapSprites).dyn_cast<SpriteSheet>();
        REQUIRE(long_sheet);
        CHECK(long_sheet->GetDuration() == std::chrono::milliseconds {4294967295u});
        long_sheet->Play({}, false, false);
        CHECK(long_sheet->AdvanceTime(std::chrono::milliseconds {4294967294LL}));
        CHECK(long_sheet->GetCurrentSprite() == long_sheet->GetSprite(1));
        CHECK_FALSE(long_sheet->AdvanceTime(std::chrono::milliseconds {1}));
    }
}

FO_END_NAMESPACE
