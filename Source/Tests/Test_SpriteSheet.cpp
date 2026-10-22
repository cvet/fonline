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
#include "DefaultSprites.h"
#include "EffectManager.h"
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

    // Every direction has distinct anchors and two-axis displacement; shared frames exercise the decoded
    // offset table as well as concrete frames without relying on a scripting backend
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
                int16_t delta = numeric_cast<int16_t>((dir + 1) * (source + 1));
                writer.write<uint32_t>(uint32_t {40});
                writer.write<int32_t>(dir + source);
                writer.write<int32_t>(-dir - source);
                writer.write<int16_t>(delta);
                writer.write<int16_t>(numeric_cast<int16_t>(-delta));
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

    struct SpriteSheetFixture
    {
        GlobalSettings Settings {MakeSpriteSheetTestSettings()};
        unique_ptr<IAppWindow> Window {GetAppWindowStub(Settings)};
        FileSystem Resources {};
        GameTimer Timer {&Settings};
        hash_storage Hashes {};
        EffectManager Effects {&Settings, &Resources, Window->GetRender()};
        SpriteManager Sprites {&Settings, Window, &Resources, &Timer, &Effects, &Hashes};
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
            Resources.AddCustomSource(std::move(source));
            auto factory = safe_alloc::make_unique<RecordingSpriteSheetFactory>(&Sprites);
            Factory = factory.as_nptr();
            Sprites.RegisterSpriteFactory(std::move(factory));
        }
    };

    class SpriteSheetTestResolver final : public AnimationResolver
    {
    public:
        uint32_t FrameFlags {};
        bool FalloutMerge {};

        auto ResolveCritterAnimationFrames(hstring, CritterStateAnim, CritterActionAnim, int32_t&, uint32_t& flags, int32_t& ox, int32_t& oy, string& anim_name) -> bool override
        {
            flags = FrameFlags;
            ox = oy = 0;
            anim_name = "Sheet.fofrm";
            return true;
        }

        auto ResolveCritterAnimationSubstitute(hstring, CritterStateAnim, CritterActionAnim, hstring&, CritterStateAnim&, CritterActionAnim&) -> bool override { return false; }
        auto ResolveCritterAnimationFallout(hstring, CritterStateAnim, CritterActionAnim, int32_t& state, int32_t& action, int32_t& extra_state, int32_t& extra_action, uint32_t&) -> bool override
        {
            state = extra_state = 1;
            action = 1;
            extra_action = 2;

            return FalloutMerge;
        }
    };
}

TEST_CASE("SpriteManagerReturnsIndependentSheetsFromTheFirstLoad", "[sprite]")
{
    SpriteSheetFixture fixture;
    auto first = fixture.Sprites.LoadSprite("Sheet.fofrm", AtlasType::MapSprites).dyn_cast<SpriteSheet>();
    REQUIRE(first);
    CHECK(first != fixture.Factory->Prototype);
    auto second = fixture.Sprites.LoadSprite("Sheet.fofrm", AtlasType::MapSprites).dyn_cast<SpriteSheet>();
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

    auto third = fixture.Sprites.LoadSprite("Sheet.fofrm", AtlasType::MapSprites).dyn_cast<SpriteSheet>();
    REQUIRE(third);
    CHECK(third->GetTime() == 0.0f);
    CHECK_FALSE(third->IsPlaying());
    CHECK(third->GetSize() == isize32 {5, 7});
    CHECK(third->GetOffset() == ipos32 {});
    CHECK(third->GetFramesCount() == 3);
    CHECK(third->GetWholeTicks() == 120);
    CHECK(fixture.Factory->LoadCount == 1);

    auto other_atlas = fixture.Sprites.LoadSprite("Sheet.fofrm", AtlasType::IfaceSprites).dyn_cast<SpriteSheet>();
    REQUIRE(other_atlas);
    CHECK(other_atlas->GetSprite(0)->GetBatchTexture() != first->GetSprite(0)->GetBatchTexture());
    // AtlasSprite retains its shared pixel/geometry owner; sheet playback and frame tables are independent
    auto quad = fixture.Sprites.LoadSprite("Quad.png", AtlasType::MapSprites);
    REQUIRE(quad);
    CHECK(quad == fixture.Sprites.LoadSprite("Quad.png", AtlasType::MapSprites));
}

