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

FontManager::FontManager(ptr<SpriteManager> spr_mngr) :
    _sprMngr {spr_mngr}
{
    FO_STACK_TRACE_ENTRY();
}

auto FontManager::GetFont(FontType font) -> ptr<FontData>
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

    return &*_allFonts[static_cast<size_t>(font)];
}

auto FontManager::GetFont(FontType font) const -> ptr<const FontData>
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

    return &*_allFonts[static_cast<size_t>(font)];
}

void FontManager::ClearFonts()
{
    FO_STACK_TRACE_ENTRY();

    _allFonts.clear();
    _formatCache.clear();
}

void FontManager::SetFontEffect(FontType font, nptr<RenderEffect> effect)
{
    FO_STACK_TRACE_ENTRY();

    GetFont(font)->DrawEffect = effect ? effect : _sprMngr->_effectMngr->Effects.Font;
}

void FontManager::StoreFont(int32_t index, FontData&& font_data)
{
    FO_STACK_TRACE_ENTRY();

    _formatCache.clear();

    if (index >= numeric_cast<int32_t>(_allFonts.size())) {
        _allFonts.resize(index + 1);
    }

    _allFonts[index].emplace(std::move(font_data));
    BuildFont(index);
    _formatCache.clear();
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

    auto font = make_ptr(&*_allFonts[index]);

    auto atlas_spr = font->ImageNormal;
    FO_VERIFY_AND_THROW(atlas_spr, "Atlas sprite is null");
    float32_t tex_w = numeric_cast<float32_t>(atlas_spr->GetAtlas()->GetSize().width);
    float32_t tex_h = numeric_cast<float32_t>(atlas_spr->GetAtlas()->GetSize().height);
    float32_t image_x = tex_w * atlas_spr->GetAtlasRect().x;
    float32_t image_y = tex_h * atlas_spr->GetAtlasRect().y;

    int32_t normal_ox = iround<int32_t>(tex_w * atlas_spr->GetAtlasRect().x);
    int32_t normal_oy = iround<int32_t>(tex_h * atlas_spr->GetAtlasRect().y);
    int32_t bordered_ox = font->ImageBordered ? iround<int32_t>(numeric_cast<float32_t>(font->ImageBordered->GetAtlas()->GetSize().width) * font->ImageBordered->GetAtlasRect().x) : 0;
    int32_t bordered_oy = font->ImageBordered ? iround<int32_t>(numeric_cast<float32_t>(font->ImageBordered->GetAtlas()->GetSize().height) * font->ImageBordered->GetAtlasRect().y) : 0;

    // Read texture data
    auto pixel_at = [](vector<ucolor>& tex_data, int32_t width, int32_t x, int32_t y) -> ptr<ucolor> { return make_ptr(&tex_data[y * width + x]); };
    vector<ucolor> data_normal = atlas_spr->GetAtlas()->GetTexture()->GetTextureRegion({normal_ox, normal_oy}, atlas_spr->GetSize());
    vector<ucolor> data_bordered;

    if (font->ImageBordered) {
        data_bordered = font->ImageBordered->GetAtlas()->GetTexture()->GetTextureRegion({bordered_ox, bordered_oy}, font->ImageBordered->GetSize());
    }

    // Bake the bound scale: downscale glyph bitmaps in place and round every metric to integers at
    // the target size, so the whole text pipeline keeps working in plain unscaled integer coordinates
    bool scale_baked = font->BakeScale != 1.0f;

    if (scale_baked) {
        BakeFontScale(*font, data_normal, atlas_spr->GetSize());

        // The bordered sheet is a second copy of the same image; border pixels are dilated over it below
        if (font->ImageBordered) {
            data_bordered = data_normal;
        }
    }

    // Fix texture coordinates
    int32_t max_h = 0;

    for (auto& letter : font->Letters | std::views::values) {
        float32_t x = numeric_cast<float32_t>(letter.Pos.x);
        float32_t y = numeric_cast<float32_t>(letter.Pos.y);
        float32_t w = numeric_cast<float32_t>(letter.Size.width);
        float32_t h = numeric_cast<float32_t>(letter.Size.height);

        letter.TexPos.x = (image_x + x - 1.0f) / tex_w;
        letter.TexPos.y = (image_y + y - 1.0f) / tex_h;
        letter.TexPos.width = (w + 2.0f) / tex_w;
        letter.TexPos.height = (h + 2.0f) / tex_h;

        max_h = std::max(letter.Size.height, max_h);
    }

    // Fill data
    font->FontTex = atlas_spr->GetAtlas()->GetTexture();

    if (font->LineHeight == 0) {
        font->LineHeight = max_h;
    }
    if (font->Letters.count(numeric_cast<uint32_t>(' ')) != 0) {
        font->SpaceWidth = font->Letters[numeric_cast<uint32_t>(' ')].XAdvance;
    }

    font->FontTexBordered = nullptr;

    if (font->ImageBordered) {
        font->FontTexBordered = font->ImageBordered->GetAtlas()->GetTexture();
    }

    // Normalize color to gray
    if (font->MakeGray) {
        for (int32_t y = 0; y < atlas_spr->GetSize().height; y++) {
            for (int32_t x = 0; x < atlas_spr->GetSize().width; x++) {
                uint8_t a = pixel_at(data_normal, atlas_spr->GetSize().width, x, y)->comp.a;

                if (a != 0) {
                    *pixel_at(data_normal, atlas_spr->GetSize().width, x, y) = ucolor {128, 128, 128, a};

                    if (font->ImageBordered) {
                        *pixel_at(data_bordered, font->ImageBordered->GetSize().width, x, y) = ucolor {128, 128, 128, a};
                    }
                }
                else {
                    *pixel_at(data_normal, atlas_spr->GetSize().width, x, y) = ucolor {0, 0, 0, 0};

                    if (font->ImageBordered) {
                        *pixel_at(data_bordered, font->ImageBordered->GetSize().width, x, y) = ucolor {0, 0, 0, 0};
                    }
                }
            }
        }
    }

    if (font->MakeGray || scale_baked) {
        atlas_spr->GetAtlas()->GetTexture()->UpdateTextureRegion({normal_ox, normal_oy}, atlas_spr->GetSize(), data_normal);
    }

    // Fill border
    if (font->ImageBordered) {
        for (int32_t y = 1; y < font->ImageBordered->GetSize().height - 2; y++) {
            for (int32_t x = 1; x < font->ImageBordered->GetSize().width - 2; x++) {
                if (*pixel_at(data_normal, atlas_spr->GetSize().width, x, y) != ucolor::clear) {
                    for (int32_t xx = -1; xx <= 1; xx++) {
                        for (int32_t yy = -1; yy <= 1; yy++) {
                            int32_t ox = x + xx;
                            int32_t oy = y + yy;

                            if (*pixel_at(data_bordered, font->ImageBordered->GetSize().width, ox, oy) == ucolor::clear) {
                                *pixel_at(data_bordered, font->ImageBordered->GetSize().width, ox, oy) = ucolor {0, 0, 0, 255};
                            }
                        }
                    }
                }
            }
        }

        font->ImageBordered->GetAtlas()->GetTexture()->UpdateTextureRegion({bordered_ox, bordered_oy}, font->ImageBordered->GetSize(), data_bordered);

        // Fix texture coordinates on bordered texture
        tex_w = numeric_cast<float32_t>(font->ImageBordered->GetAtlas()->GetSize().width);
        tex_h = numeric_cast<float32_t>(font->ImageBordered->GetAtlas()->GetSize().height);
        image_x = tex_w * font->ImageBordered->GetAtlasRect().x;
        image_y = tex_h * font->ImageBordered->GetAtlasRect().y;

        for (auto& letter : font->Letters | std::views::values) {
            float32_t x = numeric_cast<float32_t>(letter.Pos.x);
            float32_t y = numeric_cast<float32_t>(letter.Pos.y);
            float32_t w = numeric_cast<float32_t>(letter.Size.width);
            float32_t h = numeric_cast<float32_t>(letter.Size.height);
            letter.TexBorderedPos.x = (image_x + x - 1.0f) / tex_w;
            letter.TexBorderedPos.y = (image_y + y - 1.0f) / tex_h;
            letter.TexBorderedPos.width = (w + 2.0f) / tex_w;
            letter.TexBorderedPos.height = (h + 2.0f) / tex_h;
        }
    }
}

