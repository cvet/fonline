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

struct Vulkan_Renderer::Context
{
    raw_ptr<GlobalSettings> Settings {};
    raw_ptr<SDL_Window> SdlWindow {};
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
    mat44 ProjectionMatrixColMaj {};
    unique_ptr<RenderTexture> DummyTexture {};
    VkClearColorValue ClearColor {{0.0f, 0.0f, 0.0f, 1.0f}};
    VkCommandBuffer StagingCommandBuffer {};
    vector<tuple<VkBuffer, VkDeviceMemory>> StagingBuffers {};
    VkDescriptorSetLayout TextureDescriptorSetLayout {};
    VkDescriptorSetLayout UniformDescriptorSetLayout {};
    VkDescriptorPool FrameDescriptorPool {};
    VkBuffer FrameUniformBuffer {};
    VkDeviceMemory FrameUniformBufferMemory {};
    void* FrameUniformBufferMapped {};
    size_t FrameUniformOffset {};
    size_t FrameUniformBufferSize {numeric_cast<size_t>(16 * 1024 * 1024)}; // 16 MB
    VkSampler LinearSampler {};
    VkSampler PointSampler {};
    raw_ptr<RenderTexture> CurrentRenderTarget {}; // Current render target (nullptr = swapchain)
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
};

static raw_ptr<Vulkan_Renderer::Context> Ctx {};

class Vulkan_Texture;

static void RecreateSwapchain(isize32 size);
static void RecreateFrameSyncSemaphores();
static void AllocateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory);
static void AllocateImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& memory);
static void BeginFrame();
static void EndFrame();
static void TransitionColorImage(VkCommandBuffer cmd_buf, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout);
static void BeginCurrentRenderPass();
static void EndCurrentRenderPass();
static void ApplyViewportAndScissor();
static void EnsureTextureRenderTargetResources(Vulkan_Texture* vk_tex);
static void DestroyResourceSafe(VkBuffer& buffer);
static void DestroyResourceSafe(VkDeviceMemory& memory);
static void DestroyResourceSafe(VkImage& image);
static void DestroyResourceSafe(VkImageView& image_view);
static void DestroyResourceSafe(VkFramebuffer& framebuffer);
static void FlushDeferredDestroyResources();
static void BeginCommandBufferRecording(VkCommandBuffer cmd_buf);
static void EndCommandBufferRecording(VkCommandBuffer cmd_buf);
static void SubmitCommandBufferAndWait(VkCommandBuffer cmd_buf);
static void ResetCommandBufferRecording(VkCommandBuffer cmd_buf);

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

static void SubmitCommandBufferAndWait(VkCommandBuffer cmd_buf)
{
    FO_STACK_TRACE_ENTRY();

    VkSubmitInfo submit_info {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buf;

    auto vk_result = vkQueueSubmit(Ctx->GraphicsQueue, 1, &submit_info, VK_NULL_HANDLE);
    VerifyVkResult(vk_result);

    vk_result = vkQueueWaitIdle(Ctx->GraphicsQueue);
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
    Vulkan_Texture(isize32 size, bool linear_filtered, bool with_depth);
    ~Vulkan_Texture() override;

    [[nodiscard]] auto GetTexturePixel(ipos32 pos) const -> ucolor override;
    [[nodiscard]] auto GetTextureRegion(ipos32 pos, isize32 size) const -> vector<ucolor> override;

    void UpdateTextureRegion(ipos32 pos, isize32 size, const ucolor* data, bool use_dest_pitch) override;

    VkImage TextureImage {};
    VkImageView TextureImageView {};
    VkDeviceMemory TextureImageMemory {};
    VkDescriptorSet DescriptorSet {};
    VkFramebuffer TextureFramebuffer {};
    VkImageLayout TextureImageLayout {VK_IMAGE_LAYOUT_UNDEFINED};
    VkImage DepthImage {};
    VkImageView DepthImageView {};
    VkDeviceMemory DepthImageMemory {};
};

class Vulkan_DrawBuffer final : public RenderDrawBuffer
{
    friend class Vulkan_Effect;
    friend class Vulkan_Renderer;

public:
    explicit Vulkan_DrawBuffer(bool is_static) :
        RenderDrawBuffer(is_static)
    {
        FO_STACK_TRACE_ENTRY();
    }
    ~Vulkan_DrawBuffer() override;

    void Upload(EffectUsage usage, optional<size_t> custom_vertices_size, optional<size_t> custom_indices_size) override;

    VkBuffer VertexBuffer {};
    VkDeviceMemory VertexBufferMemory {};
    size_t VertexBufferSize {};
    VkBuffer IndexBuffer {};
    VkDeviceMemory IndexBufferMemory {};
    size_t IndexBufferSize {};
};

class Vulkan_Effect final : public RenderEffect
{
    friend class Vulkan_Renderer;

public:
    Vulkan_Effect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) :
        RenderEffect(usage, name, loader)
    {
        FO_STACK_TRACE_ENTRY();
    }

    ~Vulkan_Effect() override;

    void DrawBuffer(RenderDrawBuffer* dbuf, size_t start_index, optional<size_t> indices_to_draw, const RenderTexture* custom_tex) override;

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
};

static void AllocateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory)
{
    FO_STACK_TRACE_ENTRY();

    VkResult vk_result = VK_SUCCESS;

    VkBufferCreateInfo buffer_ci {};
    buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_ci.size = size;
    buffer_ci.usage = usage;
    buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vk_result = vkCreateBuffer(Ctx->Device, &buffer_ci, nullptr, &buffer);
    VerifyVkResult(vk_result);

    VkMemoryRequirements mem_req {};
    vkGetBufferMemoryRequirements(Ctx->Device, buffer, &mem_req);

    // Find suitable memory type
    optional<uint32_t> mem_type_idx;
    const VkPhysicalDeviceMemoryProperties& mem_properties = Ctx->MemoryProperties;

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((mem_req.memoryTypeBits & (1 << i)) != 0 && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            mem_type_idx = i;
            break;
        }
    }

    FO_RUNTIME_ASSERT(mem_type_idx.has_value());

    VkMemoryAllocateInfo alloc_info {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex = mem_type_idx.value();

    vk_result = vkAllocateMemory(Ctx->Device, &alloc_info, nullptr, &memory);
    VerifyVkResult(vk_result);

    vk_result = vkBindBufferMemory(Ctx->Device, buffer, memory, 0);
    VerifyVkResult(vk_result);
}

static void AllocateImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& memory)
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

    vk_result = vkCreateImage(Ctx->Device, &image_ci, nullptr, &image);
    VerifyVkResult(vk_result);

    VkMemoryRequirements mem_req {};
    vkGetImageMemoryRequirements(Ctx->Device, image, &mem_req);

    // Find suitable memory type
    optional<uint32_t> mem_type_idx;
    const VkPhysicalDeviceMemoryProperties& mem_properties = Ctx->MemoryProperties;

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((mem_req.memoryTypeBits & (1 << i)) != 0 && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            mem_type_idx = i;
            break;
        }
    }

    FO_RUNTIME_ASSERT(mem_type_idx.has_value());

    VkMemoryAllocateInfo alloc_info {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex = mem_type_idx.value();

    vk_result = vkAllocateMemory(Ctx->Device, &alloc_info, nullptr, &memory);
    VerifyVkResult(vk_result);

    vk_result = vkBindImageMemory(Ctx->Device, image, memory, 0);
    VerifyVkResult(vk_result);
}

