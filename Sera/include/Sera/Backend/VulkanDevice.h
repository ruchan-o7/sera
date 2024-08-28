#pragma once
#include <cstdint>
#include "Backend/VulkanPhysicalDevice.h"
namespace Sera {
  struct VulkanDevice {
      VulkanDevice(const VulkanPhysicalDevice*  pDevice,
                   const VkAllocationCallbacks* vkAllocator,
                   uint32_t                     queueFamily);

      VkDevice                     device         = VK_NULL_HANDLE;
      const VulkanPhysicalDevice*  physicalDevice = nullptr;
      const VkAllocationCallbacks* allocator      = VK_NULL_HANDLE;
      VkQueue                      queue          = VK_NULL_HANDLE;
      uint32_t                     queueFamily;
  };
}  // namespace Sera