void FontManager::BakeFontScale(FontData& font, vector<ucolor>& sheet_data, isize32 sheet_size)
{
    FO_STACK_TRACE_ENTRY();

    float32_t scale = font.BakeScale;
    auto scale_value = [scale](int32_t value) -> int32_t { return iround<int32_t>(numeric_cast<float32_t>(value) * scale); };

    for (auto& letter : font.Letters | std::views::values) {
        letter.Offset.x = scale_value(letter.Offset.x);
        letter.Offset.y = scale_value(letter.Offset.y);
        letter.XAdvance = scale_value(letter.XAdvance);

        int32_t src_w = letter.Size.width;
        int32_t src_h = letter.Size.height;

        if (src_w <= 0 || src_h <= 0) {
            continue;
        }

        FO_VERIFY_AND_THROW(letter.Pos.x >= 0 && letter.Pos.y >= 0 && letter.Pos.x + src_w <= sheet_size.width && letter.Pos.y + src_h <= sheet_size.height, "Font letter rect is out of the font image bounds", letter.Pos.x, letter.Pos.y, src_w, src_h, sheet_size);

        int32_t dst_w = std::max(scale_value(src_w), 1);
        int32_t dst_h = std::max(scale_value(src_h), 1);

        // Area-average resample with alpha-weighted color, so antialiased glyph edges keep their tone.
        // Sampling stays inside the letter rect, so tightly packed neighbor glyphs never bleed in.
        vector<ucolor> scaled_pixels(numeric_cast<size_t>(dst_w) * numeric_cast<size_t>(dst_h));
        float32_t x_ratio = numeric_cast<float32_t>(src_w) / numeric_cast<float32_t>(dst_w);
        float32_t y_ratio = numeric_cast<float32_t>(src_h) / numeric_cast<float32_t>(dst_h);

        for (int32_t dy = 0; dy < dst_h; dy++) {
            for (int32_t dx = 0; dx < dst_w; dx++) {
                float32_t sx_begin = x_ratio * numeric_cast<float32_t>(dx);
                float32_t sx_end = x_ratio * numeric_cast<float32_t>(dx + 1);
                float32_t sy_begin = y_ratio * numeric_cast<float32_t>(dy);
                float32_t sy_end = y_ratio * numeric_cast<float32_t>(dy + 1);

                float32_t weight_sum = 0.0f;
                float32_t alpha_sum = 0.0f;
                float32_t red_sum = 0.0f;
                float32_t green_sum = 0.0f;
                float32_t blue_sum = 0.0f;

                for (int32_t sy = iround<int32_t>(std::floor(sy_begin)); sy < src_h; sy++) {
                    float32_t cover_y = std::min(numeric_cast<float32_t>(sy + 1), sy_end) - std::max(numeric_cast<float32_t>(sy), sy_begin);

                    if (cover_y <= 0.0f) {
                        break;
                    }

                    for (int32_t sx = iround<int32_t>(std::floor(sx_begin)); sx < src_w; sx++) {
                        float32_t cover_x = std::min(numeric_cast<float32_t>(sx + 1), sx_end) - std::max(numeric_cast<float32_t>(sx), sx_begin);

                        if (cover_x <= 0.0f) {
                            break;
                        }

                        float32_t weight = cover_x * cover_y;
                        ucolor src_pixel = sheet_data[numeric_cast<size_t>(letter.Pos.y + sy) * numeric_cast<size_t>(sheet_size.width) + numeric_cast<size_t>(letter.Pos.x + sx)];
                        float32_t alpha_weight = numeric_cast<float32_t>(src_pixel.comp.a) * weight;

                        weight_sum += weight;
                        alpha_sum += alpha_weight;
                        red_sum += numeric_cast<float32_t>(src_pixel.comp.r) * alpha_weight;
                        green_sum += numeric_cast<float32_t>(src_pixel.comp.g) * alpha_weight;
                        blue_sum += numeric_cast<float32_t>(src_pixel.comp.b) * alpha_weight;
                    }
                }

                ucolor& dst_pixel = scaled_pixels[numeric_cast<size_t>(dy) * numeric_cast<size_t>(dst_w) + numeric_cast<size_t>(dx)];

                if (alpha_sum > 0.0f && weight_sum > 0.0f) {
                    dst_pixel.comp.r = iround<uint8_t>(std::min(red_sum / alpha_sum, 255.0f));
                    dst_pixel.comp.g = iround<uint8_t>(std::min(green_sum / alpha_sum, 255.0f));
                    dst_pixel.comp.b = iround<uint8_t>(std::min(blue_sum / alpha_sum, 255.0f));
                    dst_pixel.comp.a = iround<uint8_t>(std::min(alpha_sum / weight_sum, 255.0f));
                }
                else {
                    dst_pixel = ucolor::clear;
                }
            }
        }

        // Clear the original rect, then place the scaled glyph at the same top-left corner: the freed
        // space only widens the transparent gap around every neighbor glyph
        for (int32_t y = 0; y < src_h; y++) {
            size_t row_begin = numeric_cast<size_t>(letter.Pos.y + y) * numeric_cast<size_t>(sheet_size.width) + numeric_cast<size_t>(letter.Pos.x);
            std::fill_n(sheet_data.begin() + numeric_cast<ptrdiff_t>(row_begin), src_w, ucolor::clear);
        }
        for (int32_t y = 0; y < dst_h; y++) {
            size_t dst_row_begin = numeric_cast<size_t>(letter.Pos.y + y) * numeric_cast<size_t>(sheet_size.width) + numeric_cast<size_t>(letter.Pos.x);
            size_t src_row_begin = numeric_cast<size_t>(y) * numeric_cast<size_t>(dst_w);
            std::copy_n(scaled_pixels.begin() + numeric_cast<ptrdiff_t>(src_row_begin), dst_w, sheet_data.begin() + numeric_cast<ptrdiff_t>(dst_row_begin));
        }

        letter.Size = {dst_w, dst_h};
    }

    if (font.LineHeight != 0) {
        font.LineHeight = std::max(scale_value(font.LineHeight), 1);
    }
    if (font.SpaceWidth != 0) {
        font.SpaceWidth = std::max(scale_value(font.SpaceWidth), 1);
    }

    font.YAdvance = scale_value(font.YAdvance);
}

