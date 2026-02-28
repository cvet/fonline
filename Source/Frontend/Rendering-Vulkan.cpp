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

static raw_ptr<GlobalSettings> Settings {};
static raw_ptr<SDL_Window> SdlWindow {};
static VkInstance Instance {};
static VkPhysicalDevice PhysicalDevice {};
static VkDevice Device {};
static VkQueue GraphicsQueue {};
static VkSurfaceKHR Surface {};
static uint32_t GraphicsFamilyIndex {};
static VkSwapchainKHR Swapchain {};
static vector<VkImage> SwapchainImages;
static vector<VkImageLayout> SwapchainImageLayouts;
static vector<VkImageView> SwapchainImageViews;
static VkFormat SwapchainFormat {};
static isize32 SwapchainSize {};
static VkRenderPass RenderPass {};
static vector<VkFramebuffer> Framebuffers;
static VkImage SwapchainDepthImage {};
static VkImageView SwapchainDepthImageView {};
static VkDeviceMemory SwapchainDepthImageMemory {};
static VkCommandPool CommandPool {};
static VkCommandBuffer CommandBuffer {}; // Single command buffer (single-threaded)
static VkSemaphore ImageAvailableSemaphore {}; // Signaled when image acquired
static VkSemaphore RenderCompleteSemaphore {}; // Signaled when render done
static irect32 ViewPort {};
static isize32 TargetSize {};
static mat44 ProjectionMatrixColMaj {};
static unique_ptr<RenderTexture> DummyTexture {};
static VkClearColorValue ClearColor {{0.0f, 0.0f, 0.0f, 1.0f}};
static VkCommandBuffer StagingCommandBuffer {};
static vector<tuple<VkBuffer, VkDeviceMemory>> StagingBuffers;
static VkDescriptorSetLayout TextureDescriptorSetLayout {};
static VkDescriptorSetLayout UniformDescriptorSetLayout {};
static VkDescriptorPool FrameDescriptorPool {};
static VkBuffer FrameUniformBuffer {};
static VkDeviceMemory FrameUniformBufferMemory {};
static void* FrameUniformBufferMapped = nullptr;
static size_t FrameUniformOffset = 0;
static size_t FrameUniformBufferSize = numeric_cast<size_t>(16 * 1024 * 1024); // 16 MB
static VkSampler LinearSampler {};
static VkSampler PointSampler {};
static raw_ptr<RenderTexture> CurrentRenderTarget {}; // Current render target (nullptr = swapchain)
static irect32 ScissorRect {};
static bool ScissorEnabled = false;
static uint32_t CurrentSwapchainImageIndex {};
static optional<isize32> PendingSwapchainRecreateSize;
static vector<VkBuffer> DeferredDestroyBuffers;
static vector<VkDeviceMemory> DeferredDestroyMemories;
static vector<VkImage> DeferredDestroyImages;
static vector<VkImageView> DeferredDestroyImageViews;
static vector<VkFramebuffer> DeferredDestroyFramebuffers;

class Vulkan_Texture;

static void RecreateSwapchain(isize32 size);
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

    auto vk_result = vkQueueSubmit(GraphicsQueue, 1, &submit_info, VK_NULL_HANDLE);
    VerifyVkResult(vk_result);

    vk_result = vkQueueWaitIdle(GraphicsQueue);
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
    // Per-pass pipelines and descriptor sets
    VkPipeline Pipeline[EFFECT_MAX_PASSES][5] {};
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

    vk_result = vkCreateBuffer(Device, &buffer_ci, nullptr, &buffer);
    VerifyVkResult(vk_result);

    VkMemoryRequirements mem_req {};
    vkGetBufferMemoryRequirements(Device, buffer, &mem_req);

    // Find suitable memory type
    optional<uint32_t> mem_type_idx;
    VkPhysicalDeviceMemoryProperties mem_properties {};
    vkGetPhysicalDeviceMemoryProperties(PhysicalDevice, &mem_properties);

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

    vk_result = vkAllocateMemory(Device, &alloc_info, nullptr, &memory);
    VerifyVkResult(vk_result);

    vk_result = vkBindBufferMemory(Device, buffer, memory, 0);
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

    vk_result = vkCreateImage(Device, &image_ci, nullptr, &image);
    VerifyVkResult(vk_result);

    VkMemoryRequirements mem_req {};
    vkGetImageMemoryRequirements(Device, image, &mem_req);

    // Find suitable memory type
    optional<uint32_t> mem_type_idx;
    VkPhysicalDeviceMemoryProperties mem_properties {};
    vkGetPhysicalDeviceMemoryProperties(PhysicalDevice, &mem_properties);

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

    vk_result = vkAllocateMemory(Device, &alloc_info, nullptr, &memory);
    VerifyVkResult(vk_result);

    vk_result = vkBindImageMemory(Device, image, memory, 0);
    VerifyVkResult(vk_result);
}

