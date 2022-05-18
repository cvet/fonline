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
#include "Log.h"
#include "StringUtils.h"
#include "WinApi-Include.h"

#include <d3d11_1.h>
#include <d3dcompiler.h>

class Direct3D_Texture : public RenderTexture
{
public:
    Direct3D_Texture(uint width, uint height, bool linear_filtered, bool with_depth) : RenderTexture(width, height, linear_filtered, with_depth) { }
    ~Direct3D_Texture() override;
    [[nodiscard]] auto GetTexturePixel(int x, int y) -> uint override;
    [[nodiscard]] auto GetTextureRegion(int x, int y, uint w, uint h) -> vector<uint> override;
    void UpdateTextureRegion(const IRect& r, const uint* data) override;

    ID3D11Texture2D* TexHandle {};
    ID3D11Texture2D* DepthStencil {};
    ID3D11RenderTargetView* RenderTargetView {};
    ID3D11DepthStencilView* DepthStencilView {};
};

class Direct3D_DrawBuffer : public RenderDrawBuffer
{
public:
    explicit Direct3D_DrawBuffer(bool is_static) : RenderDrawBuffer(is_static) { }
    ~Direct3D_DrawBuffer() override;

    int VertexBufferSize {};
    int IndexBufferSize {};
};

class Direct3D_Effect : public RenderEffect
{
    friend class Direct3D_Renderer;

public:
    Direct3D_Effect(EffectUsage usage, string_view name, string_view defines, const RenderEffectLoader& loader) : RenderEffect(usage, name, defines, loader) { }
    ~Direct3D_Effect() override;
    void DrawBuffer(RenderDrawBuffer* dbuf, size_t start_index = 0, optional<size_t> indices_to_draw = std::nullopt, RenderTexture* custom_tex = nullptr) override;

    ID3D11Buffer* VertexBuf {};
    ID3D11Buffer* IndexBuf {};
    ID3D11VertexShader* VertexShader {};
    ID3D11InputLayout* InputLayout {};
    // ID3D11Buffer* VertexConstantBuffer {};
    ID3D11PixelShader* PixelShader {};
    // ID3D11SamplerState* FontSampler {};
    // ID3D11ShaderResourceView* FontTextureView {};
    ID3D11RasterizerState* RasterizerState {};
    ID3D11BlendState* BlendState {};
    ID3D11DepthStencilState* DepthStencilState {};
};

static bool RenderDebug {};
static SDL_Window* SdlWindow {};
static ID3D11Device* D3DDevice {};
static ID3D11DeviceContext* D3DDeviceContext {};
static IDXGISwapChain* SwapChain {};
static ID3D11RenderTargetView* MainRenderTarget {};
static ID3D11RenderTargetView* CurRenderTarget {};
static ID3D11DepthStencilView* CurDepthStencil {};
static bool VSync {};
// IDXGIFactory* GiFactory;

static auto ConvertBlend(BlendFuncType blend) -> D3D11_BLEND
{
    switch (blend) {
    case BlendFuncType::Zero:
        return D3D11_BLEND_ZERO;
    case BlendFuncType::One:
        return D3D11_BLEND_ONE;
    case BlendFuncType::SrcColor:
        return D3D11_BLEND_SRC_COLOR;
    case BlendFuncType::InvSrcColor:
        return D3D11_BLEND_INV_SRC_COLOR;
    case BlendFuncType::DstColor:
        return D3D11_BLEND_DEST_COLOR;
    case BlendFuncType::InvDstColor:
        return D3D11_BLEND_INV_DEST_COLOR;
    case BlendFuncType::SrcAlpha:
        return D3D11_BLEND_SRC_ALPHA;
    case BlendFuncType::InvSrcAlpha:
        return D3D11_BLEND_INV_SRC_ALPHA;
    case BlendFuncType::DstAlpha:
        return D3D11_BLEND_DEST_ALPHA;
    case BlendFuncType::InvDstAlpha:
        return D3D11_BLEND_INV_DEST_ALPHA;
    case BlendFuncType::ConstantColor:
        return D3D11_BLEND_SRC1_COLOR;
    case BlendFuncType::InvConstantColor:
        return D3D11_BLEND_INV_SRC1_COLOR;
    case BlendFuncType::SrcAlphaSaturate:
        return D3D11_BLEND_SRC_ALPHA_SAT;
    }
    throw UnreachablePlaceException(LINE_STR);
}