auto FontManager::ResolveFontScale(float32_t scale) -> float32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(std::isfinite(scale) && scale > 0.0f && scale <= 1.0f, "Font scale must be in range (0..1] - author a bigger font asset for larger text", scale);
    return scale;
}

void FontManager::BindFoFont(FontType font, string_view font_path, AtlasType atlas_type, bool not_bordered, bool skip_if_loaded, float32_t default_scale)
{
    FO_STACK_TRACE_ENTRY();

    int32_t index = static_cast<int32_t>(font);
    FO_VERIFY_AND_THROW(index >= 0, "Font index must not be negative", index);

    // Skip if loaded
    if (skip_if_loaded && index < numeric_cast<int32_t>(_allFonts.size()) && _allFonts[index]) {
        return;
    }

    // Load font data
    auto file = _sprMngr->_resources->ReadFile(font_path);

    if (!file) {
        throw FontManagerException("Font file not found", font_path);
    }

    FontData font_data;
    font_data.DrawEffect = _sprMngr->_effectMngr->Effects.Font;
    font_data.BakeScale = ResolveFontScale(default_scale);

    string image_name;

    // Parse data
    istringstream str(file.GetStr());
    string key;
    string letter_buf;
    nptr<FontData::Letter> cur_letter;
    int32_t version = -1;

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
            str >> font_data.LineHeight;
        }
        else if (key == "YAdvance") {
            str >> font_data.YAdvance;
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
            FO_STRONG_ASSERT(utf8_letter_begin <= letter_buf.size(), "String offset is past the end of the string");
            auto letter_pos = make_ptr(letter_buf.c_str() + utf8_letter_begin);
            uint32_t letter = utf8::Decode(letter_pos, letter_len);

            if (!utf8::IsValid(letter)) {
                throw FontManagerException("Invalid UTF-8 letter", font_path, letter_buf);
            }

            cur_letter = &font_data.Letters[letter];
        }

        if (!cur_letter) {
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

    font_data.MakeGray = make_gray;

    // Load image
    {
        image_name = strex(font_path).extract_dir().combine_path(image_name);

        font_data.ImageNormal = _sprMngr->LoadSpriteAsQuad(_sprMngr->_hashResolver->ToHashedString(image_name), atlas_type);

        if (!font_data.ImageNormal) {
            throw FontManagerException("Font image file not found", font_path, image_name);
        }
    }

    // Create bordered instance
    if (!not_bordered) {
        font_data.ImageBordered = _sprMngr->LoadSpriteAsQuad(_sprMngr->_hashResolver->ToHashedString(image_name), atlas_type);

        if (!font_data.ImageBordered) {
            throw FontManagerException("Can't load font image twice", font_path, image_name);
        }
    }

    StoreFont(index, std::move(font_data));
}

