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

#include "Rendering.h"

FO_BEGIN_NAMESPACE

static void ValidateTextureRect(const RenderTexture& tex, ipos32 pos, isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(pos.x >= 0, "Position x is negative", pos.x);
    FO_VERIFY_AND_THROW(pos.y >= 0, "Position y is negative", pos.y);
    FO_VERIFY_AND_THROW(size.width >= 0, "Size width is negative", size.width);
    FO_VERIFY_AND_THROW(size.height >= 0, "Size height is negative", size.height);
    FO_VERIFY_AND_THROW(pos.x + size.width <= tex.Size.width, "Texture rectangle right edge is outside texture bounds", pos.x, size.width, tex.Size.width);
    FO_VERIFY_AND_THROW(pos.y + size.height <= tex.Size.height, "Texture rectangle bottom edge is outside texture bounds", pos.y, size.height, tex.Size.height);
}

static auto CalcTextureIndex(const RenderTexture& tex, ipos32 pos) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(pos.x >= 0, "Position x is negative", pos.x);
    FO_VERIFY_AND_THROW(pos.y >= 0, "Position y is negative", pos.y);
    FO_VERIFY_AND_THROW(pos.x < tex.Size.width, "Position x is outside allowed range", pos.x, tex.Size.width);
    FO_VERIFY_AND_THROW(pos.y < tex.Size.height, "Position y is outside allowed range", pos.y, tex.Size.height);

    return numeric_cast<size_t>(pos.y) * numeric_cast<size_t>(tex.Size.width) + numeric_cast<size_t>(pos.x);
}

static void ValidateScissorRect(irect32 rect)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(rect.x >= 0, "Rectangle x is negative", rect.x);
    FO_VERIFY_AND_THROW(rect.y >= 0, "Rectangle y is negative", rect.y);
    FO_VERIFY_AND_THROW(rect.width >= 0, "Rectangle width is negative", rect.width);
    FO_VERIFY_AND_THROW(rect.height >= 0, "Rectangle height is negative", rect.height);
}

static auto GetFallbackTextureSizeData() -> ptr<const float32_t>
{
    FO_STACK_TRACE_ENTRY();

    static constexpr float32_t FallbackSizeData[4] {1.0f, 1.0f, 1.0f, 1.0f};

    return FallbackSizeData;
}

class Null_Texture final : public RenderTexture
{
public:
    Null_Texture(isize32 size, bool linear_filtered, bool with_depth) :
        RenderTexture(size, linear_filtered, with_depth),
        _pixels(numeric_cast<size_t>(size.width) * numeric_cast<size_t>(size.height))
    {
        FO_STACK_TRACE_ENTRY();
    }

    [[nodiscard]] auto GetTexturePixel(ipos32 pos) const -> ucolor override
    {
        FO_STACK_TRACE_ENTRY();

        return _pixels[CalcTextureIndex(*this, pos)];
    }

    [[nodiscard]] auto GetTextureRegion(ipos32 pos, isize32 size) const -> vector<ucolor> override
    {
        FO_STACK_TRACE_ENTRY();

        ValidateTextureRect(*this, pos, size);

        vector<ucolor> result;
        result.resize(numeric_cast<size_t>(size.width) * numeric_cast<size_t>(size.height));

        for (int32_t y = 0; y < size.height; y++) {
            for (int32_t x = 0; x < size.width; x++) {
                const ipos32 src_pos {pos.x + x, pos.y + y};
                const size_t dst_index = numeric_cast<size_t>(y) * numeric_cast<size_t>(size.width) + numeric_cast<size_t>(x);
                result[dst_index] = _pixels[CalcTextureIndex(*this, src_pos)];
            }
        }

        return result;
    }

    void UpdateTextureRegion(ipos32 pos, isize32 size, const_span<ucolor> data, bool use_dest_pitch) override
    {
        FO_STACK_TRACE_ENTRY();

        ValidateTextureRect(*this, pos, size);

        if (size.width == 0 || size.height == 0) {
            return;
        }

        const int32_t pitch = use_dest_pitch ? Size.width : size.width;
        const size_t required_size = (numeric_cast<size_t>(size.height - 1) * numeric_cast<size_t>(pitch)) + numeric_cast<size_t>(size.width);
        FO_VERIFY_AND_THROW(data.size() >= required_size, "Texture update source data is smaller than the required region size");

        for (int32_t y = 0; y < size.height; y++) {
            for (int32_t x = 0; x < size.width; x++) {
                const ipos32 dst_pos {pos.x + x, pos.y + y};
                const size_t src_index = numeric_cast<size_t>(y) * numeric_cast<size_t>(pitch) + numeric_cast<size_t>(x);
                _pixels[CalcTextureIndex(*this, dst_pos)] = data[src_index];
            }
        }
    }