Vulkan_Texture::Vulkan_Texture(isize32 size, bool linear_filtered, bool with_depth) :
    RenderTexture(size, linear_filtered, with_depth)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(Device);

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

    vk_result = vkCreateImageView(Device, &image_view_ci, nullptr, &TextureImageView);
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

        vk_result = vkCreateImageView(Device, &depth_view_ci, nullptr, &DepthImageView);
        VerifyVkResult(vk_result);
    }

    // Put texture into a valid sampling layout immediately.
    // Some textures are sampled before any UpdateTextureRegion call.
    BeginCommandBufferRecording(StagingCommandBuffer);

    TransitionColorImage(StagingCommandBuffer, TextureImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    EndCommandBufferRecording(StagingCommandBuffer);
    SubmitCommandBufferAndWait(StagingCommandBuffer);

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

    FO_RUNTIME_ASSERT(Device);
    FO_RUNTIME_ASSERT(TextureImage);

    VkResult vk_result = VK_SUCCESS;
    vector<ucolor> tex_region;
    const VkDeviceSize region_size = size.square() * sizeof(ucolor);

    // Create staging buffer for reading from GPU
    VkBuffer staging_buf {};
    VkDeviceMemory staging_mem {};
    AllocateBuffer(region_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, staging_buf, staging_mem);

    // Record copy commands using standalone staging command buffer
    BeginCommandBufferRecording(StagingCommandBuffer);

    const auto old_layout = TextureImageLayout;
    TransitionColorImage(StagingCommandBuffer, TextureImage, old_layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

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

    vkCmdCopyImageToBuffer(StagingCommandBuffer, TextureImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging_buf, 1, &region);

    TransitionColorImage(StagingCommandBuffer, TextureImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, old_layout);

    EndCommandBufferRecording(StagingCommandBuffer);
    SubmitCommandBufferAndWait(StagingCommandBuffer);

    // Read data from staging buffer
    void* map_data;
    vk_result = vkMapMemory(Device, staging_mem, 0, region_size, 0, &map_data);
    VerifyVkResult(vk_result);
    tex_region.resize(size.square());
    MemCopy(tex_region.data(), map_data, region_size);
    vkUnmapMemory(Device, staging_mem);

    // Swizzle B↔R: VK_FORMAT_B8G8R8A8_UNORM stores {B,G,R,A} but ucolor expects {R,G,B,A}
    {
        auto* pixels = reinterpret_cast<uint8_t*>(tex_region.data());
        for (size_t i = 0; i < tex_region.size(); i++) {
            std::swap(pixels[i * 4 + 0], pixels[i * 4 + 2]);
        }
    }

    // Cleanup
    vkDestroyBuffer(Device, staging_buf, nullptr);
    vkFreeMemory(Device, staging_mem, nullptr);

    ResetCommandBufferRecording(StagingCommandBuffer);

    return tex_region;
}

void Vulkan_Texture::UpdateTextureRegion(ipos32 pos, isize32 size, const ucolor* data, bool use_dest_pitch)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(Device);
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
    VkResult vk_result = vkMapMemory(Device, staging_mem, 0, total_data_size, 0, &mapped_data);
    VerifyVkResult(vk_result);

    if (use_dest_pitch) {
        // Data has full destination texture pitch - copy row by row respecting the stride
        uint8_t* dst = static_cast<uint8_t*>(mapped_data);
        const uint8_t* src = reinterpret_cast<const uint8_t*>(data);

        for (int32 y = 0; y < size.height; y++) {
            MemCopy(dst, src, row_bytes);
            dst += data_stride;
            src += data_stride;
        }
    }
    else {
        // Data is tightly packed for the region - direct copy
        MemCopy(mapped_data, data, row_bytes * size.height);
    }

    // Swizzle R↔B: ucolor stores {R,G,B,A} but VK_FORMAT_B8G8R8A8_UNORM expects {B,G,R,A}
    {
        uint8_t* pixels = static_cast<uint8_t*>(mapped_data);
        const size_t pixel_count = numeric_cast<size_t>(total_data_size) / sizeof(ucolor);
        for (size_t i = 0; i < pixel_count; i++) {
            std::swap(pixels[i * 4 + 0], pixels[i * 4 + 2]); // Swap R and B
        }
    }

    vkUnmapMemory(Device, staging_mem);

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

        vk_result = vkCreateImageView(Device, &image_view_ci, nullptr, &TextureImageView);
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

            vk_result = vkCreateImageView(Device, &depth_view_ci, nullptr, &DepthImageView);
            VerifyVkResult(vk_result);
        }
    }

    // Record copy commands using standalone staging command buffer
    BeginCommandBufferRecording(StagingCommandBuffer);

    const auto old_layout = TextureImageLayout;
    TransitionColorImage(StagingCommandBuffer, TextureImage, old_layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

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

    vkCmdCopyBufferToImage(StagingCommandBuffer, staging_buf, TextureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    TransitionColorImage(StagingCommandBuffer, TextureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    TextureImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    EndCommandBufferRecording(StagingCommandBuffer);
    SubmitCommandBufferAndWait(StagingCommandBuffer);

    // Free staging buffer after GPU is done
    vkDestroyBuffer(Device, staging_buf, nullptr);
    vkFreeMemory(Device, staging_mem, nullptr);

    ResetCommandBufferRecording(StagingCommandBuffer);
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

    FO_RUNTIME_ASSERT(Device);

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
            vk_result = vkMapMemory(Device, VertexBufferMemory, 0, vert_size, 0, &data);
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
            vkUnmapMemory(Device, VertexBufferMemory);
        }
        else {
            // Static buffers: use staging copy to GPU-local memory
            DestroyResourceSafe(VertexBuffer);
            DestroyResourceSafe(VertexBufferMemory);

            VkBuffer staging_vert_buf {};
            VkDeviceMemory staging_vert_mem {};

            AllocateBuffer(vert_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_vert_buf, staging_vert_mem);
            StagingBuffers.emplace_back(staging_vert_buf, staging_vert_mem);

            void* data {};
            vk_result = vkMapMemory(Device, staging_vert_mem, 0, vert_size, 0, &data);
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
            vkUnmapMemory(Device, staging_vert_mem);

            AllocateBuffer(vert_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VertexBuffer, VertexBufferMemory);

            VkBufferCopy copy_region {};
            copy_region.size = vert_size;

            BeginCommandBufferRecording(StagingCommandBuffer);

            vkCmdCopyBuffer(StagingCommandBuffer, staging_vert_buf, VertexBuffer, 1, &copy_region);

            EndCommandBufferRecording(StagingCommandBuffer);
            SubmitCommandBufferAndWait(StagingCommandBuffer);

            for (const auto& [sb, sm] : StagingBuffers) {
                if (sb != nullptr) {
                    vkDestroyBuffer(Device, sb, nullptr);
                }
                if (sm != nullptr) {
                    vkFreeMemory(Device, sm, nullptr);
                }
            }
            StagingBuffers.clear();

            ResetCommandBufferRecording(StagingCommandBuffer);
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
            vk_result = vkMapMemory(Device, IndexBufferMemory, 0, idx_size, 0, &data);
            VerifyVkResult(vk_result);
            MemCopy(data, Indices.data(), idx_size);
            vkUnmapMemory(Device, IndexBufferMemory);
        }
        else {
            // Static buffers: use staging copy to GPU-local memory
            DestroyResourceSafe(IndexBuffer);
            DestroyResourceSafe(IndexBufferMemory);

            VkBuffer staging_idx_buf {};
            VkDeviceMemory staging_idx_mem {};
            AllocateBuffer(idx_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_idx_buf, staging_idx_mem);
            StagingBuffers.emplace_back(staging_idx_buf, staging_idx_mem);

            void* data {};
            vk_result = vkMapMemory(Device, staging_idx_mem, 0, idx_size, 0, &data);
            VerifyVkResult(vk_result);
            MemCopy(data, Indices.data(), idx_size);
            vkUnmapMemory(Device, staging_idx_mem);

            AllocateBuffer(idx_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, IndexBuffer, IndexBufferMemory);

            VkBufferCopy copy_region {};
            copy_region.size = idx_size;

            BeginCommandBufferRecording(StagingCommandBuffer);

            vkCmdCopyBuffer(StagingCommandBuffer, staging_idx_buf, IndexBuffer, 1, &copy_region);

            EndCommandBufferRecording(StagingCommandBuffer);
            SubmitCommandBufferAndWait(StagingCommandBuffer);

            for (const auto& [sb, sm] : StagingBuffers) {
                if (sb != nullptr) {
                    vkDestroyBuffer(Device, sb, nullptr);
                }
                if (sm != nullptr) {
                    vkFreeMemory(Device, sm, nullptr);
                }
            }
            StagingBuffers.clear();

            ResetCommandBufferRecording(StagingCommandBuffer);
        }
    }
}

Vulkan_Effect::~Vulkan_Effect()
{
    FO_STACK_TRACE_ENTRY();

    for (size_t pass = 0; pass < EFFECT_MAX_PASSES; pass++) {
        if (VertexShaderModule[pass] != nullptr) {
            vkDestroyShaderModule(Device, VertexShaderModule[pass], nullptr);
            VertexShaderModule[pass] = VK_NULL_HANDLE;
        }
        if (FragmentShaderModule[pass] != nullptr) {
            vkDestroyShaderModule(Device, FragmentShaderModule[pass], nullptr);
            FragmentShaderModule[pass] = VK_NULL_HANDLE;
        }
        for (size_t prim = 0; prim < 5; prim++) {
            if (Pipeline[pass][prim] != nullptr) {
                vkDestroyPipeline(Device, Pipeline[pass][prim], nullptr);
                Pipeline[pass][prim] = VK_NULL_HANDLE;
            }
        }
    }

    if (PipelineLayout != nullptr) {
        vkDestroyPipelineLayout(Device, PipelineLayout, nullptr);
        PipelineLayout = VK_NULL_HANDLE;
    }
}

