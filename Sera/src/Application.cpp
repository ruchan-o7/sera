#include "Application.h"
#include "Log.h"
#include "Backend/VulkanInstance.h"
#include "Backend/VulkanPhysicalDevice.h"
#include "Backend/VulkanDevice.h"

//
// Adapted from Dear ImGui Vulkan example
//
#include "Window.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "glm/trigonometric.hpp"
#include "vulkan/vulkan_core.h"
#include <fstream>
#include <imgui_internal.h>
#include <stdio.h>   // printf, fprintf
#include <stdlib.h>  // abort
#include <array>

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include <iostream>

// Emedded font
#include "ImGui/Roboto-Regular.embed"

extern bool g_ApplicationRunning;

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to
// maximize ease of testing and compatibility with old VS compilers. To link
// with VS2010-era libraries, VS2015+ requires linking with
// legacy_stdio_definitions.lib, which we do using this pragma. Your own project
// should not be affected, as you are likely to link with a newer binary of GLFW
// that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && \
    !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

// #define IMGUI_UNLIMITED_FRAME_RATE
#ifdef _DEBUG
#define IMGUI_VULKAN_DEBUG_REPORT
#endif

static VkAllocationCallbacks      *g_Allocator              = NULL;
static Sera::VulkanInstance       *g_Instance               = nullptr;
static Sera::VulkanPhysicalDevice *g_PhysicalDevice         = nullptr;
static Sera::VulkanDevice         *g_Device                 = nullptr;
static Sera::Window               *g_Window                 = nullptr;
static uint32_t                    g_QueueFamily            = (uint32_t)-1;
static VkQueue                     g_Queue                  = VK_NULL_HANDLE;
static VkDebugReportCallbackEXT    g_DebugReport            = VK_NULL_HANDLE;
static VkPipelineCache             g_PipelineCache          = VK_NULL_HANDLE;
static VkDescriptorPool            g_DescriptorPool         = VK_NULL_HANDLE;
static VkPipeline                  g_GraphicPipeline        = VK_NULL_HANDLE;
static VkPipelineLayout            g_GraphicsPipelineLayout = VK_NULL_HANDLE;
static VkRenderPass                g_Renderpass             = VK_NULL_HANDLE;

static ImGui_ImplVulkanH_Window g_MainWindowData;
static int                      g_MinImageCount    = 3;
static bool                     g_SwapChainRebuild = false;

// Per-frame-in-flight
static std::vector<std::vector<VkCommandBuffer>> s_AllocatedCommandBuffers;
static std::vector<std::vector<std::function<void()>>> s_ResourceFreeQueue;

// Unlike g_MainWindowData.FrameIndex, this is not the the swapchain image index
// and is always guaranteed to increase (eg. 0, 1, 2, 0, 1, 2)
static uint32_t s_CurrentFrameIndex = 0;

static Sera::Application *s_Instance = nullptr;

void check_vk_result(VkResult err) {
  if (err == 0) return;
  fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
  if (err < 0) abort();
}

