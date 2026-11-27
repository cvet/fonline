// FOnline Engine
// SPDX-License-Identifier: MIT

#include "FontRasterizer.h"

#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_MODULE_H
#include FT_BITMAP_H

FO_BEGIN_NAMESPACE

// FreeType keeps the memory record by pointer for the life of the library, so it lives beside the handle
struct FontRasterizer::Library
{
    FT_MemoryRec_ Memory {};
    unique_del_nptr<FT_LibraryRec_> Handle {};
};

struct FontFace::Impl
{
    shared_ptr<FontRasterizer::Library> Library {};
    shared_ptr<const vector<uint8_t>> FontData {};
    unique_del_nptr<FT_FaceRec_> Handle {};
    // FreeType takes a non-const face even for queries, so the const accessors call through this borrow
    mutable nptr<FT_FaceRec_> Face {};
    int32_t LoadFlags {};
    FT_Render_Mode RenderMode {};
    bool Embolden {};
    bool Colored {};
};

static auto FreeTypeAlloc(FT_Memory memory, long size) noexcept -> void*;
static void FreeTypeFree(FT_Memory memory, void* block) noexcept;
static auto FreeTypeRealloc(FT_Memory memory, long cur_size, long new_size, void* block) noexcept -> void*;
static auto GetFreeTypeErrorText(FT_Error error) -> string_view;
static auto RoundFreeTypePixels(FT_Pos value) -> int32_t;
static void CopyGlyphCoverage(const FT_Bitmap& bitmap, RasterizedGlyph& glyph);
static void CopyGlyphColors(const FT_Bitmap& bitmap, RasterizedGlyph& glyph);

FontRasterizer::FontRasterizer()
{
    FO_TRACE_ZONE(Gui);

    auto library = safe_alloc::make_shared<Library>();
    library->Memory.user = nullptr;
    library->Memory.alloc = &FreeTypeAlloc;
    library->Memory.free = &FreeTypeFree;
    library->Memory.realloc = &FreeTypeRealloc;

    FT_Library raw_library = nullptr;
    FT_Error error = FT_New_Library(&library->Memory, &raw_library);

    if (error != 0) {
        throw FontRasterizerException("FreeType library initialization failed", GetFreeTypeErrorText(error));
    }

    library->Handle = make_unique_del_ptr(make_nptr(raw_library), [](FT_LibraryRec_* raw_handle) noexcept {
        if (raw_handle != nullptr) {
            (void)FT_Done_Library(raw_handle);
        }
    });

    FT_Add_Default_Modules(library->Handle.get());
    _library = std::move(library);
}

FontRasterizer::~FontRasterizer() = default;

auto FontRasterizer::OpenFace(shared_ptr<const vector<uint8_t>> font_data, int32_t pixel_size, FontFaceHinting hinting, bool monochrome, bool force_auto_hint, bool embolden) -> unique_ptr<FontFace>
{
    FO_TRACE_ZONE(Gui);

    FO_VERIFY_AND_THROW(!font_data->empty(), "Font file must not be empty");
    FO_VERIFY_AND_THROW(pixel_size > 0, "Font pixel size must be positive", pixel_size);

    auto impl = safe_alloc::make_unique<FontFace::Impl>();
    impl->Library = _library;
    impl->FontData = std::move(font_data);

    FT_Face raw_face = nullptr;
    FT_Error error = FT_New_Memory_Face(_library->Handle.get(), impl->FontData->data(), numeric_cast<FT_Long>(impl->FontData->size()), 0, &raw_face);

    if (error != 0) {
        throw FontRasterizerException("FreeType could not open the font", GetFreeTypeErrorText(error));
    }

    impl->Handle = make_unique_del_ptr(make_nptr(raw_face), [](FT_FaceRec_* raw_handle) noexcept {
        if (raw_handle != nullptr) {
            (void)FT_Done_Face(raw_handle);
        }
    });

    impl->Face = impl->Handle.get();
    auto face = make_ptr(impl->Handle.get());

    if (FT_Select_Charmap(face.get(), FT_ENCODING_UNICODE) != 0) {
        throw FontRasterizerException("Font has no Unicode character map", face->family_name != nullptr ? string_view(face->family_name) : string_view());
    }

    error = FT_Set_Pixel_Sizes(face.get(), 0, numeric_cast<FT_UInt>(pixel_size));

    if (error != 0) {
        throw FontRasterizerException("Font cannot be set to the pixel size", pixel_size, GetFreeTypeErrorText(error));
    }

    // FreeType blends the COLR layers of a colored glyph into a BGRA bitmap, which exists only antialiased: the
    // monochrome, hinting and synthetic bold options describe one-color glyphs and are not applied to such a face
    impl->Colored = FT_HAS_COLOR(face.get());

    if (impl->Colored) {
        impl->LoadFlags = FT_LOAD_COLOR | FT_LOAD_NO_HINTING;
        impl->RenderMode = FT_RENDER_MODE_NORMAL;
        return safe_alloc::make_unique<FontFace>(std::move(impl));
    }

    switch (hinting) {
    case FontFaceHinting::Light:
        impl->LoadFlags = monochrome ? FT_LOAD_TARGET_MONO : FT_LOAD_TARGET_LIGHT;
        break;
    case FontFaceHinting::Normal:
        impl->LoadFlags = monochrome ? FT_LOAD_TARGET_MONO : FT_LOAD_TARGET_NORMAL;
        break;
    case FontFaceHinting::None:
        impl->LoadFlags = FT_LOAD_NO_HINTING;
        break;
    }

    if (force_auto_hint && hinting != FontFaceHinting::None) {
        impl->LoadFlags |= FT_LOAD_FORCE_AUTOHINT;
    }

    impl->RenderMode = monochrome ? FT_RENDER_MODE_MONO : FT_RENDER_MODE_NORMAL;
    impl->Embolden = embolden;

    return safe_alloc::make_unique<FontFace>(std::move(impl));
}

