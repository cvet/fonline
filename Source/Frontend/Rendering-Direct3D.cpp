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

#include "Rendering.h"

#if FO_HAVE_DIRECT_3D

#include "Application.h"

#include "SDL3/SDL_video.h"
#include <d3d11_1.h>
#include <d3dcompiler.h>

#include "WinApiUndef-Include.h"

FO_BEGIN_NAMESPACE();

class Direct3D_Texture final : public RenderTexture
{
public:
    Direct3D_Texture(isize32 size, bool linear_filtered, bool with_depth) :
        RenderTexture(size, linear_filtered, with_depth)
    {
    }
    Direct3D_Texture(const Direct3D_Texture&) = delete;
    Direct3D_Texture(Direct3D_Texture&&) noexcept = delete;
    auto operator=(const Direct3D_Texture&) -> Direct3D_Texture& = delete;
    auto operator=(Direct3D_Texture&&) noexcept -> Direct3D_Texture& = delete;
    ~Direct3D_Texture() override;

    [[nodiscard]] auto GetTexturePixel(ipos32 pos) const -> ucolor override;
    [[nodiscard]] auto GetTextureRegion(ipos32 pos, isize32 size) const -> vector<ucolor> override;
    void UpdateTextureRegion(ipos32 pos, isize32 size, const ucolor* data, bool use_dest_pitch) override;

    raw_ptr<ID3D11Texture2D> TexHandle {};
    raw_ptr<ID3D11Texture2D> DepthStencil {};
    raw_ptr<ID3D11RenderTargetView> RenderTargetView {};
    raw_ptr<ID3D11DepthStencilView> DepthStencilView {};
    raw_ptr<ID3D11ShaderResourceView> ShaderTexView {};
};

class Direct3D_DrawBuffer final : public RenderDrawBuffer
{
public:
    explicit Direct3D_DrawBuffer(bool is_static) :
        RenderDrawBuffer(is_static)
    {
    }
    Direct3D_DrawBuffer(const Direct3D_DrawBuffer&) = delete;
    Direct3D_DrawBuffer(Direct3D_DrawBuffer&&) noexcept = delete;
    auto operator=(const Direct3D_DrawBuffer&) -> Direct3D_DrawBuffer& = delete;
    auto operator=(Direct3D_DrawBuffer&&) noexcept -> Direct3D_DrawBuffer& = delete;
    ~Direct3D_DrawBuffer() override;

    void Upload(EffectUsage usage, optional<size_t> custom_vertices_size, optional<size_t> custom_indices_size) override;

    raw_ptr<ID3D11Buffer> VertexBuf {};
    raw_ptr<ID3D11Buffer> IndexBuf {};
    size_t VertexBufSize {};
    size_t IndexBufSize {};
};

class Direct3D_Effect final : public RenderEffect
{
    friend class Direct3D_Renderer;

public:
    Direct3D_Effect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) :
        RenderEffect(usage, name, loader)
    {
    }
    Direct3D_Effect(const Direct3D_Effect&) = delete;
    Direct3D_Effect(Direct3D_Effect&&) noexcept = delete;
    auto operator=(const Direct3D_Effect&) -> Direct3D_Effect& = delete;
    auto operator=(Direct3D_Effect&&) noexcept -> Direct3D_Effect& = delete;
    ~Direct3D_Effect() override;

    void DrawBuffer(RenderDrawBuffer* dbuf, size_t start_index, optional<size_t> indices_to_draw, const RenderTexture* custom_tex) override;

    raw_ptr<ID3D11VertexShader> VertexShader[EFFECT_MAX_PASSES] {};
    raw_ptr<ID3D11InputLayout> InputLayout[EFFECT_MAX_PASSES] {};
    raw_ptr<ID3D11PixelShader> PixelShader[EFFECT_MAX_PASSES] {};
    raw_ptr<ID3D11RasterizerState> RasterizerState[EFFECT_MAX_PASSES] {};
    raw_ptr<ID3D11BlendState> BlendState[EFFECT_MAX_PASSES] {};
    raw_ptr<ID3D11DepthStencilState> DepthStencilState[EFFECT_MAX_PASSES] {};
#if FO_ENABLE_3D
    raw_ptr<ID3D11RasterizerState> RasterizerState_Culling[EFFECT_MAX_PASSES] {};
#endif

    raw_ptr<ID3D11Buffer> Cb_ProjBuf {};
    raw_ptr<ID3D11Buffer> Cb_MainTexBuf {};
    raw_ptr<ID3D11Buffer> Cb_ContourBuf {};
    raw_ptr<ID3D11Buffer> Cb_TimeBuf {};
    raw_ptr<ID3D11Buffer> Cb_RandomValueBuf {};
    raw_ptr<ID3D11Buffer> Cb_ScriptValueBuf {};
#if FO_ENABLE_3D
    raw_ptr<ID3D11Buffer> Cb_ModelBuf {};
    raw_ptr<ID3D11Buffer> Cb_ModelTexBuf {};
    raw_ptr<ID3D11Buffer> Cb_ModelAnimBuf {};
#endif
};

static raw_ptr<GlobalSettings> Settings {};
static bool RenderDebug {};
static bool VSync {};
static raw_ptr<SDL_Window> SdlWindow {};
static D3D_FEATURE_LEVEL FeatureLevel {};
static raw_ptr<IDXGISwapChain> SwapChain {};
static raw_ptr<ID3D11Device> D3DDevice {};
static raw_ptr<ID3D11DeviceContext> D3DDeviceContext {};
static raw_ptr<ID3D11DeviceContext1> D3DDeviceContext1 {};
static raw_ptr<ID3D11RenderTargetView> MainRenderTarget {};
static raw_ptr<ID3D11RenderTargetView> CurRenderTarget {};
static raw_ptr<ID3D11DepthStencilView> CurDepthStencil {};
static raw_ptr<ID3D11SamplerState> PointSampler {};
static raw_ptr<ID3D11SamplerState> LinearSampler {};
static raw_ptr<ID3D11Texture2D> OnePixStagingTex {};
static unique_ptr<RenderTexture> DummyTexture {};
static mat44 ProjectionMatrixColMaj {};
static bool ScissorEnabled {};
static D3D11_RECT ScissorRect {};
static D3D11_RECT DisabledScissorRect {};
static D3D11_VIEWPORT ViewPort {};
static irect32 ViewPortRect {};
static isize32 BackBufSize {};
static isize32 TargetSize {};

