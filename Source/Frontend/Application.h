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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

// Physical SDL-scancode key identifiers used by input events and key-state queries, plus a synthetic Text event for UTF-8 text, paste, and drop payloads
///@ ExportEnum
enum class KeyCode : uint8_t
{
    None = 0x00, // Indicates that no physical or synthetic keyboard input code is selected
    Escape = 0x01, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_ESCAPE` by the application input layer
    C1 = 0x02, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_1` by the application input layer
    C2 = 0x03, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_2` by the application input layer
    C3 = 0x04, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_3` by the application input layer
    C4 = 0x05, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_4` by the application input layer
    C5 = 0x06, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_5` by the application input layer
    C6 = 0x07, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_6` by the application input layer
    C7 = 0x08, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_7` by the application input layer
    C8 = 0x09, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_8` by the application input layer
    C9 = 0x0A, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_9` by the application input layer
    C0 = 0x0B, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_0` by the application input layer
    Minus = 0x0C, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_MINUS` by the application input layer
    Equals = 0x0D, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_EQUALS` by the application input layer
    Back = 0x0E, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_BACKSPACE` by the application input layer
    Tab = 0x0F, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_TAB` by the application input layer
    Q = 0x10, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_Q` by the application input layer
    W = 0x11, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_W` by the application input layer
    E = 0x12, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_E` by the application input layer
    R = 0x13, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_R` by the application input layer
    T = 0x14, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_T` by the application input layer
    Y = 0x15, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_Y` by the application input layer
    U = 0x16, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_U` by the application input layer
    I = 0x17, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_I` by the application input layer
    O = 0x18, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_O` by the application input layer
    P = 0x19, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_P` by the application input layer
    Lbracket = 0x1A, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_LEFTBRACKET` by the application input layer
    Rbracket = 0x1B, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_RIGHTBRACKET` by the application input layer
    Return = 0x1C, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_RETURN` by the application input layer
    Lcontrol = 0x1D, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_LCTRL` by the application input layer
    A = 0x1E, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_A` by the application input layer
    S = 0x1F, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_S` by the application input layer
    D = 0x20, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_D` by the application input layer
    F = 0x21, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_F` by the application input layer
    G = 0x22, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_G` by the application input layer
    H = 0x23, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_H` by the application input layer
    J = 0x24, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_J` by the application input layer
    K = 0x25, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_K` by the application input layer
    L = 0x26, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_L` by the application input layer
    Semicolon = 0x27, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_SEMICOLON` by the application input layer
    Apostrophe = 0x28, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_APOSTROPHE` by the application input layer
    Grave = 0x29, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_GRAVE` by the application input layer
    Lshift = 0x2A, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_LSHIFT` by the application input layer
    Backslash = 0x2B, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_BACKSLASH` by the application input layer
    Z = 0x2C, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_Z` by the application input layer
    X = 0x2D, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_X` by the application input layer
    C = 0x2E, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_C` by the application input layer
    V = 0x2F, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_V` by the application input layer
    B = 0x30, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_B` by the application input layer
    N = 0x31, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_N` by the application input layer
    M = 0x32, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_M` by the application input layer
    Comma = 0x33, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_COMMA` by the application input layer
    Period = 0x34, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_PERIOD` by the application input layer
    Slash = 0x35, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_SLASH` by the application input layer
    Rshift = 0x36, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_RSHIFT` by the application input layer
    Multiply = 0x37, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_KP_MULTIPLY` by the application input layer
    Lmenu = 0x38, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_LALT` by the application input layer
    Space = 0x39, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_SPACE` by the application input layer
    Capital = 0x3A, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_CAPSLOCK` by the application input layer
    F1 = 0x3B, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_F1` by the application input layer
    F2 = 0x3C, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_F2` by the application input layer
    F3 = 0x3D, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_F3` by the application input layer
    F4 = 0x3E, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_F4` by the application input layer
    F5 = 0x3F, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_F5` by the application input layer
    F6 = 0x40, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_F6` by the application input layer
    F7 = 0x41, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_F7` by the application input layer
    F8 = 0x42, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_F8` by the application input layer
    F9 = 0x43, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_F9` by the application input layer
    F10 = 0x44, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_F10` by the application input layer
    Numlock = 0x45, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_NUMLOCKCLEAR` by the application input layer
    Scroll = 0x46, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_SCROLLLOCK` by the application input layer
    Numpad7 = 0x47, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_KP_7` by the application input layer
    Numpad8 = 0x48, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_KP_8` by the application input layer
    Numpad9 = 0x49, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_KP_9` by the application input layer
    Subtract = 0x4A, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_KP_MINUS` by the application input layer
    Numpad4 = 0x4B, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_KP_4` by the application input layer
    Numpad5 = 0x4C, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_KP_5` by the application input layer
    Numpad6 = 0x4D, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_KP_6` by the application input layer
    Add = 0x4E, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_KP_PLUS` by the application input layer
    Numpad1 = 0x4F, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_KP_1` by the application input layer
    Numpad2 = 0x50, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_KP_2` by the application input layer
    Numpad3 = 0x51, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_KP_3` by the application input layer
    Numpad0 = 0x52, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_KP_0` by the application input layer
    Decimal = 0x53, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_KP_PERIOD` by the application input layer
    F11 = 0x57, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_F11` by the application input layer
    F12 = 0x58, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_F12` by the application input layer
    Numpadenter = 0x9C, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_KP_ENTER` by the application input layer
    Rcontrol = 0x9D, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_RCTRL` by the application input layer
    Divide = 0xB5, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_KP_DIVIDE` by the application input layer
    Sysrq = 0xB7, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_SYSREQ` by the application input layer
    Rmenu = 0xB8, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_RALT` by the application input layer
    Pause = 0xC5, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_PAUSE` by the application input layer
    Home = 0xC7, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_HOME` by the application input layer
    Up = 0xC8, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_UP` by the application input layer
    Prior = 0xC9, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_PAGEUP` by the application input layer
    Left = 0xCB, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_LEFT` by the application input layer
    Right = 0xCD, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_RIGHT` by the application input layer
    End = 0xCF, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_END` by the application input layer
    Down = 0xD0, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_DOWN` by the application input layer
    Next = 0xD1, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_PAGEDOWN` by the application input layer
    Insert = 0xD2, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_INSERT` by the application input layer
    Delete = 0xD3, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_DELETE` by the application input layer
    Lwin = 0xDB, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_LGUI` by the application input layer
    Rwin = 0xDC, // Identifies the physical keyboard key mapped from `SDL_SCANCODE_RGUI` by the application input layer
    Text = 0xFF, // Identifies a synthetic text event whose UTF-8 input, clipboard, or dropped-data payload is carried separately by the input event
};

