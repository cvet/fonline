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

#include "Rendering.h"

#if FO_HAVE_SDL_GPU

#include "Application.h"
#include "ConfigFile.h"

#include "SDL3/SDL_gpu.h"
#include "SDL3/SDL_video.h"

FO_BEGIN_NAMESPACE

// SDL_GPU works with explicit render/copy passes recorded into a per-frame command buffer, while the engine
// renderer contract is immediate-mode (interleaved target switches, clears, uploads, readbacks, draws).
// The backend therefore keeps a small pass state machine in its Context: at most one render or copy pass is
// open at a time, passes begin lazily right before the operation that needs them, clears are deferred into the
// next render pass load-op, and readbacks flush the recorded work with a fence wait. The window backbuffer is
// never rendered to directly: all backbuffer output goes to an RGBA8 proxy texture that Present() blits to the
// acquired swapchain texture, which keeps mid-frame flushes safe and pipeline target formats uniform.

class SDLGpu_Texture final : public RenderTexture
{
public:
    SDLGpu_Texture(isize32 size, bool linear_filtered, bool with_depth, ptr<SDLGpu_Renderer::Context> ctx) :
        RenderTexture(size, linear_filtered, with_depth),
        _ctx {ctx}
    {
    }
    SDLGpu_Texture(const SDLGpu_Texture&) = delete;
    SDLGpu_Texture(SDLGpu_Texture&&) noexcept = delete;
    auto operator=(const SDLGpu_Texture&) -> SDLGpu_Texture& = delete;
    auto operator=(SDLGpu_Texture&&) noexcept -> SDLGpu_Texture& = delete;
    ~SDLGpu_Texture() override;

    [[nodiscard]] auto GetTexturePixel(ipos32 pos) const -> ucolor override;
    [[nodiscard]] auto GetTextureRegion(ipos32 pos, isize32 size) const -> vector<ucolor> override;
    void UpdateTextureRegion(ipos32 pos, isize32 size, const_span<ucolor> data, bool use_dest_pitch) override;

    nptr<SDL_GPUTexture> TexHandle {};
    nptr<SDL_GPUTexture> DepthTexHandle {};

private:
    ptr<SDLGpu_Renderer::Context> _ctx;
};

class SDLGpu_DrawBuffer final : public RenderDrawBuffer
{
public:
    SDLGpu_DrawBuffer(bool is_static, ptr<SDLGpu_Renderer::Context> ctx) :
        RenderDrawBuffer(is_static),
        _ctx {ctx}
    {
    }
    SDLGpu_DrawBuffer(const SDLGpu_DrawBuffer&) = delete;
    SDLGpu_DrawBuffer(SDLGpu_DrawBuffer&&) noexcept = delete;
    auto operator=(const SDLGpu_DrawBuffer&) -> SDLGpu_DrawBuffer& = delete;
    auto operator=(SDLGpu_DrawBuffer&&) noexcept -> SDLGpu_DrawBuffer& = delete;
    ~SDLGpu_DrawBuffer() override;

    void Upload(EffectUsage usage, optional<size_t> custom_vertices_size, optional<size_t> custom_indices_size) override;

    nptr<SDL_GPUBuffer> VertexBuf {};
    nptr<SDL_GPUBuffer> IndexBuf {};
    size_t VertexBufSize {};
    size_t IndexBufSize {};
    nptr<SDL_GPUTransferBuffer> TransferBuf {};
    size_t TransferBufSize {};

private:
    ptr<SDLGpu_Renderer::Context> _ctx;
};

class SDLGpu_Effect final : public RenderEffect
{
    friend class SDLGpu_Renderer;

public:
    // Per-stage SDL_GPU binding slots and resource counts of one pass, parsed from the baked [EffectInfoSdl] section
    struct SdlPassSlots
    {
        uint32_t VertSamplers {};
        uint32_t VertUniformBufs {};
        uint32_t FragSamplers {};
        uint32_t FragUniformBufs {};

        int32_t VertMainTex {-1};
        int32_t FragMainTex {-1};
        int32_t VertIndoorMaskTex {-1};
        int32_t FragIndoorMaskTex {-1};

        int32_t VertProjBuf {-1};
        int32_t FragProjBuf {-1};
        int32_t VertMainTexBuf {-1};
        int32_t FragMainTexBuf {-1};
        int32_t VertEggBuf {-1};
        int32_t FragEggBuf {-1};
        int32_t VertSpriteBorderBuf {-1};
        int32_t FragSpriteBorderBuf {-1};
        int32_t VertTimeBuf {-1};
        int32_t FragTimeBuf {-1};
        int32_t VertRandomValueBuf {-1};
        int32_t FragRandomValueBuf {-1};
        int32_t VertScriptValueBuf {-1};
        int32_t FragScriptValueBuf {-1};
        int32_t VertCameraBuf {-1};
        int32_t FragCameraBuf {-1};
#if FO_ENABLE_3D
        int32_t VertModelBuf {-1};
        int32_t FragModelBuf {-1};
        int32_t VertModelTexBuf {-1};
        int32_t FragModelTexBuf {-1};
        int32_t VertModelAnimBuf {-1};
        int32_t FragModelAnimBuf {-1};
        int32_t VertModelTex[MODEL_MAX_TEXTURES] {};
        int32_t FragModelTex[MODEL_MAX_TEXTURES] {};
#endif
    };

    SDLGpu_Effect(EffectUsage usage, string_view name, const RenderEffectLoader& loader, ptr<SDLGpu_Renderer::Context> ctx) :
        RenderEffect(usage, name, loader),
        _ctx {ctx}
    {
    }
    SDLGpu_Effect(const SDLGpu_Effect&) = delete;
    SDLGpu_Effect(SDLGpu_Effect&&) noexcept = delete;
    auto operator=(const SDLGpu_Effect&) -> SDLGpu_Effect& = delete;
    auto operator=(SDLGpu_Effect&&) noexcept -> SDLGpu_Effect& = delete;
    ~SDLGpu_Effect() override;

    void DrawBuffer(ptr<RenderDrawBuffer> dbuf, size_t start_index, optional<size_t> indices_to_draw, nptr<const RenderTexture> custom_tex) override;

    nptr<SDL_GPUShader> VertShader[EFFECT_MAX_PASSES] {};
    nptr<SDL_GPUShader> FragShader[EFFECT_MAX_PASSES] {};
    SdlPassSlots PassSlots[EFFECT_MAX_PASSES] {};

private:
    [[nodiscard]] auto GetOrCreatePipeline(size_t pass, SDL_GPUPrimitiveType topology, bool with_depth) -> ptr<SDL_GPUGraphicsPipeline>;

    ptr<SDLGpu_Renderer::Context> _ctx;
    unordered_map<uint32_t, nptr<SDL_GPUGraphicsPipeline>> _pipelines {};
};

struct SDLGpu_Renderer::Context
{
    nptr<GlobalSettings> Settings {};
    bool VSync {};
    nptr<SDL_Window> SdlWindow {};
    nptr<SDL_GPUDevice> Device {};
    SDL_GPUShaderFormat ShaderFormat {};
    SDL_GPUTextureFormat DepthFormat {};
    nptr<SDL_GPUCommandBuffer> CmdBuf {};
    nptr<SDL_GPURenderPass> RenderPass {};
    nptr<SDL_GPUCopyPass> CopyPass {};
    nptr<SDLGpu_Texture> CurRenderTarget {}; // Null means the backbuffer proxy
    nptr<SDL_GPUTexture> BackbufferProxyTex {};
    nptr<SDL_GPUSampler> PointSampler {};
    nptr<SDL_GPUSampler> LinearSampler {};
    nptr<SDL_GPUTransferBuffer> UploadTransferBuf {};
    size_t UploadTransferBufSize {};
    nptr<SDL_GPUTransferBuffer> DownloadTransferBuf {};
    size_t DownloadTransferBufSize {};
    unique_nptr<RenderTexture> DummyTexture {};
    optional<ucolor> PendingClearColor {};
    bool PendingClearDepth {};
    mat44 ProjMatrix {};
    float32_t OrthoNear {ORTHO_DEPTH_DEFAULT_NEAR};
    float32_t OrthoFar {ORTHO_DEPTH_DEFAULT_FAR};
    bool ScissorEnabled {};
    SDL_Rect ScissorRect {};
    SDL_Rect DisabledScissorRect {};
    SDL_GPUViewport ViewPort {};
    irect32 ViewPortRect {};
    isize32 BackBufSize {};
    isize32 TargetSize {};
};

template<typename T, typename U>
static auto RenderBackendCast(ptr<U> value) -> ptr<T>
{
    FO_STACK_TRACE_ENTRY();

    auto nullable_casted = value.template cast<T>();
    FO_VERIFY_AND_THROW(nullable_casted, "Render backend object is not of the expected SDL_GPU type");
    auto casted = nullable_casted.as_ptr();
    return casted;
}

static auto GetSdlWindow(nptr<WindowInternalHandle> window) -> ptr<SDL_Window>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(window, "Window handle is null");
    return cast_from_void<SDL_Window*>(window.get());
}

static auto GetDummyTexture(ptr<SDLGpu_Renderer::Context> ctx) -> ptr<RenderTexture>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(ctx->DummyTexture, "SDL_GPU dummy texture is not created");
    auto dummy_texture = ctx->DummyTexture.as_ptr();
    return dummy_texture;
}

static auto GetContentBytes(const string& content) -> ptr<const uint8_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!content.empty(), "Content is unexpectedly empty");
    const nptr<const void> nullable_content_data = content.data();
    auto content_data = nullable_content_data.as_ptr();
    return cast_from_void<const uint8_t*>(content_data.get());
}

