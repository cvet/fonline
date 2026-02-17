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
static vector<VkCommandBuffer> CommandBuffers;
static vector<VkFence> FrameFences; // One fence per frame buffer for synchronization
static irect32 ViewPort {};
static unique_ptr<RenderTexture> DummyTexture {};
static VkClearColorValue ClearColor {{0.0f, 0.0f, 0.0f, 1.0f}};
static uint32_t FrameIndex = 0;
static VkPipelineLayout PipelineLayout {};
static VkPipeline GraphicsPipeline {};

static void RecreateSwapchain(isize32 size);

class Vulkan_Texture final : public RenderTexture
{
public:
    Vulkan_Texture(isize32 size, bool linear_filtered, bool with_depth) :
        RenderTexture(size, linear_filtered, with_depth),
        _pixels(size.square())
    {
        FO_STACK_TRACE_ENTRY();
    }

    ~Vulkan_Texture() override = default;

    [[nodiscard]] auto GetTexturePixel(ipos32 pos) const -> ucolor override
    {
        FO_STACK_TRACE_ENTRY();

        const auto idx = numeric_cast<size_t>(pos.y) * Size.width + numeric_cast<size_t>(pos.x);
        return idx < _pixels.size() ? _pixels[idx] : ucolor::clear;
    }

    [[nodiscard]] auto GetTextureRegion(ipos32 pos, isize32 size) const -> vector<ucolor> override
    {
        FO_STACK_TRACE_ENTRY();

        vector<ucolor> result;
        result.reserve(size.square());

        for (int32 y = 0; y < size.height; y++) {
            for (int32 x = 0; x < size.width; x++) {
                result.emplace_back(GetTexturePixel({pos.x + x, pos.y + y}));
            }
        }

        return result;
    }

    void UpdateTextureRegion(ipos32 pos, isize32 size, const ucolor* data, bool use_dest_pitch) override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(use_dest_pitch);

        for (int32 y = 0; y < size.height; y++) {
            for (int32 x = 0; x < size.width; x++) {
                const auto dst_idx = (numeric_cast<size_t>(pos.y + y) * Size.width) + numeric_cast<size_t>(pos.x + x);
                const auto src_idx = numeric_cast<size_t>(y) * size.width + numeric_cast<size_t>(x);

                if (dst_idx < _pixels.size()) {
                    _pixels[dst_idx] = data[src_idx];
                }
            }
        }
    }

private:
    vector<ucolor> _pixels;
};

class Vulkan_DrawBuffer final : public RenderDrawBuffer
{
    friend class Vulkan_Effect;

public:
    explicit Vulkan_DrawBuffer(bool is_static) :
        RenderDrawBuffer(is_static)
    {
        FO_STACK_TRACE_ENTRY();
    }
    ~Vulkan_DrawBuffer() override = default;

    void Upload(EffectUsage usage, optional<size_t> custom_vertices_size, optional<size_t> custom_indices_size) override
    {
        FO_STACK_TRACE_ENTRY();

        ignore_unused(usage, custom_vertices_size, custom_indices_size);

        // GPU buffer upload will be implemented when we add buffer management
        StaticDataChanged = false;
    }

    // GPU resources for Vulkan rendering
    // TODO: VkBuffer for vertices and indices with associated device memory
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
    // TODO: VkDescriptorSet for each pass for texture and constant buffer bindings
private:
};

Vulkan_Effect::~Vulkan_Effect()
{
    FO_STACK_TRACE_ENTRY();

    if (Device == nullptr) {
        return;
    }

    for (size_t pass = 0; pass < EFFECT_MAX_PASSES; pass++) {
        if (VertexShaderModule[pass] != nullptr) {
            vkDestroyShaderModule(Device, VertexShaderModule[pass], nullptr);
            VertexShaderModule[pass] = VK_NULL_HANDLE;
        }
        if (FragmentShaderModule[pass] != nullptr) {
            vkDestroyShaderModule(Device, FragmentShaderModule[pass], nullptr);
            FragmentShaderModule[pass] = VK_NULL_HANDLE;
        }
    }
}

