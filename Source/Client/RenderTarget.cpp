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

#include "RenderTarget.h"
#include "Application.h"

FO_BEGIN_NAMESPACE

static constexpr auto MAX_STORED_PIXEL_PICKS = 100;

RenderTarget::RenderTarget(isize32 size, unique_ptr<RenderTexture> texture) :
    _texture {std::move(texture)},
    _size {size}
{
    FO_STACK_TRACE_ENTRY();
}

RenderTargetManager::RenderTargetManager(ptr<IAppRender> render, FlushCallback flush) :
    _render {render},
    _flush {std::move(flush)}
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_flush, "Flush callback is null");
}

auto RenderTargetManager::GetRenderTargetStack() const -> const_span<ptr<RenderTarget>>
{
    FO_NO_STACK_TRACE_ENTRY();

    return _rtStack;
}

auto RenderTargetManager::GetCurrentRenderTarget() -> nptr<RenderTarget>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_rtStack.empty()) {
        return nullptr;
    }

    return _rtStack.back();
}

auto RenderTargetManager::GetCurrentRenderTarget() const -> nptr<const RenderTarget>
{
    FO_NO_STACK_TRACE_ENTRY();

    if (_rtStack.empty()) {
        return nullptr;
    }

    return _rtStack.back();
}

auto RenderTargetManager::CreateRenderTarget(bool with_depth, isize32 size, bool linear_filtered) -> ptr<RenderTarget>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(size.width >= 0, "Size width is negative", size.width);
    FO_VERIFY_AND_THROW(size.height >= 0, "Size height is negative", size.height);

    _flush();

    auto rt = SafeAlloc::MakeUnique<RenderTarget>(size, CreateRenderTargetTexture(size, linear_filtered, with_depth));
    rt->_lastPixelPicks.reserve(MAX_STORED_PIXEL_PICKS);

    _rtAll.push_back(std::move(rt));
    return _rtAll.back();
}

void RenderTargetManager::ResizeRenderTarget(ptr<RenderTarget> rt, isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(size.width >= 0, "Size width is negative", size.width);
    FO_VERIFY_AND_THROW(size.height >= 0, "Size height is negative", size.height);

    if (rt->_size == size) {
        return;
    }

    _flush();

    bool linear_filtered = rt->_texture->LinearFiltered;
    bool with_depth = rt->_texture->WithDepth;

    rt->_texture = CreateRenderTargetTexture(size, linear_filtered, with_depth);
    rt->_size = size;
}

auto RenderTargetManager::CreateRenderTargetTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture>
{
    FO_STACK_TRACE_ENTRY();

    isize32 tex_size = size;
    tex_size.width = std::max(tex_size.width, 1);
    tex_size.height = std::max(tex_size.height, 1);

    auto texture = _render->CreateTexture(tex_size, linear_filtered, with_depth);
    texture->FlippedHeight = _render->IsRenderTargetFlipped();

    auto prev_tex = _render->GetRenderTarget();
    auto restore_target = scope_fail([&]() noexcept { safe_call([&] { _render->SetRenderTarget(prev_tex); }); });
    _render->SetRenderTarget(texture);
    _render->ClearRenderTarget(ucolor::clear, with_depth);
    _render->SetRenderTarget(prev_tex);

    return texture;
}

void RenderTargetManager::PushRenderTarget(ptr<RenderTarget> rt)
{
    FO_STACK_TRACE_ENTRY();

    bool redundant = !_rtStack.empty() && _rtStack.back() == rt;

    if (!redundant) {
        _flush();
        _render->SetRenderTarget(rt->_texture);
        rt->_lastPixelPicks.clear();
    }

    _rtStack.emplace_back(rt);
}

void RenderTargetManager::PopRenderTarget()
{
    FO_STACK_TRACE_ENTRY();

    bool redundant = _rtStack.size() > 2 && _rtStack.back() == _rtStack[_rtStack.size() - 2];

    if (!redundant) {
        _flush();

        // Bind the target that will become the new stack top (the entry under the current top) before
        // the pop, so a SetRenderTarget throw leaves both _rtStack and the backend unchanged.
        if (_rtStack.size() >= 2) {
            _render->SetRenderTarget(_rtStack[_rtStack.size() - 2]->_texture);
        }
        else {
            _render->SetRenderTarget(nullptr);
        }
    }

    _rtStack.pop_back();
}

auto RenderTargetManager::GetRenderTargetPixel(ptr<const RenderTarget> rt, ipos32 pos) const -> ucolor
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
    ucolor color = rt->_texture->GetTexturePixel(pos);

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

    _render->ClearRenderTarget(color, with_depth);
}

void RenderTargetManager::DeleteRenderTarget(nptr<RenderTarget> rt)
{
    FO_STACK_TRACE_ENTRY();

    if (!rt) {
        return;
    }

    auto it = std::ranges::find_if(_rtAll, [&rt](auto&& check_rt) {
        auto check_rt_ptr = check_rt.as_ptr();
        return check_rt_ptr == rt;
    });
    FO_VERIFY_AND_THROW(it != _rtAll.end(), "Lookup failed in rt all");
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

    for (size_t i = 0; i < _rtAll.size(); i++) {
        auto rt = _rtAll[i].as_ptr();
        atlases_memory_size += numeric_cast<size_t>(rt->_texture->Size.width) * rt->_texture->Size.height * 4;
    }

    time_desc_t time = nanotime::now().desc(true);
    string dir = strex("{:04}.{:02}.{:02}_{:02}-{:02}-{:02}_{}.{:03}mb", //
        time.year, time.month, time.day, time.hour, time.minute, time.second, //
        atlases_memory_size / 1000000, atlases_memory_size % 1000000 / 1000);

    auto write_rt = [&dir](string_view name, ptr<const RenderTarget> rt) {
        string fname = strex("{}/{}_{}x{}.tga", dir, name, rt->_texture->Size.width, rt->_texture->Size.height);
        auto tex_data = rt->_texture->GetTextureRegion({0, 0}, rt->_texture->Size);
        WriteSimpleTga(fname, rt->_texture->Size, std::move(tex_data));
    };

    size_t num = 1;

    for (size_t i = 0; i < _rtAll.size(); i++) {
        write_rt(strex("All_{}", num), _rtAll[i]);
        num++;
    }
}

FO_END_NAMESPACE