static auto ConvertClearColor(ucolor color) -> SDL_FColor
{
    FO_NO_STACK_TRACE_ENTRY();

    SDL_FColor fcolor;
    fcolor.r = numeric_cast<float32_t>(color.comp.r) / 255.0f;
    fcolor.g = numeric_cast<float32_t>(color.comp.g) / 255.0f;
    fcolor.b = numeric_cast<float32_t>(color.comp.b) / 255.0f;
    fcolor.a = numeric_cast<float32_t>(color.comp.a) / 255.0f;
    return fcolor;
}

static auto ConvertBlendFactor(BlendFuncType blend, bool is_alpha) -> SDL_GPUBlendFactor
{
    FO_STACK_TRACE_ENTRY();

    switch (blend) {
    case BlendFuncType::Zero:
        return SDL_GPU_BLENDFACTOR_ZERO;
    case BlendFuncType::One:
        return SDL_GPU_BLENDFACTOR_ONE;
    case BlendFuncType::SrcColor:
        return !is_alpha ? SDL_GPU_BLENDFACTOR_SRC_COLOR : SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    case BlendFuncType::InvSrcColor:
        return !is_alpha ? SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR : SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    case BlendFuncType::DstColor:
        return !is_alpha ? SDL_GPU_BLENDFACTOR_DST_COLOR : SDL_GPU_BLENDFACTOR_DST_ALPHA;
    case BlendFuncType::InvDstColor:
        return !is_alpha ? SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_COLOR : SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA;
    case BlendFuncType::SrcAlpha:
        return SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    case BlendFuncType::InvSrcAlpha:
        return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    case BlendFuncType::DstAlpha:
        return SDL_GPU_BLENDFACTOR_DST_ALPHA;
    case BlendFuncType::InvDstAlpha:
        return SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA;
    case BlendFuncType::ConstantColor:
        return SDL_GPU_BLENDFACTOR_CONSTANT_COLOR;
    case BlendFuncType::InvConstantColor:
        return SDL_GPU_BLENDFACTOR_ONE_MINUS_CONSTANT_COLOR;
    case BlendFuncType::SrcAlphaSaturate:
        return SDL_GPU_BLENDFACTOR_SRC_ALPHA_SATURATE;
    }

    FO_UNREACHABLE_PLACE();
}

static auto ConvertBlendOp(BlendEquationType blend_op) -> SDL_GPUBlendOp
{
    FO_STACK_TRACE_ENTRY();

    switch (blend_op) {
    case BlendEquationType::FuncAdd:
        return SDL_GPU_BLENDOP_ADD;
    case BlendEquationType::FuncSubtract:
        return SDL_GPU_BLENDOP_SUBTRACT;
    case BlendEquationType::FuncReverseSubtract:
        return SDL_GPU_BLENDOP_REVERSE_SUBTRACT;
    case BlendEquationType::Max:
        return SDL_GPU_BLENDOP_MAX;
    case BlendEquationType::Min:
        return SDL_GPU_BLENDOP_MIN;
    }

    FO_UNREACHABLE_PLACE();
}

static auto ConvertCompareOp(DepthFuncType depth_func) -> SDL_GPUCompareOp
{
    FO_STACK_TRACE_ENTRY();

    switch (depth_func) {
    case DepthFuncType::Always:
        return SDL_GPU_COMPAREOP_ALWAYS;
    case DepthFuncType::Never:
        return SDL_GPU_COMPAREOP_NEVER;
    case DepthFuncType::Less:
        return SDL_GPU_COMPAREOP_LESS;
    case DepthFuncType::LessEqual:
        return SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    case DepthFuncType::Equal:
        return SDL_GPU_COMPAREOP_EQUAL;
    case DepthFuncType::GreaterEqual:
        return SDL_GPU_COMPAREOP_GREATER_OR_EQUAL;
    case DepthFuncType::Greater:
        return SDL_GPU_COMPAREOP_GREATER;
    case DepthFuncType::NotEqual:
        return SDL_GPU_COMPAREOP_NOT_EQUAL;
    }

    FO_UNREACHABLE_PLACE();
}

static auto ConvertPrimitiveType(RenderPrimitiveType prim_type) -> SDL_GPUPrimitiveType
{
    FO_STACK_TRACE_ENTRY();

    switch (prim_type) {
    case RenderPrimitiveType::PointList:
        // The effect vertex shaders do not write gl_PointSize, so point-list is unusable on the Vulkan driver
        // (and SPIRV-Cross cannot emit it for the other flavors). Mirror Rendering-Vulkan and remap to triangle-list;
        // point primitives are unused by content.
        return SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    case RenderPrimitiveType::LineList:
        return SDL_GPU_PRIMITIVETYPE_LINELIST;
    case RenderPrimitiveType::LineStrip:
        return SDL_GPU_PRIMITIVETYPE_LINESTRIP;
    case RenderPrimitiveType::TriangleList:
        return SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    case RenderPrimitiveType::TriangleStrip:
        return SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP;
    }

    FO_UNREACHABLE_PLACE();
}

static auto GetTargetTexHandle(ptr<SDLGpu_Renderer::Context> ctx) -> ptr<SDL_GPUTexture>
{
    FO_STACK_TRACE_ENTRY();

    if (ctx->CurRenderTarget) {
        FO_VERIFY_AND_THROW(ctx->CurRenderTarget->TexHandle, "SDL_GPU render target texture handle is null");
        auto target_tex = ctx->CurRenderTarget->TexHandle.as_ptr();
        return target_tex;
    }

    FO_VERIFY_AND_THROW(ctx->BackbufferProxyTex, "SDL_GPU backbuffer proxy texture is not created");
    auto proxy_tex = ctx->BackbufferProxyTex.as_ptr();
    return proxy_tex;
}

static auto IsTargetWithDepth(ptr<SDLGpu_Renderer::Context> ctx) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    return ctx->CurRenderTarget && ctx->CurRenderTarget->WithDepth;
}

static void EnsureCmdBuf(ptr<SDLGpu_Renderer::Context> ctx)
{
    FO_STACK_TRACE_ENTRY();

    if (ctx->CmdBuf) {
        return;
    }

    ctx->CmdBuf = SDL_AcquireGPUCommandBuffer(ctx->Device.get());
    FO_VERIFY_AND_THROW(ctx->CmdBuf, "SDL_AcquireGPUCommandBuffer failed", SDL_GetError());
}

static void EndAnyPass(ptr<SDLGpu_Renderer::Context> ctx)
{
    FO_STACK_TRACE_ENTRY();

    if (ctx->RenderPass) {
        SDL_EndGPURenderPass(ctx->RenderPass.get());
        ctx->RenderPass = nullptr;
    }
    if (ctx->CopyPass) {
        SDL_EndGPUCopyPass(ctx->CopyPass.get());
        ctx->CopyPass = nullptr;
    }
}

static auto EnsureRenderPass(ptr<SDLGpu_Renderer::Context> ctx) -> ptr<SDL_GPURenderPass>
{
    FO_STACK_TRACE_ENTRY();

    if (ctx->RenderPass) {
        auto open_render_pass = ctx->RenderPass.as_ptr();
        return open_render_pass;
    }

    EnsureCmdBuf(ctx);

    if (ctx->CopyPass) {
        SDL_EndGPUCopyPass(ctx->CopyPass.get());
        ctx->CopyPass = nullptr;
    }

    SDL_GPUColorTargetInfo color_target = {};
    color_target.texture = GetTargetTexHandle(ctx).get();
    color_target.load_op = ctx->PendingClearColor.has_value() ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
    color_target.store_op = SDL_GPU_STOREOP_STORE;

    if (ctx->PendingClearColor.has_value()) {
        color_target.clear_color = ConvertClearColor(ctx->PendingClearColor.value());
    }

    SDL_GPUDepthStencilTargetInfo depth_target = {};
    const bool with_depth = IsTargetWithDepth(ctx);

    if (with_depth) {
        FO_VERIFY_AND_THROW(ctx->CurRenderTarget->DepthTexHandle, "SDL_GPU render target depth texture handle is null");
        depth_target.texture = ctx->CurRenderTarget->DepthTexHandle.get();
        depth_target.load_op = ctx->PendingClearDepth ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
        depth_target.store_op = SDL_GPU_STOREOP_STORE;
        depth_target.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        depth_target.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        depth_target.clear_depth = 1.0f;
    }

    ctx->PendingClearColor.reset();
    ctx->PendingClearDepth = false;

    ctx->RenderPass = SDL_BeginGPURenderPass(ctx->CmdBuf.get(), &color_target, 1, with_depth ? &depth_target : nullptr);
    FO_VERIFY_AND_THROW(ctx->RenderPass, "SDL_BeginGPURenderPass failed", SDL_GetError());

    // Render state resets at pass end, so re-apply the tracked viewport and scissor on every pass begin
    SDL_SetGPUViewport(ctx->RenderPass.get(), &ctx->ViewPort);
    SDL_SetGPUScissor(ctx->RenderPass.get(), ctx->ScissorEnabled ? &ctx->ScissorRect : &ctx->DisabledScissorRect);

    auto render_pass = ctx->RenderPass.as_ptr();
    return render_pass;
}

static auto EnsureCopyPass(ptr<SDLGpu_Renderer::Context> ctx) -> ptr<SDL_GPUCopyPass>
{
    FO_STACK_TRACE_ENTRY();

    if (ctx->CopyPass) {
        auto open_copy_pass = ctx->CopyPass.as_ptr();
        return open_copy_pass;
    }

    EnsureCmdBuf(ctx);

    if (ctx->RenderPass) {
        SDL_EndGPURenderPass(ctx->RenderPass.get());
        ctx->RenderPass = nullptr;
    }

    ctx->CopyPass = SDL_BeginGPUCopyPass(ctx->CmdBuf.get());
    FO_VERIFY_AND_THROW(ctx->CopyPass, "SDL_BeginGPUCopyPass failed", SDL_GetError());

    auto copy_pass = ctx->CopyPass.as_ptr();
    return copy_pass;
}