void Vulkan_Effect::DrawBuffer(RenderDrawBuffer* dbuf, size_t start_index, optional<size_t> indices_to_draw, const RenderTexture* custom_tex)
{
    FO_STACK_TRACE_ENTRY();

    if (dbuf == nullptr || Device == nullptr || GraphicsPipeline == nullptr) {
        return;
    }

    const auto* vk_dbuf = static_cast<Vulkan_DrawBuffer*>(dbuf); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

#if FO_ENABLE_3D
    const auto* main_tex = static_cast<const Vulkan_Texture*>(custom_tex != nullptr ? custom_tex : (ModelTex[0] ? ModelTex[0].get() : (MainTex ? MainTex.get() : DummyTexture.get()))); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
#else
    const auto* main_tex = static_cast<const Vulkan_Texture*>(custom_tex != nullptr ? custom_tex : (MainTex ? MainTex.get() : DummyTexture.get())); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
#endif

    ignore_unused(vk_dbuf, main_tex, start_index, indices_to_draw);

    // TODO: Implement Vulkan-specific rendering:
    // 1. Get current command buffer from Present()
    // 2. Bind vertex/index buffers
    // 3. Bind textures and constant buffers via descriptor sets
    // 4. Record vkCmdDraw or vkCmdDrawIndexed commands
    // 5. Handle different effect usage modes (Sprite, Primitive, Model)
}

