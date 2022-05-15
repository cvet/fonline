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

#include "Rendering.h"

#if FO_HAVE_DIRECT_3D

#include "Application.h"
#include "Log.h"
#include "StringUtils.h"
#include "WinApi-Include.h"

#include <d3d11_1.h>

class Direct3D_Texture : public RenderTexture
{
public:
    Direct3D_Texture(uint width, uint height, bool linear_filtered, bool with_depth) : RenderTexture(width, height, linear_filtered, with_depth) { }
    ~Direct3D_Texture() override;
    [[nodiscard]] auto GetTexturePixel(int x, int y) -> uint override;
    [[nodiscard]] auto GetTextureRegion(int x, int y, uint w, uint h) -> vector<uint> override;
    void UpdateTextureRegion(const IRect& r, const uint* data) override;

    ID3D11Texture2D* TexHandle {};
    ID3D11Texture2D* DepthStencilHandle {};
};

class Direct3D_DrawBuffer : public RenderDrawBuffer
{
public:
    explicit Direct3D_DrawBuffer(bool is_static) : RenderDrawBuffer(is_static) { }
    ~Direct3D_DrawBuffer() override;
};

class Direct3D_Effect : public RenderEffect
{
    friend class Direct3D_Renderer;

public:
    Direct3D_Effect(EffectUsage usage, string_view name, string_view defines, const RenderEffectLoader& loader) : RenderEffect(usage, name, defines, loader) { }
    ~Direct3D_Effect() override;
    void DrawBuffer(RenderDrawBuffer* dbuf, size_t start_index = 0, optional<size_t> indices_to_draw = std::nullopt, RenderTexture* custom_tex = nullptr) override;
};

static bool RenderDebug {};
static SDL_Window* SdlWindow {};
static ID3D11Device* D3DDevice {};
static ID3D11DeviceContext* D3DDeviceContext {};
static IDXGISwapChain* SwapChain {};
static ID3D11RenderTargetView* MainRenderTarget {};
static bool VSync {};

void Direct3D_Renderer::Init(GlobalSettings& settings, SDL_Window* window)
{
    RenderDebug = settings.RenderDebug;
    SdlWindow = window;

    SDL_SysWMinfo wminfo = {};
    SDL_VERSION(&wminfo.version)
    SDL_GetWindowWMInfo(window, &wminfo);
#if !FO_UWP
    auto* hwnd = wminfo.info.win.window;
#else
    HWND hwnd = 0;
#endif

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT device_flags = 0;
    if (RenderDebug) {
        device_flags |= D3D11_CREATE_DEVICE_DEBUG;
    }

    D3D_FEATURE_LEVEL feature_level;
    constexpr D3D_FEATURE_LEVEL feature_levels[7] = {
        D3D_FEATURE_LEVEL_11_1,
#if !FO_UWP
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1,
#endif
    };
#if !FO_UWP
    constexpr UINT feature_levels_count = 7;
#else
    constexpr UINT feature_levels_count = 1;
#endif

#if !FO_UWP
    const auto d3d_create_device = ::D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, device_flags, feature_levels, feature_levels_count, D3D11_SDK_VERSION, &sd, &SwapChain, &D3DDevice, &feature_level, &D3DDeviceContext);
    if (d3d_create_device != S_OK) {
        throw AppInitException("D3D11CreateDeviceAndSwapChain failed", d3d_create_device);
    }
#else
    const auto d3d_create_device = ::D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, device_flags, feature_levels, 1, D3D11_SDK_VERSION, &D3DDevice, &feature_level, &D3DDeviceContext);
    if (d3d_create_device != S_OK)
        throw AppInitException("D3D11CreateDevice failed", d3d_create_device);
#endif

    const map<D3D_FEATURE_LEVEL, string> feature_levels_str = {
        {D3D_FEATURE_LEVEL_11_1, "11.1"},
#if !FO_UWP
        {D3D_FEATURE_LEVEL_11_0, "11.0"},
        {D3D_FEATURE_LEVEL_10_1, "10.1"},
        {D3D_FEATURE_LEVEL_10_0, "10.0"},
        {D3D_FEATURE_LEVEL_9_3, "9.3"},
        {D3D_FEATURE_LEVEL_9_2, "9.2"},
        {D3D_FEATURE_LEVEL_9_1, "9.1"},
#endif
    };
    WriteLog("Direct3D device created with feature level {}", feature_levels_str.at(feature_level));

    ID3D11Texture2D* back_buf = nullptr;
    SwapChain->GetBuffer(0, IID_PPV_ARGS(&back_buf));
    RUNTIME_ASSERT(back_buf);
    D3DDevice->CreateRenderTargetView(back_buf, nullptr, &MainRenderTarget);
    back_buf->Release();
}

