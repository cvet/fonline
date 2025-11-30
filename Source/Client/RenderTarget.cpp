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

#include "RenderTarget.h"

FO_BEGIN_NAMESPACE();

static constexpr auto MAX_STORED_PIXEL_PICKS = 100;

RenderTargetManager::RenderTargetManager(RenderSettings& settings, AppWindow* window, FlushCallback flush) :
    _settings {&settings},
    _flush {std::move(flush)}
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_flush);

    _eventUnsubscriber += window->OnScreenSizeChanged += [this] { OnScreenSizeChanged(); };
}

auto RenderTargetManager::GetRenderTargetStack() -> const vector<raw_ptr<RenderTarget>>&
{
    FO_NO_STACK_TRACE_ENTRY();

    return _rtStack;
}

auto RenderTargetManager::GetCurrentRenderTarget() -> RenderTarget*
{
    FO_NO_STACK_TRACE_ENTRY();

    return !_rtStack.empty() ? _rtStack.back().get() : nullptr;
}

auto RenderTargetManager::GetCurrentRenderTarget() const -> const RenderTarget*
{
    FO_NO_STACK_TRACE_ENTRY();

    return !_rtStack.empty() ? _rtStack.back().get() : nullptr;
}

auto RenderTargetManager::CreateRenderTarget(bool with_depth, RenderTarget::SizeKindType size_kind, isize32 base_size, bool linear_filtered) -> RenderTarget*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(base_size.width >= 0);
    FO_RUNTIME_ASSERT(base_size.height >= 0);

    _flush();

    auto rt = SafeAlloc::MakeUnique<RenderTarget>();

    rt->_sizeKind = size_kind;
    rt->_baseSize = base_size;
    rt->_lastPixelPicks.reserve(MAX_STORED_PIXEL_PICKS);

    AllocateRenderTargetTexture(rt.get(), linear_filtered, with_depth);

    _rtAll.push_back(std::move(rt));
    return _rtAll.back().get();
}

void RenderTargetManager::OnScreenSizeChanged()
{
    FO_STACK_TRACE_ENTRY();

    // Reallocate fullscreen render targets
    for (auto& rt : _rtAll) {
        if (rt->_sizeKind != RenderTarget::SizeKindType::Custom) {
            AllocateRenderTargetTexture(rt.get(), rt->_texture->LinearFiltered, rt->_texture->WithDepth);
        }
    }
}

void RenderTargetManager::AllocateRenderTargetTexture(RenderTarget* rt, bool linear_filtered, bool with_depth)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    auto tex_size = rt->_baseSize;

    if (rt->_sizeKind == RenderTarget::SizeKindType::Screen) {
        tex_size.width += _settings->ScreenWidth;
        tex_size.height += _settings->ScreenHeight;
    }
    else if (rt->_sizeKind == RenderTarget::SizeKindType::Map) {
        tex_size.width += _settings->ScreenWidth + _settings->MapHexWidth;
        tex_size.height += _settings->ScreenHeight - _settings->ScreenHudHeight + _settings->MapHexLineHeight * 2;
    }

    tex_size.width = std::max(tex_size.width, 1);
    tex_size.height = std::max(tex_size.height, 1);

    FO_RUNTIME_ASSERT(tex_size.width > 0);
    FO_RUNTIME_ASSERT(tex_size.height > 0);

    rt->_texture = App->Render.CreateTexture(tex_size, linear_filtered, with_depth);
    rt->_texture->FlippedHeight = App->Render.IsRenderTargetFlipped();

    auto* prev_tex = App->Render.GetRenderTarget();
    App->Render.SetRenderTarget(rt->_texture.get());
    App->Render.ClearRenderTarget(ucolor::clear, with_depth);
    App->Render.SetRenderTarget(prev_tex);
}

