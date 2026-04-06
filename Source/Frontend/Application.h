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

#pragma once

#include "Common.h"

#include "FileSystem.h"
#include "Rendering.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

class Application;
class IAppWindow;

FO_DECLARE_EXCEPTION(AppInitException);

///@ ExportEnum
enum class KeyCode : uint8
{
    None = 0x00,
    Escape = 0x01,
    C1 = 0x02,
    C2 = 0x03,
    C3 = 0x04,
    C4 = 0x05,
    C5 = 0x06,
    C6 = 0x07,
    C7 = 0x08,
    C8 = 0x09,
    C9 = 0x0A,
    C0 = 0x0B,
    Minus = 0x0C,
    Equals = 0x0D,
    Back = 0x0E,
    Tab = 0x0F,
    Q = 0x10,
    W = 0x11,
    E = 0x12,
    R = 0x13,
    T = 0x14,
    Y = 0x15,
    U = 0x16,
    I = 0x17,
    O = 0x18,
    P = 0x19,
    Lbracket = 0x1A,
    Rbracket = 0x1B,
    Return = 0x1C,
    Lcontrol = 0x1D,
    A = 0x1E,
    S = 0x1F,
    D = 0x20,
    F = 0x21,
    G = 0x22,
    H = 0x23,
    J = 0x24,
    K = 0x25,
    L = 0x26,
    Semicolon = 0x27,
    Apostrophe = 0x28,
    Grave = 0x29,
    Lshift = 0x2A,
    Backslash = 0x2B,
    Z = 0x2C,
    X = 0x2D,
    C = 0x2E,
    V = 0x2F,
    B = 0x30,
    N = 0x31,
    M = 0x32,
    Comma = 0x33,
    Period = 0x34,
    Slash = 0x35,
    Rshift = 0x36,
    Multiply = 0x37,
    Lmenu = 0x38,
    Space = 0x39,
    Capital = 0x3A,
    F1 = 0x3B,
    F2 = 0x3C,
    F3 = 0x3D,
    F4 = 0x3E,
    F5 = 0x3F,
    F6 = 0x40,
    F7 = 0x41,
    F8 = 0x42,
    F9 = 0x43,
    F10 = 0x44,
    Numlock = 0x45,
    Scroll = 0x46,
    Numpad7 = 0x47,
    Numpad8 = 0x48,
    Numpad9 = 0x49,
    Subtract = 0x4A,
    Numpad4 = 0x4B,
    Numpad5 = 0x4C,
    Numpad6 = 0x4D,
    Add = 0x4E,
    Numpad1 = 0x4F,
    Numpad2 = 0x50,
    Numpad3 = 0x51,
    Numpad0 = 0x52,
    Decimal = 0x53,
    F11 = 0x57,
    F12 = 0x58,
    Numpadenter = 0x9C,
    Rcontrol = 0x9D,
    Divide = 0xB5,
    Sysrq = 0xB7,
    Rmenu = 0xB8,
    Pause = 0xC5,
    Home = 0xC7,
    Up = 0xC8,
    Prior = 0xC9,
    Left = 0xCB,
    Right = 0xCD,
    End = 0xCF,
    Down = 0xD0,
    Next = 0xD1,
    Insert = 0xD2,
    Delete = 0xD3,
    Lwin = 0xDB,
    Rwin = 0xDC,
    Text = 0xFF,
};

///@ ExportEnum
enum class MouseButton : uint8
{
    Left = 0,
    Right = 1,
    Middle = 2,
    WheelUp = 3,
    WheelDown = 4,
    Ext0 = 5,
    Ext1 = 6,
    Ext2 = 7,
    Ext3 = 8,
    Ext4 = 9,
};

struct InputEvent
{
    enum class EventType : uint8
    {
        NoneEvent,
        MouseMoveEvent,
        MouseDownEvent,
        MouseUpEvent,
        MouseWheelEvent,
        TouchTapEvent,
        TouchDoubleTapEvent,
        TouchScrollEvent,
        TouchZoomEvent,
        KeyDownEvent,
        KeyUpEvent,
    } Type {};

