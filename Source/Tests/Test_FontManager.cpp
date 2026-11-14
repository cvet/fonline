// FOnline Engine
// SPDX-License-Identifier: MIT

#include "catch_amalgamated.hpp"

#include "Application.h"
#include "DefaultSprites.h"
#include "EffectManager.h"
#include "FontManager.h"
#include "ResourceManager.h"
#include "SpriteManager.h"
#include "Test_BakerHelpers.h"
#include "Test_FontFixtures.h"

FO_BEGIN_NAMESPACE

static constexpr string_view MainFontPath = "Fonts/TestSans.ttf";
static constexpr string_view FallbackFontPath = "Fonts/TestFallback.ttf";

class FontManagerTestSettings final : public GlobalSettings
{
public:
    FontManagerTestSettings();
};

// The sprite and font managers a client builds, minus the client: enough to bind, measure and upload fonts
class TrueTypeFontRig final
{
public:
    TrueTypeFontRig();
    TrueTypeFontRig(const TrueTypeFontRig&) = delete;
    TrueTypeFontRig(TrueTypeFontRig&&) noexcept = delete;
    auto operator=(const TrueTypeFontRig&) = delete;
    auto operator=(TrueTypeFontRig&&) noexcept = delete;
    ~TrueTypeFontRig() = default;

    [[nodiscard]] auto GetFonts() -> FontManager& { return _fonts; }
    [[nodiscard]] auto GetAssets() -> ResourceManager& { return _assets; }
    [[nodiscard]] auto GetTextWidth(string_view text) -> int32_t;

    void Bind(const vector<string>& faces, int32_t pixel_size, FontOptions options = {}, const vector<int32_t>& preload_ranges = {});

private:
    FontManagerTestSettings _settings {};
    FileSystem _resources;
    hash_storage _hashes {};
    GameTimer _timer;
    EffectManager _effects;
    SpriteManager _sprites;
    ResourceManager _assets;
    FontManager _fonts;
};

static auto MakeFontResources() -> FileSystem
{
    auto source = safe_alloc::make_unique<BakerTests::MemoryDataSource>("FontManagerTests");
    source->AddFile(MainFontPath, vector<uint8_t>(FontTests::MainFont.begin(), FontTests::MainFont.end()));
    source->AddFile(FallbackFontPath, vector<uint8_t>(FontTests::FallbackFont.begin(), FontTests::FallbackFont.end()));

    FileSystem resources;
    resources.AddCustomSource(std::move(source));
    return resources;
}

static auto MakeUnhintedOptions() -> FontOptions
{
    // Unhinted outlines keep every fixture coordinate on the exact pixel the font units give
    FontOptions options;
    options.Hinting = FontHinting::Unhinted;
    return options;
}

FontManagerTestSettings::FontManagerTestSettings() :
    GlobalSettings {false}
{
    SetupDefaultSettings();
    BakerTests::ApplySelfContainedClientSettings(*this);
}

TrueTypeFontRig::TrueTypeFontRig() :
    _resources {MakeFontResources()},
    _timer {&_settings},
    _effects {&_settings, &_resources, GetApp()->MainWindow.GetRender()},
    _sprites {&_settings, &GetApp()->MainWindow, &_timer, &_effects, &_hashes},
    _assets {&_resources, &_sprites},
    _fonts {&_sprites, &_assets}
{
    _assets.RegisterSpriteFactory(safe_alloc::make_unique<DefaultSpriteFactory>(&_sprites, &_assets));
}

auto TrueTypeFontRig::GetTextWidth(string_view text) -> int32_t
{
    isize32 size;
    int32_t lines = 0;
    TextFormat format;
    format.Font = FontType::Default;
    REQUIRE(_fonts.GetTextInfo({}, text, format, size, lines));
    return size.width;
}

void TrueTypeFontRig::Bind(const vector<string>& faces, int32_t pixel_size, FontOptions options, const vector<int32_t>& preload_ranges)
{
    _fonts.BindTrueTypeFont(FontType::Default, faces, pixel_size, options, preload_ranges, AtlasType::IfaceSprites);
}

