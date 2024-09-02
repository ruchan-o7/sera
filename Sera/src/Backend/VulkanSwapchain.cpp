#include "Backend/VulkanSwapchain.h"
#include <Backends/imgui_impl_vulkan.h>
#include <cstdlib>
#include "Backend/VulkanPhysicalDevice.h"
#include "Log.h"
namespace Sera {
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
    vkDestroySemaphore(device, frame->ImageAvailableSemaphore, allocator);
    vkDestroySemaphore(device, frame->RenderCompleteSemaphore, allocator);
    frame->ImageAvailableSemaphore = frame->RenderCompleteSemaphore =
        VK_NULL_HANDLE;
  }

  VulkanSwapchain::VulkanSwapchain(VulkanInstance*        instance,
                                   VulkanPhysicalDevice*  pDevice,
                                   VkAllocationCallbacks* allocator,
                                   VulkanDevice* device, bool isVsync,
                                   VkSurfaceFormatKHR surfaceFormat,
                                   VkSurfaceKHR       surface)
      : m_VkInstance(instance),
        m_Allocator(allocator),
        m_PhysicalDevice(pDevice),
        m_Vsync(isVsync),
        m_Device(device),
        m_Surface(surface),
        SurfaceFormat(surfaceFormat) {}
  void VulkanSwapchain::ReCreate() {
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
    VkSwapchainKHR oldSwapchain = m_Swapchain;
    m_Swapchain                 = VK_NULL_HANDLE;
    auto err                    = m_Device->WaitIdle();

    auto device = m_Device->device;
    for (uint32_t i = 0; i < ImageCount; i++)
      DestroyFrame(device, &Frames[i], m_Allocator);

    for (uint32_t i = 0; i < SemaphoreCount; i++)
      DestroyFrameSemaphores(device, &FrameSemaphoress[i], m_Allocator);

    free(Frames);
    free(FrameSemaphoress);

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
      info.imageExtent.width  = m_Width;
      info.imageExtent.height = m_Height;
    } else {
      info.imageExtent.width = m_Width = cap.currentExtent.width;
      info.imageExtent.height = m_Height = cap.currentExtent.height;
    }
    err = vkCreateSwapchainKHR(m_Device->device, &info, m_Allocator,
                               &m_Swapchain);
    if (err != VK_SUCCESS) SR_CORE_ERROR("Could not create swapchain");

    err = vkGetSwapchainImagesKHR(m_Device->device, m_Swapchain, &ImageCount,
                                  nullptr);
    if (err != VK_SUCCESS)
      SR_CORE_ERROR("Could not get swapchain swapchain images");

    VkImage backbuffers[16] = {};
    err = vkGetSwapchainImagesKHR(m_Device->device, m_Swapchain, &ImageCount,
                                  backbuffers);
    if (err != VK_SUCCESS)
      SR_CORE_ERROR("Could not get swapchain swapchain images");

    SemaphoreCount = ImageCount;  //+ 1;
    Frames         = (Frame*)malloc(sizeof(Frame) * ImageCount);
    FrameSemaphoress =
        (FrameSemaphores*)malloc(sizeof(FrameSemaphores) * SemaphoreCount);

    memset(Frames, 0, sizeof(Frames[0]) * ImageCount);
    memset(FrameSemaphoress, 0, sizeof(FrameSemaphoress[0]) * SemaphoreCount);

    for (uint32_t i = 0; i < ImageCount; i++)
      Frames[i].Backbuffer = backbuffers[i];

    if (oldSwapchain)
      vkDestroySwapchainKHR(m_Device->device, oldSwapchain, m_Allocator);

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
        auto* fd   = &Frames[i];
        info.image = fd->Backbuffer;
        err        = vkCreateImageView(m_Device->device, &info, m_Allocator,
                                       &fd->BackbufferView);
        if (err != VK_SUCCESS) SR_CORE_ERROR("Could not create image view");
      }
    }
    // FIXME: ! DONT FORGET TO FREE OLD DEPTHS
    CreateDepths();
    CreateFramebuffer(VK_NULL_HANDLE);
    InitializeFenceSemaphore();
  }
  void VulkanSwapchain::CreateDepths() {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width  = m_Width;
    imageInfo.extent.height = m_Height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = VK_FORMAT_D32_SFLOAT;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    auto err = vkCreateImage(m_Device->device, &imageInfo, nullptr,
                             &m_DepthBuffer.Image);
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_Device->device, m_DepthBuffer.Image,
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

    err = vkAllocateMemory(m_Device->device, &allocInfo, nullptr,
                           &m_DepthBuffer.Memory);

    vkBindImageMemory(m_Device->device, m_DepthBuffer.Image,
                      m_DepthBuffer.Memory, 0);
    {
      VkImageViewCreateInfo dci{};
      dci.image    = m_DepthBuffer.Image;
      dci.viewType = VK_IMAGE_VIEW_TYPE_2D;
      dci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      dci.format   = VK_FORMAT_D32_SFLOAT;
      dci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
      dci.subresourceRange.baseMipLevel   = 0;
      dci.subresourceRange.levelCount     = 1;
      dci.subresourceRange.baseArrayLayer = 0;
      dci.subresourceRange.layerCount     = 1;
      vkCreateImageView(m_Device->device, &dci, m_Allocator,
                        &m_DepthBuffer.ImageView);
    }
  }

  void VulkanSwapchain::CreateFramebuffer(VkRenderPass rp) {
    if (rp == VK_NULL_HANDLE && m_RenderPass == VK_NULL_HANDLE) {
      SR_CORE_WARN("Render pass null provieded");
      return;
    }
    if (rp != VK_NULL_HANDLE && rp != m_RenderPass) {
      m_RenderPass = rp;
    }

    for (int i = 0; i < ImageCount; i++) {
      auto*                   fb             = &Frames[i];
      VkImageView             attachments[2] = {fb->BackbufferView,
                                                m_DepthBuffer.ImageView};
      VkFramebufferCreateInfo framebufferInfo{};
      framebufferInfo.sType      = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebufferInfo.renderPass = m_RenderPass;
      framebufferInfo.attachmentCount = 2;
      framebufferInfo.pAttachments    = attachments;
      framebufferInfo.width           = m_Width;
      framebufferInfo.height          = m_Height;
      framebufferInfo.layers          = 1;

      auto err = vkCreateFramebuffer(m_Device->device, &framebufferInfo,
                                     nullptr, &fb->Framebuffer);
      if (err != VK_SUCCESS) SR_CORE_ERROR("Could not create framebuffer");
    }
  }
  VkResult VulkanSwapchain::Present(VkQueue queue) {
    VkSemaphore render_complete_semaphore =
        FrameSemaphoress[CurrentFrame].RenderCompleteSemaphore;
    VkPresentInfoKHR info = {};
    info.sType            = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores    = &render_complete_semaphore;

    VkSwapchainKHR swapchain[] = {m_Swapchain};
    info.swapchainCount        = 1;
    info.pSwapchains           = swapchain;

    info.pImageIndices = &ImageIndex;
    return vkQueuePresentKHR(queue, &info);
  }

  void VulkanSwapchain::InitializeFenceSemaphore() {
    VkFenceCreateInfo f{};
    f.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    f.pNext = nullptr;
    f.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (uint32_t i = 0; i < ImageCount; i++) {
      vkCreateFence(m_Device->device, &f, m_Allocator, &Frames[i].Fence);
    }
    VkSemaphoreCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = 0;
    for (int i = 0; i < SemaphoreCount; i++) {
      auto* frame = &FrameSemaphoress[i];

      auto err = vkCreateSemaphore(m_Device->device, &info, m_Allocator,
                                   &frame->ImageAvailableSemaphore);

      err = vkCreateSemaphore(m_Device->device, &info, m_Allocator,
                              &frame->RenderCompleteSemaphore);
    }
  }
}  // namespace Sera
