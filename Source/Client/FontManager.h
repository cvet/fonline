// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "EffectManager.h"
#include "FileSystem.h"
#include "FontRasterizer.h"
#include "Geometry.h"
#include "RenderTarget.h"
#include "TextureAtlas.h"

FO_BEGIN_NAMESPACE

class ResourceManager;

FO_DECLARE_EXCEPTION(FontManagerException);

class SpriteManager;
class AtlasSprite;

namespace Color
{
    static constexpr auto TextDefault {ucolor {60, 248, 0}};
}

// Font slot index. Engine ships a single named slot (Default = 0); scripts may add more entries via the codegen Enum annotation
///@ ExportEnum
enum class FontType : int32_t
{
    Default = 0, // Built-in default font slot used when no project-defined slot is selected
};

// Font flags
///@ ExportEnum
enum class FontFlag : uint32_t
{
    None = 0, // Applies no optional text-layout or glyph-rendering flags
    NoWrap = 0x0001, // Truncates the remaining text at rectangle-width overflow instead of wrapping it
    TruncateLine = 0x0002, // Skips overflowing glyphs through the next newline instead of wrapping the current line
    CenterX = 0x0004, // Horizontally centers each rendered line within the target rectangle
    CenterY = 0x0008, // Vertically centers the complete text block within the target rectangle
    AlignRight = 0x0010, // Right-aligns each rendered line within the target rectangle
    AlignBottom = 0x0020, // Aligns the text block to the bottom and makes SkipLines count from the trailing lines
    KeepTail = 0x0040, // Renders the tail of a text block that is taller than the target rectangle
    NoColorize = 0x0080, // Removes inline color tags while retaining the supplied base text color
    Justify = 0x0100, // Distributes extra spacing between words to fill each line's target width
    Bordered = 0x0200, // Uses the bordered or outlined font-texture variant for glyph rendering
};

// Bundled text formatting parameters: font slot, FontFlag bitmask, and skip-lines counter.
// `SkipLines` is "skip from top" by default; with FontFlag::AlignBottom set it becomes "skip from bottom" (trailing lines)
// Font: Font slot selected for text measurement and rendering
// Flags: FontFlag bitmask controlling alignment, wrapping, clipping, color tags, and glyph presentation
// SkipLines: Line-skip count taken from the top by default or from the bottom when AlignBottom is set
///@ ExportValueType Layout = FontType-Font+FontFlag-Flags+int32-SkipLines
struct TextFormat
{
    FontType Font {};
    FontFlag Flags {};
    int32_t SkipLines {};
};
static_assert(sizeof(TextFormat) == 12 && std::is_standard_layout_v<TextFormat>);

// How TrueType glyphs are fitted to the pixel grid
///@ ExportEnum
enum class FontHinting : int32_t
{
    Light = 0, // Automatic hinting on the vertical axis only: crisp horizontal stems, shapes stay faithful
    Normal = 1, // Full hinting, by the font's own instructions where it carries them
    Unhinted = 2, // Outlines as drawn, not fitted to the pixel grid
};

// Rendering options of a font bound from TrueType faces; the zero value is antialiased, lightly hinted text
// without kerning or an outlined variant
///@ ExportValueType Layout = FontHinting-Hinting+int32-OutlineWidth+int32-LineSpacing+bool-Monochrome+bool-Kerning+bool-ForceAutoHint+bool-Embolden
struct FontOptions
{
    FontHinting Hinting {};
    int32_t OutlineWidth {}; // Pixels of the dark outline drawn by FontFlag::Bordered, zero keeps that variant unoutlined
    int32_t LineSpacing {}; // Pixels added between lines on top of the face's own line gap, may be negative
    bool Monochrome {}; // Render without antialiasing, for pixel fonts used at their design size
    bool Kerning {};
    bool ForceAutoHint {}; // Use FreeType's auto-hinter instead of the face's instructions; Unhinted still disables hinting
    bool Embolden {}; // Synthetic bold: grow the rasterized strokes one pixel to the right and advance one pixel farther
};
static_assert(sizeof(FontOptions) == 16 && std::is_standard_layout_v<FontOptions>);

class FontManager final
{
public:
    FontManager() = delete;
    explicit FontManager(ptr<SpriteManager> spr_mngr, ptr<ResourceManager> res_mngr);
    FontManager(const FontManager&) = delete;
    FontManager(FontManager&&) noexcept = delete;
    auto operator=(const FontManager&) = delete;
    auto operator=(FontManager&&) noexcept = delete;
    ~FontManager() = default;

    // Measuring may rasterize the TrueType glyphs a text needs for the first time, hence the non-const queries
    [[nodiscard]] auto GetLinesCount(isize32 size, string_view str, FontType num_font) -> int32_t;
    [[nodiscard]] auto GetLinesHeight(isize32 size, string_view str, FontType num_font) -> int32_t;
    [[nodiscard]] auto GetLineHeight(FontType num_font) const -> int32_t;
    [[nodiscard]] auto GetTextInfo(isize32 size, string_view str, TextFormat format, isize32& result_size, int32_t& lines) -> bool;
    [[nodiscard]] auto HaveLetter(FontType num_font, uint32_t letter) const -> bool;

