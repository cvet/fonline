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

#include "FontManager.h"
#include "DefaultSprites.h"
#include "SpriteManager.h"

FO_BEGIN_NAMESPACE

static constexpr int32_t CACHE_INVALIDATION_FRAME_COUNT = 3;

FontManager::FontManager(SpriteManager& spr_mngr) :
    _sprMngr {&spr_mngr}
{
    FO_STACK_TRACE_ENTRY();
}

auto FontManager::GetFont(FontType font) -> FontData*
{
    FO_STACK_TRACE_ENTRY();

    if (static_cast<int64_t>(font) < 0) {
        throw FontManagerException("Invalid font type", static_cast<int32_t>(font));
    }
    if (static_cast<size_t>(font) >= _allFonts.size()) {
        throw FontManagerException("Font type index out of range", static_cast<int32_t>(font));
    }
    if (!_allFonts[static_cast<size_t>(font)]) {
        throw FontManagerException("Font not loaded", static_cast<int32_t>(font));
    }

    return _allFonts[static_cast<size_t>(font)].get();
}

auto FontManager::GetFont(FontType font) const -> const FontData*
{
    FO_STACK_TRACE_ENTRY();

    if (static_cast<int64_t>(font) < 0) {
        throw FontManagerException("Invalid font type", static_cast<int32_t>(font));
    }
    if (static_cast<size_t>(font) >= _allFonts.size()) {
        throw FontManagerException("Font type index out of range", static_cast<int32_t>(font));
    }
    if (!_allFonts[static_cast<size_t>(font)]) {
        throw FontManagerException("Font not loaded", static_cast<int32_t>(font));
    }

    return _allFonts[static_cast<size_t>(font)].get();
}

void FontManager::ClearFonts()
{
    FO_STACK_TRACE_ENTRY();

    _allFonts.clear();
    _formatCache.clear();
}

void FontManager::SetFontEffect(FontType font, RenderEffect* effect)
{
    FO_STACK_TRACE_ENTRY();

    GetFont(font)->DrawEffect = effect != nullptr ? effect : _sprMngr->_effectMngr->Effects.Font;
}

void FontManager::FrameUpdate()
{
    FO_STACK_TRACE_ENTRY();

    for (auto it = _formatCache.begin(); it != _formatCache.end();) {
        if (_frameIndex - it->second->LastUsedFrame >= CACHE_INVALIDATION_FRAME_COUNT) {
            it = _formatCache.erase(it);
        }
        else {
            ++it;
        }
    }

    ++_frameIndex;
}

