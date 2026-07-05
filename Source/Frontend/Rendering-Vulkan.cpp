//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
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

#if FO_HAVE_VULKAN

#include "Application.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_video.h"
#include "SDL3/SDL_vulkan.h"
#include <vulkan/vulkan.h>

FO_BEGIN_NAMESPACE

static constexpr uint32_t VULKAN_MAX_TEXTURE_BINDINGS = 16;
static constexpr uint32_t VULKAN_MAX_UNIFORM_BINDINGS = 16;

// One growable, persistently-mapped HOST_VISIBLE buffer slot of a per-frame ring pool.
// Rings are reset when the context frame index advances (BeginFrame waits the queue idle,
// so every slot written in a previous frame is GPU-free by then) and only grow capacity;
// steady-state uploads are pure memcpy with no create/map/destroy traffic.
struct VulkanPooledBuffer
{
    VkBuffer Buffer {};
    VkDeviceMemory Memory {};
    VkDeviceSize Capacity {};
    nptr<void> Mapped {};
};

struct Vulkan_Renderer::Context
{
    Context(ptr<GlobalSettings> settings, ptr<SDL_Window> sdl_window) :
        Settings {settings},
        SdlWindow {sdl_window}
    {
    }

    ptr<GlobalSettings> Settings;
    ptr<SDL_Window> SdlWindow;
    VkInstance Instance {};
    VkPhysicalDevice PhysicalDevice {};
    VkDevice Device {};
    VkQueue GraphicsQueue {};
    VkSurfaceKHR Surface {};
    uint32_t GraphicsFamilyIndex {};
    VkSwapchainKHR Swapchain {};
    vector<VkImage> SwapchainImages {};
    vector<VkImageLayout> SwapchainImageLayouts {};
    vector<VkImageView> SwapchainImageViews {};
    VkFormat SwapchainFormat {};
    isize32 SwapchainSize {};
    VkRenderPass RenderPass {};
    vector<VkFramebuffer> Framebuffers {};
    VkImage SwapchainDepthImage {};
    VkImageView SwapchainDepthImageView {};
    VkDeviceMemory SwapchainDepthImageMemory {};
    VkCommandPool CommandPool {};
    VkCommandBuffer CommandBuffer {}; // Single command buffer (single-threaded)
    VkSemaphore ImageAvailableSemaphore {}; // Signaled when image acquired
    VkSemaphore RenderCompleteSemaphore {}; // Signaled when render done
    irect32 ViewPort {};
    isize32 TargetSize {};
    mat44 ProjMatrix {};
    float32_t OrthoNear {ORTHO_DEPTH_DEFAULT_NEAR};
    float32_t OrthoFar {ORTHO_DEPTH_DEFAULT_FAR};
    unique_nptr<RenderTexture> DummyTexture {};
    VkClearColorValue ClearColor {{0.0f, 0.0f, 0.0f, 1.0f}};
    VkCommandBuffer StagingCommandBuffer {};
    VkDescriptorSetLayout TextureDescriptorSetLayout {};
    VkDescriptorSetLayout UniformDescriptorSetLayout {};
    VkDescriptorPool FrameDescriptorPool {};
    VkBuffer FrameUniformBuffer {};
    VkDeviceMemory FrameUniformBufferMemory {};
    nptr<void> FrameUniformBufferMapped {};
    size_t FrameUniformOffset {};
    size_t FrameUniformBufferSize {numeric_cast<size_t>(16 * 1024 * 1024)}; // 16 MB
    VkSampler LinearSampler {};
    VkSampler PointSampler {};
    nptr<RenderTexture> CurrentRenderTarget {}; // Current render target (nullptr = swapchain)
    irect32 ScissorRect {};
    bool ScissorEnabled {};
    uint32_t CurrentSwapchainImageIndex {};
    optional<isize32> PendingSwapchainRecreateSize {};
    vector<VkBuffer> DeferredDestroyBuffers {};
    vector<VkDeviceMemory> DeferredDestroyMemories {};
    vector<VkImage> DeferredDestroyImages {};
    vector<VkImageView> DeferredDestroyImageViews {};
    vector<VkFramebuffer> DeferredDestroyFramebuffers {};

    VkDebugUtilsMessengerEXT DebugMessenger {};

    // Cached immutable device properties (queried once in Init) to avoid per-draw / per-allocation queries.
    VkDeviceSize MinUniformBufferOffsetAlignment {};
    VkPhysicalDeviceMemoryProperties MemoryProperties {};

    bool FrameCbRecording {}; // Frame command buffer is currently recording (between BeginFrame and Present)
    bool ImageAvailableWaited {}; // ImageAvailableSemaphore was consumed by a queue submit this frame

    // Monotonic frame counter advanced right after BeginFrame's queue-idle wait; ring pools
    // compare against it to know when previous-frame slots became reusable.
    uint64_t FrameIndex {1};

    // Per-frame staging ring for texture uploads recorded into the frame command buffer.
    vector<VulkanPooledBuffer> TextureStagingRing {};
    size_t TextureStagingRingIndex {};
    uint64_t TextureStagingRingFrame {};
};

class Vulkan_Texture;

static void FlushFrameCommandBufferMidFrame(ptr<Vulkan_Renderer::Context> ctx);
static auto BuildOrthoMatrix(float32_t left, float32_t right, float32_t bottom, float32_t top, float32_t nearp, float32_t farp) -> mat44;
static void ApplySwapchainTargetMetrics(ptr<Vulkan_Renderer::Context> ctx, isize32 back_buf_size);
static void RecreateSwapchain(ptr<Vulkan_Renderer::Context> ctx, isize32 size);
static void RecreateFrameSyncSemaphores(ptr<Vulkan_Renderer::Context> ctx);
static void AllocateBuffer(ptr<Vulkan_Renderer::Context> ctx, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory);
static void AllocateImage(ptr<Vulkan_Renderer::Context> ctx, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& memory);
static void EnsurePooledBufferCapacity(ptr<Vulkan_Renderer::Context> ctx, VulkanPooledBuffer& pooled, VkDeviceSize size, VkBufferUsageFlags usage);
static auto AcquireRingBuffer(ptr<Vulkan_Renderer::Context> ctx, vector<VulkanPooledBuffer>& ring, size_t& ring_index, uint64_t& ring_frame, VkDeviceSize size, VkBufferUsageFlags usage) -> VulkanPooledBuffer&;
static void DestroyPooledBuffers(ptr<Vulkan_Renderer::Context> ctx, vector<VulkanPooledBuffer>& ring);
static void BeginFrame(ptr<Vulkan_Renderer::Context> ctx);
static void EndFrame(ptr<Vulkan_Renderer::Context> ctx);
static void TransitionColorImage(VkCommandBuffer cmd_buf, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout);
static void BeginCurrentRenderPass(ptr<Vulkan_Renderer::Context> ctx);
static void EndCurrentRenderPass(ptr<Vulkan_Renderer::Context> ctx);
static void ApplyViewportAndScissor(ptr<Vulkan_Renderer::Context> ctx);
static void EnsureTextureRenderTargetResources(ptr<Vulkan_Renderer::Context> ctx, ptr<Vulkan_Texture> vk_tex);
static void DestroyResourceSafe(ptr<Vulkan_Renderer::Context> ctx, VkBuffer& buffer);
static void DestroyResourceSafe(ptr<Vulkan_Renderer::Context> ctx, VkDeviceMemory& memory);
static void DestroyResourceSafe(ptr<Vulkan_Renderer::Context> ctx, VkImage& image);
static void DestroyResourceSafe(ptr<Vulkan_Renderer::Context> ctx, VkImageView& image_view);
static void DestroyResourceSafe(ptr<Vulkan_Renderer::Context> ctx, VkFramebuffer& framebuffer);
static void FlushDeferredDestroyResources(ptr<Vulkan_Renderer::Context> ctx);
static void BeginCommandBufferRecording(VkCommandBuffer cmd_buf);
static void EndCommandBufferRecording(VkCommandBuffer cmd_buf);
static void SubmitCommandBufferAndWait(ptr<Vulkan_Renderer::Context> ctx, VkCommandBuffer cmd_buf);
static void ResetCommandBufferRecording(VkCommandBuffer cmd_buf);

template<typename T, typename U>
static auto RenderBackendCast(ptr<U> value) -> ptr<T>
{
    FO_STACK_TRACE_ENTRY();

    auto nullable_casted = value.template cast<T>();
    FO_VERIFY_AND_THROW(nullable_casted, "Render backend object is not of the expected Vulkan type");
    return nullable_casted.as_ptr();
}

static auto GetSdlWindow(nptr<WindowInternalHandle> window) -> ptr<SDL_Window>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(window, "Window handle is null");
    return cast_from_void<SDL_Window*>(window.get());
}

static auto GetDummyTexture(ptr<Vulkan_Renderer::Context> ctx) -> ptr<RenderTexture>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(ctx->DummyTexture, "Vulkan dummy texture is not created");
    auto dummy_texture = ctx->DummyTexture.as_ptr();
    return dummy_texture;
}

static auto GetMappedMemoryData(nptr<void> mapped_data) -> ptr<void>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(mapped_data, "Mapped memory data pointer is null");
    return mapped_data.as_ptr();
}

static auto GetMappedMemoryBytes(nptr<void> mapped_data) -> ptr<uint8_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(mapped_data, "Mapped memory data pointer is null");
    nptr<uint8_t> nullable_bytes = cast_from_void<uint8_t*>(mapped_data.get());
    FO_VERIFY_AND_THROW(nullable_bytes, "Mapped memory byte pointer is null");
    return nullable_bytes.as_ptr();
}

static auto OffsetMappedBytes(ptr<uint8_t> bytes, size_t offset) noexcept -> ptr<uint8_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    return ptr<uint8_t> {bytes.get() + offset};
}

static auto OffsetMappedBytes(ptr<const uint8_t> bytes, size_t offset) noexcept -> ptr<const uint8_t>
{
    FO_NO_STACK_TRACE_ENTRY();

    return ptr<const uint8_t> {bytes.get() + offset};
}

static void VerifyVkResult(VkResult vk_result)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (vk_result != VK_SUCCESS) {
        throw RenderingException("Vulkan error", vk_result);
    }
}

