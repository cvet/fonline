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

struct Application::Context
{
    Null_Renderer HeadlessRenderer {};
    nptr<RenderTexture> HeadlessRenderTarget {};
    vector<unique_ptr<HeadlessWindowStub>> HeadlessWindowStubs {};
};

int32_t AppRender::MAX_ATLAS_WIDTH {8192};
int32_t AppRender::MAX_ATLAS_HEIGHT {8192};
int32_t AppRender::MAX_BONES {32};
const int32_t AppAudio::AUDIO_FORMAT_U8 = 0;
const int32_t AppAudio::AUDIO_FORMAT_S16 = 1;

Application::Application(GlobalSettings&& settings, AppInitFlags flags) :
    Settings {std::move(settings)},
    MainWindow {ptr<Application> {this}},
    Render {ptr<Application> {this}},
    Input {ptr<Application> {this}},
    Audio {ptr<Application> {this}},
    _ctx {SafeAlloc::MakeUnique<Context>()}
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
    ignore_unused(MainWindow._grabbed);

    _ctx->HeadlessRenderer.Init(Settings, nullptr);
    MainWindow._windowHandle = CreateInternalWindow({Settings.ScreenWidth, Settings.ScreenHeight});
    MainWindow._title = Settings.GameName;
    MainWindow._virtualSize = {Settings.ScreenWidth, Settings.ScreenHeight};
    MainWindow._virtualScreenSize = {Settings.ScreenWidth, Settings.ScreenHeight};
    ptr<AppWindow> main_window = &MainWindow;
    _allWindows.emplace_back(main_window);
    _activeWindow = main_window;
}

Application::~Application()
{
    FO_STACK_TRACE_ENTRY();

    _imguiTextures.clear();
    _imguiEffect.reset();
    _imguiDrawBuf.reset();
    _ctx->HeadlessRenderTarget = nullptr;

    for (size_t i = 0; i != _childWindows.size(); ++i) {
        auto window = _childWindows[i].as_ptr();

        window->_virtualRenderTex.reset();
    }

    _childWindows.clear();
    _allWindows.clear();
    ptr<AppWindow> main_window = &MainWindow;
    _activeWindow = main_window;
    _currentRenderingWindow = nullptr;
    _previousRenderTarget = nullptr;

    MainWindow._windowHandle = nullptr;
    _ctx->HeadlessWindowStubs.clear();
}

void Application::OpenLink(string_view link)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(link);
}

void Application::LoadImGuiEffect(const FileSystem& resources)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(resources);
}

auto Application::CreateChildWindow(isize32 size, string_view title) -> ptr<AppWindow>
{
    FO_STACK_TRACE_ENTRY();

    if (size.width <= 0 || size.height <= 0) {
        size = {Settings.ScreenWidth, Settings.ScreenHeight};
    }

    ptr<Application> app = this;
    unique_ptr<AppWindow> window = SafeAlloc::MakeUnique<AppWindow>(app);
    window->_isVirtual = true;
    window->_virtualSize = size;
    window->_virtualScreenSize = size;
    window->_virtualLayoutSize = size;
    window->_title = title.empty() ? strex("Window {}", _childWindows.size() + 1).str() : string {title};

    auto window_ptr = window.as_ptr();
    _allWindows.emplace_back(window_ptr);
    _childWindows.emplace_back(std::move(window));
    _activeWindow = window_ptr;

    return window_ptr;
}

void Application::DestroyChildWindow(nptr<AppWindow> nullable_window)
{
    FO_STACK_TRACE_ENTRY();

    ptr<AppWindow> main_window = &MainWindow;

    if (!nullable_window || nullable_window == main_window) {
        return;
    }

    auto window = nullable_window.as_ptr();

    std::erase_if(_allWindows, [&](const auto& entry) { return entry == window; });

    const auto it = std::ranges::find_if(_childWindows, [&](const auto& entry) { return entry.as_ptr() == window; });

    if (it == _childWindows.end()) {
        return;
    }

    if (_activeWindow == window) {
        _activeWindow = main_window;
    }

    if (_currentRenderingWindow == window) {
        _currentRenderingWindow = nullptr;
    }

    (*it)->_virtualRenderTex.reset();
    _childWindows.erase(it);
}

