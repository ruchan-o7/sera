#pragma once
#include <vulkan/vulkan.h>
namespace Sera {
  // Loads the debug utils functions and initialized the debug callback.
  bool SetupDebugUtils(VkInstance                          instance,
                       VkDebugUtilsMessageSeverityFlagsEXT messageSeverity,
                       VkDebugUtilsMessageTypeFlagsEXT     messageType,
                       uint32_t                            IgnoreMessageCount,
                       const char* const*                  ppIgnoreMessageNames,
                       void*                               pUserData = nullptr);

  // Initializes the debug report callback.
  bool SetupDebugReport(VkInstance instance, VkDebugReportFlagBitsEXT flags,
                        void* pUserData = nullptr);

  // Clears the debug utils/debug report callback
  void FreeDebug(VkInstance instance);

  const char* VkObjectTypeToString(VkObjectType ObjectType);
}  // namespace Sera