static void BeginCommandBufferRecording(VkCommandBuffer cmd_buf)
{
    FO_STACK_TRACE_ENTRY();

    VkCommandBufferBeginInfo begin_info {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    const auto vk_result = vkBeginCommandBuffer(cmd_buf, &begin_info);
    VerifyVkResult(vk_result);
}

static void EndCommandBufferRecording(VkCommandBuffer cmd_buf)
{
    FO_STACK_TRACE_ENTRY();

    const auto vk_result = vkEndCommandBuffer(cmd_buf);
    VerifyVkResult(vk_result);
}

static void SubmitCommandBufferAndWait(ptr<Vulkan_Renderer::Context> ctx, VkCommandBuffer cmd_buf)
{
    FO_STACK_TRACE_ENTRY();

    VkSubmitInfo submit_info {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buf;

    auto vk_result = vkQueueSubmit(ctx->GraphicsQueue, 1, &submit_info, VK_NULL_HANDLE);
    VerifyVkResult(vk_result);

    vk_result = vkQueueWaitIdle(ctx->GraphicsQueue);
    VerifyVkResult(vk_result);
}

static void ResetCommandBufferRecording(VkCommandBuffer cmd_buf)
{
    FO_STACK_TRACE_ENTRY();

    const auto vk_result = vkResetCommandBuffer(cmd_buf, 0);
    VerifyVkResult(vk_result);
}

static auto ConvertBlend(BlendFuncType blend, bool is_alpha) -> VkBlendFactor
{
    FO_STACK_TRACE_ENTRY();

    switch (blend) {
    case BlendFuncType::Zero:
        return VK_BLEND_FACTOR_ZERO;
    case BlendFuncType::One:
        return VK_BLEND_FACTOR_ONE;
    case BlendFuncType::SrcColor:
        return !is_alpha ? VK_BLEND_FACTOR_SRC_COLOR : VK_BLEND_FACTOR_SRC_ALPHA;
    case BlendFuncType::InvSrcColor:
        return !is_alpha ? VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case BlendFuncType::DstColor:
        return !is_alpha ? VK_BLEND_FACTOR_DST_COLOR : VK_BLEND_FACTOR_DST_ALPHA;
    case BlendFuncType::InvDstColor:
        return !is_alpha ? VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR : VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case BlendFuncType::SrcAlpha:
        return VK_BLEND_FACTOR_SRC_ALPHA;
    case BlendFuncType::InvSrcAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case BlendFuncType::DstAlpha:
        return VK_BLEND_FACTOR_DST_ALPHA;
    case BlendFuncType::InvDstAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case BlendFuncType::ConstantColor:
        return VK_BLEND_FACTOR_CONSTANT_COLOR;
    case BlendFuncType::InvConstantColor:
        return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
    case BlendFuncType::SrcAlphaSaturate:
        return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    }

    FO_UNREACHABLE_PLACE();
}

static auto ConvertBlendOp(BlendEquationType blend_op) -> VkBlendOp
{
    FO_STACK_TRACE_ENTRY();

    switch (blend_op) {
    case BlendEquationType::FuncAdd:
        return VK_BLEND_OP_ADD;
    case BlendEquationType::FuncSubtract:
        return VK_BLEND_OP_SUBTRACT;
    case BlendEquationType::FuncReverseSubtract:
        return VK_BLEND_OP_REVERSE_SUBTRACT;
    case BlendEquationType::Min:
        return VK_BLEND_OP_MIN;
    case BlendEquationType::Max:
        return VK_BLEND_OP_MAX;
    }

    FO_UNREACHABLE_PLACE();
}

static auto ConvertDepthFunc(DepthFuncType depth_func) -> VkCompareOp
{
    FO_STACK_TRACE_ENTRY();

    switch (depth_func) {
    case DepthFuncType::Always:
        return VK_COMPARE_OP_ALWAYS;
    case DepthFuncType::Never:
        return VK_COMPARE_OP_NEVER;
    case DepthFuncType::Less:
        return VK_COMPARE_OP_LESS;
    case DepthFuncType::LessEqual:
        return VK_COMPARE_OP_LESS_OR_EQUAL;
    case DepthFuncType::Equal:
        return VK_COMPARE_OP_EQUAL;
    case DepthFuncType::GreaterEqual:
        return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case DepthFuncType::Greater:
        return VK_COMPARE_OP_GREATER;
    case DepthFuncType::NotEqual:
        return VK_COMPARE_OP_NOT_EQUAL;
    }

    FO_UNREACHABLE_PLACE();
}

static auto ConvertPrimitive(RenderPrimitiveType prim_type) -> VkPrimitiveTopology
{
    FO_STACK_TRACE_ENTRY();

    switch (prim_type) {
    case RenderPrimitiveType::PointList:
        return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case RenderPrimitiveType::LineList:
        return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case RenderPrimitiveType::LineStrip:
        return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case RenderPrimitiveType::TriangleList:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case RenderPrimitiveType::TriangleStrip:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    }

    FO_UNREACHABLE_PLACE();
}

class Vulkan_Texture final : public RenderTexture
{
public:
    Vulkan_Texture(isize32 size, bool linear_filtered, bool with_depth, ptr<Vulkan_Renderer::Context> ctx);
    ~Vulkan_Texture() override;

    [[nodiscard]] auto GetTexturePixel(ipos32 pos) const -> ucolor override;
    [[nodiscard]] auto GetTextureRegion(ipos32 pos, isize32 size) const -> vector<ucolor> override;

    void UpdateTextureRegion(ipos32 pos, isize32 size, const_span<ucolor> data, bool use_dest_pitch) override;

    VkImage TextureImage {};
    VkImageView TextureImageView {};
    VkDeviceMemory TextureImageMemory {};
    VkDescriptorSet DescriptorSet {};
    VkFramebuffer TextureFramebuffer {};
    VkImageLayout TextureImageLayout {VK_IMAGE_LAYOUT_UNDEFINED};
    VkImage DepthImage {};
    VkImageView DepthImageView {};
    VkDeviceMemory DepthImageMemory {};

private:
    ptr<Vulkan_Renderer::Context> _ctx;
};

class Vulkan_DrawBuffer final : public RenderDrawBuffer
{
    friend class Vulkan_Effect;
    friend class Vulkan_Renderer;

public:
    Vulkan_DrawBuffer(bool is_static, ptr<Vulkan_Renderer::Context> ctx) :
        RenderDrawBuffer(is_static),
        _ctx {ctx}
    {
    }
    ~Vulkan_DrawBuffer() override;

    void Upload(EffectUsage usage, optional<size_t> custom_vertices_size, optional<size_t> custom_indices_size) override;

    // Buffers the next draw binds: the ring slot picked by the last dynamic Upload, or the
    // device-local buffer for static geometry.
    VkBuffer CurrentVertexBuffer {};
    VkBuffer CurrentIndexBuffer {};

    // Dynamic geometry ring pools (see VulkanPooledBuffer): one slot per Upload within a frame,
    // reset on frame advance, so re-uploading the same draw buffer mid-frame never overwrites
    // data a still-pending draw in the frame command buffer refers to.
    vector<VulkanPooledBuffer> VertexBufferRing {};
    size_t VertexRingIndex {};
    uint64_t VertexRingFrame {};
    vector<VulkanPooledBuffer> IndexBufferRing {};
    size_t IndexRingIndex {};
    uint64_t IndexRingFrame {};

    // Static geometry (uploaded once via staging copy to device-local memory).
    VkBuffer StaticVertexBuffer {};
    VkDeviceMemory StaticVertexBufferMemory {};
    VkBuffer StaticIndexBuffer {};
    VkDeviceMemory StaticIndexBufferMemory {};

private:
    ptr<Vulkan_Renderer::Context> _ctx;
};

class Vulkan_Effect final : public RenderEffect
{
    friend class Vulkan_Renderer;

public:
    Vulkan_Effect(EffectUsage usage, string_view name, const RenderEffectLoader& loader, ptr<Vulkan_Renderer::Context> ctx) :
        RenderEffect(usage, name, loader),
        _ctx {ctx}
    {
    }

    ~Vulkan_Effect() override;

    void DrawBuffer(ptr<RenderDrawBuffer> dbuf, size_t start_index, optional<size_t> indices_to_draw, nptr<const RenderTexture> custom_tex) override;

    VkShaderModule VertexShaderModule[EFFECT_MAX_PASSES] {};
    VkShaderModule FragmentShaderModule[EFFECT_MAX_PASSES] {};
    // Per-pass pipelines indexed by [pass][primitive_type][blend_disabled?].
    // Two variants per (pass, prim) so the engine's RenderEffect::DisableBlending flag — which
    // expresses opaque-blit semantics in OpenGL/D3D via glDisable(GL_BLEND)/null BlendState —
    // can be honoured here too. Without this, opaque blits like SpriteManager::EndScene's
    // _rtMain→virtual_render_tex flush keep alpha-blending and any zero-alpha pixels in the
    // source preserve the destination contents, which silently drops the HUD on Vulkan.
    VkPipeline Pipeline[EFFECT_MAX_PASSES][5][2] {};
    VkPipelineLayout PipelineLayout {};

private:
    ptr<Vulkan_Renderer::Context> _ctx;
};

// Submits the partially recorded frame command buffer and resumes recording. Needed before an
// immediate readback (GetTextureRegion) that must observe clears/draws/uploads recorded earlier
// this frame. Texture uploads no longer need this: they are recorded into the frame command
// buffer itself, so program order preserves the engine's immediate-mode semantics.
static void FlushFrameCommandBufferMidFrame(ptr<Vulkan_Renderer::Context> ctx)
{
    FO_STACK_TRACE_ENTRY();

    if (!ctx->FrameCbRecording) {
        return;
    }

    VkResult vk_result = VK_SUCCESS;

    EndCurrentRenderPass(ctx);
    EndCommandBufferRecording(ctx->CommandBuffer);

    // The first submit of the frame command buffer must wait for the swapchain image acquire,
    // because the buffer starts with the acquired image's layout transition and clear.
    constexpr VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &ctx->CommandBuffer;

    if (!ctx->ImageAvailableWaited) {
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &ctx->ImageAvailableSemaphore;
        submit_info.pWaitDstStageMask = &wait_stage;
        ctx->ImageAvailableWaited = true;
    }

    vk_result = vkQueueSubmit(ctx->GraphicsQueue, 1, &submit_info, VK_NULL_HANDLE);
    VerifyVkResult(vk_result);
    vk_result = vkQueueWaitIdle(ctx->GraphicsQueue);
    VerifyVkResult(vk_result);

    vk_result = vkResetCommandBuffer(ctx->CommandBuffer, 0);
    VerifyVkResult(vk_result);
    BeginCommandBufferRecording(ctx->CommandBuffer);
    BeginCurrentRenderPass(ctx);
}

static void AllocateBuffer(ptr<Vulkan_Renderer::Context> ctx, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory)
{
    FO_STACK_TRACE_ENTRY();

    VkResult vk_result = VK_SUCCESS;

    VkBufferCreateInfo buffer_ci {};
    buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_ci.size = size;
    buffer_ci.usage = usage;
    buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vk_result = vkCreateBuffer(ctx->Device, &buffer_ci, nullptr, &buffer);
    VerifyVkResult(vk_result);

    VkMemoryRequirements mem_req {};
    vkGetBufferMemoryRequirements(ctx->Device, buffer, &mem_req);

    // Find suitable memory type
    optional<uint32_t> mem_type_idx;
    const VkPhysicalDeviceMemoryProperties& mem_properties = ctx->MemoryProperties;

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((mem_req.memoryTypeBits & (1 << i)) != 0 && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            mem_type_idx = i;
            break;
        }
    }

    FO_VERIFY_AND_THROW(mem_type_idx.has_value(), "No suitable Vulkan memory type found");

    VkMemoryAllocateInfo alloc_info {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex = mem_type_idx.value();

    vk_result = vkAllocateMemory(ctx->Device, &alloc_info, nullptr, &memory);
    VerifyVkResult(vk_result);

    vk_result = vkBindBufferMemory(ctx->Device, buffer, memory, 0);
    VerifyVkResult(vk_result);
}

static void EnsurePooledBufferCapacity(ptr<Vulkan_Renderer::Context> ctx, VulkanPooledBuffer& pooled, VkDeviceSize size, VkBufferUsageFlags usage)
{
    FO_STACK_TRACE_ENTRY();

    if (pooled.Capacity >= size) {
        return;
    }

    // Growth is deferred-destroyed like any other in-flight resource; steady state never grows.
    DestroyResourceSafe(ctx, pooled.Buffer);
    DestroyResourceSafe(ctx, pooled.Memory);
    pooled.Mapped = nullptr;

    constexpr VkDeviceSize min_capacity = 4096;
    const VkDeviceSize new_capacity = std::max({size, pooled.Capacity * 2, min_capacity});

    AllocateBuffer(ctx, new_capacity, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, pooled.Buffer, pooled.Memory);
    pooled.Capacity = new_capacity;

    // Persistent mapping: slots stay mapped for their whole lifetime, uploads are plain memcpy.
    void* mapped_raw {};
    const auto vk_result = vkMapMemory(ctx->Device, pooled.Memory, 0, VK_WHOLE_SIZE, 0, &mapped_raw);
    VerifyVkResult(vk_result);
    pooled.Mapped = mapped_raw;
}

static auto AcquireRingBuffer(ptr<Vulkan_Renderer::Context> ctx, vector<VulkanPooledBuffer>& ring, size_t& ring_index, uint64_t& ring_frame, VkDeviceSize size, VkBufferUsageFlags usage) -> VulkanPooledBuffer&
{
    FO_STACK_TRACE_ENTRY();

    // Slots written in previous frames are GPU-free once BeginFrame's queue-idle wait has run,
    // which is exactly when the context frame index advances past this ring's stamp.
    if (ring_frame != ctx->FrameIndex) {
        ring_frame = ctx->FrameIndex;
        ring_index = 0;
    }

    if (ring_index >= ring.size()) {
        ring.emplace_back();
    }

    auto& pooled = ring[ring_index];
    ring_index++;

    EnsurePooledBufferCapacity(ctx, pooled, size, usage);
    return pooled;
}

static void DestroyPooledBuffers(ptr<Vulkan_Renderer::Context> ctx, vector<VulkanPooledBuffer>& ring)
{
    FO_STACK_TRACE_ENTRY();

    for (auto& pooled : ring) {
        DestroyResourceSafe(ctx, pooled.Buffer);
        DestroyResourceSafe(ctx, pooled.Memory);
        pooled.Mapped = nullptr;
        pooled.Capacity = 0;
    }

    ring.clear();
}

static void AllocateImage(ptr<Vulkan_Renderer::Context> ctx, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& memory)
{
    FO_STACK_TRACE_ENTRY();

    VkResult vk_result = VK_SUCCESS;

    VkImageCreateInfo image_ci {};
    image_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_ci.imageType = VK_IMAGE_TYPE_2D;
    image_ci.format = format;
    image_ci.extent = {.width = width, .height = height, .depth = 1};
    image_ci.mipLevels = 1;
    image_ci.arrayLayers = 1;
    image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
    image_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_ci.usage = usage;
    image_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    vk_result = vkCreateImage(ctx->Device, &image_ci, nullptr, &image);
    VerifyVkResult(vk_result);

    VkMemoryRequirements mem_req {};
    vkGetImageMemoryRequirements(ctx->Device, image, &mem_req);

    // Find suitable memory type
    optional<uint32_t> mem_type_idx;
    const VkPhysicalDeviceMemoryProperties& mem_properties = ctx->MemoryProperties;

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((mem_req.memoryTypeBits & (1 << i)) != 0 && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            mem_type_idx = i;
            break;
        }
    }

    FO_VERIFY_AND_THROW(mem_type_idx.has_value(), "No suitable Vulkan memory type found");

    VkMemoryAllocateInfo alloc_info {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex = mem_type_idx.value();

    vk_result = vkAllocateMemory(ctx->Device, &alloc_info, nullptr, &memory);
    VerifyVkResult(vk_result);

    vk_result = vkBindImageMemory(ctx->Device, image, memory, 0);
    VerifyVkResult(vk_result);
}

Vulkan_Texture::Vulkan_Texture(isize32 size, bool linear_filtered, bool with_depth, ptr<Vulkan_Renderer::Context> ctx) :
    RenderTexture(size, linear_filtered, with_depth),
    _ctx {ctx}
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_ctx->Device, "Vulkan device is not initialized");

    VkResult vk_result = VK_SUCCESS;

    AllocateImage(_ctx, numeric_cast<uint32_t>(Size.width), numeric_cast<uint32_t>(Size.height), VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, TextureImage, TextureImageMemory);

    VkImageViewCreateInfo image_view_ci {};
    image_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_ci.image = TextureImage;
    image_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_ci.format = VK_FORMAT_B8G8R8A8_UNORM;
    image_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_ci.subresourceRange.baseMipLevel = 0;
    image_view_ci.subresourceRange.levelCount = 1;
    image_view_ci.subresourceRange.baseArrayLayer = 0;
    image_view_ci.subresourceRange.layerCount = 1;

    vk_result = vkCreateImageView(_ctx->Device, &image_view_ci, nullptr, &TextureImageView);
    VerifyVkResult(vk_result);

    if (WithDepth) {
        AllocateImage(_ctx, numeric_cast<uint32_t>(Size.width), numeric_cast<uint32_t>(Size.height), VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DepthImage, DepthImageMemory);

        VkImageViewCreateInfo depth_view_ci {};
        depth_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depth_view_ci.image = DepthImage;
        depth_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depth_view_ci.format = VK_FORMAT_D32_SFLOAT;
        depth_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depth_view_ci.subresourceRange.baseMipLevel = 0;
        depth_view_ci.subresourceRange.levelCount = 1;
        depth_view_ci.subresourceRange.baseArrayLayer = 0;
        depth_view_ci.subresourceRange.layerCount = 1;

        vk_result = vkCreateImageView(_ctx->Device, &depth_view_ci, nullptr, &DepthImageView);
        VerifyVkResult(vk_result);
    }

    // Put texture into a valid sampling layout immediately.
    // Some textures are sampled before any UpdateTextureRegion call.
    BeginCommandBufferRecording(_ctx->StagingCommandBuffer);

    TransitionColorImage(_ctx->StagingCommandBuffer, TextureImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    EndCommandBufferRecording(_ctx->StagingCommandBuffer);
    SubmitCommandBufferAndWait(_ctx, _ctx->StagingCommandBuffer);

    TextureImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

Vulkan_Texture::~Vulkan_Texture()
{
    FO_STACK_TRACE_ENTRY();

    DestroyResourceSafe(_ctx, TextureFramebuffer);
    DestroyResourceSafe(_ctx, TextureImageView);
    DestroyResourceSafe(_ctx, TextureImage);
    DestroyResourceSafe(_ctx, TextureImageMemory);
    DestroyResourceSafe(_ctx, DepthImageView);
    DestroyResourceSafe(_ctx, DepthImage);
    DestroyResourceSafe(_ctx, DepthImageMemory);
}

auto Vulkan_Texture::GetTexturePixel(ipos32 pos) const -> ucolor
{
    FO_STACK_TRACE_ENTRY();

    const auto region = GetTextureRegion(pos, {1, 1});
    return !region.empty() ? region[0] : ucolor::clear;
}

auto Vulkan_Texture::GetTextureRegion(ipos32 pos, isize32 size) const -> vector<ucolor>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_ctx->Device, "Vulkan device is not initialized");
    FO_VERIFY_AND_THROW(TextureImage, "Vulkan texture image is not created");

    // Clears and draws recorded earlier this frame are still pending in the frame command buffer
    // while this readback executes immediately; flush them first so the read observes the same
    // ordering the immediate-mode backends provide.
    FlushFrameCommandBufferMidFrame(_ctx);

    VkResult vk_result = VK_SUCCESS;
    vector<ucolor> tex_region;
    const VkDeviceSize region_size = size.square() * sizeof(ucolor);

    // Create staging buffer for reading from GPU
    VkBuffer staging_buf {};
    VkDeviceMemory staging_mem {};
    AllocateBuffer(_ctx, region_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buf, staging_mem);

    // Record copy commands using standalone staging command buffer
    BeginCommandBufferRecording(_ctx->StagingCommandBuffer);

    const auto old_layout = TextureImageLayout;
    TransitionColorImage(_ctx->StagingCommandBuffer, TextureImage, old_layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    // Copy region from GPU to staging buffer
    VkBufferImageCopy region {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {.x = numeric_cast<int32_t>(pos.x), .y = numeric_cast<int32_t>(pos.y), .z = 0};
    region.imageExtent = {.width = numeric_cast<uint32_t>(size.width), .height = numeric_cast<uint32_t>(size.height), .depth = 1};

    vkCmdCopyImageToBuffer(_ctx->StagingCommandBuffer, TextureImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging_buf, 1, &region);

    TransitionColorImage(_ctx->StagingCommandBuffer, TextureImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, old_layout);

    EndCommandBufferRecording(_ctx->StagingCommandBuffer);
    SubmitCommandBufferAndWait(_ctx, _ctx->StagingCommandBuffer);

    // Read data from staging buffer
    void* map_data_raw {};
    vk_result = vkMapMemory(_ctx->Device, staging_mem, 0, region_size, 0, &map_data_raw);
    VerifyVkResult(vk_result);
    auto map_data = GetMappedMemoryData(map_data_raw);
    tex_region.resize(size.square());
    MemCopy(tex_region.data(), map_data, region_size);
    vkUnmapMemory(_ctx->Device, staging_mem);

    // Swizzle B↔R: VK_FORMAT_B8G8R8A8_UNORM stores {B,G,R,A} but ucolor expects {R,G,B,A}
    {
        auto* pixels = reinterpret_cast<uint8_t*>(tex_region.data());
        for (size_t i = 0; i < tex_region.size(); i++) {
            std::swap(pixels[i * 4 + 0], pixels[i * 4 + 2]);
        }
    }

    // Cleanup
    vkDestroyBuffer(_ctx->Device, staging_buf, nullptr);
    vkFreeMemory(_ctx->Device, staging_mem, nullptr);

    ResetCommandBufferRecording(_ctx->StagingCommandBuffer);

    return tex_region;
}

void Vulkan_Texture::UpdateTextureRegion(ipos32 pos, isize32 size, const_span<ucolor> data, bool use_dest_pitch)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_ctx->Device, "Vulkan device is not initialized");
    FO_VERIFY_AND_THROW(pos.x >= 0, "Texture region is out of bounds");
    FO_VERIFY_AND_THROW(pos.y >= 0, "Texture region is out of bounds");
    FO_VERIFY_AND_THROW(pos.x + size.width <= Size.width, "Texture region is out of bounds");
    FO_VERIFY_AND_THROW(pos.y + size.height <= Size.height, "Texture region is out of bounds");

    // Determine data pitch:
    // - use_dest_pitch=false: data is tightly packed for the region (size.width stride)
    // - use_dest_pitch=true: data has the full destination texture pitch (Size.width stride)
    const size_t row_bytes = numeric_cast<size_t>(size.width) * sizeof(ucolor);
    const size_t data_stride = use_dest_pitch ? (numeric_cast<size_t>(Size.width) * sizeof(ucolor)) : row_bytes;
    const VkDeviceSize total_data_size = numeric_cast<VkDeviceSize>(size.height) * data_stride;

    const size_t src_pitch_size = numeric_cast<size_t>(use_dest_pitch ? Size.width : size.width);
    const size_t required_size = size.height != 0 ? (numeric_cast<size_t>(size.height - 1) * src_pitch_size + numeric_cast<size_t>(size.width)) : 0;
    FO_VERIFY_AND_THROW(data.size() >= required_size, "Texture update source data is smaller than the required region size", data.size(), required_size, size, use_dest_pitch);

    const nptr<const ucolor> nullable_source_data = data.data();
    FO_VERIFY_AND_THROW(!!nullable_source_data, "Texture update source data is null");
    auto source_bytes = nullable_source_data.as_ptr().reinterpret_as<uint8_t>();

    // Fills a mapped staging area with the region pixels, swizzled R<->B: ucolor stores {R,G,B,A}
    // but VK_FORMAT_B8G8R8A8_UNORM expects {B,G,R,A}. The swizzle happens while composing from the
    // source so the staging memory is write-only: HOST_VISIBLE memory is typically write-combined,
    // and reading it back (e.g. an in-place swap loop) runs uncached and costs milliseconds per
    // large region. Only the first row_bytes of each data_stride row are populated; on the
    // use_dest_pitch path the inter-row gap is left untouched (vkCmdCopyBufferToImage skips it via
    // bufferRowLength; when use_dest_pitch is false, data_stride == row_bytes and rows are tight).
    const auto fill_staging = [&](ptr<uint8_t> mapped_bytes) {
        const size_t pixels_per_row = numeric_cast<size_t>(size.width);

        for (int32_t y = 0; y < size.height; y++) {
            const size_t row_offset = numeric_cast<size_t>(y) * data_stride;
            const uint8_t* src_row = OffsetMappedBytes(source_bytes, row_offset).get();
            uint8_t* dst_row = OffsetMappedBytes(mapped_bytes, row_offset).get();

            for (size_t i = 0; i < pixels_per_row; i++) {
                dst_row[i * 4 + 0] = src_row[i * 4 + 2];
                dst_row[i * 4 + 1] = src_row[i * 4 + 1];
                dst_row[i * 4 + 2] = src_row[i * 4 + 0];
                dst_row[i * 4 + 3] = src_row[i * 4 + 3];
            }
        }
    };

    VkResult vk_result = VK_SUCCESS;

    // Create GPU texture image if needed
    if (TextureImage == nullptr) {
        AllocateImage(_ctx, numeric_cast<uint32_t>(Size.width), numeric_cast<uint32_t>(Size.height), VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, TextureImage, TextureImageMemory);

        // Create image view
        VkImageViewCreateInfo image_view_ci {};
        image_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_ci.image = TextureImage;
        image_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_ci.format = VK_FORMAT_B8G8R8A8_UNORM;
        image_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_ci.subresourceRange.baseMipLevel = 0;
        image_view_ci.subresourceRange.levelCount = 1;
        image_view_ci.subresourceRange.baseArrayLayer = 0;
        image_view_ci.subresourceRange.layerCount = 1;

        vk_result = vkCreateImageView(_ctx->Device, &image_view_ci, nullptr, &TextureImageView);
        VerifyVkResult(vk_result);

        // Create depth buffer if requested
        if (WithDepth) {
            AllocateImage(_ctx, numeric_cast<uint32_t>(Size.width), numeric_cast<uint32_t>(Size.height), VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DepthImage, DepthImageMemory);

            // Create depth image view
            VkImageViewCreateInfo depth_view_ci {};
            depth_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            depth_view_ci.image = DepthImage;
            depth_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            depth_view_ci.format = VK_FORMAT_D32_SFLOAT;
            depth_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            depth_view_ci.subresourceRange.baseMipLevel = 0;
            depth_view_ci.subresourceRange.levelCount = 1;
            depth_view_ci.subresourceRange.baseArrayLayer = 0;
            depth_view_ci.subresourceRange.layerCount = 1;

            vk_result = vkCreateImageView(_ctx->Device, &depth_view_ci, nullptr, &DepthImageView);
            VerifyVkResult(vk_result);
        }
    }

    // Copy staging buffer to image region with position offset
    VkBufferImageCopy region {};
    region.bufferOffset = 0;
    region.bufferRowLength = use_dest_pitch ? numeric_cast<uint32_t>(Size.width) : 0;
    region.bufferImageHeight = use_dest_pitch ? numeric_cast<uint32_t>(size.height) : 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {.x = numeric_cast<int32_t>(pos.x), .y = numeric_cast<int32_t>(pos.y), .z = 0};
    region.imageExtent = {.width = numeric_cast<uint32_t>(size.width), .height = numeric_cast<uint32_t>(size.height), .depth = 1};

    const auto old_layout = TextureImageLayout;

    if (_ctx->FrameCbRecording) {
        // Record the upload into the frame command buffer: program order inside the one buffer
        // preserves the engine's immediate-mode ordering (an atlas clear recorded earlier this
        // frame executes before this copy) with no mid-frame submit or full-GPU wait. The pooled
        // staging slot stays untouched by the CPU for the rest of the frame and is only reused
        // after BeginFrame's queue-idle wait.
        auto& staging = AcquireRingBuffer(_ctx, _ctx->TextureStagingRing, _ctx->TextureStagingRingIndex, _ctx->TextureStagingRingFrame, total_data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        fill_staging(GetMappedMemoryBytes(staging.Mapped));

        // Transfers are illegal inside a render pass; suspend it around the copy.
        EndCurrentRenderPass(_ctx);

        TransitionColorImage(_ctx->CommandBuffer, TextureImage, old_layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        vkCmdCopyBufferToImage(_ctx->CommandBuffer, staging.Buffer, TextureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        TransitionColorImage(_ctx->CommandBuffer, TextureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        TextureImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        BeginCurrentRenderPass(_ctx);
    }
    else {
        // No frame is recording (texture initialization outside the render loop): perform the
        // upload immediately through the standalone staging command buffer.
        VkBuffer staging_buf {};
        VkDeviceMemory staging_mem {};
        AllocateBuffer(_ctx, total_data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buf, staging_mem);

        void* mapped_data_raw {};
        vk_result = vkMapMemory(_ctx->Device, staging_mem, 0, total_data_size, 0, &mapped_data_raw);
        VerifyVkResult(vk_result);
        fill_staging(GetMappedMemoryBytes(mapped_data_raw));
        vkUnmapMemory(_ctx->Device, staging_mem);

        BeginCommandBufferRecording(_ctx->StagingCommandBuffer);

        TransitionColorImage(_ctx->StagingCommandBuffer, TextureImage, old_layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        vkCmdCopyBufferToImage(_ctx->StagingCommandBuffer, staging_buf, TextureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        TransitionColorImage(_ctx->StagingCommandBuffer, TextureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        TextureImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        EndCommandBufferRecording(_ctx->StagingCommandBuffer);
        SubmitCommandBufferAndWait(_ctx, _ctx->StagingCommandBuffer);

        // Free staging buffer after GPU is done
        vkDestroyBuffer(_ctx->Device, staging_buf, nullptr);
        vkFreeMemory(_ctx->Device, staging_mem, nullptr);

        ResetCommandBufferRecording(_ctx->StagingCommandBuffer);
    }
}

Vulkan_DrawBuffer::~Vulkan_DrawBuffer()
{
    FO_STACK_TRACE_ENTRY();

    DestroyPooledBuffers(_ctx, VertexBufferRing);
    DestroyPooledBuffers(_ctx, IndexBufferRing);
    DestroyResourceSafe(_ctx, StaticVertexBuffer);
    DestroyResourceSafe(_ctx, StaticVertexBufferMemory);
    DestroyResourceSafe(_ctx, StaticIndexBuffer);
    DestroyResourceSafe(_ctx, StaticIndexBufferMemory);
}

void Vulkan_DrawBuffer::Upload(EffectUsage usage, optional<size_t> custom_vertices_size, optional<size_t> custom_indices_size)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_ctx->Device, "Vulkan device is not initialized");

    // Match OpenGL behavior: skip only for static buffers with no changes
    if (IsStatic && !StaticDataChanged) {
        return;
    }

    StaticDataChanged = false;

    VkResult vk_result = VK_SUCCESS;
    size_t vert_size = custom_vertices_size.value_or(VertCount) * sizeof(Vertex2D);
    const size_t idx_size = custom_indices_size.value_or(IndCount) * sizeof(vindex_t);

#if FO_ENABLE_3D
    if (usage == EffectUsage::Model) {
        vert_size = custom_vertices_size.value_or(VertCount) * sizeof(Vertex3D);
    }
#else
    ignore_unused(usage);
#endif

    const nptr<const void> vert_data =
#if FO_ENABLE_3D
        usage == EffectUsage::Model ? static_cast<const void*>(Vertices3D.data()) :
#endif
                                      static_cast<const void*>(Vertices.data());

    if (vert_size != 0) {
        if (!IsStatic) {
            // Dynamic geometry: memcpy into the next persistently-mapped ring slot. Each Upload
            // gets its own slot so earlier draws recorded in the still-pending frame command
            // buffer keep their vertex snapshots intact.
            auto& pooled = AcquireRingBuffer(_ctx, VertexBufferRing, VertexRingIndex, VertexRingFrame, vert_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
            MemCopy(GetMappedMemoryData(pooled.Mapped), vert_data.get(), vert_size);
            CurrentVertexBuffer = pooled.Buffer;
        }
        else {
            // Static buffers: use staging copy to GPU-local memory
            DestroyResourceSafe(_ctx, StaticVertexBuffer);
            DestroyResourceSafe(_ctx, StaticVertexBufferMemory);

            VkBuffer staging_vert_buf {};
            VkDeviceMemory staging_vert_mem {};

            AllocateBuffer(_ctx, vert_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_vert_buf, staging_vert_mem);

            void* mapped_data_raw {};
            vk_result = vkMapMemory(_ctx->Device, staging_vert_mem, 0, vert_size, 0, &mapped_data_raw);
            VerifyVkResult(vk_result);
            MemCopy(GetMappedMemoryData(mapped_data_raw), vert_data.get(), vert_size);
            vkUnmapMemory(_ctx->Device, staging_vert_mem);

            AllocateBuffer(_ctx, vert_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, StaticVertexBuffer, StaticVertexBufferMemory);

            VkBufferCopy copy_region {};
            copy_region.size = vert_size;

            BeginCommandBufferRecording(_ctx->StagingCommandBuffer);

            vkCmdCopyBuffer(_ctx->StagingCommandBuffer, staging_vert_buf, StaticVertexBuffer, 1, &copy_region);

            EndCommandBufferRecording(_ctx->StagingCommandBuffer);
            SubmitCommandBufferAndWait(_ctx, _ctx->StagingCommandBuffer);

            vkDestroyBuffer(_ctx->Device, staging_vert_buf, nullptr);
            vkFreeMemory(_ctx->Device, staging_vert_mem, nullptr);

            ResetCommandBufferRecording(_ctx->StagingCommandBuffer);

            CurrentVertexBuffer = StaticVertexBuffer;
        }
    }

    if (idx_size != 0) {
        if (!IsStatic) {
            // Dynamic geometry: same ring-slot scheme as the vertex path.
            auto& pooled = AcquireRingBuffer(_ctx, IndexBufferRing, IndexRingIndex, IndexRingFrame, idx_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
            MemCopy(GetMappedMemoryData(pooled.Mapped), Indices.data(), idx_size);
            CurrentIndexBuffer = pooled.Buffer;
        }
        else {
            // Static buffers: use staging copy to GPU-local memory
            DestroyResourceSafe(_ctx, StaticIndexBuffer);
            DestroyResourceSafe(_ctx, StaticIndexBufferMemory);

            VkBuffer staging_idx_buf {};
            VkDeviceMemory staging_idx_mem {};
            AllocateBuffer(_ctx, idx_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_idx_buf, staging_idx_mem);

            void* mapped_data_raw {};
            vk_result = vkMapMemory(_ctx->Device, staging_idx_mem, 0, idx_size, 0, &mapped_data_raw);
            VerifyVkResult(vk_result);
            MemCopy(GetMappedMemoryData(mapped_data_raw), Indices.data(), idx_size);
            vkUnmapMemory(_ctx->Device, staging_idx_mem);

            AllocateBuffer(_ctx, idx_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, StaticIndexBuffer, StaticIndexBufferMemory);

            VkBufferCopy copy_region {};
            copy_region.size = idx_size;

            BeginCommandBufferRecording(_ctx->StagingCommandBuffer);

            vkCmdCopyBuffer(_ctx->StagingCommandBuffer, staging_idx_buf, StaticIndexBuffer, 1, &copy_region);

            EndCommandBufferRecording(_ctx->StagingCommandBuffer);
            SubmitCommandBufferAndWait(_ctx, _ctx->StagingCommandBuffer);

            vkDestroyBuffer(_ctx->Device, staging_idx_buf, nullptr);
            vkFreeMemory(_ctx->Device, staging_idx_mem, nullptr);

            ResetCommandBufferRecording(_ctx->StagingCommandBuffer);

            CurrentIndexBuffer = StaticIndexBuffer;
        }
    }
}

Vulkan_Effect::~Vulkan_Effect()
{
    FO_STACK_TRACE_ENTRY();

    for (size_t pass = 0; pass < EFFECT_MAX_PASSES; pass++) {
        if (VertexShaderModule[pass] != nullptr) {
            vkDestroyShaderModule(_ctx->Device, VertexShaderModule[pass], nullptr);
            VertexShaderModule[pass] = VK_NULL_HANDLE;
        }
        if (FragmentShaderModule[pass] != nullptr) {
            vkDestroyShaderModule(_ctx->Device, FragmentShaderModule[pass], nullptr);
            FragmentShaderModule[pass] = VK_NULL_HANDLE;
        }
        for (size_t prim = 0; prim < 5; prim++) {
            for (size_t blend_disabled = 0; blend_disabled < 2; blend_disabled++) {
                if (Pipeline[pass][prim][blend_disabled] != nullptr) {
                    vkDestroyPipeline(_ctx->Device, Pipeline[pass][prim][blend_disabled], nullptr);
                    Pipeline[pass][prim][blend_disabled] = VK_NULL_HANDLE;
                }
            }
        }
    }

    if (PipelineLayout != nullptr) {
        vkDestroyPipelineLayout(_ctx->Device, PipelineLayout, nullptr);
        PipelineLayout = VK_NULL_HANDLE;
    }
}

void Vulkan_Effect::DrawBuffer(ptr<RenderDrawBuffer> dbuf, size_t start_index, optional<size_t> indices_to_draw, nptr<const RenderTexture> custom_tex)
{
    FO_STACK_TRACE_ENTRY();

    if (_ctx->Device == nullptr) {
        return;
    }

    auto vk_dbuf = RenderBackendCast<Vulkan_DrawBuffer>(dbuf);

#if FO_ENABLE_3D
    if (!custom_tex && ModelTex[0]) {
        custom_tex = ModelTex[0];
    }
#endif
    if (!custom_tex && MainTex) {
        custom_tex = MainTex;
    }

    auto main_tex_source = custom_tex ? custom_tex.as_ptr() : GetDummyTexture(_ctx);
    auto main_tex = RenderBackendCast<const Vulkan_Texture>(main_tex_source);

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

    // Default-initialize every other required uniform that the engine didn't fill in for this
    // draw. OpenGL and D3D get away with leaving such bindings unset (the previous frame's bind
    // persists in the pipeline state and is typically zero by the time the shader sees it),
    // but in Vulkan we allocate a fresh descriptor set per draw — any binding we don't write
    // is undefined and the shader can read garbage. Symptoms include random per-fragment alpha
    // collapse via 2D_Default.fofx's egg-cutout branch, which silently erases UI pixels even
    // though the per-vertex EggFlags are zero on a "miss" path because the gating compare
    // `EggData[0].z > 0.0` becomes true on garbage data.
    if (_needEggBuf && !EggBuf.has_value()) {
        EggBuf = EggBuffer();
    }
    if (_needSpriteBorderBuf && !SpriteBorderBuf.has_value()) {
        SpriteBorderBuf = SpriteBorderBuffer();
    }
    if (_needTimeBuf && !TimeBuf.has_value()) {
        TimeBuf = TimeBuffer();
    }
    if (_needRandomValueBuf && !RandomValueBuf.has_value()) {
        RandomValueBuf = RandomValueBuffer();
    }
    if (_needScriptValueBuf && !ScriptValueBuf.has_value()) {
        ScriptValueBuf = ScriptValueBuffer();
    }
    if (_needCameraBuf && !CameraBuf.has_value()) {
        CameraBuf = CameraBuffer();
    }
#if FO_ENABLE_3D
    if (_needModelBuf && !ModelBuf.has_value()) {
        ModelBuf = ModelBuffer();
    }
    if (_needModelTexBuf && !ModelTexBuf.has_value()) {
        ModelTexBuf = ModelTexBuffer();
    }
    if (_needModelAnimBuf && !ModelAnimBuf.has_value()) {
        ModelAnimBuf = ModelAnimBuffer();
    }
#endif

    // Skip until geometry is uploaded
    if (vk_dbuf->CurrentVertexBuffer == nullptr) {
        return;
    }

    // Skip if no geometry to render
    if (vk_dbuf->IndCount == 0 && vk_dbuf->VertCount == 0) {
        return;
    }

    // Bind vertex buffer
    constexpr VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(_ctx->CommandBuffer, 0, 1, &vk_dbuf->CurrentVertexBuffer, offsets);

    // Process each pass
    for (size_t pass = 0; pass < _passCount; pass++) {
        const auto prim_type = _usage == EffectUsage::Primitive ? vk_dbuf->PrimType : RenderPrimitiveType::TriangleList;
        const auto prim_index = static_cast<size_t>(prim_type);
        FO_VERIFY_AND_THROW(prim_index < 5, "Invalid render primitive type");

        // Bind pipeline for current pass — pick the variant that matches the current
        // DisableBlending flag (opaque writes for the blit-style flushes, alpha-blend otherwise).
        const size_t blend_variant = DisableBlending ? 1 : 0;
        if (Pipeline[pass][prim_index][blend_variant] != nullptr) {
            vkCmdBindPipeline(_ctx->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline[pass][prim_index][blend_variant]);
        }

        // Allocate descriptor sets for this draw call
        VkDescriptorSet texture_set = VK_NULL_HANDLE;
        VkDescriptorSet uniform_set = VK_NULL_HANDLE;

        VkDescriptorSetAllocateInfo alloc_info {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = _ctx->FrameDescriptorPool;
        alloc_info.descriptorSetCount = 1;

        if (_ctx->TextureDescriptorSetLayout != nullptr) {
            alloc_info.pSetLayouts = &_ctx->TextureDescriptorSetLayout;
            const auto vk_result = vkAllocateDescriptorSets(_ctx->Device, &alloc_info, &texture_set);
            VerifyVkResult(vk_result);
        }

        if (_ctx->UniformDescriptorSetLayout != nullptr) {
            alloc_info.pSetLayouts = &_ctx->UniformDescriptorSetLayout;
            const auto vk_result = vkAllocateDescriptorSets(_ctx->Device, &alloc_info, &uniform_set);
            VerifyVkResult(vk_result);
        }

        // Update and bind per-pass texture descriptor set (set = 1)
        if (texture_set != VK_NULL_HANDLE && this->PipelineLayout != nullptr) {
            VkDescriptorImageInfo image_infos[16];
            VkWriteDescriptorSet writes[16];
            size_t write_count = 0;

            const auto append_sampler = [&](int32_t binding, ptr<const Vulkan_Texture> tex) {
                if (binding < 0 || tex->TextureImageView == nullptr) {
                    return;
                }

                FO_VERIFY_AND_THROW(write_count < 16, "Too many Vulkan descriptor writes");

                auto& image_info = image_infos[write_count];
                image_info.sampler = tex->LinearFiltered ? _ctx->LinearSampler : _ctx->PointSampler;
                image_info.imageView = tex->TextureImageView;
                image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                auto& write = writes[write_count];
                write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write.pNext = nullptr;
                write.dstSet = texture_set;
                write.dstBinding = numeric_cast<uint32_t>(binding);
                write.dstArrayElement = 0;
                write.descriptorCount = 1;
                write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                write.pImageInfo = &image_info;
                write.pBufferInfo = nullptr;
                write.pTexelBufferView = nullptr;

                write_count++;
            };

            append_sampler(_posMainTex[pass], main_tex);

            if (_posIndoorMaskTex[pass] != -1) {
                auto indoor_tex_source = IndoorMaskTex ? IndoorMaskTex.as_ptr() : GetDummyTexture(_ctx);
                auto indoor_tex = RenderBackendCast<const Vulkan_Texture>(indoor_tex_source);
                append_sampler(_posIndoorMaskTex[pass], indoor_tex);
            }

#if FO_ENABLE_3D
            for (size_t model_tex_index = 0; model_tex_index < MODEL_MAX_TEXTURES; model_tex_index++) {
                auto model_tex_source = ModelTex[model_tex_index] ? ModelTex[model_tex_index].as_ptr() : GetDummyTexture(_ctx);
                auto model_tex = RenderBackendCast<const Vulkan_Texture>(model_tex_source);
                append_sampler(_posModelTex[pass][model_tex_index], model_tex);
            }
#endif

            if (write_count > 0) {
                vkUpdateDescriptorSets(_ctx->Device, numeric_cast<uint32_t>(write_count), writes, 0, nullptr);
            }

            vkCmdBindDescriptorSets(_ctx->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, this->PipelineLayout, 1, 1, &texture_set, 0, nullptr);
        }

        // Update and bind per-pass uniform descriptor set (set = 0)
        if (uniform_set != VK_NULL_HANDLE && this->PipelineLayout != nullptr) {
            VkDescriptorBufferInfo buffer_infos[16];
            VkWriteDescriptorSet writes[16];
            size_t write_count = 0;

            const size_t alignment = numeric_cast<size_t>(_ctx->MinUniformBufferOffsetAlignment);

            const auto upload_uniform_buffer = [&](int32_t binding, const void* src_data, size_t src_size) {
                if (binding < 0 || src_data == nullptr || src_size == 0) {
                    return;
                }

                FO_VERIFY_AND_THROW(write_count < 16, "Too many Vulkan descriptor writes");

                // Align offset
                _ctx->FrameUniformOffset = (_ctx->FrameUniformOffset + alignment - 1) & ~(alignment - 1);
                FO_VERIFY_AND_THROW(_ctx->FrameUniformOffset + src_size <= _ctx->FrameUniformBufferSize, "Frame uniform buffer overflow");

                // Copy data
                auto mapped_bytes = GetMappedMemoryBytes(_ctx->FrameUniformBufferMapped);
                MemCopy(OffsetMappedBytes(mapped_bytes, _ctx->FrameUniformOffset), src_data, src_size);

                auto& buffer_info = buffer_infos[write_count];
                buffer_info.buffer = _ctx->FrameUniformBuffer;
                buffer_info.offset = _ctx->FrameUniformOffset;
                buffer_info.range = numeric_cast<VkDeviceSize>(src_size);

                auto& write = writes[write_count];
                write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write.pNext = nullptr;
                write.dstSet = uniform_set;
                write.dstBinding = numeric_cast<uint32_t>(binding);
                write.dstArrayElement = 0;
                write.descriptorCount = 1;
                write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                write.pImageInfo = nullptr;
                write.pBufferInfo = &buffer_info;
                write.pTexelBufferView = nullptr;

                write_count++;
                _ctx->FrameUniformOffset += src_size;
            };

            if (ProjBuf.has_value()) {
                upload_uniform_buffer(_posProjBuf[pass], &ProjBuf.value(), sizeof(ProjBuffer));
            }
            if (MainTexBuf.has_value()) {
                upload_uniform_buffer(_posMainTexBuf[pass], &MainTexBuf.value(), sizeof(MainTexBuffer));
            }
            if (EggBuf.has_value()) {
                upload_uniform_buffer(_posEggBuf[pass], &EggBuf.value(), sizeof(EggBuffer));
            }
            if (SpriteBorderBuf.has_value()) {
                upload_uniform_buffer(_posSpriteBorderBuf[pass], &SpriteBorderBuf.value(), sizeof(SpriteBorderBuffer));
            }
            if (TimeBuf.has_value()) {
                upload_uniform_buffer(_posTimeBuf[pass], &TimeBuf.value(), sizeof(TimeBuffer));
            }
            if (RandomValueBuf.has_value()) {
                upload_uniform_buffer(_posRandomValueBuf[pass], &RandomValueBuf.value(), sizeof(RandomValueBuffer));
            }
            if (ScriptValueBuf.has_value()) {
                upload_uniform_buffer(_posScriptValueBuf[pass], &ScriptValueBuf.value(), sizeof(ScriptValueBuffer));
            }
            if (CameraBuf.has_value()) {
                upload_uniform_buffer(_posCameraBuf[pass], &CameraBuf.value(), sizeof(CameraBuffer));
            }
#if FO_ENABLE_3D
            if (ModelBuf.has_value()) {
                upload_uniform_buffer(_posModelBuf[pass], &ModelBuf.value(), sizeof(ModelBuffer));
            }
            if (ModelTexBuf.has_value()) {
                upload_uniform_buffer(_posModelTexBuf[pass], &ModelTexBuf.value(), sizeof(ModelTexBuffer));
            }
            if (ModelAnimBuf.has_value()) {
                upload_uniform_buffer(_posModelAnimBuf[pass], &ModelAnimBuf.value(), sizeof(ModelAnimBuffer));
            }
#endif

            if (write_count > 0) {
                vkUpdateDescriptorSets(_ctx->Device, numeric_cast<uint32_t>(write_count), writes, 0, nullptr);
                vkCmdBindDescriptorSets(_ctx->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, this->PipelineLayout, 0, 1, &uniform_set, 0, nullptr);
            }
        }

        // Draw indexed or non-indexed
        if (vk_dbuf->IndCount != 0 && vk_dbuf->CurrentIndexBuffer != nullptr) {
            // Bind index buffer and draw indexed
            // ReSharper disable once CppUnreachableCode
            constexpr auto index_type = sizeof(vindex_t) == sizeof(uint32_t) ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
            vkCmdBindIndexBuffer(_ctx->CommandBuffer, vk_dbuf->CurrentIndexBuffer, 0, index_type);

            const size_t num_indices = indices_to_draw.value_or(vk_dbuf->IndCount);
            vkCmdDrawIndexed(_ctx->CommandBuffer, numeric_cast<uint32_t>(num_indices), 1, numeric_cast<uint32_t>(start_index), 0, 0);
        }
        else if (vk_dbuf->VertCount != 0) {
            // Draw without indices. start_index is an index-buffer offset in the DrawBuffer contract,
            // which has no meaning without an index buffer; the engine never emits a vertices-only draw
            // with a non-zero offset (matches Direct3D's FL<=9.3 point-list path), so firstVertex is 0.
            FO_VERIFY_AND_THROW(start_index == 0, "Non-zero start index in vertices-only draw");
            const size_t num_vertices = vk_dbuf->VertCount;
            vkCmdDraw(_ctx->CommandBuffer, numeric_cast<uint32_t>(num_vertices), 1, 0, 0);
        }
    }

    ProjBuf.reset();
    MainTexBuf.reset();
    EggBuf.reset();
    SpriteBorderBuf.reset();
    TimeBuf.reset();
    RandomValueBuf.reset();
    CameraBuf.reset();
    // ScriptValueBuf intentionally not reset: it persists across draws and is re-uploaded each
    // draw, matching the OpenGL/D3D backends (D3D uploads it with reset_buf=false).
#if FO_ENABLE_3D
    ModelBuf.reset();
    ModelTexBuf.reset();
    ModelAnimBuf.reset();
#endif
}

Vulkan_Renderer::Vulkan_Renderer() = default;

Vulkan_Renderer::~Vulkan_Renderer()
{
    FO_STACK_TRACE_ENTRY();

    if (!_ctx) {
        return;
    }

    auto ctx = _ctx.as_ptr();

    if (ctx->Device != nullptr) {
        vkDeviceWaitIdle(ctx->Device);
        FlushDeferredDestroyResources(ctx);

        // Release the dummy texture's Vk handles now, while the device is still alive.
        // ~Vulkan_Texture() routes its handles through ctx->DeferredDestroy* and they must be
        // flushed against a live VkDevice. If left to the implicit Context teardown, it would
        // run after vkDestroyDevice below.
        ctx->DummyTexture.reset();
        FlushDeferredDestroyResources(ctx);

        // The texture staging ring routes through the same deferred-destroy queues.
        DestroyPooledBuffers(ctx, ctx->TextureStagingRing);
        FlushDeferredDestroyResources(ctx);
    }

    for (auto* fb : ctx->Framebuffers) {
        vkDestroyFramebuffer(ctx->Device, fb, nullptr);
    }

    ctx->Framebuffers.clear();

    for (auto* iv : ctx->SwapchainImageViews) {
        vkDestroyImageView(ctx->Device, iv, nullptr);
    }

    ctx->SwapchainImageViews.clear();

    if (ctx->SwapchainDepthImageView != nullptr) {
        vkDestroyImageView(ctx->Device, ctx->SwapchainDepthImageView, nullptr);
        ctx->SwapchainDepthImageView = VK_NULL_HANDLE;
    }
    if (ctx->SwapchainDepthImage != nullptr) {
        vkDestroyImage(ctx->Device, ctx->SwapchainDepthImage, nullptr);
        ctx->SwapchainDepthImage = VK_NULL_HANDLE;
    }
    if (ctx->SwapchainDepthImageMemory != nullptr) {
        vkFreeMemory(ctx->Device, ctx->SwapchainDepthImageMemory, nullptr);
        ctx->SwapchainDepthImageMemory = VK_NULL_HANDLE;
    }

    if (ctx->RenderPass != nullptr) {
        vkDestroyRenderPass(ctx->Device, ctx->RenderPass, nullptr);
        ctx->RenderPass = VK_NULL_HANDLE;
    }

    if (ctx->SwapchainDepthImageView != nullptr) {
        vkDestroyImageView(ctx->Device, ctx->SwapchainDepthImageView, nullptr);
        ctx->SwapchainDepthImageView = VK_NULL_HANDLE;
    }
    if (ctx->SwapchainDepthImage != nullptr) {
        vkDestroyImage(ctx->Device, ctx->SwapchainDepthImage, nullptr);
        ctx->SwapchainDepthImage = VK_NULL_HANDLE;
    }
    if (ctx->SwapchainDepthImageMemory != nullptr) {
        vkFreeMemory(ctx->Device, ctx->SwapchainDepthImageMemory, nullptr);
        ctx->SwapchainDepthImageMemory = VK_NULL_HANDLE;
    }

    if (ctx->CommandPool != nullptr) {
        vkDestroyCommandPool(ctx->Device, ctx->CommandPool, nullptr);
        ctx->CommandPool = VK_NULL_HANDLE;
    }

    if (ctx->ImageAvailableSemaphore != nullptr) {
        vkDestroySemaphore(ctx->Device, ctx->ImageAvailableSemaphore, nullptr);
        ctx->ImageAvailableSemaphore = VK_NULL_HANDLE;
    }
    if (ctx->RenderCompleteSemaphore != nullptr) {
        vkDestroySemaphore(ctx->Device, ctx->RenderCompleteSemaphore, nullptr);
        ctx->RenderCompleteSemaphore = VK_NULL_HANDLE;
    }

    if (ctx->Swapchain != nullptr) {
        vkDestroySwapchainKHR(ctx->Device, ctx->Swapchain, nullptr);
        ctx->Swapchain = VK_NULL_HANDLE;
    }
    if (ctx->TextureDescriptorSetLayout != nullptr) {
        vkDestroyDescriptorSetLayout(ctx->Device, ctx->TextureDescriptorSetLayout, nullptr);
        ctx->TextureDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (ctx->UniformDescriptorSetLayout != nullptr) {
        vkDestroyDescriptorSetLayout(ctx->Device, ctx->UniformDescriptorSetLayout, nullptr);
        ctx->UniformDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (ctx->FrameDescriptorPool != nullptr) {
        vkDestroyDescriptorPool(ctx->Device, ctx->FrameDescriptorPool, nullptr);
        ctx->FrameDescriptorPool = VK_NULL_HANDLE;
    }
    if (ctx->FrameUniformBuffer != nullptr) {
        vkDestroyBuffer(ctx->Device, ctx->FrameUniformBuffer, nullptr);
        ctx->FrameUniformBuffer = VK_NULL_HANDLE;
    }
    if (ctx->FrameUniformBufferMemory != nullptr) {
        vkUnmapMemory(ctx->Device, ctx->FrameUniformBufferMemory);
        vkFreeMemory(ctx->Device, ctx->FrameUniformBufferMemory, nullptr);
        ctx->FrameUniformBufferMemory = VK_NULL_HANDLE;
        ctx->FrameUniformBufferMapped = nullptr;
    }
    if (ctx->LinearSampler != nullptr) {
        vkDestroySampler(ctx->Device, ctx->LinearSampler, nullptr);
        ctx->LinearSampler = VK_NULL_HANDLE;
    }
    if (ctx->PointSampler != nullptr) {
        vkDestroySampler(ctx->Device, ctx->PointSampler, nullptr);
        ctx->PointSampler = VK_NULL_HANDLE;
    }
    if (ctx->Device != nullptr) {
        vkDestroyDevice(ctx->Device, nullptr);
        ctx->Device = VK_NULL_HANDLE;
    }
    if (ctx->Surface != nullptr && ctx->Instance != nullptr) {
        vkDestroySurfaceKHR(ctx->Instance, ctx->Surface, nullptr);
        ctx->Surface = VK_NULL_HANDLE;
    }
    if (ctx->DebugMessenger != nullptr && ctx->Instance != nullptr) {
        if (auto vkDestroyDebugUtilsMessengerEXT_fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(ctx->Instance, "vkDestroyDebugUtilsMessengerEXT")); vkDestroyDebugUtilsMessengerEXT_fn != nullptr) {
            vkDestroyDebugUtilsMessengerEXT_fn(ctx->Instance, ctx->DebugMessenger, nullptr);
        }
        ctx->DebugMessenger = VK_NULL_HANDLE;
    }
    if (ctx->Instance != nullptr) {
        vkDestroyInstance(ctx->Instance, nullptr);
        ctx->Instance = VK_NULL_HANDLE;
    }

    _ctx.reset();
}

auto Vulkan_Renderer::CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture>
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();
    auto tex = SafeAlloc::MakeUnique<Vulkan_Texture>(size, linear_filtered, with_depth, ctx);

    return std::move(tex);
}

auto Vulkan_Renderer::CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer>
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();
    auto dbuf = SafeAlloc::MakeUnique<Vulkan_DrawBuffer>(is_static, ctx);

    return std::move(dbuf);
}

auto Vulkan_Renderer::CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect>
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();
    auto vk_effect = SafeAlloc::MakeUnique<Vulkan_Effect>(usage, name, loader, ctx);

    for (size_t pass = 0; pass < vk_effect->_passCount; pass++) {
        // Load vertex shader SPIR-V
        {
            const string vert_fname = strex("{}.fofx-{}-vert-spv", strex(name).erase_file_extension(), pass + 1);
            string vert_content = loader(vert_fname);
            FO_VERIFY_AND_THROW(!vert_content.empty(), "Vertex shader SPIR-V is empty");
            FO_VERIFY_AND_THROW(vert_content.length() % sizeof(uint32_t) == 0, "Vertex shader SPIR-V size is not a multiple of 4");

            VkShaderModuleCreateInfo module_ci {};
            module_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            module_ci.codeSize = vert_content.length();
            module_ci.pCode = reinterpret_cast<const uint32_t*>(vert_content.data());

            if (vkCreateShaderModule(ctx->Device, &module_ci, nullptr, &vk_effect->VertexShaderModule[pass]) != VK_SUCCESS) {
                throw EffectLoadException("Failed to create vertex shader module", vert_fname);
            }
        }

        // Load fragment shader SPIR-V
        {
            const string frag_fname = strex("{}.fofx-{}-frag-spv", strex(name).erase_file_extension(), pass + 1);
            string frag_content = loader(frag_fname);
            FO_VERIFY_AND_THROW(!frag_content.empty(), "Fragment shader SPIR-V is empty");
            FO_VERIFY_AND_THROW(frag_content.length() % sizeof(uint32_t) == 0, "Fragment shader SPIR-V size is not a multiple of 4");

            VkShaderModuleCreateInfo module_ci {};
            module_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            module_ci.codeSize = frag_content.length();
            module_ci.pCode = reinterpret_cast<const uint32_t*>(frag_content.data());

            if (vkCreateShaderModule(ctx->Device, &module_ci, nullptr, &vk_effect->FragmentShaderModule[pass]) != VK_SUCCESS) {
                throw EffectLoadException("Failed to create fragment shader module", frag_fname);
            }
        }
    }

    // Create pipeline layout for this effect
    VkPipelineLayoutCreateInfo layout_ci {};
    layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    VkDescriptorSetLayout set_layouts[] = {ctx->UniformDescriptorSetLayout, ctx->TextureDescriptorSetLayout};
    layout_ci.setLayoutCount = 2;
    layout_ci.pSetLayouts = set_layouts;

    if (vkCreatePipelineLayout(ctx->Device, &layout_ci, nullptr, &vk_effect->PipelineLayout) != VK_SUCCESS) {
        throw EffectLoadException("Failed to create pipeline layout", name);
    }

    // Create graphics pipeline for each pass
    for (size_t pass = 0; pass < vk_effect->_passCount; pass++) {
        // Create vertex input descriptions based on effect usage
        VkVertexInputBindingDescription binding_desc {};
        binding_desc.binding = 0;
        binding_desc.stride = 0;
        binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attr_descs[9] {};
        size_t attr_count = 0;

#if FO_ENABLE_3D
        if (usage == EffectUsage::Model) {
            binding_desc.stride = sizeof(Vertex3D);
            // Attribute 0: Position (vec3)
            attr_descs[0].binding = 0;
            attr_descs[0].location = 0;
            attr_descs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attr_descs[0].offset = offsetof(Vertex3D, Position);
            // Attribute 1: Normal (vec3)
            attr_descs[1].binding = 0;
            attr_descs[1].location = 1;
            attr_descs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attr_descs[1].offset = offsetof(Vertex3D, Normal);
            // Attribute 2: TexCoord (vec2)
            attr_descs[2].binding = 0;
            attr_descs[2].location = 2;
            attr_descs[2].format = VK_FORMAT_R32G32_SFLOAT;
            attr_descs[2].offset = offsetof(Vertex3D, TexCoord);
            // Attribute 3: TexCoordBase (vec2)
            attr_descs[3].binding = 0;
            attr_descs[3].location = 3;
            attr_descs[3].format = VK_FORMAT_R32G32_SFLOAT;
            attr_descs[3].offset = offsetof(Vertex3D, TexCoordBase);
            // Attribute 4: Tangent (vec3)
            attr_descs[4].binding = 0;
            attr_descs[4].location = 4;
            attr_descs[4].format = VK_FORMAT_R32G32B32_SFLOAT;
            attr_descs[4].offset = offsetof(Vertex3D, Tangent);
            // Attribute 5: Bitangent (vec3)
            attr_descs[5].binding = 0;
            attr_descs[5].location = 5;
            attr_descs[5].format = VK_FORMAT_R32G32B32_SFLOAT;
            attr_descs[5].offset = offsetof(Vertex3D, Bitangent);
            // Attribute 6: BlendWeights (vec4)
            attr_descs[6].binding = 0;
            attr_descs[6].location = 6;
            attr_descs[6].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attr_descs[6].offset = offsetof(Vertex3D, BlendWeights);
            // Attribute 7: BlendIndices (vec4)
            attr_descs[7].binding = 0;
            attr_descs[7].location = 7;
            attr_descs[7].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attr_descs[7].offset = offsetof(Vertex3D, BlendIndices);
            // Attribute 8: Color (vec4 unorm)
            attr_descs[8].binding = 0;
            attr_descs[8].location = 8;
            attr_descs[8].format = VK_FORMAT_R8G8B8A8_UNORM;
            attr_descs[8].offset = offsetof(Vertex3D, Color);
            attr_count = 9;
        }
        else
#endif
        {
            binding_desc.stride = sizeof(Vertex2D);
            // Attribute 0: Position (vec3)
            attr_descs[0].binding = 0;
            attr_descs[0].location = 0;
            attr_descs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attr_descs[0].offset = offsetof(Vertex2D, PosX);
            // Attribute 1: Color (vec4 unorm)
            attr_descs[1].binding = 0;
            attr_descs[1].location = 1;
            attr_descs[1].format = VK_FORMAT_R8G8B8A8_UNORM;
            attr_descs[1].offset = offsetof(Vertex2D, Color);
            // Attribute 2: TexCoord (vec2)
            attr_descs[2].binding = 0;
            attr_descs[2].location = 2;
            attr_descs[2].format = VK_FORMAT_R32G32_SFLOAT;
            attr_descs[2].offset = offsetof(Vertex2D, TexU);
            // Attribute 3: EggFlags (vec2) — per-vertex egg-slot enable flags consumed by 2D_Default.fofx
            attr_descs[3].binding = 0;
            attr_descs[3].location = 3;
            attr_descs[3].format = VK_FORMAT_R32G32_SFLOAT;
            attr_descs[3].offset = offsetof(Vertex2D, EggFlags);
            attr_count = 4;
        }

        VkPipelineVertexInputStateCreateInfo vertex_input_ci {};
        vertex_input_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_ci.vertexBindingDescriptionCount = 1;
        vertex_input_ci.pVertexBindingDescriptions = &binding_desc;
        vertex_input_ci.vertexAttributeDescriptionCount = numeric_cast<uint32_t>(attr_count);
        vertex_input_ci.pVertexAttributeDescriptions = attr_descs;

        VkPipelineViewportStateCreateInfo viewport_ci {};
        viewport_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_ci.viewportCount = 1;
        viewport_ci.pViewports = nullptr;
        viewport_ci.scissorCount = 1;
        viewport_ci.pScissors = nullptr;

        VkPipelineRasterizationStateCreateInfo rasterization_ci {};
        rasterization_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization_ci.depthClampEnable = VK_FALSE;
        rasterization_ci.rasterizerDiscardEnable = VK_FALSE;
        rasterization_ci.polygonMode = VK_POLYGON_MODE_FILL;
#if FO_ENABLE_3D
        // The negative-height viewport flips screen-space winding exactly like the previously used
        // Y-negated projection did, so the effective front face stays the same as before.
        rasterization_ci.cullMode = (usage == EffectUsage::Model) ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
        rasterization_ci.frontFace = (usage == EffectUsage::Model) ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
#else
        rasterization_ci.cullMode = VK_CULL_MODE_NONE;
        rasterization_ci.frontFace = VK_FRONT_FACE_CLOCKWISE;
#endif
        rasterization_ci.depthBiasEnable = VK_FALSE;
        rasterization_ci.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisample_ci {};
        multisample_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample_ci.sampleShadingEnable = VK_FALSE;
        multisample_ci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState blend_attachment {};
        blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        blend_attachment.blendEnable = VK_TRUE;
        blend_attachment.srcColorBlendFactor = ConvertBlend(vk_effect->_srcBlendFunc[pass], false);
        blend_attachment.dstColorBlendFactor = ConvertBlend(vk_effect->_destBlendFunc[pass], false);
        blend_attachment.colorBlendOp = ConvertBlendOp(vk_effect->_blendEquation[pass]);
        blend_attachment.srcAlphaBlendFactor = ConvertBlend(vk_effect->_srcBlendFunc[pass], true);
        blend_attachment.dstAlphaBlendFactor = ConvertBlend(vk_effect->_destBlendFunc[pass], true);
        blend_attachment.alphaBlendOp = ConvertBlendOp(vk_effect->_blendEquation[pass]);

        VkPipelineColorBlendStateCreateInfo blend_ci {};
        blend_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend_ci.logicOpEnable = VK_FALSE;
        blend_ci.attachmentCount = 1;
        blend_ci.pAttachments = &blend_attachment;

        VkPipelineDepthStencilStateCreateInfo depth_ci {};
        depth_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_ci.depthTestEnable = VK_FALSE;
        depth_ci.depthWriteEnable = VK_FALSE;
        depth_ci.depthCompareOp = ConvertDepthFunc(vk_effect->_depthFunc[pass]);

#if FO_ENABLE_3D
        if (usage == EffectUsage::Model) {
            depth_ci.depthTestEnable = VK_TRUE;
            depth_ci.depthWriteEnable = vk_effect->_depthWrite[pass] ? VK_TRUE : VK_FALSE;
        }
#endif

        if (usage == EffectUsage::QuadSprite) {
            depth_ci.depthTestEnable = VK_TRUE;
            depth_ci.depthWriteEnable = vk_effect->_depthWrite[pass] ? VK_TRUE : VK_FALSE;
        }

        depth_ci.depthBoundsTestEnable = VK_FALSE;
        depth_ci.stencilTestEnable = VK_FALSE;

        VkPipelineShaderStageCreateInfo shader_stages[2] {};
        shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shader_stages[0].module = vk_effect->VertexShaderModule[pass];
        shader_stages[0].pName = "main";
        shader_stages[0].pNext = nullptr;

        shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shader_stages[1].module = vk_effect->FragmentShaderModule[pass];
        shader_stages[1].pName = "main";
        shader_stages[1].pNext = nullptr;

        VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic_ci {};
        dynamic_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_ci.dynamicStateCount = 2;
        dynamic_ci.pDynamicStates = dynamic_states;

        for (size_t prim = 0; prim < 5; prim++) {
            const auto prim_type = static_cast<RenderPrimitiveType>(prim);

            VkPipelineInputAssemblyStateCreateInfo input_assembly_ci {};
            input_assembly_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            // VUID-VkGraphicsPipelineCreateInfo-topology-08773 requires the vertex shader to write
            // gl_PointSize when the topology is POINT_LIST. Our shaders are cross-compiled to HLSL/MSL
            // via spirv-cross, which rejects the PointSize builtin, so we cannot emit it. Point
            // primitives are not used by current content, so map PointList to TRIANGLE_LIST to keep
            // pipeline creation validation-clean. Revisit if real point rendering is ever needed.
            input_assembly_ci.topology = prim_type == RenderPrimitiveType::PointList ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : ConvertPrimitive(prim_type);
            input_assembly_ci.primitiveRestartEnable = prim_type == RenderPrimitiveType::LineStrip || prim_type == RenderPrimitiveType::TriangleStrip ? VK_TRUE : VK_FALSE;

            // Build two pipeline variants per (pass, prim): index 0 = alpha-blend (default for sprite
            // rendering), index 1 = blending disabled (opaque blit semantics for SpriteManager flush
            // calls that pass alpha_blend=false). The runtime picks one in DrawBuffer based on
            // RenderEffect::DisableBlending.
            for (size_t blend_variant = 0; blend_variant < 2; blend_variant++) {
                VkPipelineColorBlendAttachmentState variant_blend_attachment = blend_attachment;
                if (blend_variant == 1) {
                    variant_blend_attachment.blendEnable = VK_FALSE;
                }

                VkPipelineColorBlendStateCreateInfo variant_blend_ci = blend_ci;
                variant_blend_ci.pAttachments = &variant_blend_attachment;

                VkGraphicsPipelineCreateInfo pipeline_ci {};
                pipeline_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
                pipeline_ci.stageCount = 2;
                pipeline_ci.pStages = shader_stages;
                pipeline_ci.pVertexInputState = &vertex_input_ci;
                pipeline_ci.pInputAssemblyState = &input_assembly_ci;
                pipeline_ci.pViewportState = &viewport_ci;
                pipeline_ci.pRasterizationState = &rasterization_ci;
                pipeline_ci.pMultisampleState = &multisample_ci;
                pipeline_ci.pDepthStencilState = &depth_ci;
                pipeline_ci.pColorBlendState = &variant_blend_ci;
                pipeline_ci.pDynamicState = &dynamic_ci;
                pipeline_ci.layout = vk_effect->PipelineLayout;
                pipeline_ci.renderPass = ctx->RenderPass;
                pipeline_ci.subpass = 0;

                if (vkCreateGraphicsPipelines(ctx->Device, nullptr, 1, &pipeline_ci, nullptr, &vk_effect->Pipeline[pass][prim][blend_variant]) != VK_SUCCESS) {
                    throw EffectLoadException("Failed to create graphics pipeline", name);
                }
            }
        }
    }

    return vk_effect;
}

auto Vulkan_Renderer::CreateOrthoMatrix(float32_t left, float32_t right, float32_t bottom, float32_t top, float32_t nearp, float32_t farp) const -> mat44
{
    FO_STACK_TRACE_ENTRY();

    return BuildOrthoMatrix(left, right, bottom, top, nearp, farp);
}

static auto BuildOrthoMatrix(float32_t left, float32_t right, float32_t bottom, float32_t top, float32_t nearp, float32_t farp) -> mat44
{
    FO_STACK_TRACE_ENTRY();

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

auto Vulkan_Renderer::GetViewPort() const -> irect32
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();
    return ctx->ViewPort;
}

void Vulkan_Renderer::Init(GlobalSettings& settings, nptr<WindowInternalHandle> window)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!!window, "Frontend window handle is null");
    FO_VERIFY_AND_THROW(!_ctx, "Frontend context is already initialized");
    _ctx = SafeAlloc::MakeUnique<Context>(&settings, GetSdlWindow(window));
    auto ctx = _ctx.as_ptr();

    WriteLog("Used Vulkan rendering");

    WriteLog("[VkInit] FO_DEBUG={} settings.RenderDebug={}", FO_DEBUG, settings.RenderDebug ? "Y" : "n");

    Uint32 ext_count = 0;
    const char* const* sdl_exts = SDL_Vulkan_GetInstanceExtensions(&ext_count);

    if (sdl_exts == nullptr) {
        throw RenderingException("Failed to query Vulkan extensions from SDL");
    }

    // Append VK_EXT_debug_utils so validation messages reach our log via a custom messenger
    // (without it the layer prints to stdout and gets lost in the bench's redirected console).
    // Enable based on Settings.RenderDebug — that flag is what the user toggles via the
    // -Render.RenderDebug 1 command-line argument, so it's a more reliable opt-in than FO_DEBUG.
    vector<const char*> exts_list(sdl_exts, sdl_exts + ext_count);
    const bool want_validation = settings.RenderDebug || FO_DEBUG;
    if (want_validation) {
        exts_list.push_back("VK_EXT_debug_utils");
    }

    VkResult vk_result = VK_SUCCESS;

    VkApplicationInfo app_info {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "FOnline";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "FOnline Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    // 1.1 is required for negative-viewport-height rendering (VK_KHR_maintenance1 in core), which
    // this backend uses to keep its projection matrices identical to the other backends.
    app_info.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo create_info {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = numeric_cast<uint32_t>(exts_list.size());
    create_info.ppEnabledExtensionNames = exts_list.data();

    const char* validation_layers[] = {"VK_LAYER_KHRONOS_validation"};
    bool validation_available = false;
    if (want_validation) {
        uint32_t layer_count = 0;
        vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
        vector<VkLayerProperties> available_layers(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

        for (const auto& layer : available_layers) {
            if (strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
                validation_available = true;
                break;
            }
        }

        WriteLog("[VkInit] validation layer probe: count={} available={}", layer_count, validation_available ? "Y" : "n");

        if (validation_available) {
            create_info.enabledLayerCount = 1;
            create_info.ppEnabledLayerNames = validation_layers;
            WriteLog("[VkInit] Vulkan validation layers enabled");
        }
    }

    vk_result = vkCreateInstance(&create_info, nullptr, &ctx->Instance);

    if (vk_result != VK_SUCCESS) {
        throw RenderingException("vkCreateInstance failed", vk_result);
    }

    if (want_validation) {
        // Wire the debug messenger so layer warnings/errors land in LF_Server.log under [VkLayer].
        // Without this we'd see the rendering misbehave silently — the layer would whisper to
        // stdout which the bench redirects elsewhere and we never see it.
        if (auto vkCreateDebugUtilsMessengerEXT_fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(ctx->Instance, "vkCreateDebugUtilsMessengerEXT")); vkCreateDebugUtilsMessengerEXT_fn != nullptr) {
            VkDebugUtilsMessengerCreateInfoEXT msg_ci {};
            msg_ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            msg_ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            msg_ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            msg_ci.pfnUserCallback = +[](VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* data, void* /*user*/) -> VkBool32 {
                const char* sev = severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ? "ERROR" : severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT ? "WARN" : "INFO";
                WriteLog("[VkLayer/{}] {}: {}", sev, data->pMessageIdName != nullptr ? data->pMessageIdName : "?", data->pMessage != nullptr ? data->pMessage : "?");
                ignore_unused(type);
                return VK_FALSE;
            };
            vk_result = vkCreateDebugUtilsMessengerEXT_fn(ctx->Instance, &msg_ci, nullptr, &ctx->DebugMessenger);
            if (vk_result == VK_SUCCESS) {
                WriteLog("[VkLayer] debug messenger attached");
            }
            else {
                WriteLog("[VkLayer] vkCreateDebugUtilsMessengerEXT failed: {}", static_cast<int32_t>(vk_result));
            }
        }
        else {
            WriteLog("[VkLayer] VK_EXT_debug_utils not available — layer messages will be silenced");
        }
    }

    if (!SDL_Vulkan_CreateSurface(ctx->SdlWindow.get(), ctx->Instance, nullptr, &ctx->Surface)) {
        throw RenderingException("SDL_Vulkan_CreateSurface failed");
    }

    // Enumerate physical devices (selection happens below)
    uint32_t gpu_count = 0;
    vk_result = vkEnumeratePhysicalDevices(ctx->Instance, &gpu_count, nullptr);
    VerifyVkResult(vk_result);

    if (gpu_count == 0) {
        throw RenderingException("No Vulkan physical devices found");
    }

    vector<VkPhysicalDevice> gpus;
    gpus.resize(gpu_count);
    vk_result = vkEnumeratePhysicalDevices(ctx->Instance, &gpu_count, gpus.data());
    VerifyVkResult(vk_result);

    // Pick the best presentable device instead of blindly taking the first enumerated one (which on
    // multi-GPU systems may be a compute-only or integrated device). Require a graphics+present queue
    // family for this surface and VK_KHR_swapchain support; prefer discrete over integrated.
    const auto has_swapchain_ext = [](VkPhysicalDevice dev) -> bool {
        uint32_t ext_count = 0;
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &ext_count, nullptr);
        vector<VkExtensionProperties> exts(ext_count);
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &ext_count, exts.data());
        for (const auto& e : exts) {
            if (strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                return true;
            }
        }
        return false;
    };
    const auto has_graphics_present_family = [](VkPhysicalDevice dev, VkSurfaceKHR surface) -> bool {
        uint32_t fam_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &fam_count, nullptr);
        vector<VkQueueFamilyProperties> fams(fam_count);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &fam_count, fams.data());
        for (uint32_t i = 0; i < fam_count; i++) {
            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &present);
            if ((fams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 && present != VK_FALSE) {
                return true;
            }
        }
        return false;
    };

    int32_t best_score = -1;
    for (auto* gpu : gpus) {
        if (!has_swapchain_ext(gpu) || !has_graphics_present_family(gpu, ctx->Surface)) {
            continue;
        }

        VkPhysicalDeviceProperties props {};
        vkGetPhysicalDeviceProperties(gpu, &props);
        const int32_t score = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 3 : props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 2 : props.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU ? 1 : 0;
        if (score > best_score) {
            best_score = score;
            ctx->PhysicalDevice = gpu;
        }
    }

    if (ctx->PhysicalDevice == nullptr) {
        throw RenderingException("No suitable Vulkan physical device (needs graphics+present queue and swapchain support)");
    }

    VkPhysicalDeviceProperties gpu_props {};
    vkGetPhysicalDeviceProperties(ctx->PhysicalDevice, &gpu_props);

    // Cache immutable device properties once so the hot paths don't re-query them.
    ctx->MinUniformBufferOffsetAlignment = gpu_props.limits.minUniformBufferOffsetAlignment;
    vkGetPhysicalDeviceMemoryProperties(ctx->PhysicalDevice, &ctx->MemoryProperties);

    const auto atlas_limit = numeric_cast<int32_t>(std::min(gpu_props.limits.maxImageDimension2D, numeric_cast<uint32_t>(AppRender::MAX_ATLAS_SIZE)));
    FO_VERIFY_AND_THROW(atlas_limit >= AppRender::MIN_ATLAS_SIZE, "Vulkan texture atlas size is below the required minimum", atlas_limit, AppRender::MIN_ATLAS_SIZE);
    AppRender::MAX_ATLAS_WIDTH = atlas_limit;
    AppRender::MAX_ATLAS_HEIGHT = atlas_limit;

    // Find queue family
    uint32_t qcount;
    vkGetPhysicalDeviceQueueFamilyProperties(ctx->PhysicalDevice, &qcount, nullptr);
    vector<VkQueueFamilyProperties> qprops(qcount);
    qprops.resize(qcount);
    vkGetPhysicalDeviceQueueFamilyProperties(ctx->PhysicalDevice, &qcount, qprops.data());

    optional<uint32_t> graphics_family;

    for (uint32_t i = 0; i < qcount; i++) {
        VkBool32 present_support = VK_FALSE;
        vk_result = vkGetPhysicalDeviceSurfaceSupportKHR(ctx->PhysicalDevice, i, ctx->Surface, &present_support);
        VerifyVkResult(vk_result);

        if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 && present_support != VK_FALSE) {
            graphics_family = i;
            break;
        }
    }

    if (!graphics_family.has_value()) {
        throw RenderingException("No suitable Vulkan queue family");
    }

    constexpr float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo dqci {};
    dqci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    dqci.queueFamilyIndex = graphics_family.value();
    dqci.queueCount = 1;
    dqci.pQueuePriorities = &queue_priority;

    VkDeviceCreateInfo dci {};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &dqci;

    // Enable required device extensions
    const char* device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = device_extensions;

    // Device layers are deprecated since Vulkan 1.0 (VUID-VkDeviceCreateInfo-enabledLayerCount-12384);
    // validation layers are enabled on the instance only.
    dci.enabledLayerCount = 0;
    dci.ppEnabledLayerNames = nullptr;

    vk_result = vkCreateDevice(ctx->PhysicalDevice, &dci, nullptr, &ctx->Device);
    VerifyVkResult(vk_result);

    vkGetDeviceQueue(ctx->Device, graphics_family.value(), 0, &ctx->GraphicsQueue);
    ctx->GraphicsFamilyIndex = graphics_family.value();

    // Create command pool for recording render commands
    VkCommandPoolCreateInfo cpi {};
    cpi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpi.queueFamilyIndex = ctx->GraphicsFamilyIndex;
    vk_result = vkCreateCommandPool(ctx->Device, &cpi, nullptr, &ctx->CommandPool);
    VerifyVkResult(vk_result);

    // Initialize swapchain for current window size
    int width;
    int height;
    SDL_GetWindowSizeInPixels(ctx->SdlWindow.get(), &width, &height);
    RecreateSwapchain(ctx, {std::max(width, 1), std::max(height, 1)});
    ApplySwapchainTargetMetrics(ctx, ctx->SwapchainSize);

    // Allocate single command buffer (single-threaded rendering)
    VkCommandBufferAllocateInfo cbai {};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = ctx->CommandPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;

    vk_result = vkAllocateCommandBuffers(ctx->Device, &cbai, &ctx->CommandBuffer);
    VerifyVkResult(vk_result);

    // Create semaphores for acquire/present synchronization
    VkSemaphoreCreateInfo sem_ci {};
    sem_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vk_result = vkCreateSemaphore(ctx->Device, &sem_ci, nullptr, &ctx->ImageAvailableSemaphore);
    VerifyVkResult(vk_result);
    vk_result = vkCreateSemaphore(ctx->Device, &sem_ci, nullptr, &ctx->RenderCompleteSemaphore);
    VerifyVkResult(vk_result);

    // Allocate staging command buffer for buffer uploads
    VkCommandBufferAllocateInfo staging_cbai {};
    staging_cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    staging_cbai.commandPool = ctx->CommandPool;
    staging_cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    staging_cbai.commandBufferCount = 1;

    vk_result = vkAllocateCommandBuffers(ctx->Device, &staging_cbai, &ctx->StagingCommandBuffer);
    VerifyVkResult(vk_result);

    // Create descriptor set layout for texture bindings used by effects
    vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.resize(VULKAN_MAX_TEXTURE_BINDINGS);

    for (uint32_t i = 0; i < VULKAN_MAX_TEXTURE_BINDINGS; i++) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    VkDescriptorSetLayoutCreateInfo layout_ci {};
    layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_ci.bindingCount = numeric_cast<uint32_t>(bindings.size());
    layout_ci.pBindings = bindings.data();

    vk_result = vkCreateDescriptorSetLayout(ctx->Device, &layout_ci, nullptr, &ctx->TextureDescriptorSetLayout);
    VerifyVkResult(vk_result);

    // Create descriptor set layout for uniform buffer bindings used by effects
    vector<VkDescriptorSetLayoutBinding> ubo_bindings;
    ubo_bindings.resize(VULKAN_MAX_UNIFORM_BINDINGS);

    for (uint32_t i = 0; i < VULKAN_MAX_UNIFORM_BINDINGS; i++) {
        ubo_bindings[i].binding = i;
        ubo_bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubo_bindings[i].descriptorCount = 1;
        ubo_bindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    VkDescriptorSetLayoutCreateInfo ubo_layout_ci {};
    ubo_layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ubo_layout_ci.bindingCount = numeric_cast<uint32_t>(ubo_bindings.size());
    ubo_layout_ci.pBindings = ubo_bindings.data();

    vk_result = vkCreateDescriptorSetLayout(ctx->Device, &ubo_layout_ci, nullptr, &ctx->UniformDescriptorSetLayout);
    VerifyVkResult(vk_result);

    // Create ctx->FrameDescriptorPool
    // Pool sizing: each per-draw set we allocate uses VULKAN_MAX_*_BINDINGS descriptors of its
    // type even when the shader only writes a couple of bindings (the layout is fixed-width,
    // see TextureDescriptorSetLayout / UniformDescriptorSetLayout). With a tight budget the
    // ImGui pass — which is queued last in the frame — silently runs out of descriptors and the
    // UI vanishes while the world keeps rendering. Provision per-type counts to cover maxSets.
    constexpr uint32_t kMaxFrameSets = 20000;
    VkDescriptorPoolSize frame_pool_sizes[2] {};
    frame_pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    frame_pool_sizes[0].descriptorCount = kMaxFrameSets * VULKAN_MAX_TEXTURE_BINDINGS;
    frame_pool_sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    frame_pool_sizes[1].descriptorCount = kMaxFrameSets * VULKAN_MAX_UNIFORM_BINDINGS;

    VkDescriptorPoolCreateInfo frame_pool_ci {};
    frame_pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    frame_pool_ci.poolSizeCount = 2;
    frame_pool_ci.pPoolSizes = frame_pool_sizes;
    frame_pool_ci.maxSets = kMaxFrameSets;
    frame_pool_ci.flags = 0;

    vk_result = vkCreateDescriptorPool(ctx->Device, &frame_pool_ci, nullptr, &ctx->FrameDescriptorPool);
    VerifyVkResult(vk_result);

    // Create ctx->FrameUniformBuffer
    AllocateBuffer(ctx, ctx->FrameUniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, ctx->FrameUniformBuffer, ctx->FrameUniformBufferMemory);
    vk_result = vkMapMemory(ctx->Device, ctx->FrameUniformBufferMemory, 0, ctx->FrameUniformBufferSize, 0, ctx->FrameUniformBufferMapped.get_pp());
    VerifyVkResult(vk_result);

    // Create samplers for texture filtering (linear and point)
    VkSamplerCreateInfo sampler_ci {};
    sampler_ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_ci.magFilter = VK_FILTER_LINEAR;
    sampler_ci.minFilter = VK_FILTER_LINEAR;
    sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_ci.mipLodBias = 0.0f;
    sampler_ci.compareEnable = VK_FALSE;
    sampler_ci.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_ci.minLod = 0.0f;
    sampler_ci.maxLod = 0.0f;
    sampler_ci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_ci.unnormalizedCoordinates = VK_FALSE;

    vk_result = vkCreateSampler(ctx->Device, &sampler_ci, nullptr, &ctx->LinearSampler);
    VerifyVkResult(vk_result);

    sampler_ci.magFilter = VK_FILTER_NEAREST;
    sampler_ci.minFilter = VK_FILTER_NEAREST;
    sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    vk_result = vkCreateSampler(ctx->Device, &sampler_ci, nullptr, &ctx->PointSampler);
    VerifyVkResult(vk_result);

    constexpr ucolor dummy_pixel[1] = {ucolor {255, 0, 255, 255}};
    ctx->DummyTexture = CreateTexture({1, 1}, false, false);
    auto dummy_texture = GetDummyTexture(ctx);
    dummy_texture->UpdateTextureRegion({}, {1, 1}, dummy_pixel);

    // Begin first frame and set default render target (matches OpenGL/D3D init flow)
    BeginFrame(ctx);
    SetRenderTarget(nullptr);
}

static void RecreateSwapchain(ptr<Vulkan_Renderer::Context> ctx, isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(size.width > 0, "Swapchain size must be positive");
    FO_VERIFY_AND_THROW(size.height > 0, "Swapchain size must be positive");

    VkResult vk_result = VK_SUCCESS;

    // Destroy old framebuffers and image views
    for (auto* fb : ctx->Framebuffers) {
        DestroyResourceSafe(ctx, fb);
    }

    ctx->Framebuffers.clear();

    for (auto* iv : ctx->SwapchainImageViews) {
        DestroyResourceSafe(ctx, iv);
    }

    ctx->SwapchainImageViews.clear();

    if (ctx->Swapchain != nullptr) {
        vkDeviceWaitIdle(ctx->Device);
        vkDestroySwapchainKHR(ctx->Device, ctx->Swapchain, nullptr);
        ctx->Swapchain = VK_NULL_HANDLE;
        ctx->SwapchainImages.clear();
        ctx->SwapchainImageLayouts.clear();
    }

    DestroyResourceSafe(ctx, ctx->SwapchainDepthImageView);
    DestroyResourceSafe(ctx, ctx->SwapchainDepthImage);
    DestroyResourceSafe(ctx, ctx->SwapchainDepthImageMemory);

    // Create swapchain
    VkSurfaceCapabilitiesKHR caps {};
    vk_result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx->PhysicalDevice, ctx->Surface, &caps);
    VerifyVkResult(vk_result);

    VkExtent2D swapchain_extent {};
    if (caps.currentExtent.width != UINT32_MAX) {
        swapchain_extent = caps.currentExtent;
    }
    else {
        swapchain_extent.width = std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, numeric_cast<uint32_t>(size.width)));
        swapchain_extent.height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, numeric_cast<uint32_t>(size.height)));
    }

    ctx->SwapchainSize = {numeric_cast<int32_t>(swapchain_extent.width), numeric_cast<int32_t>(swapchain_extent.height)};

    uint32_t image_count = caps.minImageCount + 1;

    if (caps.maxImageCount != 0 && image_count > caps.maxImageCount) {
        image_count = caps.maxImageCount;
    }

    // Verify the desired surface format is supported (VUID-VkSwapchainCreateInfoKHR-imageFormat-01273).
    // The backend assumes VK_FORMAT_B8G8R8A8_UNORM everywhere — swapchain images, the shared render
    // pass, and texture render targets all use it so the one render pass stays compatible with every
    // framebuffer — so require it here rather than silently substituting an incompatible format.
    {
        uint32_t format_count = 0;
        vk_result = vkGetPhysicalDeviceSurfaceFormatsKHR(ctx->PhysicalDevice, ctx->Surface, &format_count, nullptr);
        VerifyVkResult(vk_result);
        FO_VERIFY_AND_THROW(format_count != 0, "Vulkan surface has no supported formats");
        vector<VkSurfaceFormatKHR> surface_formats(format_count);
        vk_result = vkGetPhysicalDeviceSurfaceFormatsKHR(ctx->PhysicalDevice, ctx->Surface, &format_count, surface_formats.data());
        VerifyVkResult(vk_result);

        // A lone VK_FORMAT_UNDEFINED entry means the surface imposes no format restriction.
        bool format_supported = surface_formats.size() == 1 && surface_formats.front().format == VK_FORMAT_UNDEFINED;
        for (const auto& sf : surface_formats) {
            if (sf.format == VK_FORMAT_B8G8R8A8_UNORM && sf.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
                format_supported = true;
                break;
            }
        }
        FO_VERIFY_AND_THROW(format_supported, "Surface does not support required VK_FORMAT_B8G8R8A8_UNORM / SRGB_NONLINEAR");
    }

    // Present mode: FIFO is the only mode the spec guarantees and is vsync-locked. With VSync off,
    // prefer IMMEDIATE (uncapped, possible tearing), then MAILBOX (uncapped, no tearing) — matching
    // the other backends, which honor Render.VSync in their present paths.
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

    if (!ctx->Settings->VSync) {
        uint32_t present_mode_count = 0;
        vk_result = vkGetPhysicalDeviceSurfacePresentModesKHR(ctx->PhysicalDevice, ctx->Surface, &present_mode_count, nullptr);
        VerifyVkResult(vk_result);
        vector<VkPresentModeKHR> present_modes(present_mode_count);
        vk_result = vkGetPhysicalDeviceSurfacePresentModesKHR(ctx->PhysicalDevice, ctx->Surface, &present_mode_count, present_modes.data());
        VerifyVkResult(vk_result);

        bool immediate_supported = false;
        bool mailbox_supported = false;

        for (const auto mode : present_modes) {
            immediate_supported |= mode == VK_PRESENT_MODE_IMMEDIATE_KHR;
            mailbox_supported |= mode == VK_PRESENT_MODE_MAILBOX_KHR;
        }

        if (immediate_supported) {
            present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
        else if (mailbox_supported) {
            present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
        }
    }

    WriteLog("Vulkan swapchain present mode: {}", present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR ? "immediate" : (present_mode == VK_PRESENT_MODE_MAILBOX_KHR ? "mailbox" : "fifo (vsync)"));

    VkSwapchainCreateInfoKHR sci {};
    sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = ctx->Surface;
    sci.minImageCount = image_count;
    sci.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    sci.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    sci.imageExtent = swapchain_extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = present_mode;
    sci.clipped = VK_TRUE;
    sci.oldSwapchain = VK_NULL_HANDLE;

    vk_result = vkCreateSwapchainKHR(ctx->Device, &sci, nullptr, &ctx->Swapchain);
    VerifyVkResult(vk_result);
    vk_result = vkGetSwapchainImagesKHR(ctx->Device, ctx->Swapchain, &image_count, nullptr);
    VerifyVkResult(vk_result);
    ctx->SwapchainImages.resize(image_count);
    vk_result = vkGetSwapchainImagesKHR(ctx->Device, ctx->Swapchain, &image_count, ctx->SwapchainImages.data());
    VerifyVkResult(vk_result);
    ctx->SwapchainImageLayouts.assign(ctx->SwapchainImages.size(), VK_IMAGE_LAYOUT_UNDEFINED);
    ctx->SwapchainFormat = sci.imageFormat;

    if (ctx->RenderPass == nullptr) {
        VkAttachmentDescription color_attachment {};
        color_attachment.format = ctx->SwapchainFormat;
        color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_attachment_ref {};
        color_attachment_ref.attachment = 0;
        color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription depth_attachment {};
        depth_attachment.format = VK_FORMAT_D32_SFLOAT;
        depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depth_attachment_ref {};
        depth_attachment_ref.attachment = 1;
        depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment_ref;
        subpass.pDepthStencilAttachment = &depth_attachment_ref;

        const VkAttachmentDescription attachments[] = {color_attachment, depth_attachment};

        // Explicit external dependencies. The implicit subpass dependencies carry no memory scope,
        // and this pass both LOADs prior color contents and re-clears a depth attachment that the
        // previous pass instance stored — without these the color loadOp read is not synchronized
        // with the pre-pass layout-transition barrier and the depth clear races the previous pass's
        // storeOp (SYNC-HAZARD-READ-AFTER-WRITE / WRITE-AFTER-WRITE), which on current drivers
        // manifests as render targets losing all previously accumulated content between passes.
        VkSubpassDependency dependencies[2] {};
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo rp_info {};
        rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rp_info.attachmentCount = 2;
        rp_info.pAttachments = attachments;
        rp_info.subpassCount = 1;
        rp_info.pSubpasses = &subpass;
        rp_info.dependencyCount = 2;
        rp_info.pDependencies = dependencies;

        vk_result = vkCreateRenderPass(ctx->Device, &rp_info, nullptr, &ctx->RenderPass);
        VerifyVkResult(vk_result);
    }

    // Create framebuffers
    ctx->SwapchainImageViews.resize(ctx->SwapchainImages.size());
    ctx->Framebuffers.resize(ctx->SwapchainImages.size());

    AllocateImage(ctx, swapchain_extent.width, swapchain_extent.height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, ctx->SwapchainDepthImage, ctx->SwapchainDepthImageMemory);

    VkImageViewCreateInfo depth_view_ci {};
    depth_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depth_view_ci.image = ctx->SwapchainDepthImage;
    depth_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depth_view_ci.format = VK_FORMAT_D32_SFLOAT;
    depth_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depth_view_ci.subresourceRange.baseMipLevel = 0;
    depth_view_ci.subresourceRange.levelCount = 1;
    depth_view_ci.subresourceRange.baseArrayLayer = 0;
    depth_view_ci.subresourceRange.layerCount = 1;

    vk_result = vkCreateImageView(ctx->Device, &depth_view_ci, nullptr, &ctx->SwapchainDepthImageView);
    VerifyVkResult(vk_result);

    for (size_t i = 0; i < ctx->SwapchainImages.size(); i++) {
        VkImageViewCreateInfo ivci {};
        ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivci.image = ctx->SwapchainImages[i];
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format = ctx->SwapchainFormat;
        ivci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.baseMipLevel = 0;
        ivci.subresourceRange.levelCount = 1;
        ivci.subresourceRange.baseArrayLayer = 0;
        ivci.subresourceRange.layerCount = 1;

        vk_result = vkCreateImageView(ctx->Device, &ivci, nullptr, &ctx->SwapchainImageViews[i]);
        VerifyVkResult(vk_result);

        const VkImageView framebuffer_attachments[] = {ctx->SwapchainImageViews[i], ctx->SwapchainDepthImageView};

        VkFramebufferCreateInfo fci {};
        fci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fci.renderPass = ctx->RenderPass;
        fci.attachmentCount = 2;
        fci.pAttachments = framebuffer_attachments;
        fci.width = swapchain_extent.width;
        fci.height = swapchain_extent.height;
        fci.layers = 1;

        vk_result = vkCreateFramebuffer(ctx->Device, &fci, nullptr, &ctx->Framebuffers[i]);
        VerifyVkResult(vk_result);
    }
}

