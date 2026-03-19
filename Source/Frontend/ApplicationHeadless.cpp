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

#include "Application.h"

FO_BEGIN_NAMESPACE

static unique_ptr<Null_Renderer> HeadlessRenderer {};
static raw_ptr<RenderTexture> HeadlessRenderTarget {};
static vector<unique_ptr<HeadlessWindowStub>> HeadlessWindowStubs {};

static constexpr int32 MAX_ATLAS_WIDTH_ = 2048;
static constexpr int32 MAX_ATLAS_HEIGHT_ = 2048;
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
    ignore_unused(MainWindow._grabbed);

    HeadlessRenderer = SafeAlloc::MakeUnique<Null_Renderer>();
    HeadlessRenderer->Init(Settings, nullptr);
    MainWindow._windowHandle = CreateInternalWindow({Settings.ScreenWidth, Settings.ScreenHeight});
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

    auto handle = SafeAlloc::MakeUnique<HeadlessWindowStub>();
    handle->Size = size;

    auto* ptr = handle.get();
    HeadlessWindowStubs.emplace_back(std::move(handle));

    return reinterpret_cast<WindowInternalHandle*>(ptr);
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

void Application::RequestQuit(bool success) noexcept
{
    FO_STACK_TRACE_ENTRY();

    if (!success) {
        _quitSuccess.store(false);
    }

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

    return ResolveWindowStub()->Size;
}

void AppWindow::SetSize(isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ResolveWindowStub()->Size = size;
    _onWindowSizeChangedDispatcher();
}

auto AppWindow::GetScreenSize() const -> isize32
{
    FO_STACK_TRACE_ENTRY();

    return {App->Settings.ScreenWidth, App->Settings.ScreenHeight};
}

void AppWindow::SetScreenSize(isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (size.width != App->Settings.ScreenWidth || size.height != App->Settings.ScreenHeight) {
        App->Settings.ScreenWidth = size.width;
        App->Settings.ScreenHeight = size.height;

        _onScreenSizeChangedDispatcher();
    }
}

auto AppWindow::GetPosition() const -> ipos32
{
    FO_STACK_TRACE_ENTRY();

    return ResolveWindowStub()->Position;
}

void AppWindow::SetPosition(ipos32 pos)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ResolveWindowStub()->Position = pos;
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

    ResolveWindowStub()->Minimized = true;
}

auto AppWindow::IsFullscreen() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return ResolveWindowStub()->Fullscreen;
}

auto AppWindow::ToggleFullscreen(bool enable) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    auto* window = ResolveWindowStub();
    const bool changed = window->Fullscreen != enable;
    window->Fullscreen = enable;

    return changed;
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

    ResolveWindowStub()->AlwaysOnTop = enable;
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

    if (_windowHandle && this != &App->MainWindow) {
        std::erase_if(HeadlessWindowStubs, [handle = _windowHandle.get()](const auto& entry) { return entry.get() == static_cast<HeadlessWindowStub*>(handle); });
        _windowHandle = nullptr;
    }
}

auto AppWindow::ResolveWindowHandle() const -> WindowInternalHandle*
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(_windowHandle);

    return _windowHandle.get_no_const();
}

auto AppWindow::ResolveWindowStub() const -> HeadlessWindowStub*
{
    FO_STACK_TRACE_ENTRY();

    return static_cast<HeadlessWindowStub*>(ResolveWindowHandle());
}

auto AppRender::CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture>
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    return HeadlessRenderer->CreateTexture(size, linear_filtered, with_depth);
}

void AppRender::SetRenderTarget(RenderTexture* tex)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    HeadlessRenderTarget = tex;
    HeadlessRenderer->SetRenderTarget(tex);
}

auto AppRender::GetRenderTarget() -> RenderTexture*
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    return HeadlessRenderTarget.get();
}

void AppRender::ClearRenderTarget(optional<ucolor> color, bool depth, bool stencil)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    HeadlessRenderer->ClearRenderTarget(color, depth, stencil);
}

void AppRender::EnableScissor(irect32 rect)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    HeadlessRenderer->EnableScissor(rect);
}

void AppRender::DisableScissor()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    HeadlessRenderer->DisableScissor();
}

auto AppRender::CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer>
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    return HeadlessRenderer->CreateDrawBuffer(is_static);
}

auto AppRender::CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& file_loader) -> unique_ptr<RenderEffect>
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    return HeadlessRenderer->CreateEffect(usage, name, file_loader);
}

auto AppRender::CreateOrthoMatrix(float32 left, float32 right, float32 bottom, float32 top, float32 nearp, float32 farp) -> mat44
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    return HeadlessRenderer->CreateOrthoMatrix(left, right, bottom, top, nearp, farp);
}

auto AppRender::IsRenderTargetFlipped() -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    return HeadlessRenderer->IsRenderTargetFlipped();
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

FO_END_NAMESPACE