static auto ConvertBlendOp(BlendEquationType blend_op) -> D3D11_BLEND_OP
{
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
        const auto create_texure_depth = D3DDevice->CreateTexture2D(&desc, nullptr, &d3d_tex->DepthStencil);
        RUNTIME_ASSERT(create_texure_depth == S_OK);

        const auto create_depth_view = D3DDevice->CreateDepthStencilView(d3d_tex->DepthStencil, NULL, &d3d_tex->DepthStencilView);
        RUNTIME_ASSERT(create_depth_view == S_OK);
    }

    const auto create_tex_rt = D3DDevice->CreateRenderTargetView(d3d_tex->TexHandle, NULL, &d3d_tex->RenderTargetView);
    RUNTIME_ASSERT(create_tex_rt == S_OK);

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
        // Create the vertex shader
        {
            const string vertex_shader_fname = _str("{}.{}.vert.hlsl", name, pass + 1);
            string vertex_shader_content = loader(vertex_shader_fname);
            RUNTIME_ASSERT(!vertex_shader_content.empty());

            // Todo: pass additional defines to shaders (passed + internal)

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

            if (FAILED(::D3DCompile(vertex_shader_content.c_str(), vertex_shader_content.length(), NULL, NULL, NULL, "main", "vs_3_0", 0, 0, &vertex_shader_blob, &error_blob))) {
                const string error = static_cast<const char*>(error_blob->GetBufferPointer());
                throw EffectLoadException("Failed to compile Vertex Shader", vertex_shader_fname, vertex_shader_content, error);
            }

            if (D3DDevice->CreateVertexShader(vertex_shader_blob->GetBufferPointer(), vertex_shader_blob->GetBufferSize(), NULL, &d3d_effect->VertexShader) != S_OK) {
                throw EffectLoadException("Failed to create Vertex Shader from binary", vertex_shader_fname, vertex_shader_content);
            }

            // Create the input layout
#if FO_ENABLE_3D
            if (usage == EffectUsage::Model) {
                static_assert(BONES_PER_VERTEX == 4);
                const D3D11_INPUT_ELEMENT_DESC local_layout[] = {
                    {"TEXCOORD0", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(Vertex3D, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD1", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(Vertex3D, Normal)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD2", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(Vertex3D, TexCoord)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD3", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(Vertex3D, TexCoordBase)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD4", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(Vertex3D, Tangent)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD5", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(Vertex3D, Bitangent)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD6", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(Vertex3D, BlendWeights)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD7", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(Vertex3D, BlendIndices)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                };
                if (D3DDevice->CreateInputLayout(local_layout, 8, vertex_shader_blob->GetBufferPointer(), vertex_shader_blob->GetBufferSize(), &d3d_effect->InputLayout) != S_OK) {
                    throw EffectLoadException("Failed to create Vertex Shader 3D layout", vertex_shader_fname, vertex_shader_content);
                }
            }
            else
#endif
            {
                const D3D11_INPUT_ELEMENT_DESC local_layout[] = {
                    {"TEXCOORD0", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(Vertex2D, X)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD1", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, static_cast<UINT>(offsetof(Vertex2D, Diffuse)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD2", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(Vertex2D, TU)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                    {"TEXCOORD3", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(Vertex2D, TUEgg)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                };
                if (D3DDevice->CreateInputLayout(local_layout, 4, vertex_shader_blob->GetBufferPointer(), vertex_shader_blob->GetBufferSize(), &d3d_effect->InputLayout) != S_OK) {
                    throw EffectLoadException("Failed to create Vertex Shader 2D layout", vertex_shader_fname, vertex_shader_content);
                }
            }

            // Create the constant buffer
            {
                // D3D11_BUFFER_DESC desc;
                // desc.ByteWidth = sizeof(VERTEX_CONSTANT_BUFFER);
                // desc.Usage = D3D11_USAGE_DYNAMIC;
                // desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
                // desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                // desc.MiscFlags = 0;
                // D3DDevice->CreateBuffer(&desc, NULL, &d3d_effect->VertexConstantBuffer);
            }
        }

        // Create the pixel shader
        {
            const string pixel_shader_fname = _str("{}.{}.frag.hlsl", name, pass + 1);
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

            if (FAILED(::D3DCompile(pixel_shader_content.c_str(), pixel_shader_content.length(), NULL, NULL, NULL, "main", "ps_3_0", 0, 0, &pixel_shader_blob, &error_blob))) {
                const string error = static_cast<const char*>(error_blob->GetBufferPointer());
                throw EffectLoadException("Failed to compile Pixel Shader", pixel_shader_fname, pixel_shader_content, error);
            }
            if (D3DDevice->CreatePixelShader(pixel_shader_blob->GetBufferPointer(), pixel_shader_blob->GetBufferSize(), NULL, &d3d_effect->PixelShader) != S_OK) {
                throw EffectLoadException("Failed to create Pixel Shader from binary", pixel_shader_fname, pixel_shader_content);
            }
        }

        // Create the blending setup
        {
            D3D11_BLEND_DESC desc = {};
            desc.AlphaToCoverageEnable = FALSE;
            desc.RenderTarget[0].BlendEnable = TRUE;
            desc.RenderTarget[0].SrcBlend = ConvertBlend(d3d_effect->_srcBlendFunc[pass]);
            desc.RenderTarget[0].DestBlend = ConvertBlend(d3d_effect->_destBlendFunc[pass]);
            desc.RenderTarget[0].BlendOp = ConvertBlendOp(d3d_effect->_blendEquation[pass]);
            desc.RenderTarget[0].SrcBlendAlpha = ConvertBlend(d3d_effect->_srcBlendFunc[pass]);
            desc.RenderTarget[0].DestBlendAlpha = ConvertBlend(d3d_effect->_destBlendFunc[pass]);
            desc.RenderTarget[0].BlendOpAlpha = ConvertBlendOp(d3d_effect->_blendEquation[pass]);
            desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            if (D3DDevice->CreateBlendState(&desc, &d3d_effect->BlendState) != S_OK) {
                throw EffectLoadException("Failed to call CreateBlendState", name);
            }
        }

        // Create the rasterizer state
        {
            D3D11_RASTERIZER_DESC desc = {};
            desc.FillMode = D3D11_FILL_SOLID;
            desc.CullMode = D3D11_CULL_NONE;
            desc.ScissorEnable = TRUE;
            desc.DepthClipEnable = TRUE;
            if (D3DDevice->CreateRasterizerState(&desc, &d3d_effect->RasterizerState) != S_OK) {
                throw EffectLoadException("Failed to call CreateRasterizerState", name);
            }
        }

        // Create depth-stencil State
        {
            D3D11_DEPTH_STENCIL_DESC desc = {};
            desc.DepthEnable = FALSE;
            desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
            desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
            desc.StencilEnable = FALSE;
            desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
            desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
            desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
            desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
            desc.BackFace = desc.FrontFace;
            if (D3DDevice->CreateDepthStencilState(&desc, &d3d_effect->DepthStencilState) != S_OK) {
                throw EffectLoadException("Failed to call CreateDepthStencilState", name);
            }
        }
    }

    return d3d_effect.release();
}

void Direct3D_Renderer::SetRenderTarget(RenderTexture* tex)
{
    if (tex != nullptr) {
        const auto* d3d_tex = static_cast<Direct3D_Texture*>(tex);
        CurRenderTarget = d3d_tex->RenderTargetView;
        CurDepthStencil = d3d_tex->DepthStencilView;
    }
    else {
        CurRenderTarget = MainRenderTarget;
        CurDepthStencil = nullptr;
    }

    D3DDeviceContext->OMSetRenderTargets(1, &CurRenderTarget, CurDepthStencil);
}

void Direct3D_Renderer::ClearRenderTarget(optional<uint> color, bool depth, bool stencil)
{
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

void Direct3D_Renderer::EnableScissor(int x, int y, uint w, uint h)
{
    throw NotImplementedException(LINE_STR);
}

void Direct3D_Renderer::DisableScissor()
{
    throw NotImplementedException(LINE_STR);
}

Direct3D_Texture::~Direct3D_Texture()
{
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
}

auto Direct3D_Texture::GetTexturePixel(int x, int y) -> uint
{
    throw NotImplementedException(LINE_STR);
    return 0;
}

auto Direct3D_Texture::GetTextureRegion(int x, int y, uint w, uint h) -> vector<uint>
{
    RUNTIME_ASSERT(w && h);
    const auto size = w * h;
    vector<uint> result(size);

    throw NotImplementedException(LINE_STR);

    return result;
}

void Direct3D_Texture::UpdateTextureRegion(const IRect& r, const uint* data)
{
    throw NotImplementedException(LINE_STR);
    // D3DDeviceContext->UpdateSubresource()
}

Direct3D_DrawBuffer::~Direct3D_DrawBuffer()
{
}

Direct3D_Effect::~Direct3D_Effect()
{
    if (VertexBuf != nullptr) {
        VertexBuf->Release();
    }
    if (IndexBuf != nullptr) {
        IndexBuf->Release();
    }
    if (VertexShader != nullptr) {
        VertexShader->Release();
    }
    if (InputLayout != nullptr) {
        InputLayout->Release();
    }
    if (PixelShader != nullptr) {
        PixelShader->Release();
    }
    if (RasterizerState != nullptr) {
        RasterizerState->Release();
    }
    if (BlendState != nullptr) {
        BlendState->Release();
    }
    if (DepthStencilState != nullptr) {
        DepthStencilState->Release();
    }
}

void Direct3D_Effect::DrawBuffer(RenderDrawBuffer* dbuf, size_t start_index, optional<size_t> indices_to_draw, RenderTexture* custom_tex)
{
    throw NotImplementedException(LINE_STR);
}

#endif
