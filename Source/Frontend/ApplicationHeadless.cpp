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
#include "GenericUtils.h"
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

#if FO_LINUX || FO_MAC
#include <signal.h>
#endif

Application* App;

#if FO_WINDOWS && FO_DEBUG
static _CrtMemState CrtMemState;
#endif

static const int MAX_ATLAS_WIDTH_ = 1024;
static const int MAX_ATLAS_HEIGHT_ = 1024;
static const int MAX_BONES_ = 32;
const int& AppRender::MAX_ATLAS_WIDTH {MAX_ATLAS_WIDTH_};
const int& AppRender::MAX_ATLAS_HEIGHT {MAX_ATLAS_HEIGHT_};
const int& AppRender::MAX_BONES {MAX_BONES_};
const int AppAudio::AUDIO_FORMAT_U8 = 0;
const int AppAudio::AUDIO_FORMAT_S16 = 1;

#if FO_LINUX || FO_MAC
static void SignalHandler(int sig)
{
    signal(sig, SignalHandler);

    App->Settings.Quit = true;
}
#endif

void InitApp(int argc, char** argv, bool client_mode)
{
    STACK_TRACE_ENTRY();

    // Ensure that we call init only once
    static std::once_flag once;
    auto first_call = false;
    std::call_once(once, [&first_call] { first_call = true; });
    if (!first_call) {
        throw AppInitException("InitApp must be called only once");
    }

#if FO_LINUX || FO_MAC
    const auto need_fork = [&] {
        for (int i = 0; i < argc; i++) {
            if (string_view(argv[i]) == "--fork") {
                return true;
            }
        }
        return false;
    };
    if (need_fork()) {
        GenericUtils::ForkProcess();
    }
#endif

    // Unhandled exceptions handler
#if FO_WINDOWS || FO_LINUX || FO_MAC
    {
        [[maybe_unused]] static backward::SignalHandling sh;
        assert(sh.loaded());
    }
#endif

    CreateGlobalData();

#if !FO_WEB
    if (const auto exe_path = DiskFileSystem::GetExePath()) {
        LogToFile(_str("{}.log", _str(exe_path.value()).extractFileName().eraseFileExtension()));
    }
    else {
        LogToFile(_str("{}.log", FO_DEV_NAME));
    }
#endif

    WriteLog("Starting {}", FO_GAME_NAME);

    App = new Application(argc, argv, client_mode);

#if FO_LINUX || FO_MAC
    const auto set_signal = [](int sig) {
        void (*ohandler)(int) = signal(sig, SignalHandler);
        if (ohandler != SIG_DFL) {
            signal(sig, ohandler);
        }
    };
    set_signal(SIGINT);
    set_signal(SIGTERM);
#endif
}

void ExitApp(bool success)
{
    STACK_TRACE_ENTRY();

    const auto code = success ? EXIT_SUCCESS : EXIT_FAILURE;
#if !FO_WEB && !FO_MAC && !FO_IOS && !FO_ANDROID
    std::quick_exit(code);
#else
    std::exit(code);
#endif
}

auto RenderEffect::CanBatch(const RenderEffect* other) const -> bool
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(other);

    return false;
}

Application::Application(int argc, char** argv, bool client_mode) :
    Settings(argc, argv, client_mode)
{
    STACK_TRACE_ENTRY();

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
}

void Application::OpenLink(string_view link)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(link);
}

void Application::SetImGuiEffect(RenderEffect* effect)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(effect);
}

auto Application::CreateChildWindow(int width, int height) -> AppWindow*
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(width);
    UNUSED_VARIABLE(height);

    return nullptr;
}

auto Application::CreateInternalWindow(int width, int height) -> WindowInternalHandle*
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(width);
    UNUSED_VARIABLE(height);

    return nullptr;
}

#if FO_IOS
void Application::SetMainLoopCallback(void (*callback)(void*))
{
    STACK_TRACE_ENTRY();
}
#endif

void Application::BeginFrame()
{
    STACK_TRACE_ENTRY();

    _onFrameBeginDispatcher();
}

void Application::EndFrame()
{
    STACK_TRACE_ENTRY();

    _onFrameEndDispatcher();

#ifdef TRACY_ENABLE
    FrameMark;
#endif
}

auto AppWindow::GetSize() const -> tuple<int, int>
{
    STACK_TRACE_ENTRY();

    auto w = 1000;
    auto h = 1000;

    return {w, h};
}

void AppWindow::SetSize(int w, int h)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(w);
    UNUSED_VARIABLE(h);
}

auto AppWindow::GetPosition() const -> tuple<int, int>
{
    STACK_TRACE_ENTRY();

    auto x = 0;
    auto y = 0;

    return {x, y};
}