void FontManager::BindBmfFont(FontType font, string_view font_path, AtlasType atlas_type, float32_t default_scale)
{
    FO_STACK_TRACE_ENTRY();

    int32_t index = static_cast<int32_t>(font);
    FO_VERIFY_AND_THROW(index >= 0, "Font index must not be negative", index);

    FontData font_data;
    font_data.DrawEffect = _sprMngr->_effectMngr->Effects.Font;
    font_data.BakeScale = ResolveFontScale(default_scale);

    auto file = _sprMngr->_resources->ReadFile(font_path);

    if (!file) {
        throw FontManagerException("Font file not found", font_path);
    }

    auto reader = file.GetReader();

    uint32_t signature = reader.GetLEUInt32();
    auto make_signature = [](uint8_t ch0, uint8_t ch1, uint8_t ch2, uint8_t ch3) -> uint32_t { return ch0 | ch1 << 8 | ch2 << 16 | ch3 << 24; };

    if (signature != make_signature('B', 'M', 'F', 3)) {
        throw FontManagerException("Invalid font signature", font_path);
    }

    // Info
    reader.GoForward(1);
    uint32_t block_len = reader.GetLEUInt32();
    size_t next_block = block_len + reader.GetCurPos() + 1;

    reader.GoForward(7);

    if (reader.GetBEUInt32() != 0x01010101u) {
        throw FontManagerException("Wrong padding in font", font_path);
    }

    // Common
    reader.SetCurPos(next_block);
    block_len = reader.GetLEUInt32();
    next_block = block_len + reader.GetCurPos() + 1;

    int32_t line_height = reader.GetLEUInt16();
    int32_t base_height = reader.GetLEUInt16();
    reader.GoForward(2); // Texture width
    reader.GoForward(2); // Texture height

    if (reader.GetLEUInt16() != 1) {
        throw FontManagerException("Font must have exactly one texture", font_path);
    }

    // Pages
    reader.SetCurPos(next_block);
    block_len = reader.GetLEUInt32();
    next_block = block_len + reader.GetCurPos() + 1;

    string image_name = reader.GetStrNT();
    image_name = strex(font_path).extract_dir().combine_path(image_name);

    // Chars
    reader.SetCurPos(next_block);
    uint32_t count = reader.GetLEUInt32() / 20;

    for (uint32_t i = 0; i < count; i++) {
        // Read data
        uint32_t id = reader.GetLEUInt32();
        uint16_t x = reader.GetLEUInt16();
        uint16_t y = reader.GetLEUInt16();
        uint16_t w = reader.GetLEUInt16();
        uint16_t h = reader.GetLEUInt16();
        uint16_t ox = reader.GetLEUInt16();
        uint16_t oy = reader.GetLEUInt16();
        uint16_t xa = reader.GetLEUInt16();
        reader.GoForward(2);

        // Fill data
        auto& let = font_data.Letters[id];
        let.Pos.x = x + 1;
        let.Pos.y = y + 1;
        let.Size.width = w - 2;
        let.Size.height = h - 2;
        let.Offset.x = -numeric_cast<int32_t>(ox);
        let.Offset.y = -numeric_cast<int32_t>(oy) + (line_height - base_height);
        let.XAdvance = xa + 1;
    }

    font_data.LineHeight = font_data.Letters.count(numeric_cast<uint32_t>('W')) != 0 ? font_data.Letters[numeric_cast<uint32_t>('W')].Size.height : base_height;
    font_data.YAdvance = font_data.LineHeight / 2;
    font_data.MakeGray = true;

    // Load image
    {
        font_data.ImageNormal = _sprMngr->LoadSpriteAsQuad(_sprMngr->_hashResolver->ToHashedString(image_name), atlas_type);

        if (!font_data.ImageNormal) {
            throw FontManagerException("Font image file not found", font_path, image_name);
        }
    }

    // Create bordered instance
    {
        font_data.ImageBordered = _sprMngr->LoadSpriteAsQuad(_sprMngr->_hashResolver->ToHashedString(image_name), atlas_type);

        if (!font_data.ImageBordered) {
            throw FontManagerException("Can't load font image twice", font_path, image_name);
        }
    }

    StoreFont(index, std::move(font_data));
}

