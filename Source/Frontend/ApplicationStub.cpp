//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
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

class StubAppRender final : public IAppRender
{
public:
    explicit StubAppRender(GlobalSettings& settings)
    {
        _renderer = SafeAlloc::MakeUnique<Null_Renderer>();
        _renderer->Init(settings, nullptr);
    }

    [[nodiscard]] auto GetRenderTarget() -> RenderTexture* override { return _renderTarget.get(); }
    [[nodiscard]] auto CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture> override { return _renderer->CreateTexture(size, linear_filtered, with_depth); }
    [[nodiscard]] auto CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer> override { return _renderer->CreateDrawBuffer(is_static); }
    [[nodiscard]] auto CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect> override { return _renderer->CreateEffect(usage, name, loader); }
    [[nodiscard]] auto CreateOrthoMatrix(float32 left, float32 right, float32 bottom, float32 top, float32 nearp, float32 farp) -> mat44 override { return _renderer->CreateOrthoMatrix(left, right, bottom, top, nearp, farp); }
    [[nodiscard]] auto IsRenderTargetFlipped() -> bool override { return _renderer->IsRenderTargetFlipped(); }

    void SetRenderTarget(RenderTexture* tex) override
    {
        FO_STACK_TRACE_ENTRY();

        _renderTarget = tex;
        _renderer->SetRenderTarget(tex);
    }

    void ClearRenderTarget(optional<ucolor> color, bool depth = false, bool stencil = false) override
    {
        FO_STACK_TRACE_ENTRY();

        _renderer->ClearRenderTarget(color, depth, stencil);
    }

    void EnableScissor(irect32 rect) override
    {
        FO_STACK_TRACE_ENTRY();

        _renderer->EnableScissor(rect);
    }

    void DisableScissor() override
    {
        FO_STACK_TRACE_ENTRY();

        _renderer->DisableScissor();
    }

private:
    unique_ptr<Null_Renderer> _renderer {};
    raw_ptr<RenderTexture> _renderTarget {};
};

class StubAppInput final : public IAppInput
{
public:
    explicit StubAppInput(GlobalSettings& settings) :
        _settings {&settings}
    {
    }

    [[nodiscard]] auto IsMouseAvailable() const noexcept -> bool override { return false; }
    [[nodiscard]] auto GetMousePosition() const -> ipos32 override { return _settings->MousePos; }
    [[nodiscard]] auto GetClipboardText() -> const string& override { return _clipboardTextStorage; }
    [[nodiscard]] auto IsShiftDown() const noexcept -> bool override { return _shiftDown; }
    [[nodiscard]] auto IsCtrlDown() const noexcept -> bool override { return _ctrlDown; }
    [[nodiscard]] auto IsAltDown() const noexcept -> bool override { return _altDown; }

    auto PollEvent(InputEvent& ev) -> bool override
    {
        FO_STACK_TRACE_ENTRY();

        if (_eventsQueue.empty() && !_nextFrameEventsQueue.empty()) {
            _eventsQueue.swap(_nextFrameEventsQueue);
        }

        if (_eventsQueue.empty()) {
            return false;
        }

        ev = _eventsQueue.front();
        _eventsQueue.erase(_eventsQueue.begin());
        return true;
    }

    void ClearEvents() override
    {
        FO_STACK_TRACE_ENTRY();

        _eventsQueue.clear();
        _nextFrameEventsQueue.clear();
    }

    void SetMousePosition(ipos32 pos, const IAppWindow* relative_to = nullptr) override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(relative_to);
        _settings->MousePos = pos;
    }

    void PushEvent(const InputEvent& ev, bool push_to_this_frame = false) override
    {
        FO_STACK_TRACE_ENTRY();

        UpdateModifierState(ev);

        if (push_to_this_frame) {
            _eventsQueue.emplace_back(ev);
        }
        else {
            _nextFrameEventsQueue.emplace_back(ev);
        }
    }

    void SetScreenKeyboardEnabled(bool enabled) override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(enabled);
    }

    void SetClipboardText(string_view text) override
    {
        FO_STACK_TRACE_ENTRY();

        _clipboardTextStorage = string(text);
    }

private:
    void UpdateModifierState(const InputEvent& ev)
    {
        FO_STACK_TRACE_ENTRY();

        const auto update_key_state = [this](KeyCode key, bool pressed) {
            if (key == KeyCode::Lshift || key == KeyCode::Rshift) {
                _shiftDown = pressed;
            }
            else if (key == KeyCode::Lcontrol || key == KeyCode::Rcontrol) {
                _ctrlDown = pressed;
            }
            else if (key == KeyCode::Lmenu || key == KeyCode::Rmenu) {
                _altDown = pressed;
            }
        };

        if (ev.Type == InputEvent::EventType::KeyDownEvent) {
            update_key_state(ev.KeyDown.Code, true);
        }
        else if (ev.Type == InputEvent::EventType::KeyUpEvent) {
            update_key_state(ev.KeyUp.Code, false);
        }
    }

    raw_ptr<GlobalSettings> _settings {};
    string _clipboardTextStorage {};
    vector<InputEvent> _eventsQueue {};
    vector<InputEvent> _nextFrameEventsQueue {};
    bool _shiftDown {};
    bool _ctrlDown {};
    bool _altDown {};
};