void FontManager::BuildFont(int32_t index)
{
    FO_STACK_TRACE_ENTRY();

    auto& font = *_allFonts[index];

    // Fix texture coordinates
    auto* atlas_spr = font.ImageNormal.get();
    auto tex_w = numeric_cast<float32_t>(atlas_spr->GetAtlas()->GetSize().width);
    auto tex_h = numeric_cast<float32_t>(atlas_spr->GetAtlas()->GetSize().height);
    auto image_x = tex_w * atlas_spr->GetAtlasRect().x;
    auto image_y = tex_h * atlas_spr->GetAtlasRect().y;
    auto max_h = 0;

    for (auto& letter : font.Letters | std::views::values) {
        const auto x = numeric_cast<float32_t>(letter.Pos.x);
        const auto y = numeric_cast<float32_t>(letter.Pos.y);
        const auto w = numeric_cast<float32_t>(letter.Size.width);
        const auto h = numeric_cast<float32_t>(letter.Size.height);

        letter.TexPos.x = (image_x + x - 1.0f) / tex_w;
        letter.TexPos.y = (image_y + y - 1.0f) / tex_h;
        letter.TexPos.width = (w + 2.0f) / tex_w;
        letter.TexPos.height = (h + 2.0f) / tex_h;

        max_h = std::max(letter.Size.height, max_h);
    }

    // Fill data
    font.FontTex = atlas_spr->GetAtlas()->GetTexture();

    if (font.LineHeight == 0) {
        font.LineHeight = max_h;
    }
    if (font.Letters.count(numeric_cast<uint32_t>(' ')) != 0) {
        font.SpaceWidth = font.Letters[numeric_cast<uint32_t>(' ')].XAdvance;
    }

    auto* si_bordered = dynamic_cast<AtlasSprite*>(font.ImageBordered ? font.ImageBordered.get() : nullptr);
    font.FontTexBordered = si_bordered != nullptr ? si_bordered->GetAtlas()->GetTexture() : nullptr;

    const auto normal_ox = iround<int32_t>(tex_w * atlas_spr->GetAtlasRect().x);
    const auto normal_oy = iround<int32_t>(tex_h * atlas_spr->GetAtlasRect().y);
    const auto bordered_ox = si_bordered != nullptr ? iround<int32_t>(numeric_cast<float32_t>(si_bordered->GetAtlas()->GetSize().width) * si_bordered->GetAtlasRect().x) : 0;
    const auto bordered_oy = si_bordered != nullptr ? iround<int32_t>(numeric_cast<float32_t>(si_bordered->GetAtlas()->GetSize().height) * si_bordered->GetAtlasRect().y) : 0;

    // Read texture data
    const auto pixel_at = [](vector<ucolor>& tex_data, int32_t width, int32_t x, int32_t y) -> ucolor& { return tex_data[y * width + x]; };
    vector<ucolor> data_normal = atlas_spr->GetAtlas()->GetTexture()->GetTextureRegion({normal_ox, normal_oy}, atlas_spr->GetSize());
    vector<ucolor> data_bordered;

    if (si_bordered != nullptr) {
        data_bordered = si_bordered->GetAtlas()->GetTexture()->GetTextureRegion({bordered_ox, bordered_oy}, si_bordered->GetSize());
    }

    // Normalize color to gray
    if (font.MakeGray) {
        for (auto y = 0; y < atlas_spr->GetSize().height; y++) {
            for (auto x = 0; x < atlas_spr->GetSize().width; x++) {
                const auto a = pixel_at(data_normal, atlas_spr->GetSize().width, x, y).comp.a;

                if (a != 0) {
                    pixel_at(data_normal, atlas_spr->GetSize().width, x, y) = ucolor {128, 128, 128, a};

                    if (si_bordered != nullptr) {
                        pixel_at(data_bordered, si_bordered->GetSize().width, x, y) = ucolor {128, 128, 128, a};
                    }
                }
                else {
                    pixel_at(data_normal, atlas_spr->GetSize().width, x, y) = ucolor {0, 0, 0, 0};

                    if (si_bordered != nullptr) {
                        pixel_at(data_bordered, si_bordered->GetSize().width, x, y) = ucolor {0, 0, 0, 0};
                    }
                }
            }
        }

        atlas_spr->GetAtlas()->GetTexture()->UpdateTextureRegion({normal_ox, normal_oy}, atlas_spr->GetSize(), data_normal.data());
    }

    // Fill border
    if (si_bordered != nullptr) {
        for (auto y = 1; y < si_bordered->GetSize().height - 2; y++) {
            for (auto x = 1; x < si_bordered->GetSize().width - 2; x++) {
                if (pixel_at(data_normal, atlas_spr->GetSize().width, x, y) != ucolor::clear) {
                    for (auto xx = -1; xx <= 1; xx++) {
                        for (auto yy = -1; yy <= 1; yy++) {
                            const auto ox = x + xx;
                            const auto oy = y + yy;

                            if (pixel_at(data_bordered, si_bordered->GetSize().width, ox, oy) == ucolor::clear) {
                                pixel_at(data_bordered, si_bordered->GetSize().width, ox, oy) = ucolor {0, 0, 0, 255};
                            }
                        }
                    }
                }
            }
        }

        si_bordered->GetAtlas()->GetTexture()->UpdateTextureRegion({bordered_ox, bordered_oy}, si_bordered->GetSize(), data_bordered.data());

        // Fix texture coordinates on bordered texture
        tex_w = numeric_cast<float32_t>(si_bordered->GetAtlas()->GetSize().width);
        tex_h = numeric_cast<float32_t>(si_bordered->GetAtlas()->GetSize().height);
        image_x = tex_w * si_bordered->GetAtlasRect().x;
        image_y = tex_h * si_bordered->GetAtlasRect().y;

        for (auto& letter : font.Letters | std::views::values) {
            const auto x = numeric_cast<float32_t>(letter.Pos.x);
            const auto y = numeric_cast<float32_t>(letter.Pos.y);
            const auto w = numeric_cast<float32_t>(letter.Size.width);
            const auto h = numeric_cast<float32_t>(letter.Size.height);
            letter.TexBorderedPos.x = (image_x + x - 1.0f) / tex_w;
            letter.TexBorderedPos.y = (image_y + y - 1.0f) / tex_h;
            letter.TexBorderedPos.width = (w + 2.0f) / tex_w;
            letter.TexBorderedPos.height = (h + 2.0f) / tex_h;
        }
    }
}

