#pragma once
#include "Backend/VulkanDevice.h"
#include "Backend/VulkanInstance.h"
#include "Backend/VulkanPhysicalDevice.h"
#include "Backend/VulkanDevice.h"
#include <GLFW/glfw3.h>
namespace Sera {
  struct Frame {
      VkCommandPool   CommandPool    = VK_NULL_HANDLE;
      VkCommandBuffer CommandBuffer  = VK_NULL_HANDLE;
      VkFence         Fence          = VK_NULL_HANDLE;
      VkImage         Backbuffer     = VK_NULL_HANDLE;
      VkImageView     BackbufferView = VK_NULL_HANDLE;
      VkFramebuffer   Framebuffer    = VK_NULL_HANDLE;
  };
  struct FrameSemaphores {
      VkSemaphore ImageAcquiredSemaphore  = VK_NULL_HANDLE;
      VkSemaphore RenderCompleteSemaphore = VK_NULL_HANDLE;
  };
  class Window {
    public:
      void             Close();
      static Window*   Create(VulkanInstance*        instance,
                              VulkanPhysicalDevice*  pDevice,
                              VkAllocationCallbacks* allocator,
                              VulkanDevice* device, bool isVsync = true);
      void             CreateSurface();
      void             CreateSwapchain();
      void             CreateCommandBuffers();
      void             CreateDepths();
      void             CreateFramebuffer(VkRenderPass renderpass);
      Frame*           frames          = nullptr;
      FrameSemaphores* frameSemaphores = nullptr;
      VkImage          DepthImage      = VK_NULL_HANDLE;
      VkImageView      DepthImageView  = VK_NULL_HANDLE;
      VkDeviceMemory   DepthMemory     = VK_NULL_HANDLE;
      int              Width, Height;
      // VkRenderPass     Renderpass      = VK_NULL_HANDLE;

      VkSwapchainKHR swapchain;
      GLFWwindow*    GetHandle() const { return m_Handle; }

      uint32_t           ImageCount     = 0;
      uint32_t           SemaphoreCount = 0;
      uint32_t           SemaphoreIndex = 0;
      uint32_t           FrameIndex     = 0;
      VkSurfaceFormatKHR SurfaceFormat;

    private:
      Window() = default;
      ~Window();

    private:
      GLFWwindow*            m_Handle;
      VkSurfaceKHR           m_Surface;
      VulkanInstance*        m_VkInstance;
      VulkanPhysicalDevice*  m_PhysicalDevice;
      VulkanDevice*          m_Device;
      VkAllocationCallbacks* m_Allocator;
      VkPresentModeKHR       m_PresentMode;
      bool                   m_Vsync;
  };
}  // namespace Sera