void FontManager::FormatText(FontFormatInfo& fi, FormatMode mode) const
{
    FO_STACK_TRACE_ENTRY();

    string& str = fi.Text;
    auto flags = fi.Format.Flags;
    auto font = fi.CurFont;
    FO_VERIFY_AND_THROW(font, "Font is null");
    irect32 r = fi.Rect;
    bool infinity_w = r.width == 0;
    bool infinity_h = r.height == 0;
    int32_t curx = 0;
    int32_t cury = 0;
    int32_t& color_offset = fi.ColorOffset;

    // Colorize: strip `@color:...@` markers and write parsed colors into TextColor.
    nptr<ucolor> dots;
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
        size_t marker_pos = str.find(InlineColorTagPrefix, pos);

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
    bool skip_from_bottom = IsEnumSet(flags, FontFlag::AlignBottom);
    int32_t skip_line = skip_from_bottom ? 0 : fi.Format.SkipLines;
    int32_t skip_line_end = skip_from_bottom ? fi.Format.SkipLines : 0;

    // Format
    curx = r.x;
    cury = r.y;

    for (int32_t i = 0, i_advance = 1; i < numeric_cast<int32_t>(str.size()); i += i_advance) {
        size_t letter_len = utf8::DecodeStrNtLen(&str[numeric_cast<size_t>(i)]);
        uint32_t letter = utf8::Decode(&str[numeric_cast<size_t>(i)], letter_len);
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

                int32_t erased = j - i;
                str.erase(numeric_cast<size_t>(i), numeric_cast<size_t>(erased));
                letter = i < numeric_cast<int32_t>(str.size()) ? numeric_cast<uint8_t>(str[i]) : 0;
                i_advance = 1;

                if (mode == FormatMode::Draw) {
                    int32_t text_color_end = numeric_cast<int32_t>(fi.TextColor.size()) - erased;

                    for (int32_t k = i; k < text_color_end; k++) {
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
                        for (int32_t k = numeric_cast<int32_t>(fi.TextColor.size()) - 1; k > i; k--) {
                            fi.TextColor[k] = fi.TextColor[k - 1];
                        }
                    }
                }

                if (IsEnumSet(flags, FontFlag::Justify) && skip_line == 0) {
                    fi.LineSpaceWidth[fi.LinesAll - 1] = 1;

                    // Erase next first spaces
                    int32_t ii = i + i_advance;
                    j = ii;

                    while (j < numeric_cast<int32_t>(str.size()) && str[j] == ' ') {
                        j++;
                    }

                    if (j > ii) {
                        int32_t erased = j - ii;
                        str.erase(numeric_cast<size_t>(ii), numeric_cast<size_t>(erased));

                        if (mode == FormatMode::Draw) {
                            int32_t text_color_end = numeric_cast<int32_t>(fi.TextColor.size()) - erased;

                            for (int32_t k = ii; k < text_color_end; k++) {
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
    if (IsEnumSet(flags, FontFlag::KeepTail) && fi.LinesAll > fi.LinesInRect) {
        int32_t j = 0;
        int32_t line_cur = 0;
        ucolor last_color;
        int32_t str_size = numeric_cast<int32_t>(str.size());

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

    for (int32_t i = 0; i < fi.LinesAll; i++) {
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

        size_t letter_len = utf8::DecodeStrNtLen(&str[numeric_cast<size_t>(i)]);
        uint32_t letter = utf8::Decode(&str[numeric_cast<size_t>(i)], letter_len);
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
            if (auto it = font->Letters.find(letter); it != font->Letters.end()) {
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

    size_t close_pos = str.find('@', marker_pos + InlineColorTagPrefix.size());

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

auto FontManager::GetOrFormat(TextFormat format, FontType font, irect32 rect, ucolor color, FormatMode mode, string_view str) const -> ptr<const FontFormatInfo>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(static_cast<size_t>(font) < _allFonts.size(), "Text formatting requested a font index outside the loaded font table", font, _allFonts.size(), str.size(), rect);
    FO_VERIFY_AND_THROW(format.SkipLines >= 0, "Text format skip-line count must not be negative");
    FO_VERIFY_AND_THROW(rect.width >= 0, "Text layout rectangle width must not be negative", rect.width);
    FO_VERIFY_AND_THROW(rect.height >= 0, "Text layout rectangle height must not be negative", rect.height);

    std::array<uint64_t, 8> key_parts {
        HashStorage::DefaultHash(make_const_span(str)),
        static_cast<uint32_t>(font),
        static_cast<uint32_t>(format.Flags),
        static_cast<uint32_t>(format.SkipLines),
        static_cast<uint32_t>(rect.width),
        static_cast<uint32_t>(rect.height),
        color.underlying_value(),
        static_cast<uint32_t>(mode),
    };

    uint64_t key = HashStorage::DefaultHash(make_span(key_parts));

    if (auto it = _formatCache.find(key); it != _formatCache.end()) {
        it->second->LastUsedFrame = _frameIndex;
        return it->second;
    }

    auto str_len = str.length();
    auto max_chars = std::max<size_t>(str_len * 2 + 1, 1);
    auto max_lines = std::max<size_t>(str_len + 1, 1);

    auto fi = SafeAlloc::MakeUnique<FontFormatInfo>();
    FO_VERIFY_AND_THROW(_allFonts[static_cast<size_t>(font)], "Requested font is not loaded");
    fi->CurFont = &*_allFonts[static_cast<size_t>(font)];
    fi->Format = format;
    fi->Rect = irect32 {0, 0, rect.width, rect.height};
    fi->Color = color;
    fi->Text.assign(str);
    fi->TextColor.assign(max_chars, ucolor::clear);
    fi->LineWidth.assign(max_lines, 0);
    fi->LineSpaceWidth.assign(max_lines, 0);
    fi->LastUsedFrame = _frameIndex;

    FormatText(*fi, mode);

    auto fi_ptr = fi.as_ptr();
    _formatCache.emplace(key, std::move(fi));
    return fi_ptr;
}

void FontManager::DrawText(irect32 rect, string_view str, ucolor color, TextFormat format)
{
    FO_STACK_TRACE_ENTRY();

    if (str.empty()) {
        return;
    }

    auto font = GetFont(format.Font);
    color = _sprMngr->ApplyColorBrightness(color);
    auto fi = GetOrFormat(format, format.Font, rect, color, FormatMode::Draw, str);

    auto flags = format.Flags;
    const string& format_str = fi->Text;
    int32_t color_offset = fi->ColorOffset;
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

    size_t str_len = format_str.size();
    _sprMngr->_spritesDrawBuf->CheckAllocBuf(str_len * 4, str_len * 6);

    auto& vbuf = _sprMngr->_spritesDrawBuf->Vertices;
    size_t& vpos = _sprMngr->_spritesDrawBuf->VertCount;
    auto& ibuf = _sprMngr->_spritesDrawBuf->Indices;
    size_t& ipos = _sprMngr->_spritesDrawBuf->IndCount;

    size_t start_ipos = ipos;

    bool variable_space = false;
    int32_t i_advance;

    for (int32_t i = 0; i < numeric_cast<int32_t>(format_str.size()); i += i_advance) {
        if (!IsEnumSet(flags, FontFlag::NoColorize)) {
            auto new_color = fi->TextColor[i + color_offset];

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

        size_t letter_len = utf8::DecodeStrNtLen(&format_str[numeric_cast<size_t>(i)]);
        uint32_t letter = utf8::Decode(&format_str[numeric_cast<size_t>(i)], letter_len);
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
            auto it = font->Letters.find(letter);

            if (it == font->Letters.end()) {
                continue;
            }

            const auto& l = it->second;

            float32_t x = numeric_cast<float32_t>(curx - l.Offset.x - 1);
            float32_t y = numeric_cast<float32_t>(cury - l.Offset.y - 1);
            float32_t w = numeric_cast<float32_t>(l.Size.width + 2);
            float32_t h = numeric_cast<float32_t>(l.Size.height + 2);

            const frect32& texture_uv = IsEnumSet(flags, FontFlag::Bordered) ? l.TexBorderedPos : l.TexPos;
            float32_t x1 = texture_uv.x;
            float32_t y1 = texture_uv.y;
            float32_t x2 = texture_uv.x + texture_uv.width;
            float32_t y2 = texture_uv.y + texture_uv.height;

            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 0);
            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 1);
            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 3);
            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 1);
            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 2);
            ibuf[ipos++] = numeric_cast<vindex_t>(vpos + 3);

            auto& v0 = vbuf[vpos++];
            v0.PosX = x;
            v0.PosY = y + h;
            v0.PosZ = 0.0f;
            v0.TexU = x1;
            v0.TexV = y2;
            v0.EggFlags[0] = 0.0f;
            v0.EggFlags[1] = 0.0f;
            v0.Color = color;

            auto& v1 = vbuf[vpos++];
            v1.PosX = x;
            v1.PosY = y;
            v1.PosZ = 0.0f;
            v1.TexU = x1;
            v1.TexV = y1;
            v1.EggFlags[0] = 0.0f;
            v1.EggFlags[1] = 0.0f;
            v1.Color = color;

            auto& v2 = vbuf[vpos++];
            v2.PosX = x + w;
            v2.PosY = y;
            v2.PosZ = 0.0f;
            v2.TexU = x2;
            v2.TexV = y1;
            v2.EggFlags[0] = 0.0f;
            v2.EggFlags[1] = 0.0f;
            v2.Color = color;

            auto& v3 = vbuf[vpos++];
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

    size_t ind_count = ipos - start_ipos;

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

    auto font = GetFont(num_font);

    if (str.empty()) {
        return size.height / (font->LineHeight + font->YAdvance);
    }

    TextFormat format;
    format.Font = num_font;
    auto fi = GetOrFormat(format, num_font, irect32 {0, 0, size.width, size.height}, ucolor {}, FormatMode::LineCount, str);
    return fi->LinesInRect;
}

auto FontManager::GetLinesHeight(isize32 size, string_view str, FontType num_font) const -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    if (size.width <= 0 || size.height <= 0) {
        return 0;
    }

    auto font = GetFont(num_font);
    int32_t lines_count = GetLinesCount(size, str, num_font);

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

    auto font = GetFont(format.Font);

    if (str.empty()) {
        return true;
    }

    irect32 rect = irect32 {0, 0, size};
    auto fi = GetOrFormat(format, format.Font, rect, ucolor {}, FormatMode::LineCount, str);

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

    TextFormat format;
    format.Font = num_font;
    auto fi = GetOrFormat(format, num_font, rect, ucolor {}, FormatMode::Split, cstr);
    return fi->Lines;
}

auto FontManager::HaveLetter(FontType num_font, uint32_t letter) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return GetFont(num_font)->Letters.count(letter) != 0;
}

FO_END_NAMESPACE
