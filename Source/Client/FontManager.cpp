// FOnline Engine
// SPDX-License-Identifier: MIT

#include "FontManager.h"

#include "DefaultSprites.h"
#include "ResourceDefaults.h"
#include "ResourceManager.h"
#include "SpriteManager.h"

FO_BEGIN_NAMESPACE

static constexpr int32_t CACHE_INVALIDATION_FRAME_COUNT {3};
static constexpr int32_t TRUETYPE_MAX_PIXEL_SIZE {256};
static constexpr int32_t TRUETYPE_MAX_OUTLINE_WIDTH {8};
static constexpr int32_t TRUETYPE_MAX_PRELOAD_LETTERS {0x10000};
static constexpr int32_t TRUETYPE_SHEET_MIN_SIDE {64};
static constexpr int32_t TRUETYPE_SHEET_MAX_SIDE {2048};
// Transparent pixels between glyph cells, so linear atlas filtering never samples a neighbour glyph
static constexpr int32_t TRUETYPE_CELL_GAP {1};
// The shader doubles the vertex color, so mid grey is the neutral glyph color
static constexpr uint8_t TRUETYPE_GLYPH_GREY {128};

FontManager::FontManager(ptr<SpriteManager> spr_mngr, ptr<ResourceManager> res_mngr) :
    _sprMngr {spr_mngr},
    _resMngr {res_mngr}
{
}

auto FontManager::GetFont(FontType font) -> ptr<FontData>
{
    if (static_cast<int64_t>(font) < 0) {
        throw FontManagerException("Invalid font type", static_cast<int32_t>(font));
    }
    if (static_cast<size_t>(font) >= _allFonts.size()) {
        throw FontManagerException("Font type index out of range", static_cast<int32_t>(font));
    }
    if (!_allFonts[static_cast<size_t>(font)]) {
        throw FontManagerException("Font not loaded", static_cast<int32_t>(font));
    }

    return &*_allFonts[static_cast<size_t>(font)];
}

auto FontManager::GetFont(FontType font) const -> ptr<const FontData>
{
    if (static_cast<int64_t>(font) < 0) {
        throw FontManagerException("Invalid font type", static_cast<int32_t>(font));
    }
    if (static_cast<size_t>(font) >= _allFonts.size()) {
        throw FontManagerException("Font type index out of range", static_cast<int32_t>(font));
    }
    if (!_allFonts[static_cast<size_t>(font)]) {
        throw FontManagerException("Font not loaded", static_cast<int32_t>(font));
    }

    return &*_allFonts[static_cast<size_t>(font)];
}

void FontManager::ClearFonts()
{
    _allFonts.clear();
    _fontFiles.clear();
    _formatCache.clear();
}

void FontManager::SetFontEffect(FontType font, nptr<RenderEffect> effect)
{
    GetFont(font)->DrawEffect = effect ? effect : _sprMngr->_effectMngr->Effects.Font;
}

void FontManager::StoreFont(int32_t index, FontData&& font_data)
{
    _formatCache.clear();

    if (index >= numeric_cast<int32_t>(_allFonts.size())) {
        _allFonts.resize(index + 1);
    }

    // A TrueType sheet is built in memory and uploaded already finished
    _allFonts[index].emplace(std::move(font_data));
    _formatCache.clear();
}

void FontManager::BindDefaultFont(bool skip_if_loaded)
{
    FO_TRACE_ZONE(Gui);

    int32_t index {static_cast<int32_t>(FontType::Default)};

    if (skip_if_loaded && index < numeric_cast<int32_t>(_allFonts.size()) && _allFonts[index]) {
        return;
    }

    FontOptions options {};
    options.Hinting = FontHinting::Normal;
    options.OutlineWidth = 1;

    BindTrueTypeFont(FontType::Default, {string {FontDefaults::Default}}, FontDefaults::DefaultPixelSize, options, {32, 126, 160, 255, 1024, 1279}, AtlasType::IfaceSprites);
}

void FontManager::FrameUpdate()
{
    FO_TRACE_ZONE(Gui);

    for (auto it {_formatCache.begin()}; it != _formatCache.end();) {
        if (_frameIndex - it->second->LastUsedFrame >= CACHE_INVALIDATION_FRAME_COUNT) {
            it = _formatCache.erase(it);
        }
        else {
            ++it;
        }
    }

    ++_frameIndex;
}