    struct MouseMoveEvent
    {
        int32 MouseX {};
        int32 MouseY {};
        int32 DeltaX {};
        int32 DeltaY {};
    } MouseMove {};

    struct MouseDownEvent
    {
        MouseButton Button {};
    } MouseDown {};

    struct MouseUpEvent
    {
        MouseButton Button {};
    } MouseUp {};

    struct MouseWheelEvent
    {
        int32 Delta {};
    } MouseWheel {};

    struct TouchTapEvent
    {
        int32 TouchX {};
        int32 TouchY {};
    } TouchTap {};

    struct TouchDoubleTapEvent
    {
        int32 TouchX {};
        int32 TouchY {};
    } TouchDoubleTap {};

    struct TouchScrollEvent
    {
        int32 TouchX {};
        int32 TouchY {};
        int32 DeltaX {};
        int32 DeltaY {};
    } TouchScroll {};

    struct TouchZoomEvent
    {
        int32 TouchX {};
        int32 TouchY {};
        float32 Factor {};
    } TouchZoom {};

    struct KeyDownEvent
    {
        KeyCode Code {};
        string Text {};
    } KeyDown {};

    struct KeyUpEvent
    {
        KeyCode Code {};
    } KeyUp {};

    InputEvent() = default;
    explicit InputEvent(MouseMoveEvent ev) :
        Type {EventType::MouseMoveEvent},
        MouseMove {ev}
    {
    }
    explicit InputEvent(MouseDownEvent ev) :
        Type {EventType::MouseDownEvent},
        MouseDown {ev}
    {
    }
    explicit InputEvent(MouseUpEvent ev) :
        Type {EventType::MouseUpEvent},
        MouseUp {ev}
    {
    }
    explicit InputEvent(MouseWheelEvent ev) :
        Type {EventType::MouseWheelEvent},
        MouseWheel {ev}
    {
    }
    explicit InputEvent(TouchTapEvent ev) :
        Type {EventType::TouchTapEvent},
        TouchTap {ev}
    {
    }
    explicit InputEvent(TouchDoubleTapEvent ev) :
        Type {EventType::TouchDoubleTapEvent},
        TouchDoubleTap {ev}
    {
    }
    explicit InputEvent(TouchScrollEvent ev) :
        Type {EventType::TouchScrollEvent},
        TouchScroll {ev}
    {
    }
    explicit InputEvent(TouchZoomEvent ev) :
        Type {EventType::TouchZoomEvent},
        TouchZoom {ev}
    {
    }
    explicit InputEvent(KeyDownEvent ev) :
        Type {EventType::KeyDownEvent},
        KeyDown {std::move(ev)}
    {
    }
    explicit InputEvent(KeyUpEvent ev) :
        Type {EventType::KeyUpEvent},
        KeyUp {ev}
    {
    }
};

class IAppRender
{
public:
    virtual ~IAppRender() = default;

    [[nodiscard]] virtual auto GetRenderTarget() -> RenderTexture* = 0;
    [[nodiscard]] virtual auto CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture> = 0;
    [[nodiscard]] virtual auto CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer> = 0;
    [[nodiscard]] virtual auto CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect> = 0;
    [[nodiscard]] virtual auto CreateOrthoMatrix(float32 left, float32 right, float32 bottom, float32 top, float32 nearp, float32 farp) -> mat44 = 0;
    [[nodiscard]] virtual auto IsRenderTargetFlipped() -> bool = 0;

    virtual void SetRenderTarget(RenderTexture* tex) = 0;
    virtual void ClearRenderTarget(optional<ucolor> color, bool depth = false, bool stencil = false) = 0;
    virtual void EnableScissor(irect32 rect) = 0;
    virtual void DisableScissor() = 0;
};