Vulkan_Renderer::~Vulkan_Renderer()
{
    FO_STACK_TRACE_ENTRY();

    if (Device != nullptr) {
        vkDeviceWaitIdle(Device);
    }

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

    FrameFences.clear();

    if (Swapchain != nullptr) {
        vkDestroySwapchainKHR(Device, Swapchain, nullptr);
        Swapchain = VK_NULL_HANDLE;
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

    return SafeAlloc::MakeUnique<Vulkan_Texture>(size, linear_filtered, with_depth);
}

[[nodiscard]] auto Vulkan_Renderer::CreateDrawBuffer(bool is_static) -> unique_ptr<RenderDrawBuffer>
{
    FO_STACK_TRACE_ENTRY();

    return SafeAlloc::MakeUnique<Vulkan_DrawBuffer>(is_static);
}

[[nodiscard]] auto Vulkan_Renderer::CreateEffect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) -> unique_ptr<RenderEffect>
{
    FO_STACK_TRACE_ENTRY();

    auto vk_effect = SafeAlloc::MakeUnique<Vulkan_Effect>(usage, name, loader);

    for (size_t pass = 0; pass < vk_effect->_passCount; pass++) {
        // Load vertex shader SPIR-V
        {
            const string vert_fname = strex("{}.fofx-{}-vert-spirv", strex(name).erase_file_extension(), pass + 1);
            string vert_content = loader(vert_fname);
            FO_RUNTIME_ASSERT(!vert_content.empty());
            FO_RUNTIME_ASSERT(vert_content.length() % sizeof(uint32_t) == 0);

            VkShaderModuleCreateInfo module_ci {};
            module_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            module_ci.codeSize = vert_content.length();
            module_ci.pCode = reinterpret_cast<const uint32_t*>(vert_content.data());

            if (vkCreateShaderModule(Device, &module_ci, nullptr, &vk_effect->VertexShaderModule[pass]) != VK_SUCCESS) {
                throw EffectLoadException("Failed to create vertex shader module", vert_fname, vert_content);
            }
        }

        // Load fragment shader SPIR-V
        {
            const string frag_fname = strex("{}.fofx-{}-frag-spirv", strex(name).erase_file_extension(), pass + 1);
            string frag_content = loader(frag_fname);
            FO_RUNTIME_ASSERT(!frag_content.empty());
            FO_RUNTIME_ASSERT(frag_content.length() % sizeof(uint32_t) == 0);

            VkShaderModuleCreateInfo module_ci {};
            module_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            module_ci.codeSize = frag_content.length();
            module_ci.pCode = reinterpret_cast<const uint32_t*>(frag_content.data());

            if (vkCreateShaderModule(Device, &module_ci, nullptr, &vk_effect->FragmentShaderModule[pass]) != VK_SUCCESS) {
                throw EffectLoadException("Failed to create fragment shader module", frag_fname, frag_content);
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

    VkResult result = vkCreateInstance(&create_info, nullptr, &Instance);

    if (result != VK_SUCCESS) {
        throw RenderingException("vkCreateInstance failed", result);
    }

    if (!SDL_Vulkan_CreateSurface(SdlWindow.get(), Instance, nullptr, &Surface)) {
        throw RenderingException("SDL_Vulkan_CreateSurface failed");
    }

    // Pick first physical device supporting graphics
    uint32_t gpu_count = 0;
    result = vkEnumeratePhysicalDevices(Instance, &gpu_count, nullptr);
    FO_RUNTIME_ASSERT(result == VK_SUCCESS);

    if (gpu_count == 0) {
        throw RenderingException("No Vulkan physical devices found");
    }

    vector<VkPhysicalDevice> gpus;
    gpus.resize(gpu_count);
    result = vkEnumeratePhysicalDevices(Instance, &gpu_count, gpus.data());
    FO_RUNTIME_ASSERT(result == VK_SUCCESS);
    PhysicalDevice = gpus.front();

    // Find queue family
    uint32_t qcount;
    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &qcount, nullptr);
    vector<VkQueueFamilyProperties> qprops(qcount);
    qprops.resize(qcount);
    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &qcount, qprops.data());

    optional<uint32_t> graphics_family;

    for (uint32_t i = 0; i < qcount; i++) {
        VkBool32 present_support = VK_FALSE;
        result = vkGetPhysicalDeviceSurfaceSupportKHR(PhysicalDevice, i, Surface, &present_support);
        FO_RUNTIME_ASSERT(result == VK_SUCCESS);

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

    const char* debug_layers[] = {"VK_LAYER_KHRONOS_validation"};
    dci.enabledLayerCount = settings.RenderDebug ? 1 : 0;
    dci.ppEnabledLayerNames = debug_layers;

    result = vkCreateDevice(PhysicalDevice, &dci, nullptr, &Device);
    FO_RUNTIME_ASSERT(result == VK_SUCCESS);

    vkGetDeviceQueue(Device, graphics_family.value(), 0, &GraphicsQueue);
    GraphicsFamilyIndex = graphics_family.value();

    // Create command pool for recording render commands
    VkCommandPoolCreateInfo cpi {};
    cpi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpi.queueFamilyIndex = GraphicsFamilyIndex;
    result = vkCreateCommandPool(Device, &cpi, nullptr, &CommandPool);
    FO_RUNTIME_ASSERT(result == VK_SUCCESS);

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
    result = vkAllocateCommandBuffers(Device, &cbai, CommandBuffers.data());
    FO_RUNTIME_ASSERT(result == VK_SUCCESS);

    // Create frame fences for synchronization
    FrameFences.resize(3);
    VkFenceCreateInfo fci {};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start signaled so first frame can proceed

    for (auto& fence : FrameFences) {
        result = vkCreateFence(Device, &fci, nullptr, &fence);
        FO_RUNTIME_ASSERT(result == VK_SUCCESS);
    }

    DummyTexture = CreateTexture({1, 1}, false, false);
}

static void RecreateSwapchain(isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(size.width > 0);
    FO_RUNTIME_ASSERT(size.height > 0);

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
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, Surface, &caps);
    FO_RUNTIME_ASSERT(result == VK_SUCCESS);

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

    result = vkCreateSwapchainKHR(Device, &sci, nullptr, &Swapchain);
    FO_RUNTIME_ASSERT(result == VK_SUCCESS);
    result = vkGetSwapchainImagesKHR(Device, Swapchain, &image_count, nullptr);
    FO_RUNTIME_ASSERT(result == VK_SUCCESS);
    SwapchainImages.resize(image_count);
    result = vkGetSwapchainImagesKHR(Device, Swapchain, &image_count, SwapchainImages.data());
    FO_RUNTIME_ASSERT(result == VK_SUCCESS);
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

    result = vkCreateRenderPass(Device, &rp_info, nullptr, &RenderPass);
    FO_RUNTIME_ASSERT(result == VK_SUCCESS);

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

        result = vkCreateImageView(Device, &ivci, nullptr, &SwapchainImageViews[i]);
        FO_RUNTIME_ASSERT(result == VK_SUCCESS);

        VkFramebufferCreateInfo fci {};
        fci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fci.renderPass = RenderPass;
        fci.attachmentCount = 1;
        fci.pAttachments = &SwapchainImageViews[i];
        fci.width = numeric_cast<uint32_t>(size.width);
        fci.height = numeric_cast<uint32_t>(size.height);
        fci.layers = 1;

        result = vkCreateFramebuffer(Device, &fci, nullptr, &Framebuffers[i]);
        FO_RUNTIME_ASSERT(result == VK_SUCCESS);
    }
}