Vulkan_Texture::Vulkan_Texture(isize32 size, bool linear_filtered, bool with_depth) :
    RenderTexture(size, linear_filtered, with_depth)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(Ctx->Device);

    VkResult vk_result = VK_SUCCESS;

    AllocateImage(numeric_cast<uint32_t>(Size.width), numeric_cast<uint32_t>(Size.height), VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, TextureImage, TextureImageMemory);

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

    vk_result = vkCreateImageView(Ctx->Device, &image_view_ci, nullptr, &TextureImageView);
    VerifyVkResult(vk_result);

    if (WithDepth) {
        AllocateImage(numeric_cast<uint32_t>(Size.width), numeric_cast<uint32_t>(Size.height), VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DepthImage, DepthImageMemory);

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

        vk_result = vkCreateImageView(Ctx->Device, &depth_view_ci, nullptr, &DepthImageView);
        VerifyVkResult(vk_result);
    }

    // Put texture into a valid sampling layout immediately.
    // Some textures are sampled before any UpdateTextureRegion call.
    BeginCommandBufferRecording(Ctx->StagingCommandBuffer);

    TransitionColorImage(Ctx->StagingCommandBuffer, TextureImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    EndCommandBufferRecording(Ctx->StagingCommandBuffer);
    SubmitCommandBufferAndWait(Ctx->StagingCommandBuffer);

    TextureImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

Vulkan_Texture::~Vulkan_Texture()
{
    FO_STACK_TRACE_ENTRY();

    DestroyResourceSafe(TextureFramebuffer);
    DestroyResourceSafe(TextureImageView);
    DestroyResourceSafe(TextureImage);
    DestroyResourceSafe(TextureImageMemory);
    DestroyResourceSafe(DepthImageView);
    DestroyResourceSafe(DepthImage);
    DestroyResourceSafe(DepthImageMemory);
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

    FO_RUNTIME_ASSERT(Ctx->Device);
    FO_RUNTIME_ASSERT(TextureImage);

    VkResult vk_result = VK_SUCCESS;
    vector<ucolor> tex_region;
    const VkDeviceSize region_size = size.square() * sizeof(ucolor);

    // Create staging buffer for reading from GPU
    VkBuffer staging_buf {};
    VkDeviceMemory staging_mem {};
    AllocateBuffer(region_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buf, staging_mem);

    // Record copy commands using standalone staging command buffer
    BeginCommandBufferRecording(Ctx->StagingCommandBuffer);

    const auto old_layout = TextureImageLayout;
    TransitionColorImage(Ctx->StagingCommandBuffer, TextureImage, old_layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

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

    vkCmdCopyImageToBuffer(Ctx->StagingCommandBuffer, TextureImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging_buf, 1, &region);

    TransitionColorImage(Ctx->StagingCommandBuffer, TextureImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, old_layout);

    EndCommandBufferRecording(Ctx->StagingCommandBuffer);
    SubmitCommandBufferAndWait(Ctx->StagingCommandBuffer);

    // Read data from staging buffer
    void* map_data;
    vk_result = vkMapMemory(Ctx->Device, staging_mem, 0, region_size, 0, &map_data);
    VerifyVkResult(vk_result);
    tex_region.resize(size.square());
    MemCopy(tex_region.data(), map_data, region_size);
    vkUnmapMemory(Ctx->Device, staging_mem);

    // Swizzle B↔R: VK_FORMAT_B8G8R8A8_UNORM stores {B,G,R,A} but ucolor expects {R,G,B,A}
    {
        auto* pixels = reinterpret_cast<uint8_t*>(tex_region.data());
        for (size_t i = 0; i < tex_region.size(); i++) {
            std::swap(pixels[i * 4 + 0], pixels[i * 4 + 2]);
        }
    }

    // Cleanup
    vkDestroyBuffer(Ctx->Device, staging_buf, nullptr);
    vkFreeMemory(Ctx->Device, staging_mem, nullptr);

    ResetCommandBufferRecording(Ctx->StagingCommandBuffer);

    return tex_region;
}

void Vulkan_Texture::UpdateTextureRegion(ipos32 pos, isize32 size, const ucolor* data, bool use_dest_pitch)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(Ctx->Device);
    FO_RUNTIME_ASSERT(data);
    FO_RUNTIME_ASSERT(pos.x >= 0);
    FO_RUNTIME_ASSERT(pos.y >= 0);
    FO_RUNTIME_ASSERT(pos.x + size.width <= Size.width);
    FO_RUNTIME_ASSERT(pos.y + size.height <= Size.height);

    // Determine data pitch:
    // - use_dest_pitch=false: data is tightly packed for the region (size.width stride)
    // - use_dest_pitch=true: data has the full destination texture pitch (Size.width stride)
    const size_t row_bytes = numeric_cast<size_t>(size.width) * sizeof(ucolor);
    const size_t data_stride = use_dest_pitch ? (numeric_cast<size_t>(Size.width) * sizeof(ucolor)) : row_bytes;
    const VkDeviceSize total_data_size = numeric_cast<VkDeviceSize>(size.height) * data_stride;

    // Create staging buffer for texture data
    VkBuffer staging_buf {};
    VkDeviceMemory staging_mem {};

    AllocateBuffer(total_data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buf, staging_mem);

    // Copy pixel data to staging buffer
    void* mapped_data {};
    VkResult vk_result = vkMapMemory(Ctx->Device, staging_mem, 0, total_data_size, 0, &mapped_data);
    VerifyVkResult(vk_result);

    if (use_dest_pitch) {
        // Data has full destination texture pitch - copy row by row respecting the stride
        uint8_t* dst = static_cast<uint8_t*>(mapped_data);
        const uint8_t* src = reinterpret_cast<const uint8_t*>(data);

        for (int32_t y = 0; y < size.height; y++) {
            MemCopy(dst, src, row_bytes);
            dst += data_stride;
            src += data_stride;
        }
    }
    else {
        // Data is tightly packed for the region - direct copy
        MemCopy(mapped_data, data, row_bytes * size.height);
    }

    // Swizzle R↔B: ucolor stores {R,G,B,A} but VK_FORMAT_B8G8R8A8_UNORM expects {B,G,R,A}.
    // Only the first row_bytes of each data_stride row are populated; on the use_dest_pitch path the
    // inter-row gap is uninitialized, so swizzle per row and skip the gap (when use_dest_pitch is
    // false, data_stride == row_bytes, so this matches the tight-packed layout exactly).
    {
        uint8_t* row = static_cast<uint8_t*>(mapped_data);
        const size_t pixels_per_row = numeric_cast<size_t>(size.width);
        for (int32_t y = 0; y < size.height; y++) {
            for (size_t i = 0; i < pixels_per_row; i++) {
                std::swap(row[i * 4 + 0], row[i * 4 + 2]); // Swap R and B
            }
            row += data_stride;
        }
    }

    vkUnmapMemory(Ctx->Device, staging_mem);

    // Create GPU texture image if needed
    if (TextureImage == nullptr) {
        AllocateImage(numeric_cast<uint32_t>(Size.width), numeric_cast<uint32_t>(Size.height), VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, TextureImage, TextureImageMemory);

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

        vk_result = vkCreateImageView(Ctx->Device, &image_view_ci, nullptr, &TextureImageView);
        VerifyVkResult(vk_result);

        // Create depth buffer if requested
        if (WithDepth) {
            AllocateImage(numeric_cast<uint32_t>(Size.width), numeric_cast<uint32_t>(Size.height), VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DepthImage, DepthImageMemory);

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

            vk_result = vkCreateImageView(Ctx->Device, &depth_view_ci, nullptr, &DepthImageView);
            VerifyVkResult(vk_result);
        }
    }

    // Record copy commands using standalone staging command buffer
    BeginCommandBufferRecording(Ctx->StagingCommandBuffer);

    const auto old_layout = TextureImageLayout;
    TransitionColorImage(Ctx->StagingCommandBuffer, TextureImage, old_layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

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

    vkCmdCopyBufferToImage(Ctx->StagingCommandBuffer, staging_buf, TextureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    TransitionColorImage(Ctx->StagingCommandBuffer, TextureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    TextureImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    EndCommandBufferRecording(Ctx->StagingCommandBuffer);
    SubmitCommandBufferAndWait(Ctx->StagingCommandBuffer);

    // Free staging buffer after GPU is done
    vkDestroyBuffer(Ctx->Device, staging_buf, nullptr);
    vkFreeMemory(Ctx->Device, staging_mem, nullptr);

    ResetCommandBufferRecording(Ctx->StagingCommandBuffer);
}

Vulkan_DrawBuffer::~Vulkan_DrawBuffer()
{
    FO_STACK_TRACE_ENTRY();

    DestroyResourceSafe(VertexBuffer);
    DestroyResourceSafe(VertexBufferMemory);
    DestroyResourceSafe(IndexBuffer);
    DestroyResourceSafe(IndexBufferMemory);
}

void Vulkan_DrawBuffer::Upload(EffectUsage usage, optional<size_t> custom_vertices_size, optional<size_t> custom_indices_size)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(Ctx->Device);

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

    if (vert_size != 0) {
        if (!IsStatic) {
            // Dynamic buffers: allocate fresh HOST_VISIBLE buffer per upload to keep per-draw snapshots valid
            DestroyResourceSafe(VertexBuffer);
            DestroyResourceSafe(VertexBufferMemory);

            AllocateBuffer(vert_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VertexBuffer, VertexBufferMemory);
            VertexBufferSize = vert_size;

            void* data {};
            vk_result = vkMapMemory(Ctx->Device, VertexBufferMemory, 0, vert_size, 0, &data);
            VerifyVkResult(vk_result);
#if FO_ENABLE_3D
            if (usage == EffectUsage::Model) {
                MemCopy(data, Vertices3D.data(), vert_size);
            }
            else {
                MemCopy(data, Vertices.data(), vert_size);
            }
#else
            MemCopy(data, Vertices.data(), vert_size);
#endif
            vkUnmapMemory(Ctx->Device, VertexBufferMemory);
        }
        else {
            // Static buffers: use staging copy to GPU-local memory
            DestroyResourceSafe(VertexBuffer);
            DestroyResourceSafe(VertexBufferMemory);

            VkBuffer staging_vert_buf {};
            VkDeviceMemory staging_vert_mem {};

            AllocateBuffer(vert_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_vert_buf, staging_vert_mem);
            Ctx->StagingBuffers.emplace_back(staging_vert_buf, staging_vert_mem);

            void* data {};
            vk_result = vkMapMemory(Ctx->Device, staging_vert_mem, 0, vert_size, 0, &data);
            VerifyVkResult(vk_result);
#if FO_ENABLE_3D
            if (usage == EffectUsage::Model) {
                MemCopy(data, Vertices3D.data(), vert_size);
            }
            else {
                MemCopy(data, Vertices.data(), vert_size);
            }
#else
            MemCopy(data, Vertices.data(), vert_size);
#endif
            vkUnmapMemory(Ctx->Device, staging_vert_mem);

            AllocateBuffer(vert_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VertexBuffer, VertexBufferMemory);

            VkBufferCopy copy_region {};
            copy_region.size = vert_size;

            BeginCommandBufferRecording(Ctx->StagingCommandBuffer);

            vkCmdCopyBuffer(Ctx->StagingCommandBuffer, staging_vert_buf, VertexBuffer, 1, &copy_region);

            EndCommandBufferRecording(Ctx->StagingCommandBuffer);
            SubmitCommandBufferAndWait(Ctx->StagingCommandBuffer);

            for (const auto& [sb, sm] : Ctx->StagingBuffers) {
                if (sb != nullptr) {
                    vkDestroyBuffer(Ctx->Device, sb, nullptr);
                }
                if (sm != nullptr) {
                    vkFreeMemory(Ctx->Device, sm, nullptr);
                }
            }
            Ctx->StagingBuffers.clear();

            ResetCommandBufferRecording(Ctx->StagingCommandBuffer);
        }
    }

    if (idx_size != 0) {
        if (!IsStatic) {
            // Dynamic buffers: allocate fresh HOST_VISIBLE buffer per upload to keep per-draw snapshots valid
            DestroyResourceSafe(IndexBuffer);
            DestroyResourceSafe(IndexBufferMemory);

            AllocateBuffer(idx_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, IndexBuffer, IndexBufferMemory);
            IndexBufferSize = idx_size;

            void* data {};
            vk_result = vkMapMemory(Ctx->Device, IndexBufferMemory, 0, idx_size, 0, &data);
            VerifyVkResult(vk_result);
            MemCopy(data, Indices.data(), idx_size);
            vkUnmapMemory(Ctx->Device, IndexBufferMemory);
        }
        else {
            // Static buffers: use staging copy to GPU-local memory
            DestroyResourceSafe(IndexBuffer);
            DestroyResourceSafe(IndexBufferMemory);

            VkBuffer staging_idx_buf {};
            VkDeviceMemory staging_idx_mem {};
            AllocateBuffer(idx_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_idx_buf, staging_idx_mem);
            Ctx->StagingBuffers.emplace_back(staging_idx_buf, staging_idx_mem);

            void* data {};
            vk_result = vkMapMemory(Ctx->Device, staging_idx_mem, 0, idx_size, 0, &data);
            VerifyVkResult(vk_result);
            MemCopy(data, Indices.data(), idx_size);
            vkUnmapMemory(Ctx->Device, staging_idx_mem);

            AllocateBuffer(idx_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, IndexBuffer, IndexBufferMemory);

            VkBufferCopy copy_region {};
            copy_region.size = idx_size;

            BeginCommandBufferRecording(Ctx->StagingCommandBuffer);

            vkCmdCopyBuffer(Ctx->StagingCommandBuffer, staging_idx_buf, IndexBuffer, 1, &copy_region);

            EndCommandBufferRecording(Ctx->StagingCommandBuffer);
            SubmitCommandBufferAndWait(Ctx->StagingCommandBuffer);

            for (const auto& [sb, sm] : Ctx->StagingBuffers) {
                if (sb != nullptr) {
                    vkDestroyBuffer(Ctx->Device, sb, nullptr);
                }
                if (sm != nullptr) {
                    vkFreeMemory(Ctx->Device, sm, nullptr);
                }
            }
            Ctx->StagingBuffers.clear();

            ResetCommandBufferRecording(Ctx->StagingCommandBuffer);
        }
    }
}

Vulkan_Effect::~Vulkan_Effect()
{
    FO_STACK_TRACE_ENTRY();

    for (size_t pass = 0; pass < EFFECT_MAX_PASSES; pass++) {
        if (VertexShaderModule[pass] != nullptr) {
            vkDestroyShaderModule(Ctx->Device, VertexShaderModule[pass], nullptr);
            VertexShaderModule[pass] = VK_NULL_HANDLE;
        }
        if (FragmentShaderModule[pass] != nullptr) {
            vkDestroyShaderModule(Ctx->Device, FragmentShaderModule[pass], nullptr);
            FragmentShaderModule[pass] = VK_NULL_HANDLE;
        }
        for (size_t prim = 0; prim < 5; prim++) {
            for (size_t blend_disabled = 0; blend_disabled < 2; blend_disabled++) {
                if (Pipeline[pass][prim][blend_disabled] != nullptr) {
                    vkDestroyPipeline(Ctx->Device, Pipeline[pass][prim][blend_disabled], nullptr);
                    Pipeline[pass][prim][blend_disabled] = VK_NULL_HANDLE;
                }
            }
        }
    }

    if (PipelineLayout != nullptr) {
        vkDestroyPipelineLayout(Ctx->Device, PipelineLayout, nullptr);
        PipelineLayout = VK_NULL_HANDLE;
    }
}

void Vulkan_Effect::DrawBuffer(RenderDrawBuffer* dbuf, size_t start_index, optional<size_t> indices_to_draw, const RenderTexture* custom_tex)
{
    FO_STACK_TRACE_ENTRY();

    if (dbuf == nullptr || Ctx->Device == nullptr) {
        return;
    }

    auto* vk_dbuf = static_cast<Vulkan_DrawBuffer*>(dbuf); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

#if FO_ENABLE_3D
    Vulkan_Texture* main_tex;
    if (custom_tex != nullptr) {
        main_tex = static_cast<Vulkan_Texture*>(const_cast<RenderTexture*>(custom_tex)); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast,cppcoreguidelines-pro-type-const-cast)
    }
    else if (ModelTex[0]) {
        main_tex = static_cast<Vulkan_Texture*>(ModelTex[0].get()); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }
    else if (MainTex) {
        main_tex = static_cast<Vulkan_Texture*>(const_cast<RenderTexture*>(MainTex.get())); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast,cppcoreguidelines-pro-type-const-cast)
    }
    else {
        main_tex = static_cast<Vulkan_Texture*>(Ctx->DummyTexture.get()); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }
#else
    Vulkan_Texture* main_tex;
    if (custom_tex != nullptr) {
        main_tex = static_cast<Vulkan_Texture*>(const_cast<RenderTexture*>(custom_tex)); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast,cppcoreguidelines-pro-type-const-cast)
    }
    else if (MainTex) {
        main_tex = static_cast<Vulkan_Texture*>(const_cast<RenderTexture*>(MainTex.get())); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast,cppcoreguidelines-pro-type-const-cast)
    }
    else {
        main_tex = static_cast<Vulkan_Texture*>(Ctx->DummyTexture.get()); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }
#endif

    if (_needProjBuf && !ProjBuf.has_value()) {
        auto& proj_buf = ProjBuf = ProjBuffer();
        MemCopy(proj_buf->ProjMatrix, &Ctx->ProjectionMatrixColMaj[0][0], 16 * sizeof(float32_t));
    }

    if (_needMainTexBuf && !MainTexBuf.has_value()) {
        auto& main_tex_buf = MainTexBuf = MainTexBuffer();
        MemCopy(main_tex_buf->MainTexSize, main_tex->SizeData, 4 * sizeof(float32_t));
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

    // Upload draw buffer geometry
    if (vk_dbuf->VertexBuffer == nullptr) {
        return;
    }

    // Skip if no geometry to render
    if (vk_dbuf->IndCount == 0 && vk_dbuf->VertCount == 0) {
        return;
    }

    // Ensure buffers are uploaded to GPU
    if (vk_dbuf->VertexBuffer == nullptr) {
        return;
    }

    // Bind vertex buffer
    constexpr VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(Ctx->CommandBuffer, 0, 1, &vk_dbuf->VertexBuffer, offsets);

    // Process each pass
    for (size_t pass = 0; pass < _passCount; pass++) {
        const auto prim_type = _usage == EffectUsage::Primitive ? vk_dbuf->PrimType : RenderPrimitiveType::TriangleList;
        const auto prim_index = static_cast<size_t>(prim_type);
        FO_RUNTIME_ASSERT(prim_index < 5);

        // Bind pipeline for current pass — pick the variant that matches the current
        // DisableBlending flag (opaque writes for the blit-style flushes, alpha-blend otherwise).
        const size_t blend_variant = DisableBlending ? 1 : 0;
        if (Pipeline[pass][prim_index][blend_variant] != nullptr) {
            vkCmdBindPipeline(Ctx->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline[pass][prim_index][blend_variant]);
        }

        // Allocate descriptor sets for this draw call
        VkDescriptorSet texture_set = VK_NULL_HANDLE;
        VkDescriptorSet uniform_set = VK_NULL_HANDLE;

        VkDescriptorSetAllocateInfo alloc_info {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = Ctx->FrameDescriptorPool;
        alloc_info.descriptorSetCount = 1;

        if (Ctx->TextureDescriptorSetLayout != nullptr) {
            alloc_info.pSetLayouts = &Ctx->TextureDescriptorSetLayout;
            const auto vk_result = vkAllocateDescriptorSets(Ctx->Device, &alloc_info, &texture_set);
            VerifyVkResult(vk_result);
        }

        if (Ctx->UniformDescriptorSetLayout != nullptr) {
            alloc_info.pSetLayouts = &Ctx->UniformDescriptorSetLayout;
            const auto vk_result = vkAllocateDescriptorSets(Ctx->Device, &alloc_info, &uniform_set);
            VerifyVkResult(vk_result);
        }

        // Update and bind per-pass texture descriptor set (set = 1)
        if (texture_set != VK_NULL_HANDLE && this->PipelineLayout != nullptr) {
            VkDescriptorImageInfo image_infos[16];
            VkWriteDescriptorSet writes[16];
            size_t write_count = 0;

            const auto append_sampler = [&](int32_t binding, Vulkan_Texture* tex) {
                if (binding < 0 || tex == nullptr || tex->TextureImageView == nullptr) {
                    return;
                }

                FO_RUNTIME_ASSERT(write_count < 16);

                auto& image_info = image_infos[write_count];
                image_info.sampler = tex->LinearFiltered ? Ctx->LinearSampler : Ctx->PointSampler;
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
                auto* indoor_tex = static_cast<Vulkan_Texture*>(const_cast<RenderTexture*>(IndoorMaskTex ? IndoorMaskTex.get() : Ctx->DummyTexture.get())); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast,cppcoreguidelines-pro-type-const-cast)
                append_sampler(_posIndoorMaskTex[pass], indoor_tex);
            }

#if FO_ENABLE_3D
            for (size_t model_tex_index = 0; model_tex_index < MODEL_MAX_TEXTURES; model_tex_index++) {
                auto* model_tex = static_cast<Vulkan_Texture*>(ModelTex[model_tex_index] ? ModelTex[model_tex_index].get() : Ctx->DummyTexture.get()); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                append_sampler(_posModelTex[pass][model_tex_index], model_tex);
            }
#endif

            if (write_count > 0) {
                vkUpdateDescriptorSets(Ctx->Device, numeric_cast<uint32_t>(write_count), writes, 0, nullptr);
            }

            vkCmdBindDescriptorSets(Ctx->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, this->PipelineLayout, 1, 1, &texture_set, 0, nullptr);
        }

        // Update and bind per-pass uniform descriptor set (set = 0)
        if (uniform_set != VK_NULL_HANDLE && this->PipelineLayout != nullptr) {
            VkDescriptorBufferInfo buffer_infos[16];
            VkWriteDescriptorSet writes[16];
            size_t write_count = 0;

            const size_t alignment = numeric_cast<size_t>(Ctx->MinUniformBufferOffsetAlignment);

            const auto upload_uniform_buffer = [&](int32_t binding, const void* src_data, size_t src_size) {
                if (binding < 0 || src_data == nullptr || src_size == 0) {
                    return;
                }

                FO_RUNTIME_ASSERT(write_count < 16);

                // Align offset
                Ctx->FrameUniformOffset = (Ctx->FrameUniformOffset + alignment - 1) & ~(alignment - 1);
                FO_RUNTIME_ASSERT(Ctx->FrameUniformOffset + src_size <= Ctx->FrameUniformBufferSize);

                // Copy data
                MemCopy(static_cast<uint8_t*>(Ctx->FrameUniformBufferMapped) + Ctx->FrameUniformOffset, src_data, src_size);

                auto& buffer_info = buffer_infos[write_count];
                buffer_info.buffer = Ctx->FrameUniformBuffer;
                buffer_info.offset = Ctx->FrameUniformOffset;
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
                Ctx->FrameUniformOffset += src_size;
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
                vkUpdateDescriptorSets(Ctx->Device, numeric_cast<uint32_t>(write_count), writes, 0, nullptr);
                vkCmdBindDescriptorSets(Ctx->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, this->PipelineLayout, 0, 1, &uniform_set, 0, nullptr);
            }
        }

        // Draw indexed or non-indexed
        if (vk_dbuf->IndCount != 0 && vk_dbuf->IndexBuffer != nullptr) {
            // Bind index buffer and draw indexed
            // ReSharper disable once CppUnreachableCode
            constexpr auto index_type = sizeof(vindex_t) == sizeof(uint32_t) ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
            vkCmdBindIndexBuffer(Ctx->CommandBuffer, vk_dbuf->IndexBuffer, 0, index_type);

            const size_t num_indices = indices_to_draw.value_or(vk_dbuf->IndCount);
            vkCmdDrawIndexed(Ctx->CommandBuffer, numeric_cast<uint32_t>(num_indices), 1, numeric_cast<uint32_t>(start_index), 0, 0);
        }
        else if (vk_dbuf->VertCount != 0) {
            // Draw without indices. start_index is an index-buffer offset in the DrawBuffer contract,
            // which has no meaning without an index buffer; the engine never emits a vertices-only draw
            // with a non-zero offset (matches Direct3D's FL<=9.3 point-list path), so firstVertex is 0.
            FO_RUNTIME_ASSERT(start_index == 0);
            const size_t num_vertices = vk_dbuf->VertCount;
            vkCmdDraw(Ctx->CommandBuffer, numeric_cast<uint32_t>(num_vertices), 1, 0, 0);
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

Vulkan_Renderer::Vulkan_Renderer()
{
    FO_STACK_TRACE_ENTRY();

    _ctx = SafeAlloc::MakeUnique<Context>();
    Ctx = _ctx.get();
}

Vulkan_Renderer::~Vulkan_Renderer()
{
    FO_STACK_TRACE_ENTRY();

    if (Ctx->Device != nullptr) {
        vkDeviceWaitIdle(Ctx->Device);
        FlushDeferredDestroyResources();

        // Release the dummy texture's Vk handles now, while the device is still alive and the
        // global Ctx is still valid. ~Vulkan_Texture() routes its handles through Ctx->DeferredDestroy*
        // (which dereferences Ctx) and they must be flushed against a live VkDevice. If left to the
        // implicit Context teardown, it would run after Ctx = nullptr and vkDestroyDevice below.
        Ctx->DummyTexture.reset();
        FlushDeferredDestroyResources();
    }

    // Destroy staging buffers
    for (const auto& [buf, mem] : Ctx->StagingBuffers) {
        if (buf != nullptr) {
            vkDestroyBuffer(Ctx->Device, buf, nullptr);
        }
        if (mem != nullptr) {
            vkFreeMemory(Ctx->Device, mem, nullptr);
        }
    }

    Ctx->StagingBuffers.clear();

    for (auto* fb : Ctx->Framebuffers) {
        vkDestroyFramebuffer(Ctx->Device, fb, nullptr);
    }

    Ctx->Framebuffers.clear();

    for (auto* iv : Ctx->SwapchainImageViews) {
        vkDestroyImageView(Ctx->Device, iv, nullptr);
    }

    Ctx->SwapchainImageViews.clear();

    if (Ctx->SwapchainDepthImageView != nullptr) {
        vkDestroyImageView(Ctx->Device, Ctx->SwapchainDepthImageView, nullptr);
        Ctx->SwapchainDepthImageView = VK_NULL_HANDLE;
    }
    if (Ctx->SwapchainDepthImage != nullptr) {
        vkDestroyImage(Ctx->Device, Ctx->SwapchainDepthImage, nullptr);
        Ctx->SwapchainDepthImage = VK_NULL_HANDLE;
    }
    if (Ctx->SwapchainDepthImageMemory != nullptr) {
        vkFreeMemory(Ctx->Device, Ctx->SwapchainDepthImageMemory, nullptr);
        Ctx->SwapchainDepthImageMemory = VK_NULL_HANDLE;
    }

    if (Ctx->RenderPass != nullptr) {
        vkDestroyRenderPass(Ctx->Device, Ctx->RenderPass, nullptr);
        Ctx->RenderPass = VK_NULL_HANDLE;
    }

    if (Ctx->SwapchainDepthImageView != nullptr) {
        vkDestroyImageView(Ctx->Device, Ctx->SwapchainDepthImageView, nullptr);
        Ctx->SwapchainDepthImageView = VK_NULL_HANDLE;
    }
    if (Ctx->SwapchainDepthImage != nullptr) {
        vkDestroyImage(Ctx->Device, Ctx->SwapchainDepthImage, nullptr);
        Ctx->SwapchainDepthImage = VK_NULL_HANDLE;
    }
    if (Ctx->SwapchainDepthImageMemory != nullptr) {
        vkFreeMemory(Ctx->Device, Ctx->SwapchainDepthImageMemory, nullptr);
        Ctx->SwapchainDepthImageMemory = VK_NULL_HANDLE;
    }

    if (Ctx->CommandPool != nullptr) {
        vkDestroyCommandPool(Ctx->Device, Ctx->CommandPool, nullptr);
        Ctx->CommandPool = VK_NULL_HANDLE;
    }

    if (Ctx->ImageAvailableSemaphore != nullptr) {
        vkDestroySemaphore(Ctx->Device, Ctx->ImageAvailableSemaphore, nullptr);
        Ctx->ImageAvailableSemaphore = VK_NULL_HANDLE;
    }
    if (Ctx->RenderCompleteSemaphore != nullptr) {
        vkDestroySemaphore(Ctx->Device, Ctx->RenderCompleteSemaphore, nullptr);
        Ctx->RenderCompleteSemaphore = VK_NULL_HANDLE;
    }

    if (Ctx->Swapchain != nullptr) {
        vkDestroySwapchainKHR(Ctx->Device, Ctx->Swapchain, nullptr);
        Ctx->Swapchain = VK_NULL_HANDLE;
    }
    if (Ctx->TextureDescriptorSetLayout != nullptr) {
        vkDestroyDescriptorSetLayout(Ctx->Device, Ctx->TextureDescriptorSetLayout, nullptr);
        Ctx->TextureDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (Ctx->UniformDescriptorSetLayout != nullptr) {
        vkDestroyDescriptorSetLayout(Ctx->Device, Ctx->UniformDescriptorSetLayout, nullptr);
        Ctx->UniformDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (Ctx->FrameDescriptorPool != nullptr) {
        vkDestroyDescriptorPool(Ctx->Device, Ctx->FrameDescriptorPool, nullptr);
        Ctx->FrameDescriptorPool = VK_NULL_HANDLE;
    }
    if (Ctx->FrameUniformBuffer != nullptr) {
        vkDestroyBuffer(Ctx->Device, Ctx->FrameUniformBuffer, nullptr);
        Ctx->FrameUniformBuffer = VK_NULL_HANDLE;
    }
    if (Ctx->FrameUniformBufferMemory != nullptr) {
        vkUnmapMemory(Ctx->Device, Ctx->FrameUniformBufferMemory);
        vkFreeMemory(Ctx->Device, Ctx->FrameUniformBufferMemory, nullptr);
        Ctx->FrameUniformBufferMemory = VK_NULL_HANDLE;
    }
    if (Ctx->LinearSampler != nullptr) {
        vkDestroySampler(Ctx->Device, Ctx->LinearSampler, nullptr);
        Ctx->LinearSampler = VK_NULL_HANDLE;
    }
    if (Ctx->PointSampler != nullptr) {
        vkDestroySampler(Ctx->Device, Ctx->PointSampler, nullptr);
        Ctx->PointSampler = VK_NULL_HANDLE;
    }
    if (Ctx->Device != nullptr) {
        vkDestroyDevice(Ctx->Device, nullptr);
        Ctx->Device = VK_NULL_HANDLE;
    }
    if (Ctx->Surface != nullptr && Ctx->Instance != nullptr) {
        vkDestroySurfaceKHR(Ctx->Instance, Ctx->Surface, nullptr);
        Ctx->Surface = VK_NULL_HANDLE;
    }
    if (Ctx->DebugMessenger != nullptr && Ctx->Instance != nullptr) {
        if (auto vkDestroyDebugUtilsMessengerEXT_fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(Ctx->Instance, "vkDestroyDebugUtilsMessengerEXT"));
            vkDestroyDebugUtilsMessengerEXT_fn != nullptr) {
            vkDestroyDebugUtilsMessengerEXT_fn(Ctx->Instance, Ctx->DebugMessenger, nullptr);
        }
        Ctx->DebugMessenger = VK_NULL_HANDLE;
    }
    if (Ctx->Instance != nullptr) {
        vkDestroyInstance(Ctx->Instance, nullptr);
        Ctx->Instance = VK_NULL_HANDLE;
    }

    Ctx = nullptr;
}

[[nodiscard]] auto Vulkan_Renderer::CreateTexture(isize32 size, bool linear_filtered, bool with_depth) -> unique_ptr<RenderTexture>
{
    FO_STACK_TRACE_ENTRY();

    auto tex = SafeAlloc::MakeUnique<Vulkan_Texture>(size, linear_filtered, with_depth);

    return std::move(tex);
}

[[nodiscard]] auto Vulkan_Renderer::CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer>
{
    FO_STACK_TRACE_ENTRY();

    auto dbuf = SafeAlloc::MakeUnique<Vulkan_DrawBuffer>(is_static);

    return std::move(dbuf);
}

[[nodiscard]] auto Vulkan_Renderer::CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect>
{
    FO_STACK_TRACE_ENTRY();

    auto vk_effect = SafeAlloc::MakeUnique<Vulkan_Effect>(usage, name, loader);

    for (size_t pass = 0; pass < vk_effect->_passCount; pass++) {
        // Load vertex shader SPIR-V
        {
            const string vert_fname = strex("{}.fofx-{}-vert-spv", strex(name).erase_file_extension(), pass + 1);
            string vert_content = loader(vert_fname);
            FO_RUNTIME_ASSERT(!vert_content.empty());
            FO_RUNTIME_ASSERT(vert_content.length() % sizeof(uint32_t) == 0);

            VkShaderModuleCreateInfo module_ci {};
            module_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            module_ci.codeSize = vert_content.length();
            module_ci.pCode = reinterpret_cast<const uint32_t*>(vert_content.data());

            if (vkCreateShaderModule(Ctx->Device, &module_ci, nullptr, &vk_effect->VertexShaderModule[pass]) != VK_SUCCESS) {
                throw EffectLoadException("Failed to create vertex shader module", vert_fname);
            }
        }

        // Load fragment shader SPIR-V
        {
            const string frag_fname = strex("{}.fofx-{}-frag-spv", strex(name).erase_file_extension(), pass + 1);
            string frag_content = loader(frag_fname);
            FO_RUNTIME_ASSERT(!frag_content.empty());
            FO_RUNTIME_ASSERT(frag_content.length() % sizeof(uint32_t) == 0);

            VkShaderModuleCreateInfo module_ci {};
            module_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            module_ci.codeSize = frag_content.length();
            module_ci.pCode = reinterpret_cast<const uint32_t*>(frag_content.data());

            if (vkCreateShaderModule(Ctx->Device, &module_ci, nullptr, &vk_effect->FragmentShaderModule[pass]) != VK_SUCCESS) {
                throw EffectLoadException("Failed to create fragment shader module", frag_fname);
            }
        }
    }

    // Create pipeline layout for this effect
    VkPipelineLayoutCreateInfo layout_ci {};
    layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    VkDescriptorSetLayout set_layouts[] = {Ctx->UniformDescriptorSetLayout, Ctx->TextureDescriptorSetLayout};
    layout_ci.setLayoutCount = 2;
    layout_ci.pSetLayouts = set_layouts;

    if (vkCreatePipelineLayout(Ctx->Device, &layout_ci, nullptr, &vk_effect->PipelineLayout) != VK_SUCCESS) {
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
#if FO_ENABLE_3D
        if (usage == EffectUsage::Model) {
            depth_ci.depthTestEnable = VK_TRUE;
            depth_ci.depthWriteEnable = vk_effect->_depthWrite[pass] ? VK_TRUE : VK_FALSE;
            depth_ci.depthCompareOp = VK_COMPARE_OP_LESS;
        }
        else
#endif
        {
            depth_ci.depthTestEnable = VK_FALSE;
            depth_ci.depthWriteEnable = VK_FALSE;
            depth_ci.depthCompareOp = VK_COMPARE_OP_LESS;
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
                pipeline_ci.renderPass = Ctx->RenderPass;
                pipeline_ci.subpass = 0;

                if (vkCreateGraphicsPipelines(Ctx->Device, nullptr, 1, &pipeline_ci, nullptr, &vk_effect->Pipeline[pass][prim][blend_variant]) != VK_SUCCESS) {
                    throw EffectLoadException("Failed to create graphics pipeline", name);
                }
            }
        }
    }

    return vk_effect;
}

[[nodiscard]] auto Vulkan_Renderer::CreateOrthoMatrix(float32_t left, float32_t right, float32_t bottom, float32_t top, float32_t nearp, float32_t farp) -> mat44
{
    FO_STACK_TRACE_ENTRY();

    mat44 m {};
    m[0][0] = 2.0f / (right - left);
    m[1][1] = 2.0f / (bottom - top);
    m[2][2] = 1.0f / (nearp - farp);
    m[0][3] = -(right + left) / (right - left);
    m[1][3] = -(top + bottom) / (bottom - top);
    m[2][3] = nearp / (nearp - farp);
    m[3][3] = 1.0f;
    return m;
}

[[nodiscard]] auto Vulkan_Renderer::GetViewPort() -> irect32
{
    FO_STACK_TRACE_ENTRY();

    return Ctx->ViewPort;
}

void Vulkan_Renderer::Init(GlobalSettings& settings, WindowInternalHandle* window)
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Used Vulkan rendering");

    Ctx->Settings = &settings;
    Ctx->SdlWindow = static_cast<SDL_Window*>(window);

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
    app_info.apiVersion = VK_API_VERSION_1_0;

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

    vk_result = vkCreateInstance(&create_info, nullptr, &Ctx->Instance);

    if (vk_result != VK_SUCCESS) {
        throw RenderingException("vkCreateInstance failed", vk_result);
    }

    if (want_validation) {
        // Wire the debug messenger so layer warnings/errors land in LF_Server.log under [VkLayer].
        // Without this we'd see the rendering misbehave silently — the layer would whisper to
        // stdout which the bench redirects elsewhere and we never see it.
        if (auto vkCreateDebugUtilsMessengerEXT_fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(Ctx->Instance, "vkCreateDebugUtilsMessengerEXT"));
            vkCreateDebugUtilsMessengerEXT_fn != nullptr) {
            VkDebugUtilsMessengerCreateInfoEXT msg_ci {};
            msg_ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            msg_ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            msg_ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            msg_ci.pfnUserCallback = +[](VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                         VkDebugUtilsMessageTypeFlagsEXT type,
                                         const VkDebugUtilsMessengerCallbackDataEXT* data,
                                         void* /*user*/) -> VkBool32 {
                const char* sev = severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ? "ERROR" :
                                  severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT ? "WARN" : "INFO";
                WriteLog("[VkLayer/{}] {}: {}", sev, data->pMessageIdName != nullptr ? data->pMessageIdName : "?", data->pMessage != nullptr ? data->pMessage : "?");
                ignore_unused(type);
                return VK_FALSE;
            };
            vk_result = vkCreateDebugUtilsMessengerEXT_fn(Ctx->Instance, &msg_ci, nullptr, &Ctx->DebugMessenger);
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

    if (!SDL_Vulkan_CreateSurface(Ctx->SdlWindow.get(), Ctx->Instance, nullptr, &Ctx->Surface)) {
        throw RenderingException("SDL_Vulkan_CreateSurface failed");
    }

    // Enumerate physical devices (selection happens below)
    uint32_t gpu_count = 0;
    vk_result = vkEnumeratePhysicalDevices(Ctx->Instance, &gpu_count, nullptr);
    VerifyVkResult(vk_result);

    if (gpu_count == 0) {
        throw RenderingException("No Vulkan physical devices found");
    }

    vector<VkPhysicalDevice> gpus;
    gpus.resize(gpu_count);
    vk_result = vkEnumeratePhysicalDevices(Ctx->Instance, &gpu_count, gpus.data());
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
        if (!has_swapchain_ext(gpu) || !has_graphics_present_family(gpu, Ctx->Surface)) {
            continue;
        }

        VkPhysicalDeviceProperties props {};
        vkGetPhysicalDeviceProperties(gpu, &props);
        const int32_t score = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 3 :
            props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU  ? 2 :
            props.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU     ? 1 :
                                                                          0;
        if (score > best_score) {
            best_score = score;
            Ctx->PhysicalDevice = gpu;
        }
    }

    if (Ctx->PhysicalDevice == nullptr) {
        throw RenderingException("No suitable Vulkan physical device (needs graphics+present queue and swapchain support)");
    }

    VkPhysicalDeviceProperties gpu_props {};
    vkGetPhysicalDeviceProperties(Ctx->PhysicalDevice, &gpu_props);

    // Cache immutable device properties once so the hot paths don't re-query them.
    Ctx->MinUniformBufferOffsetAlignment = gpu_props.limits.minUniformBufferOffsetAlignment;
    vkGetPhysicalDeviceMemoryProperties(Ctx->PhysicalDevice, &Ctx->MemoryProperties);

    const auto atlas_limit = numeric_cast<int32_t>(std::min(gpu_props.limits.maxImageDimension2D, numeric_cast<uint32_t>(AppRender::MAX_ATLAS_SIZE)));
    FO_RUNTIME_ASSERT_STR(atlas_limit >= AppRender::MIN_ATLAS_SIZE, strex("Min texture size must be at least {}", AppRender::MIN_ATLAS_SIZE));
    const_cast<int32_t&>(AppRender::MAX_ATLAS_WIDTH) = atlas_limit;
    const_cast<int32_t&>(AppRender::MAX_ATLAS_HEIGHT) = atlas_limit;

    // Find queue family
    uint32_t qcount;
    vkGetPhysicalDeviceQueueFamilyProperties(Ctx->PhysicalDevice, &qcount, nullptr);
    vector<VkQueueFamilyProperties> qprops(qcount);
    qprops.resize(qcount);
    vkGetPhysicalDeviceQueueFamilyProperties(Ctx->PhysicalDevice, &qcount, qprops.data());

    optional<uint32_t> graphics_family;

    for (uint32_t i = 0; i < qcount; i++) {
        VkBool32 present_support = VK_FALSE;
        vk_result = vkGetPhysicalDeviceSurfaceSupportKHR(Ctx->PhysicalDevice, i, Ctx->Surface, &present_support);
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

    const char* debug_layers[] = {"VK_LAYER_KHRONOS_validation"};
    dci.enabledLayerCount = settings.RenderDebug ? 1 : 0;
    dci.ppEnabledLayerNames = debug_layers;

    vk_result = vkCreateDevice(Ctx->PhysicalDevice, &dci, nullptr, &Ctx->Device);
    VerifyVkResult(vk_result);

    vkGetDeviceQueue(Ctx->Device, graphics_family.value(), 0, &Ctx->GraphicsQueue);
    Ctx->GraphicsFamilyIndex = graphics_family.value();

    // Create command pool for recording render commands
    VkCommandPoolCreateInfo cpi {};
    cpi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpi.queueFamilyIndex = Ctx->GraphicsFamilyIndex;
    vk_result = vkCreateCommandPool(Ctx->Device, &cpi, nullptr, &Ctx->CommandPool);
    VerifyVkResult(vk_result);

    Ctx->ViewPort = irect32 {{0, 0}, {0, 0}};

    // Initialize swapchain for current window size
    int width;
    int height;
    SDL_GetWindowSizeInPixels(Ctx->SdlWindow.get(), &width, &height);
    Ctx->ViewPort = irect32 {{0, 0}, {std::max(width, 1), std::max(height, 1)}};
    Ctx->TargetSize = {Ctx->Settings->ScreenWidth, Ctx->Settings->ScreenHeight};
    Ctx->ProjectionMatrixColMaj = CreateOrthoMatrix(0.0f, numeric_cast<float32_t>(Ctx->TargetSize.width), numeric_cast<float32_t>(Ctx->TargetSize.height), 0.0f, -10.0f, 10.0f);
    Ctx->ProjectionMatrixColMaj = glm::transpose(Ctx->ProjectionMatrixColMaj);
    RecreateSwapchain({width, height});

    // Allocate single command buffer (single-threaded rendering)
    VkCommandBufferAllocateInfo cbai {};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = Ctx->CommandPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;

    vk_result = vkAllocateCommandBuffers(Ctx->Device, &cbai, &Ctx->CommandBuffer);
    VerifyVkResult(vk_result);

    // Create semaphores for acquire/present synchronization
    VkSemaphoreCreateInfo sem_ci {};
    sem_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vk_result = vkCreateSemaphore(Ctx->Device, &sem_ci, nullptr, &Ctx->ImageAvailableSemaphore);
    VerifyVkResult(vk_result);
    vk_result = vkCreateSemaphore(Ctx->Device, &sem_ci, nullptr, &Ctx->RenderCompleteSemaphore);
    VerifyVkResult(vk_result);

    // Allocate staging command buffer for buffer uploads
    VkCommandBufferAllocateInfo staging_cbai {};
    staging_cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    staging_cbai.commandPool = Ctx->CommandPool;
    staging_cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    staging_cbai.commandBufferCount = 1;

    vk_result = vkAllocateCommandBuffers(Ctx->Device, &staging_cbai, &Ctx->StagingCommandBuffer);
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

    vk_result = vkCreateDescriptorSetLayout(Ctx->Device, &layout_ci, nullptr, &Ctx->TextureDescriptorSetLayout);
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

    vk_result = vkCreateDescriptorSetLayout(Ctx->Device, &ubo_layout_ci, nullptr, &Ctx->UniformDescriptorSetLayout);
    VerifyVkResult(vk_result);

    // Create Ctx->FrameDescriptorPool
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

    vk_result = vkCreateDescriptorPool(Ctx->Device, &frame_pool_ci, nullptr, &Ctx->FrameDescriptorPool);
    VerifyVkResult(vk_result);

    // Create Ctx->FrameUniformBuffer
    AllocateBuffer(Ctx->FrameUniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, Ctx->FrameUniformBuffer, Ctx->FrameUniformBufferMemory);
    vk_result = vkMapMemory(Ctx->Device, Ctx->FrameUniformBufferMemory, 0, Ctx->FrameUniformBufferSize, 0, &Ctx->FrameUniformBufferMapped);
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

    vk_result = vkCreateSampler(Ctx->Device, &sampler_ci, nullptr, &Ctx->LinearSampler);
    VerifyVkResult(vk_result);

    sampler_ci.magFilter = VK_FILTER_NEAREST;
    sampler_ci.minFilter = VK_FILTER_NEAREST;
    sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    vk_result = vkCreateSampler(Ctx->Device, &sampler_ci, nullptr, &Ctx->PointSampler);
    VerifyVkResult(vk_result);

    constexpr ucolor dummy_pixel[1] = {ucolor {255, 0, 255, 255}};
    Ctx->DummyTexture = CreateTexture({1, 1}, false, false);
    Ctx->DummyTexture->UpdateTextureRegion({}, {1, 1}, dummy_pixel);

    // Begin first frame and set default render target (matches OpenGL/D3D init flow)
    BeginFrame();
    SetRenderTarget(nullptr);
}

static void RecreateSwapchain(isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(size.width > 0);
    FO_RUNTIME_ASSERT(size.height > 0);

    VkResult vk_result = VK_SUCCESS;

    // Destroy old framebuffers and image views
    for (auto* fb : Ctx->Framebuffers) {
        DestroyResourceSafe(fb);
    }

    Ctx->Framebuffers.clear();

    for (auto* iv : Ctx->SwapchainImageViews) {
        DestroyResourceSafe(iv);
    }

    Ctx->SwapchainImageViews.clear();

    if (Ctx->Swapchain != nullptr) {
        vkDeviceWaitIdle(Ctx->Device);
        vkDestroySwapchainKHR(Ctx->Device, Ctx->Swapchain, nullptr);
        Ctx->Swapchain = VK_NULL_HANDLE;
        Ctx->SwapchainImages.clear();
        Ctx->SwapchainImageLayouts.clear();
    }

    DestroyResourceSafe(Ctx->SwapchainDepthImageView);
    DestroyResourceSafe(Ctx->SwapchainDepthImage);
    DestroyResourceSafe(Ctx->SwapchainDepthImageMemory);

    // Create swapchain
    VkSurfaceCapabilitiesKHR caps {};
    vk_result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Ctx->PhysicalDevice, Ctx->Surface, &caps);
    VerifyVkResult(vk_result);

    VkExtent2D swapchain_extent {};
    if (caps.currentExtent.width != UINT32_MAX) {
        swapchain_extent = caps.currentExtent;
    }
    else {
        swapchain_extent.width = std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, numeric_cast<uint32_t>(size.width)));
        swapchain_extent.height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, numeric_cast<uint32_t>(size.height)));
    }

    Ctx->SwapchainSize = {numeric_cast<int32_t>(swapchain_extent.width), numeric_cast<int32_t>(swapchain_extent.height)};

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
        vk_result = vkGetPhysicalDeviceSurfaceFormatsKHR(Ctx->PhysicalDevice, Ctx->Surface, &format_count, nullptr);
        VerifyVkResult(vk_result);
        FO_RUNTIME_ASSERT(format_count != 0);
        vector<VkSurfaceFormatKHR> surface_formats(format_count);
        vk_result = vkGetPhysicalDeviceSurfaceFormatsKHR(Ctx->PhysicalDevice, Ctx->Surface, &format_count, surface_formats.data());
        VerifyVkResult(vk_result);

        // A lone VK_FORMAT_UNDEFINED entry means the surface imposes no format restriction.
        bool format_supported = surface_formats.size() == 1 && surface_formats.front().format == VK_FORMAT_UNDEFINED;
        for (const auto& sf : surface_formats) {
            if (sf.format == VK_FORMAT_B8G8R8A8_UNORM && sf.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
                format_supported = true;
                break;
            }
        }
        FO_RUNTIME_ASSERT_STR(format_supported, "Surface does not support required VK_FORMAT_B8G8R8A8_UNORM / SRGB_NONLINEAR");
    }

    VkSwapchainCreateInfoKHR sci {};
    sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = Ctx->Surface;
    sci.minImageCount = image_count;
    sci.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    sci.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    sci.imageExtent = swapchain_extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    sci.clipped = VK_TRUE;
    sci.oldSwapchain = VK_NULL_HANDLE;

    vk_result = vkCreateSwapchainKHR(Ctx->Device, &sci, nullptr, &Ctx->Swapchain);
    VerifyVkResult(vk_result);
    vk_result = vkGetSwapchainImagesKHR(Ctx->Device, Ctx->Swapchain, &image_count, nullptr);
    VerifyVkResult(vk_result);
    Ctx->SwapchainImages.resize(image_count);
    vk_result = vkGetSwapchainImagesKHR(Ctx->Device, Ctx->Swapchain, &image_count, Ctx->SwapchainImages.data());
    VerifyVkResult(vk_result);
    Ctx->SwapchainImageLayouts.assign(Ctx->SwapchainImages.size(), VK_IMAGE_LAYOUT_UNDEFINED);
    Ctx->SwapchainFormat = sci.imageFormat;

    if (Ctx->RenderPass == nullptr) {
        VkAttachmentDescription color_attachment {};
        color_attachment.format = Ctx->SwapchainFormat;
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

        VkRenderPassCreateInfo rp_info {};
        rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rp_info.attachmentCount = 2;
        rp_info.pAttachments = attachments;
        rp_info.subpassCount = 1;
        rp_info.pSubpasses = &subpass;

        vk_result = vkCreateRenderPass(Ctx->Device, &rp_info, nullptr, &Ctx->RenderPass);
        VerifyVkResult(vk_result);
    }

    // Create framebuffers
    Ctx->SwapchainImageViews.resize(Ctx->SwapchainImages.size());
    Ctx->Framebuffers.resize(Ctx->SwapchainImages.size());

    AllocateImage(swapchain_extent.width, swapchain_extent.height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, Ctx->SwapchainDepthImage, Ctx->SwapchainDepthImageMemory);

    VkImageViewCreateInfo depth_view_ci {};
    depth_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depth_view_ci.image = Ctx->SwapchainDepthImage;
    depth_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depth_view_ci.format = VK_FORMAT_D32_SFLOAT;
    depth_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depth_view_ci.subresourceRange.baseMipLevel = 0;
    depth_view_ci.subresourceRange.levelCount = 1;
    depth_view_ci.subresourceRange.baseArrayLayer = 0;
    depth_view_ci.subresourceRange.layerCount = 1;

    vk_result = vkCreateImageView(Ctx->Device, &depth_view_ci, nullptr, &Ctx->SwapchainDepthImageView);
    VerifyVkResult(vk_result);

    for (size_t i = 0; i < Ctx->SwapchainImages.size(); i++) {
        VkImageViewCreateInfo ivci {};
        ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivci.image = Ctx->SwapchainImages[i];
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format = Ctx->SwapchainFormat;
        ivci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.baseMipLevel = 0;
        ivci.subresourceRange.levelCount = 1;
        ivci.subresourceRange.baseArrayLayer = 0;
        ivci.subresourceRange.layerCount = 1;

        vk_result = vkCreateImageView(Ctx->Device, &ivci, nullptr, &Ctx->SwapchainImageViews[i]);
        VerifyVkResult(vk_result);

        const VkImageView framebuffer_attachments[] = {Ctx->SwapchainImageViews[i], Ctx->SwapchainDepthImageView};

        VkFramebufferCreateInfo fci {};
        fci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fci.renderPass = Ctx->RenderPass;
        fci.attachmentCount = 2;
        fci.pAttachments = framebuffer_attachments;
        fci.width = swapchain_extent.width;
        fci.height = swapchain_extent.height;
        fci.layers = 1;

        vk_result = vkCreateFramebuffer(Ctx->Device, &fci, nullptr, &Ctx->Framebuffers[i]);
        VerifyVkResult(vk_result);
    }
}