// Mouse buttons and wheel directions exposed through application input events
///@ ExportEnum
enum class MouseButton : uint8_t
{
    Left = 0, // Primary mouse button mapped from the platform left-button input
    Right = 1, // Secondary mouse button mapped from the platform right-button input
    Middle = 2, // Middle mouse button mapped from the platform middle-button input
    WheelUp = 3, // Synthetic button event emitted for upward mouse-wheel motion
    WheelDown = 4, // Synthetic button event emitted for downward mouse-wheel motion
    Ext0 = 5, // First extended mouse button, mapped from the platform X1 button
    Ext1 = 6, // Second extended mouse button, mapped from the platform X2 button
    Ext2 = 7, // Third extended mouse button, mapped from platform button 6
    Ext3 = 8, // Fourth extended mouse button, mapped from platform button 7
    Ext4 = 9, // Fifth extended mouse button, mapped from platform button 8
};

// Per-frame gamepad snapshot containing availability, stick and trigger values, face buttons, shoulders, sticks, and D-pad state
// LeftStickX: Left-stick horizontal axis normalized to -1 through 1 after the dead zone
// LeftStickY: Left-stick vertical axis normalized to -1 through 1 after the dead zone
// RightStickX: Right-stick horizontal axis normalized to -1 through 1 after the dead zone
// RightStickY: Right-stick vertical axis normalized to -1 through 1 after the dead zone
// LeftTrigger: Left-trigger pressure normalized to 0 through 1 after the dead zone
// RightTrigger: Right-trigger pressure normalized to 0 through 1 after the dead zone
// Available: Reports whether the application currently has an opened gamepad
// South: Reports whether the south face button is pressed
// East: Reports whether the east face button is pressed
// West: Reports whether the west face button is pressed
// North: Reports whether the north face button is pressed
// Back: Reports whether the back button is pressed
// Start: Reports whether the start button is pressed
// LeftStickButton: Reports whether the left-stick button is pressed
// RightStickButton: Reports whether the right-stick button is pressed
// LeftShoulder: Reports whether the left shoulder button is pressed
// RightShoulder: Reports whether the right shoulder button is pressed
// DpadUp: Reports whether the D-pad up button is pressed
// DpadDown: Reports whether the D-pad down button is pressed
// DpadLeft: Reports whether the D-pad left button is pressed
// DpadRight: Reports whether the D-pad right button is pressed
// Reserved: Reserved compatibility field; the current input backend leaves it false
///@ ExportValueType Layout = float32-LeftStickX+float32-LeftStickY+float32-RightStickX+float32-RightStickY+float32-LeftTrigger+float32-RightTrigger+bool-Available+bool-South+bool-East+bool-West+bool-North+bool-Back+bool-Start+bool-LeftStickButton+bool-RightStickButton+bool-LeftShoulder+bool-RightShoulder+bool-DpadUp+bool-DpadDown+bool-DpadLeft+bool-DpadRight+bool-Reserved
struct GamepadState
{
    float32_t LeftStickX {};
    float32_t LeftStickY {};
    float32_t RightStickX {};
    float32_t RightStickY {};
    float32_t LeftTrigger {};
    float32_t RightTrigger {};
    bool Available {};
    bool South {};
    bool East {};
    bool West {};
    bool North {};
    bool Back {};
    bool Start {};
    bool LeftStickButton {};
    bool RightStickButton {};
    bool LeftShoulder {};
    bool RightShoulder {};
    bool DpadUp {};
    bool DpadDown {};
    bool DpadLeft {};
    bool DpadRight {};
    bool Reserved {};
};
static_assert(sizeof(GamepadState) == 40 && std::is_standard_layout_v<GamepadState>);