void Vulkan_Effect::DrawBuffer(RenderDrawBuffer* dbuf, size_t start_index, optional<size_t> indices_to_draw, const RenderTexture* custom_tex)
{
    FO_STACK_TRACE_ENTRY();

    if (dbuf == nullptr || Device == nullptr) {
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
        main_tex = static_cast<Vulkan_Texture*>(DummyTexture.get()); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
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
        main_tex = static_cast<Vulkan_Texture*>(DummyTexture.get()); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }
#endif

    if (_needProjBuf && !ProjBuf.has_value()) {
        auto& proj_buf = ProjBuf = ProjBuffer();
        MemCopy(proj_buf->ProjMatrix, ProjectionMatrixColMaj[0], 16 * sizeof(float32));
    }

    if (_needMainTexBuf && !MainTexBuf.has_value()) {
        auto& main_tex_buf = MainTexBuf = MainTexBuffer();
        MemCopy(main_tex_buf->MainTexSize, main_tex->SizeData, 4 * sizeof(float32));
    }

    Vulkan_Texture* egg_tex;
    if (EggTex) {
        egg_tex = static_cast<Vulkan_Texture*>(const_cast<RenderTexture*>(EggTex.get())); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast,cppcoreguidelines-pro-type-const-cast)
    }
    else {
        egg_tex = static_cast<Vulkan_Texture*>(DummyTexture.get()); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    }

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
    vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &vk_dbuf->VertexBuffer, offsets);

    // Process each pass
    for (size_t pass = 0; pass < _passCount; pass++) {
        const auto prim_type = _usage == EffectUsage::Primitive ? vk_dbuf->PrimType : RenderPrimitiveType::TriangleList;
        const auto prim_index = static_cast<size_t>(prim_type);
        FO_RUNTIME_ASSERT(prim_index < 5);

        // Bind pipeline for current pass
        if (Pipeline[pass][prim_index] != nullptr) {
            vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline[pass][prim_index]);
        }

        // Allocate descriptor sets for this draw call
        VkDescriptorSet texture_set = VK_NULL_HANDLE;
        VkDescriptorSet uniform_set = VK_NULL_HANDLE;

        VkDescriptorSetAllocateInfo alloc_info {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = FrameDescriptorPool;
        alloc_info.descriptorSetCount = 1;

        if (TextureDescriptorSetLayout != nullptr) {
            alloc_info.pSetLayouts = &TextureDescriptorSetLayout;
            const auto vk_result = vkAllocateDescriptorSets(Device, &alloc_info, &texture_set);
            VerifyVkResult(vk_result);
        }

        if (UniformDescriptorSetLayout != nullptr) {
            alloc_info.pSetLayouts = &UniformDescriptorSetLayout;
            const auto vk_result = vkAllocateDescriptorSets(Device, &alloc_info, &uniform_set);
            VerifyVkResult(vk_result);
        }

        // Update and bind per-pass texture descriptor set (set = 1)
        if (texture_set != VK_NULL_HANDLE && this->PipelineLayout != nullptr) {
            VkDescriptorImageInfo image_infos[16];
            VkWriteDescriptorSet writes[16];
            size_t write_count = 0;

            const auto append_sampler = [&](int32 binding, Vulkan_Texture* tex) {
                if (binding < 0 || tex == nullptr || tex->TextureImageView == nullptr) {
                    return;
                }

                FO_RUNTIME_ASSERT(write_count < 16);

                auto& image_info = image_infos[write_count];
                image_info.sampler = tex->LinearFiltered ? LinearSampler : PointSampler;
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
            append_sampler(_posEggTex[pass], egg_tex);

#if FO_ENABLE_3D
            for (size_t model_tex_index = 0; model_tex_index < MODEL_MAX_TEXTURES; model_tex_index++) {
                auto* model_tex = static_cast<Vulkan_Texture*>(ModelTex[model_tex_index] ? ModelTex[model_tex_index].get() : DummyTexture.get()); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                append_sampler(_posModelTex[pass][model_tex_index], model_tex);
            }
#endif

            if (write_count > 0) {
                vkUpdateDescriptorSets(Device, numeric_cast<uint32_t>(write_count), writes, 0, nullptr);
            }

            vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, this->PipelineLayout, 1, 1, &texture_set, 0, nullptr);
        }

        // Update and bind per-pass uniform descriptor set (set = 0)
        if (uniform_set != VK_NULL_HANDLE && this->PipelineLayout != nullptr) {
            VkDescriptorBufferInfo buffer_infos[16];
            VkWriteDescriptorSet writes[16];
            size_t write_count = 0;

            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(PhysicalDevice, &props);
            const size_t alignment = props.limits.minUniformBufferOffsetAlignment;

            const auto upload_uniform_buffer = [&](int32 binding, const void* src_data, size_t src_size) {
                if (binding < 0 || src_data == nullptr || src_size == 0) {
                    return;
                }

                FO_RUNTIME_ASSERT(write_count < 16);

                // Align offset
                FrameUniformOffset = (FrameUniformOffset + alignment - 1) & ~(alignment - 1);
                FO_RUNTIME_ASSERT(FrameUniformOffset + src_size <= FrameUniformBufferSize);

                // Copy data
                MemCopy(static_cast<uint8_t*>(FrameUniformBufferMapped) + FrameUniformOffset, src_data, src_size);

                auto& buffer_info = buffer_infos[write_count];
                buffer_info.buffer = FrameUniformBuffer;
                buffer_info.offset = FrameUniformOffset;
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
                FrameUniformOffset += src_size;
            };

            if (ProjBuf.has_value()) {
                upload_uniform_buffer(_posProjBuf[pass], &ProjBuf.value(), sizeof(ProjBuffer));
            }
            if (MainTexBuf.has_value()) {
                upload_uniform_buffer(_posMainTexBuf[pass], &MainTexBuf.value(), sizeof(MainTexBuffer));
            }
            if (ContourBuf.has_value()) {
                upload_uniform_buffer(_posContourBuf[pass], &ContourBuf.value(), sizeof(ContourBuffer));
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
                vkUpdateDescriptorSets(Device, numeric_cast<uint32_t>(write_count), writes, 0, nullptr);
                vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, this->PipelineLayout, 0, 1, &uniform_set, 0, nullptr);
            }
        }

        // Draw indexed or non-indexed
        if (vk_dbuf->IndCount != 0 && vk_dbuf->IndexBuffer != nullptr) {
            // Bind index buffer and draw indexed
            // ReSharper disable once CppUnreachableCode
            constexpr auto index_type = sizeof(vindex_t) == sizeof(uint32) ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
            vkCmdBindIndexBuffer(CommandBuffer, vk_dbuf->IndexBuffer, 0, index_type);

            const size_t num_indices = indices_to_draw.value_or(vk_dbuf->IndCount);
            vkCmdDrawIndexed(CommandBuffer, numeric_cast<uint32_t>(num_indices), 1, numeric_cast<uint32_t>(start_index), 0, 0);
        }
        else if (vk_dbuf->VertCount != 0) {
            // Draw without indices
            const size_t num_vertices = vk_dbuf->VertCount;
            vkCmdDraw(CommandBuffer, numeric_cast<uint32_t>(num_vertices), 1, numeric_cast<uint32_t>(start_index), 0);
        }
    }

    ProjBuf.reset();
    MainTexBuf.reset();
    ContourBuf.reset();
    TimeBuf.reset();
    RandomValueBuf.reset();
    ScriptValueBuf.reset();
