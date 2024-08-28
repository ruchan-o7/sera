#include "Backend/VulkanPhysicalDevice.h"
#include <vector>
namespace Sera {
  VulkanPhysicalDevice::VulkanPhysicalDevice(VkPhysicalDevice device)
      : physicalDevice(device) {}

  void VulkanPhysicalDevice::SelectGraphicsQueueFamily() {
    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, NULL);
    std::vector<VkQueueFamilyProperties> queues(count);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count,
                                             queues.data());
    for (uint32_t i = 0; i < count; i++)
      if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        queueFamilyIndex = i;
        break;
      }
  }
}  // namespace Sera