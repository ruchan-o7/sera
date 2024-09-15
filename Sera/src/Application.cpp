#ifndef ENGINE_DLL
#define ENGINE_DLL 1
#endif

#if PLATFORM_WIN32
#define GLFW_EXPOSE_NATIVE_WIN32 1
#endif

#if D3D11_SUPPORTED
#include "Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"
#endif
#if D3D12_SUPPORTED
#include "Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h"
#endif
#if GL_SUPPORTED
#include "Graphics/GraphicsEngineOpenGL/interface/EngineFactoryOpenGL.h"
#endif
#if VULKAN_SUPPORTED
#include "Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h"
#endif
#if METAL_SUPPORTED
#include "Graphics/GraphicsEngineMetal/interface/EngineFactoryMtl.h"
#endif

#ifdef GetObject
#undef GetObject
#endif
#ifdef CreateWindow
#undef CreateWindow
#endif

#include "Application.h"
#include "Log.h"

#include <Common/interface/RefCntAutoPtr.hpp>
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include <ImGuiImplWin32.hpp>

using namespace Diligent;

#include <GLFW/glfw3.h>
#include "GLFW/glfw3native.h"
#ifdef GetObject
#undef GetObject
#endif
#ifdef CreateWindow
#undef CreateWindow
#endif

#ifdef _WIN32
// #undef FindResourceA
#ifndef NOMINMAX
#define NOMINMAX
#endif

#endif

// Emedded font
#include "ImGui/Roboto-Regular.embed"
#include <imgui.h>

// #include <imgui.h>

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

static Sera::Application                 *s_Instance = nullptr;
static RefCntAutoPtr<IRenderDevice>       m_pDevice;
static RefCntAutoPtr<IDeviceContext>      m_pImmediateContext;
static RefCntAutoPtr<ISwapChain>          m_pSwapChain;
static RefCntAutoPtr<IPipelineState>      m_pPSO;
static std::unique_ptr<ImGuiImplDiligent> m_pImGui;

static const char *VSSource = R"(
struct PSInput 
{ 
    float4 Pos   : SV_POSITION; 
    float3 Color : COLOR; 
};

void main(in  uint    VertId : SV_VertexID,
          out PSInput PSIn) 
{
    float4 Pos[3];
    Pos[0] = float4(-0.5, -0.5, 0.0, 1.0);
    Pos[1] = float4( 0.0, +0.5, 0.0, 1.0);
    Pos[2] = float4(+0.5, -0.5, 0.0, 1.0);

    float3 Col[3];
    Col[0] = float3(1.0, 0.0, 0.0); // red
    Col[1] = float3(0.0, 1.0, 0.0); // green
    Col[2] = float3(0.0, 0.0, 1.0); // blue

    PSIn.Pos   = Pos[VertId];
    PSIn.Color = Col[VertId];
}
)";

static const char *PSSource = R"(
struct PSInput 
{ 
    float4 Pos   : SV_POSITION; 
    float3 Color : COLOR; 
};

struct PSOutput
{ 
    float4 Color : SV_TARGET; 
};

void main(in  PSInput  PSIn,
          out PSOutput PSOut)
{
    PSOut.Color = float4(PSIn.Color.rgb, 1.0);
}
)";