// Materialize deferred clears of the current target before leaving it (target switch, present, flush):
// a clear with no subsequent draw still must reach the texture, so run a clear-only render pass
static void FlushPendingClears(ptr<SDLGpu_Renderer::Context> ctx)
{
    FO_STACK_TRACE_ENTRY();

    if (ctx->PendingClearColor.has_value() || ctx->PendingClearDepth) {
        (void)EnsureRenderPass(ctx);
    }

    EndAnyPass(ctx);
}

// Submit everything recorded so far and block until the GPU has finished it (used before texture readbacks)
static void SubmitAndWait(ptr<SDLGpu_Renderer::Context> ctx)
{
    FO_STACK_TRACE_ENTRY();

    FlushPendingClears(ctx);

    if (!ctx->CmdBuf) {
        return;
    }

    nptr<SDL_GPUFence> nullable_fence = SDL_SubmitGPUCommandBufferAndAcquireFence(ctx->CmdBuf.get());
    ctx->CmdBuf = nullptr;
    FO_VERIFY_AND_THROW(nullable_fence, "SDL_SubmitGPUCommandBufferAndAcquireFence failed", SDL_GetError());
    auto fence = nullable_fence.as_ptr();

    SDL_GPUFence* fence_handles[] = {fence.get()};
    const bool wait_ok = SDL_WaitForGPUFences(ctx->Device.get(), true, fence_handles, 1);
    SDL_ReleaseGPUFence(ctx->Device.get(), fence.get());
    FO_VERIFY_AND_THROW(wait_ok, "SDL_WaitForGPUFences failed", SDL_GetError());
}

static auto EnsureTransferBuffer(ptr<SDLGpu_Renderer::Context> ctx, nptr<SDL_GPUTransferBuffer>& transfer_buf, size_t& transfer_buf_size, size_t required_size, bool download) -> ptr<SDL_GPUTransferBuffer>
{
    FO_STACK_TRACE_ENTRY();

    if (!transfer_buf || required_size > transfer_buf_size) {
        if (transfer_buf) {
            SDL_ReleaseGPUTransferBuffer(ctx->Device.get(), transfer_buf.get());
            transfer_buf = nullptr;
        }

        transfer_buf_size = required_size + required_size / 2;

        SDL_GPUTransferBufferCreateInfo transfer_buf_info = {};
        transfer_buf_info.usage = download ? SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD : SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transfer_buf_info.size = numeric_cast<uint32_t>(transfer_buf_size);

        transfer_buf = SDL_CreateGPUTransferBuffer(ctx->Device.get(), &transfer_buf_info);
        FO_VERIFY_AND_THROW(transfer_buf, "SDL_CreateGPUTransferBuffer failed", SDL_GetError(), transfer_buf_size, download);
    }

    auto existing_transfer_buf = transfer_buf.as_ptr();
    return existing_transfer_buf;
}

static auto MapTransferBuffer(ptr<SDLGpu_Renderer::Context> ctx, ptr<SDL_GPUTransferBuffer> transfer_buf, bool cycle) -> ptr<void>
{
    FO_STACK_TRACE_ENTRY();

    nptr<void> nullable_mapped = SDL_MapGPUTransferBuffer(ctx->Device.get(), transfer_buf.get(), cycle);
    FO_VERIFY_AND_THROW(nullable_mapped, "SDL_MapGPUTransferBuffer failed", SDL_GetError());
    auto mapped = nullable_mapped.as_ptr();
    return mapped;
}

// Record a full clear of the backbuffer proxy so the first Present always blits defined content
static void RecordProxyClear(ptr<SDLGpu_Renderer::Context> ctx)
{
    FO_STACK_TRACE_ENTRY();

    EnsureCmdBuf(ctx);
    EndAnyPass(ctx);

    SDL_GPUColorTargetInfo color_target = {};
    color_target.texture = ctx->BackbufferProxyTex.get();
    color_target.load_op = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op = SDL_GPU_STOREOP_STORE;
    color_target.clear_color = SDL_FColor {0.0f, 0.0f, 0.0f, 1.0f};

    nptr<SDL_GPURenderPass> clear_pass = SDL_BeginGPURenderPass(ctx->CmdBuf.get(), &color_target, 1, nullptr);
    FO_VERIFY_AND_THROW(clear_pass, "SDL_BeginGPURenderPass failed for the backbuffer proxy clear", SDL_GetError());
    SDL_EndGPURenderPass(clear_pass.get());
}

static void CreateBackbufferProxy(ptr<SDLGpu_Renderer::Context> ctx, isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!ctx->BackbufferProxyTex, "SDL_GPU backbuffer proxy texture is already created");

    SDL_GPUTextureCreateInfo tex_info = {};
    tex_info.type = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    tex_info.width = numeric_cast<uint32_t>(size.width);
    tex_info.height = numeric_cast<uint32_t>(size.height);
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels = 1;
    tex_info.sample_count = SDL_GPU_SAMPLECOUNT_1;

    ctx->BackbufferProxyTex = SDL_CreateGPUTexture(ctx->Device.get(), &tex_info);
    FO_VERIFY_AND_THROW(ctx->BackbufferProxyTex, "SDL_CreateGPUTexture failed for the backbuffer proxy", SDL_GetError(), size);

    ctx->BackBufSize = size;

    RecordProxyClear(ctx);
}

SDLGpu_Renderer::SDLGpu_Renderer() = default;

void SDLGpu_Renderer::Init(GlobalSettings& settings, nptr<WindowInternalHandle> window)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!!window, "Frontend window handle is null");
    FO_VERIFY_AND_THROW(!_ctx, "Frontend context is already initialized");
    _ctx = SafeAlloc::MakeUnique<Context>();
    auto ctx = _ctx.as_ptr();

    ctx->Settings = &settings;
    ctx->VSync = settings.VSync;
    ctx->SdlWindow = GetSdlWindow(window);

    // Device
    constexpr SDL_GPUShaderFormat requested_formats = SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL;
    ptr<const string> gpu_driver_name = &settings.SDLGpuDriver;
    ctx->Device = SDL_CreateGPUDevice(requested_formats, settings.RenderDebug, gpu_driver_name->empty() ? nullptr : gpu_driver_name->c_str());

    if (!ctx->Device) {
        throw AppInitException("SDL_CreateGPUDevice failed", SDL_GetError(), settings.SDLGpuDriver);
    }

    WriteLog("Used SDL_GPU rendering ({})", SDL_GetGPUDeviceDriver(ctx->Device.get()));

    // Shader format: prefer the SPIR-V flavor (Vulkan), fall back to MSL (Metal)
    const SDL_GPUShaderFormat device_formats = SDL_GetGPUShaderFormats(ctx->Device.get());

    if ((device_formats & SDL_GPU_SHADERFORMAT_SPIRV) != 0) {
        ctx->ShaderFormat = SDL_GPU_SHADERFORMAT_SPIRV;
    }
    else if ((device_formats & SDL_GPU_SHADERFORMAT_MSL) != 0) {
        ctx->ShaderFormat = SDL_GPU_SHADERFORMAT_MSL;
    }
    else {
        throw AppInitException("SDL_GPU device supports neither SPIR-V nor MSL shaders", device_formats);
    }

    // Swapchain
    const bool claim_ok = SDL_ClaimWindowForGPUDevice(ctx->Device.get(), ctx->SdlWindow.get());

    if (!claim_ok) {
        throw AppInitException("SDL_ClaimWindowForGPUDevice failed", SDL_GetError());
    }

    if (!ctx->VSync) {
        if (SDL_WindowSupportsGPUPresentMode(ctx->Device.get(), ctx->SdlWindow.get(), SDL_GPU_PRESENTMODE_IMMEDIATE)) {
            const bool swapchain_params_ok = SDL_SetGPUSwapchainParameters(ctx->Device.get(), ctx->SdlWindow.get(), SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_IMMEDIATE);
            FO_VERIFY_AND_THROW(swapchain_params_ok, "SDL_SetGPUSwapchainParameters failed", SDL_GetError());
        }
        else {
            WriteLog("SDL_GPU immediate present mode is not supported, VSync stays enabled");
        }
    }

    // Depth format for render targets created with depth
    if (SDL_GPUTextureSupportsFormat(ctx->Device.get(), SDL_GPU_TEXTUREFORMAT_D24_UNORM, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)) {
        ctx->DepthFormat = SDL_GPU_TEXTUREFORMAT_D24_UNORM;
    }
    else {
        ctx->DepthFormat = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    }

    // Samplers
    const auto create_sampler = [&](SDL_GPUFilter filter) {
        SDL_GPUSamplerCreateInfo sampler_info = {};
        sampler_info.min_filter = filter;
        sampler_info.mag_filter = filter;
        sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

        const nptr<SDL_GPUSampler> sampler = SDL_CreateGPUSampler(ctx->Device.get(), &sampler_info);
        FO_VERIFY_AND_THROW(sampler, "SDL_CreateGPUSampler failed", SDL_GetError());
        return sampler;
    };

    ctx->PointSampler = create_sampler(SDL_GPU_FILTER_NEAREST);
    ctx->LinearSampler = create_sampler(SDL_GPU_FILTER_LINEAR);

    // SDL_GPU has no query for the maximum texture dimension; 4096 is guaranteed by every Vulkan / Metal / D3D12 device
    static_assert(AppRender::MIN_ATLAS_SIZE <= 4096, "SDL_GPU texture atlas size is below the required minimum");
    AppRender::MAX_ATLAS_WIDTH = 4096;
    AppRender::MAX_ATLAS_HEIGHT = 4096;

    // Backbuffer proxy: all backbuffer rendering goes here and Present() blits it to the swapchain
    CreateBackbufferProxy(ctx, {settings.ScreenWidth, settings.ScreenHeight});

    // Dummy texture
    constexpr ucolor dummy_pixel[1] = {ucolor {255, 0, 255, 255}};
    ctx->DummyTexture = CreateTexture({1, 1}, false, false);
    auto dummy_texture = GetDummyTexture(ctx);
    dummy_texture->UpdateTextureRegion({}, {1, 1}, dummy_pixel);

    // Init render target
    SetRenderTarget(nullptr);
    DisableScissor();
}

