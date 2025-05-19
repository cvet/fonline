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

FO_BEGIN_NAMESPACE();

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
    FO_STACK_TRACE_ENTRY();

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
    TracySetProgramName(FO_NICE_NAME);
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
    FO_STACK_TRACE_ENTRY();

    const auto code = success ? EXIT_SUCCESS : EXIT_FAILURE;
#if !FO_WEB && !FO_MAC && !FO_IOS && !FO_ANDROID
    std::quick_exit(code);
#else
    std::exit(code);
#endif
}

auto RenderEffect::CanBatch(const RenderEffect* other) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(other);

    return false;
}

Application::Application(int argc, char** argv, AppInitFlags flags) :
    Settings(argc, argv)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(flags);
    ignore_unused(_time);
    ignore_unused(_timeFrequency);
    ignore_unused(_isTablet);
    ignore_unused(_mouseCanUseGlobalState);
    ignore_unused(_pendingMouseLeaveFrame);
    ignore_unused(_mouseButtonsDown);
    ignore_unused(_imguiDrawBuf);
    ignore_unused(_imguiEffect);
    ignore_unused(_nonConstHelper);
    ignore_unused(MainWindow._windowHandle);
    ignore_unused(MainWindow._grabbed);

    // Skip SDL allocations from profiling
#if FO_WINDOWS && FO_DEBUG
    ::_CrtMemCheckpoint(&CrtMemState);
#endif
}

void Application::OpenLink(string_view link)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(link);
}

void Application::SetImGuiEffect(unique_ptr<RenderEffect> effect)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(effect);
}

auto Application::CreateChildWindow(isize size) -> AppWindow*
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(size);

    return nullptr;
}

auto Application::CreateInternalWindow(isize size) -> WindowInternalHandle*
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(size);

    return nullptr;
}

#if FO_IOS
void Application::SetMainLoopCallback(void (*callback)(void*))
{
    FO_STACK_TRACE_ENTRY();
}
#endif

void Application::BeginFrame()
{
    FO_STACK_TRACE_ENTRY();

    _onFrameBeginDispatcher();
}

void Application::EndFrame()
{
    FO_STACK_TRACE_ENTRY();

    _onFrameEndDispatcher();

#if FO_TRACY
    FrameMark;
#endif
}

void Application::RequestQuit() noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (bool expected = false; _quit.compare_exchange_strong(expected, true)) {
        WriteLog("Quit requested");

        _quitEvent.notify_all();
    }
}

void Application::WaitForRequestedQuit()
{
    FO_STACK_TRACE_ENTRY();

    auto locker = std::unique_lock {_quitLocker};

    while (!_quit) {
        _quitEvent.wait(locker);
    }
}

auto AppWindow::GetSize() const -> isize
{
    FO_STACK_TRACE_ENTRY();

    constexpr auto width = 1000;
    constexpr auto height = 1000;

    return {width, height};
}

void AppWindow::SetSize(isize size)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(size);
}

auto AppWindow::GetScreenSize() const -> isize
{
    FO_STACK_TRACE_ENTRY();

    return {1000, 1000};
}

void AppWindow::SetScreenSize(isize size)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(size);
}

auto AppWindow::GetPosition() const -> ipos
{
    FO_STACK_TRACE_ENTRY();

    constexpr auto x = 0;
    constexpr auto y = 0;

    return {x, y};
}

void AppWindow::SetPosition(ipos pos)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(pos);
}

auto AppWindow::IsFocused() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return true;
}

void AppWindow::Minimize()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();
}

auto AppWindow::IsFullscreen() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return false;
}

auto AppWindow::ToggleFullscreen(bool enable) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(enable);

    return false;
}

void AppWindow::Blink()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();
}

void AppWindow::AlwaysOnTop(bool enable)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(enable);
}

void AppWindow::GrabInput(bool enable)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(enable);
}

void AppWindow::Destroy()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();
}

auto AppRender::CreateTexture(isize size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture>
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(size);
    ignore_unused(linear_filtered);
    ignore_unused(with_depth);

    return nullptr;
}

void AppRender::SetRenderTarget(RenderTexture* tex)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(tex);
}

auto AppRender::GetRenderTarget() -> RenderTexture*
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    return nullptr;
}

void AppRender::ClearRenderTarget(optional<ucolor> color, bool depth, bool stencil)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(color);
    ignore_unused(depth);
    ignore_unused(stencil);
}

void AppRender::EnableScissor(irect rect)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(rect);
}

void AppRender::DisableScissor()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();
}

auto AppRender::CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer>
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(is_static);

    return nullptr;
}

auto AppRender::CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& file_loader) -> unique_ptr<RenderEffect>
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(usage);
    ignore_unused(name);
    ignore_unused(file_loader);

    return nullptr;
}

auto AppRender::CreateOrthoMatrix(float left, float right, float bottom, float top, float nearp, float farp) -> mat44
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(left);
    ignore_unused(right);
    ignore_unused(bottom);
    ignore_unused(top);
    ignore_unused(nearp);
    ignore_unused(farp);

    return {};
}

auto AppRender::IsRenderTargetFlipped() -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    return false;
}

auto AppInput::GetMousePosition() const -> ipos
{
    FO_STACK_TRACE_ENTRY();

    constexpr auto x = 100;
    constexpr auto y = 100;

    return {x, y};
}

void AppInput::SetMousePosition(ipos pos, const AppWindow* relative_to)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(pos);
    ignore_unused(relative_to);
}

auto AppInput::PollEvent(InputEvent& ev) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(ev);

    return false;
}

void AppInput::ClearEvents()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();
}

void AppInput::PushEvent(const InputEvent& ev, bool push_to_this_frame)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(ev);
    ignore_unused(push_to_this_frame);
}

void AppInput::SetClipboardText(string_view text)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(text);
}

auto AppInput::GetClipboardText() -> const string&
{
    FO_STACK_TRACE_ENTRY();

    return _clipboardTextStorage;
}

auto AppAudio::IsEnabled() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return false;
}

void AppAudio::SetSource(AudioStreamCallback stream_callback)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    [[maybe_unused]] auto unused = std::move(stream_callback);

    FO_RUNTIME_ASSERT(IsEnabled());
}

auto AppAudio::ConvertAudio(int format, int channels, int rate, vector<uint8>& buf) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(format);
    ignore_unused(channels);
    ignore_unused(rate);
    ignore_unused(buf);

    FO_RUNTIME_ASSERT(IsEnabled());

    return true;
}

void AppAudio::MixAudio(uint8* output, const uint8* buf, size_t len, int volume)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(output);
    ignore_unused(buf);
    ignore_unused(len);
    ignore_unused(volume);

    FO_RUNTIME_ASSERT(IsEnabled());
}

void AppAudio::LockDevice()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    FO_RUNTIME_ASSERT(IsEnabled());
}

void AppAudio::UnlockDevice()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    FO_RUNTIME_ASSERT(IsEnabled());
}

void MessageBox::ShowErrorMessage(string_view message, string_view traceback, bool fatal_error)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(message);
    ignore_unused(traceback);
    ignore_unused(fatal_error);
}

FO_END_NAMESPACE();