void FontManager::BindTrueTypeFont(FontType font, const vector<string>& face_paths, int32_t pixel_size, FontOptions options, const vector<int32_t>& preload_ranges, AtlasType atlas_type)
{
    FO_TRACE_ZONE(Gui);

    int32_t index = static_cast<int32_t>(font);
    FO_VERIFY_AND_THROW(index >= 0, "Font index must not be negative", index);
    FO_VERIFY_AND_THROW(!face_paths.empty(), "TrueType font needs at least one face", index);
    FO_VERIFY_AND_THROW(pixel_size > 0 && pixel_size <= TRUETYPE_MAX_PIXEL_SIZE, "TrueType font pixel size is out of range", pixel_size);
    FO_VERIFY_AND_THROW(options.OutlineWidth >= 0 && options.OutlineWidth <= TRUETYPE_MAX_OUTLINE_WIDTH, "TrueType font outline width is out of range", options.OutlineWidth);
    FO_VERIFY_AND_THROW(preload_ranges.size() % 2 == 0, "Preload ranges must come in first and last codepoint pairs", preload_ranges.size());

    FontFaceHinting hinting {};

    switch (options.Hinting) {
    case FontHinting::Light:
        hinting = FontFaceHinting::Light;
        break;
    case FontHinting::Normal:
        hinting = FontFaceHinting::Normal;
        break;
    case FontHinting::Unhinted:
        hinting = FontFaceHinting::None;
        break;
    default:
        throw FontManagerException("Unknown TrueType font hinting", static_cast<int32_t>(options.Hinting));
    }

    // Collect the preload letters before any face is opened, so a malformed range fails cheaply
    vector<uint32_t> preload_letters {uint32_t {' '}};

    for (size_t i = 0; i < preload_ranges.size(); i += 2) {
        int32_t first = preload_ranges[i];
        int32_t last = preload_ranges[i + 1];
        FO_VERIFY_AND_THROW(first > 0 && first <= last && last <= 0x10FFFF, "Preload range must be an ordered pair of Unicode codepoints", first, last);
        FO_VERIFY_AND_THROW(numeric_cast<int32_t>(preload_letters.size()) + (last - first) < TRUETYPE_MAX_PRELOAD_LETTERS, "Preload ranges cover too many letters", preload_ranges.size());

        for (int32_t letter = first; letter <= last; letter++) {
            preload_letters.emplace_back(numeric_cast<uint32_t>(letter));
        }
    }

    if (!_rasterizer) {
        _rasterizer = safe_alloc::make_unique<FontRasterizer>();
    }

    auto sheet = safe_alloc::make_unique<FontData::TrueTypeSheet>();
    sheet->Options = options;
    sheet->Atlas = atlas_type;
    sheet->CellPadding = std::max(options.OutlineWidth, 1);

    for (const string& face_path : face_paths) {
        sheet->Faces.emplace_back(_rasterizer->OpenFace(ReadFontFile(face_path), pixel_size, hinting, options.Monochrome, options.ForceAutoHint, options.Embolden));
    }

    FontFaceMetrics metrics = sheet->Faces.front()->GetMetrics();
    sheet->Ascender = metrics.Ascender;

    // Room for about four rows of sixteen letters; the sheet doubles on demand
    int32_t cell_side = pixel_size + sheet->CellPadding * 2 + TRUETYPE_CELL_GAP;
    int32_t sheet_width = std::clamp(numeric_cast<int32_t>(std::bit_ceil(numeric_cast<uint32_t>(cell_side * 16))), TRUETYPE_SHEET_MIN_SIDE, TRUETYPE_SHEET_MAX_SIDE);
    int32_t sheet_height = std::clamp(numeric_cast<int32_t>(std::bit_ceil(numeric_cast<uint32_t>(cell_side * 4))), TRUETYPE_SHEET_MIN_SIDE, sheet_width);
    sheet->Size = {sheet_width, sheet_height};
    sheet->Pixels.assign(numeric_cast<size_t>(sheet_width) * numeric_cast<size_t>(sheet_height), ucolor::clear);

    if (options.OutlineWidth != 0) {
        sheet->BorderedPixels.assign(sheet->Pixels.size(), ucolor::clear);
    }

    FontData font_data;
    font_data.DrawEffect = _sprMngr->_effectMngr->Effects.Font;
    font_data.LineHeight = metrics.Ascender + metrics.Descender;
    font_data.YAdvance = metrics.LineGap + options.LineSpacing;
    FO_VERIFY_AND_THROW(font_data.LineHeight > 0 && font_data.LineHeight + font_data.YAdvance > 0, "TrueType font line step must be positive", font_data.LineHeight, font_data.YAdvance);
    font_data.TrueType = std::move(sheet);

    AddTrueTypeLetters(font_data, preload_letters, false);

    auto space_it = font_data.Letters.find(uint32_t {' '});
    FO_VERIFY_AND_THROW(space_it != font_data.Letters.end(), "No face of a TrueType font has the space letter", face_paths.front());
    font_data.SpaceWidth = space_it->second.XAdvance;

    StoreFont(index, std::move(font_data));
}

