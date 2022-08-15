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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "Log.h"
#include "StringUtils.h"
#include "Version-Include.h"
#include "WinApi-Include.h"

#if FO_WINDOWS || FO_LINUX || FO_MAC
#if !FO_WINDOWS
#if __has_include(<libunwind.h>)
#define BACKWARD_HAS_LIBUNWIND 1
#elif __has_include(<bfd.h>)
#define BACKWARD_HAS_BFD 1
#endif
#endif
#include "backward.hpp"
#if FO_WINDOWS
#undef MessageBox
#endif
#endif

Application* App;

#if FO_WINDOWS && FO_DEBUG
static _CrtMemState CrtMemState;
#endif

static const uint MAX_ATLAS_WIDTH_ = 1024;
static const uint MAX_ATLAS_HEIGHT_ = 1024;
static const uint MAX_BONES_ = 32;
const uint& AppRender::MAX_ATLAS_WIDTH {MAX_ATLAS_WIDTH_};
const uint& AppRender::MAX_ATLAS_HEIGHT {MAX_ATLAS_HEIGHT_};
const uint& AppRender::MAX_BONES {MAX_BONES_};
const int AppAudio::AUDIO_FORMAT_U8 = 0;
const int AppAudio::AUDIO_FORMAT_S16 = 1;

void InitApp(int argc, char** argv, string_view name_appendix)
{
    // Ensure that we call init only once
    static std::once_flag once;
    auto first_call = false;
    std::call_once(once, [&first_call] { first_call = true; });
    if (!first_call) {
        throw AppInitException("InitApp must be called only once");
    }

    // Unhandled exceptions handler
#if FO_WINDOWS || FO_LINUX || FO_MAC
    {
        [[maybe_unused]] static backward::SignalHandling sh;
        assert(sh.loaded());
    }
#endif

    CreateGlobalData();

    App = new Application(argc, argv, name_appendix);
}

void ExitApp(bool success)
{
    const auto code = success ? EXIT_SUCCESS : EXIT_FAILURE;
#if !FO_WEB && !FO_MAC && !FO_IOS && !FO_ANDROID
    std::quick_exit(code);
#else
    std::exit(code);
#endif
}

void ReportExceptionAndExit(const std::exception& ex)
{
    if (!BreakIntoDebugger(ex.what())) {
        WriteLog(LogType::Error, "\n{}\n", ex.what());
        CreateDumpMessage("FatalException", ex.what());
        MessageBox::ShowErrorMessage("Fatal Error", ex.what(), GetStackTrace());
    }

    ExitApp(false);
}

void ReportExceptionAndContinue(const std::exception& ex)
{
    if (BreakIntoDebugger(ex.what())) {
        return;
    }

    WriteLog(LogType::Error, "\n{}\n", ex.what());

#if FO_DEBUG
    MessageBox::ShowErrorMessage("Error", ex.what(), GetStackTrace());
#endif
}

auto RenderEffect::CanBatch(const RenderEffect* other) const -> bool
{
    UNUSED_VARIABLE(other);

    return false;
}

