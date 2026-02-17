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
static VkFormat SwapchainFormat {};
static VkExtent2D SwapchainExtent {};
static irect32 ViewPort {};
static unique_ptr<RenderTexture> DummyTexture {};

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
public:
    explicit Vulkan_DrawBuffer(bool is_static) :
        RenderDrawBuffer(is_static)
    {
        FO_STACK_TRACE_ENTRY();
    }
    ~Vulkan_DrawBuffer() override = default;

    void Upload(EffectUsage /*usage*/, optional<size_t> /*custom_vertices_size*/, optional<size_t> /*custom_indices_size*/) override
    {
        FO_STACK_TRACE_ENTRY();

        StaticDataChanged = false;
    }
};

class Vulkan_Effect final : public RenderEffect
{
public:
    Vulkan_Effect(EffectUsage usage, string_view name, const RenderEffectLoader& loader) :
        RenderEffect(usage, name, loader)
    {
        FO_STACK_TRACE_ENTRY();
    }

    ~Vulkan_Effect() override = default;

    void DrawBuffer(RenderDrawBuffer* /*dbuf*/, size_t /*start_index*/, optional<size_t> /*indices_to_draw*/, const RenderTexture* /*custom_tex*/) override
    {
        FO_STACK_TRACE_ENTRY();
        // no-op
    }
};

Vulkan_Renderer::~Vulkan_Renderer()
{
    FO_STACK_TRACE_ENTRY();

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

    return SafeAlloc::MakeUnique<Vulkan_Effect>(usage, name, loader);
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

    if (settings.RenderDebug) {
        const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
        dci.enabledLayerCount = 1;
        dci.ppEnabledLayerNames = layers;
    }

    result = vkCreateDevice(PhysicalDevice, &dci, nullptr, &Device);

    if (result != VK_SUCCESS) {
        throw RenderingException("vkCreateDevice failed");
    }

    vkGetDeviceQueue(Device, graphics_family.value(), 0, &GraphicsQueue);
    GraphicsFamilyIndex = graphics_family.value();

    ViewPort = irect32 {{0, 0}, {0, 0}};

    // Initialize swapchain for current window size0
    int w;
    int h;
    SDL_GetWindowSizeInPixels(SdlWindow.get(), &w, &h);
    RecreateSwapchain({w, h});

    DummyTexture = CreateTexture({1, 1}, false, false);
}

static void RecreateSwapchain(isize32 size)
{
    FO_STACK_TRACE_ENTRY();

    if (Swapchain != nullptr) {
        vkDeviceWaitIdle(Device);
        vkDestroySwapchainKHR(Device, Swapchain, nullptr);
        Swapchain = VK_NULL_HANDLE;
        SwapchainImages.clear();
    }

    VkExtent2D ext {};

    if (size.width > 0 && size.height > 0) {
        ext.width = numeric_cast<uint32_t>(size.width);
        ext.height = numeric_cast<uint32_t>(size.height);
    }
    else {
        int w;
        int h;
        SDL_GetWindowSizeInPixels(SdlWindow.get(), &w, &h);
        ext.width = static_cast<uint32_t>(w);
        ext.height = static_cast<uint32_t>(h);
    }

    SwapchainExtent = ext;

    VkSurfaceCapabilitiesKHR caps {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, Surface, &caps);

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
        imageCount = caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR sci {};
    sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = Surface;
    sci.minImageCount = imageCount;
    sci.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    sci.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    sci.imageExtent = SwapchainExtent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    sci.clipped = VK_TRUE;
    sci.oldSwapchain = VK_NULL_HANDLE;

    vkCreateSwapchainKHR(Device, &sci, nullptr, &Swapchain);
    vkGetSwapchainImagesKHR(Device, Swapchain, &imageCount, nullptr);
    SwapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(Device, Swapchain, &imageCount, SwapchainImages.data());
    SwapchainFormat = sci.imageFormat;
}

void Vulkan_Renderer::Present()
{
    FO_STACK_TRACE_ENTRY();

    if (Swapchain == nullptr) {
        return;
    }

    uint32_t img_idx = 0;
    VkResult result = vkAcquireNextImageKHR(Device, Swapchain, UINT64_MAX, VK_NULL_HANDLE, VK_NULL_HANDLE, &img_idx);

    if (result != VK_SUCCESS) {
        return;
    }

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

void Vulkan_Renderer::ClearRenderTarget(optional<ucolor> /*color*/, bool /*depth*/, bool /*stencil*/)
{
    FO_STACK_TRACE_ENTRY();
    // not implemented
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
