//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "Application.h"
#include "DiskFileSystem.h"
#include "StringUtils.h"
#include "Version-Include.h"
#include "WinApi-Include.h"

Application* App;

#if FO_WINDOWS && FO_DEBUG
static _CrtMemState CrtMemState;
#endif

static const uint MAX_ATLAS_WIDTH_ = 1024;
static const uint MAX_ATLAS_HEIGHT_ = 1024;
static const uint MAX_BONES_ = 32;
const uint& Application::AppRender::MAX_ATLAS_WIDTH {MAX_ATLAS_WIDTH_};
const uint& Application::AppRender::MAX_ATLAS_HEIGHT {MAX_ATLAS_HEIGHT_};
const uint& Application::AppRender::MAX_BONES {MAX_BONES_};
const int Application::AppAudio::AUDIO_FORMAT_U8 = 0;
const int Application::AppAudio::AUDIO_FORMAT_S16 = 1;

auto RenderEffect::IsSame(string_view name, string_view defines) const -> bool
{
    return _str(name).compareIgnoreCase(_effectName) && defines == _effectDefines;
}

auto RenderEffect::CanBatch(const RenderEffect* other) const -> bool
{
    // Todo: implement effect CanBatch
    UNUSED_VARIABLE(other);
    return false;
}

Application::Application(int argc, char** argv, string_view name_appendix) : Settings(argc, argv)
{
    // Skip SDL allocations from profiling
#if FO_WINDOWS && FO_DEBUG
    ::_CrtMemCheckpoint(&CrtMemState);
#endif

    _name.append(FO_DEV_NAME);
    if (!name_appendix.empty()) {
        _name.append("_");
        _name.append(name_appendix);
    }
}

#if FO_IOS
void Application::SetMainLoopCallback(void (*callback)(void*))
{
}
#endif

void Application::BeginFrame()
{
    _onFrameBeginDispatcher();
}

void Application::EndFrame()
{
    _onFrameEndDispatcher();
}

auto Application::AppWindow::GetSize() const -> tuple<int, int>
{
    auto w = 1000;
    auto h = 1000;
    return {w, h};
}

void Application::AppWindow::SetSize(int w, int h)
{
    UNUSED_VARIABLE(w);
    UNUSED_VARIABLE(h);
}

auto Application::AppWindow::GetPosition() const -> tuple<int, int>
{
    auto x = 0;
    auto y = 0;
    return {x, y};
}

void Application::AppWindow::SetPosition(int x, int y)
{
    UNUSED_VARIABLE(x);
    UNUSED_VARIABLE(y);
}

auto Application::AppWindow::GetMousePosition() const -> tuple<int, int>
{
    auto x = 100;
    auto y = 100;
    return {x, y};
}

void Application::AppWindow::SetMousePosition(int x, int y)
{
    UNUSED_VARIABLE(x);
    UNUSED_VARIABLE(y);
}

auto Application::AppWindow::IsFocused() const -> bool
{
    return true;
}

void Application::AppWindow::Minimize()
{
}

auto Application::AppWindow::IsFullscreen() const -> bool
{
    return false;
}

auto Application::AppWindow::ToggleFullscreen(bool enable) -> bool
{
    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(enable);

    return false;
}

void Application::AppWindow::Blink()
{
}

void Application::AppWindow::AlwaysOnTop(bool enable)
{
    UNUSED_VARIABLE(enable);
}

auto Application::AppRender::CreateTexture(uint width, uint height, bool linear_filtered, bool with_depth) -> RenderTexture*
{
    UNUSED_VARIABLE(width);
    UNUSED_VARIABLE(height);
    UNUSED_VARIABLE(linear_filtered);
    UNUSED_VARIABLE(with_depth);

    return nullptr;
}

void Application::AppRender::SetRenderTarget(RenderTexture* tex)
{
    UNUSED_VARIABLE(tex);
}

auto Application::AppRender::GetRenderTarget() -> RenderTexture*
{
    return nullptr;
}

void Application::AppRender::ClearRenderTarget(optional<uint> color, bool depth, bool stencil)
{
    UNUSED_VARIABLE(color);
    UNUSED_VARIABLE(depth);
    UNUSED_VARIABLE(stencil);
}

void Application::AppRender::EnableScissor(int x, int y, uint w, uint h)
{
    UNUSED_VARIABLE(x);
    UNUSED_VARIABLE(y);
    UNUSED_VARIABLE(w);
    UNUSED_VARIABLE(h);
}

void Application::AppRender::DisableScissor()
{
}

auto Application::AppRender::CreateDrawBuffer(bool is_static) -> RenderDrawBuffer*
{
    UNUSED_VARIABLE(is_static);

    return nullptr;
}

auto Application::AppRender::CreateEffect(EffectUsage usage, string_view name, string_view defines, const RenderEffectLoader& file_loader) -> RenderEffect*
{
    UNUSED_VARIABLE(name);
    UNUSED_VARIABLE(defines);
    UNUSED_VARIABLE(file_loader);

    return nullptr;
}

auto Application::AppInput::PollEvent(InputEvent& event) -> bool
{
    UNUSED_VARIABLE(event);

    return false;
}

void Application::AppInput::PushEvent(const InputEvent& event)
{
    UNUSED_VARIABLE(event);
}

void Application::AppInput::SetClipboardText(string_view text)
{
    UNUSED_VARIABLE(text);
}

auto Application::AppInput::GetClipboardText() -> string
{
    return string();
}

auto Application::AppAudio::IsEnabled() -> bool
{
    return false;
}

auto Application::AppAudio::GetStreamSize() -> uint
{
    RUNTIME_ASSERT(IsEnabled());

    return 0u;
}

auto Application::AppAudio::GetSilence() -> uchar
{
    RUNTIME_ASSERT(IsEnabled());

    return 0u;
}

void Application::AppAudio::SetSource(AudioStreamCallback stream_callback)
{
    UNUSED_VARIABLE(stream_callback);

    RUNTIME_ASSERT(IsEnabled());
}

auto Application::AppAudio::ConvertAudio(int format, int channels, int rate, vector<uchar>& buf) -> bool
{
    UNUSED_VARIABLE(format);
    UNUSED_VARIABLE(channels);
    UNUSED_VARIABLE(rate);
    UNUSED_VARIABLE(buf);

    RUNTIME_ASSERT(IsEnabled());

    return true;
}

void Application::AppAudio::MixAudio(uchar* output, uchar* buf, int volume)
{
    UNUSED_VARIABLE(output);
    UNUSED_VARIABLE(buf);
    UNUSED_VARIABLE(volume);

    RUNTIME_ASSERT(IsEnabled());
}

void Application::AppAudio::LockDevice()
{
    RUNTIME_ASSERT(IsEnabled());
}

void Application::AppAudio::UnlockDevice()
{
    RUNTIME_ASSERT(IsEnabled());
}

void MessageBox::ShowErrorMessage(string_view title, string_view message, string_view traceback)
{
    UNUSED_VARIABLE(title);
    UNUSED_VARIABLE(message);
    UNUSED_VARIABLE(traceback);
}