void Application::SetActiveWindow(nptr<AppWindow> window)
{
    FO_STACK_TRACE_ENTRY();

    if (!window) {
        ptr<AppWindow> main_window = &MainWindow;
        _activeWindow = main_window;
        return;
    }

    _activeWindow = window;
}

void Application::EnsureVirtualRenderTexture(ptr<AppWindow> window, isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(window->_isVirtual, "Window is not virtual");

    if (size.width <= 0 || size.height <= 0) {
        size = window->_virtualSize;
    }

    window->_virtualLayoutSize = size;

    bool recreate_texture = true;
    auto nullable_virtual_render_tex = window->GetRenderTexture();

    if (nullable_virtual_render_tex) {
        auto virtual_render_tex = nullable_virtual_render_tex.as_ptr();
        recreate_texture = virtual_render_tex->Size != size;
    }

    if (recreate_texture) {
        window->_virtualRenderTex = _ctx->HeadlessRenderer.CreateTexture(size, true, true);
    }
}

auto Application::IsMainWindowActuallyFullscreen() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return MainWindow._windowHandle && MainWindow.ResolveWindowStub()->Fullscreen;
}

auto Application::IsMainWindowDisplayModeSize(isize32 size) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(size);
    return false;
}

auto Application::GetMainWindowBackbufferSize() const -> isize32
{
    FO_STACK_TRACE_ENTRY();

    return {Settings.ScreenWidth, Settings.ScreenHeight};
}

void Application::SyncMainWindowBackbufferSize()
{
    FO_STACK_TRACE_ENTRY();
}

auto Application::MakeAspectFitRect(isize32 source_size, isize32 target_size) const -> irect32
{
    FO_STACK_TRACE_ENTRY();

    if (source_size.width <= 0 || source_size.height <= 0 || target_size.width <= 0 || target_size.height <= 0) {
        return {};
    }

    const float32_t source_aspect = checked_div<float32_t>(numeric_cast<float32_t>(source_size.width), numeric_cast<float32_t>(source_size.height));
    const float32_t target_aspect = checked_div<float32_t>(numeric_cast<float32_t>(target_size.width), numeric_cast<float32_t>(target_size.height));
    const int32_t width = iround<int32_t>(source_aspect <= target_aspect ? numeric_cast<float32_t>(target_size.height) * source_aspect : numeric_cast<float32_t>(target_size.width));
    const int32_t height = iround<int32_t>(source_aspect <= target_aspect ? numeric_cast<float32_t>(target_size.height) : numeric_cast<float32_t>(target_size.width) / source_aspect);

    return {(target_size.width - width) / 2, (target_size.height - height) / 2, width, height};
}

void Application::BeginWindowRender(ptr<AppWindow> window)
{
    FO_STACK_TRACE_ENTRY();

    if (!window->_isVirtual) {
        _currentRenderingWindow = window;
        return;
    }

    EnsureVirtualRenderTexture(window, window->_virtualLayoutSize);

    _previousRenderTarget = _ctx->HeadlessRenderTarget;
    Render.SetRenderTarget(window->GetRenderTexture());
    _currentRenderingWindow = window;

    const isize32 screen_size = window->GetScreenSize();

    if (screen_size.width > 0 && screen_size.height > 0) {
        _hostScreenWidthSaved = Settings.ScreenWidth;
        _hostScreenHeightSaved = Settings.ScreenHeight;
        _hostScreenSizeSaved = true;
        Settings.ScreenWidth = screen_size.width;
        Settings.ScreenHeight = screen_size.height;
    }
}

void Application::EndWindowRender()
{
    FO_STACK_TRACE_ENTRY();

    if (!_currentRenderingWindow) {
        return;
    }

    const bool was_virtual = _currentRenderingWindow->_isVirtual;
    nptr<RenderTexture> prev = _previousRenderTarget;

    _previousRenderTarget = nullptr;
    _currentRenderingWindow = nullptr;

    if (was_virtual) {
        if (_hostScreenSizeSaved) {
            Settings.ScreenWidth = _hostScreenWidthSaved;
            Settings.ScreenHeight = _hostScreenHeightSaved;
            _hostScreenSizeSaved = false;
        }

        Render.SetRenderTarget(prev);
    }
}

