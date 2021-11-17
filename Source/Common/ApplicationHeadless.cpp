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
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

// Todo: move different renderers to separate modules

#include "Application.h"
#include "DiskFileSystem.h"
#include "Log.h"
#include "StringUtils.h"
#include "WinApi-Include.h"

Application* App;

#if FO_WINDOWS && FO_DEBUG
static _CrtMemState CrtMemState;
#endif

const uint Application::AppRender::MAX_ATLAS_WIDTH = 1024;
const uint Application::AppRender::MAX_ATLAS_HEIGHT = 1024;
const uint Application::AppRender::MAX_BONES = 32;
const int Application::AppAudio::AUDIO_FORMAT_U8 = 0;
const int Application::AppAudio::AUDIO_FORMAT_S16 = 1;

struct RenderTexture::Impl
{
    virtual ~Impl() = default;
};

RenderTexture::~RenderTexture()
{
}

struct RenderEffect::Impl
{
    virtual ~Impl() = default;
};

RenderEffect::~RenderEffect()
{
}

auto RenderEffect::IsSame(string_view name, string_view defines) const -> bool
{
    return _str(name).compareIgnoreCase(_effectName) && defines == _effectDefines;
}

auto RenderEffect::CanBatch(const RenderEffect* other) const -> bool
{
    return false;
}

struct RenderMesh::Impl
{
    virtual ~Impl() = default;
};

RenderMesh::~RenderMesh()
{
}

void InitApplication(GlobalSettings& settings)
{
    // Ensure that we call init only once
    static std::once_flag once;
    auto first_call = false;
    std::call_once(once, [&first_call] { first_call = true; });
    if (!first_call) {
        throw AppInitException("Application::Init must be called only once");
    }

    App = new Application(settings);
}

Application::Application(GlobalSettings& settings)
{
    // Skip SDL allocations from profiling
#if FO_WINDOWS && FO_DEBUG
    ::_CrtMemCheckpoint(&CrtMemState);
#endif
}

#if FO_IOS
void Application::SetMainLoopCallback(void (*callback)(void*))
{
}
#endif

void Application::BeginFrame()
{
    _onFrameBeginDispatcher();
}

void Application::EndFrame()
{
    _onFrameEndDispatcher();
}

auto Application::AppWindow::GetSize() const -> tuple<int, int>
{
    auto w = 1000;
    auto h = 1000;
    return {w, h};
}

void Application::AppWindow::SetSize(int w, int h)
{
}

auto Application::AppWindow::GetPosition() const -> tuple<int, int>
{
    auto x = 0;
    auto y = 0;
    return {x, y};
}

void Application::AppWindow::SetPosition(int x, int y)
{
}

auto Application::AppWindow::GetMousePosition() const -> tuple<int, int>
{
    auto x = 100;
    auto y = 100;
    return {x, y};
}

void Application::AppWindow::SetMousePosition(int x, int y)
{
}

auto Application::AppWindow::IsFocused() const -> bool
{
    return true;
}

void Application::AppWindow::Minimize()
{
}

auto Application::AppWindow::IsFullscreen() const -> bool
{
    return false;
}

auto Application::AppWindow::ToggleFullscreen(bool enable) -> bool
{
    NON_CONST_METHOD_HINT();

    return false;
}

void Application::AppWindow::Blink()
{
}

void Application::AppWindow::AlwaysOnTop(bool enable)
{
}

auto Application::AppRender::CreateTexture(uint width, uint height, bool linear_filtered, bool with_depth) -> RenderTexture*
{
    NON_CONST_METHOD_HINT();

    return nullptr;
}

auto Application::AppRender::GetTexturePixel(RenderTexture* tex, int x, int y) const -> uint
{
    return 0;
}

auto Application::AppRender::GetTextureRegion(RenderTexture* tex, int x, int y, uint w, uint h) const -> vector<uint>
{
    RUNTIME_ASSERT(w && h);
    const auto size = w * h;
    vector<uint> result(size);

    return result;
}

void Application::AppRender::UpdateTextureRegion(RenderTexture* tex, const IRect& r, const uint* data)
{
}

void Application::AppRender::SetRenderTarget(RenderTexture* tex)
{
}

auto Application::AppRender::GetRenderTarget() -> RenderTexture*
{
    return nullptr;
}

void Application::AppRender::ClearRenderTarget(uint color)
{
}

void Application::AppRender::ClearRenderTargetDepth()
{
}

void Application::AppRender::EnableScissor(int x, int y, uint w, uint h)
{
}

void Application::AppRender::DisableScissor()
{
}

auto Application::AppRender::CreateEffect(string_view /*name*/, string_view /*defines*/, const RenderEffectLoader & /*file_loader*/) -> RenderEffect*
{
    return nullptr;
}

void Application::AppRender::DrawQuads(const Vertex2DVec& vbuf, const vector<ushort>& ibuf, uint pos, RenderEffect* effect, RenderTexture* tex)
{
}

void Application::AppRender::DrawPrimitive(const Vertex2DVec& /*vbuf*/, const vector<ushort>& /*ibuf*/, RenderEffect* /*effect*/, RenderPrimitiveType /*prim*/)
{
}

void Application::AppRender::DrawMesh(RenderMesh* mesh, RenderEffect* /*effect*/)
{
}

auto Application::AppInput::PollEvent(InputEvent& event) -> bool
{
    return false;
}

void Application::AppInput::PushEvent(const InputEvent& event)
{
}

void Application::AppInput::SetClipboardText(string_view text)
{
}

auto Application::AppInput::GetClipboardText() -> string
{
    return string();
}

auto Application::AppAudio::IsEnabled() -> bool
{
    return false;
}

auto Application::AppAudio::GetStreamSize() -> uint
{
    RUNTIME_ASSERT(IsEnabled());

    return 0u;
}

auto Application::AppAudio::GetSilence() -> uchar
{
    RUNTIME_ASSERT(IsEnabled());

    return 0u;
}

void Application::AppAudio::SetSource(AudioStreamCallback stream_callback)
{
    RUNTIME_ASSERT(IsEnabled());
}

auto Application::AppAudio::ConvertAudio(int format, int channels, int rate, vector<uchar>& buf) -> bool
{
    RUNTIME_ASSERT(IsEnabled());

    return true;
}

void Application::AppAudio::MixAudio(uchar* output, uchar* buf, int volume)
{
    RUNTIME_ASSERT(IsEnabled());
}

void Application::AppAudio::LockDevice()
{
    RUNTIME_ASSERT(IsEnabled());
}

void Application::AppAudio::UnlockDevice()
{
    RUNTIME_ASSERT(IsEnabled());
}

void MessageBox::ShowErrorMessage(string_view title, string_view message, string_view traceback)
{
}
