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

#pragma once

#include "Common.h"

#include "Rendering.h"

FO_BEGIN_NAMESPACE

class IAppRender;
class RenderTargetManager;

class RenderTarget
{
    friend class RenderTargetManager;

public:
    RenderTarget(isize32 size, unique_ptr<RenderTexture> texture);

    [[nodiscard]] auto GetTexture() const noexcept -> ptr<const RenderTexture>
    {
        FO_NO_STACK_TRACE_ENTRY();

        return _texture;
    }
    [[nodiscard]] auto GetTexture() noexcept -> ptr<RenderTexture>
    {
        FO_NO_STACK_TRACE_ENTRY();

        return _texture;
    }
    [[nodiscard]] auto GetSize() const noexcept -> isize32 { return _size; }
    [[nodiscard]] auto GetCustomDrawEffect() const noexcept -> nptr<RenderEffect> { return _customDrawEffect; }

    void SetCustomDrawEffect(nptr<RenderEffect> effect) const noexcept { _customDrawEffect = effect; }
    void ClearLastPixelPicks() const noexcept { _lastPixelPicks.clear(); }

private:
    unique_ptr<RenderTexture> _texture;
    isize32 _size;
    mutable nptr<RenderEffect> _customDrawEffect {};
    mutable vector<tuple<ipos32, ucolor>> _lastPixelPicks {};
};

class RenderTargetManager
{
public:
    using FlushCallback = function<void()>;

    RenderTargetManager(ptr<IAppRender> render, FlushCallback flush);
    RenderTargetManager(const RenderTargetManager&) = delete;
    RenderTargetManager(RenderTargetManager&&) noexcept = default;
    auto operator=(const RenderTargetManager&) = delete;
    auto operator=(RenderTargetManager&&) noexcept -> RenderTargetManager& = delete;
    ~RenderTargetManager() = default;

    [[nodiscard]] auto CreateRenderTarget(bool with_depth, isize32 size, bool linear_filtered) -> ptr<RenderTarget>;
    [[nodiscard]] auto GetRenderTargetPixel(ptr<const RenderTarget> rt, ipos32 pos) const -> ucolor;
    [[nodiscard]] auto GetRenderTargetStack() const -> const_span<ptr<RenderTarget>>;
    [[nodiscard]] auto GetCurrentRenderTarget() const -> nptr<const RenderTarget>;
    [[nodiscard]] auto GetCurrentRenderTarget() -> nptr<RenderTarget>;

    void PushRenderTarget(ptr<RenderTarget> rt);
    void PopRenderTarget();
    void ClearCurrentRenderTarget(ucolor color, bool with_depth = false);
    void DeleteRenderTarget(nptr<RenderTarget> rt);
    void ResizeRenderTarget(ptr<RenderTarget> rt, isize32 size);

    void ClearStack();
    void DumpTextures() const;

private:
    auto CreateRenderTargetTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture>;

    ptr<IAppRender> _render;
    FlushCallback _flush;
    vector<unique_ptr<RenderTarget>> _rtAll {};
    vector<ptr<RenderTarget>> _rtStack {};
    bool _nonConstHelper {};
};

FO_END_NAMESPACE