class IAppInput
{
public:
    virtual ~IAppInput() = default;

    [[nodiscard]] virtual auto IsMouseAvailable() const noexcept -> bool = 0;
    [[nodiscard]] virtual auto GetMousePosition() const -> ipos32 = 0;
    [[nodiscard]] virtual auto GetClipboardText() -> const string& = 0;
    [[nodiscard]] virtual auto IsShiftDown() const noexcept -> bool = 0;
    [[nodiscard]] virtual auto IsCtrlDown() const noexcept -> bool = 0;
    [[nodiscard]] virtual auto IsAltDown() const noexcept -> bool = 0;

    virtual auto PollEvent(InputEvent& ev) -> bool = 0;
    virtual void ClearEvents() = 0;
    virtual void SetMousePosition(ipos32 pos, const IAppWindow* relative_to = nullptr) = 0;
    virtual void PushEvent(const InputEvent& ev, bool push_to_this_frame = false) = 0;
    virtual void SetScreenKeyboardEnabled(bool enabled) = 0;
    virtual void SetClipboardText(string_view text) = 0;
};

class IAppAudio
{
public:
    using AudioStreamCallback = function<void(uint8, span<uint8>)>;

    virtual ~IAppAudio() = default;

    [[nodiscard]] virtual auto IsEnabled() const -> bool = 0;

    virtual auto ConvertAudio(int32 format, int32 channels, int32 rate, vector<uint8>& buf) -> bool = 0;
    virtual void SetSource(AudioStreamCallback stream_callback) = 0;
    virtual void MixAudio(uint8* output, const uint8* buf, size_t len, int32 volume) = 0;
    virtual void LockDevice() = 0;
    virtual void UnlockDevice() = 0;
};

class IAppWindow
{
public:
    virtual ~IAppWindow() = default;

    [[nodiscard]] virtual auto GetSize() const -> isize32 = 0;
    [[nodiscard]] virtual auto GetScreenSize() const -> isize32 = 0;
    [[nodiscard]] virtual auto GetPosition() const -> ipos32 = 0;
    [[nodiscard]] virtual auto IsFocused() const -> bool = 0;
    [[nodiscard]] virtual auto IsFullscreen() const -> bool = 0;
    [[nodiscard]] virtual auto GetRender() noexcept -> IAppRender& = 0;
    [[nodiscard]] virtual auto GetInput() noexcept -> IAppInput& = 0;
    [[nodiscard]] virtual auto GetAudio() noexcept -> IAppAudio& = 0;
    [[nodiscard]] virtual auto GetOnWindowSizeChanged() noexcept -> EventObserver<>& = 0;
    [[nodiscard]] virtual auto GetOnScreenSizeChanged() noexcept -> EventObserver<>& = 0;
    [[nodiscard]] virtual auto GetOnLowMemory() noexcept -> EventObserver<>& = 0;
    [[nodiscard]] virtual auto GetWindowHandleForInput() const -> WindowInternalHandle* = 0;

    virtual void GrabInput(bool enable) = 0;
    virtual void SetSize(isize32 size) = 0;
    virtual void SetScreenSize(isize32 size) = 0;
    virtual void SetPosition(ipos32 pos) = 0;
    virtual void Minimize() = 0;
    virtual auto ToggleFullscreen(bool enable) -> bool = 0;
    virtual void Blink() = 0;
    virtual void AlwaysOnTop(bool enable) = 0;
    virtual void Destroy() = 0;
};

struct HeadlessWindowStub
{
    isize32 Size {1000, 1000};
    ipos32 Position {};
    bool Fullscreen {};
    bool AlwaysOnTop {};
    bool Minimized {};
};