#if FO_ENABLE_3D
    ModelBuf.reset();
    ModelTexBuf.reset();
    ModelAnimBuf.reset();
#endif
}

Vulkan_Renderer::~Vulkan_Renderer()
{
    FO_STACK_TRACE_ENTRY();

    if (Device != nullptr) {
        vkDeviceWaitIdle(Device);
        FlushDeferredDestroyResources();
    }

    // Destroy staging buffers
    for (const auto& [buf, mem] : StagingBuffers) {
        if (buf != nullptr) {
            vkDestroyBuffer(Device, buf, nullptr);
        }
        if (mem != nullptr) {
            vkFreeMemory(Device, mem, nullptr);
        }
    }

    StagingBuffers.clear();

    for (auto* fb : Framebuffers) {
        vkDestroyFramebuffer(Device, fb, nullptr);
    }

    Framebuffers.clear();

    for (auto* iv : SwapchainImageViews) {
        vkDestroyImageView(Device, iv, nullptr);
    }

    SwapchainImageViews.clear();

    if (SwapchainDepthImageView != nullptr) {
        vkDestroyImageView(Device, SwapchainDepthImageView, nullptr);
        SwapchainDepthImageView = VK_NULL_HANDLE;
    }
    if (SwapchainDepthImage != nullptr) {
        vkDestroyImage(Device, SwapchainDepthImage, nullptr);
        SwapchainDepthImage = VK_NULL_HANDLE;
    }
    if (SwapchainDepthImageMemory != nullptr) {
        vkFreeMemory(Device, SwapchainDepthImageMemory, nullptr);
        SwapchainDepthImageMemory = VK_NULL_HANDLE;
    }

    if (RenderPass != nullptr) {
        vkDestroyRenderPass(Device, RenderPass, nullptr);
        RenderPass = VK_NULL_HANDLE;
    }

    if (SwapchainDepthImageView != nullptr) {
        vkDestroyImageView(Device, SwapchainDepthImageView, nullptr);
        SwapchainDepthImageView = VK_NULL_HANDLE;
    }
    if (SwapchainDepthImage != nullptr) {
        vkDestroyImage(Device, SwapchainDepthImage, nullptr);
        SwapchainDepthImage = VK_NULL_HANDLE;
    }
    if (SwapchainDepthImageMemory != nullptr) {
        vkFreeMemory(Device, SwapchainDepthImageMemory, nullptr);
        SwapchainDepthImageMemory = VK_NULL_HANDLE;
    }

    if (CommandPool != nullptr) {
        vkDestroyCommandPool(Device, CommandPool, nullptr);
        CommandPool = VK_NULL_HANDLE;
    }

    if (ImageAvailableSemaphore != nullptr) {
        vkDestroySemaphore(Device, ImageAvailableSemaphore, nullptr);
        ImageAvailableSemaphore = VK_NULL_HANDLE;
    }
    if (RenderCompleteSemaphore != nullptr) {
        vkDestroySemaphore(Device, RenderCompleteSemaphore, nullptr);
        RenderCompleteSemaphore = VK_NULL_HANDLE;
    }

    if (Swapchain != nullptr) {
        vkDestroySwapchainKHR(Device, Swapchain, nullptr);
        Swapchain = VK_NULL_HANDLE;
    }
    if (TextureDescriptorSetLayout != nullptr) {
        vkDestroyDescriptorSetLayout(Device, TextureDescriptorSetLayout, nullptr);
        TextureDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (UniformDescriptorSetLayout != nullptr) {
        vkDestroyDescriptorSetLayout(Device, UniformDescriptorSetLayout, nullptr);
        UniformDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (FrameDescriptorPool != nullptr) {
        vkDestroyDescriptorPool(Device, FrameDescriptorPool, nullptr);
        FrameDescriptorPool = VK_NULL_HANDLE;
    }
    if (FrameUniformBuffer != nullptr) {
        vkDestroyBuffer(Device, FrameUniformBuffer, nullptr);
        FrameUniformBuffer = VK_NULL_HANDLE;
    }
    if (FrameUniformBufferMemory != nullptr) {
        vkUnmapMemory(Device, FrameUniformBufferMemory);
        vkFreeMemory(Device, FrameUniformBufferMemory, nullptr);
        FrameUniformBufferMemory = VK_NULL_HANDLE;
    }
    if (LinearSampler != nullptr) {
        vkDestroySampler(Device, LinearSampler, nullptr);
        LinearSampler = VK_NULL_HANDLE;
    }
    if (PointSampler != nullptr) {
        vkDestroySampler(Device, PointSampler, nullptr);
        PointSampler = VK_NULL_HANDLE;
    }
    if (Device != nullptr) {
        vkDestroyDevice(Device, nullptr);
        Device = VK_NULL_HANDLE;
    }
    if (Surface != nullptr && Instance != nullptr) {
        vkDestroySurfaceKHR(Instance, Surface, nullptr);
        Surface = VK_NULL_HANDLE;
    }
    if (Instance != nullptr) {
        vkDestroyInstance(Instance, nullptr);
        Instance = VK_NULL_HANDLE;
    }
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

            if (vkCreateShaderModule(Device, &module_ci, nullptr, &vk_effect->VertexShaderModule[pass]) != VK_SUCCESS) {
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

            if (vkCreateShaderModule(Device, &module_ci, nullptr, &vk_effect->FragmentShaderModule[pass]) != VK_SUCCESS) {
                throw EffectLoadException("Failed to create fragment shader module", frag_fname);
            }
        }
    }

    // Create pipeline layout for this effect
    VkPipelineLayoutCreateInfo layout_ci {};
    layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    VkDescriptorSetLayout set_layouts[] = {UniformDescriptorSetLayout, TextureDescriptorSetLayout};
    layout_ci.setLayoutCount = 2;
    layout_ci.pSetLayouts = set_layouts;

    if (vkCreatePipelineLayout(Device, &layout_ci, nullptr, &vk_effect->PipelineLayout) != VK_SUCCESS) {
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
            // Attribute 3: EggTexCoord (vec2)
            attr_descs[3].binding = 0;
            attr_descs[3].location = 3;
            attr_descs[3].format = VK_FORMAT_R32G32_SFLOAT;
            attr_descs[3].offset = offsetof(Vertex2D, EggTexU);
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
            input_assembly_ci.topology = prim_type == RenderPrimitiveType::PointList ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : ConvertPrimitive(prim_type);
            input_assembly_ci.primitiveRestartEnable = prim_type == RenderPrimitiveType::LineStrip || prim_type == RenderPrimitiveType::TriangleStrip ? VK_TRUE : VK_FALSE;

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
            pipeline_ci.pColorBlendState = &blend_ci;
            pipeline_ci.pDynamicState = &dynamic_ci;
            pipeline_ci.layout = vk_effect->PipelineLayout;
            pipeline_ci.renderPass = RenderPass;
            pipeline_ci.subpass = 0;

            if (vkCreateGraphicsPipelines(Device, nullptr, 1, &pipeline_ci, nullptr, &vk_effect->Pipeline[pass][prim]) != VK_SUCCESS) {
                throw EffectLoadException("Failed to create graphics pipeline", name);
            }
        }
    }

    return vk_effect;
}

