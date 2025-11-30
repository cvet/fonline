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

FO_BEGIN_NAMESPACE();

static constexpr int32 MAX_ATLAS_WIDTH_ = 1024;
static constexpr int32 MAX_ATLAS_HEIGHT_ = 1024;
static constexpr int32 MAX_BONES_ = 32;
const int32& AppRender::MAX_ATLAS_WIDTH {MAX_ATLAS_WIDTH_};
const int32& AppRender::MAX_ATLAS_HEIGHT {MAX_ATLAS_HEIGHT_};
const int32& AppRender::MAX_BONES {MAX_BONES_};
const int32 AppAudio::AUDIO_FORMAT_U8 = 0;
const int32 AppAudio::AUDIO_FORMAT_S16 = 1;

Application::Application(GlobalSettings&& settings, AppInitFlags flags) :
    Settings {std::move(settings)}
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
}

void Application::OpenLink(string_view link)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(link);
}

void Application::LoadImGuiEffect(const FileSystem& resources)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(resources);
}

auto Application::CreateChildWindow(isize32 size) -> AppWindow*
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(size);

    return nullptr;
}

auto Application::CreateInternalWindow(isize32 size) -> WindowInternalHandle*
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

auto AppWindow::GetSize() const -> isize32
{
    FO_STACK_TRACE_ENTRY();

    constexpr auto width = 1000;
    constexpr auto height = 1000;

    return {width, height};
}

void AppWindow::SetSize(isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(size);
}

auto AppWindow::GetScreenSize() const -> isize32
{
    FO_STACK_TRACE_ENTRY();

    return {1000, 1000};
}

void AppWindow::SetScreenSize(isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(size);
}

auto AppWindow::GetPosition() const -> ipos32
{
    FO_STACK_TRACE_ENTRY();

    constexpr auto x = 0;
    constexpr auto y = 0;

    return {x, y};
}

void AppWindow::SetPosition(ipos32 pos)
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

auto AppRender::CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture>
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

void AppRender::EnableScissor(irect32 rect)
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

auto AppRender::CreateOrthoMatrix(float32 left, float32 right, float32 bottom, float32 top, float32 nearp, float32 farp) -> mat44
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

auto AppInput::GetMousePosition() const -> ipos32
{
    FO_STACK_TRACE_ENTRY();

    constexpr auto x = 100;
    constexpr auto y = 100;

    return {x, y};
}

void AppInput::SetMousePosition(ipos32 pos, const AppWindow* relative_to)
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

auto AppAudio::ConvertAudio(int32 format, int32 channels, int32 rate, vector<uint8>& buf) -> bool
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

void AppAudio::MixAudio(uint8* output, const uint8* buf, size_t len, int32 volume)
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

void Application::ShowErrorMessage(string_view message, string_view traceback, bool fatal_error)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(message);
    ignore_unused(traceback);
    ignore_unused(fatal_error);
}

void Application::ShowProgressWindow(string_view text, const ProgressWindowCallback& callback)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(text);

    callback();
}

void Application::ChooseOptionsWindow(string_view title, const vector<string>& options, set<int32>& selected)
{
    FO_STACK_TRACE_ENTRY();

    if (options.empty()) {
        return;
    }

    std::cout << "=== " << title << " ===\n";

    for (size_t i = 0; i < options.size(); i++) {
        std::cout << (i + 1) << ") " << options[i] << "\n";
    }

    if (!selected.empty()) {
        std::cout << "Default input:";

        for (const int32 sel : selected) {
            std::cout << " " << (sel + 1);
        }

        std::cout << "\n";
    }

    std::cout << "Type numbers separated by space: ";

    string str;
    std::getline(std::cin, str);

    const auto in_selected = strex(str).split_to_int32(' ');

    if (!in_selected.empty()) {
        selected.clear();

        for (const int32 sel : in_selected) {
            if (sel >= 1 && sel < numeric_cast<int32>(options.size() + 1)) {
                selected.emplace(sel - 1);
            }
        }
    }
}

FO_END_NAMESPACE();