static void glfw_error_callback(int error, const char *description) {
  SR_CORE_ERROR("GLFW Error: {0}: {1}", error, description);
}
static void InitializeDiligentEngine(GLFWwindow *window) {
  auto hWnd = glfwGetWin32Window(window);

  SwapChainDesc SCDesc;

  EngineVkCreateInfo EngineCI;
  auto               GetEngineFactoryVk = LoadGraphicsEngineVk();

  auto *pFactoryVk = GetEngineFactoryVk();
  pFactoryVk->CreateDeviceAndContextsVk(EngineCI, &m_pDevice,
                                        &m_pImmediateContext);

  if (!m_pSwapChain && hWnd != nullptr) {
    Win32NativeWindow Window{hWnd};
    pFactoryVk->CreateSwapChainVk(m_pDevice, m_pImmediateContext, SCDesc,
                                  Window, &m_pSwapChain);
  }

  GraphicsPipelineStateCreateInfo PSOCreateInfo;

  PSOCreateInfo.PSODesc.Name = "Simple triangle PSO";

  PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

  PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
  PSOCreateInfo.GraphicsPipeline.RTVFormats[0] =
      m_pSwapChain->GetDesc().ColorBufferFormat;
  PSOCreateInfo.GraphicsPipeline.DSVFormat =
      m_pSwapChain->GetDesc().DepthBufferFormat;
  PSOCreateInfo.GraphicsPipeline.PrimitiveTopology =
      PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
  PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

  ShaderCreateInfo ShaderCI;
  ShaderCI.SourceLanguage                  = SHADER_SOURCE_LANGUAGE_HLSL;
  ShaderCI.Desc.UseCombinedTextureSamplers = true;
  RefCntAutoPtr<IShader> pVS;
  {
    ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
    ShaderCI.EntryPoint      = "main";
    ShaderCI.Desc.Name       = "Triangle vertex shader";
    ShaderCI.Source          = VSSource;
    m_pDevice->CreateShader(ShaderCI, &pVS);
  }

  RefCntAutoPtr<IShader> pPS;
  {
    ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
    ShaderCI.EntryPoint      = "main";
    ShaderCI.Desc.Name       = "Triangle pixel shader";
    ShaderCI.Source          = PSSource;
    m_pDevice->CreateShader(ShaderCI, &pPS);
  }

  PSOCreateInfo.pVS = pVS;
  PSOCreateInfo.pPS = pPS;
  m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPSO);
  m_pImGui =
      ImGuiImplWin32::Create(ImGuiDiligentCreateInfo{m_pDevice, SCDesc}, hWnd);
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

    m_WindowHandle =
        glfwCreateWindow(m_Specification.Width, m_Specification.Height,
                         m_Specification.Name.c_str(), nullptr, nullptr);

    glfwSetWindowUserPointer(m_WindowHandle, this);

    InitializeDiligentEngine(m_WindowHandle);

    glfwSetFramebufferSizeCallback(m_WindowHandle,[](GLFWwindow* window,int width,int heigth){
        m_pSwapChain->Resize(width,heigth);
        });
    glfwSetMouseButtonCallback(m_WindowHandle, [](GLFWwindow* w, int button, int action, int mods) {
        ImGuiIO& io = ImGui::GetIO();
        if (action == GLFW_PRESS)
            io.MouseDown[button] = true;
        if (action == GLFW_RELEASE)
            io.MouseDown[button] = false;
        });
    glfwSetKeyCallback(m_WindowHandle, [](GLFWwindow* w, int key, int scancode, int action, int mods) {
        ImGuiIO& io = ImGui::GetIO();
        if (action == GLFW_PRESS)
            io.KeysDown[key] = true;
        else if (action == GLFW_RELEASE)
            io.KeysDown[key] = false;

        io.KeyCtrl = ( mods & GLFW_MOD_CONTROL ) != 0;
        io.KeyShift = ( mods & GLFW_MOD_SHIFT ) != 0;
        io.KeyAlt = ( mods & GLFW_MOD_ALT ) != 0;
        io.KeySuper = ( mods & GLFW_MOD_SUPER ) != 0;

        });
    glfwSetScrollCallback(m_WindowHandle, [](GLFWwindow* window, double xOffset,double yOffset) {
        ImGuiIO& io = ImGui::GetIO();
        io.MouseWheelH += (float) xOffset;
        io.MouseWheel += (float) yOffset;
        });
    glfwSetCharCallback(m_WindowHandle, [](GLFWwindow* window, unsigned int c) {
        ImGuiIO& io = ImGui::GetIO();
        if (c > 0 && c < 0x10000)
            io.AddInputCharacter(c);
        });

  }

  void Application::Shutdown() {
    for (auto &layer : m_LayerStack) layer->OnDetach();

    m_LayerStack.clear();

    // Cleanup

    glfwDestroyWindow(m_WindowHandle);
    glfwTerminate();

    g_ApplicationRunning = false;
  }
  static bool showDemoWindow = true;
  void        Application::Run() {
    m_Running = true;

    // Main loop
    while (!glfwWindowShouldClose(m_WindowHandle) && m_Running) {
      glfwPollEvents();

      for (auto &layer : m_LayerStack) layer->OnUpdate(m_TimeStep);

      const auto desc = m_pSwapChain->GetDesc();
      m_pImGui->NewFrame(desc.Width, desc.Height, desc.PreTransform);

      if (showDemoWindow) {
        ImGui::ShowDemoWindow(&showDemoWindow);
      }
      {
        auto *pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
        auto *pDSV = m_pSwapChain->GetDepthBufferDSV();
        m_pImmediateContext->SetRenderTargets(
            1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        const float ClearColor[] = {0.350f, 0.350f, 0.350f, 1.0f};
        m_pImmediateContext->ClearRenderTarget(
            pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearDepthStencil(
            pDSV, CLEAR_DEPTH_FLAG, 1.f, 0,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->SetPipelineState(m_pPSO);
        DrawAttribs drawAttrs;
        drawAttrs.NumVertices = 3;  // Render 3 vertices
        m_pImmediateContext->Draw(drawAttrs);
      }

      m_pImGui->Render(m_pImmediateContext);
      m_pSwapChain->Present();

      float time      = GetTime();
      m_FrameTime     = time - m_LastFrameTime;
      m_TimeStep      = glm::min<float>(m_FrameTime, 0.0333f);
      m_LastFrameTime = time;
    }
  }

  void Application::Close() { m_Running = false; }

  float Application::GetTime() { return (float)glfwGetTime(); }
}  // namespace Sera