TEST_CASE("ResourceManagerTrimsOnlyItsOwnSpriteSheet", "[sprite]")
{
    bool shared_frame = GENERATE(false, true);
    // Native resolver flags: FirstFrame = 1, LastFrame = 2; first wins when both are present
    uint32_t frame_flags = GENERATE(uint32_t {1}, uint32_t {2}, uint32_t {3});
    CAPTURE(shared_frame, frame_flags);
    SpriteSheetFixture fixture {shared_frame};
    SpriteSheetTestResolver resolver;
    resolver.FrameFlags = frame_flags;
    ResourceManager resources {&fixture.Settings, &fixture.Resources, &fixture.Sprites, &resolver};
    hstring model = fixture.Hashes.to_hashed_string("UnitTestSheet");
    CritterStateAnim state = static_cast<CritterStateAnim>(1);
    CritterActionAnim action = static_cast<CritterActionAnim>(1);
    bool first_frame = (frame_flags & 1) != 0;
    int32_t selected_frame = first_frame ? 0 : 2;

    for (int32_t dir = 0; dir < GameSettings::MAP_DIR_COUNT; dir++) {
        auto trimmed = resources.GetCritterAnimFrames(model, state, action, hdir {numeric_cast<int8_t>(dir)});
        REQUIRE(trimmed);
        CHECK(trimmed->GetFramesCount() == 1);
        // Stop before copying a malformed legacy sheet: the old implementation could write past its buffer
        REQUIRE(trimmed->GetSpriteOffset().size() == 1);
        int32_t displacement = (dir + 1) * (first_frame ? 1 : shared_frame ? 5 : 6);
        CHECK(trimmed->GetSpriteOffset()[0] == ipos32 {displacement, -displacement});
        CHECK(trimmed->GetSprite(0)->GetOffset() == ipos32 {dir + selected_frame, -dir - selected_frame});
        auto copy = trimmed->MakeCopy().dyn_cast<SpriteSheet>();
        REQUIRE(copy);
        CHECK(copy->GetFramesCount() == 1);
        CHECK(copy->GetSpriteOffset().size() == 1);
        CHECK(copy->GetStateAnimation() == state);
        CHECK(copy->GetActionAnimation() == action);
        CHECK(copy->GetSize() == isize32 {5, 7});
        CHECK(copy->GetOffset() == trimmed->GetSprite(0)->GetOffset());
        copy->PlayDefault();
        CHECK_FALSE(copy->IsPlaying());
    }

    auto full = fixture.Sprites.LoadSprite("Sheet.fofrm", AtlasType::MapSprites).dyn_cast<SpriteSheet>();
    REQUIRE(full);
    CHECK(full->GetFramesCount() == 3);
    CHECK(full->GetDirectionCount() == GameSettings::MAP_DIR_COUNT);
    for (int32_t dir = 0; dir < GameSettings::MAP_DIR_COUNT; dir++) {
        auto direction = full->GetDirection(hdir {numeric_cast<int8_t>(dir)});
        REQUIRE(direction);
        CHECK(direction->GetFramesCount() == 3);
        CHECK(direction->GetSpriteOffset().size() == 3);
        CHECK(direction->GetSpriteOffset()[2] == ipos32 {(dir + 1) * 3, -(dir + 1) * 3});
        CHECK(direction->GetSprite(0)->GetOffset() == ipos32 {dir, -dir});
    }

    // Clearing the critter cache and resolving without flags must still see the original full sheet
    resources.CleanupCritterFrames();
    resolver.FrameFlags = 0;
    auto untrimmed = resources.GetCritterAnimFrames(model, state, action, hdir {0});
    REQUIRE(untrimmed);
    CHECK(untrimmed->GetFramesCount() == 3);
    CHECK(untrimmed->GetSpriteOffset().size() == 3);
}

