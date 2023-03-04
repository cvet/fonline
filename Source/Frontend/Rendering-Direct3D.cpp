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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "GenericUtils.h"
#include "Log.h"
#include "StringUtils.h"
#include "WinApi-Include.h"

#include <d3d11_1.h>
#include <d3dcompiler.h>

class Direct3D_Texture : public RenderTexture
{
public:
    Direct3D_Texture(int width, int height, bool linear_filtered, bool with_depth) :
        RenderTexture(width, height, linear_filtered, with_depth, false)
    {
    }
    ~Direct3D_Texture() override;

    [[nodiscard]] auto GetTexturePixel(int x, int y) -> uint override;
    [[nodiscard]] auto GetTextureRegion(int x, int y, int width, int height) -> vector<uint> override;
    void UpdateTextureRegion(const IRect& r, const uint* data) override;

    ID3D11Texture2D* TexHandle {};
    ID3D11Texture2D* DepthStencil {};
    ID3D11RenderTargetView* RenderTargetView {};
    ID3D11DepthStencilView* DepthStencilView {};
    ID3D11ShaderResourceView* ShaderTexView {};
};

class Direct3D_DrawBuffer : public RenderDrawBuffer
{
public:
    explicit Direct3D_DrawBuffer(bool is_static) :
        RenderDrawBuffer(is_static)
    {
    }
    ~Direct3D_DrawBuffer() override;

    void Upload(EffectUsage usage, size_t custom_vertices_size, size_t custom_indices_size) override;

    ID3D11Buffer* VertexBuf {};
    ID3D11Buffer* IndexBuf {};
    size_t VertexBufSize {};
    size_t IndexBufSize {};
};

class Direct3D_Effect : public RenderEffect
{
    friend class Direct3D_Renderer;

public:
    Direct3D_Effect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) :
        RenderEffect(usage, name, loader)
    {
    }
    ~Direct3D_Effect() override;

    void DrawBuffer(RenderDrawBuffer* dbuf, size_t start_index, size_t indices_to_draw, RenderTexture* custom_tex) override;

    ID3D11VertexShader* VertexShader[EFFECT_MAX_PASSES] {};
    ID3D11InputLayout* InputLayout[EFFECT_MAX_PASSES] {};
    ID3D11PixelShader* PixelShader[EFFECT_MAX_PASSES] {};
    ID3D11RasterizerState* RasterizerState[EFFECT_MAX_PASSES] {};
    ID3D11BlendState* BlendState[EFFECT_MAX_PASSES] {};
    ID3D11DepthStencilState* DepthStencilState[EFFECT_MAX_PASSES] {};
#if FO_ENABLE_3D
    ID3D11RasterizerState* RasterizerState_Culling[EFFECT_MAX_PASSES] {};
#endif

    ID3D11Buffer* Cb_ProjBuf {};
    ID3D11Buffer* Cb_MainTexBuf {};
    ID3D11Buffer* Cb_ContourBuf {};
    ID3D11Buffer* Cb_TimeBuf {};
    ID3D11Buffer* Cb_RandomValueBuf {};
    ID3D11Buffer* Cb_ScriptValueBuf {};
#if FO_ENABLE_3D
    ID3D11Buffer* Cb_ModelBuf {};
    ID3D11Buffer* Cb_ModelTexBuf {};
    ID3D11Buffer* Cb_ModelAnimBuf {};
#endif
};

static GlobalSettings* Settings {};
static bool RenderDebug {};
static bool VSync {};
static SDL_Window* SdlWindow {};
static D3D_FEATURE_LEVEL FeatureLevel {};
static IDXGISwapChain* SwapChain {};
static ID3D11Device* D3DDevice {};
static ID3D11DeviceContext* D3DDeviceContext {};
static ID3D11DeviceContext1* D3DDeviceContext1 {};
static ID3D11RenderTargetView* MainRenderTarget {};
static ID3D11RenderTargetView* CurRenderTarget {};
static ID3D11DepthStencilView* CurDepthStencil {};
static ID3D11SamplerState* PointSampler {};
static ID3D11SamplerState* LinearSampler {};
static ID3D11Texture2D* OnePixStagingTex {};
static RenderTexture* DummyTexture {};
static mat44 ProjectionMatrixColMaj {};
static bool ScissorEnabled {};
static D3D11_RECT ScissorRect {};
static D3D11_RECT DisabledScissorRect {};
static D3D11_VIEWPORT ViewPort {};
static IRect ViewPortRect {};
static int BackBufWidth {};
static int BackBufHeight {};
static int TargetWidth {};
static int TargetHeight {};

static auto ConvertBlend(BlendFuncType blend, bool is_alpha) -> D3D11_BLEND
{
    STACK_TRACE_ENTRY();

    switch (blend) {
    case BlendFuncType::Zero:
        return D3D11_BLEND_ZERO;
    case BlendFuncType::One:
        return D3D11_BLEND_ONE;
    case BlendFuncType::SrcColor:
        return !is_alpha ? D3D11_BLEND_SRC_COLOR : D3D11_BLEND_SRC_ALPHA;
    case BlendFuncType::InvSrcColor:
        return !is_alpha ? D3D11_BLEND_INV_SRC_COLOR : D3D11_BLEND_INV_SRC_ALPHA;
    case BlendFuncType::DstColor:
        return !is_alpha ? D3D11_BLEND_DEST_COLOR : D3D11_BLEND_DEST_ALPHA;
    case BlendFuncType::InvDstColor:
        return !is_alpha ? D3D11_BLEND_INV_DEST_COLOR : D3D11_BLEND_INV_DEST_ALPHA;
    case BlendFuncType::SrcAlpha:
        return D3D11_BLEND_SRC_ALPHA;
    case BlendFuncType::InvSrcAlpha:
        return D3D11_BLEND_INV_SRC_ALPHA;
    case BlendFuncType::DstAlpha:
        return D3D11_BLEND_DEST_ALPHA;
    case BlendFuncType::InvDstAlpha:
        return D3D11_BLEND_INV_DEST_ALPHA;
    case BlendFuncType::ConstantColor:
        return D3D11_BLEND_BLEND_FACTOR;
    case BlendFuncType::InvConstantColor:
        return D3D11_BLEND_INV_BLEND_FACTOR;
    case BlendFuncType::SrcAlphaSaturate:
        return D3D11_BLEND_SRC_ALPHA_SAT;
    }
    throw UnreachablePlaceException(LINE_STR);
}

static auto ConvertBlendOp(BlendEquationType blend_op) -> D3D11_BLEND_OP
{
    STACK_TRACE_ENTRY();

    switch (blend_op) {
    case BlendEquationType::FuncAdd:
        return D3D11_BLEND_OP_ADD;
    case BlendEquationType::FuncSubtract:
        return D3D11_BLEND_OP_SUBTRACT;
    case BlendEquationType::FuncReverseSubtract:
        return D3D11_BLEND_OP_REV_SUBTRACT;
    case BlendEquationType::Max:
        return D3D11_BLEND_OP_MAX;
    case BlendEquationType::Min:
        return D3D11_BLEND_OP_MIN;
    }
    throw UnreachablePlaceException(LINE_STR);
}

