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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

// Todo: optimize sprite atlas filling

#pragma once

#include "Common.h"

#include "Application.h"
#include "Rendering.h"

FO_BEGIN_NAMESPACE();

class RenderTargetManager;

class RenderTarget
{
    friend class RenderTargetManager;

public:
    enum class SizeKindType : uint8
    {
        Custom,
        Screen,
        Map,
    };

    [[nodiscard]] auto GetTexture() const noexcept -> const RenderTexture* { return _texture.get(); }
    [[nodiscard]] auto GetTexture() noexcept -> RenderTexture* { return _texture.get(); }
    [[nodiscard]] auto GetSizeKind() const noexcept -> SizeKindType { return _sizeKind; }
    [[nodiscard]] auto GetBaseSize() const noexcept -> isize32 { return _baseSize; }
    [[nodiscard]] auto GetCustomDrawEffect() const noexcept -> RenderEffect* { return _customDrawEffect.get(); }

    void SetCustomDrawEffect(RenderEffect* effect) const noexcept { _customDrawEffect = effect; }
    void ClearLastPixelPicks() const noexcept { _lastPixelPicks.clear(); }

private:
    unique_ptr<RenderTexture> _texture {};
    SizeKindType _sizeKind {};
    isize32 _baseSize {};
    mutable raw_ptr<RenderEffect> _customDrawEffect {};
    mutable vector<tuple<ipos32, ucolor>> _lastPixelPicks {};
};

class RenderTargetManager
{
public:
    using FlushCallback = function<void()>;

    RenderTargetManager(RenderSettings& settings, AppWindow* window, FlushCallback flush);
    RenderTargetManager(const RenderTargetManager&) = delete;
    RenderTargetManager(RenderTargetManager&&) noexcept = default;
    auto operator=(const RenderTargetManager&) = delete;
    auto operator=(RenderTargetManager&&) noexcept -> RenderTargetManager& = delete;
    ~RenderTargetManager() = default;

    [[nodiscard]] auto CreateRenderTarget(bool with_depth, RenderTarget::SizeKindType size_kind, isize32 base_size, bool linear_filtered) -> RenderTarget*;
    [[nodiscard]] auto GetRenderTargetPixel(const RenderTarget* rt, ipos32 pos) const -> ucolor;
    [[nodiscard]] auto GetRenderTargetStack() -> const vector<raw_ptr<RenderTarget>>&;
    [[nodiscard]] auto GetCurrentRenderTarget() const -> const RenderTarget*;
    [[nodiscard]] auto GetCurrentRenderTarget() -> RenderTarget*;

    void PushRenderTarget(RenderTarget* rt);
    void PopRenderTarget();
    void ClearCurrentRenderTarget(ucolor color, bool with_depth = false);
    void DeleteRenderTarget(RenderTarget* rt);

    void ClearStack();
    void DumpTextures() const;

private:
    void OnScreenSizeChanged();
    void AllocateRenderTargetTexture(RenderTarget* rt, bool linear_filtered, bool with_depth);

    raw_ptr<RenderSettings> _settings;
    FlushCallback _flush;
    vector<unique_ptr<RenderTarget>> _rtAll {};
    vector<raw_ptr<RenderTarget>> _rtStack {};
    EventUnsubscriber _eventUnsubscriber {};
    bool _nonConstHelper {};
};

FO_END_NAMESPACE();
