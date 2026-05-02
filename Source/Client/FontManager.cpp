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

static void StrCopy(char* to, size_t size, string_view from)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(to);
    FO_RUNTIME_ASSERT(size > 0);

    if (from.empty()) {
        to[0] = 0;
        return;
    }

    if (from.length() >= size) {
        MemCopy(to, from.data(), size - 1);
        to[size - 1] = 0;
    }
    else {
        MemCopy(to, from.data(), from.length());
        to[from.length()] = 0;
    }
}

static void StrGoTo(char*& str, char ch)
{
    FO_STACK_TRACE_ENTRY();

    while (*str != 0 && *str != ch) {
        ++str;
    }
}

static void StrEraseInterval(char* str, int32_t len)
{
    FO_STACK_TRACE_ENTRY();

    if (str == nullptr || len == 0) {
        return;
    }

    const auto* str2 = str + len;
    while (*str2 != 0) {
        *str = *str2;
        ++str;
        ++str2;
    }

    *str = 0;
}

static void StrInsert(char* to, const char* from, int32_t from_len)
{
    FO_STACK_TRACE_ENTRY();

    if (to == nullptr || from == nullptr) {
        return;
    }

    if (from_len == 0) {
        from_len = numeric_cast<int32_t>(string_view(from).length());
    }
    if (from_len == 0) {
        return;
    }

    auto* end_to = to;
    while (*end_to != 0) {
        ++end_to;
    }

    for (; end_to >= to; --end_to) {
        *(end_to + from_len) = *end_to;
    }

    while (from_len-- != 0) {
        *to = *from;
        ++to;
        ++from;
    }
}

FontManager::FontManager(SpriteManager& spr_mngr) :
    _sprMngr {&spr_mngr}
{
    FO_STACK_TRACE_ENTRY();
}

auto FontManager::GetFont(FontType font) -> FontData*
{
    FO_STACK_TRACE_ENTRY();

    auto num = static_cast<int32_t>(font);
    if (num < 0) {
        num = _defFontIndex;
    }
    if (num < 0 || num >= numeric_cast<int32_t>(_allFonts.size())) {
        return nullptr;
    }

    return _allFonts[num].get();
}

auto FontManager::GetFont(FontType font) const -> const FontData*
{
    FO_STACK_TRACE_ENTRY();

    auto num = static_cast<int32_t>(font);
    if (num < 0) {
        num = _defFontIndex;
    }
    if (num < 0 || num >= numeric_cast<int32_t>(_allFonts.size())) {
        return nullptr;
    }

    return _allFonts[num].get();
}

void FontManager::ClearFonts()
{
    FO_STACK_TRACE_ENTRY();

    _allFonts.clear();
}

void FontManager::SetDefaultFont(FontType font)
{
    FO_STACK_TRACE_ENTRY();

    _defFontIndex = static_cast<int32_t>(font);
}

void FontManager::SetFontEffect(FontType font, RenderEffect* effect)
{
    FO_STACK_TRACE_ENTRY();

    auto* font_data = GetFont(font);

    if (font_data != nullptr) {
        font_data->DrawEffect = effect != nullptr ? effect : _sprMngr->_effectMngr->Effects.Font;
    }
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

auto FontManager::LoadFontFO(FontType font, string_view font_name, AtlasType atlas_type, bool not_bordered, bool skip_if_loaded) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto index = static_cast<int32_t>(font);

    // Skip if loaded
    if (skip_if_loaded && index < numeric_cast<int32_t>(_allFonts.size()) && _allFonts[index]) {
        return true;
    }

    // Load font data
    const string fname = strex("Fonts/{}.fofnt", font_name);
    const auto file = _sprMngr->_resources->ReadFile(fname);

    if (!file) {
        WriteLog("File '{}' not found", fname);
        return false;
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
                WriteLog("Font '{}' 'Version' signature not found (used deprecated format of 'fofnt')", fname);
                return false;
            }

            str >> version;

            if (version > 2) {
                WriteLog("Font '{}' version {} not supported (try update client)", fname, version);
                return false;
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
                WriteLog("Font '{}' invalid letter specification", fname);
                return false;
            }

            utf8_letter_begin++;

            size_t letter_len = letter_buf.length() - utf8_letter_begin;
            auto letter = utf8::Decode(letter_buf.c_str() + utf8_letter_begin, letter_len);

            if (!utf8::IsValid(letter)) {
                WriteLog("Font '{}' invalid UTF8 letter at  '{}'", fname, letter_buf);
                return false;
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
        image_name.insert(0, "Fonts/");

        font_data->ImageNormal = dynamic_ptr_cast<AtlasSprite>(_sprMngr->LoadSprite(_sprMngr->_hashResolver->ToHashedString(image_name), atlas_type));

        if (!font_data->ImageNormal) {
            WriteLog("Image file '{}' not found", image_name);
            return false;
        }

        _sprMngr->_copyableSpriteCache.erase({_sprMngr->_hashResolver->ToHashedString(image_name), atlas_type});
    }

    // Create bordered instance
    if (!not_bordered) {
        font_data->ImageBordered = dynamic_ptr_cast<AtlasSprite>(_sprMngr->LoadSprite(_sprMngr->_hashResolver->ToHashedString(image_name), atlas_type));

        if (!font_data->ImageBordered) {
            WriteLog("Can't load twice file '{}'", image_name);
            return false;
        }

        _sprMngr->_copyableSpriteCache.erase({_sprMngr->_hashResolver->ToHashedString(image_name), atlas_type});
    }

    // Register
    if (index >= numeric_cast<int32_t>(_allFonts.size())) {
        _allFonts.resize(index + 1);
    }

    _allFonts[index] = std::move(font_data);

    BuildFont(index);

    return true;
}