void FontManager::FormatText(FontFormatInfo& fi, FormatMode mode) const
{
    FO_TRACE_ZONE(Gui);

    string& str {fi.Text};
    auto flags {fi.Format.Flags};
    auto font {fi.CurFont};
    FO_VERIFY_AND_THROW(font, "Font is null");
    irect32 r {fi.Rect};
    bool infinity_w {r.width == 0};
    bool infinity_h {r.height == 0};
    int32_t curx {0};
    int32_t cury {0};
    int32_t& color_offset {fi.ColorOffset};

    // Colorize: strip `@color:...@` markers and write parsed colors into TextColor
    nptr<ucolor> dots {};
    string buf {};
    buf.reserve(str.size());

    if (mode == FormatMode::Draw && !is_enum_set(flags, FontFlag::NoColorize)) {
        dots = fi.TextColor.data();
    }

    constexpr int32_t dots_history_len {10};
    ucolor dots_history[dots_history_len] {fi.Color};
    int32_t dots_history_cur {0};

    size_t pos {0};

    while (pos < str.size()) {
        size_t marker_pos {str.find(InlineColorTagPrefix, pos)};

        if (marker_pos == string::npos) {
            buf.append(str, pos, string::npos);
            break;
        }

        buf.append(str, pos, marker_pos - pos);

        size_t tag_end {0};
        ucolor d {};
        bool reset {false};

        if (!ParseInlineColorTag(str, marker_pos, tag_end, d, reset)) {
            buf.append(str, marker_pos, 1);
            pos = marker_pos + 1;
            continue;
        }

        if (dots) {
            if (reset) {
                d = (dots_history_cur > 0) ? dots_history[--dots_history_cur] : fi.Color;
            }
            else {
                if (dots_history_cur < dots_history_len - 1) {
                    dots_history[++dots_history_cur] = d;
                }
            }

            dots[numeric_cast<int32_t>(buf.size())] = d;
        }

        pos = tag_end;
    }

    str = std::move(buf);

    // Single SkipLines counter — its meaning flips between "from top" and "from bottom" based on FontFlag::AlignBottom
    bool skip_from_bottom {is_enum_set(flags, FontFlag::AlignBottom)};
    int32_t skip_line {skip_from_bottom ? 0 : fi.Format.SkipLines};
    int32_t skip_line_end {skip_from_bottom ? fi.Format.SkipLines : 0};

    // Format
    curx = r.x;
    cury = r.y;

    // Kerning applies only between two letters standing directly next to each other, the same in every pass below
    nptr<const FontData::Letter> prev_letter;

    for (int32_t i {0}, i_advance {1}; i < numeric_cast<int32_t>(str.size()); i += i_advance) {
        size_t letter_len {utf8::decode_str_nt_len(&str[numeric_cast<size_t>(i)])};
        uint32_t letter {utf8::decode(&str[numeric_cast<size_t>(i)], letter_len)};
        letter = utf8::is_valid(letter) ? letter : 0;
        i_advance = numeric_cast<int32_t>(letter_len);

        int32_t x_advance {0};
        nptr<const FontData::Letter> cur_letter;

        switch (letter) {
        case '\r':
            prev_letter = nullptr;
            continue;
        case ' ':
            x_advance = font->SpaceWidth;
            break;
        case '\t':
            x_advance = font->SpaceWidth * 4;
            break;
        default:
            auto it {font->Letters.find(letter)};
            if (it != font->Letters.end()) {
                cur_letter = &it->second;
                x_advance = it->second.XAdvance + GetKerning(*font, prev_letter, it->second);
            }
            else {
                x_advance = 0;
            }

            break;
        }

        if (!infinity_w && curx + x_advance > r.x + r.width) {
            fi.MaxCurX = std::max(curx, fi.MaxCurX);

            if (mode == FormatMode::Draw && is_enum_set(flags, FontFlag::NoWrap)) {
                str.resize(numeric_cast<size_t>(i));
                break;
            }

            if (is_enum_set(flags, FontFlag::TruncateLine)) {
                int32_t j {i};

                while (j < numeric_cast<int32_t>(str.size()) && str[j] != '\n') {
                    j++;
                }

                int32_t erased {j - i};
                str.erase(numeric_cast<size_t>(i), numeric_cast<size_t>(erased));
                letter = i < numeric_cast<int32_t>(str.size()) ? numeric_cast<uint8_t>(str[i]) : 0;
                i_advance = 1;

                if (mode == FormatMode::Draw) {
                    int32_t text_color_end {numeric_cast<int32_t>(fi.TextColor.size()) - erased};

                    for (int32_t k {i}; k < text_color_end; k++) {
                        fi.TextColor[k] = fi.TextColor[k + erased];
                    }
                }
            }
            else if (str[i] != '\n' && curx != r.x) {
                int32_t j {i};

                for (; j >= 0; j--) {
                    if (str[j] == ' ' || str[j] == '\t') {
                        str[j] = '\n';
                        i = j;
                        letter = '\n';
                        i_advance = 1;
                        break;
                    }
                    if (str[j] == '\n') {
                        j = -1;
                        break;
                    }
                }

                if (j < 0) {
                    letter = '\n';
                    i_advance = 1;
                    str.insert(numeric_cast<size_t>(i), 1, '\n');

                    if (mode == FormatMode::Draw) {
                        for (int32_t k {numeric_cast<int32_t>(fi.TextColor.size()) - 1}; k > i; k--) {
                            fi.TextColor[k] = fi.TextColor[k - 1];
                        }
                    }
                }

                if (is_enum_set(flags, FontFlag::Justify) && skip_line == 0) {
                    fi.LineSpaceWidth[fi.LinesAll - 1] = 1;

                    // Erase next first spaces
                    int32_t ii {i + i_advance};
                    j = ii;

                    while (j < numeric_cast<int32_t>(str.size()) && str[j] == ' ') {
                        j++;
                    }

                    if (j > ii) {
                        int32_t erased {j - ii};
                        str.erase(numeric_cast<size_t>(ii), numeric_cast<size_t>(erased));

                        if (mode == FormatMode::Draw) {
                            int32_t text_color_end {numeric_cast<int32_t>(fi.TextColor.size()) - erased};

                            for (int32_t k {ii}; k < text_color_end; k++) {
                                fi.TextColor[k] = fi.TextColor[k + erased];
                            }
                        }
                    }
                }
            }
        }

        switch (letter) {
        case '\n':
            prev_letter = nullptr;

            if (skip_line == 0) {
                cury += font->LineHeight + font->YAdvance;

                if (!infinity_h && cury + font->LineHeight > r.y + r.height && fi.LinesInRect == 0) {
                    fi.LinesInRect = fi.LinesAll;
                }

                if (mode == FormatMode::Draw) {
                    if (fi.LinesInRect != 0 && !is_enum_set(flags, FontFlag::KeepTail)) {
                        str.resize(numeric_cast<size_t>(i));
                        break;
                    }
                }
                else if (mode == FormatMode::Split) {
                    if (fi.LinesInRect != 0 && fi.LinesAll % fi.LinesInRect == 0) {
                        fi.Lines.emplace_back(str.substr(0, numeric_cast<size_t>(i)));
                        str.erase(0, numeric_cast<size_t>(i + i_advance));
                        i = -i_advance;
                    }
                }

                if (i + i_advance < numeric_cast<int32_t>(str.size())) {
                    fi.LinesAll++;
                }
            }
            else {
                skip_line--;
                str.erase(0, numeric_cast<size_t>(i + i_advance));
                color_offset += i + i_advance;
                i = 0;
                i_advance = 0;
            }

            fi.MaxCurX = std::max(curx, fi.MaxCurX);
            curx = r.x;
            continue;
        case 0:
            break;
        default:
            curx += x_advance;
            prev_letter = cur_letter;
            continue;
        }

        if (i >= numeric_cast<int32_t>(str.size())) {
            break;
        }
    }

    fi.MaxCurX = std::max(curx, fi.MaxCurX);

    if (skip_line_end != 0) {
        int32_t cut_pos {numeric_cast<int32_t>(str.size())};

        for (int32_t i {cut_pos - 2}; i >= 0; i--) {
            if (str[i] == '\n') {
                cut_pos = i;
                fi.LinesAll--;

                if (--skip_line_end == 0) {
                    break;
                }
            }
        }

        str.resize(numeric_cast<size_t>(cut_pos));
        FO_VERIFY_AND_THROW(skip_line_end == 0, "Text layout ended before all requested trailing lines were skipped", skip_line_end, fi.LinesAll, str.size());
    }

    FO_VERIFY_AND_THROW(skip_line == 0, "Text layout ended before all requested leading lines were skipped", skip_line, fi.LinesAll, str.size());

    if (fi.LinesInRect == 0) {
        fi.LinesInRect = fi.LinesAll;
    }

    if (mode == FormatMode::Split) {
        fi.Lines.emplace_back(str);
        return;
    }
    if (mode == FormatMode::LineCount) {
        return;
    }

    // Up text
    if (is_enum_set(flags, FontFlag::KeepTail) && fi.LinesAll > fi.LinesInRect) {
        int32_t j {0};
        int32_t line_cur {0};
        ucolor last_color {};
        int32_t str_size {numeric_cast<int32_t>(str.size())};

        for (; j < str_size; ++j) {
            if (str[j] == '\n') {
                line_cur++;

                if (line_cur >= fi.LinesAll - fi.LinesInRect) {
                    break;
                }
            }

            if (fi.TextColor[j] != ucolor::clear) {
                last_color = fi.TextColor[j];
            }
        }

        if (!is_enum_set(flags, FontFlag::NoColorize)) {
            color_offset += j + 1;

            if (last_color != ucolor::clear && fi.TextColor[j + 1] == ucolor::clear) {
                fi.TextColor[j + 1] = last_color;
            }
        }

        str.erase(0, numeric_cast<size_t>(j + 1));
        fi.LinesAll = fi.LinesInRect;
    }

    // Align
    curx = r.x;
    cury = r.y;

    for (int32_t i {0}; i < fi.LinesAll; i++) {
        fi.LineWidth[i] = curx;
    }

    bool can_count {false};
    int32_t spaces {0};
    int32_t curstr {0};
    prev_letter = nullptr;

    for (int32_t i {0}, i_advance {1};; i += i_advance) {
        if (i >= numeric_cast<int32_t>(str.size())) {
            fi.LineWidth[curstr] = curx;
            cury += font->LineHeight + font->YAdvance;
            curx = r.x;

            if (fi.LineSpaceWidth[curstr] == 1 && spaces > 0 && !infinity_w) {
                fi.LineSpaceWidth[curstr] = font->SpaceWidth + ((r.x + r.width) - fi.LineWidth[curstr]) / spaces;
            }
            else {
                fi.LineSpaceWidth[curstr] = font->SpaceWidth;
            }

            break;
        }

        size_t letter_len {utf8::decode_str_nt_len(&str[numeric_cast<size_t>(i)])};
        uint32_t letter {utf8::decode(&str[numeric_cast<size_t>(i)], letter_len)};
        letter = utf8::is_valid(letter) ? letter : 0;
        i_advance = numeric_cast<int32_t>(letter_len);

        if (letter == ' ' || letter == '\t' || letter == '\n' || letter == '\r' || letter == 0) {
            prev_letter = nullptr;
        }

        switch (letter) {
        case ' ':
            curx += font->SpaceWidth;

            if (can_count) {
                spaces++;
            }

            break;
        case '\t':
            curx += font->SpaceWidth * 4;
            break;
        case 0:
        case '\n':
            fi.LineWidth[curstr] = curx;
            cury += font->LineHeight + font->YAdvance;
            curx = r.x;

            // Align
            if (fi.LineSpaceWidth[curstr] == 1 && spaces > 0 && !infinity_w) {
                fi.LineSpaceWidth[curstr] = font->SpaceWidth + ((r.x + r.width) - fi.LineWidth[curstr]) / spaces;
            }
            else {
                fi.LineSpaceWidth[curstr] = font->SpaceWidth;
            }

            curstr++;
            can_count = false;
            spaces = 0;
            break;
        case '\r':
            break;
        default:
            if (auto it {font->Letters.find(letter)}; it != font->Letters.end()) {
                curx += it->second.XAdvance + GetKerning(*font, prev_letter, it->second);
                prev_letter = &it->second;
            }
            else {
                prev_letter = nullptr;
            }

            can_count = true;
            break;
        }
    }

    // Initial position
    fi.CurX = r.x;
    fi.CurY = r.y;

    // Align X
    if (is_enum_set(flags, FontFlag::CenterX)) {
        fi.CurX += ((r.x + r.width) - fi.LineWidth[0]) / 2;
    }
    else if (is_enum_set(flags, FontFlag::AlignRight)) {
        fi.CurX += (r.x + r.width) - fi.LineWidth[0];
    }

    // Align Y
    if (is_enum_set(flags, FontFlag::CenterY)) {
        fi.CurY = r.y + (r.height + 1 - fi.LinesInRect * font->LineHeight - (fi.LinesInRect - 1) * font->YAdvance) / 2;
    }
    else if (is_enum_set(flags, FontFlag::AlignBottom)) {
        fi.CurY = (r.y + r.height) - (fi.LinesInRect * font->LineHeight + (fi.LinesInRect - 1) * font->YAdvance);
    }
}

