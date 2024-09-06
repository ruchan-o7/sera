#pragma once
#include <vulkan/vulkan.h>
namespace Sera {
  class VulkanDevice;
  class VulkanRenderPipeline {
    public:
      struct CreateInfo {
          VulkanDevice*                device;
          const VkAllocationCallbacks* allocator;
          VkShaderModule               vertexShader;
          VkShaderModule               fragmentShader;
          VkCullModeFlags              cullMode    = VK_CULL_MODE_BACK_BIT;
          VkFrontFace                  frontFace   = VK_FRONT_FACE_CLOCKWISE;
          float                        lineWidth   = 1.0F;
          VkSampleCountFlagBits        sampleCount = VK_SAMPLE_COUNT_1_BIT;
          VkRenderPass                 renderPass;
      };
      static VulkanRenderPipeline* Create(CreateInfo info);
      VkPipeline                   GetHandle() const { return m_Handle; }
      ~VulkanRenderPipeline();

    private:
      VulkanRenderPipeline(CreateInfo info);
      CreateInfo       m_Info;
      VkPipeline       m_Handle;
      VkPipelineLayout m_Layout;
  };
}  // namespace Sera