auto Application::TranslateHostPosToActiveWindow(ipos32 pos) const -> ipos32
{
    FO_STACK_TRACE_ENTRY();

    return pos;
}

auto Application::TranslateActiveWindowPosToHost(ipos32 pos) const -> ipos32
{
    FO_STACK_TRACE_ENTRY();

    return pos;
}

auto Application::ScaleHostDeltaToActiveWindow(ipos32 delta) const -> ipos32
{
    FO_STACK_TRACE_ENTRY();

    return delta;
}

auto Application::CreateInternalWindow(isize32 size) -> ptr<WindowInternalHandle>
{
    FO_STACK_TRACE_ENTRY();

    unique_ptr<HeadlessWindowStub> handle = SafeAlloc::MakeUnique<HeadlessWindowStub>();
    handle->Size = size;

    auto headless_window = handle.as_ptr();
    _ctx->HeadlessWindowStubs.emplace_back(std::move(handle));

    return cast_to_void(headless_window.get());
}

auto Application::ResolveTouchPos(float32_t normalized_x, float32_t normalized_y) const -> ipos32
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(normalized_x);
    ignore_unused(normalized_y);

    return {0, 0};
}

auto Application::GetTouchElapsedMs(uint64_t start_time, uint64_t end_time) const -> uint32_t
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(start_time);
    ignore_unused(end_time);

    return 0;
}

auto Application::GetTouchDistance(ipos32 from, ipos32 to) const -> float32_t
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(from);
    ignore_unused(to);

    return 0.0f;
}

auto Application::FindTouchPoint(int64_t finger_id) -> nptr<TouchPointState>
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(finger_id);

    return nullptr;
}

auto Application::FindOtherTouchPoint(int64_t finger_id) -> nptr<TouchPointState>
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(finger_id);

    return nullptr;
}

auto Application::AcquireTouchPoint(int64_t finger_id) -> nptr<TouchPointState>
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(finger_id);

    return nullptr;
}

void Application::ReleaseTouchPoint(int64_t finger_id)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(finger_id);
}

void Application::ResetTouchGestures()
{
    FO_STACK_TRACE_ENTRY();
}

void Application::QueueTouchTap(ipos32 pos)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(pos);
}

void Application::QueueTouchDoubleTap(ipos32 pos)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(pos);
}

void Application::QueueTouchScroll(ipos32 pos, ipos32 delta)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(pos);
    ignore_unused(delta);
}

void Application::QueueTouchZoom(ipos32 pos, float32_t factor)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(pos);
    ignore_unused(factor);
}

void Application::FlushPendingTouchTap()
{
    FO_STACK_TRACE_ENTRY();
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

    if (_currentRenderingWindow) {
        EndWindowRender();
    }

    _onFrameEndDispatcher();

#if FO_TRACY
    FrameMark;
#endif
}

auto Application::IsHeadless() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return true;
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

    unique_lock locker {_quitLocker};

    while (!_quit) {
        _quitEvent.wait(locker);
    }
}

auto AppWindow::GetSize() const -> isize32
{
    FO_STACK_TRACE_ENTRY();

    if (_isVirtual) {
        return _virtualSize.width > 0 && _virtualSize.height > 0 ? _virtualSize : isize32 {_app->Settings.ScreenWidth, _app->Settings.ScreenHeight};
    }

    return ResolveWindowStub()->Size;
}

void AppWindow::SetSize(isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    if (_isVirtual) {
        _virtualSize = size;
        _onWindowSizeChangedDispatcher();
        return;
    }

    ResolveWindowStub()->Size = size;
    _onWindowSizeChangedDispatcher();
}

auto AppWindow::GetScreenSize() const -> isize32
{
    FO_STACK_TRACE_ENTRY();

    if (_isVirtual) {
        return _virtualScreenSize.width > 0 && _virtualScreenSize.height > 0 ? _virtualScreenSize : GetSize();
    }

    return {GetApp()->Settings.ScreenWidth, GetApp()->Settings.ScreenHeight};
}

