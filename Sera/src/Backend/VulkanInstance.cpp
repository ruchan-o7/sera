#include "Backend/VulkanInstance.h"
#include "Backend/VulkanDebug.h"
#include "Log.h"
namespace Sera {
  static constexpr std::array<const char*, 1> ValidationLayerNames = {
      "VK_LAYER_KHRONOS_validation"};
  VulkanInstance::~VulkanInstance() {
    FreeDebug(instance);
    vkDestroyInstance(instance, instanceSpecs.allocCallbacks);
  }
  VulkanInstance::VulkanInstance(Specs specs) : instanceSpecs(specs) {
    VkResult res = VK_SUCCESS;
    {
      // Enumerate available layers
      uint32_t LayerCount = 0;
      res = vkEnumerateInstanceLayerProperties(&LayerCount, nullptr);
      if (res != VK_SUCCESS) SR_CORE_ERROR("Failed to query layer count");

      layers.resize(LayerCount);
      res = vkEnumerateInstanceLayerProperties(&LayerCount, layers.data());
      if (res != VK_SUCCESS) SR_CORE_ERROR("Failed to query layers");
      SR_CORE_TRACE("Available layers:");
      for (auto& l : layers) {
        SR_CORE_TRACE("\t {0}:", l.layerName);
      }
    }
    if (!EnumerateInstanceExtension(nullptr, extensions))
      SR_CORE_ERROR("Failed to enumerate instance extensions");
    SR_CORE_TRACE("Available extensions:");
    for (auto& l : extensions) {
      SR_CORE_TRACE("\t {0}:", l.extensionName);
    }
    std::vector<const char*> instanceExtensions;
    if (IsExtensionAvailable(extensions, VK_KHR_SURFACE_EXTENSION_NAME)) {
      instanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
      instanceExtensions.push_back("VK_KHR_win32_surface");
    }
    if (IsExtensionAvailable(
            extensions,
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) {
      instanceExtensions.push_back(
          VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    }

    for (const auto* ExtName : instanceExtensions)
      if (!IsExtensionAvailable(extensions, ExtName))
        SR_CORE_ERROR("Required extension {0} is not available", ExtName);

    if (!instanceSpecs.additionalExtensions.empty()) {
      for (const auto& ext : instanceSpecs.additionalExtensions) {
        if (IsExtensionAvailable(extensions, ext)) {
          instanceExtensions.push_back(ext);
        } else {
          SR_CORE_WARN("Requested extension {0} is not available", ext);
        }
      }
    }

    std::vector<const char*> instanceLayers;
    if (!instanceSpecs.additionalLayers.empty()) {
      for (const auto& ext : instanceSpecs.additionalLayers) {
        if (IsLayerAvailable(ext)) {
          instanceLayers.push_back(ext);
        } else {
          SR_CORE_WARN("Requested layer {0} is not available", ext);
        }
      }
    }
    if (instanceSpecs.enableValidation) {
      if (IsExtensionAvailable(extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
        instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
      }
      for (const auto& layer : ValidationLayerNames) {
        if (!IsLayerAvailable(layer)) {
          SR_CORE_WARN("Validation layer {0} is not available", layer);
          continue;
        }
        instanceLayers.push_back(layer);
      }
    }
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext              = nullptr;
    appInfo.pApplicationName   = nullptr;
    appInfo.applicationVersion = instanceSpecs.appVersion;
    appInfo.pEngineName        = instanceSpecs.engineName;
    appInfo.engineVersion      = instanceSpecs.engineVersion;
    appInfo.apiVersion         = instanceSpecs.apiVersion;
    VkInstanceCreateInfo instanceCI{};
    instanceCI.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCI.pNext            = instanceSpecs.pNext;
    instanceCI.flags            = 0;
    instanceCI.pApplicationInfo = &appInfo;
    instanceCI.enabledExtensionCount =
        static_cast<uint32_t>(instanceExtensions.size());
    instanceCI.ppEnabledExtensionNames =
        instanceExtensions.empty() ? nullptr : instanceExtensions.data();
    instanceCI.enabledLayerCount = static_cast<uint32_t>(instanceLayers.size());
    instanceCI.ppEnabledLayerNames =
        instanceLayers.empty() ? nullptr : instanceLayers.data();

    auto err =
        vkCreateInstance(&instanceCI, instanceSpecs.allocCallbacks, &instance);
    if (err != VK_SUCCESS) {
      SR_CORE_CRITICAL("Vulkan instance could not created ");
      return;
    }

    enabledExtensions = std::move(instanceExtensions);
#ifdef SR_DEBUG
    constexpr VkDebugUtilsMessageSeverityFlagsEXT messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    constexpr VkDebugUtilsMessageTypeFlagsEXT messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    if (!SetupDebugUtils(instance, messageSeverity, messageType, 0, nullptr)) {
      SR_CORE_ERROR(
          "Failed to initialize debug utils. Validation layer message "
          "logging, "
          "performance markers, etc. will be disabled.");
    }
#endif
    {
      uint32_t physicalDeviceCount = 0;
      auto     err =
          vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
      if (physicalDeviceCount == 0) {
        SR_CORE_CRITICAL(
            "Suitable vulkan device not found maybe update your drivers");
      }
      physicalDevices.resize(physicalDeviceCount);
      vkEnumeratePhysicalDevices(instance, &physicalDeviceCount,
                                 physicalDevices.data());
    }
  }
  bool VulkanInstance::IsLayerAvailable(const char* layerName) {
    for (const auto& l : layers) {
      if (strcmp(l.layerName, layerName) == 0) {
        return true;
      }
    }
    return false;
  }
  bool VulkanInstance::IsExtensionAvailable(
      const std::vector<VkExtensionProperties>& extensions,
      const char*                               extensionToCheck) {
    if (extensionToCheck == nullptr)
      SR_CORE_WARN("extension to check is null pointer!");

    for (const auto& Extension : extensions) {
      if (strcmp(Extension.extensionName, extensionToCheck) == 0) {
        return true;
      }
    }
    return false;
  }
  bool VulkanInstance::IsExtensionEnabled(const char* extension) {
    for (const auto& Extension : enabledExtensions) {
      if (strcmp(Extension, extension) == 0) {
        return true;
      }
    }

    return false;
  }
  VkPhysicalDevice VulkanInstance::SelectPhysicalDevice() {
    const auto IsGraphicsAndComputeQueueSupported =
        [](VkPhysicalDevice Device) {
          uint32_t QueueFamilyCount = 0;
          vkGetPhysicalDeviceQueueFamilyProperties(Device, &QueueFamilyCount,
                                                   nullptr);
          assert(QueueFamilyCount > 0);
          std::vector<VkQueueFamilyProperties> QueueFamilyProperties(
              QueueFamilyCount);
          vkGetPhysicalDeviceQueueFamilyProperties(
              Device, &QueueFamilyCount, QueueFamilyProperties.data());
          assert(QueueFamilyCount == QueueFamilyProperties.size());

          // If an implementation exposes any queue family that supports
          // graphics operations, at least one queue family of at least one
          // physical device exposed by the implementation must support both
          // graphics and compute operations.
          for (const auto& QueueFamilyProps : QueueFamilyProperties) {
            if ((QueueFamilyProps.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 &&
                (QueueFamilyProps.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0) {
              return true;
            }
          }
          return false;
        };

    VkPhysicalDevice SelectedPhysicalDevice = VK_NULL_HANDLE;

    // Select a device that exposes a queue family that supports both compute
    // and graphics operations. Prefer discrete GPU.
    if (SelectedPhysicalDevice == VK_NULL_HANDLE) {
      for (auto Device : physicalDevices) {
        VkPhysicalDeviceProperties DeviceProps;
        vkGetPhysicalDeviceProperties(Device, &DeviceProps);

        if (IsGraphicsAndComputeQueueSupported(Device)) {
          SelectedPhysicalDevice = Device;
          if (DeviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            break;
          }
        }
      }
    }

    if (SelectedPhysicalDevice != VK_NULL_HANDLE) {
      VkPhysicalDeviceProperties SelectedDeviceProps;
      vkGetPhysicalDeviceProperties(SelectedPhysicalDevice,
                                    &SelectedDeviceProps);
      SR_CORE_INFO(
          "Using physical device '{0}', API version: {1}, DriverVersion "
          "{2}.{3}.{4} ",
          SelectedDeviceProps.deviceName,
          VK_API_VERSION_MAJOR(SelectedDeviceProps.apiVersion),
          VK_API_VERSION_MINOR(SelectedDeviceProps.apiVersion),
          VK_API_VERSION_PATCH(SelectedDeviceProps.apiVersion),
          VK_API_VERSION_MAJOR(SelectedDeviceProps.driverVersion),
          VK_API_VERSION_MINOR(SelectedDeviceProps.driverVersion),
          VK_API_VERSION_PATCH(SelectedDeviceProps.driverVersion));
    } else {
      SR_CORE_ERROR("Failed to find suitable physical device");
    }

    return SelectedPhysicalDevice;
  }
  bool VulkanInstance::EnumerateInstanceExtension(
      const char* layerName, std::vector<VkExtensionProperties>& extensions) {
    uint32_t extCount = 0;

    if (vkEnumerateInstanceExtensionProperties(layerName, &extCount, nullptr) !=
        VK_SUCCESS) {
      return false;
    }

    extensions.resize(extCount);
    if (vkEnumerateInstanceExtensionProperties(
            layerName, &extCount, extensions.data()) != VK_SUCCESS) {
      extensions.clear();
      return false;
    }
    if (extCount != extensions.size()) {
      SR_CORE_WARN(
          "The number of extensions written by "
          "vkEnumerateInstanceExtensionProperties is not "
          "consistent "
          "with the count returned in the first call. This is a Vulkan loader "
          "bug.");
    }
    return true;
  }
}  // namespace Sera