auto FontManager::IsInlineColorHex(string_view value) -> bool
{
    if (value.size() >= 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
        value.remove_prefix(2);
    }

    if (value.size() != 6 && value.size() != 8) {
        return false;
    }

    return std::ranges::all_of(value, [](char ch) { return std::isxdigit(static_cast<unsigned char>(ch)) != 0; });
}

auto FontManager::ParseInlineColorTag(string_view str, size_t marker_pos, size_t& tag_end, ucolor& color, bool& reset) -> bool
{
    if (marker_pos + InlineColorTagPrefix.size() >= str.size()) {
        return false;
    }
    if (str.compare(marker_pos, InlineColorTagPrefix.size(), InlineColorTagPrefix) != 0) {
        return false;
    }

    size_t close_pos {str.find('@', marker_pos + InlineColorTagPrefix.size())};

    if (close_pos == string_view::npos) {
        return false;
    }

    string_view value {str.substr(marker_pos + InlineColorTagPrefix.size(), close_pos - marker_pos - InlineColorTagPrefix.size())};

    if (value.empty()) {
        tag_end = close_pos + 1;
        reset = true;
        return true;
    }
    if (value.front() != ':') {
        return false;
    }

    value.remove_prefix(1);

    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
        value.remove_suffix(1);
    }
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }

    if (!IsInlineColorHex(value)) {
        return false;
    }

    if (value.size() >= 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
        value.remove_prefix(2);
    }

    string value_str {value};
    color = ucolor {numeric_cast<uint32_t>(std::strtoul(value_str.c_str(), nullptr, 16))};
    tag_end = close_pos + 1;
    reset = false;
    return true;
}

