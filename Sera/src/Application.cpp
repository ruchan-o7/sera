#include "Application.h"
#include "Backend/VulkanRenderPipeline.h"
#include "Backend/VulkanRenderpass.h"
#include "Backend/VulkanSwapchain.h"
#include "Log.h"
#include "Backend/VulkanInstance.h"
#include "Backend/VulkanPhysicalDevice.h"
#include "Backend/VulkanDevice.h"

//
// Adapted from Dear ImGui Vulkan example
//
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "vulkan/vulkan_core.h"
#include <fstream>
#include <imgui_internal.h>
#include <stdio.h>   // printf, fprintf
#include <stdlib.h>  // abort
#include <array>
#include <vector>

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

static VkAllocationCallbacks      *g_Allocator      = NULL;
static Sera::VulkanInstance       *g_Instance       = nullptr;
static Sera::VulkanPhysicalDevice *g_PhysicalDevice = nullptr;
static Sera::VulkanDevice         *g_Device         = nullptr;
static uint32_t                    g_QueueFamily    = (uint32_t)-1;
static VkQueue                     g_Queue          = VK_NULL_HANDLE;
static VkDebugReportCallbackEXT    g_DebugReport    = VK_NULL_HANDLE;
static VkPipelineCache             g_PipelineCache  = VK_NULL_HANDLE;
static VkDescriptorPool            g_DescriptorPool = VK_NULL_HANDLE;
static Sera::VulkanRenderPipeline *g_Pipeline       = nullptr;
// static VkPipeline                   g_GraphicPipeline        =
// VK_NULL_HANDLE; static VkPipelineLayout             g_GraphicsPipelineLayout
// = VK_NULL_HANDLE;
static Sera::VulkanRenderPass      *g_Renderpass = nullptr;
static VkSurfaceKHR                 g_Surface    = VK_NULL_HANDLE;
static VkSurfaceFormatKHR           g_SurfaceFormat;
static std::vector<VkCommandBuffer> g_CommandBuffers;
static Sera::VulkanSwapchain       *g_Swapchain = nullptr;

static ImGui_ImplVulkanH_Window g_MainWindowData;
static bool                     g_SwapChainRebuild = false;

// Per-frame-in-flight
static std::vector<std::vector<VkCommandBuffer>> s_AllocatedCommandBuffers;
static std::vector<std::vector<std::function<void()>>> s_ResourceFreeQueue;

static Sera::Application *s_Instance = nullptr;

void check_vk_result(VkResult err) {
  if (err == 0) return;
  SR_CORE_ERROR("[vulkan] Error: VkResult = {0}", (uint32_t)err);
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

  // Create Descriptor Pool for imgui
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
  Sera::VulkanRenderPass::CreateInfo info{};
  info.allocator     = g_Allocator;
  info.device        = g_Device;
  info.surfaceFormat = g_Swapchain->SurfaceFormat;
  g_Renderpass       = Sera::VulkanRenderPass::Create(info);
  g_Swapchain->CreateFramebuffer(g_Renderpass->GetHandle());
}
// All the ImGui_ImplVulkanH_XXX structures/functions are optional helpers used
// by the demo. Your real engine/app may not use them.
static void InitPools() {
  VkResult err;

  for (int i = 0; i < g_Swapchain->ImageCount; i++) {
    auto *frame = &g_Swapchain->Frames[i];
    {
      VkCommandPoolCreateInfo info = {};
      info.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
      info.flags                   = 0;
      info.queueFamilyIndex        = g_QueueFamily;
      err = vkCreateCommandPool(g_Device->device, &info, g_Allocator,
                                &frame->CommandPool);
    }
    {
      VkCommandBufferAllocateInfo info = {};
      info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      info.commandPool        = frame->CommandPool;
      info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      info.commandBufferCount = 1;
      err = vkAllocateCommandBuffers(g_Device->device, &info,
                                     &frame->CommandBuffer);
      check_vk_result(err);
    }
  }
}
static void SetupVulkanWindow(int width, int height) {
  // Check for WSI support
  VkBool32 res;
  vkGetPhysicalDeviceSurfaceSupportKHR(g_PhysicalDevice->physicalDevice,
                                       g_QueueFamily, g_Surface, &res);
  if (res != VK_TRUE) {
    SR_CORE_ERROR("Error no WSI support on physical device");
    exit(-1);
  }

  // Select Surface Format
  const VkFormat requestSurfaceImageFormat[] = {
      VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
      VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM};
  const VkColorSpaceKHR requestSurfaceColorSpace =
      VK_COLORSPACE_SRGB_NONLINEAR_KHR;
  g_SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
      g_PhysicalDevice->physicalDevice, g_Surface, requestSurfaceImageFormat,
      (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat),
      requestSurfaceColorSpace);

  VkPresentModeKHR present_modes[] = {VK_PRESENT_MODE_FIFO_KHR};
  auto             presentMode     = ImGui_ImplVulkanH_SelectPresentMode(
      g_PhysicalDevice->physicalDevice, g_Surface, &present_modes[0],
      IM_ARRAYSIZE(present_modes));
  g_Swapchain =
      Sera::VulkanSwapchain::Create(g_Instance, g_PhysicalDevice, g_Allocator,
                                    g_Device, true, g_SurfaceFormat, g_Surface);
  g_Swapchain->Resize(width, height);
  g_CommandBuffers.resize(g_Swapchain->ImageCount, VK_NULL_HANDLE);
  InitPools();
}

