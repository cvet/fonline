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

#include "Application.h"
#include "DiskFileSystem.h"
#include "Log.h"
#include "Platform.h"
#include "StringUtils.h"
#include "Version-Include.h"

#if FO_WINDOWS || FO_LINUX || FO_MAC
#if !FO_WINDOWS
#if __has_include(<libunwind.h>)
#define BACKWARD_HAS_LIBUNWIND 1
#elif __has_include(<bfd.h>)
#define BACKWARD_HAS_BFD 1
#endif
#endif
#include "backward.hpp"
#endif

#include "WinApiUndef-Include.h"

#if FO_LINUX || FO_MAC
#include <signal.h>
#endif

Application* App;

#if FO_WINDOWS && FO_DEBUG
static _CrtMemState CrtMemState;
#endif

static constexpr int MAX_ATLAS_WIDTH_ = 1024;
static constexpr int MAX_ATLAS_HEIGHT_ = 1024;
static constexpr int MAX_BONES_ = 32;
const int& AppRender::MAX_ATLAS_WIDTH {MAX_ATLAS_WIDTH_};
const int& AppRender::MAX_ATLAS_HEIGHT {MAX_ATLAS_HEIGHT_};
const int& AppRender::MAX_BONES {MAX_BONES_};
const int AppAudio::AUDIO_FORMAT_U8 = 0;
const int AppAudio::AUDIO_FORMAT_S16 = 1;

#if FO_LINUX || FO_MAC
static void SignalHandler(int sig)
{
    signal(sig, SignalHandler);

    App->RequestQuit();
}
#endif

void InitApp(int argc, char** argv, AppInitFlags flags)
{
    STACK_TRACE_ENTRY();

    SetThisThreadName("Main");

    // Ensure that we call init only once
    static std::once_flag once;
    auto first_call = false;
    std::call_once(once, [&first_call] { first_call = true; });

    if (!first_call) {
        throw AppInitException("InitApp must be called only once");
    }

    const auto need_fork = [&] {
        for (int i = 0; i < argc; i++) {
            if (string_view(argv[i]) == "--fork") {
                return true;
            }
        }
        return false;
    };
    if (need_fork()) {
        Platform::ForkProcess();
    }

    // Unhandled exceptions handler
#if FO_WINDOWS || FO_LINUX || FO_MAC
    if (!IsRunInDebugger()) {
        [[maybe_unused]] static backward::SignalHandling sh;
        assert(sh.loaded());
    }
#endif

    CreateGlobalData();

#if FO_TRACY
    TracySetProgramName(FO_GAME_NAME);
#endif

#if !FO_WEB
    if (const auto exe_path = Platform::GetExePath()) {
        LogToFile(strex("{}.log", strex(exe_path.value()).extractFileName().eraseFileExtension()));
    }
    else {
        LogToFile(strex("{}.log", FO_DEV_NAME));
    }
#endif

    if (IsEnumSet(flags, AppInitFlags::DisableLogTags)) {
        LogDisableTags();
    }

    WriteLog("Starting {}", FO_NICE_NAME);

    App = SafeAlloc::MakeRaw<Application>(argc, argv, flags);

#if FO_LINUX || FO_MAC
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
#endif
}

void ExitApp(bool success) noexcept
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

Application::Application(int argc, char** argv, AppInitFlags flags) :
    Settings(argc, argv)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(flags);
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
    UNUSED_VARIABLE(MainWindow._grabbed);

    // Skip SDL allocations from profiling
#if FO_WINDOWS && FO_DEBUG
    ::_CrtMemCheckpoint(&CrtMemState);
#endif
}

void Application::OpenLink(string_view link)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(link);
}

void Application::SetImGuiEffect(unique_ptr<RenderEffect> effect)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(effect);
}

auto Application::CreateChildWindow(isize size) -> AppWindow*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(size);

    return nullptr;
}

auto Application::CreateInternalWindow(isize size) -> WindowInternalHandle*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(size);

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

#if FO_TRACY
    FrameMark;
#endif
}

void Application::RequestQuit() noexcept
{
    STACK_TRACE_ENTRY();

    if (bool expected = false; _quit.compare_exchange_strong(expected, true)) {
        WriteLog("Quit requested");

        _quitEvent.notify_all();
    }
}

void Application::WaitForRequestedQuit()
{
    STACK_TRACE_ENTRY();

    auto locker = std::unique_lock {_quitLocker};

    while (!_quit) {
        _quitEvent.wait(locker);
    }
}

auto AppWindow::GetSize() const -> isize
{
    STACK_TRACE_ENTRY();

    constexpr auto width = 1000;
    constexpr auto height = 1000;

    return {width, height};
}

