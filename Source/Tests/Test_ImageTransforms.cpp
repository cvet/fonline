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

#include "ImageBaker-Fofrm.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

static auto TransformFixture() -> ImageImport::FrameCollection
{
    ImageImport::FrameCollection result;

    for (int32_t dir = 0; dir < GameSettings::MAP_DIR_COUNT; dir++) {
        auto& sequence = dir == 0 ? result.Main : result.Dirs[dir - 1];
        sequence.OffsX = 10;
        auto& shot = sequence.Frames.emplace_back();
        shot.Size = {3, 2};
        shot.Motion = {6, -7};
        shot.Offset = {4, 5};
        shot.DurationMs = 100;

        for (uint8_t pixel = 1; pixel <= 6; pixel++) {
            shot.Data.insert(shot.Data.end(), {numeric_cast<uint8_t>(pixel + dir * 10), 0, 0, 255});
        }
    }

    return result;
}

static auto LoadTransformRecipe(string_view text, ImageImport::FrameCollection source = TransformFixture()) -> ImageImport::FrameCollection
{
    FileCollection files {vector<FileHeader> {}};
    ImageImport::Context context {.Files = &files, .Resolve = [&source](string_view) { return source; }};
    vector<uint8_t> bytes(text.begin(), text.end());

    return ImageBakerFofrm::Load("test.fofrm", "", FileReader {bytes}, context);
}

TEST_CASE("FofrmSelection")
{
    SECTION("SelectsInputDirectionIndependentlyOfFileFormat")
    {
        auto collection = LoadTransformRecipe("Frame = test.toy\nInputDir = 2\n");
        CHECK(collection.Main.Frames[0].Data[0] == 21);
        CHECK(collection.Main.OffsX == 0);
        collection = LoadTransformRecipe("Source = test.toy\nInputDir = 2\n");
        CHECK(ImageImport::GetDirectionCount(collection) == 1);
        CHECK(collection.Main.Frames[0].Data[0] == 21);
        CHECK(collection.Main.OffsX == 10);
    }

    SECTION("CropsExactPixels")
    {
        auto collection = LoadTransformRecipe("Frame = test.toy\nCrop = 1 0 2 2\n");
        const auto& shot = collection.Main.Frames[0];
        CHECK(shot.Size == isize32 {2, 2});
        CHECK(shot.Data == vector<uint8_t> {2, 0, 0, 255, 3, 0, 0, 255, 5, 0, 0, 255, 6, 0, 0, 255});
        CHECK(shot.Offset == ipos32 {4, 5});
        CHECK(shot.Motion.x == 6);
    }

    SECTION("SlicesGridRowMajorAndSelectsReordersAndRepeatsFrames")
    {
        auto collection = LoadTransformRecipe("Frame = test.toy\nGrid = 1 1\nFrameIndices = 5 0 5 3\n");
        REQUIRE(collection.Main.Frames.size() == 4);
        CHECK(collection.Main.Frames[0].Data[0] == 6);
        CHECK(collection.Main.Frames[1].Data[0] == 1);
        CHECK(collection.Main.Frames[2].Data[0] == 6);
        CHECK(collection.Main.Frames[3].Data[0] == 4);
        CHECK(ImageImport::GetDurationMs(collection.Main) == 400);
    }

    SECTION("SourceSelectionAppliesToEveryDirection")
    {
        auto collection = LoadTransformRecipe("Source = test.toy\nGrid = 1 1\nFrameIndices = 4 1\n");
        REQUIRE(collection.Main.Frames.size() == 2);

        for (int32_t dir = 1; dir < GameSettings::MAP_DIR_COUNT; dir++) {
            REQUIRE(collection.Dirs[dir - 1].Frames.size() == 2);
            CHECK(collection.Dirs[dir - 1].Frames[0].Data[0] == 5 + dir * 10);
            CHECK(collection.Dirs[dir - 1].Frames[1].Data[0] == 2 + dir * 10);
        }
    }

    SECTION("RejectsOutOfBoundsSelectionAndGridRemainders")
    {
        for (string_view operation : {"InputDir = 99", "InputDir = -1", "Grid = 2 1", "Crop = 2 0 2 1", "Crop = -1 0 1 1", "FrameIndices = 1", "FrameIndices =", "FrameInputDirs = 1 2", "Crop = 0 0 1 1\nFrameCrops = 0 0 1 1"}) {
            INFO(operation);
            CHECK_THROWS(LoadTransformRecipe(strex("Frame = test.toy\n{}\n", operation).str()));
        }
    }
}