struct InputEvent
{
    enum class EventType : uint8_t
    {
        NoneEvent,
        MouseMoveEvent,
        MouseDownEvent,
        MouseUpEvent,
        MouseWheelEvent,
        TouchDownEvent,
        TouchMoveEvent,
        TouchUpEvent,
        TouchTapEvent,
        TouchDoubleTapEvent,
        TouchScrollEvent,
        TouchZoomEvent,
        KeyDownEvent,
        KeyUpEvent,
    } Type {};

    struct MouseMoveEvent
    {
        int32_t MouseX {};
        int32_t MouseY {};
        int32_t DeltaX {};
        int32_t DeltaY {};
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
        int32_t Delta {};
    } MouseWheel {};

    struct TouchDownEvent
    {
        int64_t FingerId {};
        int32_t TouchX {};
        int32_t TouchY {};
    } TouchDown {};

    struct TouchMoveEvent
    {
        int64_t FingerId {};
        int32_t TouchX {};
        int32_t TouchY {};
        int32_t DeltaX {};
        int32_t DeltaY {};
    } TouchMove {};

    struct TouchUpEvent
    {
        int64_t FingerId {};
        int32_t TouchX {};
        int32_t TouchY {};
    } TouchUp {};

    struct TouchTapEvent
    {
        int32_t TouchX {};
        int32_t TouchY {};
    } TouchTap {};

    struct TouchDoubleTapEvent
    {
        int32_t TouchX {};
        int32_t TouchY {};
    } TouchDoubleTap {};

