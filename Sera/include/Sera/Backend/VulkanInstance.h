#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "vulkan/vulkan_core.h"
namespace Sera {
  struct VulkanInstance {
      struct Specs {
          const char*              appName        = "Sera App";
          const char*              engineName     = "Sera Render Engine";
          uint32_t                 apiVersion     = VK_API_VERSION_1_3;
          uint32_t                 appVersion     = VK_MAKE_VERSION(0, 1, 0);
          uint32_t                 engineVersion  = VK_MAKE_VERSION(0, 1, 0);
          VkAllocationCallbacks*   allocCallbacks = nullptr;
          std::vector<const char*> additionalExtensions;
          std::vector<const char*> additionalLayers;
          // you can set this false in release mode
          bool enableValidation = true;
      };
      VulkanInstance(Specs specs);

      bool EnumerateInstanceExtension(
          const char*                         layerName,
          std::vector<VkExtensionProperties>& extensions);
      bool IsLayerAvailable(const char* layerName);
      bool IsExtensionAvailable(
          const std::vector<VkExtensionProperties>& extensions,
          const char*                               extensionToCheck);
      bool             IsExtensionEnabled(const char* extension);
      VkPhysicalDevice SelectPhysicalDevice();
      const std::vector<VkPhysicalDevice>& GetPhysicalDevices() const {
        return physicalDevices;
      }
      Specs                              instanceSpecs;
      VkInstance                         instance;
      std::vector<VkPhysicalDevice>      physicalDevices;
      std::vector<const char*>           enabledExtensions;
      std::vector<VkExtensionProperties> extensions;
      std::vector<VkLayerProperties>     layers;
      VkInstance                         operator()() { return instance; }
  };
}  // namespace Sera