static void RecreateFrameSyncSemaphores(ptr<Vulkan_Renderer::Context> ctx)
{
    FO_STACK_TRACE_ENTRY();

    // Callers must run with the device idle (after vkQueueWaitIdle). Binary semaphores can be left
    // in a stale signaled state after a failed present or a deferred resize; destroying and
    // recreating them guarantees a clean unsignaled state before the next acquire/submit cycle,
    // avoiding double-signal validation errors (VUID-vkAcquireNextImageKHR-semaphore-01779 et al).
    if (ctx->ImageAvailableSemaphore != nullptr) {
        vkDestroySemaphore(ctx->Device, ctx->ImageAvailableSemaphore, nullptr);
        ctx->ImageAvailableSemaphore = VK_NULL_HANDLE;
    }
    if (ctx->RenderCompleteSemaphore != nullptr) {
        vkDestroySemaphore(ctx->Device, ctx->RenderCompleteSemaphore, nullptr);
        ctx->RenderCompleteSemaphore = VK_NULL_HANDLE;
    }

    VkSemaphoreCreateInfo sem_ci {};
    sem_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkResult vk_result = vkCreateSemaphore(ctx->Device, &sem_ci, nullptr, &ctx->ImageAvailableSemaphore);
    VerifyVkResult(vk_result);
    vk_result = vkCreateSemaphore(ctx->Device, &sem_ci, nullptr, &ctx->RenderCompleteSemaphore);
    VerifyVkResult(vk_result);
}