auto FontManager::GetOrFormat(TextFormat format, FontType font, irect32 rect, ucolor color, FormatMode mode, string_view str) -> ptr<const FontFormatInfo>
{
    FO_VERIFY_AND_THROW(static_cast<size_t>(font) < _allFonts.size(), "Text formatting requested a font index outside the loaded font table", font, _allFonts.size(), str.size(), rect);
    FO_VERIFY_AND_THROW(format.SkipLines >= 0, "Text format skip-line count must not be negative");
    FO_VERIFY_AND_THROW(rect.width >= 0, "Text layout rectangle width must not be negative", rect.width);
    FO_VERIFY_AND_THROW(rect.height >= 0, "Text layout rectangle height must not be negative", rect.height);

    std::array<uint64_t, 8> key_parts {
        hash_storage::default_hash(make_const_span(str)),
        static_cast<uint32_t>(font),
        static_cast<uint32_t>(format.Flags),
        static_cast<uint32_t>(format.SkipLines),
        static_cast<uint32_t>(rect.width),
        static_cast<uint32_t>(rect.height),
        color.underlying_value(),
        static_cast<uint32_t>(mode),
    };

    uint64_t key {hash_storage::default_hash(make_span(key_parts))};

    if (auto it {_formatCache.find(key)}; it != _formatCache.end()) {
        it->second->LastUsedFrame = _frameIndex;
        return it->second;
    }

    auto str_len {str.length()};
    auto max_chars {std::max<size_t>(str_len * 2 + 1, 1)};
    auto max_lines {std::max<size_t>(str_len + 1, 1)};

    FO_VERIFY_AND_THROW(_allFonts[static_cast<size_t>(font)], "Requested font is not loaded");
    FontData& font_data {*_allFonts[static_cast<size_t>(font)]};
    EnsureLetters(font_data, str);

    auto fi {safe_alloc::make_unique<FontFormatInfo>()};
    fi->CurFont = &font_data;
    fi->Format = format;
    fi->Rect = irect32 {0, 0, rect.width, rect.height};
    fi->Color = color;
    fi->Text.assign(str);
    fi->TextColor.assign(max_chars, ucolor::clear);
    fi->LineWidth.assign(max_lines, 0);
    fi->LineSpaceWidth.assign(max_lines, 0);
    fi->LastUsedFrame = _frameIndex;

    FormatText(*fi, mode);

    auto fi_ptr {fi.as_ptr()};
    _formatCache.emplace(key, std::move(fi));
    return fi_ptr;
}

void FontManager::DrawText(irect32 rect, string_view str, ucolor color, TextFormat format)
{
    if (str.empty()) {
        return;
    }

    auto font {GetFont(format.Font)};
    color = _sprMngr->ApplyColorBrightness(color);
    auto fi {GetOrFormat(format, format.Font, rect, color, FormatMode::Draw, str)};

    auto flags {format.Flags};
    const string& format_str {fi->Text};
    int32_t color_offset {fi->ColorOffset};
    int32_t curx {rect.x + fi->CurX};
    int32_t cury {rect.y + fi->CurY};
    int32_t curstr {0};
    const auto& texture {is_enum_set(flags, FontFlag::Bordered) && font->FontTexBordered ? font->FontTexBordered : font->FontTex};

    if (!is_enum_set(flags, FontFlag::NoColorize)) {
        for (int32_t i {color_offset}; i >= 0; i--) {
            if (fi->TextColor[i] != ucolor::clear) {
                if (fi->TextColor[i].comp.a != 0) {
                    color = fi->TextColor[i]; // With alpha
                }
                else {
                    color = ucolor {fi->TextColor[i], color.comp.a};
                }

                color = _sprMngr->ApplyColorBrightness(color);
                break;
            }
        }
    }

    size_t str_len {format_str.size()};
    _sprMngr->_spritesDrawBuf->CheckAllocBuf(str_len * 4, str_len * 6);

    auto& vbuf {_sprMngr->_spritesDrawBuf->Vertices};
    size_t& vpos {_sprMngr->_spritesDrawBuf->VertCount};
    auto& ibuf {_sprMngr->_spritesDrawBuf->Indices};
    size_t& ipos {_sprMngr->_spritesDrawBuf->IndCount};

    size_t start_ipos {ipos};

    bool variable_space {false};
    nptr<const FontData::Letter> prev_letter;
    int32_t i_advance {};

    for (int32_t i {0}; i < numeric_cast<int32_t>(format_str.size()); i += i_advance) {
        if (!is_enum_set(flags, FontFlag::NoColorize)) {
            auto new_color {fi->TextColor[i + color_offset]};

            if (new_color != ucolor::clear) {
                if (new_color.comp.a != 0) {
                    color = new_color; // With alpha
                }
                else {
                    color = ucolor {new_color, color.comp.a};
                }

                color = _sprMngr->ApplyColorBrightness(color);
            }
        }

        size_t letter_len {utf8::decode_str_nt_len(&format_str[numeric_cast<size_t>(i)])};
        uint32_t letter {utf8::decode(&format_str[numeric_cast<size_t>(i)], letter_len)};
        letter = utf8::is_valid(letter) ? letter : 0;
        i_advance = numeric_cast<int32_t>(letter_len);

        switch (letter) {
        case ' ':
            curx += variable_space ? fi->LineSpaceWidth[curstr] : font->SpaceWidth;
            prev_letter = nullptr;
            continue;
        case '\t':
            curx += font->SpaceWidth * 4;
            prev_letter = nullptr;
            continue;
        case '\n':
            prev_letter = nullptr;
            cury += font->LineHeight + font->YAdvance;
            curx = rect.x;
            curstr++;
            variable_space = false;

            if (is_enum_set(flags, FontFlag::CenterX)) {
                curx += (rect.width - fi->LineWidth[curstr]) / 2;
            }
            else if (is_enum_set(flags, FontFlag::AlignRight)) {
                curx += rect.width - fi->LineWidth[curstr];
            }

            continue;
        case '\r':
            prev_letter = nullptr;
            continue;
        default:
            auto it {font->Letters.find(letter)};

            if (it == font->Letters.end()) {
                prev_letter = nullptr;
                continue;
            }

            const auto& l {it->second};
            curx += GetKerning(*font, prev_letter, l);
            prev_letter = &l;

            float32_t x {numeric_cast<float32_t>(curx - l.Offset.x - 1)};
            float32_t y {numeric_cast<float32_t>(cury - l.Offset.y - 1)};
            float32_t w {numeric_cast<float32_t>(l.Size.width + 2)};
            float32_t h {numeric_cast<float32_t>(l.Size.height + 2)};

            const frect32& texture_uv {is_enum_set(flags, FontFlag::Bordered) ? l.TexBorderedPos : l.TexPos};
            float32_t x1 {texture_uv.x};
            float32_t y1 {texture_uv.y};
            float32_t x2 {texture_uv.x + texture_uv.width};
            float32_t y2 {texture_uv.y + texture_uv.height};

            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 0);
            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 1);
            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 3);
            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 1);
            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 2);
            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 3);

            auto& v0 {vbuf[vpos++]};
            v0.PosX = x;
            v0.PosY = y + h;
            v0.PosZ = 0.0f;
            v0.TexU = x1;
            v0.TexV = y2;
            v0.EggFlags[0] = 0.0f;
            v0.EggFlags[1] = 0.0f;
            v0.Color = color;

            auto& v1 {vbuf[vpos++]};
            v1.PosX = x;
            v1.PosY = y;
            v1.PosZ = 0.0f;
            v1.TexU = x1;
            v1.TexV = y1;
            v1.EggFlags[0] = 0.0f;
            v1.EggFlags[1] = 0.0f;
            v1.Color = color;

            auto& v2 {vbuf[vpos++]};
            v2.PosX = x + w;
            v2.PosY = y;
            v2.PosZ = 0.0f;
            v2.TexU = x2;
            v2.TexV = y1;
            v2.EggFlags[0] = 0.0f;
            v2.EggFlags[1] = 0.0f;
            v2.Color = color;

            auto& v3 {vbuf[vpos++]};
            v3.PosX = x + w;
            v3.PosY = y + h;
            v3.PosZ = 0.0f;
            v3.TexU = x2;
            v3.TexV = y2;
            v3.EggFlags[0] = 0.0f;
            v3.EggFlags[1] = 0.0f;
            v3.Color = color;

            curx += l.XAdvance;
            variable_space = true;
        }
    }

    size_t ind_count {ipos - start_ipos};

    if (ind_count != 0) {
        if (_sprMngr->_dipQueue.empty() || _sprMngr->_dipQueue.back().MainTexture != texture || _sprMngr->_dipQueue.back().SourceEffect != font->DrawEffect) {
            _sprMngr->_dipQueue.emplace_back(DipData {.MainTexture = texture, .SourceEffect = font->DrawEffect, .IndicesCount = ind_count});
        }
        else {
            _sprMngr->_dipQueue.back().IndicesCount += ind_count;
        }

        if (_sprMngr->_spritesDrawBuf->VertCount >= _sprMngr->_flushVertCount) {
            _sprMngr->Flush();
        }
    }
}

