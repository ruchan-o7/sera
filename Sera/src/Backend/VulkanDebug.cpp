#include "Backend/VulkanDebug.h"
#include <sstream>
#include <iomanip>
#include "Log.h"
namespace Sera {

  PFN_vkCreateDebugUtilsMessengerEXT  CreateDebugUtilsMessengerEXT  = nullptr;
  PFN_vkDestroyDebugUtilsMessengerEXT DestroyDebugUtilsMessengerEXT = nullptr;
  PFN_vkSetDebugUtilsObjectNameEXT    SetDebugUtilsObjectNameEXT    = nullptr;
  PFN_vkSetDebugUtilsObjectTagEXT     SetDebugUtilsObjectTagEXT     = nullptr;
  PFN_vkQueueBeginDebugUtilsLabelEXT  QueueBeginDebugUtilsLabelEXT  = nullptr;
  PFN_vkQueueEndDebugUtilsLabelEXT    QueueEndDebugUtilsLabelEXT    = nullptr;
  PFN_vkQueueInsertDebugUtilsLabelEXT QueueInsertDebugUtilsLabelEXT = nullptr;

  VkDebugUtilsMessengerEXT DbgMessenger = VK_NULL_HANDLE;

  VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessengerCallback(
      VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
      VkDebugUtilsMessageTypeFlagsEXT             messageType,
      const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
      void*                                       userData) {
    std::stringstream debugMessage;
    debugMessage << "Vulkan debug message (";
    if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
      debugMessage << "general";
      if (messageType & (VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)) {
        debugMessage << ", ";
      }
    }
    if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
      debugMessage << "validation";
      if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
        debugMessage << ", ";
      }
    }
    if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
      debugMessage << "performance";
    }
    debugMessage << "): ";

    debugMessage << (callbackData->pMessageIdName != nullptr
                         ? callbackData->pMessageIdName
                         : "<Unknown name>");
    if (callbackData->pMessage != nullptr) {
      debugMessage << std::endl
                   << "                 " << callbackData->pMessage;
    }

    if (callbackData->objectCount > 0) {
      for (uint32_t obj = 0; obj < callbackData->objectCount; ++obj) {
        const auto& Object = callbackData->pObjects[obj];
        debugMessage << std::endl
                     << "                 Object[" << obj << "]"
                     << "(" << VkObjectTypeToString(Object.objectType)
                     << "): Handle " << std::hex << "0x" << Object.objectHandle;
        if (Object.pObjectName != nullptr) {
          debugMessage << ", Name: '" << Object.pObjectName << '\'';
        }
      }
    }

    if (callbackData->cmdBufLabelCount > 0) {
      for (uint32_t l = 0; l < callbackData->cmdBufLabelCount; ++l) {
        const auto& Label = callbackData->pCmdBufLabels[l];
        debugMessage << std::endl << "                 Label[" << l << "]";
        if (Label.pLabelName != nullptr) {
          debugMessage << " - " << Label.pLabelName;
        }
        debugMessage << " {";
        debugMessage << std::fixed << std::setw(4) << Label.color[0] << ", "
                     << std::fixed << std::setw(4) << Label.color[1] << ", "
                     << std::fixed << std::setw(4) << Label.color[2] << ", "
                     << std::fixed << std::setw(4) << Label.color[3] << "}";
      }
    }

    SR_CORE_WARN(debugMessage.str().c_str());
    return VK_FALSE;
  }

  bool SetupDebugUtils(VkInstance                          instance,
                       VkDebugUtilsMessageSeverityFlagsEXT messageSeverity,
                       VkDebugUtilsMessageTypeFlagsEXT     messageType,
                       uint32_t                            IgnoreMessageCount,
                       const char* const*                  ppIgnoreMessageNames,
                       void*                               pUserData) {
    CreateDebugUtilsMessengerEXT =
        reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    DestroyDebugUtilsMessengerEXT =
        reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (CreateDebugUtilsMessengerEXT == nullptr ||
        DestroyDebugUtilsMessengerEXT == nullptr) {
      return false;
    }

    VkDebugUtilsMessengerCreateInfoEXT DbgMessenger_CI{};
    DbgMessenger_CI.sType =
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    DbgMessenger_CI.pNext           = NULL;
    DbgMessenger_CI.flags           = 0;
    DbgMessenger_CI.messageSeverity = messageSeverity;
    DbgMessenger_CI.messageType     = messageType;
    DbgMessenger_CI.pfnUserCallback = DebugMessengerCallback;
    DbgMessenger_CI.pUserData       = pUserData;

    auto err = CreateDebugUtilsMessengerEXT(instance, &DbgMessenger_CI, nullptr,
                                            &DbgMessenger);
    if (err != VK_SUCCESS) {
      SR_CORE_ERROR("Failed to craete debug utils messenger");
    }

    // Load function pointers
    SetDebugUtilsObjectNameEXT =
        reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
            vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT"));
    assert(SetDebugUtilsObjectNameEXT != nullptr);
    SetDebugUtilsObjectTagEXT =
        reinterpret_cast<PFN_vkSetDebugUtilsObjectTagEXT>(
            vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectTagEXT"));
    assert(SetDebugUtilsObjectTagEXT != nullptr);

    QueueBeginDebugUtilsLabelEXT =
        reinterpret_cast<PFN_vkQueueBeginDebugUtilsLabelEXT>(
            vkGetInstanceProcAddr(instance, "vkQueueBeginDebugUtilsLabelEXT"));
    assert(QueueBeginDebugUtilsLabelEXT != nullptr);
    QueueEndDebugUtilsLabelEXT =
        reinterpret_cast<PFN_vkQueueEndDebugUtilsLabelEXT>(
            vkGetInstanceProcAddr(instance, "vkQueueEndDebugUtilsLabelEXT"));
    assert(QueueEndDebugUtilsLabelEXT != nullptr);
    QueueInsertDebugUtilsLabelEXT =
        reinterpret_cast<PFN_vkQueueInsertDebugUtilsLabelEXT>(
            vkGetInstanceProcAddr(instance, "vkQueueInsertDebugUtilsLabelEXT"));
    assert(QueueInsertDebugUtilsLabelEXT != nullptr);

    return err == VK_SUCCESS;
  }

  void FreeDebug(VkInstance instance) {
    if (DbgMessenger != VK_NULL_HANDLE) {
      DestroyDebugUtilsMessengerEXT(instance, DbgMessenger, nullptr);
    }
  }
  const char* VkObjectTypeToString(VkObjectType ObjectType) {
    switch (ObjectType) {
        // clang-format off
        case VK_OBJECT_TYPE_UNKNOWN:                        return "unknown";
        case VK_OBJECT_TYPE_INSTANCE:                       return "instance";
        case VK_OBJECT_TYPE_PHYSICAL_DEVICE:                return "physical device";
        case VK_OBJECT_TYPE_DEVICE:                         return "device";
        case VK_OBJECT_TYPE_QUEUE:                          return "queue";
        case VK_OBJECT_TYPE_SEMAPHORE:                      return "semaphore";
        case VK_OBJECT_TYPE_COMMAND_BUFFER:                 return "cmd buffer";
        case VK_OBJECT_TYPE_FENCE:                          return "fence";
        case VK_OBJECT_TYPE_DEVICE_MEMORY:                  return "memory";
        case VK_OBJECT_TYPE_BUFFER:                         return "buffer";
        case VK_OBJECT_TYPE_IMAGE:                          return "image";
        case VK_OBJECT_TYPE_EVENT:                          return "event";
        case VK_OBJECT_TYPE_QUERY_POOL:                     return "query pool";
        case VK_OBJECT_TYPE_BUFFER_VIEW:                    return "buffer view";
        case VK_OBJECT_TYPE_IMAGE_VIEW:                     return "image view";
        case VK_OBJECT_TYPE_SHADER_MODULE:                  return "shader module";
        case VK_OBJECT_TYPE_PIPELINE_CACHE:                 return "pipeline cache";
        case VK_OBJECT_TYPE_PIPELINE_LAYOUT:                return "pipeline layout";
        case VK_OBJECT_TYPE_RENDER_PASS:                    return "render pass";
        case VK_OBJECT_TYPE_PIPELINE:                       return "pipeline";
        case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT:          return "descriptor set layout";
        case VK_OBJECT_TYPE_SAMPLER:                        return "sampler";
        case VK_OBJECT_TYPE_DESCRIPTOR_POOL:                return "descriptor pool";
        case VK_OBJECT_TYPE_DESCRIPTOR_SET:                 return "descriptor set";
        case VK_OBJECT_TYPE_FRAMEBUFFER:                    return "framebuffer";
        case VK_OBJECT_TYPE_COMMAND_POOL:                   return "command pool";
        case VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION:       return "sampler ycbcr conversion";
        case VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE:     return "descriptor update template";
        case VK_OBJECT_TYPE_SURFACE_KHR:                    return "surface KHR";
        case VK_OBJECT_TYPE_SWAPCHAIN_KHR:                  return "swapchain KHR";
        case VK_OBJECT_TYPE_DISPLAY_KHR:                    return "display KHR";
        case VK_OBJECT_TYPE_DISPLAY_MODE_KHR:               return "display mode KHR";
        case VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT:      return "debug report callback";
        case VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT:      return "debug utils messenger";
        case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR:     return "acceleration structure KHR";
        case VK_OBJECT_TYPE_VALIDATION_CACHE_EXT:           return "validation cache EXT";
        case VK_OBJECT_TYPE_PERFORMANCE_CONFIGURATION_INTEL:return "performance configuration INTEL";
        case VK_OBJECT_TYPE_DEFERRED_OPERATION_KHR:         return "deferred operation KHR";
        case VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NV:    return "indirect commands layout NV";
        case VK_OBJECT_TYPE_PRIVATE_DATA_SLOT_EXT:          return "private data slot EXT";
        default: return "unknown";
        // clang-format on
    }
  }
}  // namespace Sera