Application::Application(int argc, char** argv, string_view name_appendix) : Settings(argc, argv)
{
    UNUSED_VARIABLE(_time);
    UNUSED_VARIABLE(_timeFrequency);
    UNUSED_VARIABLE(_isTablet);
    UNUSED_VARIABLE(_mouseCanUseGlobalState);
    UNUSED_VARIABLE(_pendingMouseLeaveFrame);
    UNUSED_VARIABLE(_mouseButtonsDown);
    UNUSED_VARIABLE(_imguiDrawBuf);
    UNUSED_VARIABLE(_imguiEffect);
    UNUSED_VARIABLE(_nonConstHelper);
    UNUSED_VARIABLE(MainWindow._windowHandle);

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

auto Application::GetName() const -> string_view
{
    return _name;
}

auto Application::CreateChildWindow(int width, int height) -> AppWindow*
{
    UNUSED_VARIABLE(width);
    UNUSED_VARIABLE(height);

    return nullptr;
}

auto Application::CreateInternalWindow(int width, int height) -> WindowInternalHandle*
{
    UNUSED_VARIABLE(width);
    UNUSED_VARIABLE(height);

    return nullptr;
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

auto AppWindow::GetSize() const -> tuple<int, int>
{
    auto w = 1000;
    auto h = 1000;

    return {w, h};
}

void AppWindow::SetSize(int w, int h)
{
    UNUSED_VARIABLE(w);
    UNUSED_VARIABLE(h);
}

auto AppWindow::GetPosition() const -> tuple<int, int>
{
    auto x = 0;
    auto y = 0;

    return {x, y};
}

void AppWindow::SetPosition(int x, int y)
{
    UNUSED_VARIABLE(x);
    UNUSED_VARIABLE(y);
}

auto AppWindow::IsFocused() const -> bool
{
    return true;
}

void AppWindow::Minimize()
{
}

auto AppWindow::IsFullscreen() const -> bool
{
    return false;
}

auto AppWindow::ToggleFullscreen(bool enable) -> bool
{
    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(enable);

    return false;
}

void AppWindow::Blink()
{
}

void AppWindow::AlwaysOnTop(bool enable)
{
    UNUSED_VARIABLE(enable);
}

void AppWindow::Destroy()
{
}

auto AppRender::CreateTexture(uint width, uint height, bool linear_filtered, bool with_depth) -> RenderTexture*
{
    UNUSED_VARIABLE(width);
    UNUSED_VARIABLE(height);
    UNUSED_VARIABLE(linear_filtered);
    UNUSED_VARIABLE(with_depth);

    return nullptr;
}

void AppRender::SetRenderTarget(RenderTexture* tex)
{
    UNUSED_VARIABLE(tex);
}

auto AppRender::GetRenderTarget() -> RenderTexture*
{
    return nullptr;
}

void AppRender::ClearRenderTarget(optional<uint> color, bool depth, bool stencil)
{
    UNUSED_VARIABLE(color);
    UNUSED_VARIABLE(depth);
    UNUSED_VARIABLE(stencil);
}

void AppRender::EnableScissor(int x, int y, uint w, uint h)
{
    UNUSED_VARIABLE(x);
    UNUSED_VARIABLE(y);
    UNUSED_VARIABLE(w);
    UNUSED_VARIABLE(h);
}

void AppRender::DisableScissor()
{
}

auto AppRender::CreateDrawBuffer(bool is_static) -> RenderDrawBuffer*
{
    UNUSED_VARIABLE(is_static);

    return nullptr;
}

auto AppRender::CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& file_loader) -> RenderEffect*
{
    UNUSED_VARIABLE(name);
    UNUSED_VARIABLE(file_loader);

    return nullptr;
}

auto AppInput::GetMousePosition() const -> tuple<int, int>
{
    auto x = 100;
    auto y = 100;
    return {x, y};
}

void AppInput::SetMousePosition(int x, int y, const AppWindow* relative_to)
{
    UNUSED_VARIABLE(x);
    UNUSED_VARIABLE(y);
}

auto AppInput::PollEvent(InputEvent& ev) -> bool
{
    UNUSED_VARIABLE(ev);

    return false;
}

void AppInput::PushEvent(const InputEvent& ev)
{
    UNUSED_VARIABLE(ev);
}

void AppInput::SetClipboardText(string_view text)
{
    UNUSED_VARIABLE(text);
}

auto AppInput::GetClipboardText() -> const string&
{
    return _clipboardTextStorage;
}

auto AppAudio::IsEnabled() -> bool
{
    return false;
}

auto AppAudio::GetStreamSize() -> uint
{
    RUNTIME_ASSERT(IsEnabled());

    return 0u;
}

auto AppAudio::GetSilence() -> uchar
{
    RUNTIME_ASSERT(IsEnabled());

    return 0u;
}

void AppAudio::SetSource(AudioStreamCallback stream_callback)
{
    UNUSED_VARIABLE(stream_callback);

    RUNTIME_ASSERT(IsEnabled());
}

auto AppAudio::ConvertAudio(int format, int channels, int rate, vector<uchar>& buf) -> bool
{
    UNUSED_VARIABLE(format);
    UNUSED_VARIABLE(channels);
    UNUSED_VARIABLE(rate);
    UNUSED_VARIABLE(buf);

    RUNTIME_ASSERT(IsEnabled());

    return true;
}

void AppAudio::MixAudio(uchar* output, uchar* buf, int volume)
{
    UNUSED_VARIABLE(output);
    UNUSED_VARIABLE(buf);
    UNUSED_VARIABLE(volume);

    RUNTIME_ASSERT(IsEnabled());
}

void AppAudio::LockDevice()
{
    RUNTIME_ASSERT(IsEnabled());
}

void AppAudio::UnlockDevice()
{
    RUNTIME_ASSERT(IsEnabled());
}

void MessageBox::ShowErrorMessage(string_view title, string_view message, string_view traceback)
{
    UNUSED_VARIABLE(title);
    UNUSED_VARIABLE(message);
    UNUSED_VARIABLE(traceback);
}