    void Clear(ucolor color)
    {
        FO_STACK_TRACE_ENTRY();

        std::fill(_pixels.begin(), _pixels.end(), color);
    }

private:
    vector<ucolor> _pixels;
};

class Null_DrawBuffer final : public RenderDrawBuffer
{
public:
    explicit Null_DrawBuffer(bool is_static) :
        RenderDrawBuffer(is_static)
    {
        FO_STACK_TRACE_ENTRY();
    }

    void Upload(EffectUsage usage, optional<size_t> custom_vertices_size, optional<size_t> custom_indices_size) override
    {
        FO_STACK_TRACE_ENTRY();

        if (IsStatic && !StaticDataChanged) {
            return;
        }

        const size_t upload_vertices = custom_vertices_size.value_or(VertCount);
        const size_t upload_indices = custom_indices_size.value_or(IndCount);

#if FO_ENABLE_3D
        if (usage == EffectUsage::Model) {
            FO_VERIFY_AND_THROW(Vertices.empty(), "Null renderer model upload received 2D vertices alongside 3D vertices", Vertices.size(), Vertices3D.size());
            FO_VERIFY_AND_THROW(upload_vertices <= Vertices3D.size(), "Null renderer model upload requested more 3D vertices than the draw buffer contains", upload_vertices, Vertices3D.size(), VertCount);
        }
        else {
            FO_VERIFY_AND_THROW(Vertices3D.empty(), "Null renderer 2D upload received 3D vertices alongside 2D vertices", Vertices3D.size(), Vertices.size());
            FO_VERIFY_AND_THROW(upload_vertices <= Vertices.size(), "Null renderer 2D upload requested more vertices than the draw buffer contains", upload_vertices, Vertices.size(), VertCount);
        }
#else
        ignore_unused(usage);
        FO_VERIFY_AND_THROW(upload_vertices <= Vertices.size(), "Null renderer upload requested more vertices than the draw buffer contains", upload_vertices, Vertices.size(), VertCount);
#endif

        FO_VERIFY_AND_THROW(upload_indices <= Indices.size(), "Null renderer upload requested more indices than the draw buffer contains", upload_indices, Indices.size(), IndCount);

        StaticDataChanged = false;
        _lastUploadedVertices = upload_vertices;
        _lastUploadedIndices = upload_indices;
    }

    [[nodiscard]] auto GetLastUploadedVertices() const -> size_t
    {
        FO_STACK_TRACE_ENTRY();

        return _lastUploadedVertices;
    }

    [[nodiscard]] auto GetLastUploadedIndices() const -> size_t
    {
        FO_STACK_TRACE_ENTRY();

        return _lastUploadedIndices;
    }

private:
    size_t _lastUploadedVertices {};
    size_t _lastUploadedIndices {};
};

static auto GetNullEffectConfig(string_view name, const RenderEffectLoader& loader) -> string
{
    FO_STACK_TRACE_ENTRY();

    const auto fofx_content = loader(name);

    if (!fofx_content.empty()) {
        return fofx_content;
    }

    if (name.ends_with("-info")) {
        return "[EffectInfo]\n";
    }

    return "[Effect]\nPasses = 1\n";
}

class Null_Effect final : public RenderEffect
{
public:
    Null_Effect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) :
        RenderEffect(usage, name, [&loader](string_view effect_name) { return GetNullEffectConfig(effect_name, loader); })
    {
        FO_STACK_TRACE_ENTRY();
    }

    void DrawBuffer(ptr<RenderDrawBuffer> dbuf, size_t start_index, optional<size_t> indices_to_draw, nptr<const RenderTexture> custom_tex) override
    {
        FO_STACK_TRACE_ENTRY();

        const size_t draw_indices = indices_to_draw.value_or(dbuf->IndCount - start_index);
        FO_VERIFY_AND_THROW(start_index <= dbuf->IndCount, "Draw buffer start index is outside index buffer bounds", start_index, dbuf->IndCount);
        FO_VERIFY_AND_THROW(draw_indices <= dbuf->IndCount - start_index, "Draw buffer index range is outside index buffer bounds", start_index, draw_indices, dbuf->IndCount);

#if FO_ENABLE_3D
        if (_usage == EffectUsage::Model) {
            FO_VERIFY_AND_THROW(dbuf->VertCount <= dbuf->Vertices3D.size(), "Null renderer model draw references more 3D vertices than the draw buffer contains", dbuf->VertCount, dbuf->Vertices3D.size(), start_index, draw_indices);
        }
        else {
            FO_VERIFY_AND_THROW(dbuf->VertCount <= dbuf->Vertices.size(), "Null renderer 2D draw references more vertices than the draw buffer contains", dbuf->VertCount, dbuf->Vertices.size(), start_index, draw_indices);
        }
#else
        FO_VERIFY_AND_THROW(dbuf->VertCount <= dbuf->Vertices.size(), "Null renderer draw references more vertices than the draw buffer contains", dbuf->VertCount, dbuf->Vertices.size(), start_index, draw_indices);
#endif

        if (_needMainTex) {
            if (!custom_tex) {
                custom_tex = MainTex;
            }

            if (_needMainTexBuf && !MainTexBuf.has_value()) {
                auto& main_tex_buf = MainTexBuf = MainTexBuffer();
                auto size_data = GetFallbackTextureSizeData();

                if (custom_tex) {
                    size_data = custom_tex->SizeData;
                }

                ptr<float32_t> main_texture_size = main_tex_buf->MainTexSize;
                MemCopy(main_texture_size, size_data, 4 * sizeof(float32_t));
            }
        }

        if (_needProjBuf && !ProjBuf.has_value()) {
            auto& proj_buf = ProjBuf = ProjBuffer();
            proj_buf->ProjMatrix[0] = 1.0f;
            proj_buf->ProjMatrix[5] = 1.0f;
            proj_buf->ProjMatrix[10] = 1.0f;
            proj_buf->ProjMatrix[15] = 1.0f;
        }
    }
};