void Vulkan_Renderer::Present()
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(Swapchain);
    FO_RUNTIME_ASSERT(Device);

    uint32_t img_idx = 0;
    VkResult result = vkAcquireNextImageKHR(Device, Swapchain, UINT64_MAX, VK_NULL_HANDLE, VK_NULL_HANDLE, &img_idx);
    FO_RUNTIME_ASSERT(result == VK_SUCCESS);

    // Use command buffer (rotate through 3 for consistent frame pacing)
    const uint32_t frame_buf_idx = FrameIndex % 3;
    const VkCommandBuffer cmd_buf = CommandBuffers[frame_buf_idx];
    const VkFence frame_fence = FrameFences[frame_buf_idx];

    // Wait for the fence to be signaled (frame is done rendering)
    result = vkWaitForFences(Device, 1, &frame_fence, VK_TRUE, UINT64_MAX);
    FO_RUNTIME_ASSERT(result == VK_SUCCESS);
    result = vkResetFences(Device, 1, &frame_fence);
    FO_RUNTIME_ASSERT(result == VK_SUCCESS);

    // Reset command buffer
    result = vkResetCommandBuffer(cmd_buf, 0);
    FO_RUNTIME_ASSERT(result == VK_SUCCESS);

    // Begin command buffer
    VkCommandBufferBeginInfo begin_info {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    result = vkBeginCommandBuffer(cmd_buf, &begin_info);
    FO_RUNTIME_ASSERT(result == VK_SUCCESS);

    // Begin render pass
    VkRenderPassBeginInfo rp_begin {};
    rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin.renderPass = RenderPass;
    rp_begin.framebuffer = Framebuffers[img_idx];
    rp_begin.renderArea.offset = {.x = 0, .y = 0};
    rp_begin.renderArea.extent = {.width = numeric_cast<uint32_t>(ViewPort.width), .height = numeric_cast<uint32_t>(ViewPort.height)};
    rp_begin.clearValueCount = 1;
    rp_begin.pClearValues = reinterpret_cast<VkClearValue*>(&ClearColor);

    vkCmdBeginRenderPass(cmd_buf, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, GraphicsPipeline);
    vkCmdDraw(cmd_buf, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd_buf);

    // End command buffer
    vkEndCommandBuffer(cmd_buf);

    // Submit to graphics queue with frame fence
    VkSubmitInfo submit_info {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buf;

    result = vkQueueSubmit(GraphicsQueue, 1, &submit_info, frame_fence);
    FO_RUNTIME_ASSERT(result == VK_SUCCESS);

    FrameIndex++;

    // Present to screen
    VkPresentInfoKHR info {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.swapchainCount = 1;
    info.pSwapchains = &Swapchain;
    info.pImageIndices = &img_idx;

    result = vkQueuePresentKHR(GraphicsQueue, &info);
    FO_RUNTIME_ASSERT(result == VK_SUCCESS);
}

void Vulkan_Renderer::SetRenderTarget(RenderTexture* /*tex*/)
{
    FO_STACK_TRACE_ENTRY();
    // not implemented
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

void Vulkan_Renderer::EnableScissor(irect32 /*rect*/)
{
    FO_STACK_TRACE_ENTRY();
    // not implemented
}

void Vulkan_Renderer::DisableScissor()
{
    FO_STACK_TRACE_ENTRY();
    // not implemented
}

void Vulkan_Renderer::OnResizeWindow(isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    ViewPort = irect32 {{0, 0}, size};
    RecreateSwapchain(size);
}

FO_END_NAMESPACE

#endif // FO_HAVE_VULKAN
