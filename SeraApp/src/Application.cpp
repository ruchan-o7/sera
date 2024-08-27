#include "Sera/Application.h"
#include "Sera/EntryPoint.h"

class ExampleLayer : public Sera::Layer {
  public:
    virtual void OnUIRender() override {
      ImGui::Begin("Hello");
      ImGui::Button("Button");
      ImGui::End();

      ImGui::ShowDemoWindow();
    }
};

Sera::Application *Sera::CreateApplication(int argc, char **argv) {
  Sera::ApplicationSpecification spec;
  spec.Name = "Sera Example";

  Sera::Application *app = new Sera::Application(spec);
  app->PushLayer<ExampleLayer>();
  app->SetMenubarCallback([&]() {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Exit")) {
        app->Close();
      }
      if (ImGui::MenuItem("Log Terminal")) {
        SR_INFO("Here is arguments");
        for (int i = 0; i < argc; i++) SR_INFO("{0}: {1}", i, argv[i]);
      }
      ImGui::EndMenu();
    }
  });
  return app;
}