class StubAppAudio final : public IAppAudio
{
public:
    [[nodiscard]] auto IsEnabled() const -> bool override { return false; }

    auto ConvertAudio(int32 format, int32 channels, int32 rate, vector<uint8>& buf) -> bool override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(format, channels, rate, buf);
        return false;
    }

    void SetSource(AudioStreamCallback stream_callback) override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(stream_callback);
    }

    void MixAudio(uint8* output, const uint8* buf, size_t len, int32 volume) override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(output, buf, len, volume);
    }

    void LockDevice() override { FO_STACK_TRACE_ENTRY(); }

    void UnlockDevice() override { FO_STACK_TRACE_ENTRY(); }
};

class StubAppWindow final : public IAppWindow
{
public:
    explicit StubAppWindow(GlobalSettings& settings) :
        _render {settings},
        _input {settings}
    {
        FO_STACK_TRACE_ENTRY();

        _state.Size = {settings.ScreenWidth, settings.ScreenHeight};
    }

    [[nodiscard]] auto GetSize() const -> isize32 override { return _state.Size; }
    [[nodiscard]] auto GetScreenSize() const -> isize32 override { return _state.Size; }
    [[nodiscard]] auto GetPosition() const -> ipos32 override { return _state.Position; }
    [[nodiscard]] auto IsFocused() const -> bool override { return !_state.Minimized; }
    [[nodiscard]] auto IsFullscreen() const -> bool override { return _state.Fullscreen; }
    [[nodiscard]] auto GetRender() noexcept -> IAppRender& override { return _render; }
    [[nodiscard]] auto GetInput() noexcept -> IAppInput& override { return _input; }
    [[nodiscard]] auto GetAudio() noexcept -> IAppAudio& override { return _audio; }
    [[nodiscard]] auto GetOnWindowSizeChanged() noexcept -> EventObserver<>& override { return OnWindowSizeChanged; }
    [[nodiscard]] auto GetOnScreenSizeChanged() noexcept -> EventObserver<>& override { return OnScreenSizeChanged; }
    [[nodiscard]] auto GetOnLowMemory() noexcept -> EventObserver<>& override { return OnLowMemory; }
    [[nodiscard]] auto GetWindowHandleForInput() const -> WindowInternalHandle* override { return nullptr; }

    void GrabInput(bool enable) override
    {
        FO_STACK_TRACE_ENTRY();
        _grabbed = enable;
    }

    void SetSize(isize32 size) override
    {
        FO_STACK_TRACE_ENTRY();

        if (_state.Size != size) {
            _state.Size = size;
            _onWindowSizeChangedDispatcher();
        }
    }

    void SetScreenSize(isize32 size) override
    {
        FO_STACK_TRACE_ENTRY();

        if (_state.Size != size) {
            _state.Size = size;
            _onScreenSizeChangedDispatcher();
        }
    }

    void SetPosition(ipos32 pos) override
    {
        FO_STACK_TRACE_ENTRY();

        _state.Position = pos;
    }

    void Minimize() override
    {
        FO_STACK_TRACE_ENTRY();

        _state.Minimized = true;
    }

    auto ToggleFullscreen(bool enable) -> bool override
    {
        FO_STACK_TRACE_ENTRY();

        _state.Fullscreen = enable;
        return _state.Fullscreen;
    }

    void Blink() override { FO_STACK_TRACE_ENTRY(); }

    void AlwaysOnTop(bool enable) override
    {
        FO_STACK_TRACE_ENTRY();

        _state.AlwaysOnTop = enable;
    }

    void Destroy() override { FO_STACK_TRACE_ENTRY(); }

    EventObserver<> OnWindowSizeChanged {};
    EventObserver<> OnScreenSizeChanged {};
    EventObserver<> OnLowMemory {};

private:
    HeadlessWindowStub _state {};
    StubAppRender _render;
    StubAppInput _input;
    StubAppAudio _audio;
    bool _grabbed {};
    EventDispatcher<> _onWindowSizeChangedDispatcher {OnWindowSizeChanged};
    EventDispatcher<> _onScreenSizeChangedDispatcher {OnScreenSizeChanged};
};

auto GetAppWindowStub(GlobalSettings& settings) -> unique_ptr<IAppWindow>
{
    FO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeUnique<StubAppWindow>(settings);
}

FO_END_NAMESPACE
