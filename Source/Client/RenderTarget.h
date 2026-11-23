// FOnline Engine
// SPDX-License-Identifier: MIT

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

    [[nodiscard]] auto GetTexture() const noexcept -> ptr<const RenderTexture> { return _texture; }
    [[nodiscard]] auto GetTexture() noexcept -> ptr<RenderTexture> { return _texture; }
    [[nodiscard]] auto GetSize() const noexcept -> isize32 { return _size; }
    [[nodiscard]] auto GetCustomDrawEffect() const noexcept -> nptr<RenderEffect> { return _customDrawEffect; }

    void SetCustomDrawEffect(nptr<RenderEffect> effect) const noexcept { _customDrawEffect = effect; }

private:
    unique_ptr<RenderTexture> _texture;
    isize32 _size;
    mutable nptr<RenderEffect> _customDrawEffect {};
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

    [[nodiscard]] auto GetRenderTargetStack() const -> const_span<ptr<RenderTarget>>;
    [[nodiscard]] auto GetCurrentRenderTarget() const -> nptr<const RenderTarget>;
    [[nodiscard]] auto GetCurrentRenderTarget() -> nptr<RenderTarget>;
    [[nodiscard]] auto GetRenderTargetCount() const noexcept -> size_t { return _rtAll.size(); }

    auto CreateRenderTarget(bool with_depth, isize32 size, bool linear_filtered) -> ptr<RenderTarget>;
    void PushRenderTarget(ptr<RenderTarget> rt);
    void PopRenderTarget();
    void ClearCurrentRenderTarget(ucolor color, bool with_depth = false);
    void DeleteRenderTarget(nptr<RenderTarget> rt);
    void ResizeRenderTarget(ptr<RenderTarget> rt, isize32 size);

    void ClearStack();
    // The writable root comes from the caller: this manager holds render settings, which say nothing
    // about where the process may write
    void DumpTextures(string_view writable_root) const;

private:
    auto CreateRenderTargetTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture>;

    ptr<IAppRender> _render;
    FlushCallback _flush;
    vector<unique_ptr<RenderTarget>> _rtAll {};
    vector<ptr<RenderTarget>> _rtStack {};
};

FO_END_NAMESPACE