TEST_CASE("FofrmGeometry")
{
    SECTION("FlipsAndRotatesAsymmetricPixels")
    {
        const vector<pair<string, vector<uint8_t>>> cases {
            {"Flip = X", {3, 2, 1, 6, 5, 4}},
            {"Flip = Y", {4, 5, 6, 1, 2, 3}},
            {"Flip = XY", {6, 5, 4, 3, 2, 1}},
            {"Rotate = 90", {4, 1, 5, 2, 6, 3}},
            {"Rotate = 180", {6, 5, 4, 3, 2, 1}},
            {"Rotate = 270", {3, 6, 2, 5, 1, 4}},
        };

        for (const auto& [operation, expected] : cases) {
            INFO(operation);
            auto result = LoadTransformRecipe(strex("Frame = test.toy\n{}\n", operation).str());
            const auto& shot = result.Main.Frames[0];
            REQUIRE(shot.Data.size() == expected.size() * 4);

            for (size_t pixel = 0; pixel < expected.size(); pixel++) {
                CHECK(shot.Data[pixel * 4] == expected[pixel]);
            }

            CHECK(shot.Offset == ipos32 {4, 5});
            CHECK(shot.Motion == ipos32 {6, -7});
        }
    }

    SECTION("ResizesBeforeCanvasAndPadsWithoutScalingOffsets")
    {
        auto result = LoadTransformRecipe("Frame = test.toy\nSize = 2 3\nCanvas = 4 5\nCanvasOffset = 1 1\nOffset = 10 20\nFrameOffsets = 1 -1\n");
        const auto& shot = result.Main.Frames[0];
        CHECK(shot.Size == isize32 {4, 5});
        CHECK(shot.Data[3] == 0);
        CHECK(shot.Data[(1 * shot.Size.width + 1) * 4 + 3] == 255);
        CHECK(shot.Data[(3 * shot.Size.width + 2) * 4 + 3] == 255);
        CHECK(shot.Offset == ipos32 {5, 4});
        CHECK(result.Main.OffsX == 10);
        CHECK(result.Main.OffsY == 20);
        CHECK(shot.Motion == ipos32 {6, -7});
    }

    SECTION("PerReferenceTransformsRunAfterGrid")
    {
        auto result = LoadTransformRecipe("Frames = A.toy B.toy\nCrop = 0 0 2 2\nGrid = 2 1\nFrameRotations = 90 270\nFrameCanvases = 2 2 3 3\nFrameCanvasOffsets = 0 0 1 1\n");
        REQUIRE(result.Main.Frames.size() == 4);
        CHECK(result.Main.Frames[0].Size == isize32 {2, 2});
        CHECK(result.Main.Frames[0].Data[0] == 1);
        CHECK(result.Main.Frames[0].Data[2 * 4] == 2);
        CHECK(result.Main.Frames[2].Size == isize32 {3, 3});
        CHECK(result.Main.Frames[2].Data[(1 * 3 + 1) * 4] == 2);
    }

    SECTION("RejectsCanvasClippingAndOverflow")
    {
        for (string_view operation : {"Canvas = 2 2", "Canvas = 3 2\nCanvasOffset = -1 0", "Canvas = 65536 1", "CanvasOffset = 1 0", "Rotate = 45", "Flip = Z", "FrameRotations = 0 90", "Flip = X\nFrameFlips = Y", "Canvas = 4 4\nFrameCanvases = 4 4"}) {
            INFO(operation);
            CHECK_THROWS(LoadTransformRecipe(strex("Frame = test.toy\n{}\n", operation).str()));
        }
    }
}

