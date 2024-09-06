#pragma once
#include <vulkan/vulkan.h>
namespace Sera {
  class VulkanDevice;
  class VulkanRenderPass {
    public:
      struct CreateInfo {
          VulkanDevice*                device;
          VkSurfaceFormatKHR           surfaceFormat;
          const VkAllocationCallbacks* allocator = VK_NULL_HANDLE;
      };
      ~VulkanRenderPass();
      static VulkanRenderPass* Create(CreateInfo info);
      VkRenderPass             GetHandle() const { return m_Handle; }

    private:
      VulkanRenderPass(CreateInfo info);

    private:
      CreateInfo   m_Info;
      VkRenderPass m_Handle = VK_NULL_HANDLE;
  };

}  // namespace Sera