[[nodiscard]] auto Vulkan_Renderer::CreateOrthoMatrix(float32 left, float32 right, float32 bottom, float32 top, float32 nearp, float32 farp) -> mat44
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

    return ViewPort;
}

void Vulkan_Renderer::Init(GlobalSettings& settings, WindowInternalHandle* window)
{
    FO_STACK_TRACE_ENTRY();

    WriteLog("Used Vulkan rendering (minimal stub)");

    Settings = &settings;
    SdlWindow = static_cast<SDL_Window*>(window);

    Uint32 ext_count = 0;
    const char* const* exts = SDL_Vulkan_GetInstanceExtensions(&ext_count);

    if (exts == nullptr) {
        throw RenderingException("Failed to query Vulkan extensions from SDL");
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
    create_info.enabledExtensionCount = ext_count;
    create_info.ppEnabledExtensionNames = exts;

#if FO_DEBUG
    // Enable validation layers in debug builds
    const char* validation_layers[] = {"VK_LAYER_KHRONOS_validation"};

    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

    bool validation_available = false;
    for (const auto& layer : available_layers) {
        if (strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
            validation_available = true;
            break;
        }
    }

    if (validation_available) {
        create_info.enabledLayerCount = 1;
        create_info.ppEnabledLayerNames = validation_layers;
        WriteLog("Vulkan validation layers enabled");
    }
#endif

    vk_result = vkCreateInstance(&create_info, nullptr, &Instance);

    if (vk_result != VK_SUCCESS) {
        throw RenderingException("vkCreateInstance failed", vk_result);
    }

    if (!SDL_Vulkan_CreateSurface(SdlWindow.get(), Instance, nullptr, &Surface)) {
        throw RenderingException("SDL_Vulkan_CreateSurface failed");
    }

    // Pick first physical device supporting graphics
    uint32_t gpu_count = 0;
    vk_result = vkEnumeratePhysicalDevices(Instance, &gpu_count, nullptr);
    VerifyVkResult(vk_result);

    if (gpu_count == 0) {
        throw RenderingException("No Vulkan physical devices found");
    }

    vector<VkPhysicalDevice> gpus;
    gpus.resize(gpu_count);
    vk_result = vkEnumeratePhysicalDevices(Instance, &gpu_count, gpus.data());
    VerifyVkResult(vk_result);
    PhysicalDevice = gpus.front();

    VkPhysicalDeviceProperties gpu_props {};
    vkGetPhysicalDeviceProperties(PhysicalDevice, &gpu_props);

    const auto atlas_limit = numeric_cast<int32>(std::min(gpu_props.limits.maxImageDimension2D, numeric_cast<uint32_t>(AppRender::MAX_ATLAS_SIZE)));
    FO_RUNTIME_ASSERT_STR(atlas_limit >= AppRender::MIN_ATLAS_SIZE, strex("Min texture size must be at least {}", AppRender::MIN_ATLAS_SIZE));
    const_cast<int32&>(AppRender::MAX_ATLAS_WIDTH) = atlas_limit;
    const_cast<int32&>(AppRender::MAX_ATLAS_HEIGHT) = atlas_limit;

    // Find queue family
    uint32_t qcount;
    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &qcount, nullptr);
    vector<VkQueueFamilyProperties> qprops(qcount);
    qprops.resize(qcount);
    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &qcount, qprops.data());

    optional<uint32_t> graphics_family;

    for (uint32_t i = 0; i < qcount; i++) {
        VkBool32 present_support = VK_FALSE;
        vk_result = vkGetPhysicalDeviceSurfaceSupportKHR(PhysicalDevice, i, Surface, &present_support);
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

    vk_result = vkCreateDevice(PhysicalDevice, &dci, nullptr, &Device);
    VerifyVkResult(vk_result);

    vkGetDeviceQueue(Device, graphics_family.value(), 0, &GraphicsQueue);
    GraphicsFamilyIndex = graphics_family.value();

    // Create command pool for recording render commands
    VkCommandPoolCreateInfo cpi {};
    cpi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpi.queueFamilyIndex = GraphicsFamilyIndex;
    vk_result = vkCreateCommandPool(Device, &cpi, nullptr, &CommandPool);
    VerifyVkResult(vk_result);

    ViewPort = irect32 {{0, 0}, {0, 0}};

    // Initialize swapchain for current window size
    int width;
    int height;
    SDL_GetWindowSizeInPixels(SdlWindow.get(), &width, &height);
    ViewPort = irect32 {{0, 0}, {std::max(width, 1), std::max(height, 1)}};
    TargetSize = {Settings->ScreenWidth, Settings->ScreenHeight};
    ProjectionMatrixColMaj = CreateOrthoMatrix(0.0f, numeric_cast<float32>(TargetSize.width), numeric_cast<float32>(TargetSize.height), 0.0f, -10.0f, 10.0f);
    ProjectionMatrixColMaj.Transpose();
    RecreateSwapchain({width, height});

    // Allocate single command buffer (single-threaded rendering)
    VkCommandBufferAllocateInfo cbai {};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = CommandPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;

    vk_result = vkAllocateCommandBuffers(Device, &cbai, &CommandBuffer);
    VerifyVkResult(vk_result);

    // Create semaphores for acquire/present synchronization
    VkSemaphoreCreateInfo sem_ci {};
    sem_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vk_result = vkCreateSemaphore(Device, &sem_ci, nullptr, &ImageAvailableSemaphore);
    VerifyVkResult(vk_result);
    vk_result = vkCreateSemaphore(Device, &sem_ci, nullptr, &RenderCompleteSemaphore);
    VerifyVkResult(vk_result);

    // Allocate staging command buffer for buffer uploads
    VkCommandBufferAllocateInfo staging_cbai {};
    staging_cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    staging_cbai.commandPool = CommandPool;
    staging_cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    staging_cbai.commandBufferCount = 1;

    vk_result = vkAllocateCommandBuffers(Device, &staging_cbai, &StagingCommandBuffer);
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

    vk_result = vkCreateDescriptorSetLayout(Device, &layout_ci, nullptr, &TextureDescriptorSetLayout);
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

    vk_result = vkCreateDescriptorSetLayout(Device, &ubo_layout_ci, nullptr, &UniformDescriptorSetLayout);
    VerifyVkResult(vk_result);

    // Create FrameDescriptorPool
    VkDescriptorPoolSize frame_pool_sizes[2] {};
    frame_pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    frame_pool_sizes[0].descriptorCount = 20000;
    frame_pool_sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    frame_pool_sizes[1].descriptorCount = 20000;

    VkDescriptorPoolCreateInfo frame_pool_ci {};
    frame_pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    frame_pool_ci.poolSizeCount = 2;
    frame_pool_ci.pPoolSizes = frame_pool_sizes;
    frame_pool_ci.maxSets = 20000;
    frame_pool_ci.flags = 0;

    vk_result = vkCreateDescriptorPool(Device, &frame_pool_ci, nullptr, &FrameDescriptorPool);
    VerifyVkResult(vk_result);

    // Create FrameUniformBuffer
    AllocateBuffer(FrameUniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, FrameUniformBuffer, FrameUniformBufferMemory);
    vk_result = vkMapMemory(Device, FrameUniformBufferMemory, 0, FrameUniformBufferSize, 0, &FrameUniformBufferMapped);
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

    vk_result = vkCreateSampler(Device, &sampler_ci, nullptr, &LinearSampler);
    VerifyVkResult(vk_result);

    sampler_ci.magFilter = VK_FILTER_NEAREST;
    sampler_ci.minFilter = VK_FILTER_NEAREST;
    sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    vk_result = vkCreateSampler(Device, &sampler_ci, nullptr, &PointSampler);
    VerifyVkResult(vk_result);

    constexpr ucolor dummy_pixel[1] = {ucolor {255, 0, 255, 255}};
    DummyTexture = CreateTexture({1, 1}, false, false);
    DummyTexture->UpdateTextureRegion({}, {1, 1}, dummy_pixel);

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
    for (auto* fb : Framebuffers) {
        DestroyResourceSafe(fb);
    }

    Framebuffers.clear();

    for (auto* iv : SwapchainImageViews) {
        DestroyResourceSafe(iv);
    }

    SwapchainImageViews.clear();

    if (Swapchain != nullptr) {
        vkDeviceWaitIdle(Device);
        vkDestroySwapchainKHR(Device, Swapchain, nullptr);
        Swapchain = VK_NULL_HANDLE;
        SwapchainImages.clear();
        SwapchainImageLayouts.clear();
    }

    DestroyResourceSafe(SwapchainDepthImageView);
    DestroyResourceSafe(SwapchainDepthImage);
    DestroyResourceSafe(SwapchainDepthImageMemory);

    // Create swapchain
    VkSurfaceCapabilitiesKHR caps {};
    vk_result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, Surface, &caps);
    VerifyVkResult(vk_result);

    VkExtent2D swapchain_extent {};
    if (caps.currentExtent.width != UINT32_MAX) {
        swapchain_extent = caps.currentExtent;
    }
    else {
        swapchain_extent.width = std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, numeric_cast<uint32_t>(size.width)));
        swapchain_extent.height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, numeric_cast<uint32_t>(size.height)));
    }

    SwapchainSize = {numeric_cast<int32>(swapchain_extent.width), numeric_cast<int32>(swapchain_extent.height)};

    uint32_t image_count = caps.minImageCount + 1;

    if (caps.maxImageCount != 0 && image_count > caps.maxImageCount) {
        image_count = caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR sci {};
    sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = Surface;
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

    vk_result = vkCreateSwapchainKHR(Device, &sci, nullptr, &Swapchain);
    VerifyVkResult(vk_result);
    vk_result = vkGetSwapchainImagesKHR(Device, Swapchain, &image_count, nullptr);
    VerifyVkResult(vk_result);
    SwapchainImages.resize(image_count);
    vk_result = vkGetSwapchainImagesKHR(Device, Swapchain, &image_count, SwapchainImages.data());
    VerifyVkResult(vk_result);
    SwapchainImageLayouts.assign(SwapchainImages.size(), VK_IMAGE_LAYOUT_UNDEFINED);
    SwapchainFormat = sci.imageFormat;

    if (RenderPass == nullptr) {
        VkAttachmentDescription color_attachment {};
        color_attachment.format = SwapchainFormat;
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

        vk_result = vkCreateRenderPass(Device, &rp_info, nullptr, &RenderPass);
        VerifyVkResult(vk_result);
    }

    // Create framebuffers
    SwapchainImageViews.resize(SwapchainImages.size());
    Framebuffers.resize(SwapchainImages.size());

    AllocateImage(swapchain_extent.width, swapchain_extent.height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, SwapchainDepthImage, SwapchainDepthImageMemory);

    VkImageViewCreateInfo depth_view_ci {};
    depth_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depth_view_ci.image = SwapchainDepthImage;
    depth_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depth_view_ci.format = VK_FORMAT_D32_SFLOAT;
    depth_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depth_view_ci.subresourceRange.baseMipLevel = 0;
    depth_view_ci.subresourceRange.levelCount = 1;
    depth_view_ci.subresourceRange.baseArrayLayer = 0;
    depth_view_ci.subresourceRange.layerCount = 1;

    vk_result = vkCreateImageView(Device, &depth_view_ci, nullptr, &SwapchainDepthImageView);
    VerifyVkResult(vk_result);

    for (size_t i = 0; i < SwapchainImages.size(); i++) {
        VkImageViewCreateInfo ivci {};
        ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivci.image = SwapchainImages[i];
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format = SwapchainFormat;
        ivci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.baseMipLevel = 0;
        ivci.subresourceRange.levelCount = 1;
        ivci.subresourceRange.baseArrayLayer = 0;
        ivci.subresourceRange.layerCount = 1;

        vk_result = vkCreateImageView(Device, &ivci, nullptr, &SwapchainImageViews[i]);
        VerifyVkResult(vk_result);

        const VkImageView framebuffer_attachments[] = {SwapchainImageViews[i], SwapchainDepthImageView};

        VkFramebufferCreateInfo fci {};
        fci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fci.renderPass = RenderPass;
        fci.attachmentCount = 2;
        fci.pAttachments = framebuffer_attachments;
        fci.width = swapchain_extent.width;
        fci.height = swapchain_extent.height;
        fci.layers = 1;

        vk_result = vkCreateFramebuffer(Device, &fci, nullptr, &Framebuffers[i]);
        VerifyVkResult(vk_result);
    }
}

