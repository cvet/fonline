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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

#include "Application.h"
#include "Rendering.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

namespace ParticleCpuTests
{
    struct CapturedDraw
    {
        string EffectName {};
        vector<Vertex2D> Vertices {};
#if FO_ENABLE_3D
        vector<Vertex3D> Vertices3D {};
#endif
        bool HasBackgroundTexture {};
        vector<vindex_t> Indices {};
        RenderPrimitiveType PrimitiveType {};
        bool HasMainTexture {};
        bool HasProjection {};
        optional<array<float32_t, 4>> AtlasSubRect {};
        optional<array<float32_t, 4>> Sampling {};
        CullModeType CullMode {};
    };

    struct DrawCapture
    {
        vector<CapturedDraw> Draws {};
        function<void(string_view)> Observer {};
    };

    class CapturingRenderEffect final : public RenderEffect
    {
    public:
        CapturingRenderEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader, shared_ptr<DrawCapture> capture);

        void DrawBuffer(ptr<RenderDrawBuffer> dbuf, size_t start_index, optional<size_t> indices_to_draw, nptr<const RenderTexture> custom_tex) override;

    private:
        shared_ptr<DrawCapture> _capture;
    };

    class CapturingAppRender final : public IAppRender
    {
    public:
        explicit CapturingAppRender(ptr<GlobalSettings> settings);

        [[nodiscard]] auto GetRenderTarget() -> nptr<RenderTexture> override;
        [[nodiscard]] auto CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture> override;
        [[nodiscard]] auto CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer> override;
        [[nodiscard]] auto CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect> override;
        [[nodiscard]] auto CreateOrthoMatrix(float32_t left, float32_t right, float32_t bottom, float32_t top, float32_t nearp, float32_t farp) const -> mat44 override;
        [[nodiscard]] auto IsRenderTargetFlipped() const -> bool override;
        [[nodiscard]] auto GetProjMatrix() const -> mat44 override;

        void SetRenderTarget(nptr<RenderTexture> tex) override;
        void SetOrthoDepthRange(float32_t nearp, float32_t farp) noexcept override;
        void ClearRenderTarget(optional<ucolor> color, bool depth, bool stencil) override;
        void EnableScissor(irect32 rect) override;
        void DisableScissor() override;

        void ClearDraws();
        void SetDrawObserver(function<void(string_view)> observer);
        [[nodiscard]] auto GetDraws() const -> const vector<CapturedDraw>&;

    private:
        Null_Renderer _renderer;
        AppScreenState _screen {};
        shared_ptr<DrawCapture> _capture;
        nptr<RenderTexture> _renderTarget {};
    };

    auto MakeResources(string_view extension) -> vector<pair<string, vector<uint8_t>>>;
#if FO_EFFEKSEER_PARTICLES
    auto MakeGenerationalEffekseerEffect(int32_t parent_life, int32_t child_life) -> vector<uint8_t>;
#endif
}

FO_END_NAMESPACE