class AppWindow final : public IAppWindow
{
    friend class Application;
    friend class AppInput;

public:
    [[nodiscard]] auto GetSize() const -> isize32 override;
    [[nodiscard]] auto GetScreenSize() const -> isize32 override;
    [[nodiscard]] auto GetPosition() const -> ipos32 override;
    [[nodiscard]] auto IsFocused() const -> bool override;
    [[nodiscard]] auto IsFullscreen() const -> bool override;
    [[nodiscard]] auto GetRender() noexcept -> IAppRender& override;
    [[nodiscard]] auto GetInput() noexcept -> IAppInput& override;
    [[nodiscard]] auto GetAudio() noexcept -> IAppAudio& override;
    [[nodiscard]] auto GetOnWindowSizeChanged() noexcept -> EventObserver<>& override { return OnWindowSizeChanged; }
    [[nodiscard]] auto GetOnScreenSizeChanged() noexcept -> EventObserver<>& override { return OnScreenSizeChanged; }
    [[nodiscard]] auto GetOnLowMemory() noexcept -> EventObserver<>& override;
    [[nodiscard]] auto GetWindowHandleForInput() const -> WindowInternalHandle* override;

    void GrabInput(bool enable) override;
    void SetSize(isize32 size) override;
    void SetScreenSize(isize32 size) override;
    void SetPosition(ipos32 pos) override;
    void Minimize() override;
    auto ToggleFullscreen(bool enable) -> bool override;
    void Blink() override;
    void AlwaysOnTop(bool enable) override;
    void Destroy() override;

    EventObserver<> OnWindowSizeChanged {};
    EventObserver<> OnScreenSizeChanged {};

private:
    explicit AppWindow(Application* app) :
        _app {app}
    {
    }

    [[nodiscard]] auto ResolveWindowHandle() const -> WindowInternalHandle*;
    [[nodiscard]] auto ResolveWindowStub() const -> HeadlessWindowStub*;

    raw_ptr<Application> _app;
    raw_ptr<WindowInternalHandle> _windowHandle {};
    bool _grabbed {};
    EventDispatcher<> _onWindowSizeChangedDispatcher {OnWindowSizeChanged};
    EventDispatcher<> _onScreenSizeChangedDispatcher {OnScreenSizeChanged};
    int32 _nonConstHelper {};
};

class AppRender final : public IAppRender
{
    friend class Application;

public:
    static constexpr int32 MAX_ATLAS_SIZE = 8192;
    static constexpr int32 MIN_ATLAS_SIZE = 2048;
    static const int32& MAX_ATLAS_WIDTH;
    static const int32& MAX_ATLAS_HEIGHT;
    static const int32& MAX_BONES;

    [[nodiscard]] auto GetRenderTarget() -> RenderTexture* override;
    [[nodiscard]] auto CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture> override;
    [[nodiscard]] auto CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer> override;
    [[nodiscard]] auto CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect> override;
    [[nodiscard]] auto CreateOrthoMatrix(float32 left, float32 right, float32 bottom, float32 top, float32 nearp, float32 farp) -> mat44 override;
    [[nodiscard]] auto IsRenderTargetFlipped() -> bool override;

    void SetRenderTarget(RenderTexture* tex) override;
    void ClearRenderTarget(optional<ucolor> color, bool depth = false, bool stencil = false) override;
    void EnableScissor(irect32 rect) override;
    void DisableScissor() override;

private:
    explicit AppRender(Application* app) :
        _app {app}
    {
    }

    raw_ptr<Application> _app;
    int32 _nonConstHelper {};
};

class AppInput final : public IAppInput
{
    friend class Application;

public:
    static constexpr size_t DROP_FILE_STRIP_LENGHT = 2048;

    [[nodiscard]] auto IsMouseAvailable() const noexcept -> bool override;
    [[nodiscard]] auto GetMousePosition() const -> ipos32 override;
    [[nodiscard]] auto GetClipboardText() -> const string& override;
    [[nodiscard]] auto IsShiftDown() const noexcept -> bool override { return _shiftDown; }
    [[nodiscard]] auto IsCtrlDown() const noexcept -> bool override { return _ctrlDown; }
    [[nodiscard]] auto IsAltDown() const noexcept -> bool override { return _altDown; }