FontFace::FontFace(unique_ptr<Impl> impl) :
    _impl {std::move(impl)}
{
}

FontFace::~FontFace() = default;

auto FontFace::GetGlyphIndex(uint32_t codepoint) const -> uint32_t
{
    return numeric_cast<uint32_t>(FT_Get_Char_Index(_impl->Face.get(), numeric_cast<FT_ULong>(codepoint)));
}

auto FontFace::GetMetrics() const -> FontFaceMetrics
{
    const FT_Size_Metrics& size_metrics = _impl->Face->size->metrics;

    FontFaceMetrics metrics;
    metrics.Ascender = numeric_cast<int32_t>((size_metrics.ascender + 63) >> 6);
    metrics.Descender = numeric_cast<int32_t>(-(size_metrics.descender >> 6));
    metrics.LineGap = std::max(RoundFreeTypePixels(size_metrics.height) - metrics.Ascender - metrics.Descender, 0);
    return metrics;
}

auto FontFace::IsColored() const noexcept -> bool
{
    return _impl->Colored;
}

auto FontFace::GetKerning(uint32_t left_glyph, uint32_t right_glyph) const -> int32_t
{
    if (left_glyph == 0 || right_glyph == 0 || !FT_HAS_KERNING(_impl->Face)) {
        return 0;
    }

    FT_Vector kerning {};
    FT_Error error = FT_Get_Kerning(_impl->Face.get(), left_glyph, right_glyph, FT_KERNING_DEFAULT, &kerning);

    if (error != 0) {
        throw FontRasterizerException("FreeType could not read the kerning of a glyph pair", left_glyph, right_glyph, GetFreeTypeErrorText(error));
    }

    return RoundFreeTypePixels(kerning.x);
}

auto FontFace::RenderGlyph(uint32_t glyph_index) -> RasterizedGlyph
{
    FO_TRACE_ZONE(Gui);

    auto face = make_ptr(_impl->Face.get());
    FT_Error error = FT_Load_Glyph(face.get(), glyph_index, _impl->LoadFlags);

    if (error != 0) {
        throw FontRasterizerException("FreeType could not load a glyph", glyph_index, GetFreeTypeErrorText(error));
    }

    auto slot = make_ptr(face->glyph);

    if (slot->format != FT_GLYPH_FORMAT_BITMAP) {
        error = FT_Render_Glyph(slot.get(), _impl->RenderMode);

        if (error != 0) {
            throw FontRasterizerException("FreeType could not render a glyph", glyph_index, GetFreeTypeErrorText(error));
        }
    }

    if (_impl->Embolden && slot->bitmap.width != 0 && slot->bitmap.rows != 0) {
        error = FT_Bitmap_Embolden(_impl->Library->Handle.get(), &slot->bitmap, 64, 0);

        if (error != 0) {
            throw FontRasterizerException("FreeType could not embolden a glyph", glyph_index, GetFreeTypeErrorText(error));
        }
    }

    RasterizedGlyph glyph;
    glyph.Size = {numeric_cast<int32_t>(slot->bitmap.width), numeric_cast<int32_t>(slot->bitmap.rows)};
    glyph.Bearing = {numeric_cast<int32_t>(slot->bitmap_left), numeric_cast<int32_t>(slot->bitmap_top)};
    glyph.Advance = RoundFreeTypePixels(slot->advance.x) + (_impl->Embolden ? 1 : 0);
    CopyGlyphCoverage(slot->bitmap, glyph);

    if (slot->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
        CopyGlyphColors(slot->bitmap, glyph);
    }

    return glyph;
}

// Routed through the safe_alloc raw tier so FreeType gets the engine's out-of-memory handling
static auto FreeTypeAlloc(FT_Memory memory, long size) noexcept -> void*
{
    ignore_unused(memory);
    return safe_alloc::malloc_raw(numeric_cast<size_t>(size)).get();
}