SDLGpu_Renderer::~SDLGpu_Renderer()
{
    FO_STACK_TRACE_ENTRY();

    if (!_ctx) {
        return;
    }

    auto ctx = _ctx.as_ptr();

    EndAnyPass(ctx);

    // No swapchain texture is ever held outside Present, so the recorded but unsubmitted work can be dropped
    if (ctx->CmdBuf) {
        (void)SDL_CancelGPUCommandBuffer(ctx->CmdBuf.get());
        ctx->CmdBuf = nullptr;
    }

    if (ctx->Device) {
        (void)SDL_WaitForGPUIdle(ctx->Device.get());

        ctx->DummyTexture.reset();

        if (ctx->UploadTransferBuf) {
            SDL_ReleaseGPUTransferBuffer(ctx->Device.get(), ctx->UploadTransferBuf.get());
            ctx->UploadTransferBuf = nullptr;
        }
        if (ctx->DownloadTransferBuf) {
            SDL_ReleaseGPUTransferBuffer(ctx->Device.get(), ctx->DownloadTransferBuf.get());
            ctx->DownloadTransferBuf = nullptr;
        }
        if (ctx->PointSampler) {
            SDL_ReleaseGPUSampler(ctx->Device.get(), ctx->PointSampler.get());
            ctx->PointSampler = nullptr;
        }
        if (ctx->LinearSampler) {
            SDL_ReleaseGPUSampler(ctx->Device.get(), ctx->LinearSampler.get());
            ctx->LinearSampler = nullptr;
        }
        if (ctx->BackbufferProxyTex) {
            SDL_ReleaseGPUTexture(ctx->Device.get(), ctx->BackbufferProxyTex.get());
            ctx->BackbufferProxyTex = nullptr;
        }

        SDL_ReleaseWindowFromGPUDevice(ctx->Device.get(), ctx->SdlWindow.get());
        SDL_DestroyGPUDevice(ctx->Device.get());
        ctx->Device = nullptr;
    }

    ctx->Settings = nullptr;
    ctx->SdlWindow = nullptr;
    ctx->CurRenderTarget = nullptr;

    _ctx.reset();
}

void SDLGpu_Renderer::Present()
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();

    FlushPendingClears(ctx);
    EnsureCmdBuf(ctx);

    nptr<SDL_GPUTexture> swapchain_tex {};
    uint32_t swapchain_width = 0;
    uint32_t swapchain_height = 0;
    const bool acquire_ok = SDL_WaitAndAcquireGPUSwapchainTexture(ctx->CmdBuf.get(), ctx->SdlWindow.get(), swapchain_tex.get_pp(), &swapchain_width, &swapchain_height);
    FO_VERIFY_AND_THROW(acquire_ok, "SDL_WaitAndAcquireGPUSwapchainTexture failed", SDL_GetError(), ctx->BackBufSize);

    // A null swapchain texture is not an error (window is minimized or otherwise unavailable); skip the blit
    if (swapchain_tex) {
        SDL_GPUBlitInfo blit_info = {};
        blit_info.source.texture = ctx->BackbufferProxyTex.get();
        blit_info.source.w = numeric_cast<uint32_t>(ctx->BackBufSize.width);
        blit_info.source.h = numeric_cast<uint32_t>(ctx->BackBufSize.height);
        blit_info.destination.texture = swapchain_tex.get();
        blit_info.destination.w = swapchain_width;
        blit_info.destination.h = swapchain_height;
        blit_info.load_op = SDL_GPU_LOADOP_DONT_CARE;
        blit_info.filter = SDL_GPU_FILTER_LINEAR;

        SDL_BlitGPUTexture(ctx->CmdBuf.get(), &blit_info);
    }

    const bool submit_ok = SDL_SubmitGPUCommandBuffer(ctx->CmdBuf.get());
    ctx->CmdBuf = nullptr;
    FO_VERIFY_AND_THROW(submit_ok, "SDL_SubmitGPUCommandBuffer failed", SDL_GetError());
}

auto SDLGpu_Renderer::CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture>
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();
    auto sdl_tex = SafeAlloc::MakeUnique<SDLGpu_Texture>(size, linear_filtered, with_depth, ctx);

    SDL_GPUTextureCreateInfo tex_info = {};
    tex_info.type = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    tex_info.width = numeric_cast<uint32_t>(size.width);
    tex_info.height = numeric_cast<uint32_t>(size.height);
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels = 1;
    tex_info.sample_count = SDL_GPU_SAMPLECOUNT_1;

    sdl_tex->TexHandle = SDL_CreateGPUTexture(ctx->Device.get(), &tex_info);
    FO_VERIFY_AND_THROW(sdl_tex->TexHandle, "SDL_CreateGPUTexture failed for a render texture", SDL_GetError(), size, linear_filtered, with_depth);

    if (with_depth) {
        SDL_GPUTextureCreateInfo depth_tex_info = {};
        depth_tex_info.type = SDL_GPU_TEXTURETYPE_2D;
        depth_tex_info.format = ctx->DepthFormat;
        depth_tex_info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        depth_tex_info.width = numeric_cast<uint32_t>(size.width);
        depth_tex_info.height = numeric_cast<uint32_t>(size.height);
        depth_tex_info.layer_count_or_depth = 1;
        depth_tex_info.num_levels = 1;
        depth_tex_info.sample_count = SDL_GPU_SAMPLECOUNT_1;

        sdl_tex->DepthTexHandle = SDL_CreateGPUTexture(ctx->Device.get(), &depth_tex_info);
        FO_VERIFY_AND_THROW(sdl_tex->DepthTexHandle, "SDL_CreateGPUTexture failed for a depth texture", SDL_GetError(), size);
    }

    return std::move(sdl_tex);
}

auto SDLGpu_Renderer::CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer>
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();
    auto sdl_dbuf = SafeAlloc::MakeUnique<SDLGpu_DrawBuffer>(is_static, ctx);

    return std::move(sdl_dbuf);
}

auto SDLGpu_Renderer::CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect>
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();
    auto sdl_effect = SafeAlloc::MakeUnique<SDLGpu_Effect>(usage, name, loader, ctx);

    // The SDL_GPU backend consumes the SDL-convention baked flavors (per-stage descriptor sets), not the native
    // `-spv` that Rendering-Vulkan uses: `-spv_sdl` for the Vulkan driver, SDL-remapped `-msl_*` for the Metal driver.
    const bool spirv = ctx->ShaderFormat == SDL_GPU_SHADERFORMAT_SPIRV;
#if FO_IOS
    const string_view shader_flavor = spirv ? "spv_sdl" : "msl_ios";
#else
    const string_view shader_flavor = spirv ? "spv_sdl" : "msl_mac";