    auto PollEvent(InputEvent& ev) -> bool override;
    void ClearEvents() override;
    void SetMousePosition(ipos32 pos, const IAppWindow* relative_to = nullptr) override;
    void PushEvent(const InputEvent& ev, bool push_to_this_frame = false) override;
    void SetScreenKeyboardEnabled(bool enabled) override;
    void SetClipboardText(string_view text) override;

private:
    explicit AppInput(Application* app) :
        _app {app}
    {
    }

    raw_ptr<Application> _app;
    string _clipboardTextStorage {};
    int32 _nonConstHelper {};
    bool _shiftDown {};
    bool _ctrlDown {};
    bool _altDown {};
};

class AppAudio final : public IAppAudio
{
    friend class Application;

public:
    static const int32 AUDIO_FORMAT_U8;
    static const int32 AUDIO_FORMAT_S16;

    using AudioStreamCallback = IAppAudio::AudioStreamCallback;

    [[nodiscard]] auto IsEnabled() const -> bool override;

    auto ConvertAudio(int32 format, int32 channels, int32 rate, vector<uint8>& buf) -> bool override;
    void SetSource(AudioStreamCallback stream_callback) override;
    void MixAudio(uint8* output, const uint8* buf, size_t len, int32 volume) override;
    void LockDevice() override;
    void UnlockDevice() override;

private:
    explicit AppAudio(Application* app) :
        _app {app}
    {
    }

    raw_ptr<Application> _app;
    int32 _nonConstHelper {};
};

enum class AppInitFlags : uint8
{
    None = 0x00,
    ClientMode = 0x01,
    DisableLogTags = 0x02,
    ShowMessageOnException = 0x04,
    PrebakeResources = 0x08,
};

class Application final
{
    friend void InitApp(int32 argc, char** argv, AppInitFlags flags);
    friend class SafeAlloc;
    friend class AppWindow;
    friend class AppRender;
    friend class AppInput;
    friend class AppAudio;

    Application(GlobalSettings&& settings, AppInitFlags flags);

public:
    using ProgressWindowCallback = function<void()>;

    Application(const Application&) = delete;
    Application(Application&&) noexcept = delete;
    auto operator=(const Application&) = delete;
    auto operator=(Application&&) noexcept = delete;
    ~Application();

    [[nodiscard]] auto IsQuitRequested() const -> bool { return _quit; }
    [[nodiscard]] auto GetRequestedQuitSuccess() const -> bool { return _quitSuccess; }

    auto CreateChildWindow(isize32 size) -> AppWindow*;
    void OpenLink(string_view link);
    void LoadImGuiEffect(const FileSystem& resources);
#if FO_IOS
    void SetMainLoopCallback(void (*callback)(void*));
#endif
    void BeginFrame();
    void EndFrame();
    void RequestQuit(bool success = true) noexcept;
    void WaitForRequestedQuit();

    static void ShowErrorMessage(string_view message, string_view traceback, bool fatal_error);
    static void ShowProgressWindow(string_view text, const ProgressWindowCallback& callback);
    static void ChooseOptionsWindow(string_view title, const vector<string>& options, set<int32>& selected);

    GlobalSettings Settings;

    EventObserver<> OnFrameBegin {};
    EventObserver<> OnFrameEnd {};
    EventObserver<> OnPause {};
    EventObserver<> OnResume {};
    EventObserver<> OnLowMemory {};
    EventObserver<> OnQuit {};

    AppWindow MainWindow;
    AppRender Render;
    AppInput Input;
    AppAudio Audio;

private:
    struct TouchPointState
    {
        int64 FingerId {-1};
        ipos32 StartPos {};
        ipos32 LastPos {};
        uint64 StartTime {};
        bool Active {};
        bool ScrollActive {};
    };