TEST_CASE("FofrmMovement")
{
    SECTION("RejectsPreviousMotionKeyNames")
    {
        FO_TEMPORARY_COMPAT(FofrmLegacyMotionKeys, "2026-12-31");

        for (string_view operation : {"Move = 1 2", "FrameMoves = 1 2"}) {
            INFO(operation);
            CHECK_THROWS_WITH(LoadTransformRecipe(strex("Source = A.toy\n{}\n", operation).str()), Catch::Matchers::ContainsSubstring("Motion"));
        }
    }

    SECTION("AuthorsPngRootMotionSeparatelyFromPlacement")
    {
        auto result = LoadTransformRecipe("Frames = A.toy B.toy\nFrameMotions = 1 2 -3 -4\nFrameOffsets = 7 8 9 10\n");
        REQUIRE(result.Main.Frames.size() == 2);
        CHECK(result.Main.Frames[0].Motion == ipos32 {1, 2});
        CHECK(result.Main.Frames[1].Motion == ipos32 {-3, -4});
        CHECK(result.Main.Frames[0].Offset == ipos32 {11, 13});
        CHECK(result.Main.Frames[1].Offset == ipos32 {13, 15});
    }

    SECTION("ExplicitMotionReplacesEveryExpandedFrameButUnsetPreservesImportedMotion")
    {
        auto result = LoadTransformRecipe("Source = A.toy\nGrid = 1 1\nMotion = 0 0\n");
        REQUIRE(result.Main.Frames.size() == 6);

        for (const auto& shot : result.Main.Frames) {
            CHECK(shot.Motion == ipos32 {});
        }

        result = LoadTransformRecipe("Frame = A.toy\nGrid = 1 1\n");
        CHECK(result.Main.Frames[3].Motion == ipos32 {6, -7});
    }

    SECTION("PreservesWideImportedMotionThroughBitmapTransforms")
    {
        auto source = TransformFixture();

        for (auto& direction : source.Dirs) {
            direction.Frames.clear();
        }

        source.Main.Frames[0].Motion = {40000, -40000};
        for (string_view operation : {"Flip = X", "Rotate = 90", "Size = 2 3", "Canvas = 5 4", "Crop = 1 0 2 2", "Grid = 1 1"}) {
            INFO(operation);
            auto result = LoadTransformRecipe(strex("Frame = A.toy\n{}\n", operation).str(), source);

            for (const auto& frame : result.Main.Frames) {
                CHECK(frame.Motion == ipos32 {40000, -40000});
            }
        }
    }

    SECTION("RejectsNegativeSourceDimensionsBeforeCanvas")
    {
        auto source = TransformFixture();
        source.Main.Frames[0].Size = {0, -1};
        source.Main.Frames[0].Data.clear();

        CHECK_THROWS(LoadTransformRecipe("Frame = A.toy\nCanvas = 4 4\n", source));
    }

    SECTION("RejectsMotionCountAndInt16Overflow")
    {
        for (string_view operation : {"Motion = 32768 0", "Motion = -32769 0", "FrameMotions = 1 2 3 4", "Motion = 1 2\nFrameMotions = 3 4", "Motion = 1"}) {
            INFO(operation);
            CHECK_THROWS(LoadTransformRecipe(strex("Frame = A.toy\n{}\n", operation).str()));
        }
    }
}

TEST_CASE("FofrmResampling")
{
    auto source = TransformFixture();

    for (auto& direction : source.Dirs) {
        direction.Frames.clear();
    }

    auto& shot = source.Main.Frames[0];
    shot.Size = {3, 1};
    shot.Data = {0, 0, 0, 255, 120, 120, 120, 255, 240, 240, 240, 255};

    SECTION("PreservesNearestPaletteValues")
    {
        auto result = LoadTransformRecipe("Frame = A.toy\nSize = 2 1\nResample = Nearest\n", source);
        CHECK(result.Main.Frames[0].Data == vector<uint8_t> {0, 0, 0, 255, 240, 240, 240, 255});
    }

    SECTION("AreaDownsampleMatchesCoverage")
    {
        auto result = LoadTransformRecipe("Frame = A.toy\nSize = 2 1\nResample = Area\n", source);
        CHECK(result.Main.Frames[0].Data == vector<uint8_t> {40, 40, 40, 255, 200, 200, 200, 255});
        CHECK_THROWS(LoadTransformRecipe("Frame = A.toy\nSize = 4 1\nResample = Area\n", source));
    }

    SECTION("FiltersLinearColorWithPremultipliedAlpha")
    {
        shot.Size = {2, 1};
        shot.Data = {0, 0, 0, 255, 255, 255, 255, 255};
        auto result = LoadTransformRecipe("Frame = A.toy\nSize = 1 1\nColorSpace = Linear\n", source);
        CHECK(result.Main.Frames[0].Data == vector<uint8_t> {188, 188, 188, 255});
        shot.Data = {255, 0, 0, 0, 0, 0, 255, 255};
        result = LoadTransformRecipe("Frame = A.toy\nSize = 1 1\nColorSpace = Linear\n", source);
        CHECK(result.Main.Frames[0].Data == vector<uint8_t> {0, 0, 255, 128});
    }

    SECTION("KeepsDataChannelsIndependent")
    {
        shot.Size = {2, 1};
        shot.Data = {255, 0, 0, 0, 0, 0, 255, 255};
        auto result = LoadTransformRecipe("Frame = A.toy\nSize = 1 1\nPixelUsage = Data\n", source);
        CHECK(result.Main.Frames[0].Data == vector<uint8_t> {128, 0, 128, 128});
        CHECK_THROWS(LoadTransformRecipe("Frame = A.toy\nPixelUsage = Data\nColorSpace = Encoded\n", source));
    }

    SECTION("RenormalizesNormalVectors")
    {
        shot.Size = {2, 1};
        shot.Data = {255, 128, 128, 0, 128, 255, 128, 255};
        auto result = LoadTransformRecipe("Frame = A.toy\nSize = 1 1\nPixelUsage = Normal\n", source);
        CHECK(result.Main.Frames[0].Data == vector<uint8_t> {218, 218, 128, 128});
        shot.Data = {255, 127, 127, 0, 0, 128, 128, 255};
        result = LoadTransformRecipe("Frame = A.toy\nSize = 1 1\nPixelUsage = Normal\n", source);
        CHECK(result.Main.Frames[0].Data == vector<uint8_t> {128, 128, 255, 128});
    }

    SECTION("KeepsExistingBilinearBytes")
    {
        auto original = LoadTransformRecipe("Frame = A.toy\nSize = 2 1\n", source);
        auto explicit_defaults = LoadTransformRecipe("Frame = A.toy\nSize = 2 1\nResample = Bilinear\nPixelUsage = Color\nColorSpace = Encoded\n", source);
        CHECK(original.Main.Frames[0].Data == explicit_defaults.Main.Frames[0].Data);
        CHECK(original.Main.Frames[0].Data == vector<uint8_t> {30, 30, 30, 255, 210, 210, 210, 255});
    }
}