#endif
    // SPIR-V keeps the default entry point, SPIRV-Cross renames it for MSL (matches the SDL Metal driver default)
    const string_view entry_point = spirv ? "main" : "main0";

    for (size_t pass = 0; pass < sdl_effect->_passCount; pass++) {
        // Per-stage SDL binding slots and resource counts from the baked [EffectInfoSdl] section
        const string pass_info_fname = strex("{}.fofx-{}-info", strex(name).erase_file_extension(), pass + 1);
        const string pass_info_content = loader(pass_info_fname);
        const auto pass_info = ConfigFile(pass_info_fname, pass_info_content);

        if (!pass_info.HasSection("EffectInfoSdl")) {
            throw EffectLoadException("Effect info has no EffectInfoSdl section, rebake resources", name, pass_info_fname);
        }

        auto& slots = sdl_effect->PassSlots[pass];
        slots.VertSamplers = numeric_cast<uint32_t>(pass_info.GetAsInt("EffectInfoSdl", "VertexSamplers", 0));
        slots.VertUniformBufs = numeric_cast<uint32_t>(pass_info.GetAsInt("EffectInfoSdl", "VertexUniformBuffers", 0));
        slots.FragSamplers = numeric_cast<uint32_t>(pass_info.GetAsInt("EffectInfoSdl", "FragmentSamplers", 0));
        slots.FragUniformBufs = numeric_cast<uint32_t>(pass_info.GetAsInt("EffectInfoSdl", "FragmentUniformBuffers", 0));

        const auto read_slot_pair = [&pass_info](string_view res_name, int32_t& vert_slot, int32_t& frag_slot) {
            vert_slot = pass_info.GetAsInt("EffectInfoSdl", strex("Vert{}", res_name), -1);
            frag_slot = pass_info.GetAsInt("EffectInfoSdl", strex("Frag{}", res_name), -1);
        };

        read_slot_pair("MainTex", slots.VertMainTex, slots.FragMainTex);
        read_slot_pair("IndoorMaskTex", slots.VertIndoorMaskTex, slots.FragIndoorMaskTex);
        read_slot_pair("ProjBuf", slots.VertProjBuf, slots.FragProjBuf);
        read_slot_pair("MainTexBuf", slots.VertMainTexBuf, slots.FragMainTexBuf);
        read_slot_pair("EggBuf", slots.VertEggBuf, slots.FragEggBuf);
        read_slot_pair("SpriteBorderBuf", slots.VertSpriteBorderBuf, slots.FragSpriteBorderBuf);
        read_slot_pair("TimeBuf", slots.VertTimeBuf, slots.FragTimeBuf);
        read_slot_pair("RandomValueBuf", slots.VertRandomValueBuf, slots.FragRandomValueBuf);
        read_slot_pair("ScriptValueBuf", slots.VertScriptValueBuf, slots.FragScriptValueBuf);
        read_slot_pair("CameraBuf", slots.VertCameraBuf, slots.FragCameraBuf);
#if FO_ENABLE_3D
        read_slot_pair("ModelBuf", slots.VertModelBuf, slots.FragModelBuf);
        read_slot_pair("ModelTexBuf", slots.VertModelTexBuf, slots.FragModelTexBuf);
        read_slot_pair("ModelAnimBuf", slots.VertModelAnimBuf, slots.FragModelAnimBuf);

        for (size_t i = 0; i < MODEL_MAX_TEXTURES; i++) {
            read_slot_pair(strex("ModelTex{}", i), slots.VertModelTex[i], slots.FragModelTex[i]);
        }
#endif

        // Shaders
        const auto create_shader = [&](bool is_vertex) {
            const string shader_fname = strex("{}.fofx-{}-{}-{}", strex(name).erase_file_extension(), pass + 1, is_vertex ? "vert" : "frag", shader_flavor);
            const string shader_content = loader(shader_fname);
            FO_VERIFY_AND_THROW(!shader_content.empty(), "SDL_GPU effect shader content is empty after loading", name, pass + 1, shader_fname);

            SDL_GPUShaderCreateInfo shader_info = {};
            shader_info.code_size = shader_content.size();
            shader_info.code = GetContentBytes(shader_content).get();
            shader_info.entrypoint = entry_point.data();
            shader_info.format = ctx->ShaderFormat;
            shader_info.stage = is_vertex ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
            shader_info.num_samplers = is_vertex ? slots.VertSamplers : slots.FragSamplers;
            shader_info.num_storage_textures = 0;
            shader_info.num_storage_buffers = 0;
            shader_info.num_uniform_buffers = is_vertex ? slots.VertUniformBufs : slots.FragUniformBufs;

            const nptr<SDL_GPUShader> shader = SDL_CreateGPUShader(ctx->Device.get(), &shader_info);

            if (!shader) {
                throw EffectLoadException("SDL_CreateGPUShader failed", SDL_GetError(), shader_fname);
            }

            return shader;
        };

        sdl_effect->VertShader[pass] = create_shader(true);
        sdl_effect->FragShader[pass] = create_shader(false);
    }

    return std::move(sdl_effect);
}

auto SDLGpu_Renderer::CreateOrthoMatrix(float32_t left, float32_t right, float32_t bottom, float32_t top, float32_t nearp, float32_t farp) const -> mat44
{
    FO_STACK_TRACE_ENTRY();

    // SDL_GPU normalizes clip-space depth to [0,1] across all drivers, same as Direct3D
    const auto& l = left;
    const auto& t = top;
    const auto& r = right;
    const auto& b = bottom;
    const auto& zn = nearp;
    const auto& zf = farp;

    mat44 result {1.0f};

    result[0][0] = 2.0f / (r - l);
    result[1][0] = 0.0f;
    result[2][0] = 0.0f;
    result[3][0] = (l + r) / (l - r);

    result[0][1] = 0.0f;
    result[1][1] = 2.0f / (t - b);
    result[2][1] = 0.0f;
    result[3][1] = (t + b) / (b - t);

    result[0][2] = 0.0f;
    result[1][2] = 0.0f;
    result[2][2] = 1.0f / (zn - zf);
    result[3][2] = zn / (zn - zf);

    result[0][3] = 0.0f;
    result[1][3] = 0.0f;
    result[2][3] = 0.0f;
    result[3][3] = 1.0f;

    return result;
}

auto SDLGpu_Renderer::GetViewPort() const -> irect32
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();
    return ctx->ViewPortRect;
}

void SDLGpu_Renderer::SetRenderTarget(nptr<RenderTexture> tex)
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();

    // Deferred clears belong to the target being left, so materialize them before switching
    FlushPendingClears(ctx);

    int32_t vp_ox;
    int32_t vp_oy;
    int32_t vp_width;
    int32_t vp_height;
    int32_t screen_width;
    int32_t screen_height;

    if (tex) {
        auto sdl_tex = RenderBackendCast<SDLGpu_Texture>(tex.as_ptr());
        ctx->CurRenderTarget = sdl_tex;

        vp_ox = 0;
        vp_oy = 0;
        vp_width = sdl_tex->Size.width;
        vp_height = sdl_tex->Size.height;
        screen_width = vp_width;
        screen_height = vp_height;
    }
    else {
        ctx->CurRenderTarget = nullptr;

        const auto back_buf_aspect = checked_div<float32_t>(numeric_cast<float32_t>(ctx->BackBufSize.width), numeric_cast<float32_t>(ctx->BackBufSize.height));
        const auto screen_aspect = checked_div<float32_t>(numeric_cast<float32_t>(ctx->Settings->ScreenWidth), numeric_cast<float32_t>(ctx->Settings->ScreenHeight));
        const auto fit_width = iround<int32_t>(screen_aspect <= back_buf_aspect ? numeric_cast<float32_t>(ctx->BackBufSize.height) * screen_aspect : numeric_cast<float32_t>(ctx->BackBufSize.height) * back_buf_aspect);
        const auto fit_height = iround<int32_t>(screen_aspect <= back_buf_aspect ? numeric_cast<float32_t>(ctx->BackBufSize.width) / back_buf_aspect : numeric_cast<float32_t>(ctx->BackBufSize.width) / screen_aspect);

        vp_ox = (ctx->BackBufSize.width - fit_width) / 2;
        vp_oy = (ctx->BackBufSize.height - fit_height) / 2;
        vp_width = fit_width;
        vp_height = fit_height;
        screen_width = ctx->Settings->ScreenWidth;
        screen_height = ctx->Settings->ScreenHeight;
    }

    ctx->ViewPortRect = irect32 {vp_ox, vp_oy, vp_width, vp_height};

    ctx->ViewPort.x = numeric_cast<float32_t>(vp_ox);
    ctx->ViewPort.y = numeric_cast<float32_t>(vp_oy);
    ctx->ViewPort.w = numeric_cast<float32_t>(vp_width);
    ctx->ViewPort.h = numeric_cast<float32_t>(vp_height);
    ctx->ViewPort.min_depth = 0.0f;
    ctx->ViewPort.max_depth = 1.0f;

    ctx->ProjMatrix = CreateOrthoMatrix(0.0f, numeric_cast<float32_t>(screen_width), numeric_cast<float32_t>(screen_height), 0.0f, ctx->OrthoNear, ctx->OrthoFar);

    ctx->DisabledScissorRect.x = vp_ox;
    ctx->DisabledScissorRect.y = vp_oy;
    ctx->DisabledScissorRect.w = vp_width;
    ctx->DisabledScissorRect.h = vp_height;

    ctx->TargetSize = {screen_width, screen_height};
}

void SDLGpu_Renderer::SetOrthoDepthRange(float32_t nearp, float32_t farp) noexcept
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();
    ctx->OrthoNear = nearp;
    ctx->OrthoFar = farp;
    ctx->ProjMatrix = CreateOrthoMatrix(0.0f, numeric_cast<float32_t>(ctx->TargetSize.width), numeric_cast<float32_t>(ctx->TargetSize.height), 0.0f, nearp, farp);
}

auto SDLGpu_Renderer::GetProjMatrix() const -> mat44
{
    FO_NO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();
    return ctx->ProjMatrix;
}

void SDLGpu_Renderer::ClearRenderTarget(optional<ucolor> color, bool depth, bool stencil)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(stencil); // Depth textures are created without a stencil aspect and the engine never clears stencil

    auto ctx = _ctx.as_ptr();

    // Defer the clear into the load-op of the next render pass on the current target
    EndAnyPass(ctx);

    if (color.has_value()) {
        ctx->PendingClearColor = color;
    }
    if (depth && IsTargetWithDepth(ctx)) {
        ctx->PendingClearDepth = true;
    }
}

void SDLGpu_Renderer::EnableScissor(irect32 rect)
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();

    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;

    if (ctx->ViewPortRect.width != ctx->TargetSize.width || ctx->ViewPortRect.height != ctx->TargetSize.height) {
        const float32_t x_ratio = numeric_cast<float32_t>(ctx->ViewPortRect.width) / numeric_cast<float32_t>(ctx->TargetSize.width);
        const float32_t y_ratio = numeric_cast<float32_t>(ctx->ViewPortRect.height) / numeric_cast<float32_t>(ctx->TargetSize.height);

        left = ctx->ViewPortRect.x + iround<int32_t>(numeric_cast<float32_t>(rect.x) * x_ratio);
        top = ctx->ViewPortRect.y + iround<int32_t>(numeric_cast<float32_t>(rect.y) * y_ratio);
        right = ctx->ViewPortRect.x + iround<int32_t>(numeric_cast<float32_t>(rect.x + rect.width) * x_ratio);
        bottom = ctx->ViewPortRect.y + iround<int32_t>(numeric_cast<float32_t>(rect.y + rect.height) * y_ratio);
    }
    else {
        left = ctx->ViewPortRect.x + rect.x;
        top = ctx->ViewPortRect.y + rect.y;
        right = ctx->ViewPortRect.x + rect.x + rect.width;
        bottom = ctx->ViewPortRect.y + rect.y + rect.height;
    }

    ctx->ScissorRect.x = left;
    ctx->ScissorRect.y = top;
    ctx->ScissorRect.w = right - left;
    ctx->ScissorRect.h = bottom - top;

    ctx->ScissorEnabled = true;

    // Scissor is render-pass state, so apply it right away when a pass is already open
    if (ctx->RenderPass) {
        SDL_SetGPUScissor(ctx->RenderPass.get(), &ctx->ScissorRect);
    }
}

