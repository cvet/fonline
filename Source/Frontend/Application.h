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

#pragma once

#include "Common.h"

#include "FileSystem.h"
#include "Rendering.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE();

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

constexpr string_view_nt WEB_CANVAS_ID = "#canvas";

class AppWindow final
{
    friend class Application;
    friend class AppInput;

public:
    [[nodiscard]] auto GetSize() const -> isize32;
    [[nodiscard]] auto GetScreenSize() const -> isize32;
    [[nodiscard]] auto GetPosition() const -> ipos32;
    [[nodiscard]] auto IsFocused() const -> bool;
    [[nodiscard]] auto IsFullscreen() const -> bool;

    void GrabInput(bool enable);
    void SetSize(isize32 size);
    void SetScreenSize(isize32 size);
    void SetPosition(ipos32 pos);
    void Minimize();
    auto ToggleFullscreen(bool enable) -> bool;
    void Blink();
    void AlwaysOnTop(bool enable);
    void Destroy();

    EventObserver<> OnWindowSizeChanged {};
    EventObserver<> OnScreenSizeChanged {};

private:
    AppWindow() = default;

    raw_ptr<WindowInternalHandle> _windowHandle {};
    bool _grabbed {};
    EventDispatcher<> _onWindowSizeChangedDispatcher {OnWindowSizeChanged};
    EventDispatcher<> _onScreenSizeChangedDispatcher {OnScreenSizeChanged};
    int32 _nonConstHelper {};
};

class AppRender final
{
    friend class Application;

public:
    static constexpr int32 MAX_ATLAS_SIZE = 8192;
    static constexpr int32 MIN_ATLAS_SIZE = 2048;
    static const int32& MAX_ATLAS_WIDTH;
    static const int32& MAX_ATLAS_HEIGHT;
    static const int32& MAX_BONES;

    [[nodiscard]] auto GetRenderTarget() -> RenderTexture*;
    [[nodiscard]] auto CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture>;
    [[nodiscard]] auto CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer>;
    [[nodiscard]] auto CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect>;
    [[nodiscard]] auto CreateOrthoMatrix(float32 left, float32 right, float32 bottom, float32 top, float32 nearp, float32 farp) -> mat44;
    [[nodiscard]] auto IsRenderTargetFlipped() -> bool;

    void SetRenderTarget(RenderTexture* tex);
    void ClearRenderTarget(optional<ucolor> color, bool depth = false, bool stencil = false);
    void EnableScissor(irect32 rect);
    void DisableScissor();

private:
    AppRender() = default;

    int32 _nonConstHelper {};
};

class AppInput final
{
    friend class Application;

public:
    static constexpr size_t DROP_FILE_STRIP_LENGHT = 2048;

    [[nodiscard]] auto GetMousePosition() const -> ipos32;
    [[nodiscard]] auto GetClipboardText() -> const string&;

    auto PollEvent(InputEvent& ev) -> bool;
    void ClearEvents();
    void SetMousePosition(ipos32 pos, const AppWindow* relative_to = nullptr);
    void PushEvent(const InputEvent& ev, bool push_to_this_frame = false);
    void SetClipboardText(string_view text);

private:
    AppInput() = default;

    string _clipboardTextStorage {};
    int32 _nonConstHelper {};
};

class AppAudio final
{
    friend class Application;

public:
    static const int32 AUDIO_FORMAT_U8;
    static const int32 AUDIO_FORMAT_S16;

    using AudioStreamCallback = function<void(uint8, span<uint8>)>;

    [[nodiscard]] auto IsEnabled() const -> bool;

    auto ConvertAudio(int32 format, int32 channels, int32 rate, vector<uint8>& buf) -> bool;
    void SetSource(AudioStreamCallback stream_callback);
    void MixAudio(uint8* output, const uint8* buf, size_t len, int32 volume);
    void LockDevice();
    void UnlockDevice();

private:
    AppAudio() = default;

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

    Application(GlobalSettings&& settings, AppInitFlags flags);

public:
    using ProgressWindowCallback = function<void()>;

    Application(const Application&) = delete;
    Application(Application&&) noexcept = delete;
    auto operator=(const Application&) = delete;
    auto operator=(Application&&) noexcept = delete;
    ~Application() = default;

    [[nodiscard]] auto IsQuitRequested() const -> bool { return _quit; }

    auto CreateChildWindow(isize32 size) -> AppWindow*;
    void OpenLink(string_view link);
    void LoadImGuiEffect(const FileSystem& resources);
#if FO_IOS
    void SetMainLoopCallback(void (*callback)(void*));
#endif
    void BeginFrame();
    void EndFrame();
    void RequestQuit() noexcept;
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
    auto CreateInternalWindow(isize32 size) -> WindowInternalHandle*;

    uint64 _time {};
    uint64 _timeFrequency {};
    bool _isTablet {};
    bool _mouseCanUseGlobalState {};
    int32 _pendingMouseLeaveFrame {};
    int32 _mouseButtonsDown {};
    unique_ptr<RenderDrawBuffer> _imguiDrawBuf {};
    unique_ptr<RenderEffect> _imguiEffect {};
    vector<unique_ptr<RenderTexture>> _imguiTextures {};
    vector<raw_ptr<AppWindow>> _allWindows {};
    std::atomic_bool _quit {};
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

extern raw_ptr<Application> App;
extern void InitApp(int32 argc, char** argv, AppInitFlags flags = AppInitFlags::None);

FO_END_NAMESPACE();
