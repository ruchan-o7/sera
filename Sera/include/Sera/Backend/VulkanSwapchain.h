#pragma once
#include <vulkan/vulkan.h>
#include "Backend/VulkanDevice.h"
#include "Backend/VulkanInstance.h"
#include "Backend/VulkanPhysicalDevice.h"
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
      VkSemaphore ImageAvailableSemaphore = VK_NULL_HANDLE;
      VkSemaphore RenderCompleteSemaphore = VK_NULL_HANDLE;
  };

  class VulkanSwapchain {
    public:
      static VulkanSwapchain* Create(VulkanInstance*        instance,
                                     VulkanPhysicalDevice*  pDevice,
                                     VkAllocationCallbacks* allocator,
                                     VulkanDevice* device, bool isVsync,
                                     VkSurfaceFormatKHR surfaceFormat,
                                     VkSurfaceKHR       surface) {
        return new VulkanSwapchain(instance, pDevice, allocator, device,
                                   isVsync, surfaceFormat, surface);
      }
      void Resize(int w, int h) {
        m_Width  = w;
        m_Height = h;
        ReCreate();
      }
      void SetVsync(bool val = true) {
        m_Vsync = val;
        ReCreate();
      }
      VkSwapchainKHR Get() const { return m_Swapchain; }
      void           CreateFramebuffer(VkRenderPass rp);
      VkResult       Present(VkQueue queue);
      //   void CreateCommandBuffers();

    public:
      Frame*           Frames = nullptr;
      FrameSemaphores* FrameSemaphoress =
          nullptr;  // double s for not being same name
                    // variable and type
      uint32_t           ImageCount     = 0;
      uint32_t           SemaphoreCount = 0;
      uint32_t           CurrentFrame   = 0;
      uint32_t           ImageIndex     = 0;
      VkSurfaceFormatKHR SurfaceFormat;

    private:
      void CreateDepths();
      void ReCreate();
      void InitializeFenceSemaphore();

    private:
      VkSwapchainKHR         m_Swapchain = VK_NULL_HANDLE;
      VkSurfaceKHR           m_Surface   = VK_NULL_HANDLE;
      VulkanInstance*        m_VkInstance;
      VulkanPhysicalDevice*  m_PhysicalDevice;
      VulkanDevice*          m_Device;
      VkAllocationCallbacks* m_Allocator;
      VkPresentModeKHR       m_PresentMode;
      VkRenderPass           m_RenderPass = VK_NULL_HANDLE;
      bool                   m_Vsync;
      int                    m_Width, m_Height;
      struct DepthBuffer {
          VkImage        Image     = VK_NULL_HANDLE;
          VkImageView    ImageView = VK_NULL_HANDLE;
          VkDeviceMemory Memory    = VK_NULL_HANDLE;
      } m_DepthBuffer;
      VulkanSwapchain(VulkanInstance* instance, VulkanPhysicalDevice* pDevice,
                      VkAllocationCallbacks* allocator, VulkanDevice* device,
                      bool isVsync, VkSurfaceFormatKHR surfaceFormat,
                      VkSurfaceKHR surface);
  };
}  // namespace Sera