auto Null_Renderer::CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture>
{
    FO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeUnique<Null_Texture>(size, linear_filtered, with_depth);
}

auto Null_Renderer::CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer>
{
    FO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeUnique<Null_DrawBuffer>(is_static);
}

auto Null_Renderer::CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect>
{
    FO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeUnique<Null_Effect>(usage, name, loader);
}

auto Null_Renderer::CreateOrthoMatrix(float32_t left, float32_t right, float32_t bottom, float32_t top, float32_t nearp, float32_t farp) const -> mat44
{
    FO_STACK_TRACE_ENTRY();

    const float32_t r_l = right - left;
    const float32_t t_b = top - bottom;
    const float32_t f_n = farp - nearp;
    const float32_t tx = -(right + left) / r_l;
    const float32_t ty = -(top + bottom) / t_b;
    const float32_t tz = -(farp + nearp) / f_n;

    mat44 result {1.0f};

    result[0][0] = 2.0f / r_l;
    result[1][0] = 0.0f;
    result[2][0] = 0.0f;
    result[3][0] = tx;

    result[0][1] = 0.0f;
    result[1][1] = 2.0f / t_b;
    result[2][1] = 0.0f;
    result[3][1] = ty;

    result[0][2] = 0.0f;
    result[1][2] = 0.0f;
    result[2][2] = -2.0f / f_n;
    result[3][2] = tz;

    result[0][3] = 0.0f;
    result[1][3] = 0.0f;
    result[2][3] = 0.0f;
    result[3][3] = 1.0f;

    return result;
}

auto Null_Renderer::GetViewPort() const -> irect32
{
    FO_STACK_TRACE_ENTRY();

    return _viewPortRect;
}

auto Null_Renderer::IsRenderTargetFlipped() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return false;
}

void Null_Renderer::Init(GlobalSettings& settings, nptr<WindowInternalHandle> window)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(window);

    FO_VERIFY_AND_THROW(settings.ScreenWidth > 0, "Settings screen width must be positive");
    FO_VERIFY_AND_THROW(settings.ScreenHeight > 0, "Settings screen height must be positive");

    _viewPortRect = {0, 0, settings.ScreenWidth, settings.ScreenHeight};
    _currentRenderTarget = nullptr;
    _scissorEnabled = false;
    _scissorRect = {};
}

void Null_Renderer::Present()
{
    FO_STACK_TRACE_ENTRY();
}

void Null_Renderer::SetRenderTarget(nptr<RenderTexture> tex)
{
    FO_STACK_TRACE_ENTRY();

    _currentRenderTarget = tex;

    if (tex) {
        _viewPortRect = {0, 0, tex->Size.width, tex->Size.height};
    }
}

void Null_Renderer::ClearRenderTarget(optional<ucolor> color, bool depth, bool stencil)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(depth);
    ignore_unused(stencil);

    if (!color.has_value() || !_currentRenderTarget) {
        return;
    }

    auto null_tex = _currentRenderTarget.dyn_cast<Null_Texture>();
    FO_VERIFY_AND_THROW(null_tex, "Missing required null texture");
    null_tex->Clear(color.value());
}

void Null_Renderer::EnableScissor(irect32 rect)
{
    FO_STACK_TRACE_ENTRY();

    ValidateScissorRect(rect);
    _scissorEnabled = true;
    _scissorRect = rect;
}

void Null_Renderer::DisableScissor()
{
    FO_STACK_TRACE_ENTRY();

    _scissorEnabled = false;
    _scissorRect = {};
}

void Null_Renderer::OnResizeWindow(isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(size.width > 0, "Size width must be positive", size.width);
    FO_VERIFY_AND_THROW(size.height > 0, "Size height must be positive", size.height);

    if (!_currentRenderTarget) {
        _viewPortRect = {0, 0, size.width, size.height};
    }
}

FO_END_NAMESPACE