void AppWindow::SetSize(isize size)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(size);
}

auto AppWindow::GetScreenSize() const -> isize
{
    STACK_TRACE_ENTRY();

    return {1000, 1000};
}

void AppWindow::SetScreenSize(isize size)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(size);
}

auto AppWindow::GetPosition() const -> ipos
{
    STACK_TRACE_ENTRY();

    constexpr auto x = 0;
    constexpr auto y = 0;

    return {x, y};
}

void AppWindow::SetPosition(ipos pos)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(pos);
}

auto AppWindow::IsFocused() const -> bool
{
    STACK_TRACE_ENTRY();

    return true;
}

void AppWindow::Minimize()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();
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

    NON_CONST_METHOD_HINT();
}

void AppWindow::AlwaysOnTop(bool enable)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(enable);
}

void AppWindow::GrabInput(bool enable)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(enable);
}

void AppWindow::Destroy()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();
}

auto AppRender::CreateTexture(isize size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(size);
    UNUSED_VARIABLE(linear_filtered);
    UNUSED_VARIABLE(with_depth);

    return nullptr;
}

void AppRender::SetRenderTarget(RenderTexture* tex)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(tex);
}

auto AppRender::GetRenderTarget() -> RenderTexture*
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return nullptr;
}

void AppRender::ClearRenderTarget(optional<ucolor> color, bool depth, bool stencil)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(color);
    UNUSED_VARIABLE(depth);
    UNUSED_VARIABLE(stencil);
}

void AppRender::EnableScissor(irect rect)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(rect);
}

void AppRender::DisableScissor()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();
}

auto AppRender::CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(is_static);

    return nullptr;
}

auto AppRender::CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& file_loader) -> unique_ptr<RenderEffect>
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(usage);
    UNUSED_VARIABLE(name);
    UNUSED_VARIABLE(file_loader);

    return nullptr;
}

auto AppRender::CreateOrthoMatrix(float left, float right, float bottom, float top, float nearp, float farp) -> mat44
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(left);
    UNUSED_VARIABLE(right);
    UNUSED_VARIABLE(bottom);
    UNUSED_VARIABLE(top);
    UNUSED_VARIABLE(nearp);
    UNUSED_VARIABLE(farp);

    return {};
}

auto AppRender::IsRenderTargetFlipped() -> bool
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    return false;
}

auto AppInput::GetMousePosition() const -> ipos
{
    STACK_TRACE_ENTRY();

    constexpr auto x = 100;
    constexpr auto y = 100;

    return {x, y};
}

void AppInput::SetMousePosition(ipos pos, const AppWindow* relative_to)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(pos);
    UNUSED_VARIABLE(relative_to);
}

auto AppInput::PollEvent(InputEvent& ev) -> bool
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(ev);

    return false;
}

void AppInput::ClearEvents()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();
}

void AppInput::PushEvent(const InputEvent& ev, bool push_to_this_frame)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(ev);
    UNUSED_VARIABLE(push_to_this_frame);
}

void AppInput::SetClipboardText(string_view text)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(text);
}

auto AppInput::GetClipboardText() -> const string&
{
    STACK_TRACE_ENTRY();

    return _clipboardTextStorage;
}

auto AppAudio::IsEnabled() const -> bool
{
    STACK_TRACE_ENTRY();

    return false;
}

void AppAudio::SetSource(AudioStreamCallback stream_callback)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    [[maybe_unused]] auto unused = std::move(stream_callback);

    RUNTIME_ASSERT(IsEnabled());
}

auto AppAudio::ConvertAudio(int format, int channels, int rate, vector<uint8>& buf) -> bool
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(format);
    UNUSED_VARIABLE(channels);
    UNUSED_VARIABLE(rate);
    UNUSED_VARIABLE(buf);

    RUNTIME_ASSERT(IsEnabled());

    return true;
}

void AppAudio::MixAudio(uint8* output, const uint8* buf, size_t len, int volume)
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    UNUSED_VARIABLE(output);
    UNUSED_VARIABLE(buf);
    UNUSED_VARIABLE(len);
    UNUSED_VARIABLE(volume);

    RUNTIME_ASSERT(IsEnabled());
}

void AppAudio::LockDevice()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(IsEnabled());
}

void AppAudio::UnlockDevice()
{
    STACK_TRACE_ENTRY();

    NON_CONST_METHOD_HINT();

    RUNTIME_ASSERT(IsEnabled());
}

void MessageBox::ShowErrorMessage(string_view message, string_view traceback, bool fatal_error)
{
    STACK_TRACE_ENTRY();

    UNUSED_VARIABLE(message);
    UNUSED_VARIABLE(traceback);
    UNUSED_VARIABLE(fatal_error);
}