auto FontManager::GetLinesCount(isize32 size, string_view str, FontType num_font) -> int32_t
{
    if (size.width <= 0 || size.height <= 0) {
        return 0;
    }

    auto font {GetFont(num_font)};

    if (str.empty()) {
        return size.height / (font->LineHeight + font->YAdvance);
    }

    TextFormat format {};
    format.Font = num_font;
    auto fi {GetOrFormat(format, num_font, irect32 {0, 0, size.width, size.height}, ucolor {}, FormatMode::LineCount, str)};
    return fi->LinesInRect;
}

auto FontManager::GetLinesHeight(isize32 size, string_view str, FontType num_font) -> int32_t
{
    if (size.width <= 0 || size.height <= 0) {
        return 0;
    }

    auto font {GetFont(num_font)};
    int32_t lines_count {GetLinesCount(size, str, num_font)};

    if (lines_count <= 0) {
        return 0;
    }

    return lines_count * font->LineHeight + (lines_count - 1) * font->YAdvance;
}

auto FontManager::GetLineHeight(FontType num_font) const -> int32_t
{
    return GetFont(num_font)->LineHeight;
}

auto FontManager::GetTextInfo(isize32 size, string_view str, TextFormat format, isize32& result_size, int32_t& lines) -> bool
{
    result_size = {};
    lines = {};

    auto font {GetFont(format.Font)};

    if (str.empty()) {
        return true;
    }

    irect32 rect {irect32 {0, 0, size}};
    auto fi {GetOrFormat(format, format.Font, rect, ucolor {}, FormatMode::LineCount, str)};

    result_size = {fi->MaxCurX, fi->LinesInRect * font->LineHeight + (fi->LinesInRect - 1) * font->YAdvance};
    lines = fi->LinesInRect;
    return true;
}

auto FontManager::SplitLines(irect32 rect, string_view cstr, FontType num_font) -> vector<string>
{
    if (cstr.empty()) {
        return {};
    }

    TextFormat format {};
    format.Font = num_font;
    auto fi {GetOrFormat(format, num_font, rect, ucolor {}, FormatMode::Split, cstr)};
    return fi->Lines;
}

auto FontManager::HaveLetter(FontType num_font, uint32_t letter) const -> bool
{
    auto font = GetFont(num_font);

    // A TrueType font answers for every letter its faces carry, rasterized or not
    if (font->TrueType) {
        return std::ranges::any_of(font->TrueType->Faces, [letter](const unique_ptr<FontFace>& face) { return face->GetGlyphIndex(letter) != 0; });
    }

    return font->Letters.count(letter) != 0;
}

auto FontManager::GetKerning(const FontData& font, nptr<const FontData::Letter> left, const FontData::Letter& right) -> int32_t
{
    if (!left || !font.TrueType || !font.TrueType->Options.Kerning) {
        return 0;
    }
    if (left->FaceIndex < 0 || left->FaceIndex != right.FaceIndex) {
        return 0;
    }

    return font.TrueType->Faces[numeric_cast<size_t>(right.FaceIndex)]->GetKerning(left->GlyphIndex, right.GlyphIndex);
}

auto FontManager::ReadFontFile(string_view path) -> shared_ptr<const vector<uint8_t>>
{
    FO_TRACE_ZONE(Gui);

    if (auto it = _fontFiles.find(string(path)); it != _fontFiles.end()) {
        return it->second;
    }

    auto file {_resMngr->GetResources()->ReadFile(path)};

    if (!file) {
        throw FontManagerException("Font file not found", path);
    }

    shared_ptr<const vector<uint8_t>> data = safe_alloc::make_shared<const vector<uint8_t>>(file.GetData());
    _fontFiles.emplace(string(path), data);
    return data;
}

void FontManager::EnsureLetters(FontData& font, string_view str)
{
    if (!font.TrueType) {
        return;
    }

    vector<uint32_t> missing_letters;

    for (size_t i = 0; i < str.size();) {
        // The view need not be null-terminated, so the decoder is bounded by what is left of it
        size_t letter_len = str.size() - i;
        uint32_t letter = utf8::decode(&str[i], letter_len);
        i += letter_len;

        // Control characters are layout commands, never glyphs
        if (!utf8::is_valid(letter) || letter < 0x20 || (letter >= 0x7F && letter < 0xA0)) {
            continue;
        }
        if (font.Letters.contains(letter) || std::ranges::find(missing_letters, letter) != missing_letters.end()) {
            continue;
        }

        missing_letters.emplace_back(letter);
    }

    if (!missing_letters.empty()) {
        AddTrueTypeLetters(font, missing_letters, true);
    }
}