TEST_CASE("FontManagerLaysOutTrueTypeText")
{
    // At 20 px the fixture font's ascender is 16 px, its descender 4 px and its line gap zero
    TrueTypeFontRig rig;
    FontManager& fonts = rig.GetFonts();
    rig.Bind({string(MainFontPath)}, 20, MakeUnhintedOptions());

    SECTION("TheLineStepComesFromTheFaceMetrics")
    {
        CHECK(fonts.GetLineHeight(FontType::Default) == 20);
        CHECK(fonts.GetLinesHeight({200, 100}, "A", FontType::Default) == 20);
        CHECK(fonts.GetLinesHeight({200, 100}, "A\nA\nA", FontType::Default) == 60);
    }

    SECTION("LettersAdvanceByTheirOwnWidths")
    {
        CHECK(rig.GetTextWidth("AAA") == 36);
        CHECK(rig.GetTextWidth("A A") == 30);
        CHECK(rig.GetTextWidth("W") == 18);
        CHECK(rig.GetTextWidth("\xD0\x96") == 16);
    }

    SECTION("WrappingUsesTheSameAdvances")
    {
        CHECK(fonts.GetLinesCount({30, 200}, "AA AA", FontType::Default) == 2);
        CHECK(fonts.GetLinesCount({60, 200}, "AA AA", FontType::Default) == 1);
    }

    SECTION("AnOverwideLetterMakesProgress")
    {
        isize32 size;
        int32_t lines = 0;
        TextFormat format;
        format.Font = FontType::Default;

        REQUIRE(fonts.GetTextInfo({1, 200}, "AA", format, size, lines));
        CHECK(size.width == 12);
        CHECK(size.height == 40);
        CHECK(lines == 2);
        CHECK(fonts.GetLinesCount({1, 200}, "AA", FontType::Default) == 2);
        CHECK(fonts.SplitLines({0, 0, 1, 20}, "AA", FontType::Default) == vector<string> {"A", "A"});
    }

    SECTION("ControlCharactersTakeNoRoom")
    {
        CHECK(rig.GetTextWidth("\x01"
                               "A") == 12);
    }

    SECTION("ALetterNoFaceCarriesIsTheMissingGlyphBox")
    {
        // U+2603 and U+2604 are in neither fixture font; each shows the 10 px wide box instead of vanishing
        CHECK(rig.GetTextWidth("\xE2\x98\x83") == 10);
        CHECK(rig.GetTextWidth("\xE2\x98\x83\xE2\x98\x84") == 20);
        CHECK_FALSE(fonts.HaveLetter(FontType::Default, uint32_t {0x2603}));
    }
}

TEST_CASE("FontManagerTrueTypeKerningAndFallback")
{
    TrueTypeFontRig rig;
    FontManager& fonts = rig.GetFonts();

    SECTION("KerningIsOptIn")
    {
        // At 40 px the fixture's -150 unit pair is exactly -6 px, above the size where FreeType damps kerning
        rig.Bind({string(MainFontPath)}, 40, MakeUnhintedOptions());
        CHECK(rig.GetTextWidth("AV") == 48);

        FontOptions kerned = MakeUnhintedOptions();
        kerned.Kerning = true;
        rig.Bind({string(MainFontPath)}, 40, kerned);
        CHECK(rig.GetTextWidth("AV") == 42);
        CHECK(rig.GetTextWidth("VA") == 48);
        CHECK(rig.GetTextWidth("A V") == 60);
        CHECK(fonts.GetLinesCount({42, 200}, "AV", FontType::Default) == 1);
        CHECK(fonts.GetLinesCount({41, 200}, "AV", FontType::Default) == 2);
    }

    SECTION("AFallbackFaceSuppliesWhatTheFirstLacks")
    {
        rig.Bind({string(MainFontPath)}, 20, MakeUnhintedOptions());
        CHECK_FALSE(fonts.HaveLetter(FontType::Default, uint32_t {0x4E2D}));

        rig.Bind({string(MainFontPath), string(FallbackFontPath)}, 20, MakeUnhintedOptions());
        CHECK(fonts.HaveLetter(FontType::Default, uint32_t {0x4E2D}));
        CHECK(rig.GetTextWidth("A\xE4\xB8\xAD") == 32);
    }

    SECTION("LineSpacingAddsToTheFaceGap")
    {
        FontOptions spaced = MakeUnhintedOptions();
        spaced.LineSpacing = 3;
        rig.Bind({string(MainFontPath)}, 20, spaced);
        CHECK(fonts.GetLinesHeight({200, 100}, "A\nA", FontType::Default) == 43);
    }
}