    struct TouchScrollEvent
    {
        int32_t TouchX {};
        int32_t TouchY {};
        int32_t DeltaX {};
        int32_t DeltaY {};
    } TouchScroll {};

    struct TouchZoomEvent
    {
        int32_t TouchX {};
        int32_t TouchY {};
        float32_t Factor {};
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
    explicit InputEvent(TouchDownEvent ev) :
        Type {EventType::TouchDownEvent},
        TouchDown {ev}
    {
    }
    explicit InputEvent(TouchMoveEvent ev) :
        Type {EventType::TouchMoveEvent},
        TouchMove {ev}
    {
    }
    explicit InputEvent(TouchUpEvent ev) :
        Type {EventType::TouchUpEvent},
        TouchUp {ev}
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

    [[nodiscard]] virtual auto GetRenderTarget() -> nptr<RenderTexture> = 0;
    [[nodiscard]] virtual auto CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture> = 0;
    [[nodiscard]] virtual auto CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer> = 0;
    [[nodiscard]] virtual auto CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect> = 0;
    [[nodiscard]] virtual auto CreateOrthoMatrix(float32_t left, float32_t right, float32_t bottom, float32_t top, float32_t nearp, float32_t farp) const -> mat44 = 0;
    [[nodiscard]] virtual auto IsRenderTargetFlipped() const -> bool = 0;
    [[nodiscard]] virtual auto GetProjMatrix() const -> mat44 = 0;

    virtual void SetRenderTarget(nptr<RenderTexture> tex) = 0;
    virtual void SetOrthoDepthRange(float32_t nearp, float32_t farp) noexcept = 0;
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
    [[nodiscard]] virtual auto GetGamepadState() const noexcept -> GamepadState = 0;
    [[nodiscard]] virtual auto GetClipboardText() -> const string& = 0;
    [[nodiscard]] virtual auto IsShiftDown() const noexcept -> bool = 0;
    [[nodiscard]] virtual auto IsCtrlDown() const noexcept -> bool = 0;
    [[nodiscard]] virtual auto IsAltDown() const noexcept -> bool = 0;

    virtual auto PollEvent(InputEvent& ev) -> bool = 0;
    virtual void ClearEvents() = 0;
    virtual void SetMousePosition(ipos32 pos, nptr<const IAppWindow> relative_to = nullptr) = 0;
    virtual void PushEvent(const InputEvent& ev, bool push_to_this_frame = false) = 0;
    virtual void SetScreenKeyboardEnabled(bool enabled) = 0;
    virtual void SetClipboardText(string_view text) = 0;
};

class IAppAudio
{
public:
    using AudioStreamCallback = function<void(uint8_t, span<uint8_t>)>;

    virtual ~IAppAudio() = default;

    [[nodiscard]] virtual auto IsEnabled() const -> bool = 0;