void FontManager::BindFoFont(FontType font, string_view font_path, AtlasType atlas_type, bool not_bordered, bool skip_if_loaded)
{
    FO_STACK_TRACE_ENTRY();

    const auto index = static_cast<int32_t>(font);
    FO_RUNTIME_ASSERT(index >= 0);

    // Skip if loaded
    if (skip_if_loaded && index < numeric_cast<int32_t>(_allFonts.size()) && _allFonts[index]) {
        return;
    }

    // Load font data
    const auto file = _sprMngr->_resources->ReadFile(font_path);

    if (!file) {
        throw FontManagerException("Font file not found", font_path);
    }

    auto font_data = SafeAlloc::MakeUnique<FontData>();
    font_data->DrawEffect = _sprMngr->_effectMngr->Effects.Font;

    string image_name;

    // Parse data
    istringstream str(file.GetStr());
    string key;
    string letter_buf;
    FontData::Letter* cur_letter = nullptr;
    auto version = -1;

    while (!str.eof() && !str.fail()) {
        // Get key
        str >> key;

        // Cut off comments
        auto comment = key.find('#');

        if (comment != string::npos) {
            key.erase(comment);
        }

        comment = key.find(';');

        if (comment != string::npos) {
            key.erase(comment);
        }

        // Check version
        if (version == -1) {
            if (key != "Version") {
                throw FontManagerException("Font 'Version' signature not found (deprecated 'fofnt' format)", font_path);
            }

            str >> version;

            if (version > 2) {
                throw FontManagerException("Font version not supported (update client)", font_path, version);
            }

            continue;
        }

        // Get value
        if (key == "Image") {
            str >> image_name;
        }
        else if (key == "LineHeight") {
            str >> font_data->LineHeight;
        }
        else if (key == "YAdvance") {
            str >> font_data->YAdvance;
        }
        else if (key == "End") {
            break;
        }
        else if (key == "Letter") {
            std::getline(str, letter_buf, '\n');
            auto utf8_letter_begin = letter_buf.find('\'');

            if (utf8_letter_begin == string::npos) {
                throw FontManagerException("Invalid letter specification", font_path);
            }

            utf8_letter_begin++;

            size_t letter_len = letter_buf.length() - utf8_letter_begin;
            auto letter = utf8::Decode(letter_buf.c_str() + utf8_letter_begin, letter_len);

            if (!utf8::IsValid(letter)) {
                throw FontManagerException("Invalid UTF-8 letter", font_path, letter_buf);
            }

            cur_letter = &font_data->Letters[letter];
        }

        if (cur_letter == nullptr) {
            continue;
        }

        if (key == "PositionX") {
            str >> cur_letter->Pos.x;
        }
        else if (key == "PositionY") {
            str >> cur_letter->Pos.y;
        }
        else if (key == "Width") {
            str >> cur_letter->Size.width;
        }
        else if (key == "Height") {
            str >> cur_letter->Size.height;
        }
        else if (key == "OffsetX") {
            str >> cur_letter->Offset.x;
        }
        else if (key == "OffsetY") {
            str >> cur_letter->Offset.y;
        }
        else if (key == "XAdvance") {
            str >> cur_letter->XAdvance;
        }
    }

    bool make_gray = false;

    if (image_name.back() == '*') {
        make_gray = true;
        image_name = image_name.substr(0, image_name.size() - 1);
    }

    font_data->MakeGray = make_gray;

    // Load image
    {
        image_name = strex(font_path).extract_dir().combine_path(image_name);

        font_data->ImageNormal = dynamic_ptr_cast<AtlasSprite>(_sprMngr->LoadSprite(_sprMngr->_hashResolver->ToHashedString(image_name), atlas_type));

        if (!font_data->ImageNormal) {
            throw FontManagerException("Font image file not found", font_path, image_name);
        }

        _sprMngr->_copyableSpriteCache.erase({_sprMngr->_hashResolver->ToHashedString(image_name), atlas_type});
    }

    // Create bordered instance
    if (!not_bordered) {
        font_data->ImageBordered = dynamic_ptr_cast<AtlasSprite>(_sprMngr->LoadSprite(_sprMngr->_hashResolver->ToHashedString(image_name), atlas_type));

        if (!font_data->ImageBordered) {
            throw FontManagerException("Can't load font image twice", font_path, image_name);
        }

        _sprMngr->_copyableSpriteCache.erase({_sprMngr->_hashResolver->ToHashedString(image_name), atlas_type});
    }

    // Register
    if (index >= numeric_cast<int32_t>(_allFonts.size())) {
        _allFonts.resize(index + 1);
    }

    _allFonts[index] = std::move(font_data);
    BuildFont(index);
    _formatCache.clear();
}

