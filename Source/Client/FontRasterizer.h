// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(FontRasterizerException);

enum class FontFaceHinting : uint8_t
{
    Light,
    Normal,
    None,
};

// Every value is in whole pixels at the face's pixel size, so text layout never accumulates fractions
struct FontFaceMetrics
{
    int32_t Ascender {};
    int32_t Descender {};
    int32_t LineGap {};
};

// Bearing is the bitmap's left edge from the pen and its top edge above the baseline; Coverage holds one alpha
// value per pixel, row by row
struct RasterizedGlyph
{
    isize32 Size {};
    ipos32 Bearing {};
    int32_t Advance {};
    vector<uint8_t> Coverage {};
};

class FontFace final
{
    friend class FontRasterizer;
    friend class safe_alloc;

public:
    FontFace() = delete;
    FontFace(const FontFace&) = delete;
    FontFace(FontFace&&) noexcept = delete;
    auto operator=(const FontFace&) = delete;
    auto operator=(FontFace&&) noexcept = delete;
    ~FontFace();

    [[nodiscard]] auto GetGlyphIndex(uint32_t codepoint) const -> uint32_t;
    [[nodiscard]] auto GetMetrics() const -> FontFaceMetrics;
    [[nodiscard]] auto GetKerning(uint32_t left_glyph, uint32_t right_glyph) const -> int32_t;

    auto RenderGlyph(uint32_t glyph_index) -> RasterizedGlyph;

private:
    struct Impl;

    explicit FontFace(unique_ptr<Impl> impl);

    unique_ptr<Impl> _impl;
};

class FontRasterizer final
{
public:
    // FreeType library state, owned jointly by the rasterizer and every face it opened
    struct Library;

    FontRasterizer();
    FontRasterizer(const FontRasterizer&) = delete;
    FontRasterizer(FontRasterizer&&) noexcept = delete;
    auto operator=(const FontRasterizer&) = delete;
    auto operator=(FontRasterizer&&) noexcept = delete;
    ~FontRasterizer();

    // The face reads the font file in place, so it keeps the data alive for as long as it lives
    auto OpenFace(shared_ptr<const vector<uint8_t>> font_data, int32_t pixel_size, FontFaceHinting hinting, bool monochrome, bool force_auto_hint = false, bool embolden = false) -> unique_ptr<FontFace>;

private:
    shared_ptr<Library> _library;
};

FO_END_NAMESPACE