void AppWindow::SetScreenSize(isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    if (_isVirtual) {
        if (size != _virtualScreenSize) {
            _virtualScreenSize = size;
            _onScreenSizeChangedDispatcher();
        }
    }
    else {
        if (size.width != GetApp()->Settings.ScreenWidth || size.height != GetApp()->Settings.ScreenHeight) {
            GetApp()->Settings.ScreenWidth = size.width;
            GetApp()->Settings.ScreenHeight = size.height;
            _onScreenSizeChangedDispatcher();
        }
    }
}

auto AppWindow::GetPosition() const -> ipos32
{
    FO_STACK_TRACE_ENTRY();

    if (_isVirtual) {
        return _virtualPosition;
    }

    return ResolveWindowStub()->Position;
}

void AppWindow::SetPosition(ipos32 pos)
{
    FO_STACK_TRACE_ENTRY();

    if (_isVirtual) {
        _virtualPosition = pos;
        return;
    }

    ResolveWindowStub()->Position = pos;
}

auto AppWindow::IsFocused() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (_isVirtual) {
        return _app->_activeWindow == this;
    }

    return true;
}

void AppWindow::Minimize()
{
    FO_STACK_TRACE_ENTRY();

    if (_isVirtual) {
        return;
    }

    ResolveWindowStub()->Minimized = true;
}

auto AppWindow::IsFullscreen() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (_isVirtual) {
        return false;
    }

    return ResolveWindowStub()->Fullscreen;
}

auto AppWindow::ToggleFullscreen(bool enable) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (_isVirtual) {
        ignore_unused(enable);
        return false;
    }

    auto window = ResolveWindowStub();
    const bool changed = window->Fullscreen != enable;
    window->Fullscreen = enable;
    _app->Settings.Fullscreen = enable;
    _app->_mainWindowFullscreenBackbufferMode = enable;

    return changed;
}

void AppWindow::Blink()
{
    FO_STACK_TRACE_ENTRY();
}

void AppWindow::AlwaysOnTop(bool enable)
{
    FO_STACK_TRACE_ENTRY();

    if (_isVirtual) {
        return;
    }

    ResolveWindowStub()->AlwaysOnTop = enable;
}

void AppWindow::SetTitle(string_view title)
{
    FO_STACK_TRACE_ENTRY();

    _title = string {title};
}

void AppWindow::GrabInput(bool enable)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(enable);
}

void AppWindow::Destroy()
{
    FO_STACK_TRACE_ENTRY();

    if (_isVirtual) {
        ptr<AppWindow> window = this;
        _app->DestroyChildWindow(window);
        return;
    }

    ptr<AppWindow> self = this;
    ptr<AppWindow> main_window = &GetApp()->MainWindow;

    if (_windowHandle && !(self == main_window)) {
        auto window_handle = _windowHandle.as_ptr();
        ptr<const HeadlessWindowStub> window_stub = cast_from_void<HeadlessWindowStub*>(window_handle.get());
        std::erase_if(_app->_ctx->HeadlessWindowStubs, [window_stub](const auto& entry) { return entry.as_ptr() == window_stub; });
        _windowHandle = nullptr;
    }
}

auto AppWindow::ResolveWindowHandle() const -> ptr<WindowInternalHandle>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_windowHandle, "Missing native window handle");

    ptr<WindowInternalHandle> window_handle {_windowHandle.get_no_const()};
    return window_handle;
}

auto AppWindow::ResolveWindowStub() const -> ptr<HeadlessWindowStub>
{
    FO_STACK_TRACE_ENTRY();

    return cast_from_void<HeadlessWindowStub*>(ResolveWindowHandle().get());
}

auto AppRender::CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture>
{
    FO_STACK_TRACE_ENTRY();

    return _app->_ctx->HeadlessRenderer.CreateTexture(size, linear_filtered, with_depth);
}

void AppRender::SetRenderTarget(nptr<RenderTexture> tex)
{
    FO_STACK_TRACE_ENTRY();

    _app->_ctx->HeadlessRenderTarget = tex;
    _app->_ctx->HeadlessRenderer.SetRenderTarget(tex);
}

auto AppRender::GetRenderTarget() -> nptr<RenderTexture>
{
    FO_STACK_TRACE_ENTRY();

    return _app->_ctx->HeadlessRenderTarget;
}

void AppRender::ClearRenderTarget(optional<ucolor> color, bool depth, bool stencil)
{
    FO_STACK_TRACE_ENTRY();

    _app->_ctx->HeadlessRenderer.ClearRenderTarget(color, depth, stencil);
}