void SDLGpu_Renderer::DisableScissor()
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();
    ctx->ScissorEnabled = false;

    if (ctx->RenderPass) {
        SDL_SetGPUScissor(ctx->RenderPass.get(), &ctx->DisabledScissorRect);
    }
}

void SDLGpu_Renderer::OnResizeWindow(isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();
    const auto is_backbuffer_target = !ctx->CurRenderTarget;

    // The proxy may be referenced by recorded work, so finish everything before recreating it
    SubmitAndWait(ctx);
    const bool wait_idle_ok = SDL_WaitForGPUIdle(ctx->Device.get());
    FO_VERIFY_AND_THROW(wait_idle_ok, "SDL_WaitForGPUIdle failed during window resize", SDL_GetError(), size);

    SDL_ReleaseGPUTexture(ctx->Device.get(), ctx->BackbufferProxyTex.get());
    ctx->BackbufferProxyTex = nullptr;

    CreateBackbufferProxy(ctx, size);

    if (is_backbuffer_target) {
        SetRenderTarget(nullptr);
    }
}

SDLGpu_Texture::~SDLGpu_Texture()
{
    FO_STACK_TRACE_ENTRY();

    if (TexHandle) {
        SDL_ReleaseGPUTexture(_ctx->Device.get(), TexHandle.get());
        TexHandle = nullptr;
    }
    if (DepthTexHandle) {
        SDL_ReleaseGPUTexture(_ctx->Device.get(), DepthTexHandle.get());
        DepthTexHandle = nullptr;
    }
}

auto SDLGpu_Texture::GetTexturePixel(ipos32 pos) const -> ucolor
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(Size.is_valid_pos(pos), "Requested SDL_GPU texture pixel is outside texture bounds", pos, Size);

    const auto region = GetTextureRegion(pos, {1, 1});
    return region.front();
}

auto SDLGpu_Texture::GetTextureRegion(ipos32 pos, isize32 size) const -> vector<ucolor>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(size.width > 0, "Size width must be positive", size.width);
    FO_VERIFY_AND_THROW(size.height > 0, "Size height must be positive", size.height);
    FO_VERIFY_AND_THROW(pos.x >= 0, "Position x is negative", pos.x);
    FO_VERIFY_AND_THROW(pos.y >= 0, "Position y is negative", pos.y);
    FO_VERIFY_AND_THROW(pos.x + size.width <= Size.width, "Requested texture read rectangle right edge is outside texture bounds", pos.x, size.width, Size.width);
    FO_VERIFY_AND_THROW(pos.y + size.height <= Size.height, "Requested texture read rectangle bottom edge is outside texture bounds", pos.y, size.height, Size.height);

    ptr<SDLGpu_Renderer::Context> ctx = _ctx;

    // Reads must observe everything drawn so far, so flush the recorded work first
    FlushPendingClears(ctx);

    const size_t read_size = numeric_cast<size_t>(size.width) * size.height * sizeof(ucolor);
    auto transfer_buf = EnsureTransferBuffer(ctx, ctx->DownloadTransferBuf, ctx->DownloadTransferBufSize, read_size, true);

    auto copy_pass = EnsureCopyPass(ctx);

    SDL_GPUTextureRegion src_region = {};
    src_region.texture = TexHandle.get_no_const();
    src_region.x = numeric_cast<uint32_t>(pos.x);
    src_region.y = numeric_cast<uint32_t>(pos.y);
    src_region.w = numeric_cast<uint32_t>(size.width);
    src_region.h = numeric_cast<uint32_t>(size.height);
    src_region.d = 1;

    SDL_GPUTextureTransferInfo transfer_info = {};
    transfer_info.transfer_buffer = transfer_buf.get();

    SDL_DownloadFromGPUTexture(copy_pass.get(), &src_region, &transfer_info);

    SubmitAndWait(ctx);

    vector<ucolor> result;
    result.resize(numeric_cast<size_t>(size.width) * size.height);

    auto mapped = MapTransferBuffer(ctx, transfer_buf, false);
    MemCopy(result.data(), mapped, read_size);
    SDL_UnmapGPUTransferBuffer(ctx->Device.get(), transfer_buf.get());

    return result;
}

void SDLGpu_Texture::UpdateTextureRegion(ipos32 pos, isize32 size, const_span<ucolor> data, bool use_dest_pitch)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(pos.x >= 0, "Position x is negative", pos.x);
    FO_VERIFY_AND_THROW(pos.y >= 0, "Position y is negative", pos.y);
    FO_VERIFY_AND_THROW(pos.x + size.width <= Size.width, "Texture update rectangle right edge is outside texture bounds", pos.x, size.width, Size.width);
    FO_VERIFY_AND_THROW(pos.y + size.height <= Size.height, "Texture update rectangle bottom edge is outside texture bounds", pos.y, size.height, Size.height);
    const size_t src_pitch_size = numeric_cast<size_t>(use_dest_pitch ? Size.width : size.width);
    const size_t required_size = size.height != 0 ? (numeric_cast<size_t>(size.height - 1) * src_pitch_size + numeric_cast<size_t>(size.width)) : 0;
    FO_VERIFY_AND_THROW(data.size() >= required_size, "Texture update source data is smaller than the required region size", data.size(), required_size, size, use_dest_pitch);

    if (required_size == 0) {
        return;
    }

    ptr<SDLGpu_Renderer::Context> ctx = _ctx;

    const size_t upload_size = required_size * sizeof(ucolor);
    auto transfer_buf = EnsureTransferBuffer(ctx, ctx->UploadTransferBuf, ctx->UploadTransferBufSize, upload_size, false);

    auto mapped = MapTransferBuffer(ctx, transfer_buf, true);
    MemCopy(mapped, data.data(), upload_size);
    SDL_UnmapGPUTransferBuffer(ctx->Device.get(), transfer_buf.get());

    auto copy_pass = EnsureCopyPass(ctx);

    SDL_GPUTextureTransferInfo transfer_info = {};
    transfer_info.transfer_buffer = transfer_buf.get();
    transfer_info.pixels_per_row = use_dest_pitch ? numeric_cast<uint32_t>(Size.width) : 0;

    SDL_GPUTextureRegion dest_region = {};
    dest_region.texture = TexHandle.get();
    dest_region.x = numeric_cast<uint32_t>(pos.x);
    dest_region.y = numeric_cast<uint32_t>(pos.y);
    dest_region.w = numeric_cast<uint32_t>(size.width);
    dest_region.h = numeric_cast<uint32_t>(size.height);
    dest_region.d = 1;

    // No cycle: partial updates must preserve the rest of the texture (the transfer-buffer cycle above covers the CPU hazard)
    SDL_UploadToGPUTexture(copy_pass.get(), &transfer_info, &dest_region, false);
}

SDLGpu_DrawBuffer::~SDLGpu_DrawBuffer()
{
    FO_STACK_TRACE_ENTRY();

    if (VertexBuf) {
        SDL_ReleaseGPUBuffer(_ctx->Device.get(), VertexBuf.get());
        VertexBuf = nullptr;
    }
    if (IndexBuf) {
        SDL_ReleaseGPUBuffer(_ctx->Device.get(), IndexBuf.get());
        IndexBuf = nullptr;
    }
    if (TransferBuf) {
        SDL_ReleaseGPUTransferBuffer(_ctx->Device.get(), TransferBuf.get());
        TransferBuf = nullptr;
    }
}

