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

#pragma once

#include "Common.h"

#include "EffectManager.h"
#include "FileSystem.h"
#include "Geometry.h"
#include "RenderTarget.h"
#include "TextureAtlas.h"

FO_BEGIN_NAMESPACE

class SpriteManager;
class AtlasSprite;

// Font slot index. Engine ships a single named slot (Default = 0); scripts may add more entries via the codegen Enum annotation.
///@ ExportEnum
enum class FontType : int32_t
{
    Default = 0,
};

// Font flags
///@ ExportEnum
enum class FontFlag : uint32_t
{
    None = 0,
    NoWrap = 0x0001, // On rect-width overflow truncate the rest of the text instead of wrapping it to the next line
    TruncateLine = 0x0002, // On rect-width overflow skip remaining glyphs until the next '\n' instead of wrapping
    CenterX = 0x0004, // Horizontally center each line within the rect
    CenterY = 0x0008, // Vertically center the text block within the rect
    AlignRight = 0x0010, // Right-align each line within the rect
    AlignBottom = 0x0020, // Vertically align the text block to the rect's bottom edge; also flips TextFormat::SkipLines from "skip from top" to "skip from bottom"
    KeepTail = 0x0040, // When the text block is taller than the rect, render its tail (skip the leading overflowing lines)
    NoColorize = 0x0080, // Skip parsing of inline color tokens (|c... / |xRRGGBB), render text with the base color as-is
    Justify = 0x0100, // Justify each line: distribute extra spaces between words to fill the rect width
    Bordered = 0x0200, // Render glyphs from the bordered/outlined font texture variant instead of the regular one
};

// Bundled text formatting parameters: font slot, FontFlag bitmask, and skip-lines counter.
// `SkipLines` is "skip from top" by default; with FontFlag::AlignBottom set it becomes "skip from bottom" (trailing lines).
///@ ExportValueType Layout = FontType-Font+FontFlag-Flags+int32-SkipLines
struct TextFormat
{
    FontType Font {};
    FontFlag Flags {};
    int32_t SkipLines {};
};
static_assert(sizeof(TextFormat) == 12 && std::is_standard_layout_v<TextFormat>);

class FontManager final
{
public:
    FontManager() = delete;
    explicit FontManager(SpriteManager& spr_mngr);
    FontManager(const FontManager&) = delete;
    FontManager(FontManager&&) noexcept = delete;
    auto operator=(const FontManager&) = delete;
    auto operator=(FontManager&&) noexcept = delete;
    ~FontManager() = default;

    [[nodiscard]] auto GetLinesCount(isize32 size, string_view str, FontType font) const -> int32_t;
    [[nodiscard]] auto GetLinesHeight(isize32 size, string_view str, FontType font) const -> int32_t;
    [[nodiscard]] auto GetLineHeight(FontType font) const -> int32_t;
    [[nodiscard]] auto GetTextInfo(isize32 size, string_view str, TextFormat format, isize32& result_size, int32_t& lines) const -> bool;
    [[nodiscard]] auto HaveLetter(FontType font, uint32_t letter) const -> bool;

    auto LoadFontFO(FontType font, string_view font_name, AtlasType atlas_type, bool not_bordered, bool skip_if_loaded) -> bool;
    auto LoadFontBmf(FontType font, string_view font_name, AtlasType atlas_type) -> bool;
    void SetDefaultFont(FontType font);
    void SetFontEffect(FontType font, RenderEffect* effect);
    void DrawText(irect32 rect, string_view str, ucolor color, TextFormat format);
    auto SplitLines(irect32 rect, string_view cstr, FontType font) -> vector<string>;
    void ClearFonts();

private:
    static constexpr int32_t FORMAT_TYPE_DRAW = 0;
    static constexpr int32_t FORMAT_TYPE_SPLIT = 1;
    static constexpr int32_t FORMAT_TYPE_LCOUNT = 2;

    static constexpr auto COLOR_TEXT_DEFAULT = ucolor {60, 248, 0};

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
        };

        raw_ptr<RenderEffect> DrawEffect {};
        raw_ptr<RenderTexture> FontTex {};
        raw_ptr<RenderTexture> FontTexBordered {};
        unordered_map<uint32_t, Letter> Letters {};
        int32_t SpaceWidth {};
        int32_t LineHeight {};
        int32_t YAdvance {};
        shared_ptr<AtlasSprite> ImageNormal {};
        shared_ptr<AtlasSprite> ImageBordered {};
        bool MakeGray {};
    };

    // Todo: optimize text formatting - cache previous results
    struct FontFormatInfo
    {
        raw_ptr<const FontData> CurFont {};
        TextFormat Format {};
        irect32 Rect {};
        vector<char> Str {};
        raw_ptr<char> PStr {};
        int32_t LinesAll {1};
        int32_t LinesInRect {};
        int32_t CurX {};
        int32_t CurY {};
        int32_t MaxCurX {};
        vector<ucolor> ColorDots {};
        vector<int32_t> LineWidth {};
        vector<int32_t> LineSpaceWidth {};
        int32_t OffsColDots {};
        ucolor DefColor {COLOR_TEXT_DEFAULT};
        raw_ptr<vector<string>> StrLines {};
        bool IsError {};

        void Prepare(string_view str)
        {
            const auto str_len = str.length();
            const auto max_chars = std::max<size_t>(str_len * 2 + 1, 1);
            const auto max_lines = std::max<size_t>(str_len + 1, 1);

            Str.assign(max_chars, '\0');
            ColorDots.assign(max_chars, ucolor::clear);
            LineWidth.assign(max_lines, 0);
            LineSpaceWidth.assign(max_lines, 0);
            PStr = Str.data();
            LinesAll = 1;
            LinesInRect = 0;
            CurX = 0;
            CurY = 0;
            MaxCurX = 0;
            OffsColDots = 0;
            StrLines = nullptr;
            IsError = false;
        }
    };

    [[nodiscard]] auto GetFont(FontType font) -> FontData*;
    [[nodiscard]] auto GetFont(FontType font) const -> const FontData*;

    void BuildFont(int32_t index);
    void FormatText(FontFormatInfo& fi, int32_t fmt_type) const;

    raw_ptr<SpriteManager> _sprMngr;
    vector<unique_ptr<FontData>> _allFonts {};
    int32_t _defFontIndex {};
    mutable FontFormatInfo _fontFormatInfoBuf {};
};

FO_END_NAMESPACE
