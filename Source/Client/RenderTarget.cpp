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
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "Timer.h"

static constexpr auto MAX_STORED_PIXEL_PICKS = 100;

RenderTargetManager::RenderTargetManager(RenderSettings& settings, AppWindow* window, FlushCallback flush) :
    _settings {settings},
    _flush {std::move(flush)}
{
    _eventUnsubscriber += window->OnScreenSizeChanged += [this] { OnScreenSizeChanged(); };
}

auto RenderTargetManager::GetRenderTargetStack() -> const vector<RenderTarget*>&
{
    return _rtStack;
}

auto RenderTargetManager::CreateRenderTarget(bool with_depth, RenderTarget::SizeType size, int width, int height, bool linear_filtered) -> RenderTarget*
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(width >= 0);
    RUNTIME_ASSERT(height >= 0);

    _flush();

    auto&& rt = std::make_unique<RenderTarget>();
    rt->Size = size;
    rt->BaseWidth = width;
    rt->BaseHeight = height;
    rt->LastPixelPicks.reserve(MAX_STORED_PIXEL_PICKS);

    AllocateRenderTargetTexture(rt.get(), linear_filtered, with_depth);

    _rtAll.push_back(std::move(rt));
    return _rtAll.back().get();
}

void RenderTargetManager::OnScreenSizeChanged()
{
    STACK_TRACE_ENTRY();

    // Reallocate fullscreen render targets
    for (auto&& rt : _rtAll) {
        if (rt->Size != RenderTarget::SizeType::Custom) {
            AllocateRenderTargetTexture(rt.get(), rt->MainTex->LinearFiltered, rt->MainTex->WithDepth);
        }
    }
}

void RenderTargetManager::AllocateRenderTargetTexture(RenderTarget* rt, bool linear_filtered, bool with_depth)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    auto tex_width = rt->BaseWidth;
    auto tex_height = rt->BaseHeight;

    if (rt->Size == RenderTarget::SizeType::Screen) {
        tex_width += _settings.ScreenWidth;
        tex_height += _settings.ScreenHeight;
    }
    else if (rt->Size == RenderTarget::SizeType::Map) {
        tex_width += _settings.ScreenWidth;
        tex_height += _settings.ScreenHeight - _settings.ScreenHudHeight;
    }

    tex_width = std::max(tex_width, 1);
    tex_height = std::max(tex_height, 1);

    RUNTIME_ASSERT(tex_width > 0);
    RUNTIME_ASSERT(tex_height > 0);

    rt->MainTex = unique_ptr<RenderTexture>(App->Render.CreateTexture(tex_width, tex_height, linear_filtered, with_depth));

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

auto RenderTargetManager::GetRenderTargetPixel(RenderTarget* rt, int x, int y) const -> ucolor
{
    STACK_TRACE_ENTRY();

#if FO_NO_TEXTURE_LOOKUP
    UNUSED_VARIABLE(rt);
    UNUSED_VARIABLE(x);
    UNUSED_VARIABLE(y);

    return ucolor {255, 255, 255, 255};

#else
    // Try find in last picks
    for (auto&& pix : rt->LastPixelPicks) {
        if (std::get<0>(pix) == x && std::get<1>(pix) == y) {
            return std::get<2>(pix);
        }
    }

    // Read one pixel
    auto color = rt->MainTex->GetTexturePixel(x, y);

    // Refresh picks
    rt->LastPixelPicks.emplace(rt->LastPixelPicks.begin(), x, y, color);
    if (rt->LastPixelPicks.size() > MAX_STORED_PIXEL_PICKS) {
        rt->LastPixelPicks.pop_back();
    }

    return color;
#endif
}

void RenderTargetManager::ClearCurrentRenderTarget(ucolor color, bool with_depth)
{
    STACK_TRACE_ENTRY();

    App->Render.ClearRenderTarget(color, with_depth);
}

void RenderTargetManager::DeleteRenderTarget(RenderTarget* rt)
{
    STACK_TRACE_ENTRY();

    const auto it = std::find_if(_rtAll.begin(), _rtAll.end(), [rt](auto&& check_rt) { return check_rt.get() == rt; });
    RUNTIME_ASSERT(it != _rtAll.end());
    _rtAll.erase(it);
}

void RenderTargetManager::DumpTextures() const
{
    STACK_TRACE_ENTRY();

    uint atlases_memory_size = 0;
    for (auto&& rt : _rtAll) {
        atlases_memory_size += rt->MainTex->Width * rt->MainTex->Height * 4;
    }

    const auto date = Timer::GetCurrentDateTime();
    const string dir = _str("{:04}.{:02}.{:02}_{:02}-{:02}-{:02}_{}.{:03}mb", //
        date.Year, date.Month, date.Day, date.Hour, date.Minute, date.Second, //
        atlases_memory_size / 1000000, atlases_memory_size % 1000000 / 1000);

    const auto write_rt = [&dir](string_view name, const RenderTarget* rt) {
        if (rt != nullptr) {
            const string fname = _str("{}/{}_{}x{}.tga", dir, name, rt->MainTex->Width, rt->MainTex->Height);
            auto tex_data = rt->MainTex->GetTextureRegion(0, 0, rt->MainTex->Width, rt->MainTex->Height);
            GenericUtils::WriteSimpleTga(fname, rt->MainTex->Width, rt->MainTex->Height, std::move(tex_data));
        }
    };

    int cnt = 1;
    for (auto&& rt : _rtAll) {
        write_rt(_str("All_{}", cnt), rt.get());
        cnt++;
    }
}