void SDLGpu_DrawBuffer::Upload(EffectUsage usage, optional<size_t> custom_vertices_size, optional<size_t> custom_indices_size)
{
    FO_STACK_TRACE_ENTRY();

    if (IsStatic && !StaticDataChanged) {
        return;
    }

    StaticDataChanged = false;

    size_t upload_vertices;
    size_t vert_size;

#if FO_ENABLE_3D
    if (usage == EffectUsage::Model) {
        FO_VERIFY_AND_THROW(Vertices.empty(), "SDL_GPU model draw buffer upload received 2D vertices alongside 3D vertices", Vertices.size(), Vertices3D.size(), VertCount);
        upload_vertices = custom_vertices_size.value_or(VertCount);
        vert_size = sizeof(Vertex3D);
    }
    else {
        FO_VERIFY_AND_THROW(Vertices3D.empty(), "SDL_GPU 2D draw buffer upload received 3D vertices alongside 2D vertices", Vertices3D.size(), Vertices.size(), VertCount);
        upload_vertices = custom_vertices_size.value_or(VertCount);
        vert_size = sizeof(Vertex2D);
    }
#else
    ignore_unused(usage);
    upload_vertices = custom_vertices_size.value_or(VertCount);
    vert_size = sizeof(Vertex2D);
#endif

    const size_t upload_indices = custom_indices_size.value_or(IndCount);
    const size_t vertices_data_size = upload_vertices * vert_size;
    const size_t indices_data_size = upload_indices * sizeof(vindex_t);

    if (vertices_data_size == 0 && indices_data_size == 0) {
        return;
    }

    ptr<SDLGpu_Renderer::Context> ctx = _ctx;

    // (Re)create GPU buffers with some slack
    if (!VertexBuf || upload_vertices > VertexBufSize) {
        if (VertexBuf) {
            SDL_ReleaseGPUBuffer(ctx->Device.get(), VertexBuf.get());
            VertexBuf = nullptr;
        }

        VertexBufSize = upload_vertices + 1024;

        SDL_GPUBufferCreateInfo vbuf_info = {};
        vbuf_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        vbuf_info.size = numeric_cast<uint32_t>(VertexBufSize * vert_size);

        VertexBuf = SDL_CreateGPUBuffer(ctx->Device.get(), &vbuf_info);
        FO_VERIFY_AND_THROW(VertexBuf, "SDL_CreateGPUBuffer failed for a vertex buffer", SDL_GetError(), upload_vertices, vert_size, VertexBufSize, usage);
    }

    if (!IndexBuf || upload_indices > IndexBufSize) {
        if (IndexBuf) {
            SDL_ReleaseGPUBuffer(ctx->Device.get(), IndexBuf.get());
            IndexBuf = nullptr;
        }

        IndexBufSize = upload_indices + 1024;

        SDL_GPUBufferCreateInfo ibuf_info = {};
        ibuf_info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
        ibuf_info.size = numeric_cast<uint32_t>(IndexBufSize * sizeof(vindex_t));

        IndexBuf = SDL_CreateGPUBuffer(ctx->Device.get(), &ibuf_info);
        FO_VERIFY_AND_THROW(IndexBuf, "SDL_CreateGPUBuffer failed for an index buffer", SDL_GetError(), upload_indices, sizeof(vindex_t), IndexBufSize);
    }

    // Stage vertices and indices through the per-buffer transfer buffer
    const size_t total_data_size = vertices_data_size + indices_data_size;
    auto transfer_buf = EnsureTransferBuffer(ctx, TransferBuf, TransferBufSize, total_data_size, false);

    auto mapped = MapTransferBuffer(ctx, transfer_buf, true);
    ptr<uint8_t> mapped_bytes = cast_from_void<uint8_t*>(mapped.get());

    if (vertices_data_size != 0) {
#if FO_ENABLE_3D
        if (usage == EffectUsage::Model) {
            MemCopy(mapped_bytes, Vertices3D.data(), vertices_data_size);
        }
        else {
            MemCopy(mapped_bytes, Vertices.data(), vertices_data_size);
        }
#else
        MemCopy(mapped_bytes, Vertices.data(), vertices_data_size);
#endif
    }
    if (indices_data_size != 0) {
        MemCopy(mapped_bytes.get() + vertices_data_size, Indices.data(), indices_data_size);
    }

    SDL_UnmapGPUTransferBuffer(ctx->Device.get(), transfer_buf.get());

    auto copy_pass = EnsureCopyPass(ctx);

    // Cycle the GPU buffers: draws already recorded in this command buffer keep the pre-cycle contents
    if (vertices_data_size != 0) {
        SDL_GPUTransferBufferLocation vert_src = {};
        vert_src.transfer_buffer = transfer_buf.get();
        vert_src.offset = 0;

        SDL_GPUBufferRegion vert_dest = {};
        vert_dest.buffer = VertexBuf.get();
        vert_dest.offset = 0;
        vert_dest.size = numeric_cast<uint32_t>(vertices_data_size);

        SDL_UploadToGPUBuffer(copy_pass.get(), &vert_src, &vert_dest, true);
    }

    if (indices_data_size != 0) {
        SDL_GPUTransferBufferLocation ind_src = {};
        ind_src.transfer_buffer = transfer_buf.get();
        ind_src.offset = numeric_cast<uint32_t>(vertices_data_size);

        SDL_GPUBufferRegion ind_dest = {};
        ind_dest.buffer = IndexBuf.get();
        ind_dest.offset = 0;
        ind_dest.size = numeric_cast<uint32_t>(indices_data_size);

        SDL_UploadToGPUBuffer(copy_pass.get(), &ind_src, &ind_dest, true);
    }
}

SDLGpu_Effect::~SDLGpu_Effect()
{
    FO_STACK_TRACE_ENTRY();

    for (auto& [key, pipeline] : _pipelines) {
        ignore_unused(key);

        if (pipeline) {
            SDL_ReleaseGPUGraphicsPipeline(_ctx->Device.get(), pipeline.get());
        }
    }

    _pipelines.clear();

    for (size_t i = 0; i < EFFECT_MAX_PASSES; i++) {
        if (VertShader[i]) {
            SDL_ReleaseGPUShader(_ctx->Device.get(), VertShader[i].get());
            VertShader[i] = nullptr;
        }
        if (FragShader[i]) {
            SDL_ReleaseGPUShader(_ctx->Device.get(), FragShader[i].get());
            FragShader[i] = nullptr;
        }
    }
}

auto SDLGpu_Effect::GetOrCreatePipeline(size_t pass, SDL_GPUPrimitiveType topology, bool with_depth) -> ptr<SDL_GPUGraphicsPipeline>
{
    FO_STACK_TRACE_ENTRY();

    const bool disable_culling =
#if FO_ENABLE_3D
        DisableCulling;
#else
        false;
#endif

    const uint32_t key = numeric_cast<uint32_t>(pass) | (static_cast<uint32_t>(topology) << 3) | (with_depth ? 1u << 6 : 0u) | (DisableBlending ? 1u << 7 : 0u) | (disable_culling ? 1u << 8 : 0u);

    const auto pipeline_it = _pipelines.find(key);

    if (pipeline_it != _pipelines.end()) {
        FO_VERIFY_AND_THROW(pipeline_it->second, "SDL_GPU cached pipeline is null");
        auto cached_pipeline = pipeline_it->second.as_ptr();
        return cached_pipeline;
    }

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.vertex_shader = VertShader[pass].get();
    pipeline_info.fragment_shader = FragShader[pass].get();
    pipeline_info.primitive_type = topology;

    // Vertex input layout
    SDL_GPUVertexBufferDescription vertex_buf_desc = {};
    vertex_buf_desc.slot = 0;
    vertex_buf_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    constexpr size_t max_attributes = 9;
    SDL_GPUVertexAttribute vertex_attributes[max_attributes] = {};

    const auto set_attribute = [&vertex_attributes](uint32_t location, SDL_GPUVertexElementFormat format, size_t offset) {
        vertex_attributes[location].location = location;
        vertex_attributes[location].buffer_slot = 0;
        vertex_attributes[location].format = format;
        vertex_attributes[location].offset = numeric_cast<uint32_t>(offset);
    };

#if FO_ENABLE_3D
    if (_usage == EffectUsage::Model) {
        static_assert(MODEL_BONES_PER_VERTEX == 4);

        vertex_buf_desc.pitch = sizeof(Vertex3D);
        set_attribute(0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(Vertex3D, Position));
        set_attribute(1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(Vertex3D, Normal));
        set_attribute(2, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(Vertex3D, TexCoord));
        set_attribute(3, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(Vertex3D, TexCoordBase));
        set_attribute(4, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(Vertex3D, Tangent));
        set_attribute(5, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(Vertex3D, Bitangent));
        set_attribute(6, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(Vertex3D, BlendWeights));
        set_attribute(7, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(Vertex3D, BlendIndices));
        set_attribute(8, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, offsetof(Vertex3D, Color));
        pipeline_info.vertex_input_state.num_vertex_attributes = 9;
    }
    else
#endif
    {
        vertex_buf_desc.pitch = sizeof(Vertex2D);
        set_attribute(0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(Vertex2D, PosX));
        set_attribute(1, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, offsetof(Vertex2D, Color));
        set_attribute(2, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(Vertex2D, TexU));
        set_attribute(3, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(Vertex2D, EggFlags));
        pipeline_info.vertex_input_state.num_vertex_attributes = 4;
    }

    pipeline_info.vertex_input_state.vertex_buffer_descriptions = &vertex_buf_desc;
    pipeline_info.vertex_input_state.num_vertex_buffers = 1;
    pipeline_info.vertex_input_state.vertex_attributes = vertex_attributes;

    // Rasterizer
    pipeline_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pipeline_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    pipeline_info.rasterizer_state.enable_depth_clip = true;

#if FO_ENABLE_3D
    pipeline_info.rasterizer_state.cull_mode = _usage == EffectUsage::Model && !disable_culling ? SDL_GPU_CULLMODE_BACK : SDL_GPU_CULLMODE_NONE;
#else
    pipeline_info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
#endif

    pipeline_info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;

    // Depth: only sprite and model usages test depth, and only when the current target carries a depth texture
    bool depth_usage = _usage == EffectUsage::QuadSprite;

#if FO_ENABLE_3D
    depth_usage = depth_usage || _usage == EffectUsage::Model;
#endif

    if (with_depth && depth_usage) {
        pipeline_info.depth_stencil_state.enable_depth_test = true;
        pipeline_info.depth_stencil_state.enable_depth_write = _depthWrite[pass];
        pipeline_info.depth_stencil_state.compare_op = ConvertCompareOp(_depthFunc[pass]);
    }

    // Color target: always RGBA8 (offscreen textures and the backbuffer proxy share the format)
    SDL_GPUColorTargetDescription color_target_desc = {};
    color_target_desc.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

    if (!DisableBlending) {
        color_target_desc.blend_state.enable_blend = true;
        color_target_desc.blend_state.src_color_blendfactor = ConvertBlendFactor(_srcBlendFunc[pass], false);
        color_target_desc.blend_state.dst_color_blendfactor = ConvertBlendFactor(_destBlendFunc[pass], false);
        color_target_desc.blend_state.color_blend_op = ConvertBlendOp(_blendEquation[pass]);
        color_target_desc.blend_state.src_alpha_blendfactor = ConvertBlendFactor(_srcBlendFunc[pass], true);
        color_target_desc.blend_state.dst_alpha_blendfactor = ConvertBlendFactor(_destBlendFunc[pass], true);
        color_target_desc.blend_state.alpha_blend_op = ConvertBlendOp(_blendEquation[pass]);
    }

    pipeline_info.target_info.color_target_descriptions = &color_target_desc;
    pipeline_info.target_info.num_color_targets = 1;

    if (with_depth) {
        pipeline_info.target_info.depth_stencil_format = _ctx->DepthFormat;
        pipeline_info.target_info.has_depth_stencil_target = true;
    }

    nptr<SDL_GPUGraphicsPipeline> nullable_pipeline = SDL_CreateGPUGraphicsPipeline(_ctx->Device.get(), &pipeline_info);

    if (!nullable_pipeline) {
        throw EffectLoadException("SDL_CreateGPUGraphicsPipeline failed", SDL_GetError(), _name, pass, with_depth);
    }

    _pipelines.emplace(key, nullable_pipeline);

    auto pipeline = nullable_pipeline.as_ptr();
    return pipeline;
}

