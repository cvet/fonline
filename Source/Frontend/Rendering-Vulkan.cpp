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

// The Vulkan loader is resolved dynamically through SDL (SDL_Vulkan_LoadLibrary +
// SDL_Vulkan_GetVkGetInstanceProcAddr) instead of link-time against vulkan-1.lib, so a client
// built with Vulkan support still launches on a machine without the Vulkan runtime — the loader is
// only pulled in when the Vulkan backend is actually selected. VK_NO_PROTOTYPES suppresses the
// header's function declarations; the entry points below are the sole definitions.
#define VK_NO_PROTOTYPES
#include "SDL3/SDL.h"
#include "SDL3/SDL_video.h"
#include "SDL3/SDL_vulkan.h"
#include <vulkan/vulkan.h>

FO_BEGIN_NAMESPACE

static constexpr uint32_t VULKAN_MAX_TEXTURE_BINDINGS = 16;
static constexpr uint32_t VULKAN_MAX_UNIFORM_BINDINGS = 16;
static constexpr uint32_t VULKAN_FRAMES_IN_FLIGHT = 2;

// Dynamically-loaded Vulkan entry points (mirrors the OpenGL backend's SDL_GL_GetProcAddress table).
// vkGetInstanceProcAddr is the bootstrap obtained from SDL; global functions load with a null
// instance before one exists; the rest load from the created instance. These file-scope pointers are
// process-global loader dispatch, not per-engine state, and the client renderer is single-instance.
static PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;

#define FO_VK_GLOBAL_FUNCTIONS(X) \
    X(vkCreateInstance); \
    X(vkEnumerateInstanceLayerProperties)

#define FO_VK_INSTANCE_FUNCTIONS(X) \
    X(vkDestroyInstance); \
    X(vkEnumeratePhysicalDevices); \
    X(vkGetPhysicalDeviceProperties); \
    X(vkGetPhysicalDeviceMemoryProperties); \
    X(vkGetPhysicalDeviceQueueFamilyProperties); \
    X(vkGetPhysicalDeviceSurfaceSupportKHR); \
    X(vkGetPhysicalDeviceSurfaceCapabilitiesKHR); \
    X(vkGetPhysicalDeviceSurfaceFormatsKHR); \
    X(vkGetPhysicalDeviceSurfacePresentModesKHR); \
    X(vkEnumerateDeviceExtensionProperties); \
    X(vkCreateDevice); \
    X(vkDestroyDevice); \
    X(vkGetDeviceQueue); \
    X(vkDeviceWaitIdle); \
    X(vkDestroySurfaceKHR); \
    X(vkCreateSwapchainKHR); \
    X(vkDestroySwapchainKHR); \
    X(vkGetSwapchainImagesKHR); \
    X(vkAcquireNextImageKHR); \
    X(vkQueuePresentKHR); \
    X(vkCreateImage); \
    X(vkDestroyImage); \
    X(vkCreateImageView); \
    X(vkDestroyImageView); \
    X(vkGetImageMemoryRequirements); \
    X(vkBindImageMemory); \
    X(vkCreateBuffer); \
    X(vkDestroyBuffer); \
    X(vkGetBufferMemoryRequirements); \
    X(vkBindBufferMemory); \
    X(vkAllocateMemory); \
    X(vkFreeMemory); \
    X(vkMapMemory); \
    X(vkUnmapMemory); \
    X(vkCreateSampler); \
    X(vkDestroySampler); \
    X(vkCreateRenderPass); \
    X(vkDestroyRenderPass); \
    X(vkCreateFramebuffer); \
    X(vkDestroyFramebuffer); \
    X(vkCreateShaderModule); \
    X(vkDestroyShaderModule); \
    X(vkCreateDescriptorSetLayout); \
    X(vkDestroyDescriptorSetLayout); \
    X(vkCreatePipelineLayout); \
    X(vkDestroyPipelineLayout); \
    X(vkCreateGraphicsPipelines); \
    X(vkDestroyPipeline); \
    X(vkCreateDescriptorPool); \
    X(vkDestroyDescriptorPool); \
    X(vkResetDescriptorPool); \
    X(vkAllocateDescriptorSets); \
    X(vkUpdateDescriptorSets); \
    X(vkCreateCommandPool); \
    X(vkDestroyCommandPool); \
    X(vkAllocateCommandBuffers); \
    X(vkBeginCommandBuffer); \
    X(vkEndCommandBuffer); \
    X(vkResetCommandBuffer); \
    X(vkCreateFence); \
    X(vkDestroyFence); \
    X(vkWaitForFences); \
    X(vkResetFences); \
    X(vkCreateSemaphore); \
    X(vkDestroySemaphore); \
    X(vkQueueSubmit); \
    X(vkQueueWaitIdle); \
    X(vkCmdBeginRenderPass); \
    X(vkCmdEndRenderPass); \
    X(vkCmdBindPipeline); \
    X(vkCmdBindDescriptorSets); \
    X(vkCmdBindVertexBuffers); \
    X(vkCmdBindIndexBuffer); \
    X(vkCmdSetViewport); \
    X(vkCmdSetScissor); \
    X(vkCmdDraw); \
    X(vkCmdDrawIndexed); \
    X(vkCmdClearAttachments); \
    X(vkCmdClearColorImage); \
    X(vkCmdCopyBuffer); \
    X(vkCmdCopyBufferToImage); \
    X(vkCmdCopyImageToBuffer); \
    X(vkCmdPipelineBarrier)

#define FO_VK_FUNCTION_DEF(name) static PFN_##name name = nullptr
FO_VK_GLOBAL_FUNCTIONS(FO_VK_FUNCTION_DEF);
FO_VK_INSTANCE_FUNCTIONS(FO_VK_FUNCTION_DEF);
#undef FO_VK_FUNCTION_DEF

static void LoadVulkanGlobalFunctions() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#define FO_VK_LOAD_GLOBAL(name) name = reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(VK_NULL_HANDLE, #name))
    FO_VK_GLOBAL_FUNCTIONS(FO_VK_LOAD_GLOBAL);
#undef FO_VK_LOAD_GLOBAL
}

static void LoadVulkanInstanceFunctions(VkInstance instance) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#define FO_VK_LOAD_INSTANCE(name) name = reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(instance, #name))
    FO_VK_INSTANCE_FUNCTIONS(FO_VK_LOAD_INSTANCE);
#undef FO_VK_LOAD_INSTANCE
}

// Growable, persistently-mapped HOST_VISIBLE buffer of a ring pool; steady-state uploads
// are pure memcpy.
struct VulkanPooledBuffer
{
    VkBuffer Buffer {};
    VkDeviceMemory Memory {};
    VkDeviceSize Capacity {};
    nptr<void> Mapped {};
};

// Ring of pooled buffers: one buffer per upload within a frame, index reset on frame-stamp mismatch.
struct VulkanBufferRing
{
    vector<VulkanPooledBuffer> Buffers {};
    size_t Index {};
    uint64_t Frame {};
};

// Destroy requests of one frame slot; flushed after the slot's fence (its signal implies
// completion of all earlier submissions too).
struct VulkanDeferredDestroyQueue
{
    vector<VkBuffer> Buffers {};
    vector<VkDeviceMemory> Memories {};
    vector<VkImage> Images {};
    vector<VkImageView> ImageViews {};
    vector<VkFramebuffer> Framebuffers {};
};

// Resources of one in-flight frame slot; reused only after BeginFrame waits the slot's fence.
struct VulkanFrameSlot
{
    VkCommandBuffer CommandBuffer {};
    VkFence InFlightFence {};
    VkSemaphore ImageAvailableSemaphore {};
    VkDescriptorPool DescriptorPool {};
    VkBuffer UniformBuffer {};
    VkDeviceMemory UniformBufferMemory {};
    nptr<void> UniformBufferMapped {};
    VulkanBufferRing TextureStagingRing {};
    VulkanDeferredDestroyQueue DestroyQueue {};
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

    // In-flight frame slots; draw code uses the current-slot aliases refreshed by BeginFrame.
    VulkanFrameSlot FrameSlots[VULKAN_FRAMES_IN_FLIGHT] {};
    VkCommandBuffer CommandBuffer {}; // Current slot's command buffer
    VkSemaphore ImageAvailableSemaphore {}; // Current slot's acquire semaphore

    // Per swapchain image, so a semaphore is never re-signaled while presentation may still wait on it
    vector<VkSemaphore> RenderCompleteSemaphores {};
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
    VkDescriptorPool FrameDescriptorPool {}; // Current slot's descriptor pool
    VkBuffer FrameUniformBuffer {}; // Current slot's uniform bump buffer
    nptr<void> FrameUniformBufferMapped {}; // Current slot's persistent uniform mapping
    size_t FrameUniformOffset {};
    size_t FrameUniformBufferSize {numeric_cast<size_t>(16 * 1024 * 1024)}; // 16 MB per frame slot
    VkSampler LinearSampler {};
    VkSampler PointSampler {};
    nptr<RenderTexture> CurrentRenderTarget {}; // Current render target (nullptr = swapchain)
    irect32 ScissorRect {};
    bool ScissorEnabled {};
    uint32_t CurrentSwapchainImageIndex {};
    optional<isize32> PendingSwapchainRecreateSize {};

    VkDebugUtilsMessengerEXT DebugMessenger {};

    // Cached immutable device properties (queried once in Init) to avoid per-draw / per-allocation queries.
    VkDeviceSize MinUniformBufferOffsetAlignment {};
    VkPhysicalDeviceMemoryProperties MemoryProperties {};

    bool FrameCbRecording {}; // Frame command buffer is currently recording (between BeginFrame and Present)
    bool ImageAvailableWaited {}; // ImageAvailableSemaphore was consumed by a queue submit this frame

    // Monotonic frame counter; selects the in-flight slot and stamps ring pools
    uint64_t FrameIndex {1};

    [[nodiscard]] auto CurrentFrameSlot() -> VulkanFrameSlot& { return FrameSlots[FrameIndex % VULKAN_FRAMES_IN_FLIGHT]; }
};

class Vulkan_Texture;

