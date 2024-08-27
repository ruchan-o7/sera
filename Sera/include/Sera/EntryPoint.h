#pragma once
#include "Log.h"

extern Sera::Application* Sera::CreateApplication(int argc, char** argv);
bool                      g_ApplicationRunning = true;

namespace Sera {

  int Main(int argc, char** argv) {
    try {
      Sera::Log::Init();
      while (g_ApplicationRunning) {
        Sera::Application* app = Sera::CreateApplication(argc, argv);
        app->Run();
        delete app;
      }

      return 0;

    } catch (const std::exception& e) {
      SR_CORE_ERROR("Some error happend");
      SR_CORE_ERROR("{}", e.what());
      return 1;
    }
  }

}  // namespace Sera

int main(int argc, char** argv) { return Sera::Main(argc, argv); }