void Direct3D_Renderer::Init(GlobalSettings& settings, WindowInternalHandle* window)
{
    STACK_TRACE_ENTRY();

    WriteLog("Used DirectX rendering");

    Settings = &settings;
    RenderDebug = settings.RenderDebug;
    VSync = settings.ClientMode && settings.VSync;
    SdlWindow = static_cast<SDL_Window*>(window);

    SDL_SysWMinfo wminfo = {};
    SDL_VERSION(&wminfo.version)
    SDL_GetWindowWMInfo(SdlWindow, &wminfo);
#if !FO_UWP
    auto* hwnd = wminfo.info.win.window;
#else
    HWND hwnd = 0;
#endif

    // Device
    {
        constexpr D3D_FEATURE_LEVEL feature_levels[] = {
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
        constexpr auto feature_levels_count = static_cast<UINT>(std::size(feature_levels));

        UINT device_flags = D3D11_CREATE_DEVICE_SINGLETHREADED;
        if (RenderDebug) {
            device_flags |= D3D11_CREATE_DEVICE_DEBUG;
        }

        const auto d3d_hardware_create_device = ::D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, device_flags, feature_levels, feature_levels_count, D3D11_SDK_VERSION, &D3DDevice, &FeatureLevel, &D3DDeviceContext);
        if (!SUCCEEDED(d3d_hardware_create_device)) {
            const auto d3d_warp_create_device = ::D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, device_flags, feature_levels, feature_levels_count, D3D11_SDK_VERSION, &D3DDevice, &FeatureLevel, &D3DDeviceContext);
            if (!SUCCEEDED(d3d_warp_create_device)) {
                throw AppInitException("D3D11CreateDevice failed (Hardware and Warp)", d3d_hardware_create_device, d3d_warp_create_device);
            }

            WriteLog("Warp Direct3D device created with feature level {}", feature_levels_str.at(FeatureLevel));
        }
        else {
            WriteLog("Direct3D device created with feature level {}", feature_levels_str.at(FeatureLevel));
        }

        if (SUCCEEDED(D3DDeviceContext->QueryInterface(IID_ID3D11DeviceContext1, reinterpret_cast<void**>(&D3DDeviceContext1)))) {
            D3DDeviceContext->Release();
            D3DDeviceContext = D3DDeviceContext1;
        }
        else {
            WriteLog("Direct3D ID3D11DeviceContext1 not found");
        }
    }

    // Swap chain
    {
        IDXGIFactory* factory = nullptr;
        const auto d3d_create_factory = ::CreateDXGIFactory(IID_IDXGIFactory, reinterpret_cast<void**>(&factory));
        if (!SUCCEEDED(d3d_create_factory)) {
            throw AppInitException("CreateDXGIFactory failed", d3d_create_factory);
        }

        DXGI_SWAP_CHAIN_DESC swap_chain_desc = {};
        swap_chain_desc.BufferCount = 2;
        swap_chain_desc.BufferDesc.Width = 0;
        swap_chain_desc.BufferDesc.Height = 0;
        swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swap_chain_desc.BufferDesc.RefreshRate.Numerator = 144;
        swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
        swap_chain_desc.Flags = 0;
        swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swap_chain_desc.OutputWindow = hwnd;
        swap_chain_desc.SampleDesc.Count = 1;
        swap_chain_desc.SampleDesc.Quality = 0;
        swap_chain_desc.Windowed = TRUE;

        if (VSync) {
            swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

            const auto d3d_create_swap_chain = factory->CreateSwapChain(D3DDevice, &swap_chain_desc, &SwapChain);
            if (!SUCCEEDED(d3d_create_swap_chain)) {
                swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

                const auto d3d_create_swap_chain_2 = factory->CreateSwapChain(D3DDevice, &swap_chain_desc, &SwapChain);
                if (!SUCCEEDED(d3d_create_swap_chain_2)) {
                    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

                    const auto d3d_create_swap_chain_3 = factory->CreateSwapChain(D3DDevice, &swap_chain_desc, &SwapChain);
                    if (!SUCCEEDED(d3d_create_swap_chain_3)) {
                        swap_chain_desc.BufferCount = 1;

                        const auto d3d_create_swap_chain_4 = factory->CreateSwapChain(D3DDevice, &swap_chain_desc, &SwapChain);
                        if (!SUCCEEDED(d3d_create_swap_chain_4)) {
                            throw AppInitException("CreateSwapChain failed", d3d_create_swap_chain, d3d_create_swap_chain_2, d3d_create_swap_chain_3, d3d_create_swap_chain_4);
                        }
                        else {
                            WriteLog("Direct3D swap chain created with one buffer count");
                        }
                    }
                    else {
                        WriteLog("Direct3D swap chain created with non-flip swap effect");
                    }
                }
                else {
                    WriteLog("Direct3D swap chain created with flip sequential swap effect");
                }
            }
        }
        else {
            swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

            const auto d3d_create_swap_chain = factory->CreateSwapChain(D3DDevice, &swap_chain_desc, &SwapChain);
            if (!SUCCEEDED(d3d_create_swap_chain)) {
                swap_chain_desc.BufferCount = 1;

                const auto d3d_create_swap_chain_2 = factory->CreateSwapChain(D3DDevice, &swap_chain_desc, &SwapChain);
                if (!SUCCEEDED(d3d_create_swap_chain_2)) {
                    throw AppInitException("CreateSwapChain failed", d3d_create_swap_chain, d3d_create_swap_chain_2);
                }
                else {
                    WriteLog("Direct3D swap chain created with one buffer count");
                }
            }
        }
    }

    // Samplers
    {
        const auto create_sampler = [](D3D11_FILTER filter) {
            D3D11_SAMPLER_DESC sampler_desc = {};
            sampler_desc.Filter = filter;
            sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
            sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
            sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            sampler_desc.MaxAnisotropy = 2;
            sampler_desc.MipLODBias = 0.0f;
            sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
            sampler_desc.MinLOD = 0.0f;
            sampler_desc.MaxLOD = FLT_MAX;
            sampler_desc.BorderColor[0] = 0.0f;
            sampler_desc.BorderColor[1] = 0.0f;
            sampler_desc.BorderColor[2] = 0.0f;
            sampler_desc.BorderColor[3] = 0.0f;

            ID3D11SamplerState* sampler = nullptr;
            const auto d3d_create_sampler = D3DDevice->CreateSamplerState(&sampler_desc, &sampler);
            if (!SUCCEEDED(d3d_create_sampler)) {
                throw EffectLoadException("Failed to create sampler", d3d_create_sampler);
            }

            return sampler;
        };

        PointSampler = create_sampler(D3D11_FILTER_MIN_MAG_MIP_POINT);
        LinearSampler = create_sampler(D3D11_FILTER_MIN_MAG_MIP_LINEAR);
    }

    // Calculate atlas size
    int atlas_w;
    int atlas_h;
    if (FeatureLevel >= D3D_FEATURE_LEVEL_11_0) {
        atlas_w = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
        atlas_h = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    }
    else if (FeatureLevel >= D3D_FEATURE_LEVEL_10_0) {
        atlas_w = D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION;
        atlas_h = D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    }
    else if (FeatureLevel >= D3D_FEATURE_LEVEL_9_3) {
        atlas_w = D3D_FL9_3_REQ_TEXTURE2D_U_OR_V_DIMENSION;
        atlas_h = D3D_FL9_3_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    }
    else if (FeatureLevel >= D3D_FEATURE_LEVEL_9_1) {
        atlas_w = D3D_FL9_1_REQ_TEXTURE2D_U_OR_V_DIMENSION;
        atlas_h = D3D_FL9_1_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    }
    else {
        throw UnreachablePlaceException(LINE_STR);
    }
    RUNTIME_ASSERT_STR(atlas_w >= AppRender::MIN_ATLAS_SIZE, _str("Min texture width must be at least {}", AppRender::MIN_ATLAS_SIZE));
    RUNTIME_ASSERT_STR(atlas_h >= AppRender::MIN_ATLAS_SIZE, _str("Min texture height must be at least {}", AppRender::MIN_ATLAS_SIZE));

    const_cast<int&>(AppRender::MAX_ATLAS_WIDTH) = atlas_w;
    const_cast<int&>(AppRender::MAX_ATLAS_HEIGHT) = atlas_h;

    // Back buffer view
    ID3D11Texture2D* back_buf = nullptr;
    const auto d3d_get_back_buf = SwapChain->GetBuffer(0, IID_PPV_ARGS(&back_buf));
    RUNTIME_ASSERT(SUCCEEDED(d3d_get_back_buf));
    RUNTIME_ASSERT(back_buf);
    const auto d3d_create_back_buf_rt_view = D3DDevice->CreateRenderTargetView(back_buf, nullptr, &MainRenderTarget);
    RUNTIME_ASSERT(SUCCEEDED(d3d_create_back_buf_rt_view));
    back_buf->Release();

    BackBufWidth = settings.ScreenWidth;
    BackBufHeight = settings.ScreenHeight;

    // One pixel staging texture
    D3D11_TEXTURE2D_DESC one_pix_staging_desc;
    one_pix_staging_desc.Width = 1;
    one_pix_staging_desc.Height = 1;
    one_pix_staging_desc.MipLevels = 1;
    one_pix_staging_desc.ArraySize = 1;
    one_pix_staging_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    one_pix_staging_desc.SampleDesc.Count = 1;
    one_pix_staging_desc.SampleDesc.Quality = 0;
    one_pix_staging_desc.Usage = D3D11_USAGE_STAGING;
    one_pix_staging_desc.BindFlags = 0;
    one_pix_staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    one_pix_staging_desc.MiscFlags = 0;

    const auto d3d_create_one_pix_staging_tex = D3DDevice->CreateTexture2D(&one_pix_staging_desc, nullptr, &OnePixStagingTex);
    RUNTIME_ASSERT(SUCCEEDED(d3d_create_one_pix_staging_tex));

    // Dummy texture
    constexpr uint dummy_pixel[1] = {0xFFFF00FF};
    DummyTexture = CreateTexture(1, 1, false, false);
    DummyTexture->UpdateTextureRegion({0, 0, 1, 1}, dummy_pixel);

    // Init render target
    SetRenderTarget(nullptr);
    DisableScissor();
}

