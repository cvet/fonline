// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "catch_amalgamated.hpp"

#include "ServerImage.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

// One direction, one frame and the given pixels in row order, as the image baker writes them
static auto MakeServerImageTestSprite(uint16_t width, uint16_t height, const vector<ucolor>& pixels) -> vector<uint8_t>
{
    vector<uint8_t> sprite_data {BakerTests::MakeMinimalBakedSprite(width, height)};
    size_t pixels_offset {30};

    REQUIRE(pixels.size() == numeric_cast<size_t>(width) * height);

    for (size_t i {}; i < pixels.size(); i++) {
        sprite_data[pixels_offset + i * 4 + 0] = pixels[i].comp.r;
        sprite_data[pixels_offset + i * 4 + 1] = pixels[i].comp.g;
        sprite_data[pixels_offset + i * 4 + 2] = pixels[i].comp.b;
        sprite_data[pixels_offset + i * 4 + 3] = pixels[i].comp.a;
    }

    return sprite_data;
}

TEST_CASE("ServerImage")
{
    BakerTests::TestRig rig {};

    SECTION("ReadsPixelsOfASingleFrameImage")
    {
        vector<ucolor> pixels {ucolor {10, 20, 30, 255}, ucolor {40, 50, 60, 255}, ucolor {70, 80, 90, 128}, ucolor {1, 2, 3, 0}, ucolor {4, 5, 6, 7}, ucolor {8, 9, 10, 11}};
        rig.AddBakedFile("Relief.png", MakeServerImageTestSprite(3, 2, pixels));

        refcount_ptr<ServerImage> image {LoadServerImage(rig.BakedFiles, "Relief.png")};

        CHECK(image->GetSize() == isize32 {3, 2});
        CHECK(image->GetColor({0, 0}) == pixels[0]);
        CHECK(image->GetColor({2, 0}) == pixels[2]);
        CHECK(image->GetColor({0, 1}) == pixels[3]);
        CHECK(image->GetColor({2, 1}) == pixels[5]);
    }

    SECTION("RejectsPositionsOutsideTheImage")
    {
        rig.AddBakedFile("Small.png", BakerTests::MakeMinimalBakedSprite(2, 2));

        refcount_ptr<ServerImage> image {LoadServerImage(rig.BakedFiles, "Small.png")};

        CHECK_THROWS_AS(image->GetColor({-1, 0}), ScriptException);
        CHECK_THROWS_AS(image->GetColor({0, -1}), ScriptException);
        CHECK_THROWS_AS(image->GetColor({2, 0}), ScriptException);
        CHECK_THROWS_AS(image->GetColor({0, 2}), ScriptException);
        CHECK(image->GetColor({1, 1}) == ucolor {255, 255, 255, 255});
    }

    SECTION("RejectsMissingAndAnimatedImages")
    {
        rig.AddBakedFile("Animated.png", BakerTests::MakeMultiFrameBakedSprite(2));

        CHECK_THROWS_AS(LoadServerImage(rig.BakedFiles, ""), ScriptException);
        CHECK_THROWS_AS(LoadServerImage(rig.BakedFiles, "Missing.png"), ScriptException);
        CHECK_THROWS_AS(LoadServerImage(rig.BakedFiles, "Animated.png"), ScriptException);
    }
}

FO_END_NAMESPACE