static void FlushFrameCommandBufferMidFrame(ptr<Vulkan_Renderer::Context> ctx);
static auto BuildOrthoMatrix(float32_t left, float32_t right, float32_t bottom, float32_t top, float32_t nearp, float32_t farp) -> mat44;
static void ApplySwapchainTargetMetrics(ptr<Vulkan_Renderer::Context> ctx, isize32 back_buf_size);
static void RecreateSwapchain(ptr<Vulkan_Renderer::Context> ctx, isize32 size);
static void RecreateFrameSyncObjects(ptr<Vulkan_Renderer::Context> ctx);
static void AllocateBuffer(ptr<Vulkan_Renderer::Context> ctx, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory);
static void AllocateImage(ptr<Vulkan_Renderer::Context> ctx, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& memory);
static void EnsurePooledBufferCapacity(ptr<Vulkan_Renderer::Context> ctx, VulkanPooledBuffer& pooled, VkDeviceSize size, VkBufferUsageFlags usage);
static auto AcquireRingBuffer(ptr<Vulkan_Renderer::Context> ctx, VulkanBufferRing& ring, VkDeviceSize size, VkBufferUsageFlags usage) -> VulkanPooledBuffer&;
static void DestroyPooledBuffers(ptr<Vulkan_Renderer::Context> ctx, VulkanBufferRing& ring);
static void BeginFrame(ptr<Vulkan_Renderer::Context> ctx);
static void EndFrame(ptr<Vulkan_Renderer::Context> ctx);
static void TransitionColorImage(VkCommandBuffer cmd_buf, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout);
static void BeginCurrentRenderPass(ptr<Vulkan_Renderer::Context> ctx);
static void EndCurrentRenderPass(ptr<Vulkan_Renderer::Context> ctx);
static void ApplyViewportAndScissor(ptr<Vulkan_Renderer::Context> ctx);
static void EnsureTextureRenderTargetResources(ptr<Vulkan_Renderer::Context> ctx, ptr<Vulkan_Texture> vk_tex);
static void DestroyBufferSafe(ptr<Vulkan_Renderer::Context> ctx, VkBuffer& buffer);
static void DestroyMemorySafe(ptr<Vulkan_Renderer::Context> ctx, VkDeviceMemory& memory);
static void DestroyImageSafe(ptr<Vulkan_Renderer::Context> ctx, VkImage& image);
static void DestroyImageViewSafe(ptr<Vulkan_Renderer::Context> ctx, VkImageView& image_view);
static void DestroyFramebufferSafe(ptr<Vulkan_Renderer::Context> ctx, VkFramebuffer& framebuffer);
static void FlushDeferredDestroyQueue(ptr<Vulkan_Renderer::Context> ctx, VulkanDeferredDestroyQueue& queue);
static void FlushAllDeferredDestroyQueues(ptr<Vulkan_Renderer::Context> ctx);
static void BeginCommandBufferRecording(VkCommandBuffer cmd_buf);
static void EndCommandBufferRecording(VkCommandBuffer cmd_buf);
static void SubmitCommandBufferAndWait(ptr<Vulkan_Renderer::Context> ctx, VkCommandBuffer cmd_buf);
static void ResetCommandBufferRecording(VkCommandBuffer cmd_buf);

static VKAPI_ATTR auto VKAPI_CALL VulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* data, void* user_data) noexcept -> VkBool32
{
    FO_NO_STACK_TRACE_ENTRY();

    ignore_unused(type, user_data);

    const string_view sev = severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ? string_view {"ERROR"} : severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT ? string_view {"WARN"} : string_view {"INFO"};
    const char* message_id = data != nullptr && data->pMessageIdName != nullptr ? data->pMessageIdName : "?";
    const char* message = data != nullptr && data->pMessage != nullptr ? data->pMessage : "?";
    WriteLog("[VkLayer/{}] {}: {}", sev, message_id, message);
    return VK_FALSE;
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

    // Buffers the next draw binds (last Upload's ring buffer, or the static buffer)
    VkBuffer CurrentVertexBuffer {};
    VkBuffer CurrentIndexBuffer {};

    // Dynamic geometry rings, one per in-flight frame slot: a fresh ring buffer per Upload keeps
    // still-pending draws' geometry snapshots intact.
    VulkanBufferRing VertexBufferRings[VULKAN_FRAMES_IN_FLIGHT] {};
    VulkanBufferRing IndexBufferRings[VULKAN_FRAMES_IN_FLIGHT] {};

    // Static geometry (uploaded once via staging copy to device-local memory)
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
    // Per-pass pipelines indexed by [pass][primitive_type][blend_disabled?]: the second variant
    // honours RenderEffect::DisableBlending (opaque blits; alpha-blending them drops the HUD).
    VkPipeline Pipeline[EFFECT_MAX_PASSES][5][2] {};
    VkPipelineLayout PipelineLayout {};

private:
    ptr<Vulkan_Renderer::Context> _ctx;
};

// Submits the partially recorded frame command buffer and resumes recording; needed before an
// immediate readback that must observe commands recorded earlier this frame.
static void FlushFrameCommandBufferMidFrame(ptr<Vulkan_Renderer::Context> ctx)
{
    FO_STACK_TRACE_ENTRY();

    if (!ctx->FrameCbRecording) {
        return;
    }

    // The frame recording is torn down below; on any throw the flag honestly reads false so a later
    // UpdateTextureRegion takes the safe standalone-staging path instead of recording into a dead buffer.
    ctx->FrameCbRecording = false;

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

    ctx->FrameCbRecording = true;
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

    // Growth math still uses the old capacity, so compute it before invalidating the slot.
    constexpr VkDeviceSize min_capacity = 4096;
    const VkDeviceSize new_capacity = std::max({size, pooled.Capacity * 2, min_capacity});

    // Growth is deferred-destroyed like any other in-flight resource; steady state never grows.
    DestroyBufferSafe(ctx, pooled.Buffer);
    DestroyMemorySafe(ctx, pooled.Memory);
    pooled.Mapped = nullptr;
    // Invalidate capacity before the first throwing op: on throw the slot stays a consistent empty buffer.
    pooled.Capacity = 0;

    AllocateBuffer(ctx, new_capacity, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, pooled.Buffer, pooled.Memory);

    // Persistent mapping: slots stay mapped for their whole lifetime, uploads are plain memcpy.
    void* mapped_raw {};
    const auto vk_result = vkMapMemory(ctx->Device, pooled.Memory, 0, VK_WHOLE_SIZE, 0, &mapped_raw);
    VerifyVkResult(vk_result);
    // No-throw commit tail.
    pooled.Capacity = new_capacity;
    pooled.Mapped = mapped_raw;
}

static auto AcquireRingBuffer(ptr<Vulkan_Renderer::Context> ctx, VulkanBufferRing& ring, VkDeviceSize size, VkBufferUsageFlags usage) -> VulkanPooledBuffer&
{
    FO_STACK_TRACE_ENTRY();

    // Stamp mismatch means the owning slot's fence was waited: every buffer here is GPU-free
    if (ring.Frame != ctx->FrameIndex) {
        ring.Frame = ctx->FrameIndex;
        ring.Index = 0;
    }

    if (ring.Index >= ring.Buffers.size()) {
        ring.Buffers.emplace_back();
    }

    auto& pooled = ring.Buffers[ring.Index];
    ring.Index++;

    EnsurePooledBufferCapacity(ctx, pooled, size, usage);
    return pooled;
}