void Direct3D_Renderer::Present()
{
    STACK_TRACE_ENTRY();

    const auto d3d_swap_chain = SwapChain->Present(VSync ? 1 : 0, 0);
    RUNTIME_ASSERT(SUCCEEDED(d3d_swap_chain));

    if (D3DDeviceContext1 != nullptr) {
        D3DDeviceContext1->DiscardView(CurRenderTarget);
    }

    D3DDeviceContext->OMSetRenderTargets(1, &CurRenderTarget, CurDepthStencil);
}

auto Direct3D_Renderer::CreateTexture(int width, int height, bool linear_filtered, bool with_depth) -> RenderTexture*
{
    STACK_TRACE_ENTRY();

    auto&& d3d_tex = std::make_unique<Direct3D_Texture>(width, height, linear_filtered, with_depth);

    D3D11_TEXTURE2D_DESC tex_desc = {};
    tex_desc.Width = width;
    tex_desc.Height = height;
    tex_desc.MipLevels = 1;
    tex_desc.ArraySize = 1;
    tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    tex_desc.SampleDesc.Count = 1;
    tex_desc.SampleDesc.Quality = 0;
    tex_desc.Usage = D3D11_USAGE_DEFAULT;
    tex_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    tex_desc.CPUAccessFlags = 0;
    tex_desc.MiscFlags = 0;

    const auto d3d_create_texure_2d = D3DDevice->CreateTexture2D(&tex_desc, nullptr, &d3d_tex->TexHandle);
    RUNTIME_ASSERT(SUCCEEDED(d3d_create_texure_2d));

    if (with_depth) {
        D3D11_TEXTURE2D_DESC depth_tex_desc = {};
        depth_tex_desc.Width = width;
        depth_tex_desc.Height = height;
        depth_tex_desc.MipLevels = 1;
        depth_tex_desc.ArraySize = 1;
        depth_tex_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depth_tex_desc.SampleDesc.Count = 1;
        depth_tex_desc.SampleDesc.Quality = 0;
        depth_tex_desc.Usage = D3D11_USAGE_DEFAULT;
        depth_tex_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        depth_tex_desc.CPUAccessFlags = 0;
        depth_tex_desc.MiscFlags = 0;

        const auto d3d_create_texure_depth = D3DDevice->CreateTexture2D(&depth_tex_desc, nullptr, &d3d_tex->DepthStencil);
        RUNTIME_ASSERT(SUCCEEDED(d3d_create_texure_depth));

        D3D11_DEPTH_STENCIL_VIEW_DESC depth_view_desc = {};
        depth_view_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depth_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        depth_view_desc.Texture2D.MipSlice = 0;

        const auto d3d_create_depth_view = D3DDevice->CreateDepthStencilView(d3d_tex->DepthStencil, &depth_view_desc, &d3d_tex->DepthStencilView);
        RUNTIME_ASSERT(SUCCEEDED(d3d_create_depth_view));
    }

    const auto d3d_create_tex_rt_view = D3DDevice->CreateRenderTargetView(d3d_tex->TexHandle, nullptr, &d3d_tex->RenderTargetView);
    RUNTIME_ASSERT(SUCCEEDED(d3d_create_tex_rt_view));

    D3D11_SHADER_RESOURCE_VIEW_DESC tex_view_desc = {};
    tex_view_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    tex_view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    tex_view_desc.Texture2D.MipLevels = 1;
    tex_view_desc.Texture2D.MostDetailedMip = 0;
    const auto d3d_create_shader_res_view = D3DDevice->CreateShaderResourceView(d3d_tex->TexHandle, &tex_view_desc, &d3d_tex->ShaderTexView);
    RUNTIME_ASSERT(SUCCEEDED(d3d_create_shader_res_view));

    return d3d_tex.release();
}

auto Direct3D_Renderer::CreateDrawBuffer(bool is_static) -> RenderDrawBuffer*
{
    STACK_TRACE_ENTRY();

    auto&& d3d_dbuf = std::make_unique<Direct3D_DrawBuffer>(is_static);

    return d3d_dbuf.release();
}

