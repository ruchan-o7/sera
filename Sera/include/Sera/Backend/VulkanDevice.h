#pragma once
#include <cstdint>
#include "Backend/VulkanPhysicalDevice.h"
#include <vector>
namespace Sera {
  struct VulkanDevice {
      VulkanDevice(const VulkanPhysicalDevice*  pDevice,
                   const VkAllocationCallbacks* vkAllocator,
                   uint32_t queueFamily, std::vector<const char*>& extensions,
                   void* pNext = nullptr);

      VkResult WaitIdle() { return vkDeviceWaitIdle(device); }

      VkDevice                     device         = VK_NULL_HANDLE;
      const VulkanPhysicalDevice*  physicalDevice = nullptr;
      const VkAllocationCallbacks* allocator      = VK_NULL_HANDLE;
      VkQueue                      queue          = VK_NULL_HANDLE;
      uint32_t                     queueFamily;
  };
}  // namespace Sera