TEST_CASE("FofrmStrictFieldApplicability")
{
    SECTION("RejectsRetiredFofrmEffectBeforeResolution")
    {
        FileCollection files {vector<FileHeader> {}};
        bool resolved = false;
        ImageImport::Context context {.Files = &files, .Resolve = [&resolved](string_view) {
                                          resolved = true;

                                          return TransformFixture();
                                      }};
        string text = "Frame = missing.png\nEffect = missing.fofx\n";
        vector<uint8_t> bytes(text.begin(), text.end());
        CHECK_THROWS(ImageBakerFofrm::Load("test.fofrm", "", FileReader {bytes}, context));
        CHECK_FALSE(resolved);
    }

    SECTION("RejectsNamedFpsInsteadOfIgnoringIt")
    {
        auto text = strex("Source = A.toy\n[{} 1]\nFps = 20\n", FO_GEOMETRY == 1 ? "HexDir" : "SquareDir").str();
        CHECK_THROWS(LoadTransformRecipe(text));
    }

    SECTION("RejectsAuthoredDirectionsWithoutFullTable")
    {
        auto text = strex("Frame = A.toy\n[{} 2]\nFrame = A.toy\n", FO_GEOMETRY == 1 ? "HexDir" : "SquareDir").str();
        CHECK_THROWS(LoadTransformRecipe(text));
    }
}

TEST_CASE("FofrmTimelines")
{
    SECTION("PlaysUnequalFrameDurationsAfterFinalSelection")
    {
        auto result = LoadTransformRecipe("Frame = A.toy\nGrid = 1 1\nFrameIndices = 0 1 2\nFrameDurations = 100 300 100\n");
        REQUIRE(result.Main.Frames.size() == 3);
        CHECK(ImageImport::GetDurationMs(result.Main) == 500);
    }

    SECTION("DistributesShortDurationWithoutZeroFrames")
    {
        auto result = LoadTransformRecipe("Frame = A.toy\nGrid = 1 1\nFrameIndices = 0 1 2\nDuration = 8\n");
        CHECK(ImageImport::GetDurationMs(result.Main) == 8);
    }

    SECTION("KeepsSharedPixelMetadataIndependent")
    {
        auto source = TransformFixture();

        for (auto& direction : source.Dirs) {
            direction.Frames.clear();
        }

        source.Main.Frames.emplace_back(ImageImport::FrameShot {.Offset = {10, 20}, .Motion = {1, 2}, .DurationMs = 300, .Shared = true, .SharedIndex = 0});

        auto result = LoadTransformRecipe("Source = A.toy\n", source);
        REQUIRE(result.Main.Frames.size() == 2);
        CHECK(result.Main.Frames[1].Offset == ipos32 {10, 20});
        CHECK(result.Main.Frames[1].Motion == ipos32 {1, 2});
        CHECK(ImageImport::GetDurationMs(result.Main) == 400);
    }

    SECTION("AllowsUnequalDirectionCounts")
    {
        auto text = strex("Source = A.toy\nGrid = 1 1\nFrameIndices = 0 1 2\n[{} 1]\nFrameIndices = 4 5\n", FO_GEOMETRY == 1 ? "HexDir" : "SquareDir").str();
        auto result = LoadTransformRecipe(text);
        CHECK(result.Main.Frames.size() == 3);
        CHECK(result.Dirs[0].Frames.size() == 2);
    }

    SECTION("RejectsZeroOverflowAndAmbiguousTimeline")
    {
        for (string_view operation : {"Duration = 0", "Duration = 4294967296", "FrameDurations = 100 0 100", "FrameDurations = 4294967295 1 1", "FrameDurations = 100 200", "Duration = 2", "Duration = 300\nFps = 10", "Duration = 300\nFrameDurations = 100 100 100", "Fps = 10\nFrameDurations = 100 100 100"}) {
            INFO(operation);
            CHECK_THROWS(LoadTransformRecipe(strex("Frame = A.toy\nGrid = 1 1\nFrameIndices = 0 1 2\n{}\n", operation).str()));
        }
    }
}

FO_END_NAMESPACE