    struct PendingTouchTapState
    {
        ipos32 Pos {};
        uint64 ReleaseTime {};
        bool Active {};
    };

    static constexpr uint32 TOUCH_TAP_MAX_TIME_MS = 250;
    static constexpr uint32 TOUCH_DOUBLE_TAP_MAX_TIME_MS = 200;
    static constexpr int32 TOUCH_TAP_MAX_DIST = 12;
    static constexpr int32 TOUCH_DOUBLE_TAP_MAX_DIST = 48;

    struct Context;

    auto CreateInternalWindow(isize32 size) -> WindowInternalHandle*;
    auto ResolveTouchPos(float32 normalized_x, float32 normalized_y) const -> ipos32;
    auto GetTouchElapsedMs(uint64 start_time, uint64 end_time) const -> uint32;
    auto GetTouchDistance(ipos32 from, ipos32 to) const -> float32;
    auto FindTouchPoint(int64 finger_id) -> TouchPointState*;
    auto FindOtherTouchPoint(int64 finger_id) -> TouchPointState*;
    auto AcquireTouchPoint(int64 finger_id) -> TouchPointState*;
    void ReleaseTouchPoint(int64 finger_id);
    void ResetTouchGestures();
    void QueueTouchTap(ipos32 pos);
    void QueueTouchDoubleTap(ipos32 pos);
    void QueueTouchScroll(ipos32 pos, ipos32 delta);
    void QueueTouchZoom(ipos32 pos, float32 factor);
    void FlushPendingTouchTap();

    unique_ptr<Context> _ctx {};
    uint64 _time {};
    uint64 _timeFrequency {};
    bool _isTablet {};
    bool _mouseCanUseGlobalState {};
    int32 _pendingMouseLeaveFrame {};
    int32 _mouseButtonsDown {};
    TouchPointState _touchPrimary {};
    TouchPointState _touchSecondary {};
    PendingTouchTapState _pendingTouchTap {};
    bool _touchPinchActive {};
    bool _touchTapSuppressed {};
    float32 _touchLastPinchDistance {};
    unique_ptr<RenderDrawBuffer> _imguiDrawBuf {};
    unique_ptr<RenderEffect> _imguiEffect {};
    vector<unique_ptr<RenderTexture>> _imguiTextures {};
    vector<raw_ptr<AppWindow>> _allWindows {};
    std::atomic_bool _quit {};
    std::atomic_bool _quitSuccess {true};
    std::condition_variable _quitEvent {};
    std::mutex _quitLocker {};
    EventDispatcher<> _onFrameBeginDispatcher {OnFrameBegin};
    EventDispatcher<> _onFrameEndDispatcher {OnFrameEnd};
    EventDispatcher<> _onPauseDispatcher {OnPause};
    EventDispatcher<> _onResumeDispatcher {OnResume};
    EventDispatcher<> _onLowMemoryDispatcher {OnLowMemory};
    EventDispatcher<> _onQuitDispatcher {OnQuit};
    int32 _nonConstHelper {};
};

inline auto AppWindow::GetRender() noexcept -> IAppRender&
{
    return _app->Render;
}

inline auto AppWindow::GetInput() noexcept -> IAppInput&
{
    return _app->Input;
}

inline auto AppWindow::GetAudio() noexcept -> IAppAudio&
{
    return _app->Audio;
}

inline auto AppWindow::GetOnLowMemory() noexcept -> EventObserver<>&
{
    return _app->OnLowMemory;
}

inline auto AppWindow::GetWindowHandleForInput() const -> WindowInternalHandle*
{
    return _windowHandle ? _windowHandle.get_no_const() : nullptr;
}

extern unique_ptr<Application> App;
extern void InitApp(int32 argc, char** argv, AppInitFlags flags = AppInitFlags::None);
auto GetAppWindowStub(GlobalSettings& settings) -> unique_ptr<IAppWindow>;

FO_END_NAMESPACE