static auto ConvertBlend(BlendFuncType blend, bool is_alpha) -> D3D11_BLEND
{
    FO_STACK_TRACE_ENTRY();

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

    FO_UNREACHABLE_PLACE();
}

static auto ConvertBlendOp(BlendEquationType blend_op) -> D3D11_BLEND_OP
{
    FO_STACK_TRACE_ENTRY();

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

    FO_UNREACHABLE_PLACE();
}

void Direct3D_Renderer::Init(GlobalSettings& settings, WindowInternalHandle* window)
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Used DirectX rendering");

    Settings = &settings;
    RenderDebug = settings.RenderDebug;
    VSync = settings.VSync;
    SdlWindow = static_cast<SDL_Window*>(window);

    auto* hwnd = static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(SdlWindow.get()), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));

    // Device
    {
        constexpr D3D_FEATURE_LEVEL feature_levels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
            D3D_FEATURE_LEVEL_9_3,
            D3D_FEATURE_LEVEL_9_2,
            D3D_FEATURE_LEVEL_9_1,
        };
        const map<D3D_FEATURE_LEVEL, string> feature_levels_str = {
            {D3D_FEATURE_LEVEL_11_1, "11.1"},
            {D3D_FEATURE_LEVEL_11_0, "11.0"},
            {D3D_FEATURE_LEVEL_10_1, "10.1"},
            {D3D_FEATURE_LEVEL_10_0, "10.0"},
            {D3D_FEATURE_LEVEL_9_3, "9.3"},
            {D3D_FEATURE_LEVEL_9_2, "9.2"},
            {D3D_FEATURE_LEVEL_9_1, "9.1"},
        };
        constexpr auto feature_levels_count = numeric_cast<UINT>(std::size(feature_levels));

        UINT device_flags = D3D11_CREATE_DEVICE_SINGLETHREADED;

        if (RenderDebug) {
            device_flags |= D3D11_CREATE_DEVICE_DEBUG;
        }

        const auto d3d_hardware_create_device = ::D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, device_flags, feature_levels, feature_levels_count, D3D11_SDK_VERSION, D3DDevice.get_pp(), &FeatureLevel, D3DDeviceContext.get_pp());

        if (FAILED(d3d_hardware_create_device)) {
            const auto d3d_warp_create_device = ::D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, device_flags, feature_levels, feature_levels_count, D3D11_SDK_VERSION, D3DDevice.get_pp(), &FeatureLevel, D3DDeviceContext.get_pp());

            if (FAILED(d3d_warp_create_device)) {
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

        if (FAILED(d3d_create_factory)) {
            throw AppInitException("CreateDXGIFactory failed", d3d_create_factory);
        }

        auto factory_release = ScopeCallback([&factory]() noexcept { factory->Release(); });

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

            const auto d3d_create_swap_chain = factory->CreateSwapChain(D3DDevice.get(), &swap_chain_desc, SwapChain.get_pp());

            if (FAILED(d3d_create_swap_chain)) {
                swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

                const auto d3d_create_swap_chain_2 = factory->CreateSwapChain(D3DDevice.get(), &swap_chain_desc, SwapChain.get_pp());

                if (FAILED(d3d_create_swap_chain_2)) {
                    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

                    const auto d3d_create_swap_chain_3 = factory->CreateSwapChain(D3DDevice.get(), &swap_chain_desc, SwapChain.get_pp());

                    if (FAILED(d3d_create_swap_chain_3)) {
                        swap_chain_desc.BufferCount = 1;

                        const auto d3d_create_swap_chain_4 = factory->CreateSwapChain(D3DDevice.get(), &swap_chain_desc, SwapChain.get_pp());

                        if (FAILED(d3d_create_swap_chain_4)) {
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

            const auto d3d_create_swap_chain = factory->CreateSwapChain(D3DDevice.get(), &swap_chain_desc, SwapChain.get_pp());

            if (FAILED(d3d_create_swap_chain)) {
                swap_chain_desc.BufferCount = 1;

                const auto d3d_create_swap_chain_2 = factory->CreateSwapChain(D3DDevice.get(), &swap_chain_desc, SwapChain.get_pp());

                if (FAILED(d3d_create_swap_chain_2)) {
                    throw AppInitException("CreateSwapChain failed", d3d_create_swap_chain, d3d_create_swap_chain_2);
                }
                else {
                    WriteLog("Direct3D swap chain created with one buffer count");
                }
            }
        }

        // Disable Alt+Enter
        if (IDXGIFactory* factory2 = nullptr; SUCCEEDED(SwapChain->GetParent(IID_IDXGIFactory, reinterpret_cast<void**>(&factory2)))) {
            factory2->MakeWindowAssociation(hwnd, DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_PRINT_SCREEN);
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

            if (FAILED(d3d_create_sampler)) {
                throw EffectLoadException("Failed to create sampler", d3d_create_sampler);
            }

            return sampler;
        };

        PointSampler = create_sampler(D3D11_FILTER_MIN_MAG_MIP_POINT);
        LinearSampler = create_sampler(D3D11_FILTER_MIN_MAG_MIP_LINEAR);
    }

    // Calculate atlas size
    int32 atlas_w;
    int32 atlas_h;

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
        FO_UNREACHABLE_PLACE();
    }

    FO_RUNTIME_ASSERT_STR(atlas_w >= AppRender::MIN_ATLAS_SIZE, strex("Min texture width must be at least {}", AppRender::MIN_ATLAS_SIZE));
    FO_RUNTIME_ASSERT_STR(atlas_h >= AppRender::MIN_ATLAS_SIZE, strex("Min texture height must be at least {}", AppRender::MIN_ATLAS_SIZE));

    const_cast<int32&>(AppRender::MAX_ATLAS_WIDTH) = atlas_w;
    const_cast<int32&>(AppRender::MAX_ATLAS_HEIGHT) = atlas_h;

    // Back buffer view
    ID3D11Texture2D* back_buf = nullptr;
    const auto d3d_get_back_buf = SwapChain->GetBuffer(0, IID_PPV_ARGS(&back_buf));
    FO_RUNTIME_ASSERT(SUCCEEDED(d3d_get_back_buf));
    FO_RUNTIME_ASSERT(back_buf);
    const auto d3d_create_back_buf_rt_view = D3DDevice->CreateRenderTargetView(back_buf, nullptr, MainRenderTarget.get_pp());
    FO_RUNTIME_ASSERT(SUCCEEDED(d3d_create_back_buf_rt_view));
    back_buf->Release();

    BackBufSize = {settings.ScreenWidth, settings.ScreenHeight};

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

    const auto d3d_create_one_pix_staging_tex = D3DDevice->CreateTexture2D(&one_pix_staging_desc, nullptr, OnePixStagingTex.get_pp());
    FO_RUNTIME_ASSERT(SUCCEEDED(d3d_create_one_pix_staging_tex));

    // Dummy texture
    constexpr ucolor dummy_pixel[1] = {ucolor {255, 0, 255, 255}};
    DummyTexture = CreateTexture({1, 1}, false, false);
    DummyTexture->UpdateTextureRegion({}, {1, 1}, dummy_pixel);

    // Init render target
    SetRenderTarget(nullptr);
    DisableScissor();
}

void Direct3D_Renderer::Present()
{
    FO_STACK_TRACE_ENTRY();

    const auto d3d_swap_chain = SwapChain->Present(VSync ? 1 : 0, 0);
    FO_RUNTIME_ASSERT(SUCCEEDED(d3d_swap_chain));

    if (D3DDeviceContext1) {
        D3DDeviceContext1->DiscardView(CurRenderTarget.get());
    }

    D3DDeviceContext->OMSetRenderTargets(1, CurRenderTarget.get_pp(), CurDepthStencil.get());
}

auto Direct3D_Renderer::CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture>
{
    FO_STACK_TRACE_ENTRY();

    auto d3d_tex = SafeAlloc::MakeUnique<Direct3D_Texture>(size, linear_filtered, with_depth);

    D3D11_TEXTURE2D_DESC tex_desc = {};
    tex_desc.Width = size.width;
    tex_desc.Height = size.height;
    tex_desc.MipLevels = 1;
    tex_desc.ArraySize = 1;
    tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    tex_desc.SampleDesc.Count = 1;
    tex_desc.SampleDesc.Quality = 0;
    tex_desc.Usage = D3D11_USAGE_DEFAULT;
    tex_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    tex_desc.CPUAccessFlags = 0;
    tex_desc.MiscFlags = 0;

    const auto d3d_create_texure_2d = D3DDevice->CreateTexture2D(&tex_desc, nullptr, d3d_tex->TexHandle.get_pp());
    FO_RUNTIME_ASSERT(SUCCEEDED(d3d_create_texure_2d));

    if (with_depth) {
        D3D11_TEXTURE2D_DESC depth_tex_desc = {};
        depth_tex_desc.Width = size.width;
        depth_tex_desc.Height = size.height;
        depth_tex_desc.MipLevels = 1;
        depth_tex_desc.ArraySize = 1;
        depth_tex_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depth_tex_desc.SampleDesc.Count = 1;
        depth_tex_desc.SampleDesc.Quality = 0;
        depth_tex_desc.Usage = D3D11_USAGE_DEFAULT;
        depth_tex_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        depth_tex_desc.CPUAccessFlags = 0;
        depth_tex_desc.MiscFlags = 0;

        const auto d3d_create_texure_depth = D3DDevice->CreateTexture2D(&depth_tex_desc, nullptr, d3d_tex->DepthStencil.get_pp());
        FO_RUNTIME_ASSERT(SUCCEEDED(d3d_create_texure_depth));

        D3D11_DEPTH_STENCIL_VIEW_DESC depth_view_desc = {};
        depth_view_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depth_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        depth_view_desc.Texture2D.MipSlice = 0;

        const auto d3d_create_depth_view = D3DDevice->CreateDepthStencilView(d3d_tex->DepthStencil.get(), &depth_view_desc, d3d_tex->DepthStencilView.get_pp());
        FO_RUNTIME_ASSERT(SUCCEEDED(d3d_create_depth_view));
    }

    const auto d3d_create_tex_rt_view = D3DDevice->CreateRenderTargetView(d3d_tex->TexHandle.get(), nullptr, d3d_tex->RenderTargetView.get_pp());
    FO_RUNTIME_ASSERT(SUCCEEDED(d3d_create_tex_rt_view));

    D3D11_SHADER_RESOURCE_VIEW_DESC tex_view_desc = {};
    tex_view_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    tex_view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    tex_view_desc.Texture2D.MipLevels = 1;
    tex_view_desc.Texture2D.MostDetailedMip = 0;
    const auto d3d_create_shader_res_view = D3DDevice->CreateShaderResourceView(d3d_tex->TexHandle.get(), &tex_view_desc, d3d_tex->ShaderTexView.get_pp());
    FO_RUNTIME_ASSERT(SUCCEEDED(d3d_create_shader_res_view));

    return std::move(d3d_tex);
}

auto Direct3D_Renderer::CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer>
{
    FO_STACK_TRACE_ENTRY();

    auto d3d_dbuf = SafeAlloc::MakeUnique<Direct3D_DrawBuffer>(is_static);

    return std::move(d3d_dbuf);
}

auto Direct3D_Renderer::CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect>
{
    FO_STACK_TRACE_ENTRY();

    auto d3d_effect = SafeAlloc::MakeUnique<Direct3D_Effect>(usage, name, loader);

    for (size_t pass = 0; pass < d3d_effect->_passCount; pass++) {
        // Create the vertex shader
        {
            const string vertex_shader_fname = strex("{}.fofx-{}-vert-hlsl", strex(name).erase_file_extension(), pass + 1);
            string vertex_shader_content = loader(vertex_shader_fname);
            FO_RUNTIME_ASSERT(!vertex_shader_content.empty());

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

            const auto d3d_compile = ::D3DCompile(vertex_shader_content.c_str(), vertex_shader_content.length(), nullptr, nullptr, nullptr, "main", "vs_4_0_level_9_1", 0, 0, &vertex_shader_blob, &error_blob);

            if (FAILED(d3d_compile)) {
                const string error = static_cast<const char*>(error_blob->GetBufferPointer());
                throw EffectLoadException("Failed to compile Vertex Shader", vertex_shader_fname, vertex_shader_content, error);
            }

            const auto d3d_create_vertex_shader = D3DDevice->CreateVertexShader(vertex_shader_blob->GetBufferPointer(), vertex_shader_blob->GetBufferSize(), nullptr, d3d_effect->VertexShader[pass].get_pp());

            if (FAILED(d3d_create_vertex_shader)) {
                throw EffectLoadException("Failed to create Vertex Shader from binary", d3d_create_vertex_shader, vertex_shader_fname, vertex_shader_content);
            }

            // Create the input layout
#if FO_ENABLE_3D
            if (usage == EffectUsage::Model) {
                static_assert(BONES_PER_VERTEX == 4);

                constexpr D3D11_INPUT_ELEMENT_DESC local_layout[] = {
                    {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, numeric_cast<UINT>(offsetof(Vertex3D, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, numeric_cast<UINT>(offsetof(Vertex3D, Normal)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT, 0, numeric_cast<UINT>(offsetof(Vertex3D, TexCoord)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD", 3, DXGI_FORMAT_R32G32_FLOAT, 0, numeric_cast<UINT>(offsetof(Vertex3D, TexCoordBase)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD", 4, DXGI_FORMAT_R32G32B32_FLOAT, 0, numeric_cast<UINT>(offsetof(Vertex3D, Tangent)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD", 5, DXGI_FORMAT_R32G32B32_FLOAT, 0, numeric_cast<UINT>(offsetof(Vertex3D, Bitangent)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD", 6, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, numeric_cast<UINT>(offsetof(Vertex3D, BlendWeights)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD", 7, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, numeric_cast<UINT>(offsetof(Vertex3D, BlendIndices)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD", 8, DXGI_FORMAT_R8G8B8A8_UNORM, 0, numeric_cast<UINT>(offsetof(Vertex3D, Color)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                };

                const auto d3d_create_input_layout = D3DDevice->CreateInputLayout(local_layout, 9, vertex_shader_blob->GetBufferPointer(), vertex_shader_blob->GetBufferSize(), d3d_effect->InputLayout[pass].get_pp());

                if (FAILED(d3d_create_input_layout)) {
                    throw EffectLoadException("Failed to create Vertex Shader 3D layout", d3d_create_input_layout, vertex_shader_fname, vertex_shader_content);
                }
            }
            else
#endif
            {
                constexpr D3D11_INPUT_ELEMENT_DESC local_layout[] = {
                    {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, numeric_cast<UINT>(offsetof(Vertex2D, PosX)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD", 1, DXGI_FORMAT_R8G8B8A8_UNORM, 0, numeric_cast<UINT>(offsetof(Vertex2D, Color)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT, 0, numeric_cast<UINT>(offsetof(Vertex2D, TexU)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD", 3, DXGI_FORMAT_R32G32_FLOAT, 0, numeric_cast<UINT>(offsetof(Vertex2D, EggTexU)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                };

                const auto d3d_create_input_layout = D3DDevice->CreateInputLayout(local_layout, 4, vertex_shader_blob->GetBufferPointer(), vertex_shader_blob->GetBufferSize(), d3d_effect->InputLayout[pass].get_pp());

                if (FAILED(d3d_create_input_layout)) {
                    throw EffectLoadException("Failed to create Vertex Shader 2D layout", d3d_create_input_layout, vertex_shader_fname, vertex_shader_content);
                }
            }
        }

        // Create the pixel shader
        {
            const string pixel_shader_fname = strex("{}.fofx-{}-frag-hlsl", strex(name).erase_file_extension(), pass + 1);
            string pixel_shader_content = loader(pixel_shader_fname);
            FO_RUNTIME_ASSERT(!pixel_shader_content.empty());

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

            const auto d3d_compile = ::D3DCompile(pixel_shader_content.c_str(), pixel_shader_content.length(), nullptr, nullptr, nullptr, "main", "ps_4_0_level_9_1", 0, 0, &pixel_shader_blob, &error_blob);

            if (FAILED(d3d_compile)) {
                const string error = static_cast<const char*>(error_blob->GetBufferPointer());
                throw EffectLoadException("Failed to compile Pixel Shader", pixel_shader_fname, pixel_shader_content, error);
            }

            const auto d3d_create_pixel_shader = D3DDevice->CreatePixelShader(pixel_shader_blob->GetBufferPointer(), pixel_shader_blob->GetBufferSize(), nullptr, d3d_effect->PixelShader[pass].get_pp());

            if (FAILED(d3d_create_pixel_shader)) {
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

            const auto d3d_create_blend_state = D3DDevice->CreateBlendState(&blend_desc, d3d_effect->BlendState[pass].get_pp());

            if (FAILED(d3d_create_blend_state)) {
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

            const auto d3d_create_rasterized_state = D3DDevice->CreateRasterizerState(&rasterizer_desc, d3d_effect->RasterizerState[pass].get_pp());

            if (FAILED(d3d_create_rasterized_state)) {
                throw EffectLoadException("Failed to call CreateRasterizerState", d3d_create_rasterized_state, name);
            }

#if FO_ENABLE_3D
            D3D11_RASTERIZER_DESC rasterizer_culling_desc = rasterizer_desc;
            rasterizer_desc.CullMode = D3D11_CULL_BACK;
            rasterizer_desc.FrontCounterClockwise = TRUE;

            const auto d3d_create_rasterized_state_culling = D3DDevice->CreateRasterizerState(&rasterizer_culling_desc, d3d_effect->RasterizerState_Culling[pass].get_pp());

            if (FAILED(d3d_create_rasterized_state_culling)) {
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

            const auto d3d_create_depth_stencil_state = D3DDevice->CreateDepthStencilState(&depth_stencil_desc, d3d_effect->DepthStencilState[pass].get_pp());

            if (FAILED(d3d_create_depth_stencil_state)) {
                throw EffectLoadException("Failed to call CreateDepthStencilState", d3d_create_depth_stencil_state, name);
            }
        }
    }

    return std::move(d3d_effect);
}

auto Direct3D_Renderer::CreateOrthoMatrix(float32 left, float32 right, float32 bottom, float32 top, float32 nearp, float32 farp) -> mat44
{
    FO_STACK_TRACE_ENTRY();

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

auto Direct3D_Renderer::GetViewPort() -> irect32
{
    FO_STACK_TRACE_ENTRY();

    return ViewPortRect;
}

void Direct3D_Renderer::SetRenderTarget(RenderTexture* tex)
{
    FO_STACK_TRACE_ENTRY();

    int32 vp_ox;
    int32 vp_oy;
    int32 vp_width;
    int32 vp_height;
    int32 screen_width;
    int32 screen_height;

    if (tex != nullptr) {
        const auto* d3d_tex = static_cast<Direct3D_Texture*>(tex); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        CurRenderTarget = d3d_tex->RenderTargetView;
        CurDepthStencil = d3d_tex->DepthStencilView;

        vp_ox = 0;
        vp_oy = 0;
        vp_width = d3d_tex->Size.width;
        vp_height = d3d_tex->Size.height;
        screen_width = vp_width;
        screen_height = vp_height;
    }
    else {
        CurRenderTarget = MainRenderTarget;
        CurDepthStencil = nullptr;

        const auto back_buf_aspect = checked_div<float32>(numeric_cast<float32>(BackBufSize.width), numeric_cast<float32>(BackBufSize.height));
        const auto screen_aspect = checked_div<float32>(numeric_cast<float32>(Settings->ScreenWidth), numeric_cast<float32>(Settings->ScreenHeight));
        const auto fit_width = iround<int32>(screen_aspect <= back_buf_aspect ? numeric_cast<float32>(BackBufSize.height) * screen_aspect : numeric_cast<float32>(BackBufSize.height) * back_buf_aspect);
        const auto fit_height = iround<int32>(screen_aspect <= back_buf_aspect ? numeric_cast<float32>(BackBufSize.width) / back_buf_aspect : numeric_cast<float32>(BackBufSize.width) / screen_aspect);

        vp_ox = (BackBufSize.width - fit_width) / 2;
        vp_oy = (BackBufSize.height - fit_height) / 2;
        vp_width = fit_width;
        vp_height = fit_height;
        screen_width = Settings->ScreenWidth;
        screen_height = Settings->ScreenHeight;
    }

    D3DDeviceContext->OMSetRenderTargets(1, CurRenderTarget.get_pp(), CurDepthStencil.get());

    ViewPortRect = irect32 {vp_ox, vp_oy, vp_width, vp_height};

    ViewPort.Width = numeric_cast<FLOAT>(vp_width);
    ViewPort.Height = numeric_cast<FLOAT>(vp_height);
    ViewPort.MinDepth = 0.0f;
    ViewPort.MaxDepth = 1.0f;
    ViewPort.TopLeftX = numeric_cast<FLOAT>(vp_ox);
    ViewPort.TopLeftY = numeric_cast<FLOAT>(vp_oy);

    D3DDeviceContext->RSSetViewports(1, &ViewPort);

    ProjectionMatrixColMaj = CreateOrthoMatrix(0.0f, numeric_cast<float32>(screen_width), numeric_cast<float32>(screen_height), 0.0f, -10.0f, 10.0f);
    ProjectionMatrixColMaj.Transpose(); // Convert to column major order

    DisabledScissorRect.left = vp_ox;
    DisabledScissorRect.top = vp_oy;
    DisabledScissorRect.right = vp_ox + vp_width;
    DisabledScissorRect.bottom = vp_oy + vp_height;

    TargetSize = {screen_width, screen_height};
}

void Direct3D_Renderer::ClearRenderTarget(optional<ucolor> color, bool depth, bool stencil)
{
    FO_STACK_TRACE_ENTRY();

    if (color.has_value()) {
        const auto r = numeric_cast<float32>(color.value().comp.r) / 255.0f;
        const auto g = numeric_cast<float32>(color.value().comp.g) / 255.0f;
        const auto b = numeric_cast<float32>(color.value().comp.b) / 255.0f;
        const auto a = numeric_cast<float32>(color.value().comp.a) / 255.0f;
        const float32 color_rgba[] {r, g, b, a};

        D3DDeviceContext->ClearRenderTargetView(CurRenderTarget.get(), color_rgba);
    }

    if ((depth || stencil) && CurDepthStencil) {
        UINT clear_flags = 0;

        if (depth) {
            clear_flags |= D3D11_CLEAR_DEPTH;
        }
        if (stencil) {
            clear_flags |= D3D11_CLEAR_STENCIL;
        }

        D3DDeviceContext->ClearDepthStencilView(CurDepthStencil.get(), clear_flags, 1.0f, 0);
    }
}

void Direct3D_Renderer::EnableScissor(irect32 rect)
{
    FO_STACK_TRACE_ENTRY();

    if (ViewPortRect.width != TargetSize.width || ViewPortRect.height != TargetSize.height) {
        const float32 x_ratio = numeric_cast<float32>(ViewPortRect.width) / numeric_cast<float32>(TargetSize.width);
        const float32 y_ratio = numeric_cast<float32>(ViewPortRect.height) / numeric_cast<float32>(TargetSize.height);

        ScissorRect.left = ViewPortRect.x + iround<int32>(numeric_cast<float32>(rect.x) * x_ratio);
        ScissorRect.top = ViewPortRect.y + iround<int32>(numeric_cast<float32>(rect.y) * y_ratio);
        ScissorRect.right = ViewPortRect.x + iround<int32>(numeric_cast<float32>(rect.x + rect.width) * x_ratio);
        ScissorRect.bottom = ViewPortRect.y + iround<int32>(numeric_cast<float32>(rect.y + rect.height) * y_ratio);
    }
    else {
        ScissorRect.left = ViewPortRect.x + rect.x;
        ScissorRect.top = ViewPortRect.y + rect.y;
        ScissorRect.right = ViewPortRect.x + rect.x + rect.width;
        ScissorRect.bottom = ViewPortRect.y + rect.y + rect.height;
    }

    ScissorEnabled = true;
}

void Direct3D_Renderer::DisableScissor()
{
    FO_STACK_TRACE_ENTRY();

    ScissorEnabled = false;
}

void Direct3D_Renderer::OnResizeWindow(isize32 size)
{
    const auto is_cur_rt = CurRenderTarget == MainRenderTarget;

    if (is_cur_rt) {
        CurRenderTarget = nullptr;
        FO_RUNTIME_ASSERT(!CurDepthStencil);
        D3DDeviceContext->OMSetRenderTargets(1, CurRenderTarget.get_pp(), CurDepthStencil.get());
    }

    MainRenderTarget->Release();
    MainRenderTarget = nullptr;

    const auto d3d_resize_buffers = SwapChain->ResizeBuffers(0, size.width, size.height, DXGI_FORMAT_UNKNOWN, 0);
    FO_RUNTIME_ASSERT(SUCCEEDED(d3d_resize_buffers));

    ID3D11Texture2D* back_buf = nullptr;
    const auto d3d_get_back_buf = SwapChain->GetBuffer(0, IID_PPV_ARGS(&back_buf));
    FO_RUNTIME_ASSERT(SUCCEEDED(d3d_get_back_buf));
    FO_RUNTIME_ASSERT(back_buf);
    const auto d3d_create_back_buf_rt_view = D3DDevice->CreateRenderTargetView(back_buf, nullptr, MainRenderTarget.get_pp());
    FO_RUNTIME_ASSERT(SUCCEEDED(d3d_create_back_buf_rt_view));
    back_buf->Release();

    BackBufSize = size;

    if (is_cur_rt) {
        SetRenderTarget(nullptr);
    }
}

Direct3D_Texture::~Direct3D_Texture()
{
    FO_STACK_TRACE_ENTRY();

    if (TexHandle) {
        TexHandle->Release();
    }
    if (DepthStencil) {
        DepthStencil->Release();
    }
    if (RenderTargetView) {
        RenderTargetView->Release();
    }
    if (DepthStencilView) {
        DepthStencilView->Release();
    }
    if (ShaderTexView) {
        ShaderTexView->Release();
    }
}

auto Direct3D_Texture::GetTexturePixel(ipos32 pos) const -> ucolor
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(Size.is_valid_pos(pos));

    D3D11_BOX src_box;
    src_box.left = pos.x;
    src_box.top = pos.y;
    src_box.right = pos.x + 1;
    src_box.bottom = pos.y + 1;
    src_box.front = 0;
    src_box.back = 1;

    D3DDeviceContext->CopySubresourceRegion(OnePixStagingTex.get(), 0, 0, 0, 0, TexHandle.get_no_const(), 0, &src_box);

    D3D11_MAPPED_SUBRESOURCE tex_resource;
    const auto d3d_map_staging_texture = D3DDeviceContext->Map(OnePixStagingTex.get(), 0, D3D11_MAP_READ, 0, &tex_resource);
    FO_RUNTIME_ASSERT(SUCCEEDED(d3d_map_staging_texture));

    const auto result = *static_cast<ucolor*>(tex_resource.pData);

    D3DDeviceContext->Unmap(OnePixStagingTex.get(), 0);

    return result;
}

auto Direct3D_Texture::GetTextureRegion(ipos32 pos, isize32 size) const -> vector<ucolor>
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(size.width > 0);
    FO_RUNTIME_ASSERT(size.height > 0);
    FO_RUNTIME_ASSERT(pos.x >= 0);
    FO_RUNTIME_ASSERT(pos.y >= 0);
    FO_RUNTIME_ASSERT(pos.x + size.width <= Size.width);
    FO_RUNTIME_ASSERT(pos.y + size.height <= Size.height);

    vector<ucolor> result;
    result.resize(numeric_cast<size_t>(size.width) * size.height);

    D3D11_TEXTURE2D_DESC staging_desc;
    staging_desc.Width = size.width;
    staging_desc.Height = size.height;
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
    FO_RUNTIME_ASSERT(SUCCEEDED(d3d_create_staging_tex));

    D3D11_BOX src_box;
    src_box.left = pos.x;
    src_box.top = pos.y;
    src_box.right = pos.x + size.width;
    src_box.bottom = pos.y + size.height;
    src_box.front = 0;
    src_box.back = 1;

    D3DDeviceContext->CopySubresourceRegion(staging_tex, 0, 0, 0, 0, TexHandle.get_no_const(), 0, &src_box);

    D3D11_MAPPED_SUBRESOURCE tex_resource;
    const auto d3d_map_staging_texture = D3DDeviceContext->Map(staging_tex, 0, D3D11_MAP_READ, 0, &tex_resource);
    FO_RUNTIME_ASSERT(SUCCEEDED(d3d_map_staging_texture));

    for (int32 i = 0; i < size.height; i++) {
        const auto* src = static_cast<uint8*>(tex_resource.pData) + numeric_cast<size_t>(tex_resource.RowPitch) * i;
        MemCopy(&result[numeric_cast<size_t>(i) * size.width], src, numeric_cast<size_t>(size.width) * 4);
    }

    D3DDeviceContext->Unmap(staging_tex, 0);
    staging_tex->Release();

    return result;
}

void Direct3D_Texture::UpdateTextureRegion(ipos32 pos, isize32 size, const ucolor* data, bool use_dest_pitch)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(pos.x >= 0);
    FO_RUNTIME_ASSERT(pos.y >= 0);
    FO_RUNTIME_ASSERT(pos.x + size.width <= Size.width);
    FO_RUNTIME_ASSERT(pos.y + size.height <= Size.height);

    D3D11_BOX dest_box;
    dest_box.left = pos.x;
    dest_box.top = pos.y;
    dest_box.right = pos.x + size.width;
    dest_box.bottom = pos.y + size.height;
    dest_box.front = 0;
    dest_box.back = 1;

    const UINT src_pitch = (use_dest_pitch ? Size.width : size.width) * 4;

    D3DDeviceContext->UpdateSubresource(TexHandle.get_no_const(), 0, &dest_box, data, src_pitch, 0);
}

Direct3D_DrawBuffer::~Direct3D_DrawBuffer()
{
    FO_STACK_TRACE_ENTRY();

    if (VertexBuf) {
        VertexBuf->Release();
    }
    if (IndexBuf) {
        IndexBuf->Release();
    }
}

void Direct3D_DrawBuffer::Upload(EffectUsage usage, optional<size_t> custom_vertices_size, optional<size_t> custom_indices_size)
{
    FO_STACK_TRACE_ENTRY();

    if (IsStatic && !StaticDataChanged) {
        return;
    }

    StaticDataChanged = false;

    // Fill vertex buffer
    size_t upload_vertices;
    size_t vert_size;

#if FO_ENABLE_3D
    if (usage == EffectUsage::Model) {
        FO_RUNTIME_ASSERT(Vertices.empty());
        upload_vertices = custom_vertices_size.value_or(VertCount);
        vert_size = sizeof(Vertex3D);
    }
    else {
        FO_RUNTIME_ASSERT(Vertices3D.empty());
        upload_vertices = custom_vertices_size.value_or(VertCount);
        vert_size = sizeof(Vertex2D);
    }

#else
    ignore_unused(usage);
    upload_vertices = custom_vertices_size.value_or(VertCount);
    vert_size = sizeof(Vertex2D);
#endif

    if (VertexBuf == nullptr || upload_vertices > VertexBufSize) {
        if (VertexBuf) {
            VertexBuf->Release();
            VertexBuf = nullptr;
        }

        VertexBufSize = upload_vertices + 1024;

        D3D11_BUFFER_DESC vbuf_desc = {};
        vbuf_desc.Usage = D3D11_USAGE_DYNAMIC;
        vbuf_desc.ByteWidth = numeric_cast<UINT>(VertexBufSize * vert_size);
        vbuf_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbuf_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        vbuf_desc.MiscFlags = 0;

        const auto d3d_create_vertex_buffer = D3DDevice->CreateBuffer(&vbuf_desc, nullptr, VertexBuf.get_pp());
        FO_RUNTIME_ASSERT(SUCCEEDED(d3d_create_vertex_buffer));
    }

    D3D11_MAPPED_SUBRESOURCE vertices_resource;
    const auto d3d_map_vertex_buffer = D3DDeviceContext->Map(VertexBuf.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &vertices_resource);
    FO_RUNTIME_ASSERT(SUCCEEDED(d3d_map_vertex_buffer));

#if FO_ENABLE_3D
    if (usage == EffectUsage::Model) {
        MemCopy(vertices_resource.pData, Vertices3D.data(), upload_vertices * vert_size);
    }
    else {
        MemCopy(vertices_resource.pData, Vertices.data(), upload_vertices * vert_size);
    }
#else
    MemCopy(vertices_resource.pData, Vertices.data(), upload_vertices * vert_size);
#endif

    D3DDeviceContext->Unmap(VertexBuf.get(), 0);

    // Fill index buffer
    const auto upload_indices = custom_indices_size.value_or(IndCount);

    if (IndexBuf == nullptr || upload_indices > IndexBufSize) {
        if (IndexBuf) {
            IndexBuf->Release();
            IndexBuf = nullptr;
        }

        IndexBufSize = upload_indices + 1024;

        D3D11_BUFFER_DESC ibuf_desc = {};
        ibuf_desc.Usage = D3D11_USAGE_DYNAMIC;
        ibuf_desc.ByteWidth = numeric_cast<UINT>(IndexBufSize * sizeof(vindex_t));
        ibuf_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        ibuf_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        ibuf_desc.MiscFlags = 0;

        const auto d3d_create_index_buffer = D3DDevice->CreateBuffer(&ibuf_desc, nullptr, IndexBuf.get_pp());
        FO_RUNTIME_ASSERT(SUCCEEDED(d3d_create_index_buffer));
    }

    D3D11_MAPPED_SUBRESOURCE indices_resource;
    const auto d3d_map_index_buffer = D3DDeviceContext->Map(IndexBuf.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &indices_resource);
    FO_RUNTIME_ASSERT(SUCCEEDED(d3d_map_index_buffer));

    MemCopy(indices_resource.pData, Indices.data(), upload_indices * sizeof(vindex_t));

    D3DDeviceContext->Unmap(IndexBuf.get(), 0);
}

Direct3D_Effect::~Direct3D_Effect()
{
    FO_STACK_TRACE_ENTRY();

#define SAFE_RELEASE(ptr) \
    if (ptr) { \
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

void Direct3D_Effect::DrawBuffer(RenderDrawBuffer* dbuf, size_t start_index, optional<size_t> indices_to_draw, const RenderTexture* custom_tex)
{
    FO_STACK_TRACE_ENTRY();

    const auto* d3d_dbuf = static_cast<Direct3D_DrawBuffer*>(dbuf); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

#if FO_ENABLE_3D
    const auto* main_tex = static_cast<const Direct3D_Texture*>(custom_tex != nullptr ? custom_tex : (ModelTex[0] ? ModelTex[0].get() : (MainTex ? MainTex.get() : DummyTexture.get()))); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
#else
    const auto* main_tex = static_cast<const Direct3D_Texture*>(custom_tex != nullptr ? custom_tex : (MainTex ? MainTex.get() : DummyTexture.get())); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
#endif

    auto draw_mode = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    if (_usage == EffectUsage::Primitive) {
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
    const auto setup_cbuffer = [this](auto&& buf, auto&& buf_handle) {
        if (buf_handle == nullptr) {
            D3D11_BUFFER_DESC cbuf_desc = {};
            cbuf_desc.ByteWidth = sizeof(buf);
            cbuf_desc.Usage = D3D11_USAGE_DYNAMIC;
            cbuf_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            cbuf_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            cbuf_desc.MiscFlags = 0;

            const auto d3d_create_cbuffer = D3DDevice->CreateBuffer(&cbuf_desc, nullptr, buf_handle.get_pp());
            FO_RUNTIME_ASSERT(SUCCEEDED(d3d_create_cbuffer));
        }

        D3D11_MAPPED_SUBRESOURCE cbuffer_resource;
        const auto d3d_map_cbuffer = D3DDeviceContext->Map(buf_handle.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &cbuffer_resource);
        FO_RUNTIME_ASSERT(SUCCEEDED(d3d_map_cbuffer));

#if FO_ENABLE_3D
        if constexpr (std::same_as<std::decay_t<decltype(buf)>, ModelBuffer>) {
            const auto bind_size = sizeof(ModelBuffer) - (MODEL_MAX_BONES - MatrixCount) * sizeof(float32) * 16;
            MemCopy(cbuffer_resource.pData, &buf, bind_size);
        }
        else
#endif
        {
            ignore_unused(this);
            MemCopy(cbuffer_resource.pData, &buf, sizeof(buf));
        }

        D3DDeviceContext->Unmap(buf_handle.get(), 0);
    };

    if (_needProjBuf && !ProjBuf.has_value()) {
        auto& proj_buf = ProjBuf = ProjBuffer();
        MemCopy(proj_buf->ProjMatrix, ProjectionMatrixColMaj[0], 16 * sizeof(float32));
    }

    if (_needMainTexBuf && !MainTexBuf.has_value()) {
        auto& main_tex_buf = MainTexBuf = MainTexBuffer();
        MemCopy(main_tex_buf->MainTexSize, main_tex->SizeData, 4 * sizeof(float32));
    }

#define CBUF_UPLOAD_BUFFER(buf) \
    if (_need##buf && buf.has_value()) { \
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

    const auto* egg_tex = static_cast<const Direct3D_Texture*>(EggTex ? EggTex.get() : DummyTexture.get()); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    const auto draw_count = numeric_cast<UINT>(indices_to_draw.value_or(d3d_dbuf->IndCount));

    for (size_t pass = 0; pass < _passCount; pass++) {
#if FO_ENABLE_3D
        if (DisableShadow && _isShadow[pass]) {
            continue;
        }
#endif
        // Setup shader and vertex buffers
#if FO_ENABLE_3D
        UINT stride = _usage == EffectUsage::Model ? sizeof(Vertex3D) : sizeof(Vertex2D);
#else
        UINT stride = sizeof(Vertex2D);
#endif
        UINT offset = 0;

        D3DDeviceContext->IASetInputLayout(InputLayout[pass].get());
        D3DDeviceContext->IASetVertexBuffers(0, 1, d3d_dbuf->VertexBuf.get_pp(), &stride, &offset);

        if constexpr (sizeof(vindex_t) == 2) {
            D3DDeviceContext->IASetIndexBuffer(d3d_dbuf->IndexBuf.get_no_const(), DXGI_FORMAT_R16_UINT, 0);
        }
        else {
            D3DDeviceContext->IASetIndexBuffer(d3d_dbuf->IndexBuf.get_no_const(), DXGI_FORMAT_R32_UINT, 0);
        }

        D3DDeviceContext->IASetPrimitiveTopology(draw_mode);

        D3DDeviceContext->VSSetShader(VertexShader[pass].get(), nullptr, 0);
        D3DDeviceContext->PSSetShader(PixelShader[pass].get(), nullptr, 0);

#define CBUF_SET_BUFFER(buf) \
    if (_pos##buf[pass] != -1 && Cb_##buf) { \
        D3DDeviceContext->VSSetConstantBuffers(_pos##buf[pass], 1, Cb_##buf.get_pp()); \
        D3DDeviceContext->PSSetConstantBuffers(_pos##buf[pass], 1, Cb_##buf.get_pp()); \
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
                return LinearSampler.get_pp();
            }
            else {
                return PointSampler.get_pp();
            }
        };

        if (_posMainTex[pass] != -1) {
            D3DDeviceContext->PSSetShaderResources(_posMainTex[pass], 1, main_tex->ShaderTexView.get_pp());
            D3DDeviceContext->PSSetSamplers(_posMainTex[pass], 1, find_tex_sampler(main_tex));
        }

        if (_posEggTex[pass] != -1) {
            D3DDeviceContext->PSSetShaderResources(_posEggTex[pass], 1, egg_tex->ShaderTexView.get_pp());
            D3DDeviceContext->PSSetSamplers(_posEggTex[pass], 1, find_tex_sampler(egg_tex));
        }

#if FO_ENABLE_3D
        if (_needModelTex[pass]) {
            for (size_t i = 0; i < MODEL_MAX_TEXTURES; i++) {
                if (_posModelTex[pass][i] != -1) {
                    const auto* model_tex = static_cast<Direct3D_Texture*>(ModelTex[i] ? ModelTex[i].get() : DummyTexture.get()); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                    D3DDeviceContext->PSSetShaderResources(_posModelTex[pass][i], 1, model_tex->ShaderTexView.get_pp());
                    D3DDeviceContext->PSSetSamplers(_posModelTex[pass][i], 1, find_tex_sampler(model_tex));
                }
            }
        }
#endif

        constexpr float32 blend_factor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        D3DDeviceContext->OMSetBlendState(DisableBlending ? nullptr : BlendState[pass].get(), blend_factor, 0xFFFFFFFF);
        D3DDeviceContext->OMSetDepthStencilState(DepthStencilState[pass].get(), 0);

#if FO_ENABLE_3D
        D3DDeviceContext->RSSetState(DisableCulling ? RasterizerState[pass].get() : RasterizerState_Culling[pass].get());
#else
        D3DDeviceContext->RSSetState(RasterizerState[pass].get());
#endif
        D3DDeviceContext->RSSetScissorRects(1, ScissorEnabled ? &ScissorRect : &DisabledScissorRect);

        if (FeatureLevel <= D3D_FEATURE_LEVEL_9_3 && draw_mode == D3D11_PRIMITIVE_TOPOLOGY_POINTLIST) {
            FO_RUNTIME_ASSERT(start_index == 0);
            D3DDeviceContext->Draw(draw_count, 0);
        }
        else {
            D3DDeviceContext->DrawIndexed(draw_count, numeric_cast<UINT>(start_index), 0);
        }

        // Unbind
        constexpr ID3D11Buffer* null_buf = nullptr;
        constexpr ID3D11ShaderResourceView* null_res = nullptr;
        constexpr ID3D11SamplerState* null_sampler = nullptr;

#define CBUF_UNSET_BUFFER(buf) \
    if (_pos##buf[pass] != -1 && Cb_##buf) { \
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
        if (_needModelTex[pass]) {
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

        if constexpr (sizeof(vindex_t) == 2) {
            D3DDeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
        }
        else {
            D3DDeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
        }

        D3DDeviceContext->VSSetShader(nullptr, nullptr, 0);
        D3DDeviceContext->PSSetShader(nullptr, nullptr, 0);
    }
}

FO_END_NAMESPACE();

#endif
