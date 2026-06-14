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
    unique_ptr<Null_Renderer> HeadlessRenderer {};
    raw_ptr<RenderTexture> HeadlessRenderTarget {};
    vector<unique_ptr<HeadlessWindowStub>> HeadlessWindowStubs {};
};

static constexpr int32_t MAX_ATLAS_WIDTH_ = 8192;
static constexpr int32_t MAX_ATLAS_HEIGHT_ = 8192;
static constexpr int32_t MAX_BONES_ = 32;
const int32_t& AppRender::MAX_ATLAS_WIDTH {MAX_ATLAS_WIDTH_};
const int32_t& AppRender::MAX_ATLAS_HEIGHT {MAX_ATLAS_HEIGHT_};
const int32_t& AppRender::MAX_BONES {MAX_BONES_};
const int32_t AppAudio::AUDIO_FORMAT_U8 = 0;
const int32_t AppAudio::AUDIO_FORMAT_S16 = 1;

Application::Application(GlobalSettings&& settings, AppInitFlags flags) :
    Settings {std::move(settings)},
    MainWindow {this},
    Render {this},
    Input {this},
    Audio {this}
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_ctx, "Frontend context is already initialized");
    _ctx = SafeAlloc::MakeUnique<Context>();

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

    _ctx->HeadlessRenderer = SafeAlloc::MakeUnique<Null_Renderer>();
    _ctx->HeadlessRenderer->Init(Settings, nullptr);
    MainWindow._windowHandle = CreateInternalWindow({Settings.ScreenWidth, Settings.ScreenHeight});
    MainWindow._title = Settings.GameName;
    MainWindow._virtualSize = {Settings.ScreenWidth, Settings.ScreenHeight};
    MainWindow._virtualScreenSize = {Settings.ScreenWidth, Settings.ScreenHeight};
    _allWindows.emplace_back(&MainWindow);
    _activeWindow = &MainWindow;
}

Application::~Application()
{
    FO_STACK_TRACE_ENTRY();

    if (!_ctx) {
        return;
    }

    _imguiTextures.clear();
    _imguiEffect = nullptr;
    _imguiDrawBuf = nullptr;
    _ctx->HeadlessRenderTarget = nullptr;

    _ctx->HeadlessRenderer = nullptr;

    for (auto& window : _childWindows) {
        window->_virtualRenderTex.reset();
    }

    _childWindows.clear();
    _allWindows.clear();
    _activeWindow = &MainWindow;
    _currentRenderingWindow = nullptr;
    _previousRenderTarget = nullptr;

    MainWindow._windowHandle = nullptr;
    _ctx->HeadlessWindowStubs.clear();
    _ctx = nullptr;
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

auto Application::CreateChildWindow(isize32 size, string_view title) -> AppWindow*
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (size.width <= 0 || size.height <= 0) {
        size = {Settings.ScreenWidth, Settings.ScreenHeight};
    }

    auto window = unique_ptr<AppWindow> {new AppWindow {this}};
    window->_isVirtual = true;
    window->_virtualSize = size;
    window->_virtualScreenSize = size;
    window->_virtualLayoutSize = size;
    window->_title = title.empty() ? strex("Window {}", _childWindows.size() + 1).str() : string {title};

    auto* ptr = window.get();
    _allWindows.emplace_back(ptr);
    _childWindows.emplace_back(std::move(window));
    _activeWindow = ptr;

    return ptr;
}

void Application::DestroyChildWindow(AppWindow* window)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (window == nullptr || window == &MainWindow) {
        return;
    }

    std::erase_if(_allWindows, [&](const auto& entry) { return entry.get_no_const() == window; });

    const auto it = std::ranges::find_if(_childWindows, [&](const auto& entry) { return entry.get() == window; });

    if (it == _childWindows.end()) {
        return;
    }

    if (_activeWindow.get_no_const() == window) {
        _activeWindow = &MainWindow;
    }

    if (_currentRenderingWindow.get_no_const() == window) {
        _currentRenderingWindow = nullptr;
    }

    (*it)->_virtualRenderTex.reset();
    _childWindows.erase(it);
}