void AppRender::EnableScissor(irect32 rect)
{
    FO_STACK_TRACE_ENTRY();

    _app->_ctx->HeadlessRenderer.EnableScissor(rect);
}

void AppRender::DisableScissor()
{
    FO_STACK_TRACE_ENTRY();

    _app->_ctx->HeadlessRenderer.DisableScissor();
}

auto AppRender::CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer>
{
    FO_STACK_TRACE_ENTRY();

    return _app->_ctx->HeadlessRenderer.CreateDrawBuffer(is_static);
}

auto AppRender::CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect>
{
    FO_STACK_TRACE_ENTRY();

    return _app->_ctx->HeadlessRenderer.CreateEffect(usage, name, loader);
}

auto AppRender::CreateOrthoMatrix(float32_t left, float32_t right, float32_t bottom, float32_t top, float32_t nearp, float32_t farp) const -> mat44
{
    FO_STACK_TRACE_ENTRY();

    return _app->_ctx->HeadlessRenderer.CreateOrthoMatrix(left, right, bottom, top, nearp, farp);
}

auto AppRender::IsRenderTargetFlipped() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return _app->_ctx->HeadlessRenderer.IsRenderTargetFlipped();
}

auto AppRender::GetProjMatrix() const -> mat44
{
    FO_STACK_TRACE_ENTRY();

    return _app->_ctx->HeadlessRenderer.GetProjMatrix();
}

void AppRender::SetOrthoDepthRange(float32_t nearp, float32_t farp) noexcept
{
    FO_STACK_TRACE_ENTRY();

    _app->_ctx->HeadlessRenderer.SetOrthoDepthRange(nearp, farp);
}

auto AppInput::IsMouseAvailable() const noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    return false;
}

auto AppInput::GetMousePosition() const -> ipos32
{
    FO_STACK_TRACE_ENTRY();

    return {0, 0};
}

auto AppInput::GetGamepadState() const noexcept -> GamepadState
{
    FO_STACK_TRACE_ENTRY();

    return {};
}

void AppInput::SetMousePosition(ipos32 pos, nptr<const IAppWindow> relative_to)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(pos);
    ignore_unused(relative_to);
}

auto AppInput::PollEvent(InputEvent& ev) -> bool
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(ev);

    return false;
}

void AppInput::ClearEvents()
{
    FO_STACK_TRACE_ENTRY();
}

void AppInput::PushEvent(const InputEvent& ev, bool push_to_this_frame)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(ev);
    ignore_unused(push_to_this_frame);
}

void AppInput::SetScreenKeyboardEnabled(bool enabled)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(enabled);
}

void AppInput::SetClipboardText(string_view text)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(text);
}

auto AppInput::GetClipboardText() -> string_view
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

    [[maybe_unused]] auto unused = std::move(stream_callback);

    FO_VERIFY_AND_THROW(IsEnabled(), "Application subsystem is not enabled");
}

auto AppAudio::ConvertAudio(int32_t format, int32_t channels, int32_t rate, vector<uint8_t>& buf) -> bool
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(format);
    ignore_unused(channels);
    ignore_unused(rate);
    ignore_unused(buf);

    FO_VERIFY_AND_THROW(IsEnabled(), "Application subsystem is not enabled");

    return true;
}

void AppAudio::MixAudio(span<uint8_t> output, const_span<uint8_t> buf, int32_t volume)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(output);
    ignore_unused(buf);
    ignore_unused(volume);

    FO_VERIFY_AND_THROW(IsEnabled(), "Application subsystem is not enabled");
}

void AppAudio::LockDevice()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(IsEnabled(), "Application subsystem is not enabled");
}

void AppAudio::UnlockDevice()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(IsEnabled(), "Application subsystem is not enabled");
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

void Application::ChooseOptionsWindow(string_view title, const vector<string>& options, set<int32_t>& selected)
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

        for (const int32_t sel : selected) {
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

        for (const int32_t sel : in_selected) {
            if (sel >= 1 && sel < numeric_cast<int32_t>(options.size() + 1)) {
                selected.emplace(sel - 1);
            }
        }
    }
}

FO_END_NAMESPACE
