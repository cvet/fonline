//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
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

#if FO_HAVE_VULKAN

#include "Application.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_video.h"
#include <vulkan/vulkan.h>

FO_BEGIN_NAMESPACE

class Vulkan_Texture final : public RenderTexture
{
public:
    Vulkan_Texture(isize32 size, bool linear_filtered, bool with_depth) :
        RenderTexture(size, linear_filtered, with_depth)
    {
    }

    ~Vulkan_Texture() override = default;

    [[nodiscard]] auto GetTexturePixel(ipos32 /*pos*/) const -> ucolor override { throw NotImplementedException(FO_LINE_STR); }
    [[nodiscard]] auto GetTextureRegion(ipos32 /*pos*/, isize32 /*size*/) const -> vector<ucolor> override { throw NotImplementedException(FO_LINE_STR); }
    void UpdateTextureRegion(ipos32 /*pos*/, isize32 /*size*/, const ucolor* /*data*/, bool /*use_dest_pitch*/) override { throw NotImplementedException(FO_LINE_STR); }
};

class Vulkan_DrawBuffer final : public RenderDrawBuffer
{
public:
    explicit Vulkan_DrawBuffer(bool is_static) :
        RenderDrawBuffer(is_static)
    {
    }
    ~Vulkan_DrawBuffer() override = default;

    void Upload(EffectUsage /*usage*/, optional<size_t> /*custom_vertices_size*/, optional<size_t> /*custom_indices_size*/) override { throw NotImplementedException(FO_LINE_STR); }
};

class Vulkan_Effect final : public RenderEffect
{
public:
    Vulkan_Effect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) :
        RenderEffect(usage, name, loader)
    {
    }

    ~Vulkan_Effect() override = default;

    void DrawBuffer(RenderDrawBuffer* /*dbuf*/, size_t /*start_index*/, optional<size_t> /*indices_to_draw*/, const RenderTexture* /*custom_tex*/) override { throw NotImplementedException(FO_LINE_STR); }
};

[[nodiscard]] auto Vulkan_Renderer::CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture>
{
    throw NotImplementedException(FO_LINE_STR);
}

[[nodiscard]] auto Vulkan_Renderer::CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer>
{
    throw NotImplementedException(FO_LINE_STR);
}

[[nodiscard]] auto Vulkan_Renderer::CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect>
{
    throw NotImplementedException(FO_LINE_STR);
}

[[nodiscard]] auto Vulkan_Renderer::CreateOrthoMatrix(float32 left, float32 right, float32 bottom, float32 top, float32 nearp, float32 farp) -> mat44
{
    throw NotImplementedException(FO_LINE_STR);
}

[[nodiscard]] auto Vulkan_Renderer::GetViewPort() -> irect32
{
    throw NotImplementedException(FO_LINE_STR);
}

void Vulkan_Renderer::Init(GlobalSettings& /*settings*/, WindowInternalHandle* /*window*/)
{
    throw NotImplementedException(FO_LINE_STR);
}

void Vulkan_Renderer::Present()
{
    throw NotImplementedException(FO_LINE_STR);
}

void Vulkan_Renderer::SetRenderTarget(RenderTexture* /*tex*/)
{
    throw NotImplementedException(FO_LINE_STR);
}

void Vulkan_Renderer::ClearRenderTarget(optional<ucolor> /*color*/, bool /*depth*/, bool /*stencil*/)
{
    throw NotImplementedException(FO_LINE_STR);
}

void Vulkan_Renderer::EnableScissor(irect32 /*rect*/)
{
    throw NotImplementedException(FO_LINE_STR);
}

void Vulkan_Renderer::DisableScissor()
{
    throw NotImplementedException(FO_LINE_STR);
}

void Vulkan_Renderer::OnResizeWindow(isize32 /*size*/)
{
    throw NotImplementedException(FO_LINE_STR);
}

FO_END_NAMESPACE

#endif // FO_HAVE_VULKAN
