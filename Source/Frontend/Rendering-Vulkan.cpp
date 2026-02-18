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
static vector<VkImageView> SwapchainImageViews;
static VkFormat SwapchainFormat {};
static VkRenderPass RenderPass {};
static vector<VkFramebuffer> Framebuffers;
static VkCommandPool CommandPool {};
static vector<VkCommandBuffer> CommandBuffers {};
static vector<VkFence> FrameFences {}; // One fence per frame buffer for synchronization
static vector<VkSemaphore> ImageAvailableSemaphores {}; // Signaled when image acquired
static vector<VkSemaphore> RenderCompleteSemaphores {}; // Signaled when render done
static irect32 ViewPort {};
static unique_ptr<RenderTexture> DummyTexture {};
static VkClearColorValue ClearColor {{0.0f, 0.0f, 0.0f, 1.0f}};
static size_t FrameIndex = 0;
static VkPipelineLayout PipelineLayout {};
static VkPipeline GraphicsPipeline {};
static VkCommandBuffer StagingCommandBuffer {};
static vector<tuple<VkBuffer, VkDeviceMemory>> StagingBuffers;
static VkCommandBuffer CurrentCommandBuffer {}; // Current frame's recording command buffer
static VkDescriptorSetLayout TextureDescriptorSetLayout {};
static VkDescriptorPool DescriptorPool {};
static VkSampler TextureSampler {};
static raw_ptr<RenderTexture> CurrentRenderTarget {}; // Current render target (nullptr = swapchain)
static irect32 ScissorRect {};
static bool ScissorEnabled = false;

static void RecreateSwapchain(isize32 size);
static void AllocateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory);
static void AllocateImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& memory);

static void VerifyVkResult(VkResult vk_result)
{
    FO_NO_STACK_TRACE_ENTRY();

    if (vk_result != VK_SUCCESS) {
        throw RenderingException("Vulkan error", vk_result);
    }
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
    VkBuffer IndexBuffer {};
    VkDeviceMemory IndexBufferMemory {};
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
    VkPipeline Pipeline[EFFECT_MAX_PASSES] {};
    VkPipelineLayout PipelineLayout {};
    VkDescriptorSet DescriptorSet[EFFECT_MAX_PASSES] {};
};

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
        if (Pipeline[pass] != nullptr) {
            vkDestroyPipeline(Device, Pipeline[pass], nullptr);
            Pipeline[pass] = VK_NULL_HANDLE;
        }
    }

    if (PipelineLayout != nullptr) {
        vkDestroyPipelineLayout(Device, PipelineLayout, nullptr);
        PipelineLayout = VK_NULL_HANDLE;
    }
}

Vulkan_DrawBuffer::~Vulkan_DrawBuffer()
{
    FO_STACK_TRACE_ENTRY();

    if (VertexBuffer != nullptr) {
        vkDestroyBuffer(Device, VertexBuffer, nullptr);
        VertexBuffer = VK_NULL_HANDLE;
    }
    if (VertexBufferMemory != nullptr) {
        vkFreeMemory(Device, VertexBufferMemory, nullptr);
        VertexBufferMemory = VK_NULL_HANDLE;
    }
    if (IndexBuffer != nullptr) {
        vkDestroyBuffer(Device, IndexBuffer, nullptr);
        IndexBuffer = VK_NULL_HANDLE;
    }
    if (IndexBufferMemory != nullptr) {
        vkFreeMemory(Device, IndexBufferMemory, nullptr);
        IndexBufferMemory = VK_NULL_HANDLE;
    }
}