void AppWindow::SetPosition(int x, int y)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(x);
    UNUSED_VARIABLE(y);
}

auto AppWindow::IsFocused() const -> bool
{
    STACK_TRACE_ENTRY();

    return true;
}

void AppWindow::Minimize()
{
    STACK_TRACE_ENTRY();
}

auto AppWindow::IsFullscreen() const -> bool
{
    STACK_TRACE_ENTRY();

    return false;
}

auto AppWindow::ToggleFullscreen(bool enable) -> bool
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(enable);

    return false;
}

void AppWindow::Blink()
{
    STACK_TRACE_ENTRY();
}

void AppWindow::AlwaysOnTop(bool enable)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(enable);
}

void AppWindow::GrabInput(bool enable)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(enable);
}

void AppWindow::Destroy()
{
    STACK_TRACE_ENTRY();
}

auto AppRender::CreateTexture(int width, int height, bool linear_filtered, bool with_depth) -> RenderTexture*
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(width);
    UNUSED_VARIABLE(height);
    UNUSED_VARIABLE(linear_filtered);
    UNUSED_VARIABLE(with_depth);

    return nullptr;
}

void AppRender::SetRenderTarget(RenderTexture* tex)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(tex);
}

auto AppRender::GetRenderTarget() -> RenderTexture*
{
    STACK_TRACE_ENTRY();

    return nullptr;
}

void AppRender::ClearRenderTarget(optional<uint> color, bool depth, bool stencil)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(color);
    UNUSED_VARIABLE(depth);
    UNUSED_VARIABLE(stencil);
}

void AppRender::EnableScissor(int x, int y, int width, int height)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(x);
    UNUSED_VARIABLE(y);
    UNUSED_VARIABLE(width);
    UNUSED_VARIABLE(height);
}

void AppRender::DisableScissor()
{
    STACK_TRACE_ENTRY();
}

auto AppRender::CreateDrawBuffer(bool is_static) -> RenderDrawBuffer*
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(is_static);

    return nullptr;
}

auto AppRender::CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& file_loader) -> RenderEffect*
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(usage);
    UNUSED_VARIABLE(name);
    UNUSED_VARIABLE(file_loader);

    return nullptr;
}

auto AppRender::CreateOrthoMatrix(float left, float right, float bottom, float top, float nearp, float farp) -> mat44
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(left);
    UNUSED_VARIABLE(right);
    UNUSED_VARIABLE(bottom);
    UNUSED_VARIABLE(top);
    UNUSED_VARIABLE(nearp);
    UNUSED_VARIABLE(farp);

    return {};
}

auto AppInput::GetMousePosition() const -> tuple<int, int>
{
    STACK_TRACE_ENTRY();

    auto x = 100;
    auto y = 100;
    return {x, y};
}

void AppInput::SetMousePosition(int x, int y, const AppWindow* relative_to)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(x);
    UNUSED_VARIABLE(y);
}

auto AppInput::PollEvent(InputEvent& ev) -> bool
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(ev);

    return false;
}

void AppInput::ClearEvents()
{
    STACK_TRACE_ENTRY();
}

void AppInput::PushEvent(const InputEvent& ev)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(ev);
}

void AppInput::SetClipboardText(string_view text)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(text);
}

auto AppInput::GetClipboardText() -> const string&
{
    STACK_TRACE_ENTRY();

    return _clipboardTextStorage;
}

auto AppAudio::IsEnabled() -> bool
{
    STACK_TRACE_ENTRY();

    return false;
}

auto AppAudio::GetStreamSize() -> uint
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(IsEnabled());

    return 0u;
}

auto AppAudio::GetSilence() -> uint8
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(IsEnabled());

    return 0u;
}

void AppAudio::SetSource(AudioStreamCallback stream_callback)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(stream_callback);

    RUNTIME_ASSERT(IsEnabled());
}

auto AppAudio::ConvertAudio(int format, int channels, int rate, vector<uint8>& buf) -> bool
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(format);
    UNUSED_VARIABLE(channels);
    UNUSED_VARIABLE(rate);
    UNUSED_VARIABLE(buf);

    RUNTIME_ASSERT(IsEnabled());

    return true;
}

void AppAudio::MixAudio(uint8* output, uint8* buf, int volume)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(output);
    UNUSED_VARIABLE(buf);
    UNUSED_VARIABLE(volume);

    RUNTIME_ASSERT(IsEnabled());
}

void AppAudio::LockDevice()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(IsEnabled());
}

void AppAudio::UnlockDevice()
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(IsEnabled());
}

void MessageBox::ShowErrorMessage(string_view title, string_view message, string_view traceback)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(title);
    UNUSED_VARIABLE(message);
    UNUSED_VARIABLE(traceback);
}