void Direct3D_Renderer::Present()
{
    const auto swap_chain = SwapChain->Present(VSync ? 1 : 0, 0);
    RUNTIME_ASSERT(swap_chain == S_OK);

    D3DDeviceContext->OMSetRenderTargets(1, &MainRenderTarget, nullptr);
    constexpr float color[4] {0, 0, 0, 1};
    D3DDeviceContext->ClearRenderTargetView(MainRenderTarget, color);
}

auto Direct3D_Renderer::CreateTexture(uint width, uint height, bool linear_filtered, bool with_depth) -> RenderTexture*
{
    auto&& d3d_tex = std::make_unique<Direct3D_Texture>(width, height, linear_filtered, with_depth);

    D3D11_TEXTURE2D_DESC desc;
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;

    const auto create_texure_2d = D3DDevice->CreateTexture2D(&desc, nullptr, &d3d_tex->TexHandle);
    RUNTIME_ASSERT(create_texure_2d == S_OK);

    if (with_depth) {
        desc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        desc.CPUAccessFlags = 0;
        const auto create_texure_depth = D3DDevice->CreateTexture2D(&desc, nullptr, &d3d_tex->DepthStencilHandle);
        RUNTIME_ASSERT(create_texure_depth == S_OK);
    }

    return d3d_tex.release();
}

auto Direct3D_Renderer::CreateDrawBuffer(bool is_static) -> RenderDrawBuffer*
{
    auto&& d3d_dbuf = std::make_unique<Direct3D_DrawBuffer>(is_static);

    return d3d_dbuf.release();
}

auto Direct3D_Renderer::CreateEffect(EffectUsage usage, string_view name, string_view defines, const RenderEffectLoader& loader) -> RenderEffect*
{
    auto&& d3d_effect = std::make_unique<Direct3D_Effect>(usage, name, defines, loader);

    for (size_t pass = 0; pass < d3d_effect->_passCount; pass++) {
        string ext = "glsl";
        if constexpr (FO_OPENGL_ES) {
            ext = "glsl-es";
        }

        const string vert_fname = _str("{}.{}.vert.glsl", name, pass + 1, ext);
        string vert_content = loader(vert_fname);
        RUNTIME_ASSERT(!vert_content.empty());
        const string frag_fname = _str("{}.{}.frag.glsl", name, pass + 1, ext);
        string frag_content = loader(frag_fname);
        RUNTIME_ASSERT(!frag_content.empty());
    }

    return d3d_effect.release();
}

void Direct3D_Renderer::SetRenderTarget(RenderTexture* tex)
{
}

void Direct3D_Renderer::ClearRenderTarget(optional<uint> color, bool depth, bool stencil)
{
}

void Direct3D_Renderer::EnableScissor(int x, int y, uint w, uint h)
{
}

void Direct3D_Renderer::DisableScissor()
{
}

Direct3D_Texture::~Direct3D_Texture()
{
    TexHandle->Release();
    if (DepthStencilHandle != nullptr) {
        DepthStencilHandle->Release();
    }
}

auto Direct3D_Texture::GetTexturePixel(int x, int y) -> uint
{
    // ...
    return 0;
}

auto Direct3D_Texture::GetTextureRegion(int x, int y, uint w, uint h) -> vector<uint>
{
    RUNTIME_ASSERT(w && h);
    const auto size = w * h;
    vector<uint> result(size);

    return result;
}

void Direct3D_Texture::UpdateTextureRegion(const IRect& r, const uint* data)
{
    // D3DDeviceContext->UpdateSubresource()
}

Direct3D_DrawBuffer::~Direct3D_DrawBuffer()
{
}

Direct3D_Effect::~Direct3D_Effect()
{
}

void Direct3D_Effect::DrawBuffer(RenderDrawBuffer* dbuf, size_t start_index, optional<size_t> indices_to_draw, RenderTexture* custom_tex)
{
}

#endif