    virtual auto ConvertAudio(int32_t channels, int32_t rate, vector<uint8_t>& buf) -> bool = 0;
    virtual void SetSource(AudioStreamCallback stream_callback) = 0;
    virtual void MixAudio(span<uint8_t> output, const_span<uint8_t> buf, int32_t volume) = 0;
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
    [[nodiscard]] virtual auto IsVirtual() const noexcept -> bool = 0;
    [[nodiscard]] virtual auto GetRender() noexcept -> ptr<IAppRender> = 0;
    [[nodiscard]] virtual auto GetInput() noexcept -> ptr<IAppInput> = 0;
    [[nodiscard]] virtual auto GetAudio() noexcept -> ptr<IAppAudio> = 0;
    [[nodiscard]] virtual auto GetOnWindowSizeChanged() noexcept -> ptr<EventObserver<>> = 0;
    [[nodiscard]] virtual auto GetOnScreenSizeChanged() noexcept -> ptr<EventObserver<>> = 0;
    [[nodiscard]] virtual auto GetOnLowMemory() noexcept -> ptr<EventObserver<>> = 0;
    [[nodiscard]] virtual auto GetWindowHandleForInput() const -> nptr<WindowInternalHandle> = 0;

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
    friend class safe_alloc;

public:
    [[nodiscard]] auto GetSize() const -> isize32 override;
    [[nodiscard]] auto GetScreenSize() const -> isize32 override;
    [[nodiscard]] auto GetPosition() const -> ipos32 override;
    [[nodiscard]] auto IsFocused() const -> bool override;
    [[nodiscard]] auto IsFullscreen() const -> bool override;
    [[nodiscard]] auto GetRender() noexcept -> ptr<IAppRender> override;
    [[nodiscard]] auto GetInput() noexcept -> ptr<IAppInput> override;
    [[nodiscard]] auto GetAudio() noexcept -> ptr<IAppAudio> override;
    [[nodiscard]] auto GetOnWindowSizeChanged() noexcept -> ptr<EventObserver<>> override { return &OnWindowSizeChanged; }
    [[nodiscard]] auto GetOnScreenSizeChanged() noexcept -> ptr<EventObserver<>> override { return &OnScreenSizeChanged; }
    [[nodiscard]] auto GetOnLowMemory() noexcept -> ptr<EventObserver<>> override;
    [[nodiscard]] auto GetWindowHandleForInput() const -> nptr<WindowInternalHandle> override;
    [[nodiscard]] auto IsVirtual() const noexcept -> bool override { return _isVirtual; }
    [[nodiscard]] auto GetTitle() const noexcept -> string_view { return _title; }
    [[nodiscard]] auto GetRenderTexture() noexcept -> nptr<RenderTexture> { return _virtualRenderTex; }
    [[nodiscard]] auto GetDisplayRect() const noexcept -> irect32 { return _displayRect; }

    void GrabInput(bool enable) override;
    void SetSize(isize32 size) override;
    void SetScreenSize(isize32 size) override;
    void SetPosition(ipos32 pos) override;
    void Minimize() override;
    auto ToggleFullscreen(bool enable) -> bool override;
    void Blink() override;
    void AlwaysOnTop(bool enable) override;
    void Destroy() override;
    void SetTitle(string_view title);
    void SetDisplayRect(irect32 rect) noexcept { _displayRect = rect; }

    EventObserver<> OnWindowSizeChanged {};
    EventObserver<> OnScreenSizeChanged {};

private:
    explicit AppWindow(ptr<Application> app) :
        _app {app}
    {
    }

    [[nodiscard]] auto ResolveWindowHandle() const -> ptr<WindowInternalHandle>;
    [[nodiscard]] auto ResolveWindowStub() const -> ptr<HeadlessWindowStub>;

    ptr<Application> _app;
    nptr<WindowInternalHandle> _windowHandle {};
    bool _grabbed {};
    bool _isVirtual {};
    string _title {};
    isize32 _virtualSize {};
    isize32 _virtualScreenSize {};
    ipos32 _virtualPosition {};
    isize32 _virtualLayoutSize {};
    irect32 _displayRect {};
    unique_nptr<RenderTexture> _virtualRenderTex {};
    EventDispatcher<> _onWindowSizeChangedDispatcher {&OnWindowSizeChanged};
    EventDispatcher<> _onScreenSizeChangedDispatcher {&OnScreenSizeChanged};
};

class AppRender final : public IAppRender
{
    friend class Application;

public:
    static constexpr int32_t MAX_ATLAS_SIZE = 8192;
    static constexpr int32_t MIN_ATLAS_SIZE = 2048;
    static int32_t MAX_ATLAS_WIDTH;
    static int32_t MAX_ATLAS_HEIGHT;
    static int32_t MAX_BONES;

    [[nodiscard]] auto GetRenderTarget() -> nptr<RenderTexture> override;
    [[nodiscard]] auto CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture> override;
    [[nodiscard]] auto CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer> override;
    [[nodiscard]] auto CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect> override;
    [[nodiscard]] auto CreateOrthoMatrix(float32_t left, float32_t right, float32_t bottom, float32_t top, float32_t nearp, float32_t farp) const -> mat44 override;
    [[nodiscard]] auto IsRenderTargetFlipped() const -> bool override;
    [[nodiscard]] auto GetProjMatrix() const -> mat44 override;