void FontManager::BindBmfFont(FontType font, string_view font_path, AtlasType atlas_type)
{
    FO_STACK_TRACE_ENTRY();

    const auto index = static_cast<int32_t>(font);
    FO_RUNTIME_ASSERT(index >= 0);

    auto font_data = SafeAlloc::MakeUnique<FontData>();
    font_data->DrawEffect = _sprMngr->_effectMngr->Effects.Font;

    const auto file = _sprMngr->_resources->ReadFile(font_path);

    if (!file) {
        throw FontManagerException("Font file not found", font_path);
    }

    auto reader = file.GetReader();

    const auto signature = reader.GetLEUInt32();
    const auto make_signature = [](uint8_t ch0, uint8_t ch1, uint8_t ch2, uint8_t ch3) -> uint32_t { return ch0 | ch1 << 8 | ch2 << 16 | ch3 << 24; };

    if (signature != make_signature('B', 'M', 'F', 3)) {
        throw FontManagerException("Invalid font signature", font_path);
    }

    // Info
    reader.GoForward(1);
    auto block_len = reader.GetLEUInt32();
    auto next_block = block_len + reader.GetCurPos() + 1;

    reader.GoForward(7);

    if (reader.GetBEUInt32() != 0x01010101u) {
        throw FontManagerException("Wrong padding in font", font_path);
    }

    // Common
    reader.SetCurPos(next_block);
    block_len = reader.GetLEUInt32();
    next_block = block_len + reader.GetCurPos() + 1;

    const int32_t line_height = reader.GetLEUInt16();
    const int32_t base_height = reader.GetLEUInt16();
    reader.GoForward(2); // Texture width
    reader.GoForward(2); // Texture height

    if (reader.GetLEUInt16() != 1) {
        throw FontManagerException("Font must have exactly one texture", font_path);
    }

    // Pages
    reader.SetCurPos(next_block);
    block_len = reader.GetLEUInt32();
    next_block = block_len + reader.GetCurPos() + 1;

    auto image_name = reader.GetStrNT();
    image_name = strex(font_path).extract_dir().combine_path(image_name);

    // Chars
    reader.SetCurPos(next_block);
    const auto count = reader.GetLEUInt32() / 20;

    for ([[maybe_unused]] const auto i : iterate_range(count)) {
        // Read data
        const auto id = reader.GetLEUInt32();
        const auto x = reader.GetLEUInt16();
        const auto y = reader.GetLEUInt16();
        const auto w = reader.GetLEUInt16();
        const auto h = reader.GetLEUInt16();
        const auto ox = reader.GetLEUInt16();
        const auto oy = reader.GetLEUInt16();
        const auto xa = reader.GetLEUInt16();
        reader.GoForward(2);

        // Fill data
        auto& let = font_data->Letters[id];
        let.Pos.x = x + 1;
        let.Pos.y = y + 1;
        let.Size.width = w - 2;
        let.Size.height = h - 2;
        let.Offset.x = -numeric_cast<int32_t>(ox);
        let.Offset.y = -numeric_cast<int32_t>(oy) + (line_height - base_height);
        let.XAdvance = xa + 1;
    }

    font_data->LineHeight = font_data->Letters.count(numeric_cast<uint32_t>('W')) != 0 ? font_data->Letters[numeric_cast<uint32_t>('W')].Size.height : base_height;
    font_data->YAdvance = font_data->LineHeight / 2;
    font_data->MakeGray = true;

    // Load image
    {
        font_data->ImageNormal = dynamic_ptr_cast<AtlasSprite>(_sprMngr->LoadSprite(_sprMngr->_hashResolver->ToHashedString(image_name), atlas_type));

        if (!font_data->ImageNormal) {
            throw FontManagerException("Font image file not found", font_path, image_name);
        }

        _sprMngr->_copyableSpriteCache.erase({_sprMngr->_hashResolver->ToHashedString(image_name), atlas_type});
    }

    // Create bordered instance
    {
        font_data->ImageBordered = dynamic_ptr_cast<AtlasSprite>(_sprMngr->LoadSprite(_sprMngr->_hashResolver->ToHashedString(image_name), atlas_type));

        if (!font_data->ImageBordered) {
            throw FontManagerException("Can't load font image twice", font_path, image_name);
        }

        _sprMngr->_copyableSpriteCache.erase({_sprMngr->_hashResolver->ToHashedString(image_name), atlas_type});
    }

    // Register
    if (index >= numeric_cast<int32_t>(_allFonts.size())) {
        _allFonts.resize(index + 1);
    }

    _allFonts[index] = std::move(font_data);
    BuildFont(index);
    _formatCache.clear();
}