    void BindFoFont(FontType font, string_view font_path, AtlasType atlas_type, bool not_bordered, bool skip_if_loaded, float32_t default_scale = 1.0f);
    void BindBmfFont(FontType font, string_view font_path, AtlasType atlas_type, float32_t default_scale = 1.0f);
    // Faces form a fallback chain: a letter comes from the first face that has it, or is the first face's missing-glyph box.
    // Preload ranges are inclusive codepoint pairs rasterized at once; every other letter is rasterized on first use
    void BindTrueTypeFont(FontType font, const vector<string>& face_paths, int32_t pixel_size, FontOptions options, const vector<int32_t>& preload_ranges, AtlasType atlas_type);
    void SetFontEffect(FontType font, nptr<RenderEffect> effect);
    void DrawText(irect32 rect, string_view str, ucolor color, TextFormat format);
    auto SplitLines(irect32 rect, string_view cstr, FontType num_font) -> vector<string>;
    void ClearFonts();
    void FrameUpdate();

private:
    enum class FormatMode : int32_t
    {
        Draw,
        Split,
        LineCount,
    };

    struct FontData
    {
        struct Letter
        {
            ipos32 Pos {};
            isize32 Size {};
            ipos32 Offset {};
            int32_t XAdvance {};
            frect32 TexPos {};
            frect32 TexBorderedPos {};
            int32_t FaceIndex {-1};
            uint32_t GlyphIndex {};
        };

        // Glyph cells are packed in shelves; the sheet doubles when full and is uploaded again as a new atlas sprite
        struct TrueTypeSheet
        {
            vector<unique_ptr<FontFace>> Faces {};
            FontOptions Options {};
            AtlasType Atlas {};
            int32_t Ascender {};
            int32_t CellPadding {};
            isize32 Size {};
            vector<ucolor> Pixels {};
            vector<ucolor> BorderedPixels {};
            ipos32 ShelfPos {};
            int32_t ShelfHeight {};
            optional<Letter> MissingLetter {};
        };

        nptr<RenderEffect> DrawEffect {};
        nptr<RenderTexture> FontTex {};
        nptr<RenderTexture> FontTexBordered {};
        unordered_map<uint32_t, Letter> Letters {};
        int32_t SpaceWidth {};
        int32_t LineHeight {};
        int32_t YAdvance {};
        float32_t BakeScale {1.0f};
        shared_ptr<AtlasSprite> ImageNormal {};
        shared_ptr<AtlasSprite> ImageBordered {};
        bool MakeGray {};
        unique_nptr<TrueTypeSheet> TrueType {};
    };

    struct FontFormatInfo
    {
        TextFormat Format {};
        nptr<const FontData> CurFont {};
        irect32 Rect {};
        string Text {};
        int32_t LinesAll {1};
        int32_t LinesInRect {};
        int32_t CurX {};
        int32_t CurY {};
        int32_t MaxCurX {};
        vector<ucolor> TextColor {};
        vector<int32_t> LineWidth {};
        vector<int32_t> LineSpaceWidth {};
        int32_t ColorOffset {};
        ucolor Color {Color::TextDefault};
        vector<string> Lines {};
        uint64_t LastUsedFrame {};
    };

    auto GetFont(FontType font) -> ptr<FontData>;
    auto GetFont(FontType font) const -> ptr<const FontData>;

    void StoreFont(int32_t index, FontData&& font_data);
    void BuildFont(int32_t index);
    static void BakeFontScale(FontData& font, vector<ucolor>& sheet_data, isize32 sheet_size);
    void FormatText(FontFormatInfo& fi, FormatMode mode) const;
    static auto ResolveFontScale(float32_t scale) -> float32_t;
    static auto IsInlineColorHex(string_view value) -> bool;
    static auto ParseInlineColorTag(string_view str, size_t marker_pos, size_t& tag_end, ucolor& color, bool& reset) -> bool;
    static auto GetKerning(const FontData& font, nptr<const FontData::Letter> left, const FontData::Letter& right) -> int32_t;
    auto GetOrFormat(TextFormat format, FontType font, irect32 rect, ucolor color, FormatMode mode, string_view str) -> ptr<const FontFormatInfo>;

    auto ReadFontFile(string_view path) -> shared_ptr<const vector<uint8_t>>;
    void EnsureLetters(FontData& font, string_view str);
    void AddTrueTypeLetters(FontData& font, const vector<uint32_t>& letters, bool substitute_missing);
    auto RasterizeTrueTypeLetter(FontData& font, int32_t face_index, uint32_t glyph_index, vector<irect32>& new_cells) -> FontData::Letter;
    auto ReserveSheetCell(FontData::TrueTypeSheet& sheet, isize32 cell_size) -> ipos32;
    void UploadTrueTypeSheet(FontData& font);
    void UploadTrueTypeCell(FontData& font, irect32 cell);
    static void UpdateLetterTexPos(const FontData& font, FontData::Letter& letter);

    static constexpr string_view InlineColorTagPrefix {"@color"};

    ptr<SpriteManager> _sprMngr;
    ptr<ResourceManager> _resMngr;
    unique_nptr<FontRasterizer> _rasterizer {};
    unordered_map<string, shared_ptr<const vector<uint8_t>>> _fontFiles {};
    vector<optional<FontData>> _allFonts {};
    uint64_t _frameIndex {1};
    mutable unordered_map<uint64_t, unique_ptr<FontFormatInfo>> _formatCache {};
};

FO_END_NAMESPACE