    void SetRenderTarget(nptr<RenderTexture> tex) override;
    void SetOrthoDepthRange(float32_t nearp, float32_t farp) noexcept override;
    void ClearRenderTarget(optional<ucolor> color, bool depth = false, bool stencil = false) override;
    void EnableScissor(irect32 rect) override;
    void DisableScissor() override;

private:
    explicit AppRender(ptr<Application> app) :
        _app {app}
    {
    }

    ptr<Application> _app;
};

class AppInput final : public IAppInput
{
    friend class Application;

public:
    static constexpr size_t DROP_FILE_STRIP_LENGHT = 2048;

    [[nodiscard]] auto IsMouseAvailable() const noexcept -> bool override;
    [[nodiscard]] auto GetMousePosition() const -> ipos32 override;
    [[nodiscard]] auto GetGamepadState() const noexcept -> GamepadState override;
    [[nodiscard]] auto GetClipboardText() -> const string& override;
    [[nodiscard]] auto IsShiftDown() const noexcept -> bool override { return _shiftDown; }
    [[nodiscard]] auto IsCtrlDown() const noexcept -> bool override { return _ctrlDown; }
    [[nodiscard]] auto IsAltDown() const noexcept -> bool override { return _altDown; }

    auto PollEvent(InputEvent& ev) -> bool override;
    void ClearEvents() override;
    void SetMousePosition(ipos32 pos, nptr<const IAppWindow> relative_to = nullptr) override;
    void PushEvent(const InputEvent& ev, bool push_to_this_frame = false) override;
    void SetScreenKeyboardEnabled(bool enabled) override;
    void SetClipboardText(string_view text) override;

private:
    explicit AppInput(ptr<Application> app) :
        _app {app}
    {
    }

    ptr<Application> _app;
    string _clipboardTextStorage {};
    ipos32 _lastMousePos {};
    bool _shiftDown {};
    bool _ctrlDown {};
    bool _altDown {};
};

class AppAudio final : public IAppAudio
{
    friend class Application;

public:
    using AudioStreamCallback = IAppAudio::AudioStreamCallback;

    [[nodiscard]] auto IsEnabled() const -> bool override;

    auto ConvertAudio(int32_t channels, int32_t rate, vector<uint8_t>& buf) -> bool override;
    void SetSource(AudioStreamCallback stream_callback) override;
    void MixAudio(span<uint8_t> output, const_span<uint8_t> buf, int32_t volume) override;
    void LockDevice() override;
    void UnlockDevice() override;

private:
    explicit AppAudio(ptr<Application> app) :
        _app {app}
    {
    }

    ptr<Application> _app;
};

enum class AppInitFlags : uint8_t
{
    None = 0x00,
    ClientMode = 0x01,
    DisableLogTags = 0x02,
    ShowMessageOnException = 0x04,
    PrebakeResources = 0x08,
    AppendLogFile = 0x10,
};

class Application final
{
    friend void InitApp(CommandLineArgs args, AppInitFlags flags);
    friend class safe_alloc;
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

    [[nodiscard]] auto IsQuitRequested() const -> bool;
    [[nodiscard]] auto GetRequestedQuitSuccess() const -> bool { return _quitSuccess; }
    [[nodiscard]] auto IsHeadless() const noexcept -> bool;
    [[nodiscard]] auto GetActiveWindow() noexcept -> nptr<AppWindow> { return _activeWindow; }
    [[nodiscard]] auto GetChildWindowsCount() const noexcept -> size_t { return _childWindows.size(); }
    [[nodiscard]] auto GetChildWindow(size_t index) noexcept -> nptr<AppWindow>
    {
        if (index < _childWindows.size()) {
            return _childWindows[index];
        }

        return nullptr;
    }
    [[nodiscard]] auto TranslateHostPosToActiveWindow(ipos32 pos) const -> ipos32;
    [[nodiscard]] auto TranslateActiveWindowPosToHost(ipos32 pos) const -> ipos32;
    [[nodiscard]] auto ScaleHostDeltaToActiveWindow(ipos32 delta) const -> ipos32;