static void DestroyPooledBuffers(ptr<Vulkan_Renderer::Context> ctx, VulkanBufferRing& ring)
{
    FO_STACK_TRACE_ENTRY();

    for (auto& pooled : ring.Buffers) {
        DestroyBufferSafe(ctx, pooled.Buffer);
        DestroyMemorySafe(ctx, pooled.Memory);
        pooled.Mapped = nullptr;
        pooled.Capacity = 0;
    }

    ring.Buffers.clear();
    ring.Index = 0;
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

    DestroyFramebufferSafe(_ctx, TextureFramebuffer);
    DestroyImageViewSafe(_ctx, TextureImageView);
    DestroyImageSafe(_ctx, TextureImage);
    DestroyMemorySafe(_ctx, TextureImageMemory);
    DestroyImageViewSafe(_ctx, DepthImageView);
    DestroyImageSafe(_ctx, DepthImage);
    DestroyMemorySafe(_ctx, DepthImageMemory);
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

    // Flush pending frame commands so this immediate readback observes them
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
    auto map_data = make_nptr(map_data_raw);
    FO_VERIFY_AND_THROW(map_data, "Mapped memory data pointer is null");
    tex_region.resize(size.square());
    MemCopy(tex_region.data(), map_data, region_size);
    vkUnmapMemory(_ctx->Device, staging_mem);

    // Swizzle B↔R: VK_FORMAT_B8G8R8A8_UNORM stores {B,G,R,A} but ucolor expects {R,G,B,A}
    {
        auto pixels = make_nptr(tex_region.data()).reinterpret_as<uint8_t>();
        FO_VERIFY_AND_THROW(pixels, "Texture region pixel data is null");
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

    const auto source_data = make_nptr(data.data());
    FO_VERIFY_AND_THROW(source_data, "Texture update source data is null");
    auto source_bytes = source_data.reinterpret_as<uint8_t>();

    // Fills staging with the region pixels swizzled R<->B for VK_FORMAT_B8G8R8A8_UNORM. Compose
    // from the source so the write-combined staging memory is never read back (an in-place swap
    // costs milliseconds per large region); inter-row gaps on the use_dest_pitch path are skipped
    // by the copy via bufferRowLength.
    const auto fill_staging = [&](ptr<uint8_t> mapped_bytes) {
        const size_t pixels_per_row = numeric_cast<size_t>(size.width);

        for (int32_t y = 0; y < size.height; y++) {
            const size_t row_offset = numeric_cast<size_t>(y) * data_stride;
            auto src_row = source_bytes.offset(row_offset);
            auto dst_row = mapped_bytes.offset(row_offset);

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
    if (TextureImage == VK_NULL_HANDLE) {
        // Build every image/view into locals first; on any throw the members stay null so the whole
        // block re-runs cleanly next call instead of leaving TextureImage set with a null view behind.
        VkImage tex_image {};
        VkDeviceMemory tex_image_memory {};
        AllocateImage(_ctx, numeric_cast<uint32_t>(Size.width), numeric_cast<uint32_t>(Size.height), VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tex_image, tex_image_memory);

        // Create image view
        VkImageViewCreateInfo image_view_ci {};
        image_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_ci.image = tex_image;
        image_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_ci.format = VK_FORMAT_B8G8R8A8_UNORM;
        image_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_ci.subresourceRange.baseMipLevel = 0;
        image_view_ci.subresourceRange.levelCount = 1;
        image_view_ci.subresourceRange.baseArrayLayer = 0;
        image_view_ci.subresourceRange.layerCount = 1;

        VkImageView tex_image_view {};
        vk_result = vkCreateImageView(_ctx->Device, &image_view_ci, nullptr, &tex_image_view);
        VerifyVkResult(vk_result);

        VkImage depth_image {};
        VkDeviceMemory depth_image_memory {};
        VkImageView depth_image_view {};

        // Create depth buffer if requested
        if (WithDepth) {
            AllocateImage(_ctx, numeric_cast<uint32_t>(Size.width), numeric_cast<uint32_t>(Size.height), VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depth_image, depth_image_memory);

            // Create depth image view
            VkImageViewCreateInfo depth_view_ci {};
            depth_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            depth_view_ci.image = depth_image;
            depth_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            depth_view_ci.format = VK_FORMAT_D32_SFLOAT;
            depth_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            depth_view_ci.subresourceRange.baseMipLevel = 0;
            depth_view_ci.subresourceRange.levelCount = 1;
            depth_view_ci.subresourceRange.baseArrayLayer = 0;
            depth_view_ci.subresourceRange.layerCount = 1;

            vk_result = vkCreateImageView(_ctx->Device, &depth_view_ci, nullptr, &depth_image_view);
            VerifyVkResult(vk_result);
        }

        // No-throw commit tail.
        TextureImage = tex_image;
        TextureImageMemory = tex_image_memory;
        TextureImageView = tex_image_view;
        DepthImage = depth_image;
        DepthImageMemory = depth_image_memory;
        DepthImageView = depth_image_view;
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
        // Record the upload into the frame command buffer: program order preserves the engine's
        // immediate-mode ordering (e.g. an earlier atlas clear) with no mid-frame submit or wait.
        auto& staging = AcquireRingBuffer(_ctx, _ctx->CurrentFrameSlot().TextureStagingRing, total_data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        FO_VERIFY_AND_THROW(staging.Mapped, "Mapped memory data pointer is null");
        fill_staging(staging.Mapped.reinterpret_as<uint8_t>());

        // Transfers are illegal inside a render pass
        EndCurrentRenderPass(_ctx);

        TransitionColorImage(_ctx->CommandBuffer, TextureImage, old_layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        vkCmdCopyBufferToImage(_ctx->CommandBuffer, staging.Buffer, TextureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        TransitionColorImage(_ctx->CommandBuffer, TextureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        TextureImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        BeginCurrentRenderPass(_ctx);
    }
    else {
        // No frame recording (texture init): upload immediately through the staging command buffer
        VkBuffer staging_buf {};
        VkDeviceMemory staging_mem {};
        AllocateBuffer(_ctx, total_data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buf, staging_mem);

        void* mapped_data_raw {};
        vk_result = vkMapMemory(_ctx->Device, staging_mem, 0, total_data_size, 0, &mapped_data_raw);
        VerifyVkResult(vk_result);
        auto mapped_data = make_nptr(mapped_data_raw);
        FO_VERIFY_AND_THROW(mapped_data, "Mapped memory data pointer is null");
        fill_staging(mapped_data.reinterpret_as<uint8_t>());
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

    for (auto& ring : VertexBufferRings) {
        DestroyPooledBuffers(_ctx, ring);
    }
    for (auto& ring : IndexBufferRings) {
        DestroyPooledBuffers(_ctx, ring);
    }

    DestroyBufferSafe(_ctx, StaticVertexBuffer);
    DestroyMemorySafe(_ctx, StaticVertexBufferMemory);
    DestroyBufferSafe(_ctx, StaticIndexBuffer);
    DestroyMemorySafe(_ctx, StaticIndexBufferMemory);
}

void Vulkan_DrawBuffer::Upload(EffectUsage usage, optional<size_t> custom_vertices_size, optional<size_t> custom_indices_size)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_ctx->Device, "Vulkan device is not initialized");

    // Match OpenGL behavior: skip only for static buffers with no changes
    if (IsStatic && !StaticDataChanged) {
        return;
    }

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
            // Dynamic geometry: memcpy into the next ring buffer (own buffer per Upload keeps
            // pending draws' snapshots intact)
            auto& pooled = AcquireRingBuffer(_ctx, VertexBufferRings[_ctx->FrameIndex % VULKAN_FRAMES_IN_FLIGHT], vert_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
            FO_VERIFY_AND_THROW(pooled.Mapped, "Mapped memory data pointer is null");
            MemCopy(pooled.Mapped, vert_data.get(), vert_size);
            CurrentVertexBuffer = pooled.Buffer;
        }
        else {
            // Static buffers: use staging copy to GPU-local memory. Build into locals and keep the old
            // static buffers alive/referenced until the copy succeeds, so a throw leaves the current
            // static vertex buffer valid and StaticDataChanged still true for a clean retry.
            VkBuffer staging_vert_buf {};
            VkDeviceMemory staging_vert_mem {};

            AllocateBuffer(_ctx, vert_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_vert_buf, staging_vert_mem);

            void* mapped_data_raw {};
            vk_result = vkMapMemory(_ctx->Device, staging_vert_mem, 0, vert_size, 0, &mapped_data_raw);
            VerifyVkResult(vk_result);
            auto mapped_data = make_nptr(mapped_data_raw);
            FO_VERIFY_AND_THROW(mapped_data, "Mapped memory data pointer is null");
            MemCopy(mapped_data, vert_data.get(), vert_size);
            vkUnmapMemory(_ctx->Device, staging_vert_mem);

            VkBuffer new_static_buf {};
            VkDeviceMemory new_static_mem {};
            AllocateBuffer(_ctx, vert_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, new_static_buf, new_static_mem);

            VkBufferCopy copy_region {};
            copy_region.size = vert_size;

            BeginCommandBufferRecording(_ctx->StagingCommandBuffer);

            vkCmdCopyBuffer(_ctx->StagingCommandBuffer, staging_vert_buf, new_static_buf, 1, &copy_region);

            EndCommandBufferRecording(_ctx->StagingCommandBuffer);
            SubmitCommandBufferAndWait(_ctx, _ctx->StagingCommandBuffer);

            vkDestroyBuffer(_ctx->Device, staging_vert_buf, nullptr);
            vkFreeMemory(_ctx->Device, staging_vert_mem, nullptr);

            ResetCommandBufferRecording(_ctx->StagingCommandBuffer);

            // No-throw commit tail: retire the old buffers and swap in the new ones.
            DestroyBufferSafe(_ctx, StaticVertexBuffer);
            DestroyMemorySafe(_ctx, StaticVertexBufferMemory);
            StaticVertexBuffer = new_static_buf;
            StaticVertexBufferMemory = new_static_mem;
            CurrentVertexBuffer = StaticVertexBuffer;
        }
    }

    if (idx_size != 0) {
        if (!IsStatic) {
            // Dynamic geometry: same ring scheme as the vertex path
            auto& pooled = AcquireRingBuffer(_ctx, IndexBufferRings[_ctx->FrameIndex % VULKAN_FRAMES_IN_FLIGHT], idx_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
            FO_VERIFY_AND_THROW(pooled.Mapped, "Mapped memory data pointer is null");
            MemCopy(pooled.Mapped, Indices.data(), idx_size);
            CurrentIndexBuffer = pooled.Buffer;
        }
        else {
            // Static buffers: use staging copy to GPU-local memory. Build into locals and keep the old
            // static buffers alive/referenced until the copy succeeds, so a throw leaves the current
            // static index buffer valid and StaticDataChanged still true for a clean retry.
            VkBuffer staging_idx_buf {};
            VkDeviceMemory staging_idx_mem {};
            AllocateBuffer(_ctx, idx_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_idx_buf, staging_idx_mem);

            void* mapped_data_raw {};
            vk_result = vkMapMemory(_ctx->Device, staging_idx_mem, 0, idx_size, 0, &mapped_data_raw);
            VerifyVkResult(vk_result);
            auto mapped_data = make_nptr(mapped_data_raw);
            FO_VERIFY_AND_THROW(mapped_data, "Mapped memory data pointer is null");
            MemCopy(mapped_data, Indices.data(), idx_size);
            vkUnmapMemory(_ctx->Device, staging_idx_mem);

            VkBuffer new_static_buf {};
            VkDeviceMemory new_static_mem {};
            AllocateBuffer(_ctx, idx_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, new_static_buf, new_static_mem);

            VkBufferCopy copy_region {};
            copy_region.size = idx_size;

            BeginCommandBufferRecording(_ctx->StagingCommandBuffer);

            vkCmdCopyBuffer(_ctx->StagingCommandBuffer, staging_idx_buf, new_static_buf, 1, &copy_region);

            EndCommandBufferRecording(_ctx->StagingCommandBuffer);
            SubmitCommandBufferAndWait(_ctx, _ctx->StagingCommandBuffer);

            vkDestroyBuffer(_ctx->Device, staging_idx_buf, nullptr);
            vkFreeMemory(_ctx->Device, staging_idx_mem, nullptr);

            ResetCommandBufferRecording(_ctx->StagingCommandBuffer);

            // No-throw commit tail: retire the old buffers and swap in the new ones.
            DestroyBufferSafe(_ctx, StaticIndexBuffer);
            DestroyMemorySafe(_ctx, StaticIndexBufferMemory);
            StaticIndexBuffer = new_static_buf;
            StaticIndexBufferMemory = new_static_mem;
            CurrentIndexBuffer = StaticIndexBuffer;
        }
    }

    // All static builds succeeded: mark the static data uploaded (no-throw commit).
    StaticDataChanged = false;
}

Vulkan_Effect::~Vulkan_Effect()
{
    FO_STACK_TRACE_ENTRY();

    for (size_t pass = 0; pass < EFFECT_MAX_PASSES; pass++) {
        if (VertexShaderModule[pass] != VK_NULL_HANDLE) {
            vkDestroyShaderModule(_ctx->Device, VertexShaderModule[pass], nullptr);
            VertexShaderModule[pass] = VK_NULL_HANDLE;
        }
        if (FragmentShaderModule[pass] != VK_NULL_HANDLE) {
            vkDestroyShaderModule(_ctx->Device, FragmentShaderModule[pass], nullptr);
            FragmentShaderModule[pass] = VK_NULL_HANDLE;
        }
        for (size_t prim = 0; prim < 5; prim++) {
            for (size_t blend_disabled = 0; blend_disabled < 2; blend_disabled++) {
                if (Pipeline[pass][prim][blend_disabled] != VK_NULL_HANDLE) {
                    vkDestroyPipeline(_ctx->Device, Pipeline[pass][prim][blend_disabled], nullptr);
                    Pipeline[pass][prim][blend_disabled] = VK_NULL_HANDLE;
                }
            }
        }
    }

    if (PipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(_ctx->Device, PipelineLayout, nullptr);
        PipelineLayout = VK_NULL_HANDLE;
    }
}

void Vulkan_Effect::DrawBuffer(ptr<RenderDrawBuffer> dbuf, size_t start_index, optional<size_t> indices_to_draw, nptr<const RenderTexture> custom_tex)
{
    FO_STACK_TRACE_ENTRY();

    if (_ctx->Device == VK_NULL_HANDLE) {
        return;
    }

    auto vk_dbuf = dbuf.dyn_cast<Vulkan_DrawBuffer>();
    FO_VERIFY_AND_THROW(vk_dbuf, "Vulkan draw buffer is not of the expected backend type");

#if FO_ENABLE_3D
    if (!custom_tex && ModelTex[0]) {
        custom_tex = ModelTex[0];
    }
#endif
    if (!custom_tex && MainTex) {
        custom_tex = MainTex;
    }

    nptr<const RenderTexture> main_tex_source = custom_tex ? custom_tex : _ctx->DummyTexture;
    FO_VERIFY_AND_THROW(main_tex_source, "Vulkan dummy texture is not created");
    auto main_tex = main_tex_source.dyn_cast<const Vulkan_Texture>();
    FO_VERIFY_AND_THROW(main_tex, "Vulkan main texture is not of the expected backend type");

    if (_needProjBuf && !ProjBuf.has_value()) {
        auto& proj_buf = ProjBuf = ProjBuffer();
        auto projection_matrix = proj_buf->ProjMatrix;
        auto projection_matrix_values = make_ptr(glm::value_ptr(_ctx->ProjMatrix));
        MemCopy(projection_matrix, projection_matrix_values, 16 * sizeof(float32_t));
    }

    if (_needMainTexBuf && !MainTexBuf.has_value()) {
        auto& main_tex_buf = MainTexBuf = MainTexBuffer();
        auto main_texture_size = main_tex_buf->MainTexSize;
        auto main_texture_size_data = main_tex->SizeData;
        MemCopy(main_texture_size, main_texture_size_data, 4 * sizeof(float32_t));
    }

    // Default-initialize every required-but-unset uniform: descriptor sets are fresh per draw,
    // so an unwritten binding is undefined and the shader reads garbage (e.g. random egg-cutout
    // alpha collapse in 2D_Default.fofx).
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
    if (vk_dbuf->CurrentVertexBuffer == VK_NULL_HANDLE) {
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
        if (Pipeline[pass][prim_index][blend_variant] != VK_NULL_HANDLE) {
            vkCmdBindPipeline(_ctx->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline[pass][prim_index][blend_variant]);
        }

        // Allocate descriptor sets for this draw call
        VkDescriptorSet texture_set = VK_NULL_HANDLE;
        VkDescriptorSet uniform_set = VK_NULL_HANDLE;

        VkDescriptorSetAllocateInfo alloc_info {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = _ctx->FrameDescriptorPool;
        alloc_info.descriptorSetCount = 1;

        if (_ctx->TextureDescriptorSetLayout != VK_NULL_HANDLE) {
            alloc_info.pSetLayouts = &_ctx->TextureDescriptorSetLayout;
            const auto vk_result = vkAllocateDescriptorSets(_ctx->Device, &alloc_info, &texture_set);
            VerifyVkResult(vk_result);
        }

        if (_ctx->UniformDescriptorSetLayout != VK_NULL_HANDLE) {
            alloc_info.pSetLayouts = &_ctx->UniformDescriptorSetLayout;
            const auto vk_result = vkAllocateDescriptorSets(_ctx->Device, &alloc_info, &uniform_set);
            VerifyVkResult(vk_result);
        }

        // Update and bind per-pass texture descriptor set (set = 1)
        if (texture_set != VK_NULL_HANDLE && this->PipelineLayout != VK_NULL_HANDLE) {
            VkDescriptorImageInfo image_infos[16];
            VkWriteDescriptorSet writes[16];
            size_t write_count = 0;

            const auto append_sampler = [&](int32_t binding, ptr<const Vulkan_Texture> tex) {
                if (binding < 0 || tex->TextureImageView == VK_NULL_HANDLE) {
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
                nptr<const RenderTexture> indoor_tex_source = IndoorMaskTex ? IndoorMaskTex : _ctx->DummyTexture;
                FO_VERIFY_AND_THROW(indoor_tex_source, "Vulkan dummy texture is not created");
                auto indoor_tex = indoor_tex_source.dyn_cast<const Vulkan_Texture>();
                FO_VERIFY_AND_THROW(indoor_tex, "Vulkan indoor mask texture is not of the expected backend type");
                append_sampler(_posIndoorMaskTex[pass], indoor_tex);
            }

#if FO_ENABLE_3D
            for (size_t model_tex_index = 0; model_tex_index < MODEL_MAX_TEXTURES; model_tex_index++) {
                nptr<const RenderTexture> model_tex_source = ModelTex[model_tex_index] ? ModelTex[model_tex_index] : _ctx->DummyTexture;
                FO_VERIFY_AND_THROW(model_tex_source, "Vulkan dummy texture is not created");
                auto model_tex = model_tex_source.dyn_cast<const Vulkan_Texture>();
                FO_VERIFY_AND_THROW(model_tex, "Vulkan model texture is not of the expected backend type");
                append_sampler(_posModelTex[pass][model_tex_index], model_tex);
            }
#endif

            if (write_count > 0) {
                vkUpdateDescriptorSets(_ctx->Device, numeric_cast<uint32_t>(write_count), writes, 0, nullptr);
            }

            vkCmdBindDescriptorSets(_ctx->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, this->PipelineLayout, 1, 1, &texture_set, 0, nullptr);
        }

        // Update and bind per-pass uniform descriptor set (set = 0)
        if (uniform_set != VK_NULL_HANDLE && this->PipelineLayout != VK_NULL_HANDLE) {
            VkDescriptorBufferInfo buffer_infos[16];
            VkWriteDescriptorSet writes[16];
            size_t write_count = 0;

            const size_t alignment = numeric_cast<size_t>(_ctx->MinUniformBufferOffsetAlignment);

            const auto upload_uniform_buffer = [&](int32_t binding, const_span<uint8_t> src_data) {
                if (binding < 0 || src_data.empty()) {
                    return;
                }

                FO_VERIFY_AND_THROW(write_count < 16, "Too many Vulkan descriptor writes");

                const size_t src_size = src_data.size();

                // Align offset
                _ctx->FrameUniformOffset = (_ctx->FrameUniformOffset + alignment - 1) & ~(alignment - 1);
                FO_VERIFY_AND_THROW(_ctx->FrameUniformOffset + src_size <= _ctx->FrameUniformBufferSize, "Frame uniform buffer overflow");

                // Copy data
                FO_VERIFY_AND_THROW(_ctx->FrameUniformBufferMapped, "Mapped memory data pointer is null");
                auto mapped_bytes = _ctx->FrameUniformBufferMapped.reinterpret_as<uint8_t>();
                MemCopy(mapped_bytes.offset(_ctx->FrameUniformOffset), src_data.data(), src_size);

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
                upload_uniform_buffer(_posProjBuf[pass], object_to_bytes(ProjBuf.value()));
            }
            if (MainTexBuf.has_value()) {
                upload_uniform_buffer(_posMainTexBuf[pass], object_to_bytes(MainTexBuf.value()));
            }
            if (EggBuf.has_value()) {
                upload_uniform_buffer(_posEggBuf[pass], object_to_bytes(EggBuf.value()));
            }
            if (SpriteBorderBuf.has_value()) {
                upload_uniform_buffer(_posSpriteBorderBuf[pass], object_to_bytes(SpriteBorderBuf.value()));
            }
            if (TimeBuf.has_value()) {
                upload_uniform_buffer(_posTimeBuf[pass], object_to_bytes(TimeBuf.value()));
            }
            if (RandomValueBuf.has_value()) {
                upload_uniform_buffer(_posRandomValueBuf[pass], object_to_bytes(RandomValueBuf.value()));
            }
            if (ScriptValueBuf.has_value()) {
                upload_uniform_buffer(_posScriptValueBuf[pass], object_to_bytes(ScriptValueBuf.value()));
            }
            if (CameraBuf.has_value()) {
                upload_uniform_buffer(_posCameraBuf[pass], object_to_bytes(CameraBuf.value()));
            }
#if FO_ENABLE_3D
            if (ModelBuf.has_value()) {
                upload_uniform_buffer(_posModelBuf[pass], object_to_bytes(ModelBuf.value()));
            }
            if (ModelTexBuf.has_value()) {
                upload_uniform_buffer(_posModelTexBuf[pass], object_to_bytes(ModelTexBuf.value()));
            }
            if (ModelAnimBuf.has_value()) {
                upload_uniform_buffer(_posModelAnimBuf[pass], object_to_bytes(ModelAnimBuf.value()));
            }
#endif

            if (write_count > 0) {
                vkUpdateDescriptorSets(_ctx->Device, numeric_cast<uint32_t>(write_count), writes, 0, nullptr);
                vkCmdBindDescriptorSets(_ctx->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, this->PipelineLayout, 0, 1, &uniform_set, 0, nullptr);
            }
        }

        // Draw indexed or non-indexed
        if (vk_dbuf->IndCount != 0 && vk_dbuf->CurrentIndexBuffer != VK_NULL_HANDLE) {
            // Bind index buffer and draw indexed
            // ReSharper disable once CppUnreachableCode
            constexpr auto index_type = sizeof(vindex_t) == sizeof(uint32_t) ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
            vkCmdBindIndexBuffer(_ctx->CommandBuffer, vk_dbuf->CurrentIndexBuffer, 0, index_type);

            const size_t num_indices = indices_to_draw.value_or(vk_dbuf->IndCount);
            vkCmdDrawIndexed(_ctx->CommandBuffer, numeric_cast<uint32_t>(num_indices), 1, numeric_cast<uint32_t>(start_index), 0, 0);
        }
        else if (vk_dbuf->VertCount != 0) {
            // start_index is an index-buffer offset, meaningless without an index buffer; the
            // engine never emits a vertices-only draw with a non-zero offset
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

    if (_ctx->Device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(_ctx->Device);
        FlushAllDeferredDestroyQueues(_ctx);

        // Release the dummy texture now: its handles go through the deferred-destroy queues,
        // which must flush against a live VkDevice
        _ctx->DummyTexture.reset();
        FlushAllDeferredDestroyQueues(_ctx);

        // Staging rings route through the same deferred-destroy queues
        for (auto& slot : _ctx->FrameSlots) {
            DestroyPooledBuffers(_ctx, slot.TextureStagingRing);
        }
        FlushAllDeferredDestroyQueues(_ctx);
    }

    for (VkFramebuffer fb : _ctx->Framebuffers) {
        vkDestroyFramebuffer(_ctx->Device, fb, nullptr);
    }

    _ctx->Framebuffers.clear();

    for (VkImageView iv : _ctx->SwapchainImageViews) {
        vkDestroyImageView(_ctx->Device, iv, nullptr);
    }

    _ctx->SwapchainImageViews.clear();

    if (_ctx->SwapchainDepthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(_ctx->Device, _ctx->SwapchainDepthImageView, nullptr);
        _ctx->SwapchainDepthImageView = VK_NULL_HANDLE;
    }
    if (_ctx->SwapchainDepthImage != VK_NULL_HANDLE) {
        vkDestroyImage(_ctx->Device, _ctx->SwapchainDepthImage, nullptr);
        _ctx->SwapchainDepthImage = VK_NULL_HANDLE;
    }
    if (_ctx->SwapchainDepthImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(_ctx->Device, _ctx->SwapchainDepthImageMemory, nullptr);
        _ctx->SwapchainDepthImageMemory = VK_NULL_HANDLE;
    }

    if (_ctx->RenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(_ctx->Device, _ctx->RenderPass, nullptr);
        _ctx->RenderPass = VK_NULL_HANDLE;
    }

    if (_ctx->SwapchainDepthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(_ctx->Device, _ctx->SwapchainDepthImageView, nullptr);
        _ctx->SwapchainDepthImageView = VK_NULL_HANDLE;
    }
    if (_ctx->SwapchainDepthImage != VK_NULL_HANDLE) {
        vkDestroyImage(_ctx->Device, _ctx->SwapchainDepthImage, nullptr);
        _ctx->SwapchainDepthImage = VK_NULL_HANDLE;
    }
    if (_ctx->SwapchainDepthImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(_ctx->Device, _ctx->SwapchainDepthImageMemory, nullptr);
        _ctx->SwapchainDepthImageMemory = VK_NULL_HANDLE;
    }

    if (_ctx->CommandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(_ctx->Device, _ctx->CommandPool, nullptr);
        _ctx->CommandPool = VK_NULL_HANDLE;
    }

    for (auto& slot : _ctx->FrameSlots) {
        if (slot.ImageAvailableSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(_ctx->Device, slot.ImageAvailableSemaphore, nullptr);
            slot.ImageAvailableSemaphore = VK_NULL_HANDLE;
        }
        if (slot.InFlightFence != VK_NULL_HANDLE) {
            vkDestroyFence(_ctx->Device, slot.InFlightFence, nullptr);
            slot.InFlightFence = VK_NULL_HANDLE;
        }
        if (slot.DescriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(_ctx->Device, slot.DescriptorPool, nullptr);
            slot.DescriptorPool = VK_NULL_HANDLE;
        }
        if (slot.UniformBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(_ctx->Device, slot.UniformBuffer, nullptr);
            slot.UniformBuffer = VK_NULL_HANDLE;
        }
        if (slot.UniformBufferMemory != VK_NULL_HANDLE) {
            vkUnmapMemory(_ctx->Device, slot.UniformBufferMemory);
            vkFreeMemory(_ctx->Device, slot.UniformBufferMemory, nullptr);
            slot.UniformBufferMemory = VK_NULL_HANDLE;
            slot.UniformBufferMapped = nullptr;
        }
    }

    _ctx->CommandBuffer = VK_NULL_HANDLE;
    _ctx->ImageAvailableSemaphore = VK_NULL_HANDLE;
    _ctx->FrameDescriptorPool = VK_NULL_HANDLE;
    _ctx->FrameUniformBuffer = VK_NULL_HANDLE;
    _ctx->FrameUniformBufferMapped = nullptr;

    for (auto& semaphore : _ctx->RenderCompleteSemaphores) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(_ctx->Device, semaphore, nullptr);
        }
    }
    _ctx->RenderCompleteSemaphores.clear();

    if (_ctx->Swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(_ctx->Device, _ctx->Swapchain, nullptr);
        _ctx->Swapchain = VK_NULL_HANDLE;
    }
    if (_ctx->TextureDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(_ctx->Device, _ctx->TextureDescriptorSetLayout, nullptr);
        _ctx->TextureDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (_ctx->UniformDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(_ctx->Device, _ctx->UniformDescriptorSetLayout, nullptr);
        _ctx->UniformDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (_ctx->LinearSampler != VK_NULL_HANDLE) {
        vkDestroySampler(_ctx->Device, _ctx->LinearSampler, nullptr);
        _ctx->LinearSampler = VK_NULL_HANDLE;
    }
    if (_ctx->PointSampler != VK_NULL_HANDLE) {
        vkDestroySampler(_ctx->Device, _ctx->PointSampler, nullptr);
        _ctx->PointSampler = VK_NULL_HANDLE;
    }
    if (_ctx->Device != VK_NULL_HANDLE) {
        vkDestroyDevice(_ctx->Device, nullptr);
        _ctx->Device = VK_NULL_HANDLE;
    }
    if (_ctx->Surface != VK_NULL_HANDLE && _ctx->Instance != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(_ctx->Instance, _ctx->Surface, nullptr);
        _ctx->Surface = VK_NULL_HANDLE;
    }
    if (_ctx->DebugMessenger != VK_NULL_HANDLE && _ctx->Instance != VK_NULL_HANDLE) {
        if (auto vkDestroyDebugUtilsMessengerEXT_fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(_ctx->Instance, "vkDestroyDebugUtilsMessengerEXT")); vkDestroyDebugUtilsMessengerEXT_fn != nullptr) {
            vkDestroyDebugUtilsMessengerEXT_fn(_ctx->Instance, _ctx->DebugMessenger, nullptr);
        }
        _ctx->DebugMessenger = VK_NULL_HANDLE;
    }
    if (_ctx->Instance != VK_NULL_HANDLE) {
        vkDestroyInstance(_ctx->Instance, nullptr);
        _ctx->Instance = VK_NULL_HANDLE;
    }

    // Balance the SDL_Vulkan_LoadLibrary from Init (SDL ref-counts the loader).
    SDL_Vulkan_UnloadLibrary();

    _ctx.reset();
}

auto Vulkan_Renderer::CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_ctx, "Context is null");
    auto tex = SafeAlloc::MakeUnique<Vulkan_Texture>(size, linear_filtered, with_depth, _ctx);

    return std::move(tex);
}

auto Vulkan_Renderer::CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_ctx, "Context is null");
    auto dbuf = SafeAlloc::MakeUnique<Vulkan_DrawBuffer>(is_static, _ctx);

    return std::move(dbuf);
}

auto Vulkan_Renderer::CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_ctx, "Context is null");
    auto vk_effect = SafeAlloc::MakeUnique<Vulkan_Effect>(usage, name, loader, _ctx);

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

            if (vkCreateShaderModule(_ctx->Device, &module_ci, nullptr, &vk_effect->VertexShaderModule[pass]) != VK_SUCCESS) {
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

            if (vkCreateShaderModule(_ctx->Device, &module_ci, nullptr, &vk_effect->FragmentShaderModule[pass]) != VK_SUCCESS) {
                throw EffectLoadException("Failed to create fragment shader module", frag_fname);
            }
        }
    }

    // Create pipeline layout for this effect
    VkPipelineLayoutCreateInfo layout_ci {};
    layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    VkDescriptorSetLayout set_layouts[] = {_ctx->UniformDescriptorSetLayout, _ctx->TextureDescriptorSetLayout};
    layout_ci.setLayoutCount = 2;
    layout_ci.pSetLayouts = set_layouts;

    if (vkCreatePipelineLayout(_ctx->Device, &layout_ci, nullptr, &vk_effect->PipelineLayout) != VK_SUCCESS) {
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
            // POINT_LIST requires gl_PointSize, which spirv-cross cannot emit for our shaders;
            // point primitives are unused by content, so map to TRIANGLE_LIST
            input_assembly_ci.topology = prim_type == RenderPrimitiveType::PointList ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : ConvertPrimitive(prim_type);
            input_assembly_ci.primitiveRestartEnable = prim_type == RenderPrimitiveType::LineStrip || prim_type == RenderPrimitiveType::TriangleStrip ? VK_TRUE : VK_FALSE;

            // Two variants per (pass, prim): 0 = alpha-blend, 1 = blending disabled (see Pipeline)
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
                pipeline_ci.renderPass = _ctx->RenderPass;
                pipeline_ci.subpass = 0;

                if (vkCreateGraphicsPipelines(_ctx->Device, VK_NULL_HANDLE, 1, &pipeline_ci, nullptr, &vk_effect->Pipeline[pass][prim][blend_variant]) != VK_SUCCESS) {
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

    FO_VERIFY_AND_THROW(_ctx, "Context is null");
    return _ctx->ViewPort;
}

void Vulkan_Renderer::Init(GlobalSettings& settings, nptr<WindowInternalHandle> window)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(window, "Frontend window handle is null");
    FO_VERIFY_AND_THROW(!_ctx, "Frontend context is already initialized");
    _ctx = SafeAlloc::MakeUnique<Context>(&settings, window.reinterpret_as<SDL_Window>());
    FO_VERIFY_AND_THROW(_ctx, "Context is null");

    WriteLog("Used Vulkan rendering");

    WriteLog("[VkInit] FO_DEBUG={} settings.RenderDebug={}", FO_DEBUG, settings.RenderDebug ? "Y" : "n");

    // Load the Vulkan loader through SDL (dynamic, at selection time) instead of a link-time
    // vulkan-1.dll import, then bootstrap the entry-point table from it.
    if (!SDL_Vulkan_LoadLibrary(nullptr)) {
        throw RenderingException("SDL_Vulkan_LoadLibrary failed", SDL_GetError());
    }

    vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_Vulkan_GetVkGetInstanceProcAddr());
    if (vkGetInstanceProcAddr == nullptr) {
        throw RenderingException("SDL_Vulkan_GetVkGetInstanceProcAddr returned null", SDL_GetError());
    }

    LoadVulkanGlobalFunctions();

    Uint32 ext_count = 0;
    const char* const* sdl_exts = SDL_Vulkan_GetInstanceExtensions(&ext_count);

    if (sdl_exts == nullptr) {
        throw RenderingException("Failed to query Vulkan extensions from SDL");
    }

    // VK_EXT_debug_utils routes validation messages into our log; opt-in via Render.RenderDebug
    vector<ptr<const char>> exts_list;
    exts_list.reserve(ext_count + 1);

    for (uint32_t i = 0; i < ext_count; i++) {
        exts_list.emplace_back(sdl_exts[i]);
    }

    const bool want_validation = settings.RenderDebug || FO_DEBUG;

    if (want_validation) {
        exts_list.emplace_back("VK_EXT_debug_utils");
    }

    vector<const char*> raw_exts_list;
    raw_exts_list.reserve(exts_list.size());

    for (ptr<const char> ext : exts_list) {
        raw_exts_list.emplace_back(ext.get());
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
    create_info.enabledExtensionCount = numeric_cast<uint32_t>(raw_exts_list.size());
    create_info.ppEnabledExtensionNames = raw_exts_list.data();

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

    vk_result = vkCreateInstance(&create_info, nullptr, &_ctx->Instance);

    if (vk_result != VK_SUCCESS) {
        throw RenderingException("vkCreateInstance failed", vk_result);
    }

    // Resolve the remaining entry points now that an instance exists.
    LoadVulkanInstanceFunctions(_ctx->Instance);

    if (want_validation) {
        // Route layer warnings/errors into the log under [VkLayer]
        if (auto vkCreateDebugUtilsMessengerEXT_fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(_ctx->Instance, "vkCreateDebugUtilsMessengerEXT")); vkCreateDebugUtilsMessengerEXT_fn != nullptr) {
            VkDebugUtilsMessengerCreateInfoEXT msg_ci {};
            msg_ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            msg_ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            msg_ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            msg_ci.pfnUserCallback = VulkanDebugCallback;
            vk_result = vkCreateDebugUtilsMessengerEXT_fn(_ctx->Instance, &msg_ci, nullptr, &_ctx->DebugMessenger);

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

    if (!SDL_Vulkan_CreateSurface(_ctx->SdlWindow.get(), _ctx->Instance, nullptr, &_ctx->Surface)) {
        throw RenderingException("SDL_Vulkan_CreateSurface failed");
    }

    // Enumerate physical devices (selection happens below)
    uint32_t gpu_count = 0;
    vk_result = vkEnumeratePhysicalDevices(_ctx->Instance, &gpu_count, nullptr);
    VerifyVkResult(vk_result);

    if (gpu_count == 0) {
        throw RenderingException("No Vulkan physical devices found");
    }

    vector<VkPhysicalDevice> gpus;
    gpus.resize(gpu_count);
    vk_result = vkEnumeratePhysicalDevices(_ctx->Instance, &gpu_count, gpus.data());
    VerifyVkResult(vk_result);

    // Require a graphics+present queue family and swapchain support; prefer a discrete GPU
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

    for (VkPhysicalDevice gpu : gpus) {
        if (!has_swapchain_ext(gpu) || !has_graphics_present_family(gpu, _ctx->Surface)) {
            continue;
        }

        VkPhysicalDeviceProperties props {};
        vkGetPhysicalDeviceProperties(gpu, &props);
        const int32_t score = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 3 : props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 2 : props.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU ? 1 : 0;
        if (score > best_score) {
            best_score = score;
            _ctx->PhysicalDevice = gpu;
        }
    }

    if (_ctx->PhysicalDevice == VK_NULL_HANDLE) {
        throw RenderingException("No suitable Vulkan physical device (needs graphics+present queue and swapchain support)");
    }

    VkPhysicalDeviceProperties gpu_props {};
    vkGetPhysicalDeviceProperties(_ctx->PhysicalDevice, &gpu_props);

    // Cache immutable device properties once so the hot paths don't re-query them.
    _ctx->MinUniformBufferOffsetAlignment = gpu_props.limits.minUniformBufferOffsetAlignment;
    vkGetPhysicalDeviceMemoryProperties(_ctx->PhysicalDevice, &_ctx->MemoryProperties);

    const auto atlas_limit = numeric_cast<int32_t>(std::min(gpu_props.limits.maxImageDimension2D, numeric_cast<uint32_t>(AppRender::MAX_ATLAS_SIZE)));
    FO_VERIFY_AND_THROW(atlas_limit >= AppRender::MIN_ATLAS_SIZE, "Vulkan texture atlas size is below the required minimum", atlas_limit, AppRender::MIN_ATLAS_SIZE);
    AppRender::MAX_ATLAS_WIDTH = atlas_limit;
    AppRender::MAX_ATLAS_HEIGHT = atlas_limit;

    // Find queue family
    uint32_t qcount;
    vkGetPhysicalDeviceQueueFamilyProperties(_ctx->PhysicalDevice, &qcount, nullptr);
    vector<VkQueueFamilyProperties> qprops(qcount);
    qprops.resize(qcount);
    vkGetPhysicalDeviceQueueFamilyProperties(_ctx->PhysicalDevice, &qcount, qprops.data());

    optional<uint32_t> graphics_family;

    for (uint32_t i = 0; i < qcount; i++) {
        VkBool32 present_support = VK_FALSE;
        vk_result = vkGetPhysicalDeviceSurfaceSupportKHR(_ctx->PhysicalDevice, i, _ctx->Surface, &present_support);
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

    vk_result = vkCreateDevice(_ctx->PhysicalDevice, &dci, nullptr, &_ctx->Device);
    VerifyVkResult(vk_result);

    vkGetDeviceQueue(_ctx->Device, graphics_family.value(), 0, &_ctx->GraphicsQueue);
    _ctx->GraphicsFamilyIndex = graphics_family.value();

    // Create command pool for recording render commands
    VkCommandPoolCreateInfo cpi {};
    cpi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpi.queueFamilyIndex = _ctx->GraphicsFamilyIndex;
    vk_result = vkCreateCommandPool(_ctx->Device, &cpi, nullptr, &_ctx->CommandPool);
    VerifyVkResult(vk_result);

    // Initialize swapchain for current window size
    int width;
    int height;
    SDL_GetWindowSizeInPixels(_ctx->SdlWindow.get(), &width, &height);
    RecreateSwapchain(_ctx, {std::max(width, 1), std::max(height, 1)});
    ApplySwapchainTargetMetrics(_ctx, _ctx->SwapchainSize);

    // Allocate one command buffer per in-flight frame slot plus per-slot acquire semaphores and
    // in-flight fences (created signaled so the first wait on each slot passes immediately).
    {
        VkCommandBufferAllocateInfo cbai {};
        cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cbai.commandPool = _ctx->CommandPool;
        cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbai.commandBufferCount = 1;

        VkSemaphoreCreateInfo sem_ci {};
        sem_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_ci {};
        fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (auto& slot : _ctx->FrameSlots) {
            vk_result = vkAllocateCommandBuffers(_ctx->Device, &cbai, &slot.CommandBuffer);
            VerifyVkResult(vk_result);
            vk_result = vkCreateSemaphore(_ctx->Device, &sem_ci, nullptr, &slot.ImageAvailableSemaphore);
            VerifyVkResult(vk_result);
            vk_result = vkCreateFence(_ctx->Device, &fence_ci, nullptr, &slot.InFlightFence);
            VerifyVkResult(vk_result);
        }
    }

    // Allocate staging command buffer for buffer uploads
    VkCommandBufferAllocateInfo staging_cbai {};
    staging_cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    staging_cbai.commandPool = _ctx->CommandPool;
    staging_cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    staging_cbai.commandBufferCount = 1;

    vk_result = vkAllocateCommandBuffers(_ctx->Device, &staging_cbai, &_ctx->StagingCommandBuffer);
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

    vk_result = vkCreateDescriptorSetLayout(_ctx->Device, &layout_ci, nullptr, &_ctx->TextureDescriptorSetLayout);
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

    vk_result = vkCreateDescriptorSetLayout(_ctx->Device, &ubo_layout_ci, nullptr, &_ctx->UniformDescriptorSetLayout);
    VerifyVkResult(vk_result);

    // Create per-slot descriptor pools and uniform bump buffers. Each per-draw set consumes the
    // full fixed-width VULKAN_MAX_*_BINDINGS descriptor count; an undersized pool silently starves
    // the frame-final ImGui pass (UI vanishes), so provision per-type counts to cover maxSets.
    {
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

        for (auto& slot : _ctx->FrameSlots) {
            vk_result = vkCreateDescriptorPool(_ctx->Device, &frame_pool_ci, nullptr, &slot.DescriptorPool);
            VerifyVkResult(vk_result);

            AllocateBuffer(_ctx, _ctx->FrameUniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, slot.UniformBuffer, slot.UniformBufferMemory);
            vk_result = vkMapMemory(_ctx->Device, slot.UniformBufferMemory, 0, _ctx->FrameUniformBufferSize, 0, slot.UniformBufferMapped.get_pp());
            VerifyVkResult(vk_result);
        }
    }

    // Point the current-slot aliases at the initial slot; the first BeginFrame refreshes them.
    {
        auto& initial_slot = _ctx->CurrentFrameSlot();
        _ctx->CommandBuffer = initial_slot.CommandBuffer;
        _ctx->ImageAvailableSemaphore = initial_slot.ImageAvailableSemaphore;
        _ctx->FrameDescriptorPool = initial_slot.DescriptorPool;
        _ctx->FrameUniformBuffer = initial_slot.UniformBuffer;
        _ctx->FrameUniformBufferMapped = initial_slot.UniformBufferMapped;
    }

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

    vk_result = vkCreateSampler(_ctx->Device, &sampler_ci, nullptr, &_ctx->LinearSampler);
    VerifyVkResult(vk_result);

    sampler_ci.magFilter = VK_FILTER_NEAREST;
    sampler_ci.minFilter = VK_FILTER_NEAREST;
    sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    vk_result = vkCreateSampler(_ctx->Device, &sampler_ci, nullptr, &_ctx->PointSampler);
    VerifyVkResult(vk_result);

    constexpr ucolor dummy_pixel[1] = {ucolor {255, 0, 255, 255}};
    _ctx->DummyTexture = CreateTexture({1, 1}, false, false);
    _ctx->DummyTexture->UpdateTextureRegion({}, {1, 1}, dummy_pixel);

    // Begin first frame and set default render target (matches OpenGL/D3D init flow)
    BeginFrame(_ctx);
    SetRenderTarget(nullptr);
}

static void RecreateSwapchain(ptr<Vulkan_Renderer::Context> ctx, isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(size.width > 0, "Swapchain size must be positive");
    FO_VERIFY_AND_THROW(size.height > 0, "Swapchain size must be positive");

    VkResult vk_result = VK_SUCCESS;

    // Destroy old framebuffers and image views
    for (VkFramebuffer fb : ctx->Framebuffers) {
        DestroyFramebufferSafe(ctx, fb);
    }

    ctx->Framebuffers.clear();

    for (VkImageView iv : ctx->SwapchainImageViews) {
        DestroyImageViewSafe(ctx, iv);
    }

    ctx->SwapchainImageViews.clear();

    if (ctx->Swapchain != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(ctx->Device);
        vkDestroySwapchainKHR(ctx->Device, ctx->Swapchain, nullptr);
        ctx->Swapchain = VK_NULL_HANDLE;
        ctx->SwapchainImages.clear();
        ctx->SwapchainImageLayouts.clear();
    }

    DestroyImageViewSafe(ctx, ctx->SwapchainDepthImageView);
    DestroyImageSafe(ctx, ctx->SwapchainDepthImage);
    DestroyMemorySafe(ctx, ctx->SwapchainDepthImageMemory);

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

    // The backend assumes VK_FORMAT_B8G8R8A8_UNORM everywhere (one render pass shared by all
    // framebuffers), so require it rather than silently substituting an incompatible format.
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

    // FIFO is vsync-locked and the only guaranteed mode; with VSync off prefer IMMEDIATE, then MAILBOX
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

    // (Re)create the per-image render-complete semaphores to match the image count
    {
        VkSemaphoreCreateInfo sem_ci {};
        sem_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        for (auto& semaphore : ctx->RenderCompleteSemaphores) {
            if (semaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(ctx->Device, semaphore, nullptr);
            }
        }

        ctx->RenderCompleteSemaphores.assign(ctx->SwapchainImages.size(), VK_NULL_HANDLE);

        for (auto& semaphore : ctx->RenderCompleteSemaphores) {
            vk_result = vkCreateSemaphore(ctx->Device, &sem_ci, nullptr, &semaphore);
            VerifyVkResult(vk_result);
        }
    }

    ctx->SwapchainFormat = sci.imageFormat;

    if (ctx->RenderPass == VK_NULL_HANDLE) {
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

        // Explicit external dependencies: the pass LOADs prior color and re-clears depth stored by
        // the previous pass instance; implicit dependencies carry no memory scope, and the races
        // manifest as render targets losing accumulated content between passes.
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

static void RecreateFrameSyncObjects(ptr<Vulkan_Renderer::Context> ctx)
{
    FO_STACK_TRACE_ENTRY();

    // Requires an idle device. Recreating semaphores clears stale signals left by a failed
    // present/deferred resize; fences are recreated signaled (caller re-resets its slot's fence).
    VkResult vk_result = VK_SUCCESS;

    VkSemaphoreCreateInfo sem_ci {};
    sem_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_ci {};
    fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (auto& slot : ctx->FrameSlots) {
        if (slot.ImageAvailableSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(ctx->Device, slot.ImageAvailableSemaphore, nullptr);
            slot.ImageAvailableSemaphore = VK_NULL_HANDLE;
        }
        if (slot.InFlightFence != VK_NULL_HANDLE) {
            vkDestroyFence(ctx->Device, slot.InFlightFence, nullptr);
            slot.InFlightFence = VK_NULL_HANDLE;
        }

        vk_result = vkCreateSemaphore(ctx->Device, &sem_ci, nullptr, &slot.ImageAvailableSemaphore);
        VerifyVkResult(vk_result);
        vk_result = vkCreateFence(ctx->Device, &fence_ci, nullptr, &slot.InFlightFence);
        VerifyVkResult(vk_result);
    }

    for (auto& semaphore : ctx->RenderCompleteSemaphores) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(ctx->Device, semaphore, nullptr);
        }

        vk_result = vkCreateSemaphore(ctx->Device, &sem_ci, nullptr, &semaphore);
        VerifyVkResult(vk_result);
    }
}

static void BeginFrame(ptr<Vulkan_Renderer::Context> ctx)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(ctx->Swapchain, "Vulkan swapchain is not created");
    FO_VERIFY_AND_THROW(ctx->Device, "Vulkan device is not initialized");

    VkResult vk_result = VK_SUCCESS;

    // Advance to the next frame slot and wait until the GPU released it (CPU records frame N
    // while the GPU still renders frame N-1)
    ctx->FrameIndex++;
    auto& frame_slot = ctx->CurrentFrameSlot();

    vk_result = vkWaitForFences(ctx->Device, 1, &frame_slot.InFlightFence, VK_TRUE, UINT64_MAX);
    VerifyVkResult(vk_result);
    vk_result = vkResetFences(ctx->Device, 1, &frame_slot.InFlightFence);
    VerifyVkResult(vk_result);

    // The fence wait made everything this slot owns GPU-free
    FlushDeferredDestroyQueue(ctx, frame_slot.DestroyQueue);

    if (ctx->PendingSwapchainRecreateSize.has_value()) {
        const auto recreate_size = ctx->PendingSwapchainRecreateSize.value();
        ctx->PendingSwapchainRecreateSize.reset();
        // Settle the device, drop pending destroys and rebuild sync objects so nothing stale
        // survives the recreate
        vk_result = vkDeviceWaitIdle(ctx->Device);
        VerifyVkResult(vk_result);
        FlushAllDeferredDestroyQueues(ctx);
        RecreateSwapchain(ctx, {std::max(recreate_size.width, 1), std::max(recreate_size.height, 1)});
        RecreateFrameSyncObjects(ctx);

        // Fences were recreated signaled; this frame submits with the current slot's fence
        vk_result = vkResetFences(ctx->Device, 1, &frame_slot.InFlightFence);
        VerifyVkResult(vk_result);

        if (!ctx->CurrentRenderTarget) {
            ApplySwapchainTargetMetrics(ctx, ctx->SwapchainSize);
        }
    }

    vk_result = vkResetDescriptorPool(ctx->Device, frame_slot.DescriptorPool, 0);
    VerifyVkResult(vk_result);

    // Point the current-slot aliases at this frame's resources
    ctx->CommandBuffer = frame_slot.CommandBuffer;
    ctx->ImageAvailableSemaphore = frame_slot.ImageAvailableSemaphore;
    ctx->FrameDescriptorPool = frame_slot.DescriptorPool;
    ctx->FrameUniformBuffer = frame_slot.UniformBuffer;
    ctx->FrameUniformBufferMapped = frame_slot.UniformBufferMapped;
    ctx->FrameUniformOffset = 0;

    vk_result = vkAcquireNextImageKHR(ctx->Device, ctx->Swapchain, UINT64_MAX, ctx->ImageAvailableSemaphore, VK_NULL_HANDLE, &ctx->CurrentSwapchainImageIndex);

    if (vk_result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Acquire failed with the semaphore unsignaled: rebuild everything and re-acquire
        int width;
        int height;
        SDL_GetWindowSizeInPixels(ctx->SdlWindow.get(), &width, &height);
        vk_result = vkDeviceWaitIdle(ctx->Device);
        VerifyVkResult(vk_result);
        FlushAllDeferredDestroyQueues(ctx);
        RecreateSwapchain(ctx, {std::max(width, 1), std::max(height, 1)});
        RecreateFrameSyncObjects(ctx);
        ctx->CommandBuffer = frame_slot.CommandBuffer;
        ctx->ImageAvailableSemaphore = frame_slot.ImageAvailableSemaphore;
        vk_result = vkResetFences(ctx->Device, 1, &frame_slot.InFlightFence);
        VerifyVkResult(vk_result);

        vk_result = vkAcquireNextImageKHR(ctx->Device, ctx->Swapchain, UINT64_MAX, ctx->ImageAvailableSemaphore, VK_NULL_HANDLE, &ctx->CurrentSwapchainImageIndex);
        VerifyVkResult(vk_result);
    }
    else if (vk_result == VK_SUBOPTIMAL_KHR) {
        // SUBOPTIMAL is a success: the image was acquired and the semaphore signaled, so render
        // this frame as-is and defer the recreate (re-acquiring on a signaled semaphore is illegal)
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

    ctx->FrameCbRecording = false;

    // Wait the acquire semaphore only on the frame's first submit (a mid-frame flush may have
    // consumed it), at the stages of the image's first use (TRANSFER clear + attachment writes);
    // signal the acquired image's render-complete semaphore and the slot's in-flight fence.
    FO_VERIFY_AND_THROW(ctx->CurrentSwapchainImageIndex < ctx->RenderCompleteSemaphores.size(), "Swapchain image index out of render-complete semaphore range");
    VkSemaphore render_complete_semaphore = ctx->RenderCompleteSemaphores[ctx->CurrentSwapchainImageIndex];

    constexpr VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &ctx->CommandBuffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &render_complete_semaphore;

    if (!ctx->ImageAvailableWaited) {
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &ctx->ImageAvailableSemaphore;
        submit_info.pWaitDstStageMask = &wait_stage;
        ctx->ImageAvailableWaited = true;
    }

    vk_result = vkQueueSubmit(ctx->GraphicsQueue, 1, &submit_info, ctx->CurrentFrameSlot().InFlightFence);
    VerifyVkResult(vk_result);

    // Present to screen
    VkPresentInfoKHR info {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &render_complete_semaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &ctx->Swapchain;
    info.pImageIndices = &ctx->CurrentSwapchainImageIndex;

    vk_result = vkQueuePresentKHR(ctx->GraphicsQueue, &info);

    if (vk_result == VK_ERROR_OUT_OF_DATE_KHR || vk_result == VK_SUBOPTIMAL_KHR) {
        // Defer the recreate to the next BeginFrame: a failed present can leave the semaphore in
        // an ambiguous state, and BeginFrame rebuilds all sync objects on an idle device
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
        // First use: chain after the acquire semaphore's wait scope (TRANSFER | ATTACHMENT_OUTPUT)
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
        // Chain after the acquire semaphore's wait scope so the transition cannot precede the acquire
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
        // READ is required in the destination scope: loadOp=LOAD reads in this stage
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
        auto vk_tex = ctx->CurrentRenderTarget.dyn_cast<Vulkan_Texture>();
        FO_VERIFY_AND_THROW(vk_tex, "Vulkan render target texture is not of the expected backend type");
        EnsureTextureRenderTargetResources(ctx, vk_tex);
        FO_VERIFY_AND_THROW(vk_tex->TextureFramebuffer != VK_NULL_HANDLE, "Render target framebuffer is not created");
        FO_VERIFY_AND_THROW(vk_tex->TextureImage != VK_NULL_HANDLE, "Render target image is not created");

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

        // Vulkan requires the scissor inside the framebuffer; clamp against the full render-target
        // extent (GL/D3D clamp implicitly)
        int32_t rt_w = ctx->SwapchainSize.width;
        int32_t rt_h = ctx->SwapchainSize.height;

        if (ctx->CurrentRenderTarget) {
            auto rt_tex = ctx->CurrentRenderTarget.dyn_cast<const Vulkan_Texture>();
            FO_VERIFY_AND_THROW(rt_tex, "Vulkan render target texture is not of the expected backend type");
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

    FO_VERIFY_AND_THROW(vk_tex->TextureImage != VK_NULL_HANDLE, "Render target image is not created");
    FO_VERIFY_AND_THROW(vk_tex->TextureImageView != VK_NULL_HANDLE, "Render target image view is not created");

    VkResult vk_result = VK_SUCCESS;

    if (vk_tex->DepthImage == VK_NULL_HANDLE) {
        // Build depth image + view into locals; on throw all three members stay null so the whole
        // block re-runs cleanly instead of leaving DepthImage set with a null DepthImageView behind.
        VkImage depth_image {};
        VkDeviceMemory depth_image_memory {};
        AllocateImage(ctx, numeric_cast<uint32_t>(vk_tex->Size.width), numeric_cast<uint32_t>(vk_tex->Size.height), VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depth_image, depth_image_memory);

        VkImageViewCreateInfo depth_view_ci {};
        depth_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depth_view_ci.image = depth_image;
        depth_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depth_view_ci.format = VK_FORMAT_D32_SFLOAT;
        depth_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depth_view_ci.subresourceRange.baseMipLevel = 0;
        depth_view_ci.subresourceRange.levelCount = 1;
        depth_view_ci.subresourceRange.baseArrayLayer = 0;
        depth_view_ci.subresourceRange.layerCount = 1;

        VkImageView depth_image_view {};
        vk_result = vkCreateImageView(ctx->Device, &depth_view_ci, nullptr, &depth_image_view);
        VerifyVkResult(vk_result);

        // No-throw commit tail.
        vk_tex->DepthImage = depth_image;
        vk_tex->DepthImageMemory = depth_image_memory;
        vk_tex->DepthImageView = depth_image_view;
    }

    if (vk_tex->TextureFramebuffer == VK_NULL_HANDLE) {
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

    FO_VERIFY_AND_THROW(_ctx, "Context is null");
    EndFrame(_ctx);
}

// Recomputes the letterboxed viewport, target size and projection for back-buffer rendering;
// called on back-buffer select and on size changes while it is active (the server host UI never
// calls SetRenderTarget, so resizes must refresh these here).
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

    FO_VERIFY_AND_THROW(_ctx, "Context is null");

    if (_ctx->CurrentRenderTarget == tex) {
        return;
    }

    // Resolve every fallible value (new-target validation + new viewport/target-size/projection) into
    // locals BEFORE closing the render pass, so a throw here leaves the current pass open and the frame
    // consistent. Only the no-throw member commit and the pass close/reopen touch pass state below.
    irect32 new_viewport {};
    isize32 new_target_size {};

    if (!tex) {
        // Back-buffer target: letterbox the configured screen size into the swapchain. Mirror of
        // ApplySwapchainTargetMetrics, computed into locals (its own ProjMatrix write is redundant here
        // because the projection is rebuilt below from the resolved target size).
        const isize32 back_buf_size = _ctx->SwapchainSize;
        const auto back_buf_aspect = checked_div<float32_t>(numeric_cast<float32_t>(back_buf_size.width), numeric_cast<float32_t>(back_buf_size.height));
        const auto screen_aspect = checked_div<float32_t>(numeric_cast<float32_t>(_ctx->Settings->ScreenWidth), numeric_cast<float32_t>(_ctx->Settings->ScreenHeight));
        const auto fit_width = iround<int32_t>(screen_aspect <= back_buf_aspect ? numeric_cast<float32_t>(back_buf_size.height) * screen_aspect : numeric_cast<float32_t>(back_buf_size.height) * back_buf_aspect);
        const auto fit_height = iround<int32_t>(screen_aspect <= back_buf_aspect ? numeric_cast<float32_t>(back_buf_size.width) / back_buf_aspect : numeric_cast<float32_t>(back_buf_size.width) / screen_aspect);
        const auto vp_ox = (back_buf_size.width - fit_width) / 2;
        const auto vp_oy = (back_buf_size.height - fit_height) / 2;
        new_viewport = irect32 {vp_ox, vp_oy, fit_width, fit_height};
        new_target_size = {_ctx->Settings->ScreenWidth, _ctx->Settings->ScreenHeight};
    }
    else {
        auto vk_tex = tex.dyn_cast<const Vulkan_Texture>();
        FO_VERIFY_AND_THROW(vk_tex, "Vulkan render target texture is not of the expected backend type");
        new_viewport = irect32 {0, 0, vk_tex->Size.width, vk_tex->Size.height};
        new_target_size = vk_tex->Size;
    }

    const auto new_proj_matrix = CreateOrthoMatrix(0.0f, numeric_cast<float32_t>(new_target_size.width), numeric_cast<float32_t>(new_target_size.height), 0.0f, _ctx->OrthoNear, _ctx->OrthoFar);

    EndCurrentRenderPass(_ctx);

    if (_ctx->CurrentRenderTarget) {
        auto prev_tex = _ctx->CurrentRenderTarget.dyn_cast<Vulkan_Texture>();
        FO_VERIFY_AND_THROW(prev_tex, "Vulkan render target texture is not of the expected backend type");
        TransitionColorImage(_ctx->CommandBuffer, prev_tex->TextureImage, prev_tex->TextureImageLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        prev_tex->TextureImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    // No-throw commit tail: switch the target and metrics, then reopen the pass.
    _ctx->CurrentRenderTarget = tex;
    _ctx->ViewPort = new_viewport;
    _ctx->TargetSize = new_target_size;
    _ctx->ProjMatrix = new_proj_matrix;

    BeginCurrentRenderPass(_ctx);
}

void Vulkan_Renderer::SetOrthoDepthRange(float32_t nearp, float32_t farp) noexcept
{
    FO_STACK_TRACE_ENTRY();

    _ctx->OrthoNear = nearp;
    _ctx->OrthoFar = farp;
    _ctx->ProjMatrix = CreateOrthoMatrix(0.0f, numeric_cast<float32_t>(_ctx->TargetSize.width), numeric_cast<float32_t>(_ctx->TargetSize.height), 0.0f, nearp, farp);
}

auto Vulkan_Renderer::GetProjMatrix() const -> mat44
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_ctx, "Context is null");
    return _ctx->ProjMatrix;
}

void Vulkan_Renderer::ClearRenderTarget(optional<ucolor> color, bool depth, bool stencil)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(stencil);

    if (!color.has_value() && !depth) {
        return;
    }

    FO_VERIFY_AND_THROW(_ctx, "Context is null");

    small_vector<VkClearAttachment, 2> attachments;
    attachments.reserve(2);

    if (color.has_value()) {
        _ctx->ClearColor.float32[0] = numeric_cast<float32_t>(color.value().comp.r) / 255.0f;
        _ctx->ClearColor.float32[1] = numeric_cast<float32_t>(color.value().comp.g) / 255.0f;
        _ctx->ClearColor.float32[2] = numeric_cast<float32_t>(color.value().comp.b) / 255.0f;
        _ctx->ClearColor.float32[3] = numeric_cast<float32_t>(color.value().comp.a) / 255.0f;

        VkClearAttachment color_attachment {};
        color_attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        color_attachment.colorAttachment = 0;
        color_attachment.clearValue.color = _ctx->ClearColor;
        attachments.emplace_back(color_attachment);
    }

    if (depth) {
        VkClearAttachment depth_attachment {};
        depth_attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depth_attachment.clearValue.depthStencil = {.depth = 1.0f, .stencil = 0};
        attachments.emplace_back(depth_attachment);
    }

    VkClearRect clear_rect {};
    clear_rect.rect.offset = {.x = numeric_cast<int32_t>(_ctx->ViewPort.x), .y = numeric_cast<int32_t>(_ctx->ViewPort.y)};
    clear_rect.rect.extent = {.width = numeric_cast<uint32_t>(_ctx->ViewPort.width), .height = numeric_cast<uint32_t>(_ctx->ViewPort.height)};
    clear_rect.baseArrayLayer = 0;
    clear_rect.layerCount = 1;

    vkCmdClearAttachments(_ctx->CommandBuffer, numeric_cast<uint32_t>(attachments.size()), attachments.data(), 1, &clear_rect);
}

void Vulkan_Renderer::EnableScissor(irect32 rect)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_ctx, "Context is null");
    _ctx->ScissorRect = rect;
    _ctx->ScissorEnabled = true;

    ApplyViewportAndScissor(_ctx);
}

void Vulkan_Renderer::DisableScissor()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_ctx, "Context is null");
    _ctx->ScissorEnabled = false;

    ApplyViewportAndScissor(_ctx);
}

void Vulkan_Renderer::OnResizeWindow(isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_ctx, "Context is null");
    const auto clamped_size = isize32 {std::max(size.width, 1), std::max(size.height, 1)};
    _ctx->PendingSwapchainRecreateSize = clamped_size;

    if (!_ctx->CurrentRenderTarget) {
        // The back buffer stays the active target across the resize (the server host UI never calls
        // SetRenderTarget), so refresh the letterbox/projection here against the new size.
        ApplySwapchainTargetMetrics(_ctx, clamped_size);
    }
    else {
        _ctx->ViewPort = irect32 {{0, 0}, clamped_size};
    }
}

// Destroys enqueue into the current slot's queue; its fence wait covers both in-flight frames.
static void DestroyBufferSafe(ptr<Vulkan_Renderer::Context> ctx, VkBuffer& buffer)
{
    FO_STACK_TRACE_ENTRY();

    if (buffer != VK_NULL_HANDLE) {
        ctx->CurrentFrameSlot().DestroyQueue.Buffers.emplace_back(buffer);
        buffer = VK_NULL_HANDLE;
    }
}

static void DestroyMemorySafe(ptr<Vulkan_Renderer::Context> ctx, VkDeviceMemory& memory)
{
    FO_STACK_TRACE_ENTRY();

    if (memory != VK_NULL_HANDLE) {
        ctx->CurrentFrameSlot().DestroyQueue.Memories.emplace_back(memory);
        memory = VK_NULL_HANDLE;
    }
}

static void DestroyImageSafe(ptr<Vulkan_Renderer::Context> ctx, VkImage& image)
{
    FO_STACK_TRACE_ENTRY();

    if (image != VK_NULL_HANDLE) {
        ctx->CurrentFrameSlot().DestroyQueue.Images.emplace_back(image);
        image = VK_NULL_HANDLE;
    }
}

static void DestroyImageViewSafe(ptr<Vulkan_Renderer::Context> ctx, VkImageView& image_view)
{
    FO_STACK_TRACE_ENTRY();

    if (image_view != VK_NULL_HANDLE) {
        ctx->CurrentFrameSlot().DestroyQueue.ImageViews.emplace_back(image_view);
        image_view = VK_NULL_HANDLE;
    }
}

static void DestroyFramebufferSafe(ptr<Vulkan_Renderer::Context> ctx, VkFramebuffer& framebuffer)
{
    FO_STACK_TRACE_ENTRY();

    if (framebuffer != VK_NULL_HANDLE) {
        ctx->CurrentFrameSlot().DestroyQueue.Framebuffers.emplace_back(framebuffer);
        framebuffer = VK_NULL_HANDLE;
    }
}

static void FlushDeferredDestroyQueue(ptr<Vulkan_Renderer::Context> ctx, VulkanDeferredDestroyQueue& queue)
{
    FO_STACK_TRACE_ENTRY();

    for (VkFramebuffer framebuffer : queue.Framebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(ctx->Device, framebuffer, nullptr);
        }
    }

    queue.Framebuffers.clear();

    for (VkImageView image_view : queue.ImageViews) {
        if (image_view != VK_NULL_HANDLE) {
            vkDestroyImageView(ctx->Device, image_view, nullptr);
        }
    }

    queue.ImageViews.clear();

    for (VkImage image : queue.Images) {
        if (image != VK_NULL_HANDLE) {
            vkDestroyImage(ctx->Device, image, nullptr);
        }
    }

    queue.Images.clear();

    for (VkBuffer buffer : queue.Buffers) {
        if (buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(ctx->Device, buffer, nullptr);
        }
    }

    queue.Buffers.clear();

    for (VkDeviceMemory memory : queue.Memories) {
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(ctx->Device, memory, nullptr);
        }
    }

    queue.Memories.clear();
}

// Flush every slot's queue; the caller must have proven the device idle (vkDeviceWaitIdle).
static void FlushAllDeferredDestroyQueues(ptr<Vulkan_Renderer::Context> ctx)
{
    FO_STACK_TRACE_ENTRY();

    for (auto& slot : ctx->FrameSlots) {
        FlushDeferredDestroyQueue(ctx, slot.DestroyQueue);
    }
}

FO_END_NAMESPACE

#endif // FO_HAVE_VULKAN