void FontManager::AddTrueTypeLetters(FontData& font, const vector<uint32_t>& letters, bool substitute_missing)
{
    FO_TRACE_ZONE(Gui);

    auto& sheet = *font.TrueType;
    vector<irect32> new_cells;
    vector<uint32_t> added_letters;
    bool had_missing_letter = sheet.MissingLetter.has_value();

    // A letter must never outlive a failed batch without its pixels in the atlas
    auto discard_on_fail = scope_fail([&font, &sheet, &added_letters, had_missing_letter]() noexcept {
        for (uint32_t letter : added_letters) {
            font.Letters.erase(letter);
        }

        if (!had_missing_letter) {
            sheet.MissingLetter.reset();
        }
    });

    for (uint32_t letter : letters) {
        if (font.Letters.contains(letter)) {
            continue;
        }

        auto face_it = std::ranges::find_if(sheet.Faces, [letter](const unique_ptr<FontFace>& face) { return face->GetGlyphIndex(letter) != 0; });

        if (face_it != sheet.Faces.end()) {
            int32_t face_index = numeric_cast<int32_t>(std::distance(sheet.Faces.begin(), face_it));
            FontData::Letter rasterized = RasterizeTrueTypeLetter(font, face_index, (*face_it)->GetGlyphIndex(letter), new_cells);
            font.Letters.emplace(letter, rasterized);
        }
        else if (substitute_missing) {
            // Every letter no face carries shares one cell with the first face's missing-glyph box
            if (!sheet.MissingLetter) {
                sheet.MissingLetter = RasterizeTrueTypeLetter(font, 0, 0, new_cells);
            }

            font.Letters.emplace(letter, *sheet.MissingLetter);
        }
        else {
            continue;
        }

        added_letters.emplace_back(letter);
    }

    if (added_letters.empty()) {
        return;
    }

    // A sheet that grew since its last upload, in this batch or in a failed one, goes up whole
    if (!font.ImageNormal || font.ImageNormal->GetSize() != sheet.Size) {
        UploadTrueTypeSheet(font);
        return;
    }

    for (irect32 cell : new_cells) {
        UploadTrueTypeCell(font, cell);
    }
    for (uint32_t letter : added_letters) {
        UpdateLetterTexPos(font, font.Letters[letter]);
    }
}

auto FontManager::RasterizeTrueTypeLetter(FontData& font, int32_t face_index, uint32_t glyph_index, vector<irect32>& new_cells) -> FontData::Letter
{
    auto& sheet = *font.TrueType;
    RasterizedGlyph glyph = sheet.Faces[numeric_cast<size_t>(face_index)]->RenderGlyph(glyph_index);

    int32_t padding = sheet.CellPadding;
    isize32 cell_size = {glyph.Size.width + padding * 2, glyph.Size.height + padding * 2};
    ipos32 cell_pos = ReserveSheetCell(sheet, cell_size);
    size_t sheet_width = numeric_cast<size_t>(sheet.Size.width);

    auto coverage_at = [&glyph, padding](int32_t cell_x, int32_t cell_y) -> int32_t {
        int32_t x = cell_x - padding;
        int32_t y = cell_y - padding;

        if (x < 0 || y < 0 || x >= glyph.Size.width || y >= glyph.Size.height) {
            return 0;
        }

        return glyph.Coverage[numeric_cast<size_t>(y) * numeric_cast<size_t>(glyph.Size.width) + numeric_cast<size_t>(x)];
    };

    int32_t outline = sheet.Options.OutlineWidth;

    for (int32_t cell_y = 0; cell_y < cell_size.height; cell_y++) {
        for (int32_t cell_x = 0; cell_x < cell_size.width; cell_x++) {
            size_t pixel_index = numeric_cast<size_t>(cell_pos.y + cell_y) * sheet_width + numeric_cast<size_t>(cell_pos.x + cell_x);
            int32_t coverage = coverage_at(cell_x, cell_y);

            // A colored glyph keeps its own colors, which the font shader multiplies by the text color just as it does
            // for the neutral grey of an ordinary glyph
            ucolor glyph_color {TRUETYPE_GLYPH_GREY, TRUETYPE_GLYPH_GREY, TRUETYPE_GLYPH_GREY, 255};
            int32_t glyph_x {cell_x - padding};
            int32_t glyph_y {cell_y - padding};

            if (!glyph.Colors.empty() && glyph_x >= 0 && glyph_y >= 0 && glyph_x < glyph.Size.width && glyph_y < glyph.Size.height) {
                glyph_color = glyph.Colors[numeric_cast<size_t>(glyph_y) * numeric_cast<size_t>(glyph.Size.width) + numeric_cast<size_t>(glyph_x)];
            }

            if (coverage != 0) {
                sheet.Pixels[pixel_index] = ucolor {glyph_color.comp.r, glyph_color.comp.g, glyph_color.comp.b, numeric_cast<uint8_t>(coverage)};
            }

            if (outline == 0) {
                continue;
            }

            // The outline is the glyph's coverage spread over a disc of the outline radius, laid black under the glyph
            int32_t spread = 0;

            for (int32_t dy = -outline; dy <= outline; dy++) {
                for (int32_t dx = -outline; dx <= outline; dx++) {
                    if (dx * dx + dy * dy <= outline * outline + outline) {
                        spread = std::max(spread, coverage_at(cell_x + dx, cell_y + dy));
                    }
                }
            }

            int32_t alpha = coverage + (spread * (255 - coverage) + 127) / 255;

            if (alpha != 0) {
                auto under_outline = [coverage, alpha](uint8_t channel) -> uint8_t { return numeric_cast<uint8_t>((channel * coverage + alpha / 2) / alpha); };
                sheet.BorderedPixels[pixel_index] = ucolor {under_outline(glyph_color.comp.r), under_outline(glyph_color.comp.g), under_outline(glyph_color.comp.b), numeric_cast<uint8_t>(alpha)};
            }
        }
    }

    new_cells.emplace_back(irect32 {cell_pos, cell_size});

    // The drawn quad is the letter rect grown by one pixel, which is exactly the cell
    FontData::Letter letter;
    letter.Pos = {cell_pos.x + 1, cell_pos.y + 1};
    letter.Size = {cell_size.width - 2, cell_size.height - 2};
    letter.Offset = {padding - 1 - glyph.Bearing.x, glyph.Bearing.y - sheet.Ascender + padding - 1};
    letter.XAdvance = glyph.Advance;
    letter.FaceIndex = face_index;
    letter.GlyphIndex = glyph_index;
    return letter;
}

