#include "Window.h"
#include "Backend/VulkanInstance.h"
#include "Backend/VulkanPhysicalDevice.h"
#include <Backends/imgui_impl_vulkan.h>
#include "GLFW/glfw3.h"
#include "Log.h"
#include "vulkan/vulkan_core.h"
namespace Sera {
  Window* Window::Create(VulkanInstance*        instance,
                         VulkanPhysicalDevice*  pDevice,
                         VkAllocationCallbacks* allocator, VulkanDevice* device,
                         bool isVsync) {
    Window* w           = new Window;
    w->m_VkInstance     = instance;
    w->m_PhysicalDevice = pDevice;
    w->m_Allocator      = allocator;
    w->m_Vsync          = isVsync;
    w->m_Device         = device;
    w->m_Surface        = VK_NULL_HANDLE;
    w->swapchain        = VK_NULL_HANDLE;
    w->m_Handle = glfwCreateWindow(300, 300, "Sera app", nullptr, nullptr);

    glfwGetFramebufferSize(w->m_Handle, &w->Width, &w->Height);

    w->CreateSurface();
    w->CreateSwapchain();
    w->CreateCommandBuffers();
    return w;
  }
  void Window::CreateSurface() {
    VkBool32 res;
    VkResult err;
    if (m_Surface != VK_NULL_HANDLE) {
      vkGetPhysicalDeviceSurfaceSupportKHR(m_PhysicalDevice->physicalDevice,
                                           m_PhysicalDevice->queueFamilyIndex,
                                           m_Surface, &res);
      if (res != VK_TRUE) {
        SR_CORE_ERROR("Error no WSI support on physical device");
        exit(-1);
      }
    } else {
      err = glfwCreateWindowSurface(m_VkInstance->instance, m_Handle,
                                    m_Allocator, &m_Surface);
      vkGetPhysicalDeviceSurfaceSupportKHR(m_PhysicalDevice->physicalDevice,
                                           m_PhysicalDevice->queueFamilyIndex,
                                           m_Surface, &res);
      if (res != VK_TRUE) {
        SR_CORE_ERROR("Error no WSI support on physical device");
        exit(-1);
      }
    }
    // Select Surface Format
    const VkFormat requestSurfaceImageFormat[] = {
        VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM};
    const VkColorSpaceKHR requestSurfaceColorSpace =
        VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
        m_PhysicalDevice->physicalDevice, m_Surface, requestSurfaceImageFormat,
        (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat),
        requestSurfaceColorSpace);
    if (m_Vsync) {
      VkPresentModeKHR present_modes[] = {VK_PRESENT_MODE_FIFO_KHR};
      m_PresentMode                    = ImGui_ImplVulkanH_SelectPresentMode(
          m_PhysicalDevice->physicalDevice, m_Surface, &present_modes[0],
          IM_ARRAYSIZE(present_modes));

    } else {
      VkPresentModeKHR present_modes[] = {VK_PRESENT_MODE_MAILBOX_KHR,
                                          VK_PRESENT_MODE_IMMEDIATE_KHR,
                                          VK_PRESENT_MODE_FIFO_KHR};
      m_PresentMode                    = ImGui_ImplVulkanH_SelectPresentMode(
          m_PhysicalDevice->physicalDevice, m_Surface, &present_modes[0],
          IM_ARRAYSIZE(present_modes));
    }
  }
  static void DestroyFrame(VkDevice device, Frame* frame,
                           VkAllocationCallbacks* allocator) {
    vkDestroyFence(device, frame->Fence, allocator);
    vkFreeCommandBuffers(device, frame->CommandPool, 1, &frame->CommandBuffer);
    vkDestroyCommandPool(device, frame->CommandPool, allocator);
    frame->Fence         = VK_NULL_HANDLE;
    frame->CommandBuffer = VK_NULL_HANDLE;
    frame->CommandPool   = VK_NULL_HANDLE;

    vkDestroyImageView(device, frame->BackbufferView, allocator);
    vkDestroyFramebuffer(device, frame->Framebuffer, allocator);
  }
  static void DestroyFrameSemaphores(VkDevice device, FrameSemaphores* frame,
                                     VkAllocationCallbacks* allocator) {
    vkDestroySemaphore(device, frame->ImageAcquiredSemaphore, allocator);
    vkDestroySemaphore(device, frame->RenderCompleteSemaphore, allocator);
    frame->ImageAcquiredSemaphore = frame->RenderCompleteSemaphore =
        VK_NULL_HANDLE;
  }
  void Window::CreateSwapchain() {
    VkSwapchainKHR oldSwapchain = swapchain;
    swapchain                   = VK_NULL_HANDLE;
    auto err                    = m_Device->WaitIdle();

    auto device = m_Device->device;
    for (uint32_t i = 0; i < ImageCount; i++)
      DestroyFrame(device, &frames[i], m_Allocator);

    for (uint32_t i = 0; i < SemaphoreCount; i++)
      DestroyFrameSemaphores(device, &frameSemaphores[i], m_Allocator);

    free(frames);
    free(frameSemaphores);

    uint32_t minImageCount = 3;

    VkSwapchainCreateInfoKHR info = {};
    info.sType                    = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface                  = m_Surface;
    info.minImageCount            = minImageCount;
    info.imageFormat              = SurfaceFormat.format;
    info.imageColorSpace          = SurfaceFormat.colorSpace;
    info.imageArrayLayers         = 1;
    info.imageUsage               = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.imageSharingMode =
        VK_SHARING_MODE_EXCLUSIVE;  // Assume that graphics family == present
                                    // family
    info.preTransform   = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode    = m_PresentMode;
    info.clipped        = VK_TRUE;
    info.oldSwapchain   = oldSwapchain;

    VkSurfaceCapabilitiesKHR cap;
    err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        m_PhysicalDevice->physicalDevice, m_Surface, &cap);
    if (err != VK_SUCCESS) {
      SR_CORE_ERROR("Could not get device surface capabilities");
    }
    if (info.minImageCount < cap.minImageCount)
      info.minImageCount = cap.minImageCount;
    else if (cap.maxImageCount != 0 && info.minImageCount > cap.maxImageCount)
      info.minImageCount = cap.maxImageCount;
    if (cap.currentExtent.width == 0xffffffff) {
      info.imageExtent.width  = Width;
      info.imageExtent.height = Height;
    } else {
      info.imageExtent.width = Width = cap.currentExtent.width;
      info.imageExtent.height = Height = cap.currentExtent.height;
    }
    err =
        vkCreateSwapchainKHR(m_Device->device, &info, m_Allocator, &swapchain);
    if (err != VK_SUCCESS) {
      SR_CORE_ERROR("Could not create swapchain");
    }
    err = vkGetSwapchainImagesKHR(m_Device->device, swapchain, &ImageCount,
                                  nullptr);
    if (err != VK_SUCCESS) {
      SR_CORE_ERROR("Could not get swapchain swapchain images");
    }
    VkImage backbuffers[16] = {};
    IM_ASSERT(ImageCount >= minImageCount);
    IM_ASSERT(ImageCount < IM_ARRAYSIZE(backbuffers));
    err = vkGetSwapchainImagesKHR(m_Device->device, swapchain, &ImageCount,
                                  backbuffers);
    if (err != VK_SUCCESS) {
      SR_CORE_ERROR("Could not get swapchain swapchain images");
    }

    IM_ASSERT(frames == nullptr && frameSemaphores == nullptr);
    SemaphoreCount = ImageCount + 1;
    frames         = (Frame*)malloc(sizeof(Frame) * ImageCount);
    frameSemaphores =
        (FrameSemaphores*)malloc(sizeof(FrameSemaphores) * SemaphoreCount);
    memset(frames, 0, sizeof(frames[0]) * ImageCount);
    memset(frameSemaphores, 0, sizeof(frameSemaphores[0]) * SemaphoreCount);
    for (uint32_t i = 0; i < ImageCount; i++)
      frames[i].Backbuffer = backbuffers[i];
    if (oldSwapchain)
      vkDestroySwapchainKHR(m_Device->device, oldSwapchain, m_Allocator);
    // color
    {
      VkImageViewCreateInfo info = {};
      info.sType                 = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      info.viewType              = VK_IMAGE_VIEW_TYPE_2D;
      info.format                = SurfaceFormat.format;
      info.components.r          = VK_COMPONENT_SWIZZLE_R;
      info.components.g          = VK_COMPONENT_SWIZZLE_G;
      info.components.b          = VK_COMPONENT_SWIZZLE_B;
      info.components.a          = VK_COMPONENT_SWIZZLE_A;
      VkImageSubresourceRange image_range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0,
                                             1};
      info.subresourceRange               = image_range;
      for (uint32_t i = 0; i < ImageCount; i++) {
        auto* fd   = &frames[i];
        info.image = fd->Backbuffer;
        err        = vkCreateImageView(m_Device->device, &info, m_Allocator,
                                       &fd->BackbufferView);
        if (err != VK_SUCCESS) {
          SR_CORE_ERROR("Could not create image view");
        }
      }
    }
  }
  void Window::CreateCommandBuffers() {
    // Create Command Buffers
    VkResult err;
    auto     device = m_Device->device;
    for (uint32_t i = 0; i < ImageCount; i++) {
      auto* fd = &frames[i];
      {
        VkCommandPoolCreateInfo info = {};
        info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        info.flags            = 0;
        info.queueFamilyIndex = m_PhysicalDevice->queueFamilyIndex;
        err = vkCreateCommandPool(device, &info, m_Allocator, &fd->CommandPool);
        if (err != VK_SUCCESS) SR_CORE_ERROR("Could not create command pool");
      }
      {
        VkCommandBufferAllocateInfo info = {};
        info.sType       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        info.commandPool = fd->CommandPool;
        info.level       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        info.commandBufferCount = 1;
        err = vkAllocateCommandBuffers(device, &info, &fd->CommandBuffer);
        if (err != VK_SUCCESS) SR_CORE_ERROR("Could not create command pool");
      }
      {
        VkFenceCreateInfo info = {};
        info.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        info.flags             = VK_FENCE_CREATE_SIGNALED_BIT;
        err = vkCreateFence(device, &info, m_Allocator, &fd->Fence);
        if (err != VK_SUCCESS) SR_CORE_ERROR("Could not create command pool");
      }
    }

    for (uint32_t i = 0; i < SemaphoreCount; i++) {
      auto* fsd = &frameSemaphores[i];
      {
        VkSemaphoreCreateInfo info = {};
        info.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        err = vkCreateSemaphore(device, &info, m_Allocator,
                                &fsd->ImageAcquiredSemaphore);
        if (err != VK_SUCCESS) SR_CORE_ERROR("Could not create semaphore");
        err = vkCreateSemaphore(device, &info, m_Allocator,
                                &fsd->RenderCompleteSemaphore);
        if (err != VK_SUCCESS) SR_CORE_ERROR("Could not create semaphore");
      }
    }
  }
  void Window::CreateDepths() {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width  = Width;
    imageInfo.extent.height = Height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = VK_FORMAT_D32_SFLOAT;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    auto err =
        vkCreateImage(m_Device->device, &imageInfo, nullptr, &DepthImage);
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_Device->device, DepthImage,
                                 &memRequirements);
    auto findMemoryType = [&](VkPhysicalDevice d, uint32_t filter) {
      VkPhysicalDeviceMemoryProperties memProperties;
      vkGetPhysicalDeviceMemoryProperties(d, &memProperties);

      for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((filter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ==
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
          return i;
        }
      }
      SR_CORE_ERROR("Could not found memory type for depth image");
      return 0u;
    };
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(m_PhysicalDevice->physicalDevice,
                                               memRequirements.memoryTypeBits);

    err = vkAllocateMemory(m_Device->device, &allocInfo, nullptr, &DepthMemory);

    vkBindImageMemory(m_Device->device, DepthImage, DepthMemory, 0);
    {
      VkImageViewCreateInfo dci{};
      dci.image    = DepthImage;
      dci.viewType = VK_IMAGE_VIEW_TYPE_2D;
      dci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      dci.format   = VK_FORMAT_D32_SFLOAT;
      dci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
      dci.subresourceRange.baseMipLevel   = 0;
      dci.subresourceRange.levelCount     = 1;
      dci.subresourceRange.baseArrayLayer = 0;
      dci.subresourceRange.layerCount     = 1;
      vkCreateImageView(m_Device->device, &dci, m_Allocator, &DepthImageView);
    }
  }
  void Window::CreateFramebuffer(VkRenderPass renderpass) {
    if (renderpass == VK_NULL_HANDLE) {
      SR_CORE_WARN("Render pass null provieded");
      return;
    }
    for (int i = 0; i < ImageCount; i++) {
      auto*       fb             = &frames[i];
      VkImageView attachments[2] = {fb->BackbufferView, DepthImageView};
      VkFramebufferCreateInfo framebufferInfo{};
      framebufferInfo.sType      = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebufferInfo.renderPass = renderpass;
      framebufferInfo.attachmentCount = 2;
      framebufferInfo.pAttachments    = attachments;
      framebufferInfo.width           = Width;
      framebufferInfo.height          = Height;
      framebufferInfo.layers          = 1;

      auto err = vkCreateFramebuffer(m_Device->device, &framebufferInfo,
                                     nullptr, &fb->Framebuffer);
      if (err != VK_SUCCESS) SR_CORE_ERROR("Could not create framebuffer");
    }
  }
  void Window::Close() {}
  Window::~Window() {}
}  // namespace Sera