auto FontManager::LoadFontBmf(FontType font, string_view font_name, AtlasType atlas_type) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto index = static_cast<int32_t>(font);

    if (index < 0) {
        WriteLog("Invalid index");
        return false;
    }

    auto font_data = SafeAlloc::MakeUnique<FontData>();
    font_data->DrawEffect = _sprMngr->_effectMngr->Effects.Font;

    const auto file = _sprMngr->_resources->ReadFile(strex("Fonts/{}.fnt", font_name));

    if (!file) {
        WriteLog("Font file '{}.fnt' not found", font_name);
        return false;
    }

    auto reader = file.GetReader();

    const auto signature = reader.GetLEUInt32();
    const auto make_signature = [](uint8_t ch0, uint8_t ch1, uint8_t ch2, uint8_t ch3) -> uint32_t { return ch0 | ch1 << 8 | ch2 << 16 | ch3 << 24; };

    if (signature != make_signature('B', 'M', 'F', 3)) {
        WriteLog("Invalid signature of font '{}'", font_name);
        return false;
    }

    // Info
    reader.GoForward(1);
    auto block_len = reader.GetLEUInt32();
    auto next_block = block_len + reader.GetCurPos() + 1;

    reader.GoForward(7);

    if (reader.GetBEUInt32() != 0x01010101u) {
        WriteLog("Wrong padding in font '{}'", font_name);
        return false;
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
        WriteLog("Texture for font '{}' must be one", font_name);
        return false;
    }

    // Pages
    reader.SetCurPos(next_block);
    block_len = reader.GetLEUInt32();
    next_block = block_len + reader.GetCurPos() + 1;

    // Image name
    auto image_name = reader.GetStrNT();
    image_name.insert(0, "Fonts/");

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
            WriteLog("Image file '{}' not found", image_name);
            return false;
        }

        _sprMngr->_copyableSpriteCache.erase({_sprMngr->_hashResolver->ToHashedString(image_name), atlas_type});
    }

    // Create bordered instance
    {
        font_data->ImageBordered = dynamic_ptr_cast<AtlasSprite>(_sprMngr->LoadSprite(_sprMngr->_hashResolver->ToHashedString(image_name), atlas_type));

        if (!font_data->ImageBordered) {
            WriteLog("Can't load twice file '{}'", image_name);
            return false;
        }

        _sprMngr->_copyableSpriteCache.erase({_sprMngr->_hashResolver->ToHashedString(image_name), atlas_type});
    }

    // Register
    if (index >= numeric_cast<int32_t>(_allFonts.size())) {
        _allFonts.resize(index + 1);
    }

    _allFonts[index] = std::move(font_data);

    BuildFont(index);

    return true;
}