TEST_CASE("FontManagerTrueTypeSheetLifetime")
{
    TrueTypeFontRig rig;
    FontManager& fonts = rig.GetFonts();

    SECTION("ASheetGrowsToHoldEveryLetterItMeets")
    {
        // More letters than the first sheet a 40 px font gets; each is measured again after the sheet moved
        rig.Bind({string(MainFontPath)}, 40, MakeUnhintedOptions());

        string bulk;
        int32_t expected_width = 0;

        for (uint32_t index = 0; index < FontTests::BulkLetterCount; index++) {
            char encoded[4] = {};
            size_t encoded_len = utf8::encode(FontTests::BulkFirstLetter + index, encoded);
            bulk.append(encoded, encoded_len);
            expected_width += (600 + numeric_cast<int32_t>(index % 7) * 50) * 40 / 1000;
        }

        CHECK(rig.GetTextWidth(bulk) == expected_width);
        CHECK(rig.GetTextWidth("AAA") == 72);
    }

    SECTION("RebindingReleasesTheOldSheet")
    {
        FontOptions outlined = MakeUnhintedOptions();
        outlined.OutlineWidth = 2;
        rig.Bind({string(MainFontPath)}, 24, outlined);

        vector<ucolor> probe_pixels {ucolor::clear};
        auto probe = rig.GetAssets().CreateQuadSprite(AtlasType::IfaceSprites, {1, 1}, probe_pixels);
        auto atlas = probe->GetAtlas();
        REQUIRE(atlas);
        size_t used_area = atlas->GetLayout()->GetUsedArea();

        for (int32_t attempt = 0; attempt < 20; attempt++) {
            rig.Bind({string(MainFontPath)}, 24, outlined);
            CHECK(rig.GetTextWidth("AV") > 0);
        }

        CHECK(atlas->GetLayout()->GetUsedArea() == used_area);
    }

    SECTION("PreloadSkipsLettersNoFaceCarries")
    {
        CHECK_NOTHROW(rig.Bind({string(MainFontPath)}, 20, MakeUnhintedOptions(), {0x41, 0x5A, 0x2600, 0x2610}));
        CHECK(fonts.HaveLetter(FontType::Default, uint32_t {'A'}));
        CHECK(rig.GetTextWidth("A") == 12);
    }

    SECTION("ARejectedBindingLeavesThePreviousOne")
    {
        rig.Bind({string(MainFontPath)}, 20, MakeUnhintedOptions());

        FontOptions too_thick = MakeUnhintedOptions();
        too_thick.OutlineWidth = 100;

        CHECK_THROWS(rig.Bind({}, 20));
        CHECK_THROWS(rig.Bind({string(MainFontPath)}, 0));
        CHECK_THROWS(rig.Bind({string(MainFontPath)}, 1000));
        CHECK_THROWS(rig.Bind({string(MainFontPath)}, 20, too_thick));
        CHECK_THROWS(rig.Bind({string(MainFontPath)}, 20, {}, {0x41}));
        CHECK_THROWS(rig.Bind({string(MainFontPath)}, 20, {}, {0x5A, 0x41}));
        CHECK_THROWS_AS(rig.Bind({"Fonts/NoSuchFont.ttf"}, 20), FontManagerException);

        CHECK(fonts.GetLineHeight(FontType::Default) == 20);
        CHECK(rig.GetTextWidth("AAA") == 36);
    }
}

FO_END_NAMESPACE