Vulkan_Texture::~Vulkan_Texture()
{
    FO_STACK_TRACE_ENTRY();

    if (TextureImageView != nullptr) {
        vkDestroyImageView(Device, TextureImageView, nullptr);
        TextureImageView = VK_NULL_HANDLE;
    }
    if (TextureImage != nullptr) {
        vkDestroyImage(Device, TextureImage, nullptr);
        TextureImage = VK_NULL_HANDLE;
    }
    if (TextureImageMemory != nullptr) {
        vkFreeMemory(Device, TextureImageMemory, nullptr);
        TextureImageMemory = VK_NULL_HANDLE;
    }
    if (DepthImageView != nullptr) {
        vkDestroyImageView(Device, DepthImageView, nullptr);
        DepthImageView = VK_NULL_HANDLE;
    }
    if (DepthImage != nullptr) {
        vkDestroyImage(Device, DepthImage, nullptr);
        DepthImage = VK_NULL_HANDLE;
    }
    if (DepthImageMemory != nullptr) {
        vkFreeMemory(Device, DepthImageMemory, nullptr);
        DepthImageMemory = VK_NULL_HANDLE;
    }
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

    // Begin staging command buffer to record copy
    VkCommandBufferBeginInfo begin_info {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vk_result = vkBeginCommandBuffer(StagingCommandBuffer, &begin_info);
    VerifyVkResult(vk_result);

    // Transition image from SHADER_READ_ONLY to TRANSFER_SRC
    VkImageMemoryBarrier img_barrier {};
    img_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    img_barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    img_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    img_barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    img_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    img_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    img_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    img_barrier.image = TextureImage;
    img_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    img_barrier.subresourceRange.baseMipLevel = 0;
    img_barrier.subresourceRange.levelCount = 1;
    img_barrier.subresourceRange.baseArrayLayer = 0;
    img_barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(StagingCommandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &img_barrier);

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

    // Transition image back to SHADER_READ_ONLY
    img_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    img_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    img_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    img_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    vkCmdPipelineBarrier(StagingCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &img_barrier);

    vk_result = vkEndCommandBuffer(StagingCommandBuffer);
    VerifyVkResult(vk_result);

    // Submit and wait
    VkSubmitInfo submit_info {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &StagingCommandBuffer;

    vk_result = vkQueueSubmit(GraphicsQueue, 1, &submit_info, VK_NULL_HANDLE);
    VerifyVkResult(vk_result);
    vk_result = vkQueueWaitIdle(GraphicsQueue);
    VerifyVkResult(vk_result);

    // Read data from staging buffer
    void* data;
    vk_result = vkMapMemory(Device, staging_mem, 0, region_size, 0, &data);
    VerifyVkResult(vk_result);
    tex_region.resize(size.square());
    MemCopy(tex_region.data(), data, region_size);
    vkUnmapMemory(Device, staging_mem);

    // Cleanup staging buffer
    vkDestroyBuffer(Device, staging_buf, nullptr);
    vkFreeMemory(Device, staging_mem, nullptr);

    // Reset staging command buffer for next use
    vk_result = vkResetCommandBuffer(StagingCommandBuffer, 0);
    VerifyVkResult(vk_result);

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
    StagingBuffers.emplace_back(staging_buf, staging_mem);

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

    vkUnmapMemory(Device, staging_mem);

    // Create GPU texture image if needed
    if (TextureImage == nullptr) {
        AllocateImage(numeric_cast<uint32_t>(Size.width), numeric_cast<uint32_t>(Size.height), VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, TextureImage, TextureImageMemory);

        // Create image view
        VkImageViewCreateInfo image_view_ci {};
        image_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_ci.image = TextureImage;
        image_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_ci.format = VK_FORMAT_R8G8B8A8_UNORM;
        image_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_ci.subresourceRange.baseMipLevel = 0;
        image_view_ci.subresourceRange.levelCount = 1;
        image_view_ci.subresourceRange.baseArrayLayer = 0;
        image_view_ci.subresourceRange.layerCount = 1;

        vk_result = vkCreateImageView(Device, &image_view_ci, nullptr, &TextureImageView);
        VerifyVkResult(vk_result);

        // Allocate descriptor set
        VkDescriptorSetAllocateInfo alloc_info {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = DescriptorPool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &TextureDescriptorSetLayout;

        vk_result = vkAllocateDescriptorSets(Device, &alloc_info, &DescriptorSet);
        VerifyVkResult(vk_result);

        // Write descriptor set
        VkDescriptorImageInfo image_info {};
        image_info.sampler = TextureSampler;
        image_info.imageView = TextureImageView;
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write_set {};
        write_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write_set.dstSet = DescriptorSet;
        write_set.dstBinding = 0;
        write_set.descriptorCount = 1;
        write_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write_set.pImageInfo = &image_info;

        vkUpdateDescriptorSets(Device, 1, &write_set, 0, nullptr);

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

    // Begin staging command buffer to record copy
    VkCommandBufferBeginInfo begin_info {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vk_result = vkBeginCommandBuffer(StagingCommandBuffer, &begin_info);
    VerifyVkResult(vk_result);

    // Transition image to transfer destination layout
    VkImageMemoryBarrier img_barrier {};
    img_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    img_barrier.srcAccessMask = 0;
    img_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    img_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    img_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    img_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    img_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    img_barrier.image = TextureImage;
    img_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    img_barrier.subresourceRange.baseMipLevel = 0;
    img_barrier.subresourceRange.levelCount = 1;
    img_barrier.subresourceRange.baseArrayLayer = 0;
    img_barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(StagingCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &img_barrier);

    // Copy staging buffer to image region with position offset
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

    vkCmdCopyBufferToImage(StagingCommandBuffer, staging_buf, TextureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition image to shader read layout
    img_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    img_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    img_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    img_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    vkCmdPipelineBarrier(StagingCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &img_barrier);

    vk_result = vkEndCommandBuffer(StagingCommandBuffer);
    VerifyVkResult(vk_result);

    // Submit staging command buffer
    VkSubmitInfo submit_info {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &StagingCommandBuffer;

    vk_result = vkQueueSubmit(GraphicsQueue, 1, &submit_info, VK_NULL_HANDLE);
    VerifyVkResult(vk_result);
    vk_result = vkQueueWaitIdle(GraphicsQueue);
    VerifyVkResult(vk_result);

    // Reset staging command buffer for next upload
    vk_result = vkResetCommandBuffer(StagingCommandBuffer, 0);
    VerifyVkResult(vk_result);
}

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
}

void Vulkan_DrawBuffer::Upload(EffectUsage usage, optional<size_t> custom_vertices_size, optional<size_t> custom_indices_size)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(Device);

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

    if (vert_size != 0 && (VertexBuffer == nullptr || StaticDataChanged)) {
        if (VertexBuffer != nullptr) {
            vkDestroyBuffer(Device, VertexBuffer, nullptr);
            VertexBuffer = VK_NULL_HANDLE;
        }
        if (VertexBufferMemory != nullptr) {
            vkFreeMemory(Device, VertexBufferMemory, nullptr);
            VertexBufferMemory = VK_NULL_HANDLE;
        }

        // Create staging buffer for vertex data
        VkBuffer staging_vert_buf {};
        VkDeviceMemory staging_vert_mem {};

        AllocateBuffer(vert_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_vert_buf, staging_vert_mem);
        StagingBuffers.emplace_back(staging_vert_buf, staging_vert_mem);

        // Copy vertex data to staging buffer
        void* data {};
        vk_result = vkMapMemory(Device, staging_vert_mem, 0, vert_size, 0, &data);
        VerifyVkResult(vk_result);
        MemCopy(data, Vertices.data(), vert_size);
        vkUnmapMemory(Device, staging_vert_mem);

        // Create GPU-local vertex buffer
        AllocateBuffer(vert_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VertexBuffer, VertexBufferMemory);

        // Copy from staging to GPU-local
        VkBufferCopy copy_region {};
        copy_region.size = vert_size;

        VkCommandBufferBeginInfo begin_info {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vk_result = vkBeginCommandBuffer(StagingCommandBuffer, &begin_info);
        VerifyVkResult(vk_result);

        vkCmdCopyBuffer(StagingCommandBuffer, staging_vert_buf, VertexBuffer, 1, &copy_region);

        vk_result = vkEndCommandBuffer(StagingCommandBuffer);
        VerifyVkResult(vk_result);

        VkSubmitInfo submit_info {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &StagingCommandBuffer;

        vk_result = vkQueueSubmit(GraphicsQueue, 1, &submit_info, VK_NULL_HANDLE);
        VerifyVkResult(vk_result);
        vk_result = vkQueueWaitIdle(GraphicsQueue);
        VerifyVkResult(vk_result);

        vk_result = vkResetCommandBuffer(StagingCommandBuffer, 0);
        VerifyVkResult(vk_result);
    }

    if (idx_size != 0 && (IndexBuffer == nullptr || StaticDataChanged)) {
        if (IndexBuffer != nullptr) {
            vkDestroyBuffer(Device, IndexBuffer, nullptr);
            IndexBuffer = VK_NULL_HANDLE;
        }
        if (IndexBufferMemory != nullptr) {
            vkFreeMemory(Device, IndexBufferMemory, nullptr);
            IndexBufferMemory = VK_NULL_HANDLE;
        }

        // Create staging buffer for index data
        VkBuffer staging_idx_buf {};
        VkDeviceMemory staging_idx_mem {};
        AllocateBuffer(idx_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_idx_buf, staging_idx_mem);
        StagingBuffers.emplace_back(staging_idx_buf, staging_idx_mem);

        // Copy index data to staging buffer
        void* data {};
        vk_result = vkMapMemory(Device, staging_idx_mem, 0, idx_size, 0, &data);
        VerifyVkResult(vk_result);
        MemCopy(data, Indices.data(), idx_size);
        vkUnmapMemory(Device, staging_idx_mem);

        // Create GPU-local index buffer
        AllocateBuffer(idx_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, IndexBuffer, IndexBufferMemory);

        // Copy from staging to GPU-local
        VkBufferCopy copy_region {};
        copy_region.size = idx_size;

        VkCommandBufferBeginInfo begin_info {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vk_result = vkBeginCommandBuffer(StagingCommandBuffer, &begin_info);
        VerifyVkResult(vk_result);

        vkCmdCopyBuffer(StagingCommandBuffer, staging_idx_buf, IndexBuffer, 1, &copy_region);

        vk_result = vkEndCommandBuffer(StagingCommandBuffer);
        VerifyVkResult(vk_result);

        VkSubmitInfo submit_info {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &StagingCommandBuffer;

        vk_result = vkQueueSubmit(GraphicsQueue, 1, &submit_info, VK_NULL_HANDLE);
        VerifyVkResult(vk_result);
        vk_result = vkQueueWaitIdle(GraphicsQueue);
        VerifyVkResult(vk_result);

        vk_result = vkResetCommandBuffer(StagingCommandBuffer, 0);
        VerifyVkResult(vk_result);
    }

    StaticDataChanged = false;
}

void Vulkan_Effect::DrawBuffer(RenderDrawBuffer* dbuf, size_t start_index, optional<size_t> indices_to_draw, const RenderTexture* custom_tex)
{
    FO_STACK_TRACE_ENTRY();

    if (dbuf == nullptr || Device == nullptr || CurrentCommandBuffer == nullptr) {
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
    vkCmdBindVertexBuffers(CurrentCommandBuffer, 0, 1, &vk_dbuf->VertexBuffer, offsets);

    // Process each pass
    for (size_t pass = 0; pass < _passCount; pass++) {
        // Bind pipeline for current pass
        if (Pipeline[pass] != nullptr) {
            vkCmdBindPipeline(CurrentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline[pass]);
        }

        // Bind texture descriptor set using effect's pipeline layout
        if (main_tex != nullptr && main_tex->DescriptorSet != nullptr && PipelineLayout != nullptr) {
            vkCmdBindDescriptorSets(CurrentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, PipelineLayout, 0, 1, &main_tex->DescriptorSet, 0, nullptr);
        }

        // Draw indexed or non-indexed
        if (vk_dbuf->IndCount != 0 && vk_dbuf->IndexBuffer != nullptr) {
            // Bind index buffer and draw indexed
            vkCmdBindIndexBuffer(CurrentCommandBuffer, vk_dbuf->IndexBuffer, 0, VK_INDEX_TYPE_UINT16);

            const size_t num_indices = indices_to_draw.value_or(vk_dbuf->IndCount);
            vkCmdDrawIndexed(CurrentCommandBuffer, numeric_cast<uint32_t>(num_indices), 1, numeric_cast<uint32_t>(start_index), 0, 0);
        }
        else if (vk_dbuf->VertCount != 0) {
            // Draw without indices
            const size_t num_vertices = vk_dbuf->VertCount;
            vkCmdDraw(CurrentCommandBuffer, numeric_cast<uint32_t>(num_vertices), 1, numeric_cast<uint32_t>(start_index), 0);
        }
    }
}

Vulkan_Renderer::~Vulkan_Renderer()
{
    FO_STACK_TRACE_ENTRY();

    if (Device != nullptr) {
        vkDeviceWaitIdle(Device);
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

    if (RenderPass != nullptr) {
        vkDestroyRenderPass(Device, RenderPass, nullptr);
        RenderPass = VK_NULL_HANDLE;
    }

    // Destroy pipeline and pipeline layout
    if (GraphicsPipeline != nullptr) {
        vkDestroyPipeline(Device, GraphicsPipeline, nullptr);
        GraphicsPipeline = VK_NULL_HANDLE;
    }
    if (PipelineLayout != nullptr) {
        vkDestroyPipelineLayout(Device, PipelineLayout, nullptr);
        PipelineLayout = VK_NULL_HANDLE;
    }
    if (CommandPool != nullptr) {
        vkDestroyCommandPool(Device, CommandPool, nullptr);
        CommandPool = VK_NULL_HANDLE;
    }

    for (auto* fence : FrameFences) {
        if (fence != nullptr) {
            vkDestroyFence(Device, fence, nullptr);
        }
    }
    for (auto* semaphore : ImageAvailableSemaphores) {
        if (semaphore != nullptr) {
            vkDestroySemaphore(Device, semaphore, nullptr);
        }
    }
    for (auto* semaphore : RenderCompleteSemaphores) {
        if (semaphore != nullptr) {
            vkDestroySemaphore(Device, semaphore, nullptr);
        }
    }

    FrameFences.clear();
    ImageAvailableSemaphores.clear();
    RenderCompleteSemaphores.clear();

    if (Swapchain != nullptr) {
        vkDestroySwapchainKHR(Device, Swapchain, nullptr);
        Swapchain = VK_NULL_HANDLE;
    }
    if (TextureDescriptorSetLayout != nullptr) {
        vkDestroyDescriptorSetLayout(Device, TextureDescriptorSetLayout, nullptr);
        TextureDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (DescriptorPool != nullptr) {
        vkDestroyDescriptorPool(Device, DescriptorPool, nullptr);
        DescriptorPool = VK_NULL_HANDLE;
    }
    if (TextureSampler != nullptr) {
        vkDestroySampler(Device, TextureSampler, nullptr);
        TextureSampler = VK_NULL_HANDLE;
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
    layout_ci.setLayoutCount = 1;
    layout_ci.pSetLayouts = &TextureDescriptorSetLayout;

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

        VkPipelineInputAssemblyStateCreateInfo input_assembly_ci {};
        input_assembly_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_ci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly_ci.primitiveRestartEnable = VK_FALSE;

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

        if (vkCreateGraphicsPipelines(Device, nullptr, 1, &pipeline_ci, nullptr, &vk_effect->Pipeline[pass]) != VK_SUCCESS) {
            throw EffectLoadException("Failed to create graphics pipeline", name);
        }
    }

    return vk_effect;
}

[[nodiscard]] auto Vulkan_Renderer::CreateOrthoMatrix(float32 left, float32 right, float32 bottom, float32 top, float32 nearp, float32 farp) -> mat44
{
    FO_STACK_TRACE_ENTRY();

    mat44 m {};
    m[0][0] = 2.0f / (right - left);
    m[1][1] = 2.0f / (top - bottom);
    m[2][2] = -2.0f / (farp - nearp);
    m[3][0] = -(right + left) / (right - left);
    m[3][1] = -(top + bottom) / (top - bottom);
    m[3][2] = -(farp + nearp) / (farp - nearp);
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

    // Check for swapchain extension support
    /*uint32_t device_ext_count = 0;
    vk_result = vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &device_ext_count, nullptr);
    VerifyVkResult(vk_result);
    vector<VkExtensionProperties> ext_props(device_ext_count);
    vk_result = vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &device_ext_count, ext_props.data());
    VerifyVkResult(vk_result);

    if (!std::ranges::any_of(exts, exts + device_ext_count, [](auto&& ext) { return string_view(ext) == VK_KHR_SWAPCHAIN_EXTENSION_NAME; })) {
        throw RenderingException("VK_KHR_swapchain extension not supported");
    }*/

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
    RecreateSwapchain({width, height});

    // Allocate command buffers for rendering (3 for triple-buffering)
    VkCommandBufferAllocateInfo cbai {};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = CommandPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 3;

    CommandBuffers.resize(3);
    vk_result = vkAllocateCommandBuffers(Device, &cbai, CommandBuffers.data());
    VerifyVkResult(vk_result);

    // Create frame fences and semaphores for synchronization
    for (size_t i = 0; i < 3; i++) {
        VkFenceCreateInfo fci {};
        fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start signaled so first frame can proceed
        vk_result = vkCreateFence(Device, &fci, nullptr, &FrameFences[i]);
        VerifyVkResult(vk_result);

        // Create semaphores for frame synchronization
        VkSemaphoreCreateInfo sci {};
        sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        vk_result = vkCreateSemaphore(Device, &sci, nullptr, &ImageAvailableSemaphores[i]);
        VerifyVkResult(vk_result);
        vk_result = vkCreateSemaphore(Device, &sci, nullptr, &RenderCompleteSemaphores[i]);
        VerifyVkResult(vk_result);
    }

    // Allocate staging command buffer for buffer uploads
    VkCommandBufferAllocateInfo staging_cbai {};
    staging_cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    staging_cbai.commandPool = CommandPool;
    staging_cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    staging_cbai.commandBufferCount = 1;

    vk_result = vkAllocateCommandBuffers(Device, &staging_cbai, &StagingCommandBuffer);
    VerifyVkResult(vk_result);

    // Create descriptor set layout for texture binding
    VkDescriptorSetLayoutBinding binding {};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layout_ci {};
    layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_ci.bindingCount = 1;
    layout_ci.pBindings = &binding;

    vk_result = vkCreateDescriptorSetLayout(Device, &layout_ci, nullptr, &TextureDescriptorSetLayout);
    VerifyVkResult(vk_result);

    // Create descriptor pool for texture descriptor sets (allocate space for many textures)
    VkDescriptorPoolSize pool_size {};
    pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size.descriptorCount = 10000; // Allow many textures

    VkDescriptorPoolCreateInfo pool_ci {};
    pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_ci.poolSizeCount = 1;
    pool_ci.pPoolSizes = &pool_size;
    pool_ci.maxSets = 10000;

    vk_result = vkCreateDescriptorPool(Device, &pool_ci, nullptr, &DescriptorPool);
    VerifyVkResult(vk_result);

    // Create sampler for texture filtering
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

    vk_result = vkCreateSampler(Device, &sampler_ci, nullptr, &TextureSampler);
    VerifyVkResult(vk_result);

    // Update pipeline layout to include descriptor set layout for textures
    VkPipelineLayoutCreateInfo pipe_layout_ci {};
    pipe_layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipe_layout_ci.setLayoutCount = 1;
    pipe_layout_ci.pSetLayouts = &TextureDescriptorSetLayout;

    vk_result = vkCreatePipelineLayout(Device, &pipe_layout_ci, nullptr, &PipelineLayout);
    VerifyVkResult(vk_result);

    DummyTexture = CreateTexture({1, 1}, false, false);
}

static void RecreateSwapchain(isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(size.width > 0);
    FO_RUNTIME_ASSERT(size.height > 0);

    VkResult vk_result = VK_SUCCESS;

    // Destroy old framebuffers and image views
    for (auto* fb : Framebuffers) {
        vkDestroyFramebuffer(Device, fb, nullptr);
    }

    Framebuffers.clear();

    for (auto* iv : SwapchainImageViews) {
        vkDestroyImageView(Device, iv, nullptr);
    }

    SwapchainImageViews.clear();

    if (RenderPass != nullptr) {
        vkDestroyRenderPass(Device, RenderPass, nullptr);
        RenderPass = VK_NULL_HANDLE;
    }

    if (Swapchain != nullptr) {
        vkDeviceWaitIdle(Device);
        vkDestroySwapchainKHR(Device, Swapchain, nullptr);
        Swapchain = VK_NULL_HANDLE;
        SwapchainImages.clear();
    }

    // Create swapchain
    VkSurfaceCapabilitiesKHR caps {};
    vk_result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, Surface, &caps);
    VerifyVkResult(vk_result);

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
    sci.imageExtent = {.width = numeric_cast<uint32_t>(size.width), .height = numeric_cast<uint32_t>(size.height)};
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
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
    SwapchainFormat = sci.imageFormat;

    // Create render pass
    VkAttachmentDescription color_attachment {};
    color_attachment.format = SwapchainFormat;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkRenderPassCreateInfo rp_info {};
    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.attachmentCount = 1;
    rp_info.pAttachments = &color_attachment;
    rp_info.subpassCount = 1;
    rp_info.pSubpasses = &subpass;

    vk_result = vkCreateRenderPass(Device, &rp_info, nullptr, &RenderPass);
    VerifyVkResult(vk_result);

    // Create framebuffers
    SwapchainImageViews.resize(SwapchainImages.size());
    Framebuffers.resize(SwapchainImages.size());

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

        VkFramebufferCreateInfo fci {};
        fci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fci.renderPass = RenderPass;
        fci.attachmentCount = 1;
        fci.pAttachments = &SwapchainImageViews[i];
        fci.width = numeric_cast<uint32_t>(size.width);
        fci.height = numeric_cast<uint32_t>(size.height);
        fci.layers = 1;

        vk_result = vkCreateFramebuffer(Device, &fci, nullptr, &Framebuffers[i]);
        VerifyVkResult(vk_result);
    }
}

void Vulkan_Renderer::Present()
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(Swapchain);
    FO_RUNTIME_ASSERT(Device);

    VkResult vk_result = VK_SUCCESS;

    uint32_t img_idx = 0;
    const size_t frame_buf_idx = FrameIndex % 3;
    const VkSemaphore image_available = ImageAvailableSemaphores[frame_buf_idx];
    const VkSemaphore render_complete = RenderCompleteSemaphores[frame_buf_idx];
    const VkCommandBuffer cmd_buf = CommandBuffers[frame_buf_idx];
    const VkFence frame_fence = FrameFences[frame_buf_idx];

    // Acquire next image with semaphore signal
    vk_result = vkAcquireNextImageKHR(Device, Swapchain, UINT64_MAX, image_available, VK_NULL_HANDLE, &img_idx);
    VerifyVkResult(vk_result);

    // Wait for the fence to be signaled (frame is done rendering)
    vk_result = vkWaitForFences(Device, 1, &frame_fence, VK_TRUE, UINT64_MAX);
    VerifyVkResult(vk_result);
    vk_result = vkResetFences(Device, 1, &frame_fence);
    VerifyVkResult(vk_result);

    // Reset command buffer
    vk_result = vkResetCommandBuffer(cmd_buf, 0);
    VerifyVkResult(vk_result);

    // Begin command buffer
    VkCommandBufferBeginInfo begin_info {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vk_result = vkBeginCommandBuffer(cmd_buf, &begin_info);
    VerifyVkResult(vk_result);

    // Begin render pass
    VkRenderPassBeginInfo rp_begin {};
    rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin.renderPass = RenderPass;
    rp_begin.framebuffer = Framebuffers[img_idx];
    rp_begin.renderArea.offset = {.x = 0, .y = 0};
    rp_begin.renderArea.extent = {.width = numeric_cast<uint32_t>(ViewPort.width), .height = numeric_cast<uint32_t>(ViewPort.height)};
    rp_begin.clearValueCount = 1;
    rp_begin.pClearValues = reinterpret_cast<VkClearValue*>(&ClearColor);

    CurrentCommandBuffer = cmd_buf; // Set current buffer for DrawBuffer calls
    vkCmdBeginRenderPass(cmd_buf, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, GraphicsPipeline);

    // Set viewport to match the render area
    VkViewport viewport {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = numeric_cast<float32>(ViewPort.width);
    viewport.height = numeric_cast<float32>(ViewPort.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd_buf, 0, 1, &viewport);

    // Apply to scissor rectangle if enabled, otherwise use viewport rect
    if (ScissorEnabled) {
        VkRect2D scissor_rect {};
        scissor_rect.offset = {.x = numeric_cast<int32_t>(ScissorRect.x), .y = numeric_cast<int32_t>(ScissorRect.y)};
        scissor_rect.extent = {.width = numeric_cast<uint32_t>(std::max(1, ScissorRect.width)), .height = numeric_cast<uint32_t>(std::max(1, ScissorRect.height))};
        vkCmdSetScissor(cmd_buf, 0, 1, &scissor_rect);
    }
    else {
        VkRect2D scissor_rect {};
        scissor_rect.offset = {.x = 0, .y = 0};
        scissor_rect.extent = {.width = numeric_cast<uint32_t>(ViewPort.width), .height = numeric_cast<uint32_t>(ViewPort.height)};
        vkCmdSetScissor(cmd_buf, 0, 1, &scissor_rect);
    }

    // Note: DrawBuffer() calls from game logic will record commands into CurrentCommandBuffer
    // For now, draw a default triangle if no custom draw calls were made
    vkCmdDraw(cmd_buf, 3, 1, 0, 0);

    vkCmdEndRenderPass(cmd_buf);
    CurrentCommandBuffer = VK_NULL_HANDLE;

    // End command buffer
    vk_result = vkEndCommandBuffer(cmd_buf);
    VerifyVkResult(vk_result);

    // Submit to graphics queue with proper synchronization
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &image_available; // Wait for image to be acquired
    submit_info.pWaitDstStageMask = &wait_stage;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buf;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &render_complete; // Signal when render done

    vk_result = vkQueueSubmit(GraphicsQueue, 1, &submit_info, frame_fence);
    VerifyVkResult(vk_result);

    FrameIndex++;

    // Present to screen with proper synchronization
    VkPresentInfoKHR info {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &render_complete; // Wait for render to complete
    info.swapchainCount = 1;
    info.pSwapchains = &Swapchain;
    info.pImageIndices = &img_idx;

    vk_result = vkQueuePresentKHR(GraphicsQueue, &info);
    VerifyVkResult(vk_result);
}

void Vulkan_Renderer::SetRenderTarget(RenderTexture* tex)
{
    FO_STACK_TRACE_ENTRY();

    CurrentRenderTarget = tex;
    // Note: Full implementation would require creating Vulkan framebuffers from texture images
    // and switching render passes. For now, this stores the target for future use.
}

void Vulkan_Renderer::ClearRenderTarget(optional<ucolor> color, bool /*depth*/, bool /*stencil*/)
{
    FO_STACK_TRACE_ENTRY();

    if (color.has_value()) {
        const auto c = color.value().underlying_value();
        ClearColor.float32[0] = numeric_cast<float32>((c >> 16) & 0xFF) / 255.0f; // R
        ClearColor.float32[1] = numeric_cast<float32>((c >> 8) & 0xFF) / 255.0f; // G
        ClearColor.float32[2] = numeric_cast<float32>(c & 0xFF) / 255.0f; // B
        ClearColor.float32[3] = numeric_cast<float32>((c >> 24) & 0xFF) / 255.0f; // A
    }
}

void Vulkan_Renderer::EnableScissor(irect32 rect)
{
    FO_STACK_TRACE_ENTRY();

    ScissorRect = rect;
    ScissorEnabled = true;
}

void Vulkan_Renderer::DisableScissor()
{
    FO_STACK_TRACE_ENTRY();

    ScissorEnabled = false;
}

void Vulkan_Renderer::OnResizeWindow(isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    ViewPort = irect32 {{0, 0}, {std::max(size.width, 1), std::max(size.height, 1)}};
    RecreateSwapchain(size);
}

FO_END_NAMESPACE

#endif // FO_HAVE_VULKAN