    auto CreateChildWindow(isize32 size, string_view title = {}) -> ptr<AppWindow>;
    void DestroyChildWindow(nptr<AppWindow> window);
    void SetActiveWindow(nptr<AppWindow> window);
    void BeginWindowRender(ptr<AppWindow> window);
    void EndWindowRender();

    void OpenLink(string_view link);
    void LoadImGuiEffect(const FileSystem& resources);
    void BeginFrame();
    void EndFrame();
    void RequestQuit(bool success = true) noexcept;
    void WaitForRequestedQuit();

#if FO_IOS
    void SetMainLoopCallback(void (*callback)(void*));
#endif

    static void ShowErrorMessage(string_view message, string_view traceback, bool fatal_error);
    static void ShowProgressWindow(string_view text, const ProgressWindowCallback& callback);
    static void ChooseOptionsWindow(string_view title, const vector<string>& options, set<int32_t>& selected);

    GlobalSettings Settings;

    EventObserver<> OnFrameBegin {};
    EventObserver<> OnFrameEnd {};
    EventObserver<> OnPause {};
    EventObserver<> OnResume {};
    EventObserver<> OnLowMemory {};
    EventObserver<> OnQuit {};

    AppWindow MainWindow;
    AppScreenState ScreenState {};
    AppRender Render;
    AppInput Input;
    AppAudio Audio;

private:
    struct TouchPointState
    {
        int64_t FingerId {-1};
        ipos32 StartPos {};
        ipos32 LastPos {};
        uint64_t StartTime {};
        bool Active {};
        bool ScrollActive {};
    };

    struct PendingTouchTapState
    {
        ipos32 Pos {};
        uint64_t ReleaseTime {};
        bool Active {};
    };

    static constexpr uint32_t TOUCH_TAP_MAX_TIME_MS = 250;
    static constexpr uint32_t TOUCH_DOUBLE_TAP_MAX_TIME_MS = 200;
    static constexpr int32_t TOUCH_TAP_MAX_DIST = 12;
    static constexpr int32_t TOUCH_DOUBLE_TAP_MAX_DIST = 48;

    struct Context;

    auto CreateInternalWindow(isize32 size) -> ptr<WindowInternalHandle>;
    void EnsureVirtualRenderTexture(ptr<AppWindow> window, isize32 size);
    auto IsMainWindowActuallyFullscreen() const -> bool;
    auto IsMainWindowDisplayModeSize(isize32 size) const -> bool;
    auto GetMainWindowBackbufferSize() const -> isize32;
    void SyncMainWindowBackbufferSize();
    auto MakeAspectFitRect(isize32 source_size, isize32 target_size) const -> irect32;
    auto ResolveTouchPos(float32_t normalized_x, float32_t normalized_y) const -> ipos32;
    auto GetTouchElapsedMs(uint64_t start_time, uint64_t end_time) const -> uint32_t;
    auto GetTouchDistance(ipos32 from, ipos32 to) const -> float32_t;
    auto FindTouchPoint(int64_t finger_id) -> nptr<TouchPointState>;
    auto FindOtherTouchPoint(int64_t finger_id) -> nptr<TouchPointState>;
    auto AcquireTouchPoint(int64_t finger_id) -> nptr<TouchPointState>;
    void ReleaseTouchPoint(int64_t finger_id);
    void ResetTouchGestures();
    void QueueTouchDown(int64_t finger_id, ipos32 pos);
    void QueueTouchMove(int64_t finger_id, ipos32 pos, ipos32 delta);
    void QueueTouchUp(int64_t finger_id, ipos32 pos);
    void QueueTouchTap(ipos32 pos);
    void QueueTouchDoubleTap(ipos32 pos);
    void QueueTouchScroll(ipos32 pos, ipos32 delta);
    void QueueTouchZoom(ipos32 pos, float32_t factor);
    void FlushPendingTouchTap();
    void UpdateNativeCursorVisibility(bool imguiOverlayWantsCursor);
    void CloseGamepad();
    void RefreshGamepadConnection();
    void UpdateGamepadAxis(int32_t axis, int32_t value);
    void UpdateGamepadButton(int32_t button, bool pressed);