TEST_CASE("ResourceManagerMergesBothMotionAxes", "[sprite]")
{
    SpriteSheetFixture fixture;
    SpriteSheetTestResolver resolver;
    resolver.FalloutMerge = true;
    ResourceManager resources {&fixture.Settings, &fixture.Resources, &fixture.Sprites, &resolver};
    auto model = fixture.Hashes.to_hashed_string("art/critters/TestXX");

    for (int32_t dir = 0; dir < GameSettings::MAP_DIR_COUNT; dir++) {
        auto merged = resources.GetCritterAnimFrames(model, static_cast<CritterStateAnim>(1), static_cast<CritterActionAnim>(1), hdir(dir));
        REQUIRE(merged);
        CHECK(merged->GetFramesCount() == 6);
        CHECK(merged->GetWholeTicks() == 240);
        CHECK(merged->GetSpriteOffset()[3] == ipos32 {-5 * (dir + 1), 5 * (dir + 1)});
    }
}

TEST_CASE("SpriteSheetVariableTimelines", "[sprite]")
{
    SECTION("PlaysUnequalFrameDurations")
    {
        SpriteSheetFixture fixture {vector<vector<uint32_t>> {{100, 300, 100}}};
        auto sheet = fixture.Sprites.LoadSprite("Sheet.fofrm", AtlasType::MapSprites).dyn_cast<SpriteSheet>();
        REQUIRE(sheet);
        CHECK(sheet->GetWholeTicks() == 500);
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
        auto sheet = fixture.Sprites.LoadSprite("Sheet.fofrm", AtlasType::MapSprites).dyn_cast<SpriteSheet>();
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
        auto sheet = fixture.Sprites.LoadSprite("Sheet.fofrm", AtlasType::MapSprites).dyn_cast<SpriteSheet>();
        REQUIRE(sheet);
        sheet->SetTime(0.9f);
        CHECK(sheet->GetCurrentSprite() == sheet->GetDirection(hdir {0})->GetSprite(2));
        sheet->SetDirection(hdir {1});
        CHECK(sheet->GetTime() == 0.9f);
        CHECK(sheet->GetFramesCount() == 2);
        CHECK(sheet->GetWholeTicks() == 200);
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
        auto sheet = fixture.Sprites.LoadSprite("Sheet.fofrm", AtlasType::MapSprites).dyn_cast<SpriteSheet>();
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
        auto sheet = fixture.Sprites.LoadSprite("Sheet.fofrm", AtlasType::MapSprites).dyn_cast<SpriteSheet>();
        REQUIRE(sheet);
        sheet->Play({}, false, false);
        CHECK(sheet->AdvanceTime(std::chrono::milliseconds {1}));
        CHECK(sheet->GetCurrentSprite() == sheet->GetSprite(1));
        CHECK_FALSE(sheet->AdvanceTime(std::chrono::milliseconds {2}));
        CHECK(sheet->GetCurrentSprite() == sheet->GetSprite(2));
        SpriteSheetFixture long_fixture {vector<vector<uint32_t>> {{4294967294u, 1u}}};
        auto long_sheet = long_fixture.Sprites.LoadSprite("Sheet.fofrm", AtlasType::MapSprites).dyn_cast<SpriteSheet>();
        REQUIRE(long_sheet);
        CHECK(long_sheet->GetWholeTicks() == 4294967295u);
        long_sheet->Play({}, false, false);
        CHECK(long_sheet->AdvanceTime(std::chrono::milliseconds {4294967294LL}));
        CHECK(long_sheet->GetCurrentSprite() == long_sheet->GetSprite(1));
        CHECK_FALSE(long_sheet->AdvanceTime(std::chrono::milliseconds {1}));
    }
}

FO_END_NAMESPACE