static void RecreateFrameSyncSemaphores()
{
    FO_STACK_TRACE_ENTRY();

    // Callers must run with the device idle (after vkQueueWaitIdle). Binary semaphores can be left
    // in a stale signaled state after a failed present or a deferred resize; destroying and
    // recreating them guarantees a clean unsignaled state before the next acquire/submit cycle,
    // avoiding double-signal validation errors (VUID-vkAcquireNextImageKHR-semaphore-01779 et al).
    if (Ctx->ImageAvailableSemaphore != nullptr) {
        vkDestroySemaphore(Ctx->Device, Ctx->ImageAvailableSemaphore, nullptr);
        Ctx->ImageAvailableSemaphore = VK_NULL_HANDLE;
    }
    if (Ctx->RenderCompleteSemaphore != nullptr) {
        vkDestroySemaphore(Ctx->Device, Ctx->RenderCompleteSemaphore, nullptr);
        Ctx->RenderCompleteSemaphore = VK_NULL_HANDLE;
    }

    VkSemaphoreCreateInfo sem_ci {};
    sem_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkResult vk_result = vkCreateSemaphore(Ctx->Device, &sem_ci, nullptr, &Ctx->ImageAvailableSemaphore);
    VerifyVkResult(vk_result);
    vk_result = vkCreateSemaphore(Ctx->Device, &sem_ci, nullptr, &Ctx->RenderCompleteSemaphore);
    VerifyVkResult(vk_result);
}

