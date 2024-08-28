#include "Backend/VulkanDevice.h"
#include "Log.h"
#include "vulkan/vulkan_core.h"
namespace Sera {
  VulkanDevice::VulkanDevice(const VulkanPhysicalDevice*  pDevice,
                             const VkAllocationCallbacks* vkAllocator,
                             uint32_t                     queueFamily,
                             std::vector<const char*>& extensions, void* pNext)
      : physicalDevice(pDevice),
        allocator(vkAllocator),
        queueFamily(queueFamily) {
    int device_extension_count = 1;
    extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    const float             queue_priority[] = {1.0f};
    VkDeviceQueueCreateInfo queue_info[1]    = {};
    queue_info[0].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info[0].queueFamilyIndex = queueFamily;
    queue_info[0].queueCount       = 1;
    queue_info[0].pQueuePriorities = queue_priority;
    VkDeviceCreateInfo create_info = {};
    create_info.sType              = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount =
        sizeof(queue_info) / sizeof(queue_info[0]);
    create_info.pQueueCreateInfos       = queue_info;
    create_info.enabledExtensionCount   = extensions.size();
    create_info.ppEnabledExtensionNames = extensions.data();
    create_info.pNext                   = pNext;
    auto err = vkCreateDevice(physicalDevice->physicalDevice, &create_info,
                              allocator, &device);
    if (err != VK_SUCCESS) {
      SR_CORE_ERROR("Vulkan device could not initialized");
    }
    vkGetDeviceQueue(device, queueFamily, 0, &queue);
  }
}  // namespace Sera