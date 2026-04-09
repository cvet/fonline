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

    FO_RUNTIME_ASSERT(pos.x >= 0);
    FO_RUNTIME_ASSERT(pos.y >= 0);
    FO_RUNTIME_ASSERT(size.width >= 0);
    FO_RUNTIME_ASSERT(size.height >= 0);
    FO_RUNTIME_ASSERT(pos.x + size.width <= tex.Size.width);
    FO_RUNTIME_ASSERT(pos.y + size.height <= tex.Size.height);
}

static auto CalcTextureIndex(const RenderTexture& tex, ipos32 pos) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(pos.x >= 0);
    FO_RUNTIME_ASSERT(pos.y >= 0);
    FO_RUNTIME_ASSERT(pos.x < tex.Size.width);
    FO_RUNTIME_ASSERT(pos.y < tex.Size.height);

    return numeric_cast<size_t>(pos.y) * numeric_cast<size_t>(tex.Size.width) + numeric_cast<size_t>(pos.x);
}

static void ValidateScissorRect(irect32 rect)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(rect.x >= 0);
    FO_RUNTIME_ASSERT(rect.y >= 0);
    FO_RUNTIME_ASSERT(rect.width >= 0);
    FO_RUNTIME_ASSERT(rect.height >= 0);
}

static auto GetFallbackTextureSizeData() -> const float32*
{
    FO_STACK_TRACE_ENTRY();

    static constexpr float32 FallbackSizeData[4] {1.0f, 1.0f, 1.0f, 1.0f};

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

        for (int32 y = 0; y < size.height; y++) {
            for (int32 x = 0; x < size.width; x++) {
                const ipos32 src_pos {pos.x + x, pos.y + y};
                const size_t dst_index = numeric_cast<size_t>(y) * numeric_cast<size_t>(size.width) + numeric_cast<size_t>(x);
                result[dst_index] = _pixels[CalcTextureIndex(*this, src_pos)];
            }
        }

        return result;
    }

    void UpdateTextureRegion(ipos32 pos, isize32 size, const ucolor* data, bool use_dest_pitch) override
    {
        FO_STACK_TRACE_ENTRY();

        ValidateTextureRect(*this, pos, size);

        if (size.width == 0 || size.height == 0) {
            return;
        }

        FO_RUNTIME_ASSERT(data != nullptr);

        const int32 pitch = use_dest_pitch ? Size.width : size.width;

        for (int32 y = 0; y < size.height; y++) {
            for (int32 x = 0; x < size.width; x++) {
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
            FO_RUNTIME_ASSERT(Vertices.empty());
            FO_RUNTIME_ASSERT(upload_vertices <= Vertices3D.size());
        }
        else {
            FO_RUNTIME_ASSERT(Vertices3D.empty());
            FO_RUNTIME_ASSERT(upload_vertices <= Vertices.size());
        }
#else
        ignore_unused(usage);
        FO_RUNTIME_ASSERT(upload_vertices <= Vertices.size());
#endif

        FO_RUNTIME_ASSERT(upload_indices <= Indices.size());

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

    void DrawBuffer(RenderDrawBuffer* dbuf, size_t start_index, optional<size_t> indices_to_draw, const RenderTexture* custom_tex) override
    {
        FO_STACK_TRACE_ENTRY();

        FO_RUNTIME_ASSERT(dbuf != nullptr);

        const size_t draw_indices = indices_to_draw.value_or(dbuf->IndCount - start_index);
        FO_RUNTIME_ASSERT(start_index <= dbuf->IndCount);
        FO_RUNTIME_ASSERT(draw_indices <= dbuf->IndCount - start_index);

#if FO_ENABLE_3D
        if (_usage == EffectUsage::Model) {
            FO_RUNTIME_ASSERT(dbuf->VertCount <= dbuf->Vertices3D.size());
        }
        else {
            FO_RUNTIME_ASSERT(dbuf->VertCount <= dbuf->Vertices.size());
        }
#else
        FO_RUNTIME_ASSERT(dbuf->VertCount <= dbuf->Vertices.size());
#endif

        if (_needMainTex) {
            const RenderTexture* main_tex = custom_tex != nullptr ? custom_tex : MainTex.get();

            if (_needMainTexBuf && !MainTexBuf.has_value()) {
                auto& main_tex_buf = MainTexBuf = MainTexBuffer();
                const float32* size_data = main_tex != nullptr ? main_tex->SizeData : GetFallbackTextureSizeData();
                MemCopy(main_tex_buf->MainTexSize, size_data, 4 * sizeof(float32));
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

auto Null_Renderer::CreateOrthoMatrix(float32 left, float32 right, float32 bottom, float32 top, float32 nearp, float32 farp) -> mat44
{
    FO_STACK_TRACE_ENTRY();

    const float32 r_l = right - left;
    const float32 t_b = top - bottom;
    const float32 f_n = farp - nearp;
    const float32 tx = -(right + left) / r_l;
    const float32 ty = -(top + bottom) / t_b;
    const float32 tz = -(farp + nearp) / f_n;

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

auto Null_Renderer::GetViewPort() -> irect32
{
    FO_STACK_TRACE_ENTRY();

    return _viewPortRect;
}

auto Null_Renderer::IsRenderTargetFlipped() -> bool
{
    FO_STACK_TRACE_ENTRY();

    return false;
}

void Null_Renderer::Init(GlobalSettings& settings, WindowInternalHandle* window)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(window);

    FO_RUNTIME_ASSERT(settings.ScreenWidth > 0);
    FO_RUNTIME_ASSERT(settings.ScreenHeight > 0);

    _viewPortRect = {0, 0, settings.ScreenWidth, settings.ScreenHeight};
    _currentRenderTarget = nullptr;
    _scissorEnabled = false;
    _scissorRect = {};
}

void Null_Renderer::Present()
{
    FO_STACK_TRACE_ENTRY();
}

void Null_Renderer::SetRenderTarget(RenderTexture* tex)
{
    FO_STACK_TRACE_ENTRY();

    _currentRenderTarget = tex;

    if (tex != nullptr) {
        _viewPortRect = {0, 0, tex->Size.width, tex->Size.height};
    }
}

void Null_Renderer::ClearRenderTarget(optional<ucolor> color, bool depth, bool stencil)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(depth);
    ignore_unused(stencil);

    if (!color.has_value() || _currentRenderTarget == nullptr) {
        return;
    }

    auto* null_tex = dynamic_cast<Null_Texture*>(_currentRenderTarget.get());
    FO_RUNTIME_ASSERT(null_tex != nullptr);
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

    FO_RUNTIME_ASSERT(size.width > 0);
    FO_RUNTIME_ASSERT(size.height > 0);

    if (_currentRenderTarget == nullptr) {
        _viewPortRect = {0, 0, size.width, size.height};
    }
}

FO_END_NAMESPACE