void SDLGpu_Effect::DrawBuffer(ptr<RenderDrawBuffer> dbuf, size_t start_index, optional<size_t> indices_to_draw, nptr<const RenderTexture> custom_tex)
{
    FO_STACK_TRACE_ENTRY();

    auto sdl_dbuf = RenderBackendCast<SDLGpu_DrawBuffer>(dbuf);

#if FO_ENABLE_3D
    if (!custom_tex && ModelTex[0]) {
        custom_tex = ModelTex[0];
    }
#endif
    if (!custom_tex && MainTex) {
        custom_tex = MainTex;
    }

    auto main_tex_source = custom_tex ? custom_tex.as_ptr() : GetDummyTexture(_ctx);
    auto main_tex = RenderBackendCast<const SDLGpu_Texture>(main_tex_source);

    auto topology = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    if (_usage == EffectUsage::Primitive) {
        topology = ConvertPrimitiveType(sdl_dbuf->PrimType);
    }

    const auto draw_count = numeric_cast<uint32_t>(indices_to_draw.value_or(sdl_dbuf->IndCount));

    if (draw_count == 0) {
        return;
    }

    // Derive ProjBuf/MainTexBuf from renderer state ONLY when a caller has not already supplied them.
    // 3D model draws set ProjBuf externally to the per-frame model projection (3dStuff), so overwriting
    // it here with the renderer's current 2D ortho would project the skinned mesh off-screen and it
    // would render nothing (only its 2D nameplate remained). The other externally fed buffers keep
    // their last value inside the optionals to emulate GPU-buffer persistence; ProjBuf/MainTexBuf are
    // reset at the end of the draw so the next non-model draw re-derives them.
    if (_needProjBuf && !ProjBuf.has_value()) {
        auto& proj_buf = ProjBuf = ProjBuffer();
        ptr<float32_t> projection_matrix = proj_buf->ProjMatrix;
        ptr<const float32_t> projection_matrix_values = glm::value_ptr(_ctx->ProjMatrix);
        MemCopy(projection_matrix, projection_matrix_values, 16 * sizeof(float32_t));
    }

    if (_needMainTexBuf && !MainTexBuf.has_value()) {
        auto& main_tex_buf = MainTexBuf = MainTexBuffer();
        ptr<float32_t> main_texture_size = main_tex_buf->MainTexSize;
        ptr<const float32_t> main_texture_size_data = main_tex->SizeData;
        MemCopy(main_texture_size, main_texture_size_data, 4 * sizeof(float32_t));
    }

    const bool with_depth = IsTargetWithDepth(_ctx);

    auto render_pass = EnsureRenderPass(_ctx);
    auto cmd_buf = _ctx->CmdBuf.as_ptr();

    for (size_t pass = 0; pass < _passCount; pass++) {
#if FO_ENABLE_3D
        if (DisableShadow && _isShadow[pass]) {
            continue;
        }
#endif

        const auto& slots = PassSlots[pass];

        auto pipeline = GetOrCreatePipeline(pass, topology, with_depth);
        SDL_BindGPUGraphicsPipeline(render_pass.get(), pipeline.get());

        // Vertex and index buffers
        SDL_GPUBufferBinding vertex_binding = {};
        vertex_binding.buffer = sdl_dbuf->VertexBuf.get();
        SDL_BindGPUVertexBuffers(render_pass.get(), 0, &vertex_binding, 1);

        SDL_GPUBufferBinding index_binding = {};
        index_binding.buffer = sdl_dbuf->IndexBuf.get();

        if constexpr (sizeof(vindex_t) == 2) {
            SDL_BindGPUIndexBuffer(render_pass.get(), &index_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
        }
        else {
            SDL_BindGPUIndexBuffer(render_pass.get(), &index_binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
        }

        // Samplers
        const auto bind_sampler = [&](int32_t vert_slot, int32_t frag_slot, ptr<const SDLGpu_Texture> tex) {
            SDL_GPUTextureSamplerBinding sampler_binding = {};
            sampler_binding.texture = tex->TexHandle.get_no_const();
            sampler_binding.sampler = tex->LinearFiltered ? _ctx->LinearSampler.get() : _ctx->PointSampler.get();

            if (vert_slot != -1) {
                SDL_BindGPUVertexSamplers(render_pass.get(), numeric_cast<uint32_t>(vert_slot), &sampler_binding, 1);
            }
            if (frag_slot != -1) {
                SDL_BindGPUFragmentSamplers(render_pass.get(), numeric_cast<uint32_t>(frag_slot), &sampler_binding, 1);
            }
        };

        if (slots.VertMainTex != -1 || slots.FragMainTex != -1) {
            bind_sampler(slots.VertMainTex, slots.FragMainTex, main_tex);
        }

        if (slots.VertIndoorMaskTex != -1 || slots.FragIndoorMaskTex != -1) {
            auto indoor_tex_source = IndoorMaskTex ? IndoorMaskTex.as_ptr() : GetDummyTexture(_ctx);
            auto indoor_tex = RenderBackendCast<const SDLGpu_Texture>(indoor_tex_source);
            bind_sampler(slots.VertIndoorMaskTex, slots.FragIndoorMaskTex, indoor_tex);
        }

#if FO_ENABLE_3D
        if (_needModelTex[pass]) {
            for (size_t i = 0; i < MODEL_MAX_TEXTURES; i++) {
                if (slots.VertModelTex[i] != -1 || slots.FragModelTex[i] != -1) {
                    auto model_tex_source = ModelTex[i] ? ModelTex[i].as_ptr() : GetDummyTexture(_ctx);
                    auto model_tex = RenderBackendCast<const SDLGpu_Texture>(model_tex_source);
                    bind_sampler(slots.VertModelTex[i], slots.FragModelTex[i], model_tex);
                }
            }
        }
#endif

        // Uniform buffers (push-style, re-recorded on every draw)
        const auto push_uniform = [&](bool need_buf, const auto& opt_buf, int32_t vert_slot, int32_t frag_slot, optional<size_t> custom_size = std::nullopt) {
            if (!need_buf || !opt_buf.has_value()) {
                return;
            }

            const auto data_size = numeric_cast<uint32_t>(custom_size.value_or(sizeof(*opt_buf)));

            if (vert_slot != -1) {
                SDL_PushGPUVertexUniformData(cmd_buf.get(), numeric_cast<uint32_t>(vert_slot), &opt_buf.value(), data_size);
            }
            if (frag_slot != -1) {
                SDL_PushGPUFragmentUniformData(cmd_buf.get(), numeric_cast<uint32_t>(frag_slot), &opt_buf.value(), data_size);
            }
        };

        push_uniform(_needProjBuf, ProjBuf, slots.VertProjBuf, slots.FragProjBuf);
        push_uniform(_needMainTexBuf, MainTexBuf, slots.VertMainTexBuf, slots.FragMainTexBuf);
        push_uniform(_needEggBuf, EggBuf, slots.VertEggBuf, slots.FragEggBuf);
        push_uniform(_needSpriteBorderBuf, SpriteBorderBuf, slots.VertSpriteBorderBuf, slots.FragSpriteBorderBuf);
        push_uniform(_needTimeBuf, TimeBuf, slots.VertTimeBuf, slots.FragTimeBuf);
        push_uniform(_needRandomValueBuf, RandomValueBuf, slots.VertRandomValueBuf, slots.FragRandomValueBuf);
        push_uniform(_needScriptValueBuf, ScriptValueBuf, slots.VertScriptValueBuf, slots.FragScriptValueBuf);
        push_uniform(_needCameraBuf, CameraBuf, slots.VertCameraBuf, slots.FragCameraBuf);
#if FO_ENABLE_3D
        // Push the full ModelBuffer (matching the Vulkan backend) rather than trimming to
        // 32+64*MatrixCount bytes: SDL_GPU validates pushed uniform data against the shader's declared
        // UBO size, so a short push can trip validation. The unused tail matrices are harmless — the
        // shader reads only MatrixCount of them.
        push_uniform(_needModelBuf, ModelBuf, slots.VertModelBuf, slots.FragModelBuf, sizeof(ModelBuffer));
        push_uniform(_needModelTexBuf, ModelTexBuf, slots.VertModelTexBuf, slots.FragModelTexBuf);
        push_uniform(_needModelAnimBuf, ModelAnimBuf, slots.VertModelAnimBuf, slots.FragModelAnimBuf);
#endif

        SDL_DrawGPUIndexedPrimitives(render_pass.get(), draw_count, 1, numeric_cast<uint32_t>(start_index), 0, 0);
    }

    // Derived buffers are per-draw: clear them so the next draw re-derives ProjBuf/MainTexBuf from the
    // renderer state (or preserves a fresh externally-supplied ProjBuf, e.g. the next model's projection).
    ProjBuf.reset();
    MainTexBuf.reset();
}

FO_END_NAMESPACE

#endif
