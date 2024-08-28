#pragma once
#include <vulkan/vulkan.h>
namespace Sera {
  struct VulkanPhysicalDevice {
      VulkanPhysicalDevice(VkPhysicalDevice device);

      void             SelectGraphicsQueueFamily();
      uint32_t         queueFamilyIndex = 0;
      VkPhysicalDevice physicalDevice;
  };
}  // namespace Sera