auto FontManager::ReserveSheetCell(FontData::TrueTypeSheet& sheet, isize32 cell_size) -> ipos32
{
    while (true) {
        bool fits_shelf = sheet.ShelfPos.x + cell_size.width <= sheet.Size.width;

        if (!fits_shelf && sheet.ShelfPos.x != 0) {
            sheet.ShelfPos = {0, sheet.ShelfPos.y + sheet.ShelfHeight + TRUETYPE_CELL_GAP};
            sheet.ShelfHeight = 0;
            continue;
        }

        if (fits_shelf && sheet.ShelfPos.y + cell_size.height <= sheet.Size.height) {
            ipos32 cell_pos = sheet.ShelfPos;
            sheet.ShelfPos.x += cell_size.width + TRUETYPE_CELL_GAP;
            sheet.ShelfHeight = std::max(sheet.ShelfHeight, cell_size.height);
            return cell_pos;
        }

        // Cells keep their places: the grown sheet takes the old one as its top left corner
        isize32 new_size = sheet.Size.height < sheet.Size.width ? isize32 {sheet.Size.width, sheet.Size.height * 2} : isize32 {sheet.Size.width * 2, sheet.Size.height};

        if (new_size.width > TRUETYPE_SHEET_MAX_SIDE || new_size.height > TRUETYPE_SHEET_MAX_SIDE) {
            throw FontManagerException("TrueType glyph sheet is full", sheet.Size, cell_size);
        }

        auto grow = [&sheet, new_size](vector<ucolor>& pixels) {
            if (pixels.empty()) {
                return;
            }

            vector<ucolor> grown(numeric_cast<size_t>(new_size.width) * numeric_cast<size_t>(new_size.height), ucolor::clear);

            for (int32_t y = 0; y < sheet.Size.height; y++) {
                auto row = pixels.begin() + numeric_cast<ptrdiff_t>(y) * sheet.Size.width;
                std::copy_n(row, sheet.Size.width, grown.begin() + numeric_cast<ptrdiff_t>(y) * new_size.width);
            }

            pixels = std::move(grown);
        };

        grow(sheet.Pixels);
        grow(sheet.BorderedPixels);
        sheet.Size = new_size;
    }
}

void FontManager::UploadTrueTypeSheet(FontData& font)
{
    FO_TRACE_ZONE(Gui);

    auto& sheet = *font.TrueType;

    // The new sprites are created beside the old ones, so a failure leaves the font drawing from the old
    shared_ptr<AtlasSprite> image_normal = _resMngr->CreateQuadSprite(sheet.Atlas, sheet.Size, sheet.Pixels);
    shared_ptr<AtlasSprite> image_bordered;

    if (!sheet.BorderedPixels.empty()) {
        image_bordered = _resMngr->CreateQuadSprite(sheet.Atlas, sheet.Size, sheet.BorderedPixels);
    }

    // Text queued this frame still samples the old sprites, which are released below
    _sprMngr->Flush();

    font.ImageNormal = std::move(image_normal);
    font.FontTex = font.ImageNormal->GetAtlas()->GetTexture();

    if (image_bordered) {
        font.ImageBordered = std::move(image_bordered);
        font.FontTexBordered = font.ImageBordered->GetAtlas()->GetTexture();
    }

    for (auto& letter : font.Letters | std::views::values) {
        UpdateLetterTexPos(font, letter);
    }
}

void FontManager::UploadTrueTypeCell(FontData& font, irect32 cell)
{
    auto& sheet = *font.TrueType;

    auto upload = [&sheet, cell](shared_ptr<AtlasSprite>& sprite, const vector<ucolor>& pixels) {
        vector<ucolor> cell_pixels;
        cell_pixels.reserve(numeric_cast<size_t>(cell.width) * numeric_cast<size_t>(cell.height));

        for (int32_t y = 0; y < cell.height; y++) {
            auto row = pixels.begin() + numeric_cast<ptrdiff_t>(cell.y + y) * sheet.Size.width + cell.x;
            cell_pixels.insert(cell_pixels.end(), row, row + cell.width);
        }

        auto atlas = sprite->GetAtlas();
        int32_t sheet_x = iround<int32_t>(numeric_cast<float32_t>(atlas->GetSize().width) * sprite->GetAtlasRect().x);
        int32_t sheet_y = iround<int32_t>(numeric_cast<float32_t>(atlas->GetSize().height) * sprite->GetAtlasRect().y);
        atlas->GetTexture()->UpdateTextureRegion({sheet_x + cell.x, sheet_y + cell.y}, {cell.width, cell.height}, cell_pixels);
    };

    upload(font.ImageNormal, sheet.Pixels);

    if (font.ImageBordered) {
        upload(font.ImageBordered, sheet.BorderedPixels);
    }
}

void FontManager::UpdateLetterTexPos(const FontData& font, FontData::Letter& letter)
{
    auto tex_pos_in = [&letter](const shared_ptr<AtlasSprite>& sprite) -> frect32 {
        float32_t tex_w = numeric_cast<float32_t>(sprite->GetAtlas()->GetSize().width);
        float32_t tex_h = numeric_cast<float32_t>(sprite->GetAtlas()->GetSize().height);
        float32_t image_x = tex_w * sprite->GetAtlasRect().x;
        float32_t image_y = tex_h * sprite->GetAtlasRect().y;

        frect32 tex_pos;
        tex_pos.x = (image_x + numeric_cast<float32_t>(letter.Pos.x) - 1.0f) / tex_w;
        tex_pos.y = (image_y + numeric_cast<float32_t>(letter.Pos.y) - 1.0f) / tex_h;
        tex_pos.width = numeric_cast<float32_t>(letter.Size.width + 2) / tex_w;
        tex_pos.height = numeric_cast<float32_t>(letter.Size.height + 2) / tex_h;
        return tex_pos;
    };

    letter.TexPos = tex_pos_in(font.ImageNormal);
    letter.TexBorderedPos = font.ImageBordered ? tex_pos_in(font.ImageBordered) : letter.TexPos;
}

FO_END_NAMESPACE