void RenderTargetManager::PushRenderTarget(RenderTarget* rt)
{
    FO_STACK_TRACE_ENTRY();

    const auto redundant = !_rtStack.empty() && _rtStack.back() == rt;

    if (!redundant) {
        _flush();
    }

    _rtStack.emplace_back(rt);

    if (!redundant) {
        App->Render.SetRenderTarget(rt->_texture.get());
        rt->_lastPixelPicks.clear();
    }
}

void RenderTargetManager::PopRenderTarget()
{
    FO_STACK_TRACE_ENTRY();

    const auto redundant = _rtStack.size() > 2 && _rtStack.back() == _rtStack[_rtStack.size() - 2];

    if (!redundant) {
        _flush();
    }

    _rtStack.pop_back();

    if (!redundant) {
        if (!_rtStack.empty()) {
            App->Render.SetRenderTarget(_rtStack.back()->_texture.get());
        }
        else {
            App->Render.SetRenderTarget(nullptr);
        }
    }
}

auto RenderTargetManager::GetRenderTargetPixel(const RenderTarget* rt, ipos32 pos) const -> ucolor
{
    FO_STACK_TRACE_ENTRY();

#if FO_NO_TEXTURE_LOOKUP
    ignore_unused(rt);
    ignore_unused(x);
    ignore_unused(y);

    return ucolor {255, 255, 255, 255};

#else
    // Try to find in last picks
    for (auto&& [last_pos, last_color] : rt->_lastPixelPicks) {
        if (last_pos == pos) {
            return last_color;
        }
    }

    // Read one pixel
    const auto color = rt->_texture->GetTexturePixel(pos);

    // Refresh picks
    rt->_lastPixelPicks.emplace(rt->_lastPixelPicks.begin(), pos, color);

    if (rt->_lastPixelPicks.size() > MAX_STORED_PIXEL_PICKS) {
        rt->_lastPixelPicks.pop_back();
    }

    return color;
#endif
}

void RenderTargetManager::ClearCurrentRenderTarget(ucolor color, bool with_depth)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    App->Render.ClearRenderTarget(color, with_depth);
}

void RenderTargetManager::DeleteRenderTarget(RenderTarget* rt)
{
    FO_STACK_TRACE_ENTRY();

    if (rt == nullptr) {
        return;
    }

    const auto it = std::ranges::find_if(_rtAll, [rt](auto&& check_rt) { return check_rt.get() == rt; });
    FO_RUNTIME_ASSERT(it != _rtAll.end());
    _rtAll.erase(it);
}

void RenderTargetManager::ClearStack()
{
    FO_STACK_TRACE_ENTRY();

    _rtStack.clear();
}

void RenderTargetManager::DumpTextures() const
{
    FO_STACK_TRACE_ENTRY();

    size_t atlases_memory_size = 0;

    for (const auto& rt : _rtAll) {
        atlases_memory_size += numeric_cast<size_t>(rt->_texture->Size.width) * rt->_texture->Size.height * 4;
    }

    const auto time = nanotime::now().desc(true);
    const string dir = strex("{:04}.{:02}.{:02}_{:02}-{:02}-{:02}_{}.{:03}mb", //
        time.year, time.month, time.day, time.hour, time.minute, time.second, //
        atlases_memory_size / 1000000, atlases_memory_size % 1000000 / 1000);

    const auto write_rt = [&dir](string_view name, const RenderTarget* rt) {
        if (rt != nullptr) {
            const string fname = strex("{}/{}_{}x{}.tga", dir, name, rt->_texture->Size.width, rt->_texture->Size.height);
            auto tex_data = rt->_texture->GetTextureRegion({0, 0}, rt->_texture->Size);
            GenericUtils::WriteSimpleTga(fname, rt->_texture->Size, std::move(tex_data));
        }
    };

    size_t num = 1;

    for (const auto& rt : _rtAll) {
        write_rt(strex("All_{}", num), rt.get());
        num++;
    }
}

FO_END_NAMESPACE();