static void BeginFrame(ptr<Vulkan_Renderer::Context> ctx)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(ctx->Swapchain, "Vulkan swapchain is not created");
    FO_VERIFY_AND_THROW(ctx->Device, "Vulkan device is not initialized");

    VkResult vk_result = VK_SUCCESS;

    // Single-threaded: wait for all GPU work to finish before reusing command buffer
    vk_result = vkQueueWaitIdle(ctx->GraphicsQueue);
    VerifyVkResult(vk_result);

    // The GPU consumed everything from the previous frame: ring pools (draw-buffer geometry,
    // texture staging) may reuse their slots from the start.
    ctx->FrameIndex++;

    FlushDeferredDestroyResources(ctx);

    if (ctx->PendingSwapchainRecreateSize.has_value()) {
        const auto recreate_size = ctx->PendingSwapchainRecreateSize.value();
        ctx->PendingSwapchainRecreateSize.reset();
        RecreateSwapchain(ctx, {std::max(recreate_size.width, 1), std::max(recreate_size.height, 1)});
        // Device is idle here (vkQueueWaitIdle above); clear any stale semaphore signal left by a
        // deferred resize or a failed present before the next acquire/submit cycle.
        RecreateFrameSyncSemaphores(ctx);

        if (!ctx->CurrentRenderTarget) {
            ApplySwapchainTargetMetrics(ctx, ctx->SwapchainSize);
        }
    }

    vk_result = vkResetDescriptorPool(ctx->Device, ctx->FrameDescriptorPool, 0);
    VerifyVkResult(vk_result);
    ctx->FrameUniformOffset = 0;

    vk_result = vkAcquireNextImageKHR(ctx->Device, ctx->Swapchain, UINT64_MAX, ctx->ImageAvailableSemaphore, VK_NULL_HANDLE, &ctx->CurrentSwapchainImageIndex);
    if (vk_result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Acquire failed and the semaphore was NOT signaled, so recreating and re-acquiring on the
        // same (now freshly recreated) semaphore is safe.
        int width;
        int height;
        SDL_GetWindowSizeInPixels(ctx->SdlWindow.get(), &width, &height);
        RecreateSwapchain(ctx, {std::max(width, 1), std::max(height, 1)});
        RecreateFrameSyncSemaphores(ctx);

        vk_result = vkAcquireNextImageKHR(ctx->Device, ctx->Swapchain, UINT64_MAX, ctx->ImageAvailableSemaphore, VK_NULL_HANDLE, &ctx->CurrentSwapchainImageIndex);
        VerifyVkResult(vk_result);
    }
    else if (vk_result == VK_SUBOPTIMAL_KHR) {
        // SUBOPTIMAL is a success code: the image WAS acquired and ImageAvailableSemaphore IS
        // signaled. Render/present this frame as-is and defer the recreate to the next BeginFrame
        // (which runs after vkQueueWaitIdle and recreates the semaphores). Re-acquiring on the
        // already-signaled semaphore here would be illegal.
        int width;
        int height;
        SDL_GetWindowSizeInPixels(ctx->SdlWindow.get(), &width, &height);
        ctx->PendingSwapchainRecreateSize = isize32 {std::max(width, 1), std::max(height, 1)};
    }
    else {
        VerifyVkResult(vk_result);
    }

    ctx->ImageAvailableWaited = false;

    vk_result = vkResetCommandBuffer(ctx->CommandBuffer, 0);
    VerifyVkResult(vk_result);

    BeginCommandBufferRecording(ctx->CommandBuffer);

    // Explicitly clear the swapchain image before first render pass begin
    // This ensures valid content for loadOp=LOAD (image starts as UNDEFINED after acquire)
    TransitionColorImage(ctx->CommandBuffer, ctx->SwapchainImages[ctx->CurrentSwapchainImageIndex], ctx->SwapchainImageLayouts[ctx->CurrentSwapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    ctx->SwapchainImageLayouts[ctx->CurrentSwapchainImageIndex] = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    const VkClearColorValue frame_clear = ctx->ClearColor;
    VkImageSubresourceRange clear_range {};
    clear_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clear_range.baseMipLevel = 0;
    clear_range.levelCount = 1;
    clear_range.baseArrayLayer = 0;
    clear_range.layerCount = 1;
    vkCmdClearColorImage(ctx->CommandBuffer, ctx->SwapchainImages[ctx->CurrentSwapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &frame_clear, 1, &clear_range);

    TransitionColorImage(ctx->CommandBuffer, ctx->SwapchainImages[ctx->CurrentSwapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    ctx->SwapchainImageLayouts[ctx->CurrentSwapchainImageIndex] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    BeginCurrentRenderPass(ctx);

    ctx->FrameCbRecording = true;
}

static void EndFrame(ptr<Vulkan_Renderer::Context> ctx)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(ctx->Swapchain, "Vulkan swapchain is not created");
    FO_VERIFY_AND_THROW(ctx->Device, "Vulkan device is not initialized");

    VkResult vk_result = VK_SUCCESS;

    EndCurrentRenderPass(ctx);

    if (!ctx->CurrentRenderTarget && ctx->CurrentSwapchainImageIndex < ctx->SwapchainImages.size() && ctx->CurrentSwapchainImageIndex < ctx->SwapchainImageLayouts.size()) {
        TransitionColorImage(ctx->CommandBuffer, ctx->SwapchainImages[ctx->CurrentSwapchainImageIndex], ctx->SwapchainImageLayouts[ctx->CurrentSwapchainImageIndex], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        ctx->SwapchainImageLayouts[ctx->CurrentSwapchainImageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }

    // End command buffer
    vk_result = vkEndCommandBuffer(ctx->CommandBuffer);
    VerifyVkResult(vk_result);

    // Submit to graphics queue.
    // The acquire semaphore must be waited at the stage of the FIRST use of the freshly acquired
    // swapchain image. BeginFrame's first commands against it are a layout transition + vkCmdClearColorImage
    // which run at TRANSFER; later draws run at COLOR_ATTACHMENT_OUTPUT. Cover both so the GPU never
    // touches the image before the presentation engine has released it.
    ctx->FrameCbRecording = false;

    // A mid-frame flush may have already consumed the acquire semaphore with the frame's first
    // submit; wait for it here only if this is the first submit of the frame.
    constexpr VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &ctx->CommandBuffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &ctx->RenderCompleteSemaphore;

    if (!ctx->ImageAvailableWaited) {
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &ctx->ImageAvailableSemaphore;
        submit_info.pWaitDstStageMask = &wait_stage;
        ctx->ImageAvailableWaited = true;
    }

    vk_result = vkQueueSubmit(ctx->GraphicsQueue, 1, &submit_info, VK_NULL_HANDLE);
    VerifyVkResult(vk_result);

    // Present to screen
    VkPresentInfoKHR info {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &ctx->RenderCompleteSemaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &ctx->Swapchain;
    info.pImageIndices = &ctx->CurrentSwapchainImageIndex;

    vk_result = vkQueuePresentKHR(ctx->GraphicsQueue, &info);
    if (vk_result == VK_ERROR_OUT_OF_DATE_KHR || vk_result == VK_SUBOPTIMAL_KHR) {
        // Defer the recreate to the next BeginFrame rather than recreating inline: a failed present
        // may leave RenderCompleteSemaphore in an ambiguous state, and BeginFrame recreates both
        // sync semaphores after vkQueueWaitIdle, guaranteeing a clean state for the next submit.
        int width;
        int height;
        SDL_GetWindowSizeInPixels(ctx->SdlWindow.get(), &width, &height);
        ctx->PendingSwapchainRecreateSize = isize32 {std::max(width, 1), std::max(height, 1)};
    }
    else {
        VerifyVkResult(vk_result);
    }

    // Begin recording the next frame immediately (command buffer always recording, like OpenGL/D3D)
    BeginFrame(ctx);
}

static void TransitionColorImage(VkCommandBuffer cmd_buf, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout)
{
    FO_STACK_TRACE_ENTRY();

    if (old_layout == new_layout) {
        return;
    }

    VkImageMemoryBarrier img_barrier {};
    img_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    img_barrier.oldLayout = old_layout;
    img_barrier.newLayout = new_layout;
    img_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    img_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    img_barrier.image = image;
    img_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    img_barrier.subresourceRange.baseMipLevel = 0;
    img_barrier.subresourceRange.levelCount = 1;
    img_barrier.subresourceRange.baseArrayLayer = 0;
    img_barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    switch (old_layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        // First use of an image. For freshly acquired swapchain images the acquire semaphore's wait
        // scope is TRANSFER | COLOR_ATTACHMENT_OUTPUT (see EndFrame submit), so chain the transition
        // after those stages; for regular images this only widens the execution dependency.
        img_barrier.srcAccessMask = 0;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        img_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        img_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        img_barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        break;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        img_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        break;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        // The acquired swapchain image is released by the presentation engine through the
        // ImageAvailableSemaphore whose wait scope is TRANSFER | COLOR_ATTACHMENT_OUTPUT (see EndFrame
        // submit). Chain the transition after those stages so it cannot start before the acquire.
        img_barrier.srcAccessMask = 0;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        break;
    default:
        img_barrier.srcAccessMask = 0;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        break;
    }

    switch (new_layout) {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        img_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        img_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        img_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        break;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        // Read is required in the destination scope: the render pass color loadOp=LOAD performs a
        // VK_ACCESS_COLOR_ATTACHMENT_READ_BIT access in this stage, and without it the previous
        // contents are not guaranteed visible to the load (drivers may legally return garbage).
        img_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        break;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        img_barrier.dstAccessMask = 0;
        dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        break;
    default:
        img_barrier.dstAccessMask = 0;
        dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        break;
    }

    vkCmdPipelineBarrier(cmd_buf, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &img_barrier);
}

static void BeginCurrentRenderPass(ptr<Vulkan_Renderer::Context> ctx)
{
    FO_STACK_TRACE_ENTRY();

    VkRenderPassBeginInfo rp_begin {};
    rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin.renderPass = ctx->RenderPass;
    rp_begin.renderArea.offset = {.x = 0, .y = 0};

    VkClearValue clear_values[2] {};
    clear_values[0].color = ctx->ClearColor;
    clear_values[1].depthStencil = {.depth = 1.0f, .stencil = 0};
    rp_begin.clearValueCount = 2;
    rp_begin.pClearValues = clear_values;

    if (ctx->CurrentRenderTarget) {
        auto vk_tex = RenderBackendCast<Vulkan_Texture>(ctx->CurrentRenderTarget.as_ptr());
        EnsureTextureRenderTargetResources(ctx, vk_tex);
        FO_VERIFY_AND_THROW(vk_tex->TextureFramebuffer != nullptr, "Render target framebuffer is not created");
        FO_VERIFY_AND_THROW(vk_tex->TextureImage != nullptr, "Render target image is not created");

        TransitionColorImage(ctx->CommandBuffer, vk_tex->TextureImage, vk_tex->TextureImageLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        vk_tex->TextureImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        rp_begin.framebuffer = vk_tex->TextureFramebuffer;
        rp_begin.renderArea.extent = {.width = numeric_cast<uint32_t>(vk_tex->Size.width), .height = numeric_cast<uint32_t>(vk_tex->Size.height)};
        ctx->ViewPort = irect32 {{0, 0}, {vk_tex->Size.width, vk_tex->Size.height}};
    }
    else {
        FO_VERIFY_AND_THROW(ctx->CurrentSwapchainImageIndex < ctx->Framebuffers.size(), "Swapchain image index out of range");
        FO_VERIFY_AND_THROW(ctx->CurrentSwapchainImageIndex < ctx->SwapchainImageLayouts.size(), "Swapchain image index out of range");

        TransitionColorImage(ctx->CommandBuffer, ctx->SwapchainImages[ctx->CurrentSwapchainImageIndex], ctx->SwapchainImageLayouts[ctx->CurrentSwapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        ctx->SwapchainImageLayouts[ctx->CurrentSwapchainImageIndex] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        rp_begin.framebuffer = ctx->Framebuffers[ctx->CurrentSwapchainImageIndex];
        rp_begin.renderArea.offset = {.x = 0, .y = 0};

        rp_begin.renderArea.extent = {.width = numeric_cast<uint32_t>(ctx->SwapchainSize.width), .height = numeric_cast<uint32_t>(ctx->SwapchainSize.height)};
    }

    vkCmdBeginRenderPass(ctx->CommandBuffer, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
    ApplyViewportAndScissor(ctx);
}

static void EndCurrentRenderPass(ptr<Vulkan_Renderer::Context> ctx)
{
    FO_STACK_TRACE_ENTRY();

    vkCmdEndRenderPass(ctx->CommandBuffer);
}

static void ApplyViewportAndScissor(ptr<Vulkan_Renderer::Context> ctx)
{
    FO_STACK_TRACE_ENTRY();

    // Negative-height viewport (core since Vulkan 1.1): flips Vulkan's Y-down clip space so the
    // same Y-up projection matrices as OpenGL/Direct3D produce top-left-origin output. Winding is
    // flipped by the negative height, which the pipeline frontFace settings account for.
    VkViewport viewport {};
    viewport.x = numeric_cast<float32_t>(ctx->ViewPort.x);
    viewport.y = numeric_cast<float32_t>(ctx->ViewPort.y + ctx->ViewPort.height);
    viewport.width = numeric_cast<float32_t>(ctx->ViewPort.width);
    viewport.height = -numeric_cast<float32_t>(ctx->ViewPort.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(ctx->CommandBuffer, 0, 1, &viewport);

    if (ctx->ScissorEnabled) {
        VkRect2D scissor_rect {};

        int32_t left;
        int32_t top;
        int32_t right;
        int32_t bottom;

        if (ctx->ViewPort.width != ctx->TargetSize.width || ctx->ViewPort.height != ctx->TargetSize.height) {
            const auto x_ratio = checked_div<float32_t>(numeric_cast<float32_t>(ctx->ViewPort.width), numeric_cast<float32_t>(ctx->TargetSize.width));
            const auto y_ratio = checked_div<float32_t>(numeric_cast<float32_t>(ctx->ViewPort.height), numeric_cast<float32_t>(ctx->TargetSize.height));

            left = ctx->ViewPort.x + iround<int32_t>(numeric_cast<float32_t>(ctx->ScissorRect.x) * x_ratio);
            top = ctx->ViewPort.y + iround<int32_t>(numeric_cast<float32_t>(ctx->ScissorRect.y) * y_ratio);
            right = ctx->ViewPort.x + iround<int32_t>(numeric_cast<float32_t>(ctx->ScissorRect.x + ctx->ScissorRect.width) * x_ratio);
            bottom = ctx->ViewPort.y + iround<int32_t>(numeric_cast<float32_t>(ctx->ScissorRect.y + ctx->ScissorRect.height) * y_ratio);
        }
        else {
            left = ctx->ViewPort.x + ctx->ScissorRect.x;
            top = ctx->ViewPort.y + ctx->ScissorRect.y;
            right = ctx->ViewPort.x + ctx->ScissorRect.x + ctx->ScissorRect.width;
            bottom = ctx->ViewPort.y + ctx->ScissorRect.y + ctx->ScissorRect.height;
        }

        // Vulkan requires the scissor to satisfy offset >= 0 and offset + extent <= framebuffer
        // dimensions. OpenGL/D3D clamp implicitly; replicate that here against the active render
        // target's framebuffer size (the full swapchain/texture extent, not the letterboxed viewport).
        int32_t rt_w = ctx->SwapchainSize.width;
        int32_t rt_h = ctx->SwapchainSize.height;

        if (ctx->CurrentRenderTarget) {
            auto rt_tex = RenderBackendCast<const Vulkan_Texture>(ctx->CurrentRenderTarget.as_ptr());
            rt_w = rt_tex->Size.width;
            rt_h = rt_tex->Size.height;
        }

        left = std::clamp(left, 0, rt_w);
        top = std::clamp(top, 0, rt_h);
        right = std::clamp(right, left, rt_w);
        bottom = std::clamp(bottom, top, rt_h);

        scissor_rect.offset = {.x = left, .y = top};
        scissor_rect.extent = {.width = numeric_cast<uint32_t>(right - left), .height = numeric_cast<uint32_t>(bottom - top)};
        vkCmdSetScissor(ctx->CommandBuffer, 0, 1, &scissor_rect);
    }
    else {
        VkRect2D scissor_rect {};
        scissor_rect.offset = {.x = numeric_cast<int32_t>(ctx->ViewPort.x), .y = numeric_cast<int32_t>(ctx->ViewPort.y)};
        scissor_rect.extent = {.width = numeric_cast<uint32_t>(ctx->ViewPort.width), .height = numeric_cast<uint32_t>(ctx->ViewPort.height)};
        vkCmdSetScissor(ctx->CommandBuffer, 0, 1, &scissor_rect);
    }
}

static void EnsureTextureRenderTargetResources(ptr<Vulkan_Renderer::Context> ctx, ptr<Vulkan_Texture> vk_tex)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(vk_tex->TextureImage != nullptr, "Render target image is not created");
    FO_VERIFY_AND_THROW(vk_tex->TextureImageView != nullptr, "Render target image view is not created");

    VkResult vk_result = VK_SUCCESS;

    if (vk_tex->DepthImage == nullptr) {
        AllocateImage(ctx, numeric_cast<uint32_t>(vk_tex->Size.width), numeric_cast<uint32_t>(vk_tex->Size.height), VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vk_tex->DepthImage, vk_tex->DepthImageMemory);

        VkImageViewCreateInfo depth_view_ci {};
        depth_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depth_view_ci.image = vk_tex->DepthImage;
        depth_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depth_view_ci.format = VK_FORMAT_D32_SFLOAT;
        depth_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depth_view_ci.subresourceRange.baseMipLevel = 0;
        depth_view_ci.subresourceRange.levelCount = 1;
        depth_view_ci.subresourceRange.baseArrayLayer = 0;
        depth_view_ci.subresourceRange.layerCount = 1;

        vk_result = vkCreateImageView(ctx->Device, &depth_view_ci, nullptr, &vk_tex->DepthImageView);
        VerifyVkResult(vk_result);
    }

    if (vk_tex->TextureFramebuffer == nullptr) {
        const VkImageView framebuffer_attachments[] = {vk_tex->TextureImageView, vk_tex->DepthImageView};

        VkFramebufferCreateInfo fci {};
        fci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fci.renderPass = ctx->RenderPass;
        fci.attachmentCount = 2;
        fci.pAttachments = framebuffer_attachments;
        fci.width = numeric_cast<uint32_t>(vk_tex->Size.width);
        fci.height = numeric_cast<uint32_t>(vk_tex->Size.height);
        fci.layers = 1;

        vk_result = vkCreateFramebuffer(ctx->Device, &fci, nullptr, &vk_tex->TextureFramebuffer);
        VerifyVkResult(vk_result);
    }
}

void Vulkan_Renderer::Present()
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();
    EndFrame(ctx);
}

// Recomputes the letterboxed viewport, logical target size and projection for rendering into the
// swapchain back buffer. Called when the target switches to the back buffer and when the window or
// logical screen size changes while the back buffer is the active target (e.g. the server host UI
// never calls SetRenderTarget, so a post-init resize must refresh these here).
static void ApplySwapchainTargetMetrics(ptr<Vulkan_Renderer::Context> ctx, isize32 back_buf_size)
{
    FO_STACK_TRACE_ENTRY();

    const auto back_buf_aspect = checked_div<float32_t>(numeric_cast<float32_t>(back_buf_size.width), numeric_cast<float32_t>(back_buf_size.height));
    const auto screen_aspect = checked_div<float32_t>(numeric_cast<float32_t>(ctx->Settings->ScreenWidth), numeric_cast<float32_t>(ctx->Settings->ScreenHeight));
    const auto fit_width = iround<int32_t>(screen_aspect <= back_buf_aspect ? numeric_cast<float32_t>(back_buf_size.height) * screen_aspect : numeric_cast<float32_t>(back_buf_size.height) * back_buf_aspect);
    const auto fit_height = iround<int32_t>(screen_aspect <= back_buf_aspect ? numeric_cast<float32_t>(back_buf_size.width) / back_buf_aspect : numeric_cast<float32_t>(back_buf_size.width) / screen_aspect);

    const auto vp_ox = (back_buf_size.width - fit_width) / 2;
    const auto vp_oy = (back_buf_size.height - fit_height) / 2;
    ctx->ViewPort = irect32 {vp_ox, vp_oy, fit_width, fit_height};
    ctx->TargetSize = {ctx->Settings->ScreenWidth, ctx->Settings->ScreenHeight};
    ctx->ProjMatrix = BuildOrthoMatrix(0.0f, numeric_cast<float32_t>(ctx->TargetSize.width), numeric_cast<float32_t>(ctx->TargetSize.height), 0.0f, ctx->OrthoNear, ctx->OrthoFar);
}

void Vulkan_Renderer::SetRenderTarget(nptr<RenderTexture> tex)
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();

    if (ctx->CurrentRenderTarget == tex) {
        return;
    }

    EndCurrentRenderPass(ctx);

    if (ctx->CurrentRenderTarget) {
        auto prev_tex = RenderBackendCast<Vulkan_Texture>(ctx->CurrentRenderTarget.as_ptr());
        TransitionColorImage(ctx->CommandBuffer, prev_tex->TextureImage, prev_tex->TextureImageLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        prev_tex->TextureImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    ctx->CurrentRenderTarget = tex;

    if (!ctx->CurrentRenderTarget) {
        ApplySwapchainTargetMetrics(ctx, ctx->SwapchainSize);
    }
    else {
        auto vk_tex = RenderBackendCast<const Vulkan_Texture>(ctx->CurrentRenderTarget.as_ptr());
        ctx->ViewPort = irect32 {0, 0, vk_tex->Size.width, vk_tex->Size.height};
        ctx->TargetSize = vk_tex->Size;
    }

    ctx->ProjMatrix = CreateOrthoMatrix(0.0f, numeric_cast<float32_t>(ctx->TargetSize.width), numeric_cast<float32_t>(ctx->TargetSize.height), 0.0f, ctx->OrthoNear, ctx->OrthoFar);

    BeginCurrentRenderPass(ctx);
}

void Vulkan_Renderer::SetOrthoDepthRange(float32_t nearp, float32_t farp) noexcept
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();
    ctx->OrthoNear = nearp;
    ctx->OrthoFar = farp;
    ctx->ProjMatrix = CreateOrthoMatrix(0.0f, numeric_cast<float32_t>(ctx->TargetSize.width), numeric_cast<float32_t>(ctx->TargetSize.height), 0.0f, nearp, farp);
}

auto Vulkan_Renderer::GetProjMatrix() const -> mat44
{
    FO_NO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();
    return ctx->ProjMatrix;
}

void Vulkan_Renderer::ClearRenderTarget(optional<ucolor> color, bool depth, bool stencil)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(stencil);

    if (!color.has_value() && !depth) {
        return;
    }

    auto ctx = _ctx.as_ptr();

    vector<VkClearAttachment> attachments;
    attachments.reserve(2);

    if (color.has_value()) {
        ctx->ClearColor.float32[0] = numeric_cast<float32_t>(color.value().comp.r) / 255.0f;
        ctx->ClearColor.float32[1] = numeric_cast<float32_t>(color.value().comp.g) / 255.0f;
        ctx->ClearColor.float32[2] = numeric_cast<float32_t>(color.value().comp.b) / 255.0f;
        ctx->ClearColor.float32[3] = numeric_cast<float32_t>(color.value().comp.a) / 255.0f;

        VkClearAttachment color_attachment {};
        color_attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        color_attachment.colorAttachment = 0;
        color_attachment.clearValue.color = ctx->ClearColor;
        attachments.emplace_back(color_attachment);
    }

    if (depth) {
        VkClearAttachment depth_attachment {};
        depth_attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depth_attachment.clearValue.depthStencil = {.depth = 1.0f, .stencil = 0};
        attachments.emplace_back(depth_attachment);
    }

    VkClearRect clear_rect {};
    clear_rect.rect.offset = {.x = numeric_cast<int32_t>(ctx->ViewPort.x), .y = numeric_cast<int32_t>(ctx->ViewPort.y)};
    clear_rect.rect.extent = {.width = numeric_cast<uint32_t>(ctx->ViewPort.width), .height = numeric_cast<uint32_t>(ctx->ViewPort.height)};
    clear_rect.baseArrayLayer = 0;
    clear_rect.layerCount = 1;

    vkCmdClearAttachments(ctx->CommandBuffer, numeric_cast<uint32_t>(attachments.size()), attachments.data(), 1, &clear_rect);
}

void Vulkan_Renderer::EnableScissor(irect32 rect)
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();
    ctx->ScissorRect = rect;
    ctx->ScissorEnabled = true;

    ApplyViewportAndScissor(ctx);
}

void Vulkan_Renderer::DisableScissor()
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();
    ctx->ScissorEnabled = false;

    ApplyViewportAndScissor(ctx);
}

void Vulkan_Renderer::OnResizeWindow(isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    auto ctx = _ctx.as_ptr();
    const auto clamped_size = isize32 {std::max(size.width, 1), std::max(size.height, 1)};
    ctx->PendingSwapchainRecreateSize = clamped_size;

    if (!ctx->CurrentRenderTarget) {
        // The back buffer stays the active target across the resize (the server host UI never calls
        // SetRenderTarget), so refresh the letterbox/projection here against the new size.
        ApplySwapchainTargetMetrics(ctx, clamped_size);
    }
    else {
        ctx->ViewPort = irect32 {{0, 0}, clamped_size};
    }
}

static void DestroyResourceSafe(ptr<Vulkan_Renderer::Context> ctx, VkBuffer& buffer)
{
    FO_STACK_TRACE_ENTRY();

    if (buffer != VK_NULL_HANDLE) {
        ctx->DeferredDestroyBuffers.emplace_back(buffer);
        buffer = VK_NULL_HANDLE;
    }
}

static void DestroyResourceSafe(ptr<Vulkan_Renderer::Context> ctx, VkDeviceMemory& memory)
{
    FO_STACK_TRACE_ENTRY();

    if (memory != VK_NULL_HANDLE) {
        ctx->DeferredDestroyMemories.emplace_back(memory);
        memory = VK_NULL_HANDLE;
    }
}

static void DestroyResourceSafe(ptr<Vulkan_Renderer::Context> ctx, VkImage& image)
{
    FO_STACK_TRACE_ENTRY();

    if (image != VK_NULL_HANDLE) {
        ctx->DeferredDestroyImages.emplace_back(image);
        image = VK_NULL_HANDLE;
    }
}

static void DestroyResourceSafe(ptr<Vulkan_Renderer::Context> ctx, VkImageView& image_view)
{
    FO_STACK_TRACE_ENTRY();

    if (image_view != VK_NULL_HANDLE) {
        ctx->DeferredDestroyImageViews.emplace_back(image_view);
        image_view = VK_NULL_HANDLE;
    }
}

static void DestroyResourceSafe(ptr<Vulkan_Renderer::Context> ctx, VkFramebuffer& framebuffer)
{
    FO_STACK_TRACE_ENTRY();

    if (framebuffer != VK_NULL_HANDLE) {
        ctx->DeferredDestroyFramebuffers.emplace_back(framebuffer);
        framebuffer = VK_NULL_HANDLE;
    }
}

static void FlushDeferredDestroyResources(ptr<Vulkan_Renderer::Context> ctx)
{
    FO_STACK_TRACE_ENTRY();

    for (auto* framebuffer : ctx->DeferredDestroyFramebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(ctx->Device, framebuffer, nullptr);
        }
    }
    ctx->DeferredDestroyFramebuffers.clear();

    for (auto* image_view : ctx->DeferredDestroyImageViews) {
        if (image_view != VK_NULL_HANDLE) {
            vkDestroyImageView(ctx->Device, image_view, nullptr);
        }
    }
    ctx->DeferredDestroyImageViews.clear();

    for (auto* image : ctx->DeferredDestroyImages) {
        if (image != VK_NULL_HANDLE) {
            vkDestroyImage(ctx->Device, image, nullptr);
        }
    }
    ctx->DeferredDestroyImages.clear();

    for (auto* buffer : ctx->DeferredDestroyBuffers) {
        if (buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(ctx->Device, buffer, nullptr);
        }
    }
    ctx->DeferredDestroyBuffers.clear();

    for (auto* memory : ctx->DeferredDestroyMemories) {
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(ctx->Device, memory, nullptr);
        }
    }
    ctx->DeferredDestroyMemories.clear();
}

FO_END_NAMESPACE

#endif // FO_HAVE_VULKAN