void FontManager::FormatText(FontFormatInfo& fi, FormatMode mode) const
{
    FO_STACK_TRACE_ENTRY();

    auto& str = fi.Text;
    const auto flags = fi.Format.Flags;
    const auto* font = fi.CurFont.get();
    const auto r = fi.Rect;
    const auto infinity_w = r.width == 0;
    const auto infinity_h = r.height == 0;
    auto curx = 0;
    auto cury = 0;
    auto& color_offset = fi.ColorOffset;

    // Colorize: strip `@color:...@` markers and write parsed colors into TextColor.
    ucolor* dots = nullptr;
    string buf;
    buf.reserve(str.size());

    if (mode == FormatMode::Draw && !IsEnumSet(flags, FontFlag::NoColorize)) {
        dots = fi.TextColor.data();
    }

    constexpr int32_t dots_history_len = 10;
    ucolor dots_history[dots_history_len] = {fi.Color};
    int32_t dots_history_cur = 0;

    size_t pos = 0;

    while (pos < str.size()) {
        const size_t marker_pos = str.find(InlineColorTagPrefix, pos);

        if (marker_pos == string::npos) {
            buf.append(str, pos, string::npos);
            break;
        }

        buf.append(str, pos, marker_pos - pos);

        size_t tag_end = 0;
        ucolor d;
        bool reset = false;

        if (!ParseInlineColorTag(str, marker_pos, tag_end, d, reset)) {
            buf.append(str, marker_pos, 1);
            pos = marker_pos + 1;
            continue;
        }

        if (dots != nullptr) {
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
    const bool skip_from_bottom = IsEnumSet(flags, FontFlag::AlignBottom);
    int32_t skip_line = skip_from_bottom ? 0 : fi.Format.SkipLines;
    int32_t skip_line_end = skip_from_bottom ? fi.Format.SkipLines : 0;

    // Format
    curx = r.x;
    cury = r.y;

    for (int32_t i = 0, i_advance = 1; i < numeric_cast<int32_t>(str.size()); i += i_advance) {
        size_t letter_len = utf8::DecodeStrNtLen(&str[i]);
        uint32_t letter = utf8::Decode(&str[i], letter_len);
        letter = utf8::IsValid(letter) ? letter : 0;
        i_advance = numeric_cast<int32_t>(letter_len);

        int32_t x_advance = 0;

        switch (letter) {
        case '\r':
            continue;
        case ' ':
            x_advance = font->SpaceWidth;
            break;
        case '\t':
            x_advance = font->SpaceWidth * 4;
            break;
        default:
            auto it = font->Letters.find(letter);
            if (it != font->Letters.end()) {
                x_advance = it->second.XAdvance;
            }
            else {
                x_advance = 0;
            }
            break;
        }

        if (!infinity_w && curx + x_advance > r.x + r.width) {
            fi.MaxCurX = std::max(curx, fi.MaxCurX);

            if (mode == FormatMode::Draw && IsEnumSet(flags, FontFlag::NoWrap)) {
                str.resize(numeric_cast<size_t>(i));
                break;
            }

            if (IsEnumSet(flags, FontFlag::TruncateLine)) {
                int32_t j = i;

                while (j < numeric_cast<int32_t>(str.size()) && str[j] != '\n') {
                    j++;
                }

                const auto erased = j - i;
                str.erase(numeric_cast<size_t>(i), numeric_cast<size_t>(erased));
                letter = i < numeric_cast<int32_t>(str.size()) ? numeric_cast<uint8_t>(str[i]) : 0;
                i_advance = 1;

                if (mode == FormatMode::Draw) {
                    for (auto k = i, l = numeric_cast<int32_t>(fi.TextColor.size()) - erased; k < l; k++) {
                        fi.TextColor[k] = fi.TextColor[k + erased];
                    }
                }
            }
            else if (str[i] != '\n') {
                int32_t j = i;

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
                        for (auto k = numeric_cast<int32_t>(fi.TextColor.size()) - 1; k > i; k--) {
                            fi.TextColor[k] = fi.TextColor[k - 1];
                        }
                    }
                }

                if (IsEnumSet(flags, FontFlag::Justify) && skip_line == 0) {
                    fi.LineSpaceWidth[fi.LinesAll - 1] = 1;

                    // Erase next first spaces
                    const int32_t ii = i + i_advance;
                    j = ii;

                    while (j < numeric_cast<int32_t>(str.size()) && str[j] == ' ') {
                        j++;
                    }

                    if (j > ii) {
                        const auto erased = j - ii;
                        str.erase(numeric_cast<size_t>(ii), numeric_cast<size_t>(erased));

                        if (mode == FormatMode::Draw) {
                            for (auto k = ii, l = numeric_cast<int32_t>(fi.TextColor.size()) - erased; k < l; k++) {
                                fi.TextColor[k] = fi.TextColor[k + erased];
                            }
                        }
                    }
                }
            }
        }

        switch (letter) {
        case '\n':
            if (skip_line == 0) {
                cury += font->LineHeight + font->YAdvance;

                if (!infinity_h && cury + font->LineHeight > r.y + r.height && fi.LinesInRect == 0) {
                    fi.LinesInRect = fi.LinesAll;
                }

                if (mode == FormatMode::Draw) {
                    if (fi.LinesInRect != 0 && !IsEnumSet(flags, FontFlag::KeepTail)) {
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
            continue;
        }

        if (i >= numeric_cast<int32_t>(str.size())) {
            break;
        }
    }

    fi.MaxCurX = std::max(curx, fi.MaxCurX);

    if (skip_line_end != 0) {
        int32_t cut_pos = numeric_cast<int32_t>(str.size());

        for (int32_t i = cut_pos - 2; i >= 0; i--) {
            if (str[i] == '\n') {
                cut_pos = i;
                fi.LinesAll--;

                if (--skip_line_end == 0) {
                    break;
                }
            }
        }

        str.resize(numeric_cast<size_t>(cut_pos));
        FO_RUNTIME_ASSERT(skip_line_end == 0);
    }

    FO_RUNTIME_ASSERT(skip_line == 0);

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
    if (IsEnumSet(flags, FontFlag::KeepTail) && fi.LinesAll > fi.LinesInRect) {
        int32_t j = 0;
        int32_t line_cur = 0;
        ucolor last_color;
        const int32_t str_size = numeric_cast<int32_t>(str.size());

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

        if (!IsEnumSet(flags, FontFlag::NoColorize)) {
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

    for (const auto i : iterate_range(fi.LinesAll)) {
        fi.LineWidth[i] = curx;
    }

    bool can_count = false;
    int32_t spaces = 0;
    int32_t curstr = 0;

    for (int32_t i = 0, i_advance = 1;; i += i_advance) {
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

        auto letter_len = utf8::DecodeStrNtLen(&str[i]);
        auto letter = utf8::Decode(&str[i], letter_len);
        letter = utf8::IsValid(letter) ? letter : 0;
        i_advance = numeric_cast<int32_t>(letter_len);

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
            if (const auto it = font->Letters.find(letter); it != font->Letters.end()) {
                curx += it->second.XAdvance;
            }

            can_count = true;
            break;
        }
    }

    // Initial position
    fi.CurX = r.x;
    fi.CurY = r.y;

    // Align X
    if (IsEnumSet(flags, FontFlag::CenterX)) {
        fi.CurX += ((r.x + r.width) - fi.LineWidth[0]) / 2;
    }
    else if (IsEnumSet(flags, FontFlag::AlignRight)) {
        fi.CurX += (r.x + r.width) - fi.LineWidth[0];
    }

    // Align Y
    if (IsEnumSet(flags, FontFlag::CenterY)) {
        fi.CurY = r.y + (r.height + 1 - fi.LinesInRect * font->LineHeight - (fi.LinesInRect - 1) * font->YAdvance) / 2;
    }
    else if (IsEnumSet(flags, FontFlag::AlignBottom)) {
        fi.CurY = (r.y + r.height) - (fi.LinesInRect * font->LineHeight + (fi.LinesInRect - 1) * font->YAdvance);
    }
}

auto FontManager::IsInlineColorHex(string_view value) -> bool
{
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

    if (marker_pos + InlineColorTagPrefix.size() >= str.size()) {
        return false;
    }
    if (str.compare(marker_pos, InlineColorTagPrefix.size(), InlineColorTagPrefix) != 0) {
        return false;
    }

    const size_t close_pos = str.find('@', marker_pos + InlineColorTagPrefix.size());

    if (close_pos == string_view::npos) {
        return false;
    }

    string_view value = str.substr(marker_pos + InlineColorTagPrefix.size(), close_pos - marker_pos - InlineColorTagPrefix.size());

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

auto FontManager::GetOrFormat(TextFormat format, FontType font, irect32 rect, ucolor color, FormatMode mode, string_view str) const -> const FontFormatInfo*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(static_cast<size_t>(font) < _allFonts.size());
    FO_RUNTIME_ASSERT(format.SkipLines >= 0);
    FO_RUNTIME_ASSERT(rect.width >= 0);
    FO_RUNTIME_ASSERT(rect.height >= 0);

    const std::array<uint64_t, 8> key_parts {
        hashing_ex::hash(str.data(), str.size()),
        static_cast<uint32_t>(font),
        static_cast<uint32_t>(format.Flags),
        static_cast<uint32_t>(format.SkipLines),
        static_cast<uint32_t>(rect.width),
        static_cast<uint32_t>(rect.height),
        color.underlying_value(),
        static_cast<uint32_t>(mode),
    };

    const uint64_t key = hashing_ex::hash(key_parts.data(), key_parts.size() * sizeof(uint64_t));

    if (auto it = _formatCache.find(key); it != _formatCache.end()) {
        it->second->LastUsedFrame = _frameIndex;
        return it->second.get();
    }

    const auto str_len = str.length();
    const auto max_chars = std::max<size_t>(str_len * 2 + 1, 1);
    const auto max_lines = std::max<size_t>(str_len + 1, 1);

    auto fi = SafeAlloc::MakeUnique<FontFormatInfo>();
    fi->CurFont = _allFonts[static_cast<size_t>(font)].get();
    fi->Format = format;
    fi->Rect = irect32 {0, 0, rect.width, rect.height};
    fi->Color = color;
    fi->Text.assign(str);
    fi->TextColor.assign(max_chars, ucolor::clear);
    fi->LineWidth.assign(max_lines, 0);
    fi->LineSpaceWidth.assign(max_lines, 0);
    fi->LastUsedFrame = _frameIndex;

    FormatText(*fi, mode);

    auto* fi_ptr = fi.get();
    _formatCache.emplace(key, std::move(fi));
    return fi_ptr;
}

void FontManager::DrawText(irect32 rect, string_view str, ucolor color, TextFormat format)
{
    FO_STACK_TRACE_ENTRY();

    if (str.empty()) {
        return;
    }

    const auto* font = GetFont(format.Font);
    color = _sprMngr->ApplyColorBrightness(color);
    const auto* fi = GetOrFormat(format, format.Font, rect, color, FormatMode::Draw, str);

    const auto flags = format.Flags;
    const string& format_str = fi->Text;
    const int32_t color_offset = fi->ColorOffset;
    int32_t curx = rect.x + fi->CurX;
    int32_t cury = rect.y + fi->CurY;
    int32_t curstr = 0;
    const auto& texture = IsEnumSet(flags, FontFlag::Bordered) && font->FontTexBordered ? font->FontTexBordered : font->FontTex;

    if (!IsEnumSet(flags, FontFlag::NoColorize)) {
        for (int32_t i = color_offset; i >= 0; i--) {
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

    const size_t str_len = format_str.size();
    _sprMngr->_spritesDrawBuf->CheckAllocBuf(str_len * 4, str_len * 6);

    auto& vbuf = _sprMngr->_spritesDrawBuf->Vertices;
    auto& vpos = _sprMngr->_spritesDrawBuf->VertCount;
    auto& ibuf = _sprMngr->_spritesDrawBuf->Indices;
    auto& ipos = _sprMngr->_spritesDrawBuf->IndCount;

    const auto start_ipos = ipos;

    auto variable_space = false;
    int32_t i_advance;

    for (int32_t i = 0; i < numeric_cast<int32_t>(format_str.size()); i += i_advance) {
        if (!IsEnumSet(flags, FontFlag::NoColorize)) {
            const auto new_color = fi->TextColor[i + color_offset];

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

        size_t letter_len = utf8::DecodeStrNtLen(&format_str[i]);
        uint32_t letter = utf8::Decode(&format_str[i], letter_len);
        letter = utf8::IsValid(letter) ? letter : 0;
        i_advance = numeric_cast<int32_t>(letter_len);

        switch (letter) {
        case ' ':
            curx += variable_space ? fi->LineSpaceWidth[curstr] : font->SpaceWidth;
            continue;
        case '\t':
            curx += font->SpaceWidth * 4;
            continue;
        case '\n':
            cury += font->LineHeight + font->YAdvance;
            curx = rect.x;
            curstr++;
            variable_space = false;

            if (IsEnumSet(flags, FontFlag::CenterX)) {
                curx += (rect.width - fi->LineWidth[curstr]) / 2;
            }
            else if (IsEnumSet(flags, FontFlag::AlignRight)) {
                curx += rect.width - fi->LineWidth[curstr];
            }
            continue;
        case '\r':
            continue;
        default:
            const auto it = font->Letters.find(letter);

            if (it == font->Letters.end()) {
                continue;
            }

            const auto& l = it->second;

            const auto x = numeric_cast<float32_t>(curx - l.Offset.x - 1);
            const auto y = numeric_cast<float32_t>(cury - l.Offset.y - 1);
            const auto w = numeric_cast<float32_t>(l.Size.width + 2);
            const auto h = numeric_cast<float32_t>(l.Size.height + 2);

            const auto& texture_uv = IsEnumSet(flags, FontFlag::Bordered) ? l.TexBorderedPos : l.TexPos;
            const auto x1 = texture_uv.x;
            const auto y1 = texture_uv.y;
            const auto x2 = texture_uv.x + texture_uv.width;
            const auto y2 = texture_uv.y + texture_uv.height;

            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 0);
            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 1);
            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 3);
            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 1);
            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 2);
            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 3);

            auto& v0 = vbuf[vpos++];
            v0.PosX = x;
            v0.PosY = y + h;
            v0.TexU = x1;
            v0.TexV = y2;
            v0.EggFlags[0] = 0.0f;
            v0.EggFlags[1] = 0.0f;
            v0.Color = color;

            auto& v1 = vbuf[vpos++];
            v1.PosX = x;
            v1.PosY = y;
            v1.TexU = x1;
            v1.TexV = y1;
            v1.EggFlags[0] = 0.0f;
            v1.EggFlags[1] = 0.0f;
            v1.Color = color;

            auto& v2 = vbuf[vpos++];
            v2.PosX = x + w;
            v2.PosY = y;
            v2.TexU = x2;
            v2.TexV = y1;
            v2.EggFlags[0] = 0.0f;
            v2.EggFlags[1] = 0.0f;
            v2.Color = color;

            auto& v3 = vbuf[vpos++];
            v3.PosX = x + w;
            v3.PosY = y + h;
            v3.TexU = x2;
            v3.TexV = y2;
            v3.EggFlags[0] = 0.0f;
            v3.EggFlags[1] = 0.0f;
            v3.Color = color;

            curx += l.XAdvance;
            variable_space = true;
        }
    }

    const auto ind_count = ipos - start_ipos;

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

auto FontManager::GetLinesCount(isize32 size, string_view str, FontType num_font) const -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    if (size.width <= 0 || size.height <= 0) {
        return 0;
    }

    const auto* font = GetFont(num_font);

    if (str.empty()) {
        return size.height / (font->LineHeight + font->YAdvance);
    }

    const auto* fi = GetOrFormat(TextFormat {}, num_font, irect32 {0, 0, size.width, size.height}, ucolor {}, FormatMode::LineCount, str);
    return fi->LinesInRect;
}

auto FontManager::GetLinesHeight(isize32 size, string_view str, FontType num_font) const -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    if (size.width <= 0 || size.height <= 0) {
        return 0;
    }

    const auto* font = GetFont(num_font);
    const auto lines_count = GetLinesCount(size, str, num_font);

    if (lines_count <= 0) {
        return 0;
    }

    return lines_count * font->LineHeight + (lines_count - 1) * font->YAdvance;
}

auto FontManager::GetLineHeight(FontType num_font) const -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    return GetFont(num_font)->LineHeight;
}

auto FontManager::GetTextInfo(isize32 size, string_view str, TextFormat format, isize32& result_size, int32_t& lines) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    result_size = {};
    lines = {};

    const auto* font = GetFont(format.Font);

    if (str.empty()) {
        return true;
    }

    const auto rect = irect32 {0, 0, size};
    const auto* fi = GetOrFormat(format, format.Font, rect, ucolor {}, FormatMode::LineCount, str);

    result_size = {fi->MaxCurX, fi->LinesInRect * font->LineHeight + (fi->LinesInRect - 1) * font->YAdvance};
    lines = fi->LinesInRect;
    return true;
}

auto FontManager::SplitLines(irect32 rect, string_view cstr, FontType num_font) -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    if (cstr.empty()) {
        return {};
    }

    const auto* fi = GetOrFormat(TextFormat {}, num_font, rect, ucolor {}, FormatMode::Split, cstr);
    return fi->Lines;
}

auto FontManager::HaveLetter(FontType num_font, uint32_t letter) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return GetFont(num_font)->Letters.count(letter) != 0;
}

FO_END_NAMESPACE