static inline VkSemaphore GetImageAcquiredSemaphore() {
  return g_Swapchain->FrameSemaphoress[g_Swapchain->CurrentFrame]
      .ImageAvailableSemaphore;
}
static inline VkSemaphore GetRenderCompleteSemaphore() {
  return g_Swapchain->FrameSemaphoress[g_Swapchain->CurrentFrame]
      .RenderCompleteSemaphore;
}

static void CleanupVulkan() {
  vkDestroyDescriptorPool(g_Device->device, g_DescriptorPool, g_Allocator);
  delete g_Renderpass;
  delete g_Swapchain;
  vkDestroySurfaceKHR(g_Instance->instance, g_Surface, g_Allocator);
  delete g_Pipeline;
  delete g_Device;
  delete g_Instance;
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

static void FrameRender(ImDrawData *draw_data) {
  VkResult err;

  Sera::Frame *frameData = &g_Swapchain->Frames[g_Swapchain->CurrentFrame];

  auto image_acquired_semaphore = GetImageAcquiredSemaphore();

  err = vkWaitForFences(g_Device->device, 1, &frameData->Fence, VK_TRUE,
                        UINT64_MAX);

  err = vkAcquireNextImageKHR(g_Device->device, g_Swapchain->Get(), UINT64_MAX,
                              image_acquired_semaphore, VK_NULL_HANDLE,
                              &g_Swapchain->ImageIndex);
  if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
    g_SwapChainRebuild = true;
    return;
  }
  check_vk_result(err);

  err = vkResetFences(g_Device->device, 1, &frameData->Fence);
  check_vk_result(err);

  {
    // Free resources in queue
    for (auto &func : s_ResourceFreeQueue[g_Swapchain->CurrentFrame]) func();
    s_ResourceFreeQueue[g_Swapchain->CurrentFrame].clear();
  }
  {
    // Free command buffers allocated by Application::GetCommandBuffer
    // These use g_MainWindowData.FrameIndex and not s_CurrentFrameIndex because
    // they're tied to the swapchain image index
    auto &allocatedCommandBuffers =
        s_AllocatedCommandBuffers[g_Swapchain->CurrentFrame];
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
    clearValues[1].depthStencil   = {1.0f, 0};
    VkRenderPassBeginInfo info    = {};
    info.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass               = g_Renderpass->GetHandle();
    info.framebuffer              = frameData->Framebuffer;
    info.renderArea.extent.width  = g_Swapchain->GetWidth();
    info.renderArea.extent.height = g_Swapchain->GetHeight();
    info.clearValueCount          = clearValues.size();
    info.pClearValues             = clearValues.data();
    vkCmdBeginRenderPass(frameData->CommandBuffer, &info,
                         VK_SUBPASS_CONTENTS_INLINE);
  }
  // DRAW COMMANDS
  {
    vkCmdBindPipeline(frameData->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      g_Pipeline->GetHandle());
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
    auto render_complete_semaphore = GetRenderCompleteSemaphore();
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

  auto err = g_Swapchain->Present(g_Queue);
  if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
    g_SwapChainRebuild = true;
    return;
  }
  check_vk_result(err);
  g_Swapchain->CurrentFrame =
      (g_Swapchain->CurrentFrame + 1) % g_Swapchain->ImageCount;
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

  Sera::VulkanRenderPipeline::CreateInfo info;
  info.device         = g_Device;
  info.allocator      = g_Allocator;
  info.vertexShader   = vertModule;
  info.fragmentShader = fragModule;
  info.renderPass     = g_Renderpass->GetHandle();
  g_Pipeline          = Sera::VulkanRenderPipeline::Create(info);

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

    m_WindowHandle =
        glfwCreateWindow(m_Specification.Width, m_Specification.Height,
                         m_Specification.Name.c_str(), nullptr, nullptr);

    auto err = glfwCreateWindowSurface(g_Instance->instance, m_WindowHandle,
                                       g_Allocator, &g_Surface);
    SetupVulkanWindow(m_Specification.Width, m_Specification.Height);
    SetupRenderpass();

    s_AllocatedCommandBuffers.resize(g_Swapchain->ImageCount);
    s_ResourceFreeQueue.resize(g_Swapchain->ImageCount);

    VkBool32                  res;
    ImGui_ImplVulkanH_Window *wd = &g_MainWindowData;
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
        ImGuiConfigFlags_ViewportsEnable;  // Enable Multi-Viewport
                                           // / Platform Windows
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
    ImGui_ImplGlfw_InitForVulkan(GetWindowHandle(), true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance                  = g_Instance->instance;
    init_info.PhysicalDevice            = g_PhysicalDevice->physicalDevice;
    init_info.Device                    = g_Device->device;
    init_info.QueueFamily               = g_QueueFamily;
    init_info.Queue                     = g_Queue;
    init_info.PipelineCache             = g_PipelineCache;
    init_info.DescriptorPool            = g_DescriptorPool;
    init_info.Subpass                   = 0;
    init_info.MinImageCount             = g_Swapchain->ImageCount;
    init_info.ImageCount                = g_Swapchain->ImageCount + 1;
    init_info.MSAASamples               = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator                 = g_Allocator;
    init_info.CheckVkResultFn           = check_vk_result;
    init_info.RenderPass = g_Renderpass->GetHandle();  // wd->RenderPass;
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
          g_Swapchain->Frames[g_Swapchain->CurrentFrame].CommandPool;
      VkCommandBuffer command_buffer =
          g_Swapchain->Frames[g_Swapchain->CurrentFrame].CommandBuffer;

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

    glfwDestroyWindow(m_WindowHandle);
    glfwTerminate();

    g_ApplicationRunning = false;
  }

  void Application::Run() {
    m_Running = true;

    ImGui_ImplVulkanH_Window *wd          = &g_MainWindowData;
    ImVec4                    clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    ImGuiIO                  &io          = ImGui::GetIO();

    // Main loop
    while (!glfwWindowShouldClose(m_WindowHandle) && m_Running) {
      glfwPollEvents();

      for (auto &layer : m_LayerStack) layer->OnUpdate(m_TimeStep);

      // Resize swap chain?
      if (g_SwapChainRebuild) {
        int width, height;
        glfwGetFramebufferSize(m_WindowHandle, &width, &height);
        if (width > 0 && height > 0) {
          g_Device->WaitIdle();
          g_Swapchain->Resize(width, height);
          g_Swapchain->CurrentFrame = 0;
          InitPools();
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
      if (!main_is_minimized) FrameRender(main_draw_data);

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
    s_ResourceFreeQueue[g_Swapchain->CurrentFrame].emplace_back(func);
  }

}  // namespace Sera