    unique_ptr<Context> _ctx;
    uint64_t _time {};
    uint64_t _timeFrequency {};
    bool _isTablet {};
    bool _clientMode {};
    bool _nativeCursorHidden {};
    bool _mouseCanUseGlobalState {};
    int32_t _pendingMouseLeaveFrame {};
    int32_t _mouseButtonsDown {};
    ipos32 _lastMouseMoveHostPos {};
    bool _lastMouseMoveHostPosValid {};
    TouchPointState _touchPrimary {};
    TouchPointState _touchSecondary {};
    PendingTouchTapState _pendingTouchTap {};
    bool _touchPinchActive {};
    bool _touchTapSuppressed {};
    float32_t _touchLastPinchDistance {};
    nptr<void> _gamepadHandle {};
    int32_t _gamepadInstanceId {-1};
    GamepadState _gamepadState {};
    unique_nptr<RenderDrawBuffer> _imguiDrawBuf {};
    unique_nptr<RenderEffect> _imguiEffect {};
    vector<unique_ptr<RenderTexture>> _imguiTextures {};
    vector<ptr<AppWindow>> _allWindows {};
    vector<unique_ptr<AppWindow>> _childWindows {};
    nptr<AppWindow> _activeWindow {};
    nptr<AppWindow> _currentRenderingWindow {};
    nptr<RenderTexture> _previousRenderTarget {};
    int32_t _hostScreenWidthSaved {};
    int32_t _hostScreenHeightSaved {};
    bool _hostScreenSizeSaved {};
    bool _mainWindowFullscreenTransition {};
    bool _mainWindowFullscreenBackbufferMode {};
    std::atomic_bool _quit {};
    std::atomic_bool _quitSuccess {true};
    std::condition_variable_any _quitEvent {};
    mutex _quitLocker {};
    EventDispatcher<> _onFrameBeginDispatcher {&OnFrameBegin};
    EventDispatcher<> _onFrameEndDispatcher {&OnFrameEnd};
    EventDispatcher<> _onPauseDispatcher {&OnPause};
    EventDispatcher<> _onResumeDispatcher {&OnResume};
    EventDispatcher<> _onLowMemoryDispatcher {&OnLowMemory};
    EventDispatcher<> _onQuitDispatcher {&OnQuit};
};

inline auto AppWindow::GetRender() noexcept -> ptr<IAppRender>
{
    return &_app->Render;
}

inline auto AppWindow::GetInput() noexcept -> ptr<IAppInput>
{
    return &_app->Input;
}

inline auto AppWindow::GetAudio() noexcept -> ptr<IAppAudio>
{
    return &_app->Audio;
}

inline auto AppWindow::GetOnLowMemory() noexcept -> ptr<EventObserver<>>
{
    return &_app->OnLowMemory;
}

inline auto AppWindow::GetWindowHandleForInput() const -> nptr<WindowInternalHandle>
{
    return _windowHandle;
}

auto IsAppInitialized() noexcept -> bool;
auto GetApp() noexcept -> ptr<Application>;
void ResetApp() noexcept;
auto LoadAppSettings(CommandLineArgs args) -> GlobalSettings;
void InitApp(CommandLineArgs args, AppInitFlags flags = AppInitFlags::None);
void InitAppForTesting(AppInitFlags flags = AppInitFlags::None);
auto GetExeLogFileName() -> string;
auto GetExePreviousLogFileName() -> string;
auto ResolveWritableRoot(CommandLineArgs args) -> string;
auto GetAppWindowStub(GlobalSettings& settings) -> unique_ptr<IAppWindow>;
auto IsQuitSignalReceived() noexcept -> bool;

FO_END_NAMESPACE
