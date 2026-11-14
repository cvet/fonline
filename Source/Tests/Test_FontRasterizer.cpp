// FOnline Engine
// SPDX-License-Identifier: MIT

#include "catch_amalgamated.hpp"

#include "FontRasterizer.h"
#include "Test_FontFixtures.h"

FO_BEGIN_NAMESPACE

template<size_t N>
static auto MakeFontData(const std::array<uint8_t, N>& bytes) -> shared_ptr<const vector<uint8_t>>
{
    return safe_alloc::make_shared<const vector<uint8_t>>(bytes.begin(), bytes.end());
}

TEST_CASE("FontRasterizerReadsTheFixtureFontExactly")
{
    // At 20 px every fixture coordinate, a multiple of 50 font units, lands on a whole pixel
    FontRasterizer rasterizer;
    auto face = rasterizer.OpenFace(MakeFontData(FontTests::MainFont), 20, FontFaceHinting::None, false);

    SECTION("LineMetricsFollowTheFontUnits")
    {
        FontFaceMetrics metrics = face->GetMetrics();
        CHECK(metrics.Ascender == 16);
        CHECK(metrics.Descender == 4);
        CHECK(metrics.LineGap == 0);
    }

    SECTION("GlyphIndicesComeFromTheUnicodeMap")
    {
        CHECK(face->GetGlyphIndex(uint32_t {'A'}) != 0);
        CHECK(face->GetGlyphIndex(uint32_t {0x416}) != 0);
        CHECK(face->GetGlyphIndex(uint32_t {0x4E2D}) == 0);
    }

    SECTION("ARectangleRendersAsItsRectangle")
    {
        RasterizedGlyph glyph = face->RenderGlyph(face->GetGlyphIndex(uint32_t {'A'}));
        CHECK(glyph.Size == isize32 {10, 14});
        CHECK(glyph.Bearing == ipos32 {1, 14});
        CHECK(glyph.Advance == 12);
        REQUIRE(glyph.Coverage.size() == 140);
        CHECK(std::ranges::all_of(glyph.Coverage, [](uint8_t value) { return value == 255; }));
    }

    SECTION("ADescenderReachesBelowTheBaseline")
    {
        RasterizedGlyph glyph = face->RenderGlyph(face->GetGlyphIndex(uint32_t {'p'}));
        CHECK(glyph.Bearing.y == 10);
        CHECK(glyph.Size.height == 14);
    }

    SECTION("SpaceIsAnEmptyBitmapWithAnAdvance")
    {
        RasterizedGlyph glyph = face->RenderGlyph(face->GetGlyphIndex(uint32_t {' '}));
        CHECK(glyph.Size == isize32 {0, 0});
        CHECK(glyph.Coverage.empty());
        CHECK(glyph.Advance == 6);
    }

    SECTION("PairKerningComesFromGpos")
    {
        // The pair is -150 font units; FreeType damps grid-fitted kerning below 25 ppem by ppem / 25
        uint32_t a = face->GetGlyphIndex(uint32_t {'A'});
        uint32_t v = face->GetGlyphIndex(uint32_t {'V'});
        CHECK(face->GetKerning(a, v) == -2);
        CHECK(face->GetKerning(v, a) == 0);
        CHECK(face->GetKerning(0, v) == 0);

        auto large_face = rasterizer.OpenFace(MakeFontData(FontTests::MainFont), 40, FontFaceHinting::None, false);
        CHECK(large_face->GetKerning(large_face->GetGlyphIndex(uint32_t {'A'}), large_face->GetGlyphIndex(uint32_t {'V'})) == -6);
    }

    SECTION("TheMissingGlyphIsTheNotdefBox")
    {
        RasterizedGlyph glyph = face->RenderGlyph(0);
        CHECK(glyph.Size == isize32 {8, 14});
        CHECK(glyph.Advance == 10);
        CHECK(std::ranges::count(glyph.Coverage, uint8_t {0}) > 0);
    }
}

TEST_CASE("FontRasterizerAntialiasingAndHinting")
{
    FontRasterizer rasterizer;
    auto font_data = MakeFontData(FontTests::MainFont);

    // At 13 px a 50-unit edge falls at 0.65 px, so the grey rasterizer produces partial coverage
    SECTION("GreyRenderingCoversFractionalEdgesPartially")
    {
        auto face = rasterizer.OpenFace(font_data, 13, FontFaceHinting::None, false);
        RasterizedGlyph glyph = face->RenderGlyph(face->GetGlyphIndex(uint32_t {'A'}));
        CHECK(std::ranges::any_of(glyph.Coverage, [](uint8_t value) { return value != 0 && value != 255; }));
    }

    SECTION("MonochromeRenderingIsBinary")
    {
        auto face = rasterizer.OpenFace(font_data, 13, FontFaceHinting::Light, true);
        RasterizedGlyph glyph = face->RenderGlyph(face->GetGlyphIndex(uint32_t {'A'}));
        REQUIRE_FALSE(glyph.Coverage.empty());
        CHECK(std::ranges::all_of(glyph.Coverage, [](uint8_t value) { return value == 0 || value == 255; }));
    }

    SECTION("HintedAdvancesAreWholePixels")
    {
        auto face = rasterizer.OpenFace(font_data, 13, FontFaceHinting::Light, false);
        RasterizedGlyph glyph = face->RenderGlyph(face->GetGlyphIndex(uint32_t {'W'}));
        CHECK(glyph.Advance == 12);
    }

    SECTION("TheSameInputRendersTheSameBytes")
    {
        auto first = rasterizer.OpenFace(font_data, 13, FontFaceHinting::Normal, false);
        auto second = rasterizer.OpenFace(font_data, 13, FontFaceHinting::Normal, false);

        for (uint32_t letter = FontTests::BulkFirstLetter; letter < FontTests::BulkFirstLetter + FontTests::BulkLetterCount; letter++) {
            RasterizedGlyph from_first = first->RenderGlyph(first->GetGlyphIndex(letter));
            RasterizedGlyph from_second = second->RenderGlyph(second->GetGlyphIndex(letter));
            CHECK(from_first.Coverage == from_second.Coverage);
            CHECK(from_first.Advance == from_second.Advance);
        }
    }
}

