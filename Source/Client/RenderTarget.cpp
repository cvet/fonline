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
#include "GenericUtils.h"
#include "StringUtils.h"

static constexpr auto MAX_STORED_PIXEL_PICKS = 100;

RenderTargetManager::RenderTargetManager(RenderSettings& settings, AppWindow* window, FlushCallback flush) :
    _settings {settings},
    _flush {std::move(flush)}
{
    STACK_TRACE_ENTRY();

    _eventUnsubscriber += window->OnScreenSizeChanged += [this] { OnScreenSizeChanged(); };
}

auto RenderTargetManager::GetRenderTargetStack() -> const vector<RenderTarget*>&
{
    NO_STACK_TRACE_ENTRY();

    return _rtStack;
}

auto RenderTargetManager::GetCurrentRenderTarget() -> RenderTarget*
{
    NO_STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return !_rtStack.empty() ? _rtStack.back() : nullptr;
}

auto RenderTargetManager::CreateRenderTarget(bool with_depth, RenderTarget::SizeKindType size_kind, isize base_size, bool linear_filtered) -> RenderTarget*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(base_size.width >= 0);
    RUNTIME_ASSERT(base_size.height >= 0);

    _flush();

    auto rt = SafeAlloc::MakeUnique<RenderTarget>();

    rt->SizeKind = size_kind;
    rt->BaseSize = base_size;
    rt->LastPixelPicks.reserve(MAX_STORED_PIXEL_PICKS);

    AllocateRenderTargetTexture(rt.get(), linear_filtered, with_depth);

    _rtAll.push_back(std::move(rt));
    return _rtAll.back().get();
}

void RenderTargetManager::OnScreenSizeChanged()
{
    STACK_TRACE_ENTRY();

    // Reallocate fullscreen render targets
    for (auto& rt : _rtAll) {
        if (rt->SizeKind != RenderTarget::SizeKindType::Custom) {
            AllocateRenderTargetTexture(rt.get(), rt->MainTex->LinearFiltered, rt->MainTex->WithDepth);
        }
    }
}

void RenderTargetManager::AllocateRenderTargetTexture(RenderTarget* rt, bool linear_filtered, bool with_depth)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    isize tex_size = rt->BaseSize;

    if (rt->SizeKind == RenderTarget::SizeKindType::Screen) {
        tex_size.width += _settings.ScreenWidth;
        tex_size.height += _settings.ScreenHeight;
    }
    else if (rt->SizeKind == RenderTarget::SizeKindType::Map) {
        tex_size.width += _settings.ScreenWidth;
        tex_size.height += _settings.ScreenHeight - _settings.ScreenHudHeight;
    }

    tex_size.width = std::max(tex_size.width, 1);
    tex_size.height = std::max(tex_size.height, 1);

    RUNTIME_ASSERT(tex_size.width > 0);
    RUNTIME_ASSERT(tex_size.height > 0);

    rt->MainTex = App->Render.CreateTexture(tex_size, linear_filtered, with_depth);

    rt->MainTex->FlippedHeight = App->Render.IsRenderTargetFlipped();

    auto* prev_tex = App->Render.GetRenderTarget();
    App->Render.SetRenderTarget(rt->MainTex.get());
    App->Render.ClearRenderTarget(ucolor::clear, with_depth);
    App->Render.SetRenderTarget(prev_tex);
}

void RenderTargetManager::PushRenderTarget(RenderTarget* rt)
{
    STACK_TRACE_ENTRY();

    const auto redundant = !_rtStack.empty() && _rtStack.back() == rt;

    _rtStack.push_back(rt);

    if (!redundant) {
        _flush();

        App->Render.SetRenderTarget(rt->MainTex.get());
        rt->LastPixelPicks.clear();
    }
}

void RenderTargetManager::PopRenderTarget()
{
    STACK_TRACE_ENTRY();

    const auto redundant = _rtStack.size() > 2 && _rtStack.back() == _rtStack[_rtStack.size() - 2];

    _rtStack.pop_back();

    if (!redundant) {
        _flush();

        if (!_rtStack.empty()) {
            App->Render.SetRenderTarget(_rtStack.back()->MainTex.get());
        }
        else {
            App->Render.SetRenderTarget(nullptr);
        }
    }
}

auto RenderTargetManager::GetRenderTargetPixel(RenderTarget* rt, ipos pos) const -> ucolor
{
    STACK_TRACE_ENTRY();

#if FO_NO_TEXTURE_LOOKUP
    UNUSED_VARIABLE(rt);
    UNUSED_VARIABLE(x);
    UNUSED_VARIABLE(y);

    return ucolor {255, 255, 255, 255};

#else
    // Try find in last picks
    for (auto&& [last_pos, last_color] : rt->LastPixelPicks) {
        if (last_pos == pos) {
            return last_color;
        }
    }

    // Read one pixel
    const auto color = rt->MainTex->GetTexturePixel(pos);

    // Refresh picks
    rt->LastPixelPicks.emplace(rt->LastPixelPicks.begin(), pos, color);

    if (rt->LastPixelPicks.size() > MAX_STORED_PIXEL_PICKS) {
        rt->LastPixelPicks.pop_back();
    }

    return color;
#endif
}

void RenderTargetManager::ClearCurrentRenderTarget(ucolor color, bool with_depth)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    App->Render.ClearRenderTarget(color, with_depth);
}

void RenderTargetManager::DeleteRenderTarget(RenderTarget* rt)
{
    STACK_TRACE_ENTRY();

    const auto it = std::find_if(_rtAll.begin(), _rtAll.end(), [rt](auto&& check_rt) { return check_rt.get() == rt; });
    RUNTIME_ASSERT(it != _rtAll.end());
    _rtAll.erase(it);
}

void RenderTargetManager::ClearStack()
{
    STACK_TRACE_ENTRY();

    _rtStack.clear();
}

void RenderTargetManager::DumpTextures() const
{
    STACK_TRACE_ENTRY();

    uint atlases_memory_size = 0;

    for (const auto& rt : _rtAll) {
        atlases_memory_size += rt->MainTex->Size.width * rt->MainTex->Size.height * 4;
    }

    const auto time = nanotime::now().desc();
    const string dir = strex("{:04}.{:02}.{:02}_{:02}-{:02}-{:02}_{}.{:03}mb", //
        time.year, time.month, time.day, time.hour, time.minute, time.second, //
        atlases_memory_size / 1000000, atlases_memory_size % 1000000 / 1000);

    const auto write_rt = [&dir](string_view name, const RenderTarget* rt) {
        if (rt != nullptr) {
            const string fname = strex("{}/{}_{}x{}.tga", dir, name, rt->MainTex->Size.width, rt->MainTex->Size.height);
            auto tex_data = rt->MainTex->GetTextureRegion({0, 0}, rt->MainTex->Size);
            GenericUtils::WriteSimpleTga(fname, rt->MainTex->Size, std::move(tex_data));
        }
    };

    int cnt = 1;

    for (const auto& rt : _rtAll) {
        write_rt(strex("All_{}", cnt), rt.get());
        cnt++;
    }
}