void FontManager::FormatText(FontFormatInfo& fi, int32_t fmt_type) const
{
    FO_STACK_TRACE_ENTRY();

    fi.PStr = fi.Str.data();

    auto* str = fi.PStr.get();
    auto flags = fi.Format.Flags;
    const auto* font = fi.CurFont.get();
    auto r = fi.Rect;
    auto infinity_w = r.width == 0;
    auto infinity_h = r.height == 0;
    auto curx = 0;
    auto cury = 0;
    auto& offs_col = fi.OffsColDots;

    if (fmt_type != FORMAT_TYPE_DRAW && fmt_type != FORMAT_TYPE_LCOUNT && fmt_type != FORMAT_TYPE_SPLIT) {
        fi.IsError = true;
        return;
    }

    if (fmt_type == FORMAT_TYPE_SPLIT && fi.StrLines == nullptr) {
        fi.IsError = true;
        return;
    }

    // Colorize
    ucolor* dots = nullptr;
    int32_t d_offs = 0;
    string buf;

    if (fmt_type == FORMAT_TYPE_DRAW && !IsEnumSet(flags, FontFlag::NoColorize)) {
        dots = fi.ColorDots.data();
    }

    constexpr int32_t dots_history_len = 10;
    ucolor dots_history[dots_history_len] = {fi.DefColor};
    int32_t dots_history_cur = 0;

    for (auto* str_ = str; *str_ != 0;) {
        auto* s0 = str_;
        StrGoTo(str_, '|');
        auto* s1 = str_;
        StrGoTo(str_, ' ');
        auto* s2 = str_;

        if (dots != nullptr) {
            auto d_len = numeric_cast<int32_t>(s2 - s1) + 1;
            ucolor d;

            if (d_len == 2) {
                if (dots_history_cur > 0) {
                    d = dots_history[--dots_history_cur];
                }
                else {
                    d = fi.DefColor;
                }
            }
            else {
                if (*(s1 + 1) == 'x') {
                    d = ucolor {numeric_cast<uint32_t>(std::strtoul(s1 + 2, nullptr, 16))};
                }
                else {
                    d = ucolor {numeric_cast<uint32_t>(std::strtoul(s1 + 1, nullptr, 0))};
                }

                if (dots_history_cur < dots_history_len - 1) {
                    dots_history[++dots_history_cur] = d;
                }
            }

            dots[numeric_cast<int32_t>(s1 - str) - d_offs] = d;
            d_offs += d_len;
        }

        *s1 = 0;
        buf += s0;

        if (*str_ == 0) {
            break;
        }
        str_++;
    }

    StrCopy(fi.PStr.get(), fi.Str.size(), buf);

    // Single SkipLines counter — its meaning flips between "from top" and "from bottom" based on FontFlag::AlignBottom
    const auto skip_from_bottom = IsEnumSet(flags, FontFlag::AlignBottom);
    auto skip_line = skip_from_bottom ? 0 : fi.Format.SkipLines;
    auto skip_line_end = skip_from_bottom ? fi.Format.SkipLines : 0;

    // Format
    curx = r.x;
    cury = r.y;

    for (auto i = 0, i_advance = 1; str[i] != 0; i += i_advance) {
        auto letter_len = utf8::DecodeStrNtLen(&str[i]);
        auto letter = utf8::Decode(&str[i], letter_len);
        letter = utf8::IsValid(letter) ? letter : 0;
        i_advance = numeric_cast<int32_t>(letter_len);

        auto x_advance = 0;
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

            if (fmt_type == FORMAT_TYPE_DRAW && IsEnumSet(flags, FontFlag::NoWrap)) {
                str[i] = 0;
                break;
            }

            if (IsEnumSet(flags, FontFlag::TruncateLine)) {
                auto j = i;

                for (; str[j] != 0; j++) {
                    if (str[j] == '\n') {
                        break;
                    }
                }

                StrEraseInterval(&str[i], j - i);
                letter = numeric_cast<uint8_t>(str[i]);
                i_advance = 1;

                if (fmt_type == FORMAT_TYPE_DRAW) {
                    for (auto k = i, l = numeric_cast<int32_t>(fi.ColorDots.size()) - (j - i); k < l; k++) {
                        fi.ColorDots[k] = fi.ColorDots[k + (j - i)];
                    }
                }
            }
            else if (str[i] != '\n') {
                auto j = i;

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
                    StrInsert(&str[i], "\n", 0);

                    if (fmt_type == FORMAT_TYPE_DRAW) {
                        for (auto k = numeric_cast<int32_t>(fi.ColorDots.size()) - 1; k > i; k--) {
                            fi.ColorDots[k] = fi.ColorDots[k - 1];
                        }
                    }
                }

                if (IsEnumSet(flags, FontFlag::Justify) && skip_line == 0) {
                    fi.LineSpaceWidth[fi.LinesAll - 1] = 1;

                    // Erase next first spaces
                    auto ii = i + i_advance;

                    for (j = ii;; j++) {
                        if (str[j] != ' ') {
                            break;
                        }
                    }

                    if (j > ii) {
                        StrEraseInterval(&str[ii], j - ii);
                        if (fmt_type == FORMAT_TYPE_DRAW) {
                            for (auto k = ii, l = numeric_cast<int32_t>(fi.ColorDots.size()) - (j - ii); k < l; k++) {
                                fi.ColorDots[k] = fi.ColorDots[k + (j - ii)];
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

                if (fmt_type == FORMAT_TYPE_DRAW) {
                    if (fi.LinesInRect != 0 && !IsEnumSet(flags, FontFlag::KeepTail)) {
                        // fi.LinesAll++;
                        str[i] = 0;
                        break;
                    }
                }
                else if (fmt_type == FORMAT_TYPE_SPLIT) {
                    if (fi.LinesInRect != 0 && fi.LinesAll % fi.LinesInRect == 0) {
                        str[i] = 0;
                        fi.StrLines->emplace_back(str);
                        str = &str[i + i_advance];
                        i = -i_advance;
                    }
                }

                if (str[i + i_advance] != 0) {
                    fi.LinesAll++;
                }
            }
            else {
                skip_line--;
                StrEraseInterval(str, i + i_advance);
                offs_col += i + i_advance;
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

        if (str[i] == 0) {
            break;
        }
    }

    fi.MaxCurX = std::max(curx, fi.MaxCurX);

    if (skip_line_end != 0) {
        auto len = numeric_cast<int32_t>(string_view(str).length());

        for (auto i = len - 2; i >= 0; i--) {
            if (str[i] == '\n') {
                str[i] = 0;
                fi.LinesAll--;
                if (--skip_line_end == 0) {
                    break;
                }
            }
        }

        if (skip_line_end != 0) {
            WriteLog("error3");
            fi.IsError = true;
            return;
        }
    }

    if (skip_line != 0) {
        WriteLog("error4");
        fi.IsError = true;
        return;
    }

    if (fi.LinesInRect == 0) {
        fi.LinesInRect = fi.LinesAll;
    }

    if (fmt_type == FORMAT_TYPE_SPLIT) {
        fi.StrLines->emplace_back(str);
        return;
    }
    if (fmt_type == FORMAT_TYPE_LCOUNT) {
        return;
    }

    // Up text
    if (IsEnumSet(flags, FontFlag::KeepTail) && fi.LinesAll > fi.LinesInRect) {
        int32_t j = 0;
        int32_t line_cur = 0;
        ucolor last_color;

        for (; str[j] != 0; ++j) {
            if (str[j] == '\n') {
                line_cur++;

                if (line_cur >= fi.LinesAll - fi.LinesInRect) {
                    break;
                }
            }

            if (fi.ColorDots[j] != ucolor::clear) {
                last_color = fi.ColorDots[j];
            }
        }

        if (!IsEnumSet(flags, FontFlag::NoColorize)) {
            offs_col += j + 1;

            if (last_color != ucolor::clear && fi.ColorDots[j + 1] == ucolor::clear) {
                fi.ColorDots[j + 1] = last_color;
            }
        }

        str = &str[j + 1];
        fi.PStr = str;

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

    for (auto i = 0, i_advance = 1;; i += i_advance) {
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
            auto it = font->Letters.find(letter);
            if (it != font->Letters.end()) {
                curx += it->second.XAdvance;
            }
            can_count = true;
            break;
        }

        if (str[i] == 0) {
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

void FontManager::DrawText(irect32 rect, string_view str, ucolor color, TextFormat format)
{
    FO_STACK_TRACE_ENTRY();

    // Check
    if (str.empty()) {
        return;
    }

    // Get font
    const auto* font = GetFont(format.Font);

    if (font == nullptr) {
        return;
    }

    // Format
    color = _sprMngr->ApplyColorBrightness(color);

    auto& fi = _fontFormatInfoBuf = {.CurFont = font, .Format = format, .Rect = rect};
    fi.Prepare(str);
    StrCopy(fi.Str.data(), fi.Str.size(), str);
    fi.DefColor = color;

    FormatText(fi, FORMAT_TYPE_DRAW);

    if (fi.IsError) {
        return;
    }

    const auto flags = format.Flags;
    const auto* str_ = fi.PStr.get();
    const auto offs_col = fi.OffsColDots;
    auto curx = fi.CurX;
    auto cury = fi.CurY;
    auto curstr = 0;
    const auto& texture = IsEnumSet(flags, FontFlag::Bordered) && font->FontTexBordered ? font->FontTexBordered : font->FontTex;

    if (!IsEnumSet(flags, FontFlag::NoColorize)) {
        for (auto i = offs_col; i >= 0; i--) {
            if (fi.ColorDots[i] != ucolor::clear) {
                if (fi.ColorDots[i].comp.a != 0) {
                    color = fi.ColorDots[i]; // With alpha
                }
                else {
                    color = ucolor {fi.ColorDots[i], color.comp.a};
                }
                color = _sprMngr->ApplyColorBrightness(color);
                break;
            }
        }
    }

    const auto str_len = string_view(str_).length();
    _sprMngr->_spritesDrawBuf->CheckAllocBuf(str_len * 4, str_len * 6);

    auto& vbuf = _sprMngr->_spritesDrawBuf->Vertices;
    auto& vpos = _sprMngr->_spritesDrawBuf->VertCount;
    auto& ibuf = _sprMngr->_spritesDrawBuf->Indices;
    auto& ipos = _sprMngr->_spritesDrawBuf->IndCount;

    const auto start_ipos = ipos;

    auto variable_space = false;
    int32_t i_advance;

    for (auto i = 0; str_[i] != 0; i += i_advance) {
        if (!IsEnumSet(flags, FontFlag::NoColorize)) {
            const auto new_color = fi.ColorDots[i + offs_col];

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

        auto letter_len = utf8::DecodeStrNtLen(&str_[i]);
        auto letter = utf8::Decode(&str_[i], letter_len);
        letter = utf8::IsValid(letter) ? letter : 0;
        i_advance = numeric_cast<int32_t>(letter_len);

        switch (letter) {
        case ' ':
            curx += variable_space ? fi.LineSpaceWidth[curstr] : font->SpaceWidth;
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
                curx += (rect.x + rect.width - fi.LineWidth[curstr]) / 2;
            }
            else if (IsEnumSet(flags, FontFlag::AlignRight)) {
                curx += rect.x + rect.width - fi.LineWidth[curstr];
            }
            continue;
        case '\r':
            continue;
        default:
            auto it = font->Letters.find(letter);
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

    if (font == nullptr) {
        return 0;
    }

    if (str.empty()) {
        return size.height / (font->LineHeight + font->YAdvance);
    }

    auto& fi = _fontFormatInfoBuf = {.CurFont = font, .Format = TextFormat {}, .Rect = {0, 0, size.width, size.height}};
    fi.Prepare(str);
    StrCopy(fi.Str.data(), fi.Str.size(), str);

    FormatText(fi, FORMAT_TYPE_LCOUNT);

    if (fi.IsError) {
        return 0;
    }

    return fi.LinesInRect;
}

auto FontManager::GetLinesHeight(isize32 size, string_view str, FontType num_font) const -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    if (size.width <= 0 || size.height <= 0) {
        return 0;
    }

    const auto* font = GetFont(num_font);

    if (font == nullptr) {
        return 0;
    }

    const auto lines_count = GetLinesCount(size, str, num_font);

    if (lines_count <= 0) {
        return 0;
    }

    return lines_count * font->LineHeight + (lines_count - 1) * font->YAdvance;
}

auto FontManager::GetLineHeight(FontType num_font) const -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    const auto* font = GetFont(num_font);

    if (font == nullptr) {
        return 0;
    }

    return font->LineHeight;
}

auto FontManager::GetTextInfo(isize32 size, string_view str, TextFormat format, isize32& result_size, int32_t& lines) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    result_size = {};
    lines = {};

    const auto* font = GetFont(format.Font);

    if (font == nullptr) {
        return false;
    }

    if (str.empty()) {
        return true;
    }

    auto& fi = _fontFormatInfoBuf = {.CurFont = font, .Format = format, .Rect = {0, 0, size}};
    fi.Prepare(str);
    StrCopy(fi.Str.data(), fi.Str.size(), str);

    FormatText(fi, FORMAT_TYPE_LCOUNT);

    if (fi.IsError) {
        return false;
    }

    result_size = {fi.MaxCurX - fi.Rect.x, fi.LinesInRect * font->LineHeight + (fi.LinesInRect - 1) * font->YAdvance};
    lines = fi.LinesInRect;

    return true;
}

auto FontManager::SplitLines(irect32 rect, string_view cstr, FontType num_font) -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    vector<string> result;

    if (cstr.empty()) {
        return {};
    }

    const auto* font = GetFont(num_font);

    if (font == nullptr) {
        return {};
    }

    auto& fi = _fontFormatInfoBuf = {.CurFont = font, .Format = TextFormat {}, .Rect = rect};
    fi.Prepare(cstr);
    StrCopy(fi.Str.data(), fi.Str.size(), cstr);
    fi.StrLines = &result;

    FormatText(fi, FORMAT_TYPE_SPLIT);

    if (fi.IsError) {
        return {};
    }

    return result;
}

auto FontManager::HaveLetter(FontType num_font, uint32_t letter) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    const auto* font = GetFont(num_font);

    if (font == nullptr) {
        return false;
    }

    return font->Letters.count(letter) != 0;
}

FO_END_NAMESPACE