TEST_CASE("FontRasterizerAutoHintAndSyntheticBold")
{
    FontRasterizer rasterizer;

    SECTION("AutoHintIgnoresNativeGlyphInstructions")
    {
        auto font_data = MakeFontData(FontTests::HintedFont);
        auto native_face = rasterizer.OpenFace(font_data, 20, FontFaceHinting::Normal, true);
        auto automatic_face = rasterizer.OpenFace(font_data, 20, FontFaceHinting::Normal, true, true);
        RasterizedGlyph native = native_face->RenderGlyph(native_face->GetGlyphIndex(uint32_t {'A'}));
        RasterizedGlyph automatic = automatic_face->RenderGlyph(automatic_face->GetGlyphIndex(uint32_t {'A'}));
        CHECK(native.Bearing.y == automatic.Bearing.y + 1);
        CHECK(std::ranges::all_of(automatic.Coverage, [](uint8_t value) { return value == 0 || value == 255; }));
    }

    SECTION("UnhintedDisablesBothHinters")
    {
        auto font_data = MakeFontData(FontTests::HintedFont);
        auto plain_face = rasterizer.OpenFace(font_data, 13, FontFaceHinting::None, true);
        auto automatic_face = rasterizer.OpenFace(font_data, 13, FontFaceHinting::None, true, true);
        RasterizedGlyph plain = plain_face->RenderGlyph(plain_face->GetGlyphIndex(uint32_t {'A'}));
        RasterizedGlyph automatic = automatic_face->RenderGlyph(automatic_face->GetGlyphIndex(uint32_t {'A'}));
        CHECK(plain.Coverage == automatic.Coverage);
        CHECK(plain.Bearing == automatic.Bearing);
    }

    SECTION("SyntheticBoldGrowsCoverageAndAdvanceByOnePixel")
    {
        auto font_data = MakeFontData(FontTests::MainFont);
        auto plain_face = rasterizer.OpenFace(font_data, 20, FontFaceHinting::None, true);
        auto bold_face = rasterizer.OpenFace(font_data, 20, FontFaceHinting::None, true, false, true);
        RasterizedGlyph plain = plain_face->RenderGlyph(plain_face->GetGlyphIndex(uint32_t {'A'}));
        RasterizedGlyph bold = bold_face->RenderGlyph(bold_face->GetGlyphIndex(uint32_t {'A'}));
        CHECK(bold.Size == isize32 {plain.Size.width + 1, plain.Size.height});
        CHECK(bold.Bearing == plain.Bearing);
        CHECK(bold.Advance == plain.Advance + 1);
        CHECK(std::ranges::all_of(bold.Coverage, [](uint8_t value) { return value == 255; }));
        RasterizedGlyph space = bold_face->RenderGlyph(bold_face->GetGlyphIndex(uint32_t {' '}));
        CHECK(std::ranges::all_of(space.Coverage, [](uint8_t value) { return value == 0; }));
        CHECK(space.Advance == 7);
    }
}

TEST_CASE("FontRasterizerRejectsWhatItCannotRender")
{
    FontRasterizer rasterizer;

    SECTION("DataThatIsNoFontFails")
    {
        auto garbage = safe_alloc::make_shared<const vector<uint8_t>>(vector<uint8_t>(64, uint8_t {0x5A}));
        CHECK_THROWS_AS(rasterizer.OpenFace(garbage, 16, FontFaceHinting::Light, false), FontRasterizerException);
    }

    SECTION("APixelSizeMustBePositive")
    {
        CHECK_THROWS(rasterizer.OpenFace(MakeFontData(FontTests::MainFont), 0, FontFaceHinting::Light, false));
    }

    SECTION("AFaceOutlivesTheRasterizerThatOpenedIt")
    {
        unique_nptr<FontFace> face;

        {
            FontRasterizer scoped_rasterizer;
            face = scoped_rasterizer.OpenFace(MakeFontData(FontTests::FallbackFont), 16, FontFaceHinting::Light, false);
        }

        CHECK(face->GetGlyphIndex(uint32_t {0x4E2D}) != 0);
        CHECK(face->RenderGlyph(face->GetGlyphIndex(uint32_t {0x4E2D})).Advance == 16);
    }
}

FO_END_NAMESPACE