auto Direct3D_Renderer::CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> RenderEffect*
{
    STACK_TRACE_ENTRY();

    auto&& d3d_effect = std::make_unique<Direct3D_Effect>(usage, name, loader);

    for (size_t pass = 0; pass < d3d_effect->_passCount; pass++) {
        // Create the vertex shader
        {
            const string vertex_shader_fname = _str("{}.{}.vert.hlsl", _str(name).eraseFileExtension(), pass + 1);
            string vertex_shader_content = loader(vertex_shader_fname);
            RUNTIME_ASSERT(!vertex_shader_content.empty());

            ID3DBlob* vertex_shader_blob = nullptr;
            ScopeCallback vertex_shader_blob_release {[&vertex_shader_blob]() noexcept {
                if (vertex_shader_blob != nullptr) {
                    vertex_shader_blob->Release();
                }
            }};

            ID3DBlob* error_blob = nullptr;
            ScopeCallback error_blob_release {[&error_blob]() noexcept {
                if (error_blob != nullptr) {
                    error_blob->Release();
                }
            }};

            if (FAILED(::D3DCompile(vertex_shader_content.c_str(), vertex_shader_content.length(), nullptr, nullptr, nullptr, "main", "vs_4_0_level_9_1", 0, 0, &vertex_shader_blob, &error_blob))) {
                const string error = static_cast<const char*>(error_blob->GetBufferPointer());
                throw EffectLoadException("Failed to compile Vertex Shader", vertex_shader_fname, vertex_shader_content, error);
            }

            const auto d3d_create_vertex_shader = D3DDevice->CreateVertexShader(vertex_shader_blob->GetBufferPointer(), vertex_shader_blob->GetBufferSize(), nullptr, &d3d_effect->VertexShader[pass]);
            if (!SUCCEEDED(d3d_create_vertex_shader)) {
                throw EffectLoadException("Failed to create Vertex Shader from binary", d3d_create_vertex_shader, vertex_shader_fname, vertex_shader_content);
            }

            // Create the input layout
#if FO_ENABLE_3D
            if (usage == EffectUsage::Model) {
                static_assert(BONES_PER_VERTEX == 4);
                const D3D11_INPUT_ELEMENT_DESC local_layout[] = {
                    {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(Vertex3D, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(Vertex3D, Normal)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(Vertex3D, TexCoord)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD", 3, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(Vertex3D, TexCoordBase)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD", 4, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(Vertex3D, Tangent)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD", 5, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(Vertex3D, Bitangent)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD", 6, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(Vertex3D, BlendWeights)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD", 7, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(Vertex3D, BlendIndices)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD", 8, DXGI_FORMAT_R8G8B8A8_UNORM, 0, static_cast<UINT>(offsetof(Vertex3D, Color)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                };

                const auto d3d_create_input_layout = D3DDevice->CreateInputLayout(local_layout, 9, vertex_shader_blob->GetBufferPointer(), vertex_shader_blob->GetBufferSize(), &d3d_effect->InputLayout[pass]);
                if (!SUCCEEDED(d3d_create_input_layout)) {
                    throw EffectLoadException("Failed to create Vertex Shader 3D layout", d3d_create_input_layout, vertex_shader_fname, vertex_shader_content);
                }
            }
            else
#endif
            {
                const D3D11_INPUT_ELEMENT_DESC local_layout[] = {
                    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(Vertex2D, PosX)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD", 1, DXGI_FORMAT_R8G8B8A8_UNORM, 0, static_cast<UINT>(offsetof(Vertex2D, Color)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(Vertex2D, TexU)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD", 3, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(Vertex2D, EggTexU)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                };

                const auto d3d_create_input_layout = D3DDevice->CreateInputLayout(local_layout, 4, vertex_shader_blob->GetBufferPointer(), vertex_shader_blob->GetBufferSize(), &d3d_effect->InputLayout[pass]);
                if (!SUCCEEDED(d3d_create_input_layout)) {
                    throw EffectLoadException("Failed to create Vertex Shader 2D layout", d3d_create_input_layout, vertex_shader_fname, vertex_shader_content);
                }
            }
        }

        // Create the pixel shader
        {
            const string pixel_shader_fname = _str("{}.{}.frag.hlsl", _str(name).eraseFileExtension(), pass + 1);
            string pixel_shader_content = loader(pixel_shader_fname);
            RUNTIME_ASSERT(!pixel_shader_content.empty());

            ID3DBlob* pixel_shader_blob = nullptr;
            ScopeCallback vertex_shader_blob_release {[&pixel_shader_blob]() noexcept {
                if (pixel_shader_blob != nullptr) {
                    pixel_shader_blob->Release();
                }
            }};

            ID3DBlob* error_blob = nullptr;
            ScopeCallback error_blob_release {[&error_blob]() noexcept {
                if (error_blob != nullptr) {
                    error_blob->Release();
                }
            }};

            if (FAILED(::D3DCompile(pixel_shader_content.c_str(), pixel_shader_content.length(), nullptr, nullptr, nullptr, "main", "ps_4_0_level_9_1", 0, 0, &pixel_shader_blob, &error_blob))) {
                const string error = static_cast<const char*>(error_blob->GetBufferPointer());
                throw EffectLoadException("Failed to compile Pixel Shader", pixel_shader_fname, pixel_shader_content, error);
            }

            const auto d3d_create_pixel_shader = D3DDevice->CreatePixelShader(pixel_shader_blob->GetBufferPointer(), pixel_shader_blob->GetBufferSize(), nullptr, &d3d_effect->PixelShader[pass]);
            if (!SUCCEEDED(d3d_create_pixel_shader)) {
                throw EffectLoadException("Failed to create Pixel Shader from binary", d3d_create_pixel_shader, pixel_shader_fname, pixel_shader_content);
            }
        }

        // Create the blending setup
        {
            D3D11_BLEND_DESC blend_desc = {};
            blend_desc.RenderTarget[0].BlendEnable = TRUE;
            blend_desc.RenderTarget[0].SrcBlend = ConvertBlend(d3d_effect->_srcBlendFunc[pass], false);
            blend_desc.RenderTarget[0].DestBlend = ConvertBlend(d3d_effect->_destBlendFunc[pass], false);
            blend_desc.RenderTarget[0].BlendOp = ConvertBlendOp(d3d_effect->_blendEquation[pass]);
            blend_desc.RenderTarget[0].SrcBlendAlpha = ConvertBlend(d3d_effect->_srcBlendFunc[pass], true);
            blend_desc.RenderTarget[0].DestBlendAlpha = ConvertBlend(d3d_effect->_destBlendFunc[pass], true);
            blend_desc.RenderTarget[0].BlendOpAlpha = ConvertBlendOp(d3d_effect->_blendEquation[pass]);
            blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

            const auto d3d_create_blend_state = D3DDevice->CreateBlendState(&blend_desc, &d3d_effect->BlendState[pass]);
            if (!SUCCEEDED(d3d_create_blend_state)) {
                throw EffectLoadException("Failed to call CreateBlendState", d3d_create_blend_state, name);
            }
        }

        // Create the rasterizer state
        {
            D3D11_RASTERIZER_DESC rasterizer_desc = {};
            rasterizer_desc.FillMode = D3D11_FILL_SOLID;
            rasterizer_desc.CullMode = D3D11_CULL_NONE;
            rasterizer_desc.DepthClipEnable = TRUE;
            rasterizer_desc.ScissorEnable = TRUE;
            rasterizer_desc.DepthBiasClamp = 0;

            const auto d3d_create_rasterized_state = D3DDevice->CreateRasterizerState(&rasterizer_desc, &d3d_effect->RasterizerState[pass]);
            if (!SUCCEEDED(d3d_create_rasterized_state)) {
                throw EffectLoadException("Failed to call CreateRasterizerState", d3d_create_rasterized_state, name);
            }

#if FO_ENABLE_3D
            D3D11_RASTERIZER_DESC rasterizer_culling_desc = rasterizer_desc;
            rasterizer_desc.CullMode = D3D11_CULL_BACK;
            rasterizer_desc.FrontCounterClockwise = TRUE;

            const auto d3d_create_rasterized_state_culling = D3DDevice->CreateRasterizerState(&rasterizer_culling_desc, &d3d_effect->RasterizerState_Culling[pass]);
            if (!SUCCEEDED(d3d_create_rasterized_state_culling)) {
                throw EffectLoadException("Failed to call CreateRasterizerState", d3d_create_rasterized_state_culling, name);
            }
#endif
        }

        // Create depth-stencil state
        {
            D3D11_DEPTH_STENCIL_DESC depth_stencil_desc = {};

#if FO_ENABLE_3D
            if (usage == EffectUsage::Model) {
                depth_stencil_desc.DepthEnable = TRUE;
                depth_stencil_desc.DepthWriteMask = d3d_effect->_depthWrite[pass] ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
                depth_stencil_desc.DepthFunc = D3D11_COMPARISON_LESS;
            }
#endif

            const auto d3d_create_depth_stencil_state = D3DDevice->CreateDepthStencilState(&depth_stencil_desc, &d3d_effect->DepthStencilState[pass]);
            if (!SUCCEEDED(d3d_create_depth_stencil_state)) {
                throw EffectLoadException("Failed to call CreateDepthStencilState", d3d_create_depth_stencil_state, name);
            }
        }
    }

    return d3d_effect.release();
}

auto Direct3D_Renderer::CreateOrthoMatrix(float left, float right, float bottom, float top, float nearp, float farp) -> mat44
{
    STACK_TRACE_ENTRY();

    const auto& l = left;
    const auto& t = top;
    const auto& r = right;
    const auto& b = bottom;
    const auto& zn = nearp;
    const auto& zf = farp;

    mat44 result;

    result.a1 = 2.0f / (r - l);
    result.a2 = 0.0f;
    result.a3 = 0.0f;
    result.a4 = (l + r) / (l - r);

    result.b1 = 0.0f;
    result.b2 = 2.0f / (t - b);
    result.b3 = 0.0f;
    result.b4 = (t + b) / (b - t);

    result.c1 = 0.0f;
    result.c2 = 0.0f;
    result.c3 = 1.0f / (zn - zf);
    result.c4 = zn / (zn - zf);

    result.d1 = 0.0f;
    result.d2 = 0.0f;
    result.d3 = 0.0f;
    result.d4 = 1.0f;

    return result;
}

auto Direct3D_Renderer::GetViewPort() -> IRect
{
    STACK_TRACE_ENTRY();

    return ViewPortRect;
}

void Direct3D_Renderer::SetRenderTarget(RenderTexture* tex)
{
    STACK_TRACE_ENTRY();

    int vp_ox;
    int vp_oy;
    int vp_width;
    int vp_height;
    int screen_width;
    int screen_height;

    if (tex != nullptr) {
        const auto* d3d_tex = static_cast<Direct3D_Texture*>(tex);
        CurRenderTarget = d3d_tex->RenderTargetView;
        CurDepthStencil = d3d_tex->DepthStencilView;

        vp_ox = 0;
        vp_oy = 0;
        vp_width = d3d_tex->Width;
        vp_height = d3d_tex->Height;
        screen_width = vp_width;
        screen_height = vp_height;
    }
    else {
        CurRenderTarget = MainRenderTarget;
        CurDepthStencil = nullptr;

        const float back_buf_aspect = static_cast<float>(BackBufWidth) / static_cast<float>(BackBufHeight);
        const float screen_aspect = static_cast<float>(Settings->ScreenWidth) / static_cast<float>(Settings->ScreenHeight);
        const int fit_width = iround(screen_aspect <= back_buf_aspect ? static_cast<float>(BackBufHeight) * screen_aspect : static_cast<float>(BackBufHeight) * back_buf_aspect);
        const int fit_height = iround(screen_aspect <= back_buf_aspect ? static_cast<float>(BackBufWidth) / back_buf_aspect : static_cast<float>(BackBufWidth) / screen_aspect);

        vp_ox = (BackBufWidth - fit_width) / 2;
        vp_oy = (BackBufHeight - fit_height) / 2;
        vp_width = fit_width;
        vp_height = fit_height;
        screen_width = Settings->ScreenWidth;
        screen_height = Settings->ScreenHeight;
    }

    D3DDeviceContext->OMSetRenderTargets(1, &CurRenderTarget, CurDepthStencil);

    ViewPortRect = IRect {vp_ox, vp_oy, vp_ox + vp_width, vp_oy + vp_height};

    ViewPort.Width = static_cast<FLOAT>(vp_width);
    ViewPort.Height = static_cast<FLOAT>(vp_height);
    ViewPort.MinDepth = 0.0f;
    ViewPort.MaxDepth = 1.0f;
    ViewPort.TopLeftX = static_cast<FLOAT>(vp_ox);
    ViewPort.TopLeftY = static_cast<FLOAT>(vp_oy);

    D3DDeviceContext->RSSetViewports(1, &ViewPort);

    ProjectionMatrixColMaj = CreateOrthoMatrix(0.0f, static_cast<float>(screen_width), static_cast<float>(screen_height), 0.0f, -10.0f, 10.0f);
    ProjectionMatrixColMaj.Transpose(); // Convert to column major order

    DisabledScissorRect.left = vp_ox;
    DisabledScissorRect.top = vp_oy;
    DisabledScissorRect.right = vp_ox + vp_width;
    DisabledScissorRect.bottom = vp_oy + vp_height;

    TargetWidth = screen_width;
    TargetHeight = screen_height;
}

void Direct3D_Renderer::ClearRenderTarget(optional<uint> color, bool depth, bool stencil)
{
    STACK_TRACE_ENTRY();

    if (color.has_value()) {
        const auto color_value = color.value();
        const auto a = static_cast<float>((color_value >> 24) & 0xFF) / 255.0f;
        const auto r = static_cast<float>((color_value >> 16) & 0xFF) / 255.0f;
        const auto g = static_cast<float>((color_value >> 8) & 0xFF) / 255.0f;
        const auto b = static_cast<float>((color_value >> 0) & 0xFF) / 255.0f;
        const float color_rgba[] {r, g, b, a};

        D3DDeviceContext->ClearRenderTargetView(CurRenderTarget, color_rgba);
    }

    if ((depth || stencil) && CurDepthStencil != nullptr) {
        UINT clear_flags = 0;
        if (depth) {
            clear_flags |= D3D11_CLEAR_DEPTH;
        }
        if (stencil) {
            clear_flags |= D3D11_CLEAR_STENCIL;
        }

        D3DDeviceContext->ClearDepthStencilView(CurDepthStencil, clear_flags, 1.0f, 0);
    }
}

void Direct3D_Renderer::EnableScissor(int x, int y, int width, int height)
{
    STACK_TRACE_ENTRY();

    if (ViewPortRect.Width() != TargetWidth || ViewPortRect.Height() != TargetHeight) {
        const float x_ratio = static_cast<float>(ViewPortRect.Width()) / static_cast<float>(TargetWidth);
        const float y_ratio = static_cast<float>(ViewPortRect.Height()) / static_cast<float>(TargetHeight);

        ScissorRect.left = ViewPortRect.Left + iround(static_cast<float>(x) * x_ratio);
        ScissorRect.top = ViewPortRect.Top + iround(static_cast<float>(y) * y_ratio);
        ScissorRect.right = ViewPortRect.Left + iround(static_cast<float>(x + width) * x_ratio);
        ScissorRect.bottom = ViewPortRect.Top + iround(static_cast<float>(y + height) * y_ratio);
    }
    else {
        ScissorRect.left = ViewPortRect.Left + x;
        ScissorRect.top = ViewPortRect.Top + y;
        ScissorRect.right = ViewPortRect.Left + x + width;
        ScissorRect.bottom = ViewPortRect.Top + y + height;
    }

    ScissorEnabled = true;
}

void Direct3D_Renderer::DisableScissor()
{
    STACK_TRACE_ENTRY();

    ScissorEnabled = false;
}

void Direct3D_Renderer::OnResizeWindow(int width, int height)
{
    const auto is_cur_rt = CurRenderTarget == MainRenderTarget;

    if (is_cur_rt) {
        CurRenderTarget = nullptr;
        RUNTIME_ASSERT(!CurDepthStencil);
        D3DDeviceContext->OMSetRenderTargets(1, &CurRenderTarget, CurDepthStencil);
    }

    MainRenderTarget->Release();
    MainRenderTarget = nullptr;

    const auto d3d_resize_buffers = SwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    RUNTIME_ASSERT(SUCCEEDED(d3d_resize_buffers));

    ID3D11Texture2D* back_buf = nullptr;
    const auto d3d_get_back_buf = SwapChain->GetBuffer(0, IID_PPV_ARGS(&back_buf));
    RUNTIME_ASSERT(SUCCEEDED(d3d_get_back_buf));
    RUNTIME_ASSERT(back_buf);
    const auto d3d_create_back_buf_rt_view = D3DDevice->CreateRenderTargetView(back_buf, nullptr, &MainRenderTarget);
    RUNTIME_ASSERT(SUCCEEDED(d3d_create_back_buf_rt_view));
    back_buf->Release();

    BackBufWidth = width;
    BackBufHeight = height;

    if (is_cur_rt) {
        SetRenderTarget(nullptr);
    }
}

Direct3D_Texture::~Direct3D_Texture()
{
    STACK_TRACE_ENTRY();

    if (TexHandle != nullptr) {
        TexHandle->Release();
    }
    if (DepthStencil != nullptr) {
        DepthStencil->Release();
    }
    if (RenderTargetView != nullptr) {
        RenderTargetView->Release();
    }
    if (DepthStencilView != nullptr) {
        DepthStencilView->Release();
    }
    if (ShaderTexView != nullptr) {
        ShaderTexView->Release();
    }
}

auto Direct3D_Texture::GetTexturePixel(int x, int y) -> uint
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(x >= 0);
    RUNTIME_ASSERT(y >= 0);
    RUNTIME_ASSERT(x < Width);
    RUNTIME_ASSERT(y < Height);

    D3D11_BOX src_box;
    src_box.left = x;
    src_box.top = y;
    src_box.right = x + 1;
    src_box.bottom = y + 1;
    src_box.front = 0;
    src_box.back = 1;

    D3DDeviceContext->CopySubresourceRegion(OnePixStagingTex, 0, 0, 0, 0, TexHandle, 0, &src_box);

    D3D11_MAPPED_SUBRESOURCE tex_resource;
    const auto d3d_map_staging_texture = D3DDeviceContext->Map(OnePixStagingTex, 0, D3D11_MAP_READ, 0, &tex_resource);
    RUNTIME_ASSERT(SUCCEEDED(d3d_map_staging_texture));

    const auto result = *static_cast<uint*>(tex_resource.pData);

    D3DDeviceContext->Unmap(OnePixStagingTex, 0);

    return result;
}

auto Direct3D_Texture::GetTextureRegion(int x, int y, int width, int height) -> vector<uint>
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(width > 0);
    RUNTIME_ASSERT(height > 0);
    RUNTIME_ASSERT(x >= 0);
    RUNTIME_ASSERT(y >= 0);
    RUNTIME_ASSERT(x + width <= Width);
    RUNTIME_ASSERT(y + height <= Height);

    vector<uint> result;
    result.resize(width * height);

    D3D11_TEXTURE2D_DESC staging_desc;
    staging_desc.Width = width;
    staging_desc.Height = height;
    staging_desc.MipLevels = 1;
    staging_desc.ArraySize = 1;
    staging_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    staging_desc.SampleDesc.Count = 1;
    staging_desc.SampleDesc.Quality = 0;
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.BindFlags = 0;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    staging_desc.MiscFlags = 0;

    ID3D11Texture2D* staging_tex = nullptr;
    const auto d3d_create_staging_tex = D3DDevice->CreateTexture2D(&staging_desc, nullptr, &staging_tex);
    RUNTIME_ASSERT(SUCCEEDED(d3d_create_staging_tex));

    D3D11_BOX src_box;
    src_box.left = x;
    src_box.top = y;
    src_box.right = x + width;
    src_box.bottom = y + height;
    src_box.front = 0;
    src_box.back = 1;

    D3DDeviceContext->CopySubresourceRegion(staging_tex, 0, 0, 0, 0, TexHandle, 0, &src_box);

    D3D11_MAPPED_SUBRESOURCE tex_resource;
    const auto d3d_map_staging_texture = D3DDeviceContext->Map(staging_tex, 0, D3D11_MAP_READ, 0, &tex_resource);
    RUNTIME_ASSERT(SUCCEEDED(d3d_map_staging_texture));

    for (int i = 0; i < height; i++) {
        std::memcpy(&result[i * width], static_cast<uint8*>(tex_resource.pData) + tex_resource.RowPitch * i, width * 4);
    }

    D3DDeviceContext->Unmap(staging_tex, 0);
    staging_tex->Release();

    return result;
}

void Direct3D_Texture::UpdateTextureRegion(const IRect& r, const uint* data)
{
    STACK_TRACE_ENTRY();

    RUNTIME_ASSERT(r.Left >= 0);
    RUNTIME_ASSERT(r.Right >= 0);
    RUNTIME_ASSERT(r.Right <= Width);
    RUNTIME_ASSERT(r.Bottom <= Height);
    RUNTIME_ASSERT(r.Right > r.Left);
    RUNTIME_ASSERT(r.Bottom > r.Top);

    D3D11_BOX dest_box;
    dest_box.left = r.Left;
    dest_box.top = r.Top;
    dest_box.right = r.Right;
    dest_box.bottom = r.Bottom;
    dest_box.front = 0;
    dest_box.back = 1;

    D3DDeviceContext->UpdateSubresource(TexHandle, 0, &dest_box, data, 4 * r.Width(), 0);
}

Direct3D_DrawBuffer::~Direct3D_DrawBuffer()
{
    STACK_TRACE_ENTRY();

    if (VertexBuf != nullptr) {
        VertexBuf->Release();
    }
    if (IndexBuf != nullptr) {
        IndexBuf->Release();
    }
}

void Direct3D_DrawBuffer::Upload(EffectUsage usage, size_t custom_vertices_size, size_t custom_indices_size)
{
    STACK_TRACE_ENTRY();

    if (IsStatic && !StaticDataChanged) {
        return;
    }

    StaticDataChanged = false;

    // Fill vertex buffer
    size_t upload_vertices;
    size_t vert_size;

#if FO_ENABLE_3D
    if (usage == EffectUsage::Model) {
        RUNTIME_ASSERT(Vertices2D.empty());
        upload_vertices = custom_vertices_size == static_cast<size_t>(-1) ? Vertices3D.size() : custom_vertices_size;
        vert_size = sizeof(Vertex3D);
    }
    else {
        RUNTIME_ASSERT(Vertices3D.empty());
        upload_vertices = custom_vertices_size == static_cast<size_t>(-1) ? Vertices2D.size() : custom_vertices_size;
        vert_size = sizeof(Vertex2D);
    }
#else
    upload_vertices = custom_vertices_size == static_cast<size_t>(-1) ? Vertices2D.size() : custom_vertices_size;
    vert_size = sizeof(Vertex2D);
#endif

    if (VertexBuf == nullptr || upload_vertices > VertexBufSize) {
        if (VertexBuf != nullptr) {
            VertexBuf->Release();
            VertexBuf = nullptr;
        }

        VertexBufSize = upload_vertices + 1024;

        D3D11_BUFFER_DESC vbuf_desc = {};
        vbuf_desc.Usage = D3D11_USAGE_DYNAMIC;
        vbuf_desc.ByteWidth = static_cast<UINT>(VertexBufSize * vert_size);
        vbuf_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbuf_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        vbuf_desc.MiscFlags = 0;

        const auto d3d_create_vertex_buffer = D3DDevice->CreateBuffer(&vbuf_desc, nullptr, &VertexBuf);
        RUNTIME_ASSERT(SUCCEEDED(d3d_create_vertex_buffer));
    }

    D3D11_MAPPED_SUBRESOURCE vertices_resource;
    const auto d3d_map_vertex_buffer = D3DDeviceContext->Map(VertexBuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &vertices_resource);
    RUNTIME_ASSERT(SUCCEEDED(d3d_map_vertex_buffer));

#if FO_ENABLE_3D
    if (usage == EffectUsage::Model) {
        std::memcpy(vertices_resource.pData, Vertices3D.data(), upload_vertices * vert_size);
    }
    else {
        std::memcpy(vertices_resource.pData, Vertices2D.data(), upload_vertices * vert_size);
    }
#else
    std::memcpy(vertices_resource.pData, Vertices2D.data(), upload_vertices * vert_size);
#endif

    D3DDeviceContext->Unmap(VertexBuf, 0);

    // Auto generate indices
    bool need_upload_indices = false;
    if (!IsStatic) {
        switch (usage) {
#if FO_ENABLE_3D
        case EffectUsage::Model:
#endif
        case EffectUsage::ImGui: {
            // Nothing, indices generated manually
            need_upload_indices = true;
        } break;
        case EffectUsage::QuadSprite: {
            // Sprite quad
            RUNTIME_ASSERT(upload_vertices % 4 == 0);
            auto& indices = Indices;
            const auto need_size = upload_vertices / 4 * 6;
            if (indices.size() < need_size) {
                const auto prev_size = indices.size();
                indices.resize(need_size);
                for (size_t i = prev_size / 6, j = indices.size() / 6; i < j; i++) {
                    indices[i * 6 + 0] = static_cast<uint16>(i * 4 + 0);
                    indices[i * 6 + 1] = static_cast<uint16>(i * 4 + 1);
                    indices[i * 6 + 2] = static_cast<uint16>(i * 4 + 3);
                    indices[i * 6 + 3] = static_cast<uint16>(i * 4 + 1);
                    indices[i * 6 + 4] = static_cast<uint16>(i * 4 + 2);
                    indices[i * 6 + 5] = static_cast<uint16>(i * 4 + 3);
                }

                need_upload_indices = true;
            }
        } break;
        case EffectUsage::Primitive: {
            // One to one
            auto& indices = Indices;
            if (indices.size() < upload_vertices) {
                const auto prev_size = indices.size();
                indices.resize(upload_vertices);
                for (size_t i = prev_size; i < indices.size(); i++) {
                    indices[i] = static_cast<uint16>(i);
                }

                need_upload_indices = true;
            }
        } break;
        }
    }
    else {
        need_upload_indices = true;
    }

    // Fill index buffer
    if (need_upload_indices) {
        const auto upload_indices = custom_indices_size == static_cast<size_t>(-1) ? Indices.size() : custom_indices_size;

        if (IndexBuf == nullptr || upload_indices > IndexBufSize) {
            if (IndexBuf != nullptr) {
                IndexBuf->Release();
                IndexBuf = nullptr;
            }

            IndexBufSize = upload_indices + 1024;

            D3D11_BUFFER_DESC ibuf_desc = {};
            ibuf_desc.Usage = D3D11_USAGE_DYNAMIC;
            ibuf_desc.ByteWidth = static_cast<UINT>(IndexBufSize * sizeof(uint16));
            ibuf_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
            ibuf_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            ibuf_desc.MiscFlags = 0;

            const auto d3d_create_index_buffer = D3DDevice->CreateBuffer(&ibuf_desc, nullptr, &IndexBuf);
            RUNTIME_ASSERT(SUCCEEDED(d3d_create_index_buffer));
        }

        D3D11_MAPPED_SUBRESOURCE indices_resource;
        const auto d3d_map_index_buffer = D3DDeviceContext->Map(IndexBuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &indices_resource);
        RUNTIME_ASSERT(SUCCEEDED(d3d_map_index_buffer));

        std::memcpy(indices_resource.pData, Indices.data(), upload_indices * sizeof(uint16));

        D3DDeviceContext->Unmap(IndexBuf, 0);
    }
    else {
        RUNTIME_ASSERT(IndexBuf);
    }
}

Direct3D_Effect::~Direct3D_Effect()
{
    STACK_TRACE_ENTRY();

#define SAFE_RELEASE(ptr) \
    if (ptr != nullptr) { \
        ptr->Release(); \
    }
    for (size_t i = 0; i < EFFECT_MAX_PASSES; i++) {
        SAFE_RELEASE(VertexShader[i]);
        SAFE_RELEASE(InputLayout[i]);
        SAFE_RELEASE(PixelShader[i]);
        SAFE_RELEASE(RasterizerState[i]);
        SAFE_RELEASE(BlendState[i]);
        SAFE_RELEASE(DepthStencilState[i]);
#if FO_ENABLE_3D
        SAFE_RELEASE(RasterizerState_Culling[i]);
#endif
    }
    SAFE_RELEASE(Cb_ProjBuf);
    SAFE_RELEASE(Cb_MainTexBuf);
    SAFE_RELEASE(Cb_ContourBuf);
    SAFE_RELEASE(Cb_TimeBuf);
    SAFE_RELEASE(Cb_RandomValueBuf);
    SAFE_RELEASE(Cb_ScriptValueBuf);
#if FO_ENABLE_3D
    SAFE_RELEASE(Cb_ModelBuf);
    SAFE_RELEASE(Cb_ModelTexBuf);
    SAFE_RELEASE(Cb_ModelAnimBuf);
#endif
#undef SAFE_RELEASE
}

void Direct3D_Effect::DrawBuffer(RenderDrawBuffer* dbuf, size_t start_index, size_t indices_to_draw, RenderTexture* custom_tex)
{
    STACK_TRACE_ENTRY();

    const auto* d3d_dbuf = static_cast<Direct3D_DrawBuffer*>(dbuf);

#if FO_ENABLE_3D
    const auto* main_tex = static_cast<Direct3D_Texture*>(custom_tex != nullptr ? custom_tex : (ModelTex[0] != nullptr ? ModelTex[0] : (MainTex != nullptr ? MainTex : DummyTexture)));
#else
    const auto* main_tex = static_cast<Direct3D_Texture*>(custom_tex != nullptr ? custom_tex : (MainTex != nullptr ? MainTex : DummyTexture));
#endif

    auto draw_mode = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    if (Usage == EffectUsage::Primitive) {
        switch (d3d_dbuf->PrimType) {
        case RenderPrimitiveType::PointList:
            draw_mode = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
            break;
        case RenderPrimitiveType::LineList:
            draw_mode = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
            break;
        case RenderPrimitiveType::LineStrip:
            draw_mode = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
            break;
        case RenderPrimitiveType::TriangleList:
            draw_mode = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            break;
        case RenderPrimitiveType::TriangleStrip:
            draw_mode = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
            break;
        }
    }

    // Fill constant buffers
    const auto setup_cbuffer = [this](auto& buf, auto& buf_handle) {
        if (buf_handle == nullptr) {
            D3D11_BUFFER_DESC cbuf_desc = {};
            cbuf_desc.ByteWidth = sizeof(buf);
            cbuf_desc.Usage = D3D11_USAGE_DYNAMIC;
            cbuf_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            cbuf_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            cbuf_desc.MiscFlags = 0;

            const auto d3d_create_cbuffer = D3DDevice->CreateBuffer(&cbuf_desc, nullptr, &buf_handle);
            RUNTIME_ASSERT(SUCCEEDED(d3d_create_cbuffer));
        }

        D3D11_MAPPED_SUBRESOURCE cbuffer_resource;
        const auto d3d_map_cbuffer = D3DDeviceContext->Map(buf_handle, 0, D3D11_MAP_WRITE_DISCARD, 0, &cbuffer_resource);
        RUNTIME_ASSERT(SUCCEEDED(d3d_map_cbuffer));

#if FO_ENABLE_3D
        if constexpr (std::is_same_v<std::decay_t<decltype(buf)>, ModelBuffer>) {
            const auto bind_size = sizeof(ModelBuffer) - (MODEL_MAX_BONES - MatrixCount) * sizeof(float) * 16;
            std::memcpy(cbuffer_resource.pData, &buf, bind_size);
        }
        else
#endif
        {
            UNUSED_VARIABLE(this);
            std::memcpy(cbuffer_resource.pData, &buf, sizeof(buf));
        }

        D3DDeviceContext->Unmap(buf_handle, 0);
    };

    if (NeedProjBuf && !ProjBuf.has_value()) {
        auto&& proj_buf = ProjBuf = ProjBuffer();
        std::memcpy(proj_buf->ProjMatrix, ProjectionMatrixColMaj[0], 16 * sizeof(float));
    }

    if (NeedMainTexBuf && !MainTexBuf.has_value()) {
        auto&& main_tex_buf = MainTexBuf = MainTexBuffer();
        std::memcpy(main_tex_buf->MainTexSize, main_tex->SizeData, 4 * sizeof(float));
    }

#define CBUF_UPLOAD_BUFFER(buf) \
    if (Need##buf && buf.has_value()) { \
        setup_cbuffer(*buf, Cb_##buf); \
        buf.reset(); \
    }
    CBUF_UPLOAD_BUFFER(ProjBuf);
    CBUF_UPLOAD_BUFFER(MainTexBuf);
    CBUF_UPLOAD_BUFFER(ContourBuf);
    CBUF_UPLOAD_BUFFER(TimeBuf);
    CBUF_UPLOAD_BUFFER(RandomValueBuf);
    CBUF_UPLOAD_BUFFER(ScriptValueBuf);
#if FO_ENABLE_3D
    CBUF_UPLOAD_BUFFER(ModelBuf);
    CBUF_UPLOAD_BUFFER(ModelTexBuf);
    CBUF_UPLOAD_BUFFER(ModelAnimBuf);
#endif
#undef CBUF_UPLOAD_BUFFER

    const auto* egg_tex = static_cast<Direct3D_Texture*>(EggTex != nullptr ? EggTex : DummyTexture);
    const auto draw_count = static_cast<UINT>(indices_to_draw == static_cast<size_t>(-1) ? d3d_dbuf->Indices.size() : indices_to_draw);

    for (size_t pass = 0; pass < _passCount; pass++) {
#if FO_ENABLE_3D
        if (DisableShadow && _isShadow[pass]) {
            continue;
        }
#endif
        // Setup shader and vertex buffers
#if FO_ENABLE_3D
        UINT stride = Usage == EffectUsage::Model ? sizeof(Vertex3D) : sizeof(Vertex2D);
#else
        UINT stride = sizeof(Vertex2D);
#endif
        UINT offset = 0;

        D3DDeviceContext->IASetInputLayout(InputLayout[pass]);
        D3DDeviceContext->IASetVertexBuffers(0, 1, &d3d_dbuf->VertexBuf, &stride, &offset);
        D3DDeviceContext->IASetIndexBuffer(d3d_dbuf->IndexBuf, DXGI_FORMAT_R16_UINT, 0);
        D3DDeviceContext->IASetPrimitiveTopology(draw_mode);

        D3DDeviceContext->VSSetShader(VertexShader[pass], nullptr, 0);
        D3DDeviceContext->PSSetShader(PixelShader[pass], nullptr, 0);

#define CBUF_SET_BUFFER(buf) \
    if (_pos##buf[pass] != -1 && Cb_##buf != nullptr) { \
        D3DDeviceContext->VSSetConstantBuffers(_pos##buf[pass], 1, &Cb_##buf); \
        D3DDeviceContext->PSSetConstantBuffers(_pos##buf[pass], 1, &Cb_##buf); \
    }
        CBUF_SET_BUFFER(ProjBuf);
        CBUF_SET_BUFFER(MainTexBuf);
        CBUF_SET_BUFFER(ContourBuf);
        CBUF_SET_BUFFER(TimeBuf);
        CBUF_SET_BUFFER(RandomValueBuf);
        CBUF_SET_BUFFER(ScriptValueBuf);
#if FO_ENABLE_3D
        CBUF_SET_BUFFER(ModelBuf);
        CBUF_SET_BUFFER(ModelTexBuf);
        CBUF_SET_BUFFER(ModelAnimBuf);
#endif
#undef CBUF_SET_BUFFER

        const auto find_tex_sampler = [](const Direct3D_Texture* tex) {
            if (tex->LinearFiltered) {
                return &LinearSampler;
            }
            else {
                return &PointSampler;
            }
        };

        if (_posMainTex[pass] != -1) {
            D3DDeviceContext->PSSetShaderResources(_posMainTex[pass], 1, &main_tex->ShaderTexView);
            D3DDeviceContext->PSSetSamplers(_posMainTex[pass], 1, find_tex_sampler(main_tex));
        }

        if (_posEggTex[pass] != -1) {
            D3DDeviceContext->PSSetShaderResources(_posEggTex[pass], 1, &egg_tex->ShaderTexView);
            D3DDeviceContext->PSSetSamplers(_posEggTex[pass], 1, find_tex_sampler(egg_tex));
        }

#if FO_ENABLE_3D
        if (NeedModelTex[pass]) {
            for (size_t i = 0; i < MODEL_MAX_TEXTURES; i++) {
                if (_posModelTex[pass][i] != -1) {
                    const auto* model_tex = static_cast<Direct3D_Texture*>(ModelTex[i] != nullptr ? ModelTex[i] : DummyTexture);
                    D3DDeviceContext->PSSetShaderResources(_posModelTex[pass][i], 1, &model_tex->ShaderTexView);
                    D3DDeviceContext->PSSetSamplers(_posModelTex[pass][i], 1, find_tex_sampler(model_tex));
                }
            }
        }
#endif

        constexpr float blend_factor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        D3DDeviceContext->OMSetBlendState(DisableBlending ? nullptr : BlendState[pass], blend_factor, 0xFFFFFFFF);
        D3DDeviceContext->OMSetDepthStencilState(DepthStencilState[pass], 0);

#if FO_ENABLE_3D
        D3DDeviceContext->RSSetState(DisableCulling ? RasterizerState[pass] : RasterizerState_Culling[pass]);
#else
        D3DDeviceContext->RSSetState(RasterizerState[pass]);
#endif
        D3DDeviceContext->RSSetScissorRects(1, ScissorEnabled ? &ScissorRect : &DisabledScissorRect);

        if (FeatureLevel <= D3D_FEATURE_LEVEL_9_3 && draw_mode == D3D11_PRIMITIVE_TOPOLOGY_POINTLIST) {
            RUNTIME_ASSERT(start_index == 0);
            D3DDeviceContext->Draw(draw_count, 0);
        }
        else {
            D3DDeviceContext->DrawIndexed(draw_count, static_cast<UINT>(start_index), 0);
        }

        // Unbind
        constexpr ID3D11Buffer* null_buf = nullptr;
        constexpr ID3D11ShaderResourceView* null_res = nullptr;
        constexpr ID3D11SamplerState* null_sampler = nullptr;

#define CBUF_UNSET_BUFFER(buf) \
    if (_pos##buf[pass] != -1 && Cb_##buf != nullptr) { \
        D3DDeviceContext->VSSetConstantBuffers(_pos##buf[pass], 1, &null_buf); \
        D3DDeviceContext->PSSetConstantBuffers(_pos##buf[pass], 1, &null_buf); \
    }
        CBUF_UNSET_BUFFER(ProjBuf);
        CBUF_UNSET_BUFFER(MainTexBuf);
        CBUF_UNSET_BUFFER(ContourBuf);
        CBUF_UNSET_BUFFER(TimeBuf);
        CBUF_UNSET_BUFFER(RandomValueBuf);
        CBUF_UNSET_BUFFER(ScriptValueBuf);
#if FO_ENABLE_3D
        CBUF_UNSET_BUFFER(ModelBuf);
        CBUF_UNSET_BUFFER(ModelTexBuf);
        CBUF_UNSET_BUFFER(ModelAnimBuf);
#endif
#undef CBUF_SET_BUFFER

        if (_posMainTex[pass] != -1) {
            D3DDeviceContext->PSSetShaderResources(_posMainTex[pass], 1, &null_res);
            D3DDeviceContext->PSSetSamplers(_posMainTex[pass], 1, &null_sampler);
        }

        if (_posEggTex[pass] != -1) {
            D3DDeviceContext->PSSetShaderResources(_posEggTex[pass], 1, &null_res);
            D3DDeviceContext->PSSetSamplers(_posEggTex[pass], 1, &null_sampler);
        }

#if FO_ENABLE_3D
        if (NeedModelTex[pass]) {
            for (size_t i = 0; i < MODEL_MAX_TEXTURES; i++) {
                if (_posModelTex[pass][i] != -1) {
                    D3DDeviceContext->PSSetShaderResources(_posModelTex[pass][i], 0, &null_res);
                    D3DDeviceContext->PSSetSamplers(_posModelTex[pass][i], 1, &null_sampler);
                }
            }
        }
#endif

        D3DDeviceContext->IASetInputLayout(nullptr);
        D3DDeviceContext->IASetVertexBuffers(0, 1, &null_buf, &stride, &offset);
        D3DDeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);

        D3DDeviceContext->VSSetShader(nullptr, nullptr, 0);
        D3DDeviceContext->PSSetShader(nullptr, nullptr, 0);
    }
}

#endif