void Application::SetActiveWindow(AppWindow* window)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    _activeWindow = window != nullptr ? window : &MainWindow;
}

void Application::EnsureVirtualRenderTexture(AppWindow* window, isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    FO_VERIFY_AND_THROW(window, "Missing application window");
    FO_VERIFY_AND_THROW(window->_isVirtual, "Window is not virtual");

    if (size.width <= 0 || size.height <= 0) {
        size = window->_virtualSize;
    }

    window->_virtualLayoutSize = size;

    if (!window->_virtualRenderTex || window->_virtualRenderTex->Size != size) {
        window->_virtualRenderTex = _ctx->HeadlessRenderer->CreateTexture(size, true, true);
    }
}

auto Application::IsMainWindowActuallyFullscreen() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return MainWindow._windowHandle != nullptr && MainWindow.ResolveWindowStub()->Fullscreen;
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

    FO_NON_CONST_METHOD_HINT();
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

void Application::BeginWindowRender(AppWindow* window)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    FO_VERIFY_AND_THROW(window, "Missing application window");

    if (!window->_isVirtual) {
        _currentRenderingWindow = window;
        return;
    }

    EnsureVirtualRenderTexture(window, window->_virtualLayoutSize);

    _previousRenderTarget = _ctx->HeadlessRenderTarget;
    Render.SetRenderTarget(window->_virtualRenderTex.get());
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

    FO_NON_CONST_METHOD_HINT();

    if (_currentRenderingWindow == nullptr) {
        return;
    }

    const bool was_virtual = _currentRenderingWindow->_isVirtual;
    auto* prev = _previousRenderTarget.get_no_const();

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

auto Application::CreateInternalWindow(isize32 size) -> WindowInternalHandle*
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    auto handle = SafeAlloc::MakeUnique<HeadlessWindowStub>();
    handle->Size = size;

    auto* ptr = handle.get();
    _ctx->HeadlessWindowStubs.emplace_back(std::move(handle));

    return reinterpret_cast<WindowInternalHandle*>(ptr);
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

auto Application::FindTouchPoint(int64_t finger_id) -> TouchPointState*
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(finger_id);

    return nullptr;
}

auto Application::FindOtherTouchPoint(int64_t finger_id) -> TouchPointState*
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(finger_id);

    return nullptr;
}

auto Application::AcquireTouchPoint(int64_t finger_id) -> TouchPointState*
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(finger_id);

    return nullptr;
}

void Application::ReleaseTouchPoint(int64_t finger_id)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(finger_id);
}

void Application::ResetTouchGestures()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();
}

void Application::QueueTouchTap(ipos32 pos)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(pos);
}

void Application::QueueTouchDoubleTap(ipos32 pos)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(pos);
}

void Application::QueueTouchScroll(ipos32 pos, ipos32 delta)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(pos);
    ignore_unused(delta);
}

void Application::QueueTouchZoom(ipos32 pos, float32_t factor)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(pos);
    ignore_unused(factor);
}

void Application::FlushPendingTouchTap()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();
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

    if (_currentRenderingWindow != nullptr) {
        EndWindowRender();
    }

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

    FO_NON_CONST_METHOD_HINT();

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

    return {App->Settings.ScreenWidth, App->Settings.ScreenHeight};
}

void AppWindow::SetScreenSize(isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    if (_isVirtual) {
        if (size != _virtualScreenSize) {
            _virtualScreenSize = size;
            _onScreenSizeChangedDispatcher();
        }
    }
    else {
        if (size.width != App->Settings.ScreenWidth || size.height != App->Settings.ScreenHeight) {
            App->Settings.ScreenWidth = size.width;
            App->Settings.ScreenHeight = size.height;
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

    FO_NON_CONST_METHOD_HINT();

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
        return _app->_activeWindow.get() == this;
    }

    return true;
}

void AppWindow::Minimize()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

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

    FO_NON_CONST_METHOD_HINT();

    if (_isVirtual) {
        ignore_unused(enable);
        return false;
    }

    auto* window = ResolveWindowStub();
    const bool changed = window->Fullscreen != enable;
    window->Fullscreen = enable;
    _app->Settings.Fullscreen = enable;
    _app->_mainWindowFullscreenBackbufferMode = enable;

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

    if (_isVirtual) {
        return;
    }

    ResolveWindowStub()->AlwaysOnTop = enable;
}