static void BeginFrame()
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(Ctx->Swapchain);
    FO_RUNTIME_ASSERT(Ctx->Device);

    VkResult vk_result = VK_SUCCESS;

    // Single-threaded: wait for all GPU work to finish before reusing command buffer
    vk_result = vkQueueWaitIdle(Ctx->GraphicsQueue);
    VerifyVkResult(vk_result);

    FlushDeferredDestroyResources();

    if (Ctx->PendingSwapchainRecreateSize.has_value()) {
        const auto recreate_size = Ctx->PendingSwapchainRecreateSize.value();
        Ctx->PendingSwapchainRecreateSize.reset();
        RecreateSwapchain({std::max(recreate_size.width, 1), std::max(recreate_size.height, 1)});
        // Device is idle here (vkQueueWaitIdle above); clear any stale semaphore signal left by a
        // deferred resize or a failed present before the next acquire/submit cycle.
        RecreateFrameSyncSemaphores();
    }

    vk_result = vkResetDescriptorPool(Ctx->Device, Ctx->FrameDescriptorPool, 0);
    VerifyVkResult(vk_result);
    Ctx->FrameUniformOffset = 0;

    vk_result = vkAcquireNextImageKHR(Ctx->Device, Ctx->Swapchain, UINT64_MAX, Ctx->ImageAvailableSemaphore, VK_NULL_HANDLE, &Ctx->CurrentSwapchainImageIndex);
    if (vk_result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Acquire failed and the semaphore was NOT signaled, so recreating and re-acquiring on the
        // same (now freshly recreated) semaphore is safe.
        int width;
        int height;
        SDL_GetWindowSizeInPixels(Ctx->SdlWindow.get(), &width, &height);
        RecreateSwapchain({std::max(width, 1), std::max(height, 1)});
        RecreateFrameSyncSemaphores();

        vk_result = vkAcquireNextImageKHR(Ctx->Device, Ctx->Swapchain, UINT64_MAX, Ctx->ImageAvailableSemaphore, VK_NULL_HANDLE, &Ctx->CurrentSwapchainImageIndex);
        VerifyVkResult(vk_result);
    }
    else if (vk_result == VK_SUBOPTIMAL_KHR) {
        // SUBOPTIMAL is a success code: the image WAS acquired and ImageAvailableSemaphore IS
        // signaled. Render/present this frame as-is and defer the recreate to the next BeginFrame
        // (which runs after vkQueueWaitIdle and recreates the semaphores). Re-acquiring on the
        // already-signaled semaphore here would be illegal.
        int width;
        int height;
        SDL_GetWindowSizeInPixels(Ctx->SdlWindow.get(), &width, &height);
        Ctx->PendingSwapchainRecreateSize = isize32 {std::max(width, 1), std::max(height, 1)};
    }
    else {
        VerifyVkResult(vk_result);
    }

    vk_result = vkResetCommandBuffer(Ctx->CommandBuffer, 0);
    VerifyVkResult(vk_result);

    BeginCommandBufferRecording(Ctx->CommandBuffer);

    // Explicitly clear the swapchain image before first render pass begin
    // This ensures valid content for loadOp=LOAD (image starts as UNDEFINED after acquire)
    TransitionColorImage(Ctx->CommandBuffer, Ctx->SwapchainImages[Ctx->CurrentSwapchainImageIndex], Ctx->SwapchainImageLayouts[Ctx->CurrentSwapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    Ctx->SwapchainImageLayouts[Ctx->CurrentSwapchainImageIndex] = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    const VkClearColorValue frame_clear = Ctx->ClearColor;
    VkImageSubresourceRange clear_range {};
    clear_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clear_range.baseMipLevel = 0;
    clear_range.levelCount = 1;
    clear_range.baseArrayLayer = 0;
    clear_range.layerCount = 1;
    vkCmdClearColorImage(Ctx->CommandBuffer, Ctx->SwapchainImages[Ctx->CurrentSwapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &frame_clear, 1, &clear_range);

    TransitionColorImage(Ctx->CommandBuffer, Ctx->SwapchainImages[Ctx->CurrentSwapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    Ctx->SwapchainImageLayouts[Ctx->CurrentSwapchainImageIndex] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    BeginCurrentRenderPass();
}

static void EndFrame()
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(Ctx->Swapchain);
    FO_RUNTIME_ASSERT(Ctx->Device);

    VkResult vk_result = VK_SUCCESS;

    EndCurrentRenderPass();

    if (Ctx->CurrentRenderTarget == nullptr && Ctx->CurrentSwapchainImageIndex < Ctx->SwapchainImages.size() && Ctx->CurrentSwapchainImageIndex < Ctx->SwapchainImageLayouts.size()) {
        TransitionColorImage(Ctx->CommandBuffer, Ctx->SwapchainImages[Ctx->CurrentSwapchainImageIndex], Ctx->SwapchainImageLayouts[Ctx->CurrentSwapchainImageIndex], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        Ctx->SwapchainImageLayouts[Ctx->CurrentSwapchainImageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }

    // End command buffer
    vk_result = vkEndCommandBuffer(Ctx->CommandBuffer);
    VerifyVkResult(vk_result);

    // Submit to graphics queue.
    // The acquire semaphore must be waited at the stage of the FIRST use of the freshly acquired
    // swapchain image. BeginFrame's first commands against it are a layout transition + vkCmdClearColorImage
    // which run at TRANSFER; later draws run at COLOR_ATTACHMENT_OUTPUT. Cover both so the GPU never
    // touches the image before the presentation engine has released it.
    constexpr VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &Ctx->ImageAvailableSemaphore;
    submit_info.pWaitDstStageMask = &wait_stage;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &Ctx->CommandBuffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &Ctx->RenderCompleteSemaphore;

    vk_result = vkQueueSubmit(Ctx->GraphicsQueue, 1, &submit_info, VK_NULL_HANDLE);
    VerifyVkResult(vk_result);

    // Present to screen
    VkPresentInfoKHR info {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &Ctx->RenderCompleteSemaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &Ctx->Swapchain;
    info.pImageIndices = &Ctx->CurrentSwapchainImageIndex;

    vk_result = vkQueuePresentKHR(Ctx->GraphicsQueue, &info);
    if (vk_result == VK_ERROR_OUT_OF_DATE_KHR || vk_result == VK_SUBOPTIMAL_KHR) {
        // Defer the recreate to the next BeginFrame rather than recreating inline: a failed present
        // may leave RenderCompleteSemaphore in an ambiguous state, and BeginFrame recreates both
        // sync semaphores after vkQueueWaitIdle, guaranteeing a clean state for the next submit.
        int width;
        int height;
        SDL_GetWindowSizeInPixels(Ctx->SdlWindow.get(), &width, &height);
        Ctx->PendingSwapchainRecreateSize = isize32 {std::max(width, 1), std::max(height, 1)};
    }
    else {
        VerifyVkResult(vk_result);
    }

    // Begin recording the next frame immediately (command buffer always recording, like OpenGL/D3D)
    BeginFrame();
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
        img_barrier.srcAccessMask = 0;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
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
        img_barrier.srcAccessMask = 0;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
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
        img_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
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

static void BeginCurrentRenderPass()
{
    FO_STACK_TRACE_ENTRY();

    VkRenderPassBeginInfo rp_begin {};
    rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin.renderPass = Ctx->RenderPass;
    rp_begin.renderArea.offset = {.x = 0, .y = 0};

    VkClearValue clear_values[2] {};
    clear_values[0].color = Ctx->ClearColor;
    clear_values[1].depthStencil = {.depth = 1.0f, .stencil = 0};
    rp_begin.clearValueCount = 2;
    rp_begin.pClearValues = clear_values;

    if (Ctx->CurrentRenderTarget != nullptr) {
        auto* vk_tex = static_cast<Vulkan_Texture*>(Ctx->CurrentRenderTarget.get()); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        EnsureTextureRenderTargetResources(vk_tex);
        FO_RUNTIME_ASSERT(vk_tex->TextureFramebuffer != nullptr);
        FO_RUNTIME_ASSERT(vk_tex->TextureImage != nullptr);

        TransitionColorImage(Ctx->CommandBuffer, vk_tex->TextureImage, vk_tex->TextureImageLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        vk_tex->TextureImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        rp_begin.framebuffer = vk_tex->TextureFramebuffer;
        rp_begin.renderArea.extent = {.width = numeric_cast<uint32_t>(vk_tex->Size.width), .height = numeric_cast<uint32_t>(vk_tex->Size.height)};
        Ctx->ViewPort = irect32 {{0, 0}, {vk_tex->Size.width, vk_tex->Size.height}};
    }
    else {
        FO_RUNTIME_ASSERT(Ctx->CurrentSwapchainImageIndex < Ctx->Framebuffers.size());
        FO_RUNTIME_ASSERT(Ctx->CurrentSwapchainImageIndex < Ctx->SwapchainImageLayouts.size());

        TransitionColorImage(Ctx->CommandBuffer, Ctx->SwapchainImages[Ctx->CurrentSwapchainImageIndex], Ctx->SwapchainImageLayouts[Ctx->CurrentSwapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        Ctx->SwapchainImageLayouts[Ctx->CurrentSwapchainImageIndex] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        rp_begin.framebuffer = Ctx->Framebuffers[Ctx->CurrentSwapchainImageIndex];
        rp_begin.renderArea.offset = {.x = 0, .y = 0};

        rp_begin.renderArea.extent = {.width = numeric_cast<uint32_t>(Ctx->SwapchainSize.width), .height = numeric_cast<uint32_t>(Ctx->SwapchainSize.height)};
    }

    vkCmdBeginRenderPass(Ctx->CommandBuffer, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
    ApplyViewportAndScissor();
}

static void EndCurrentRenderPass()
{
    FO_STACK_TRACE_ENTRY();

    vkCmdEndRenderPass(Ctx->CommandBuffer);
}

static void ApplyViewportAndScissor()
{
    FO_STACK_TRACE_ENTRY();

    VkViewport viewport {};
    viewport.x = numeric_cast<float32_t>(Ctx->ViewPort.x);
    viewport.y = numeric_cast<float32_t>(Ctx->ViewPort.y);
    viewport.width = numeric_cast<float32_t>(Ctx->ViewPort.width);
    viewport.height = numeric_cast<float32_t>(Ctx->ViewPort.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(Ctx->CommandBuffer, 0, 1, &viewport);

    if (Ctx->ScissorEnabled) {
        VkRect2D scissor_rect {};

        int32_t left;
        int32_t top;
        int32_t right;
        int32_t bottom;

        if (Ctx->ViewPort.width != Ctx->TargetSize.width || Ctx->ViewPort.height != Ctx->TargetSize.height) {
            const auto x_ratio = checked_div<float32_t>(numeric_cast<float32_t>(Ctx->ViewPort.width), numeric_cast<float32_t>(Ctx->TargetSize.width));
            const auto y_ratio = checked_div<float32_t>(numeric_cast<float32_t>(Ctx->ViewPort.height), numeric_cast<float32_t>(Ctx->TargetSize.height));

            left = Ctx->ViewPort.x + iround<int32_t>(numeric_cast<float32_t>(Ctx->ScissorRect.x) * x_ratio);
            top = Ctx->ViewPort.y + iround<int32_t>(numeric_cast<float32_t>(Ctx->ScissorRect.y) * y_ratio);
            right = Ctx->ViewPort.x + iround<int32_t>(numeric_cast<float32_t>(Ctx->ScissorRect.x + Ctx->ScissorRect.width) * x_ratio);
            bottom = Ctx->ViewPort.y + iround<int32_t>(numeric_cast<float32_t>(Ctx->ScissorRect.y + Ctx->ScissorRect.height) * y_ratio);
        }
        else {
            left = Ctx->ViewPort.x + Ctx->ScissorRect.x;
            top = Ctx->ViewPort.y + Ctx->ScissorRect.y;
            right = Ctx->ViewPort.x + Ctx->ScissorRect.x + Ctx->ScissorRect.width;
            bottom = Ctx->ViewPort.y + Ctx->ScissorRect.y + Ctx->ScissorRect.height;
        }

        // Vulkan requires the scissor to satisfy offset >= 0 and offset + extent <= framebuffer
        // dimensions. OpenGL/D3D clamp implicitly; replicate that here against the active render
        // target's framebuffer size (the full swapchain/texture extent, not the letterboxed viewport).
        const int32_t rt_w = Ctx->CurrentRenderTarget != nullptr ? static_cast<const Vulkan_Texture*>(Ctx->CurrentRenderTarget.get())->Size.width : Ctx->SwapchainSize.width; // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        const int32_t rt_h = Ctx->CurrentRenderTarget != nullptr ? static_cast<const Vulkan_Texture*>(Ctx->CurrentRenderTarget.get())->Size.height : Ctx->SwapchainSize.height; // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        left = std::clamp(left, 0, rt_w);
        top = std::clamp(top, 0, rt_h);
        right = std::clamp(right, left, rt_w);
        bottom = std::clamp(bottom, top, rt_h);

        scissor_rect.offset = {.x = left, .y = top};
        scissor_rect.extent = {.width = numeric_cast<uint32_t>(right - left), .height = numeric_cast<uint32_t>(bottom - top)};
        vkCmdSetScissor(Ctx->CommandBuffer, 0, 1, &scissor_rect);
    }
    else {
        VkRect2D scissor_rect {};
        scissor_rect.offset = {.x = numeric_cast<int32_t>(Ctx->ViewPort.x), .y = numeric_cast<int32_t>(Ctx->ViewPort.y)};
        scissor_rect.extent = {.width = numeric_cast<uint32_t>(Ctx->ViewPort.width), .height = numeric_cast<uint32_t>(Ctx->ViewPort.height)};
        vkCmdSetScissor(Ctx->CommandBuffer, 0, 1, &scissor_rect);
    }
}

static void EnsureTextureRenderTargetResources(Vulkan_Texture* vk_tex)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(vk_tex != nullptr);
    FO_RUNTIME_ASSERT(vk_tex->TextureImage != nullptr);
    FO_RUNTIME_ASSERT(vk_tex->TextureImageView != nullptr);

    VkResult vk_result = VK_SUCCESS;

    if (vk_tex->DepthImage == nullptr) {
        AllocateImage(numeric_cast<uint32_t>(vk_tex->Size.width), numeric_cast<uint32_t>(vk_tex->Size.height), VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vk_tex->DepthImage, vk_tex->DepthImageMemory);

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

        vk_result = vkCreateImageView(Ctx->Device, &depth_view_ci, nullptr, &vk_tex->DepthImageView);
        VerifyVkResult(vk_result);
    }

    if (vk_tex->TextureFramebuffer == nullptr) {
        const VkImageView framebuffer_attachments[] = {vk_tex->TextureImageView, vk_tex->DepthImageView};

        VkFramebufferCreateInfo fci {};
        fci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fci.renderPass = Ctx->RenderPass;
        fci.attachmentCount = 2;
        fci.pAttachments = framebuffer_attachments;
        fci.width = numeric_cast<uint32_t>(vk_tex->Size.width);
        fci.height = numeric_cast<uint32_t>(vk_tex->Size.height);
        fci.layers = 1;

        vk_result = vkCreateFramebuffer(Ctx->Device, &fci, nullptr, &vk_tex->TextureFramebuffer);
        VerifyVkResult(vk_result);
    }
}

void Vulkan_Renderer::Present()
{
    FO_STACK_TRACE_ENTRY();

    EndFrame();
}

void Vulkan_Renderer::SetRenderTarget(RenderTexture* tex)
{
    FO_STACK_TRACE_ENTRY();

    if (Ctx->CurrentRenderTarget == tex) {
        return;
    }

    EndCurrentRenderPass();

    if (Ctx->CurrentRenderTarget != nullptr) {
        auto* prev_tex = static_cast<Vulkan_Texture*>(Ctx->CurrentRenderTarget.get()); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        TransitionColorImage(Ctx->CommandBuffer, prev_tex->TextureImage, prev_tex->TextureImageLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        prev_tex->TextureImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    Ctx->CurrentRenderTarget = tex;

    if (Ctx->CurrentRenderTarget == nullptr) {
        const auto back_buf_size = Ctx->SwapchainSize;
        const auto back_buf_aspect = checked_div<float32_t>(numeric_cast<float32_t>(back_buf_size.width), numeric_cast<float32_t>(back_buf_size.height));
        const auto screen_aspect = checked_div<float32_t>(numeric_cast<float32_t>(Ctx->Settings->ScreenWidth), numeric_cast<float32_t>(Ctx->Settings->ScreenHeight));
        const auto fit_width = iround<int32_t>(screen_aspect <= back_buf_aspect ? numeric_cast<float32_t>(back_buf_size.height) * screen_aspect : numeric_cast<float32_t>(back_buf_size.height) * back_buf_aspect);
        const auto fit_height = iround<int32_t>(screen_aspect <= back_buf_aspect ? numeric_cast<float32_t>(back_buf_size.width) / back_buf_aspect : numeric_cast<float32_t>(back_buf_size.width) / screen_aspect);

        const auto vp_ox = (back_buf_size.width - fit_width) / 2;
        const auto vp_oy = (back_buf_size.height - fit_height) / 2;
        Ctx->ViewPort = irect32 {vp_ox, vp_oy, fit_width, fit_height};
        Ctx->TargetSize = {Ctx->Settings->ScreenWidth, Ctx->Settings->ScreenHeight};
    }
    else {
        const auto* vk_tex = static_cast<const Vulkan_Texture*>(Ctx->CurrentRenderTarget.get()); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        Ctx->ViewPort = irect32 {0, 0, vk_tex->Size.width, vk_tex->Size.height};
        Ctx->TargetSize = vk_tex->Size;
    }

    Ctx->ProjectionMatrixColMaj = CreateOrthoMatrix(0.0f, numeric_cast<float32_t>(Ctx->TargetSize.width), numeric_cast<float32_t>(Ctx->TargetSize.height), 0.0f, -10.0f, 10.0f);
    Ctx->ProjectionMatrixColMaj = glm::transpose(Ctx->ProjectionMatrixColMaj);

    BeginCurrentRenderPass();
}

void Vulkan_Renderer::ClearRenderTarget(optional<ucolor> color, bool depth, bool stencil)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(stencil);

    if (!color.has_value() && !depth) {
        return;
    }

    vector<VkClearAttachment> attachments;
    attachments.reserve(2);

    if (color.has_value()) {
        Ctx->ClearColor.float32[0] = numeric_cast<float32_t>(color.value().comp.r) / 255.0f;
        Ctx->ClearColor.float32[1] = numeric_cast<float32_t>(color.value().comp.g) / 255.0f;
        Ctx->ClearColor.float32[2] = numeric_cast<float32_t>(color.value().comp.b) / 255.0f;
        Ctx->ClearColor.float32[3] = numeric_cast<float32_t>(color.value().comp.a) / 255.0f;

        VkClearAttachment color_attachment {};
        color_attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        color_attachment.colorAttachment = 0;
        color_attachment.clearValue.color = Ctx->ClearColor;
        attachments.emplace_back(color_attachment);
    }

    if (depth) {
        VkClearAttachment depth_attachment {};
        depth_attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depth_attachment.clearValue.depthStencil = {.depth = 1.0f, .stencil = 0};
        attachments.emplace_back(depth_attachment);
    }

    VkClearRect clear_rect {};
    clear_rect.rect.offset = {.x = numeric_cast<int32_t>(Ctx->ViewPort.x), .y = numeric_cast<int32_t>(Ctx->ViewPort.y)};
    clear_rect.rect.extent = {.width = numeric_cast<uint32_t>(Ctx->ViewPort.width), .height = numeric_cast<uint32_t>(Ctx->ViewPort.height)};
    clear_rect.baseArrayLayer = 0;
    clear_rect.layerCount = 1;

    vkCmdClearAttachments(Ctx->CommandBuffer, numeric_cast<uint32_t>(attachments.size()), attachments.data(), 1, &clear_rect);
}

void Vulkan_Renderer::EnableScissor(irect32 rect)
{
    FO_STACK_TRACE_ENTRY();

    Ctx->ScissorRect = rect;
    Ctx->ScissorEnabled = true;

    ApplyViewportAndScissor();
}

void Vulkan_Renderer::DisableScissor()
{
    FO_STACK_TRACE_ENTRY();

    Ctx->ScissorEnabled = false;

    ApplyViewportAndScissor();
}

void Vulkan_Renderer::OnResizeWindow(isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    Ctx->ViewPort = irect32 {{0, 0}, {std::max(size.width, 1), std::max(size.height, 1)}};
    Ctx->PendingSwapchainRecreateSize = isize32 {std::max(size.width, 1), std::max(size.height, 1)};
}

static void DestroyResourceSafe(VkBuffer& buffer)
{
    FO_STACK_TRACE_ENTRY();

    if (buffer != VK_NULL_HANDLE) {
        Ctx->DeferredDestroyBuffers.emplace_back(buffer);
        buffer = VK_NULL_HANDLE;
    }
}

static void DestroyResourceSafe(VkDeviceMemory& memory)
{
    FO_STACK_TRACE_ENTRY();

    if (memory != VK_NULL_HANDLE) {
        Ctx->DeferredDestroyMemories.emplace_back(memory);
        memory = VK_NULL_HANDLE;
    }
}

static void DestroyResourceSafe(VkImage& image)
{
    FO_STACK_TRACE_ENTRY();

    if (image != VK_NULL_HANDLE) {
        Ctx->DeferredDestroyImages.emplace_back(image);
        image = VK_NULL_HANDLE;
    }
}

static void DestroyResourceSafe(VkImageView& image_view)
{
    FO_STACK_TRACE_ENTRY();

    if (image_view != VK_NULL_HANDLE) {
        Ctx->DeferredDestroyImageViews.emplace_back(image_view);
        image_view = VK_NULL_HANDLE;
    }
}

static void DestroyResourceSafe(VkFramebuffer& framebuffer)
{
    FO_STACK_TRACE_ENTRY();

    if (framebuffer != VK_NULL_HANDLE) {
        Ctx->DeferredDestroyFramebuffers.emplace_back(framebuffer);
        framebuffer = VK_NULL_HANDLE;
    }
}

static void FlushDeferredDestroyResources()
{
    FO_STACK_TRACE_ENTRY();

    for (auto* framebuffer : Ctx->DeferredDestroyFramebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(Ctx->Device, framebuffer, nullptr);
        }
    }
    Ctx->DeferredDestroyFramebuffers.clear();

    for (auto* image_view : Ctx->DeferredDestroyImageViews) {
        if (image_view != VK_NULL_HANDLE) {
            vkDestroyImageView(Ctx->Device, image_view, nullptr);
        }
    }
    Ctx->DeferredDestroyImageViews.clear();

    for (auto* image : Ctx->DeferredDestroyImages) {
        if (image != VK_NULL_HANDLE) {
            vkDestroyImage(Ctx->Device, image, nullptr);
        }
    }
    Ctx->DeferredDestroyImages.clear();

    for (auto* buffer : Ctx->DeferredDestroyBuffers) {
        if (buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(Ctx->Device, buffer, nullptr);
        }
    }
    Ctx->DeferredDestroyBuffers.clear();

    for (auto* memory : Ctx->DeferredDestroyMemories) {
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(Ctx->Device, memory, nullptr);
        }
    }
    Ctx->DeferredDestroyMemories.clear();
}

FO_END_NAMESPACE

#endif // FO_HAVE_VULKAN