static void FreeTypeFree(FT_Memory memory, void* block) noexcept
{
    ignore_unused(memory);
    safe_alloc::free_raw(block);
}

static auto FreeTypeRealloc(FT_Memory memory, long cur_size, long new_size, void* block) noexcept -> void*
{
    ignore_unused(memory, cur_size);
    return safe_alloc::realloc_raw(block, numeric_cast<size_t>(new_size)).get();
}

static auto GetFreeTypeErrorText(FT_Error error) -> string_view
{
    auto text = make_nptr(FT_Error_String(error));
    return text ? string_view(text.get()) : string_view("unknown FreeType error");
}

// FreeType's 26.6 fixed point is integer arithmetic, so the rounding is identical on every platform
static auto RoundFreeTypePixels(FT_Pos value) -> int32_t
{
    return numeric_cast<int32_t>((value + 32) >> 6);
}

static void CopyGlyphCoverage(const FT_Bitmap& bitmap, RasterizedGlyph& glyph)
{
    size_t width = numeric_cast<size_t>(bitmap.width);
    size_t rows = numeric_cast<size_t>(bitmap.rows);
    glyph.Coverage.resize(width * rows);

    if (width == 0 || rows == 0) {
        return;
    }

    auto buffer_ptr = make_nptr(bitmap.buffer);
    FO_VERIFY_AND_THROW(buffer_ptr, "Rendered glyph bitmap has no pixel buffer", bitmap.width, bitmap.rows);

    size_t pitch = numeric_cast<size_t>(std::abs(bitmap.pitch));
    auto buffer = make_span(buffer_ptr.as_ptr(), pitch * rows);

    for (size_t y = 0; y < rows; y++) {
        size_t source_row = bitmap.pitch >= 0 ? y : rows - 1 - y;
        auto row = buffer.subspan(source_row * pitch, pitch);
        auto coverage_row = glyph.Coverage.begin() + numeric_cast<ptrdiff_t>(y * width);

        switch (bitmap.pixel_mode) {
        case FT_PIXEL_MODE_MONO:
            for (size_t x = 0; x < width; x++) {
                bool is_set = (row[x / 8] & (0x80u >> (x % 8))) != 0;
                coverage_row[numeric_cast<ptrdiff_t>(x)] = is_set ? uint8_t {255} : uint8_t {0};
            }
            break;
        case FT_PIXEL_MODE_GRAY:
            FO_VERIFY_AND_THROW(bitmap.num_grays == 256, "Grey glyph bitmap must use 256 levels", bitmap.num_grays);
            std::copy_n(row.begin(), width, coverage_row);
            break;
        case FT_PIXEL_MODE_BGRA:
            for (size_t x = 0; x < width; x++) {
                coverage_row[numeric_cast<ptrdiff_t>(x)] = row[x * 4 + 3];
            }
            break;
        default:
            throw FontRasterizerException("Glyph bitmap has an unsupported pixel mode", bitmap.pixel_mode);
        }
    }
}

// FreeType hands out colored glyphs as premultiplied BGRA; the engine's glyph sheet keeps straight alpha
static void CopyGlyphColors(const FT_Bitmap& bitmap, RasterizedGlyph& glyph)
{
    size_t width = numeric_cast<size_t>(bitmap.width);
    size_t rows = numeric_cast<size_t>(bitmap.rows);
    glyph.Colors.assign(width * rows, ucolor::clear);

    if (width == 0 || rows == 0) {
        return;
    }

    auto buffer_ptr = make_nptr(bitmap.buffer);
    FO_VERIFY_AND_THROW(buffer_ptr, "Rendered glyph bitmap has no pixel buffer", bitmap.width, bitmap.rows);

    size_t pitch = numeric_cast<size_t>(std::abs(bitmap.pitch));
    auto buffer = make_span(buffer_ptr.as_ptr(), pitch * rows);

    for (size_t y = 0; y < rows; y++) {
        size_t source_row = bitmap.pitch >= 0 ? y : rows - 1 - y;
        auto row = buffer.subspan(source_row * pitch, pitch);

        for (size_t x = 0; x < width; x++) {
            uint32_t alpha = row[x * 4 + 3];

            if (alpha == 0) {
                continue;
            }

            auto unpremultiply = [alpha](uint8_t value) -> uint8_t { return numeric_cast<uint8_t>(std::min<uint32_t>((uint32_t {value} * 255 + alpha / 2) / alpha, 255)); };
            glyph.Colors[y * width + x] = ucolor {unpremultiply(row[x * 4 + 2]), unpremultiply(row[x * 4 + 1]), unpremultiply(row[x * 4 + 0]), numeric_cast<uint8_t>(alpha)};
        }
    }
}

FO_END_NAMESPACE