void AppWindow::SetTitle(string_view title)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    _title = string {title};
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

    if (_isVirtual) {
        _app->DestroyChildWindow(this);
        return;
    }

    if (_windowHandle && this != &App->MainWindow) {
        std::erase_if(_app->_ctx->HeadlessWindowStubs, [handle = _windowHandle.get()](const auto& entry) { return entry.get() == static_cast<HeadlessWindowStub*>(handle); });
        _windowHandle = nullptr;
    }
}

auto AppWindow::ResolveWindowHandle() const -> WindowInternalHandle*
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_windowHandle, "Missing native window handle");

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

    return _app->_ctx->HeadlessRenderer->CreateTexture(size, linear_filtered, with_depth);
}

void AppRender::SetRenderTarget(RenderTexture* tex)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    _app->_ctx->HeadlessRenderTarget = tex;
    _app->_ctx->HeadlessRenderer->SetRenderTarget(tex);
}

auto AppRender::GetRenderTarget() -> RenderTexture*
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    return _app->_ctx->HeadlessRenderTarget.get();
}

void AppRender::ClearRenderTarget(optional<ucolor> color, bool depth, bool stencil)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    _app->_ctx->HeadlessRenderer->ClearRenderTarget(color, depth, stencil);
}

void AppRender::EnableScissor(irect32 rect)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    _app->_ctx->HeadlessRenderer->EnableScissor(rect);
}

void AppRender::DisableScissor()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    _app->_ctx->HeadlessRenderer->DisableScissor();
}

auto AppRender::CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer>
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    return _app->_ctx->HeadlessRenderer->CreateDrawBuffer(is_static);
}

auto AppRender::CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& file_loader) -> unique_ptr<RenderEffect>
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    return _app->_ctx->HeadlessRenderer->CreateEffect(usage, name, file_loader);
}

auto AppRender::CreateOrthoMatrix(float32_t left, float32_t right, float32_t bottom, float32_t top, float32_t nearp, float32_t farp) const -> mat44
{
    FO_STACK_TRACE_ENTRY();

    return _app->_ctx->HeadlessRenderer->CreateOrthoMatrix(left, right, bottom, top, nearp, farp);
}

auto AppRender::IsRenderTargetFlipped() const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return _app->_ctx->HeadlessRenderer->IsRenderTargetFlipped();
}

auto AppRender::GetProjMatrix() const -> mat44
{
    FO_STACK_TRACE_ENTRY();

    return _app->_ctx->HeadlessRenderer->GetProjMatrix();
}

void AppRender::SetOrthoDepthRange(float32_t nearp, float32_t farp) noexcept
{
    FO_STACK_TRACE_ENTRY();

    _app->_ctx->HeadlessRenderer->SetOrthoDepthRange(nearp, farp);
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

void AppInput::SetMousePosition(ipos32 pos, const IAppWindow* relative_to)
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

void AppInput::SetScreenKeyboardEnabled(bool enabled)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(enabled);
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

    FO_VERIFY_AND_THROW(IsEnabled(), "Application subsystem is not enabled");
}

auto AppAudio::ConvertAudio(int32_t format, int32_t channels, int32_t rate, vector<uint8_t>& buf) -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(format);
    ignore_unused(channels);
    ignore_unused(rate);
    ignore_unused(buf);

    FO_VERIFY_AND_THROW(IsEnabled(), "Application subsystem is not enabled");

    return true;
}

void AppAudio::MixAudio(uint8_t* output, const uint8_t* buf, size_t len, int32_t volume)
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    ignore_unused(output);
    ignore_unused(buf);
    ignore_unused(len);
    ignore_unused(volume);

    FO_VERIFY_AND_THROW(IsEnabled(), "Application subsystem is not enabled");
}

void AppAudio::LockDevice()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

    FO_VERIFY_AND_THROW(IsEnabled(), "Application subsystem is not enabled");
}

void AppAudio::UnlockDevice()
{
    FO_STACK_TRACE_ENTRY();

    FO_NON_CONST_METHOD_HINT();

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