static void BeginFrame()
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(Swapchain);
    FO_RUNTIME_ASSERT(Device);

    VkResult vk_result = VK_SUCCESS;

    // Single-threaded: wait for all GPU work to finish before reusing command buffer
    vk_result = vkQueueWaitIdle(GraphicsQueue);
    VerifyVkResult(vk_result);

    FlushDeferredDestroyResources();

    if (PendingSwapchainRecreateSize.has_value()) {
        const auto recreate_size = PendingSwapchainRecreateSize.value();
        PendingSwapchainRecreateSize.reset();
        RecreateSwapchain({std::max(recreate_size.width, 1), std::max(recreate_size.height, 1)});
    }

    vk_result = vkResetDescriptorPool(Device, FrameDescriptorPool, 0);
    VerifyVkResult(vk_result);
    FrameUniformOffset = 0;

    vk_result = vkAcquireNextImageKHR(Device, Swapchain, UINT64_MAX, ImageAvailableSemaphore, VK_NULL_HANDLE, &CurrentSwapchainImageIndex);
    if (vk_result == VK_ERROR_OUT_OF_DATE_KHR || vk_result == VK_SUBOPTIMAL_KHR) {
        int width;
        int height;
        SDL_GetWindowSizeInPixels(SdlWindow.get(), &width, &height);
        RecreateSwapchain({std::max(width, 1), std::max(height, 1)});

        vk_result = vkAcquireNextImageKHR(Device, Swapchain, UINT64_MAX, ImageAvailableSemaphore, VK_NULL_HANDLE, &CurrentSwapchainImageIndex);
    }
    VerifyVkResult(vk_result);

    vk_result = vkResetCommandBuffer(CommandBuffer, 0);
    VerifyVkResult(vk_result);

    BeginCommandBufferRecording(CommandBuffer);

    // Explicitly clear the swapchain image before first render pass begin
    // This ensures valid content for loadOp=LOAD (image starts as UNDEFINED after acquire)
    TransitionColorImage(CommandBuffer, SwapchainImages[CurrentSwapchainImageIndex], SwapchainImageLayouts[CurrentSwapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    SwapchainImageLayouts[CurrentSwapchainImageIndex] = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    const VkClearColorValue frame_clear = ClearColor;
    VkImageSubresourceRange clear_range {};
    clear_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clear_range.baseMipLevel = 0;
    clear_range.levelCount = 1;
    clear_range.baseArrayLayer = 0;
    clear_range.layerCount = 1;
    vkCmdClearColorImage(CommandBuffer, SwapchainImages[CurrentSwapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &frame_clear, 1, &clear_range);

    TransitionColorImage(CommandBuffer, SwapchainImages[CurrentSwapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    SwapchainImageLayouts[CurrentSwapchainImageIndex] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    BeginCurrentRenderPass();
}

static void EndFrame()
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(Swapchain);
    FO_RUNTIME_ASSERT(Device);

    VkResult vk_result = VK_SUCCESS;

    EndCurrentRenderPass();

    if (CurrentRenderTarget == nullptr && CurrentSwapchainImageIndex < SwapchainImages.size() && CurrentSwapchainImageIndex < SwapchainImageLayouts.size()) {
        TransitionColorImage(CommandBuffer, SwapchainImages[CurrentSwapchainImageIndex], SwapchainImageLayouts[CurrentSwapchainImageIndex], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        SwapchainImageLayouts[CurrentSwapchainImageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }

    // End command buffer
    vk_result = vkEndCommandBuffer(CommandBuffer);
    VerifyVkResult(vk_result);

    // Submit to graphics queue
    constexpr VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &ImageAvailableSemaphore;
    submit_info.pWaitDstStageMask = &wait_stage;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &CommandBuffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &RenderCompleteSemaphore;

    vk_result = vkQueueSubmit(GraphicsQueue, 1, &submit_info, VK_NULL_HANDLE);
    VerifyVkResult(vk_result);

    // Present to screen
    VkPresentInfoKHR info {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &RenderCompleteSemaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &Swapchain;
    info.pImageIndices = &CurrentSwapchainImageIndex;

    vk_result = vkQueuePresentKHR(GraphicsQueue, &info);
    if (vk_result == VK_ERROR_OUT_OF_DATE_KHR || vk_result == VK_SUBOPTIMAL_KHR) {
        int width;
        int height;
        SDL_GetWindowSizeInPixels(SdlWindow.get(), &width, &height);
        RecreateSwapchain({std::max(width, 1), std::max(height, 1)});
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
    rp_begin.renderPass = RenderPass;
    rp_begin.renderArea.offset = {.x = 0, .y = 0};

    VkClearValue clear_values[2] {};
    clear_values[0].color = ClearColor;
    clear_values[1].depthStencil = {.depth = 1.0f, .stencil = 0};
    rp_begin.clearValueCount = 2;
    rp_begin.pClearValues = clear_values;

    if (CurrentRenderTarget != nullptr) {
        auto* vk_tex = static_cast<Vulkan_Texture*>(CurrentRenderTarget.get()); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        EnsureTextureRenderTargetResources(vk_tex);
        FO_RUNTIME_ASSERT(vk_tex->TextureFramebuffer != nullptr);
        FO_RUNTIME_ASSERT(vk_tex->TextureImage != nullptr);

        TransitionColorImage(CommandBuffer, vk_tex->TextureImage, vk_tex->TextureImageLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        vk_tex->TextureImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        rp_begin.framebuffer = vk_tex->TextureFramebuffer;
        rp_begin.renderArea.extent = {.width = numeric_cast<uint32_t>(vk_tex->Size.width), .height = numeric_cast<uint32_t>(vk_tex->Size.height)};
        ViewPort = irect32 {{0, 0}, {vk_tex->Size.width, vk_tex->Size.height}};
    }
    else {
        FO_RUNTIME_ASSERT(CurrentSwapchainImageIndex < Framebuffers.size());
        FO_RUNTIME_ASSERT(CurrentSwapchainImageIndex < SwapchainImageLayouts.size());

        TransitionColorImage(CommandBuffer, SwapchainImages[CurrentSwapchainImageIndex], SwapchainImageLayouts[CurrentSwapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        SwapchainImageLayouts[CurrentSwapchainImageIndex] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        rp_begin.framebuffer = Framebuffers[CurrentSwapchainImageIndex];
        rp_begin.renderArea.offset = {.x = 0, .y = 0};

        rp_begin.renderArea.extent = {.width = numeric_cast<uint32_t>(SwapchainSize.width), .height = numeric_cast<uint32_t>(SwapchainSize.height)};
    }

    vkCmdBeginRenderPass(CommandBuffer, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
    ApplyViewportAndScissor();
}

static void EndCurrentRenderPass()
{
    FO_STACK_TRACE_ENTRY();

    vkCmdEndRenderPass(CommandBuffer);
}

static void ApplyViewportAndScissor()
{
    FO_STACK_TRACE_ENTRY();

    VkViewport viewport {};
    viewport.x = numeric_cast<float32>(ViewPort.x);
    viewport.y = numeric_cast<float32>(ViewPort.y);
    viewport.width = numeric_cast<float32>(ViewPort.width);
    viewport.height = numeric_cast<float32>(ViewPort.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(CommandBuffer, 0, 1, &viewport);

    if (ScissorEnabled) {
        VkRect2D scissor_rect {};

        int32 left;
        int32 top;
        int32 right;
        int32 bottom;

        if (ViewPort.width != TargetSize.width || ViewPort.height != TargetSize.height) {
            const auto x_ratio = checked_div<float32>(numeric_cast<float32>(ViewPort.width), numeric_cast<float32>(TargetSize.width));
            const auto y_ratio = checked_div<float32>(numeric_cast<float32>(ViewPort.height), numeric_cast<float32>(TargetSize.height));

            left = ViewPort.x + iround<int32>(numeric_cast<float32>(ScissorRect.x) * x_ratio);
            top = ViewPort.y + iround<int32>(numeric_cast<float32>(ScissorRect.y) * y_ratio);
            right = ViewPort.x + iround<int32>(numeric_cast<float32>(ScissorRect.x + ScissorRect.width) * x_ratio);
            bottom = ViewPort.y + iround<int32>(numeric_cast<float32>(ScissorRect.y + ScissorRect.height) * y_ratio);
        }
        else {
            left = ViewPort.x + ScissorRect.x;
            top = ViewPort.y + ScissorRect.y;
            right = ViewPort.x + ScissorRect.x + ScissorRect.width;
            bottom = ViewPort.y + ScissorRect.y + ScissorRect.height;
        }

        scissor_rect.offset = {.x = left, .y = top};
        scissor_rect.extent = {.width = numeric_cast<uint32_t>(std::max(1, right - left)), .height = numeric_cast<uint32_t>(std::max(1, bottom - top))};
        vkCmdSetScissor(CommandBuffer, 0, 1, &scissor_rect);
    }
    else {
        VkRect2D scissor_rect {};
        scissor_rect.offset = {.x = numeric_cast<int32_t>(ViewPort.x), .y = numeric_cast<int32_t>(ViewPort.y)};
        scissor_rect.extent = {.width = numeric_cast<uint32_t>(ViewPort.width), .height = numeric_cast<uint32_t>(ViewPort.height)};
        vkCmdSetScissor(CommandBuffer, 0, 1, &scissor_rect);
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

        vk_result = vkCreateImageView(Device, &depth_view_ci, nullptr, &vk_tex->DepthImageView);
        VerifyVkResult(vk_result);
    }

    if (vk_tex->TextureFramebuffer == nullptr) {
        const VkImageView framebuffer_attachments[] = {vk_tex->TextureImageView, vk_tex->DepthImageView};

        VkFramebufferCreateInfo fci {};
        fci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fci.renderPass = RenderPass;
        fci.attachmentCount = 2;
        fci.pAttachments = framebuffer_attachments;
        fci.width = numeric_cast<uint32_t>(vk_tex->Size.width);
        fci.height = numeric_cast<uint32_t>(vk_tex->Size.height);
        fci.layers = 1;

        vk_result = vkCreateFramebuffer(Device, &fci, nullptr, &vk_tex->TextureFramebuffer);
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

    if (CurrentRenderTarget == tex) {
        return;
    }

    EndCurrentRenderPass();

    if (CurrentRenderTarget != nullptr) {
        auto* prev_tex = static_cast<Vulkan_Texture*>(CurrentRenderTarget.get()); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        TransitionColorImage(CommandBuffer, prev_tex->TextureImage, prev_tex->TextureImageLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        prev_tex->TextureImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    CurrentRenderTarget = tex;

    if (CurrentRenderTarget == nullptr) {
        const auto back_buf_size = SwapchainSize;
        const auto back_buf_aspect = checked_div<float32>(numeric_cast<float32>(back_buf_size.width), numeric_cast<float32>(back_buf_size.height));
        const auto screen_aspect = checked_div<float32>(numeric_cast<float32>(Settings->ScreenWidth), numeric_cast<float32>(Settings->ScreenHeight));
        const auto fit_width = iround<int32>(screen_aspect <= back_buf_aspect ? numeric_cast<float32>(back_buf_size.height) * screen_aspect : numeric_cast<float32>(back_buf_size.height) * back_buf_aspect);
        const auto fit_height = iround<int32>(screen_aspect <= back_buf_aspect ? numeric_cast<float32>(back_buf_size.width) / back_buf_aspect : numeric_cast<float32>(back_buf_size.width) / screen_aspect);

        const auto vp_ox = (back_buf_size.width - fit_width) / 2;
        const auto vp_oy = (back_buf_size.height - fit_height) / 2;
        ViewPort = irect32 {vp_ox, vp_oy, fit_width, fit_height};
        TargetSize = {Settings->ScreenWidth, Settings->ScreenHeight};
    }
    else {
        const auto* vk_tex = static_cast<const Vulkan_Texture*>(CurrentRenderTarget.get()); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        ViewPort = irect32 {0, 0, vk_tex->Size.width, vk_tex->Size.height};
        TargetSize = vk_tex->Size;
    }

    ProjectionMatrixColMaj = CreateOrthoMatrix(0.0f, numeric_cast<float32>(TargetSize.width), numeric_cast<float32>(TargetSize.height), 0.0f, -10.0f, 10.0f);
    ProjectionMatrixColMaj.Transpose();

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
        ClearColor.float32[0] = numeric_cast<float32>(color.value().comp.r) / 255.0f;
        ClearColor.float32[1] = numeric_cast<float32>(color.value().comp.g) / 255.0f;
        ClearColor.float32[2] = numeric_cast<float32>(color.value().comp.b) / 255.0f;
        ClearColor.float32[3] = numeric_cast<float32>(color.value().comp.a) / 255.0f;

        VkClearAttachment color_attachment {};
        color_attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        color_attachment.colorAttachment = 0;
        color_attachment.clearValue.color = ClearColor;
        attachments.emplace_back(color_attachment);
    }

    if (depth) {
        VkClearAttachment depth_attachment {};
        depth_attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depth_attachment.clearValue.depthStencil = {.depth = 1.0f, .stencil = 0};
        attachments.emplace_back(depth_attachment);
    }

    VkClearRect clear_rect {};
    clear_rect.rect.offset = {.x = numeric_cast<int32_t>(ViewPort.x), .y = numeric_cast<int32_t>(ViewPort.y)};
    clear_rect.rect.extent = {.width = numeric_cast<uint32_t>(ViewPort.width), .height = numeric_cast<uint32_t>(ViewPort.height)};
    clear_rect.baseArrayLayer = 0;
    clear_rect.layerCount = 1;

    vkCmdClearAttachments(CommandBuffer, numeric_cast<uint32_t>(attachments.size()), attachments.data(), 1, &clear_rect);
}

void Vulkan_Renderer::EnableScissor(irect32 rect)
{
    FO_STACK_TRACE_ENTRY();

    ScissorRect = rect;
    ScissorEnabled = true;

    ApplyViewportAndScissor();
}

void Vulkan_Renderer::DisableScissor()
{
    FO_STACK_TRACE_ENTRY();

    ScissorEnabled = false;

    ApplyViewportAndScissor();
}

void Vulkan_Renderer::OnResizeWindow(isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    ViewPort = irect32 {{0, 0}, {std::max(size.width, 1), std::max(size.height, 1)}};
    PendingSwapchainRecreateSize = isize32 {std::max(size.width, 1), std::max(size.height, 1)};
}

static void DestroyResourceSafe(VkBuffer& buffer)
{
    FO_STACK_TRACE_ENTRY();

    if (buffer != VK_NULL_HANDLE) {
        DeferredDestroyBuffers.emplace_back(buffer);
        buffer = VK_NULL_HANDLE;
    }
}

static void DestroyResourceSafe(VkDeviceMemory& memory)
{
    FO_STACK_TRACE_ENTRY();

    if (memory != VK_NULL_HANDLE) {
        DeferredDestroyMemories.emplace_back(memory);
        memory = VK_NULL_HANDLE;
    }
}

static void DestroyResourceSafe(VkImage& image)
{
    FO_STACK_TRACE_ENTRY();

    if (image != VK_NULL_HANDLE) {
        DeferredDestroyImages.emplace_back(image);
        image = VK_NULL_HANDLE;
    }
}

static void DestroyResourceSafe(VkImageView& image_view)
{
    FO_STACK_TRACE_ENTRY();

    if (image_view != VK_NULL_HANDLE) {
        DeferredDestroyImageViews.emplace_back(image_view);
        image_view = VK_NULL_HANDLE;
    }
}

static void DestroyResourceSafe(VkFramebuffer& framebuffer)
{
    FO_STACK_TRACE_ENTRY();

    if (framebuffer != VK_NULL_HANDLE) {
        DeferredDestroyFramebuffers.emplace_back(framebuffer);
        framebuffer = VK_NULL_HANDLE;
    }
}

static void FlushDeferredDestroyResources()
{
    FO_STACK_TRACE_ENTRY();

    for (auto* framebuffer : DeferredDestroyFramebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(Device, framebuffer, nullptr);
        }
    }
    DeferredDestroyFramebuffers.clear();

    for (auto* image_view : DeferredDestroyImageViews) {
        if (image_view != VK_NULL_HANDLE) {
            vkDestroyImageView(Device, image_view, nullptr);
        }
    }
    DeferredDestroyImageViews.clear();

    for (auto* image : DeferredDestroyImages) {
        if (image != VK_NULL_HANDLE) {
            vkDestroyImage(Device, image, nullptr);
        }
    }
    DeferredDestroyImages.clear();

    for (auto* buffer : DeferredDestroyBuffers) {
        if (buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(Device, buffer, nullptr);
        }
    }
    DeferredDestroyBuffers.clear();

    for (auto* memory : DeferredDestroyMemories) {
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(Device, memory, nullptr);
        }
    }
    DeferredDestroyMemories.clear();
}

FO_END_NAMESPACE

#endif // FO_HAVE_VULKAN