static void SetupVulkan(const char **extensions, uint32_t extensions_count) {
  VkResult err;

  // Create Vulkan Instance
  {
    Sera::VulkanInstance::Specs instanceSpecs;
    instanceSpecs.additionalExtensions.push_back(
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    g_Instance = new Sera::VulkanInstance(instanceSpecs);
  }

  // Select GPU
  g_PhysicalDevice =
      new Sera::VulkanPhysicalDevice(g_Instance->SelectPhysicalDevice());
  // Select graphics queue family
  {
    g_PhysicalDevice->SelectGraphicsQueueFamily();
    g_QueueFamily = g_PhysicalDevice->queueFamilyIndex;
  }
  // Create Logical Device (with 1 queue)
  {
    std::vector<const char *> ext;
    g_Device = new Sera::VulkanDevice(g_PhysicalDevice, g_Allocator,
                                      g_QueueFamily, ext);
    g_Queue  = g_Device->queue;
  }

  // Create Descriptor Pool
  {
    VkDescriptorPoolSize pool_sizes[] = {
        {               VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {         VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {         VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {  VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {  VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {      VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets       = 1000 * IM_ARRAYSIZE(pool_sizes);
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes    = pool_sizes;
    err = vkCreateDescriptorPool(g_Device->device, &pool_info, g_Allocator,
                                 &g_DescriptorPool);
    check_vk_result(err);
  }
}
static void SetupRenderpass() {
  VkAttachmentDescription colorAttachment{};
  colorAttachment.format         = g_Window->SurfaceFormat.format;
  colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentDescription depthAttachment{};
  depthAttachment.format         = VK_FORMAT_D32_SFLOAT;
  depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
  depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
  depthAttachment.finalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference colorAttachmentRef{};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthAttachmentRef{};
  depthAttachmentRef.attachment = 1;
  depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount    = 1;
  subpass.pColorAttachments       = &colorAttachmentRef;
  subpass.pDepthStencilAttachment = &depthAttachmentRef;

  VkSubpassDependency dependency{};
  dependency.srcSubpass   = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass   = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  std::array<VkAttachmentDescription, 2> attachments = {colorAttachment,
                                                        depthAttachment};
  VkRenderPassCreateInfo                 renderPassInfo{};
  renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  renderPassInfo.pAttachments    = attachments.data();
  renderPassInfo.subpassCount    = 1;
  renderPassInfo.pSubpasses      = &subpass;
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies   = &dependency;

  auto err = vkCreateRenderPass(g_Device->device, &renderPassInfo, nullptr,
                                &g_Renderpass);
  if (err != VK_SUCCESS) SR_CORE_ERROR("Could not load renderpass");
  g_Window->CreateDepths();
  g_Window->CreateFramebuffer(g_Renderpass);
}
// All the ImGui_ImplVulkanH_XXX structures/functions are optional helpers used
// by the demo. Your real engine/app may not use them.
static void SetupVulkanWindow(ImGui_ImplVulkanH_Window *wd,
                              VkSurfaceKHR surface, int width, int height) {
  wd->Surface = surface;

  // Check for WSI support
  VkBool32 res;
  vkGetPhysicalDeviceSurfaceSupportKHR(g_PhysicalDevice->physicalDevice,
                                       g_QueueFamily, wd->Surface, &res);
  if (res != VK_TRUE) {
    fprintf(stderr, "Error no WSI support on physical device 0\n");
    exit(-1);
  }

  // Select Surface Format
  const VkFormat requestSurfaceImageFormat[] = {
      VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
      VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM};
  const VkColorSpaceKHR requestSurfaceColorSpace =
      VK_COLORSPACE_SRGB_NONLINEAR_KHR;
  wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
      g_PhysicalDevice->physicalDevice, wd->Surface, requestSurfaceImageFormat,
      (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat),
      requestSurfaceColorSpace);

  // Select Present Mode
#ifdef IMGUI_UNLIMITED_FRAME_RATE
  VkPresentModeKHR present_modes[] = {VK_PRESENT_MODE_MAILBOX_KHR,
                                      VK_PRESENT_MODE_IMMEDIATE_KHR,
                                      VK_PRESENT_MODE_FIFO_KHR};
#else
  VkPresentModeKHR present_modes[] = {VK_PRESENT_MODE_FIFO_KHR};
#endif
  wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(
      g_PhysicalDevice->physicalDevice, wd->Surface, &present_modes[0],
      IM_ARRAYSIZE(present_modes));
  // printf("[vulkan] Selected PresentMode = %d\n", wd->PresentMode);

  // Create SwapChain, RenderPass, Framebuffer, etc.
  IM_ASSERT(g_MinImageCount >= 2);
  ImGui_ImplVulkanH_CreateOrResizeWindow(
      g_Instance->instance, g_PhysicalDevice->physicalDevice, g_Device->device,
      wd, g_QueueFamily, g_Allocator, width, height, g_MinImageCount);
}

static void CleanupVulkan() {
  vkDestroyDescriptorPool(g_Device->device, g_DescriptorPool, g_Allocator);

#ifdef IMGUI_VULKAN_DEBUG_REPORT
  // Remove the debug report callback
  auto vkDestroyDebugReportCallbackEXT =
      (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(
          g_Instance, "vkDestroyDebugReportCallbackEXT");
  vkDestroyDebugReportCallbackEXT(g_Instance, g_DebugReport, g_Allocator);
#endif  // IMGUI_VULKAN_DEBUG_REPORT

  vkDestroyDevice(g_Device->device, g_Allocator);
  vkDestroyInstance(g_Instance->instance, g_Allocator);
}

void read_file(const char *path, std::vector<char> &out) {
  std::ifstream fileStream{path, std::ios::ate | std::ios::binary};
  if (!fileStream.is_open()) {
    SR_CORE_ERROR("Could not open file: {0}", path);
    fileStream.close();
    return;
  }
  size_t fileSize = (size_t)fileStream.tellg();
  out.resize(fileSize);
  fileStream.seekg(0);
  fileStream.read(out.data(), fileSize);
  fileStream.close();
}

static void CleanupVulkanWindow() {
  ImGui_ImplVulkanH_DestroyWindow(g_Instance->instance, g_Device->device,
                                  &g_MainWindowData, g_Allocator);
}

static void FrameRender(ImGui_ImplVulkanH_Window *wd, ImDrawData *draw_data) {
  VkResult err;

  VkSemaphore image_acquired_semaphore =
      g_Window->frameSemaphores[g_Window->SemaphoreIndex]
          .ImageAcquiredSemaphore;
  VkSemaphore render_complete_semaphore =
      g_Window->frameSemaphores[g_Window->SemaphoreIndex]
          .RenderCompleteSemaphore;
  err = vkAcquireNextImageKHR(g_Device->device, g_Window->swapchain, UINT64_MAX,
                              image_acquired_semaphore, VK_NULL_HANDLE,
                              &wd->FrameIndex);
  if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
    g_SwapChainRebuild = true;
    return;
  }
  check_vk_result(err);

  s_CurrentFrameIndex = (s_CurrentFrameIndex + 1) % g_Window->ImageCount;

  Sera::Frame *frameData = &g_Window->frames[g_Window->FrameIndex];

  {
    err = vkWaitForFences(
        g_Device->device, 1, &frameData->Fence, VK_TRUE,
        UINT64_MAX);  // wait indefinitely instead of periodically checking
    check_vk_result(err);

    err = vkResetFences(g_Device->device, 1, &frameData->Fence);
    check_vk_result(err);
  }

  {
    // Free resources in queue
    for (auto &func : s_ResourceFreeQueue[s_CurrentFrameIndex]) func();
    s_ResourceFreeQueue[s_CurrentFrameIndex].clear();
  }
  {
    // Free command buffers allocated by Application::GetCommandBuffer
    // These use g_MainWindowData.FrameIndex and not s_CurrentFrameIndex because
    // they're tied to the swapchain image index
    auto &allocatedCommandBuffers =
        s_AllocatedCommandBuffers[g_Window->FrameIndex];
    if (allocatedCommandBuffers.size() > 0) {
      vkFreeCommandBuffers(g_Device->device, frameData->CommandPool,
                           (uint32_t)allocatedCommandBuffers.size(),
                           allocatedCommandBuffers.data());
      allocatedCommandBuffers.clear();
    }

    err = vkResetCommandPool(g_Device->device, frameData->CommandPool, 0);
    check_vk_result(err);
    VkCommandBufferBeginInfo info = {};
    info.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(frameData->CommandBuffer, &info);
    check_vk_result(err);
  }
  {
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {
        {0.0f, 0.0f, 0.0f, 1.0f}
    };
    clearValues[1].depthStencil = {1.0f, 0};
    VkRenderPassBeginInfo info  = {};
    info.sType                  = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass             = g_Renderpass;
    info.framebuffer            = frameData->Framebuffer;
    // g_Window->FrameBuffers[g_Window->ImageCount];  // fd->Framebuffer;
    info.renderArea.extent.width  = g_Window->Width;
    info.renderArea.extent.height = g_Window->Height;
    info.clearValueCount          = clearValues.size();
    info.pClearValues             = clearValues.data();
    vkCmdBeginRenderPass(frameData->CommandBuffer, &info,
                         VK_SUBPASS_CONTENTS_INLINE);
  }
  // DRAW COMMANDS
  {
    vkCmdBindPipeline(frameData->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      g_GraphicPipeline);
    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(300);
    viewport.height   = static_cast<float>(300);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(frameData->CommandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {300, 300};
    vkCmdSetScissor(frameData->CommandBuffer, 0, 1, &scissor);

    vkCmdDraw(frameData->CommandBuffer, 3, 1, 0, 0);
  }

  // Record dear imgui primitives into command buffer
  ImGui_ImplVulkan_RenderDrawData(draw_data, frameData->CommandBuffer);

  // Submit command buffer
  vkCmdEndRenderPass(frameData->CommandBuffer);
  {
    VkPipelineStageFlags wait_stage =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo info         = {};
    info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.waitSemaphoreCount   = 1;
    info.pWaitSemaphores      = &image_acquired_semaphore;
    info.pWaitDstStageMask    = &wait_stage;
    info.commandBufferCount   = 1;
    info.pCommandBuffers      = &frameData->CommandBuffer;
    info.signalSemaphoreCount = 1;
    info.pSignalSemaphores    = &render_complete_semaphore;

    err = vkEndCommandBuffer(frameData->CommandBuffer);
    check_vk_result(err);
    err = vkQueueSubmit(g_Queue, 1, &info, frameData->Fence);
    check_vk_result(err);
  }
}

static void FramePresent(ImGui_ImplVulkanH_Window *wd) {
  if (g_SwapChainRebuild) return;
  VkSemaphore render_complete_semaphore =
      g_Window->frameSemaphores[g_Window->SemaphoreIndex]
          .RenderCompleteSemaphore;
  VkPresentInfoKHR info   = {};
  info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  info.waitSemaphoreCount = 1;
  info.pWaitSemaphores    = &render_complete_semaphore;
  info.swapchainCount     = 1;
  info.pSwapchains        = &g_Window->swapchain;
  info.pImageIndices      = &g_Window->FrameIndex;
  VkResult err            = vkQueuePresentKHR(g_Queue, &info);
  if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
    g_SwapChainRebuild = true;
    return;
  }
  check_vk_result(err);
  g_Window->SemaphoreIndex =
      (g_Window->SemaphoreIndex + 1) %
      g_Window->ImageCount;  // Now we can use the next set of semaphores
}

static void glfw_error_callback(int error, const char *description) {
  SR_CORE_ERROR("GLFW Error: {0}: {1}", error, description);
}
static void SetuPipeline() {
  auto create_shader_module = [&](const std::vector<char> &code) {
    VkShaderModuleCreateInfo ci{};
    ci.sType              = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize           = code.size();
    ci.pCode              = reinterpret_cast<const uint32_t *>(code.data());
    VkShaderModule module = {};
    if (vkCreateShaderModule(g_Device->device, &ci, nullptr, &module)) {
      SR_CORE_ERROR("Could not load Shader");
    }
    return module;
  };
  std::vector<char> vertShaderCode;
  std::vector<char> fragShaderCode;

  read_file("Assets/shaders/triangle-vert.spv", vertShaderCode);
  read_file("Assets/shaders/triangle-frag.spv", fragShaderCode);

  auto vertModule = create_shader_module(vertShaderCode);
  auto fragModule = create_shader_module(fragShaderCode);

  VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
  vertShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertShaderStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
  vertShaderStageInfo.module = vertModule;
  vertShaderStageInfo.pName  = "main";

  VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
  fragShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragShaderStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragShaderStageInfo.module = fragModule;
  fragShaderStageInfo.pName  = "main";

  VkPipelineShaderStageCreateInfo      shaderStages[] = {vertShaderStageInfo,
                                                         fragShaderStageInfo};
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount   = 0;
  vertexInputInfo.vertexAttributeDescriptionCount = 0;

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount  = 1;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable        = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth               = 1.0f;
  rasterizer.cullMode                = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace               = VK_FRONT_FACE_CLOCKWISE;
  rasterizer.depthBiasEnable         = VK_FALSE;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable  = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable     = VK_FALSE;
  colorBlending.logicOp           = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount   = 1;
  colorBlending.pAttachments      = &colorBlendAttachment;
  colorBlending.blendConstants[0] = 0.0f;
  colorBlending.blendConstants[1] = 0.0f;
  colorBlending.blendConstants[2] = 0.0f;
  colorBlending.blendConstants[3] = 0.0f;

  std::array<VkDynamicState, 2>    dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                    VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates    = dynamicStates.data();

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount         = 0;
  pipelineLayoutInfo.pushConstantRangeCount = 0;

  if (vkCreatePipelineLayout(g_Device->device, &pipelineLayoutInfo, nullptr,
                             &g_GraphicsPipelineLayout) != VK_SUCCESS) {
    SR_CORE_ERROR("Failed to create pipeline layout");
    return;
  }

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType      = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages    = shaderStages;
  pipelineInfo.pVertexInputState   = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState      = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState   = &multisampling;
  pipelineInfo.pColorBlendState    = &colorBlending;
  pipelineInfo.pDynamicState       = &dynamicState;
  pipelineInfo.layout              = g_GraphicsPipelineLayout;
  pipelineInfo.renderPass          = g_Renderpass;
  pipelineInfo.subpass             = 0;
  pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;

  if (vkCreateGraphicsPipelines(g_Device->device, VK_NULL_HANDLE, 1,
                                &pipelineInfo, nullptr,
                                &g_GraphicPipeline) != VK_SUCCESS) {
    SR_CORE_ERROR("Failed to create graphics pipeline");
  }

  vkDestroyShaderModule(g_Device->device, fragModule, nullptr);
  vkDestroyShaderModule(g_Device->device, vertModule, nullptr);
}
namespace Sera {

  Application::Application(const ApplicationSpecification &specification)
      : m_Specification(specification) {
    s_Instance = this;

    Init();
  }

  Application::~Application() {
    Shutdown();

    s_Instance = nullptr;
  }

  GLFWwindow *Application::GetWindowHandle() const {
    return g_Window->GetHandle();
  }
  Application &Application::Get() { return *s_Instance; }

  void Application::Init() {
    // Setup GLFW window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
      SR_CORE_CRITICAL("Could not initialize GLFW!");
      return;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // Setup Vulkan
    if (!glfwVulkanSupported()) {
      SR_CORE_CRITICAL("GLFW: Vulkan not supported");
      return;
    }
    uint32_t     extensions_count = 0;
    const char **extensions =
        glfwGetRequiredInstanceExtensions(&extensions_count);
    SetupVulkan(extensions, extensions_count);

    ImGui_ImplVulkanH_Window *wd = &g_MainWindowData;

    g_Window =
        Window::Create(g_Instance, g_PhysicalDevice, g_Allocator, g_Device);
    SetupRenderpass();

    VkResult err;

    s_AllocatedCommandBuffers.resize(g_Window->ImageCount);
    s_ResourceFreeQueue.resize(g_Window->ImageCount);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |=
        ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable
    // Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;  // Enable Docking
    io.ConfigFlags |=
        ImGuiConfigFlags_ViewportsEnable;  // Enable Multi-Viewport /
                                           // Platform Windows
    // io.ConfigViewportsNoAutoMerge = true;
    // io.ConfigViewportsNoTaskBarIcon = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsClassic();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform
    // windows can look identical to regular ones.
    ImGuiStyle &style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
      style.WindowRounding              = 0.0f;
      style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(g_Window->GetHandle(), true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance                  = g_Instance->instance;
    init_info.PhysicalDevice            = g_PhysicalDevice->physicalDevice;
    init_info.Device                    = g_Device->device;
    init_info.QueueFamily               = g_QueueFamily;
    init_info.Queue                     = g_Queue;
    init_info.PipelineCache             = g_PipelineCache;
    init_info.DescriptorPool            = g_DescriptorPool;
    init_info.Subpass                   = 0;
    init_info.MinImageCount             = g_MinImageCount;
    init_info.ImageCount                = g_Window->ImageCount + 1;
    init_info.MSAASamples               = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator                 = g_Allocator;
    init_info.CheckVkResultFn           = check_vk_result;
    init_info.RenderPass                = g_Renderpass;  // wd->RenderPass;
    ImGui_ImplVulkan_Init(&init_info);

    // Load default font
    ImFontConfig fontConfig;
    fontConfig.FontDataOwnedByAtlas = false;
    ImFont *robotoFont              = io.Fonts->AddFontFromMemoryTTF(
        (void *)g_RobotoRegular, sizeof(g_RobotoRegular), 20.0f, &fontConfig);
    io.FontDefault = robotoFont;

    // Upload Fonts
    {
      // Use any command queue
      VkCommandPool command_pool =
          g_Window->frames[g_Window->FrameIndex].CommandPool;
      VkCommandBuffer command_buffer =
          g_Window->frames[g_Window->FrameIndex].CommandBuffer;

      err = vkResetCommandPool(g_Device->device, command_pool, 0);
      check_vk_result(err);
      VkCommandBufferBeginInfo begin_info = {};
      begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      err = vkBeginCommandBuffer(command_buffer, &begin_info);
      check_vk_result(err);

      ImGui_ImplVulkan_CreateFontsTexture();
      // ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

      VkSubmitInfo end_info       = {};
      end_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
      end_info.commandBufferCount = 1;
      end_info.pCommandBuffers    = &command_buffer;
      err                         = vkEndCommandBuffer(command_buffer);
      check_vk_result(err);
      err = vkQueueSubmit(g_Queue, 1, &end_info, VK_NULL_HANDLE);
      check_vk_result(err);

      err = vkDeviceWaitIdle(g_Device->device);
      check_vk_result(err);
      ImGui_ImplVulkan_DestroyFontsTexture();
      // ImGui_ImplVulkan_DestroyFontUploadObjects();
    }
    SetuPipeline();
  }

  void Application::Shutdown() {
    for (auto &layer : m_LayerStack) layer->OnDetach();

    m_LayerStack.clear();

    // Cleanup
    VkResult err = vkDeviceWaitIdle(g_Device->device);
    check_vk_result(err);

    // Free resources in queue
    for (auto &queue : s_ResourceFreeQueue) {
      for (auto &func : queue) func();
    }
    s_ResourceFreeQueue.clear();

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    CleanupVulkanWindow();
    CleanupVulkan();

    glfwDestroyWindow(g_Window->GetHandle());
    glfwTerminate();

    g_ApplicationRunning = false;
  }

  void Application::Run() {
    m_Running = true;

    ImGui_ImplVulkanH_Window *wd          = &g_MainWindowData;
    ImVec4                    clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    ImGuiIO                  &io          = ImGui::GetIO();

    // Main loop
    while (!glfwWindowShouldClose(g_Window->GetHandle()) && m_Running) {
      glfwPollEvents();

      for (auto &layer : m_LayerStack) layer->OnUpdate(m_TimeStep);

      // Resize swap chain?
      if (g_SwapChainRebuild) {
        int width, height;
        glfwGetFramebufferSize(g_Window->GetHandle(), &width, &height);
        if (width > 0 && height > 0) {
          ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
          ImGui_ImplVulkanH_CreateOrResizeWindow(
              g_Instance->instance, g_PhysicalDevice->physicalDevice,
              g_Device->device, &g_MainWindowData, g_QueueFamily, g_Allocator,
              width, height, g_MinImageCount);
          g_MainWindowData.FrameIndex = 0;

          // Clear allocated command buffers from here since entire pool is
          // destroyed
          s_AllocatedCommandBuffers.clear();
          s_AllocatedCommandBuffers.resize(g_MainWindowData.ImageCount);

          g_SwapChainRebuild = false;
        }
      }

      // Start the Dear ImGui frame
      ImGui_ImplVulkan_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();

      {
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

        // We are using the ImGuiWindowFlags_NoDocking flag to make the parent
        // window not dockable into, because it would be confusing to have two
        // docking targets within each others.
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
        if (m_MenubarCallback) window_flags |= ImGuiWindowFlags_MenuBar;

        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar |
                        ImGuiWindowFlags_NoCollapse |
                        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus |
                        ImGuiWindowFlags_NoNavFocus;

        // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will
        // render our background and handle the pass-thru hole, so we ask
        // Begin() to not render a background.
        // if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
        //   window_flags |= ImGuiWindowFlags_NoBackground;

        // Important: note that we proceed even if Begin() returns false (aka
        // window is collapsed). This is because we want to keep our DockSpace()
        // active. If a DockSpace() is inactive, all active windows docked into
        // it will lose their parent and become undocked. We cannot preserve the
        // docking relationship between an active window and an inactive
        // docking, otherwise any change of dockspace/settings would lead to
        // windows being stuck in limbo and never being visible.
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace Demo", nullptr, window_flags);
        ImGui::PopStyleVar();

        ImGui::PopStyleVar(2);

        // Submit the DockSpace
        ImGuiIO &io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
          ImGuiID dockspace_id = ImGui::GetID("VulkanAppDockspace");
          ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }

        if (m_MenubarCallback) {
          if (ImGui::BeginMenuBar()) {
            m_MenubarCallback();
            ImGui::EndMenuBar();
          }
        }

        for (auto &layer : m_LayerStack) layer->OnUIRender();

        ImGui::End();
      }

      // Rendering
      ImGui::Render();
      ImDrawData *main_draw_data    = ImGui::GetDrawData();
      const bool  main_is_minimized = (main_draw_data->DisplaySize.x <= 0.0f ||
                                      main_draw_data->DisplaySize.y <= 0.0f);
      wd->ClearValue.color.float32[0] = clear_color.x * clear_color.w;
      wd->ClearValue.color.float32[1] = clear_color.y * clear_color.w;
      wd->ClearValue.color.float32[2] = clear_color.z * clear_color.w;
      wd->ClearValue.color.float32[3] = clear_color.w;
      if (!main_is_minimized) FrameRender(wd, main_draw_data);

      // Update and Render additional Platform Windows
      if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
      }

      // Present Main Platform Window
      if (!main_is_minimized) FramePresent(wd);

      float time      = GetTime();
      m_FrameTime     = time - m_LastFrameTime;
      m_TimeStep      = glm::min<float>(m_FrameTime, 0.0333f);
      m_LastFrameTime = time;
    }
  }

  void Application::Close() { m_Running = false; }

  float Application::GetTime() { return (float)glfwGetTime(); }

  VkInstance Application::GetInstance() { return g_Instance->instance; }

  VkPhysicalDevice Application::GetPhysicalDevice() {
    return g_PhysicalDevice->physicalDevice;
  }

  VkDevice Application::GetDevice() { return g_Device->device; }

  VkCommandBuffer Application::GetCommandBuffer(bool begin) {
    ImGui_ImplVulkanH_Window *wd = &g_MainWindowData;

    // Use any command queue
    VkCommandPool command_pool = wd->Frames[wd->FrameIndex].CommandPool;

    VkCommandBufferAllocateInfo cmdBufAllocateInfo = {};
    cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufAllocateInfo.commandPool        = command_pool;
    cmdBufAllocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufAllocateInfo.commandBufferCount = 1;

    // VkCommandBuffer &command_buffer =
    //     s_AllocatedCommandBuffers[wd->FrameIndex].emplace_back();
    // FIXME: Fix this command buffer allocation
    VkCommandBuffer
         command_buffer;  //= s_AllocatedCommandBuffers[wd->FrameIndex];
    auto err = vkAllocateCommandBuffers(g_Device->device, &cmdBufAllocateInfo,
                                        &command_buffer);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(command_buffer, &begin_info);
    check_vk_result(err);

    return command_buffer;
  }

  void Application::FlushCommandBuffer(VkCommandBuffer commandBuffer) {
    const uint64_t DEFAULT_FENCE_TIMEOUT = 100000000000;

    VkSubmitInfo end_info       = {};
    end_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    end_info.commandBufferCount = 1;
    end_info.pCommandBuffers    = &commandBuffer;
    auto err                    = vkEndCommandBuffer(commandBuffer);
    check_vk_result(err);

    // Create fence to ensure that the command buffer has finished executing
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags             = 0;
    VkFence fence;
    err = vkCreateFence(g_Device->device, &fenceCreateInfo, nullptr, &fence);
    check_vk_result(err);

    err = vkQueueSubmit(g_Queue, 1, &end_info, fence);
    check_vk_result(err);

    err = vkWaitForFences(g_Device->device, 1, &fence, VK_TRUE,
                          DEFAULT_FENCE_TIMEOUT);
    check_vk_result(err);

    vkDestroyFence(g_Device->device, fence, nullptr);
  }

  void Application::SubmitResourceFree(std::function<void()> &&func) {
    s_ResourceFreeQueue[s_CurrentFrameIndex].emplace_back(func);
  }

}  // namespace Sera
