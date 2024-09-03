//imgui version >= 1.90.7
#pragma once
#include <GLFW/glfw3.h>
#include <stdio.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// #include "implot.h"//TODO ImPlot
// #include <cstdlib>
//  #include <functional>
//  #include <iostream>

static void ErrorCallback(int error, const char* description) {
  fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

template <typename Derived>  // CRTP
class UiBase {
 public:
  UiBase() {
    glfwSetErrorCallback(ErrorCallback);

    if (!glfwInit()) std::exit(1);  // TODO throw exception or put it in Init()

      // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);  // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+
    // only glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // 3.0+ only
#endif

    // Create _window with graphics context
    _window = glfwCreateWindow(1280, 720, "Default Ui", nullptr, nullptr);
    if (_window == nullptr)
      std::exit(1);  // TODO throw exception or put it in Init()
    // glfwSetWindowSize(_window, 1920, 1080);
    glfwMakeContextCurrent(_window);
    glfwSwapInterval(1);  // Enable vsync

    // Add _window based callbacks to the underlying app
    glfwSetMouseButtonCallback(_window, &Derived::MouseButtonCallback);
    glfwSetCursorPosCallback(_window, &Derived::CursorPosCallback);
    glfwSetKeyCallback(_window, &Derived::KeyCallback);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    // ImPlot::CreateContext();//TODO ImPlot
    _io = ImGui::GetIO();
    (void)_io;
    _io.ConfigFlags |=
        ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    // _io.ConfigFlags |=
    //     ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    /// docking version
    _io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;  // Enable Docking
    _io.ConfigFlags |=
        ImGuiConfigFlags_ViewportsEnable;  // Enable Multi-Viewport / Platform
                                           // Windows
    // io.ConfigViewportsNoAutoMerge = true;
    // io.ConfigViewportsNoTaskBarIcon = true;
    /// docking version

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();

    /// custom
    // ImVec4* colors = ImGui::GetStyle().Colors;
    // colors[ImGuiCol_Text] =
    //     ImVec4(252.f / 255.f, 224.f / 255.f, 176.f / 255.f, 1.f);
    // colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    // colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    // colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    // colors[ImGuiCol_PopupBg] = ImVec4(0.19f, 0.19f, 0.19f, 0.92f);
    // colors[ImGuiCol_Border] = ImVec4(0.19f, 0.19f, 0.19f, 0.29f);
    // colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.24f);
    // colors[ImGuiCol_FrameBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
    // colors[ImGuiCol_FrameBgHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
    // colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
    // colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    // colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    // colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    // colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    // colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
    // colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
    // colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f,
    // 0.54f); colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.56f, 0.56f,
    // 0.56f, 0.54f); colors[ImGuiCol_CheckMark] = ImVec4(0.33f, 0.67f,
    // 0.86f, 1.00f); colors[ImGuiCol_SliderGrab] = ImVec4(0.34f, 0.34f, 0.34f,
    // 0.54f); colors[ImGuiCol_SliderGrabActive] = ImVec4(0.56f, 0.56f, 0.56f,
    // 0.54f); colors[ImGuiCol_Button] = ImVec4(0.25f, 0.25f, 0.25f, 0.54f);
    // colors[ImGuiCol_ButtonHovered] = ImVec4(0.3f, 0.3f, 0.3f, 0.54f);
    // colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
    // colors[ImGuiCol_Header] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    // colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.00f, 0.00f, 0.36f);
    // colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.22f, 0.23f, 0.33f);
    // colors[ImGuiCol_Separator] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
    // colors[ImGuiCol_SeparatorHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
    // colors[ImGuiCol_SeparatorActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
    // colors[ImGuiCol_ResizeGrip] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
    // colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
    // colors[ImGuiCol_ResizeGripActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
    // colors[ImGuiCol_Tab] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    // colors[ImGuiCol_TabHovered] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    // colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 0.36f);
    // colors[ImGuiCol_TabUnfocused] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    // colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    // colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    // colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    // colors[ImGuiCol_PlotHistogram] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    // colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.00f,
    // 0.00f, 1.00f); colors[ImGuiCol_TableHeaderBg] = ImVec4(0.00f, 0.00f,
    // 0.00f, 0.52f); colors[ImGuiCol_TableBorderStrong] = ImVec4(0.00f, 0.00f,
    // 0.00f, 0.52f); colors[ImGuiCol_TableBorderLight] = ImVec4(0.28f, 0.28f,
    // 0.28f, 0.29f); colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f,
    // 0.00f); colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f,
    // 0.06f); colors[ImGuiCol_TextSelectedBg] = ImVec4(0.20f, 0.22f,
    // 0.23f, 1.00f); colors[ImGuiCol_DragDropTarget] = ImVec4(0.33f, 0.67f,
    // 0.86f, 1.00f); colors[ImGuiCol_NavHighlight] = ImVec4(1.00f, 0.00f,
    // 0.00f, 1.00f); colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f,
    // 0.00f, 0.00f, 0.70f); colors[ImGuiCol_NavWindowingDimBg] = ImVec4(1.00f,
    // 0.00f, 0.00f, 0.20f); colors[ImGuiCol_ModalWindowDimBg] = ImVec4(1.00f,
    // 0.00f, 0.00f, 0.35f);

    // ImGuiStyle& style = ImGui::GetStyle();
    // style.WindowPadding = ImVec2(8.00f, 8.00f);
    // style.FramePadding = ImVec2(5.00f, 2.00f);
    // style.CellPadding = ImVec2(6.00f, 6.00f);
    // style.ItemSpacing = ImVec2(6.00f, 6.00f);
    // style.ItemInnerSpacing = ImVec2(6.00f, 6.00f);
    // style.TouchExtraPadding = ImVec2(0.00f, 0.00f);
    // style.IndentSpacing = 25;
    // style.ScrollbarSize = 15;
    // style.GrabMinSize = 10;
    // style.WindowBorderSize = 1;
    // style.ChildBorderSize = 1;
    // style.PopupBorderSize = 1;
    // style.FrameBorderSize = 1;
    // style.TabBorderSize = 1;
    // style.WindowRounding = 7;
    // style.ChildRounding = 4;
    // style.FrameRounding = 3;
    // style.PopupRounding = 4;
    // style.ScrollbarRounding = 9;
    // style.GrabRounding = 3;
    // style.LogSliderDeadzone = 4;
    // style.TabRounding = 4;
    /// custom

    /// docking version
    // When viewports are enabled we tweak WindowRounding/WindowBg so platform
    // windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (_io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
      style.WindowRounding = 0.0f;
      style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
    /// docking version

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(_window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Add custom fonts
    // ImGuiIO& _io = ImGui::GetIO();
    // _io.Fonts->AddFontFromFileTTF("../../../imgui/misc/fonts/Roboto-Medium.ttf",
    // 18.0f);
  }

  virtual ~UiBase() {
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    // ImPlot::DestroyContext();//TODO ImPlot
    ImGui::DestroyContext();

    glfwDestroyWindow(_window);
    glfwTerminate();
  }

  void Run() {
    // Initialize the underlying app
    StartUp();

    while (!glfwWindowShouldClose(_window)) {
      // Poll and handle events (inputs, window resize, etc.)
      // You can read the _io.WantCaptureMouse, _io.WantCaptureKeyboard flags to
      // tell if dear imgui wants to use your inputs.
      // - When _io.WantCaptureMouse is true, do not dispatch mouse input data
      // to your main application, or clear/overwrite your copy of the mouse
      // data.
      // - When _io.WantCaptureKeyboard is true, do not dispatch keyboard input
      // data to your main application, or clear/overwrite your copy of the
      // keyboard data. Generally you may always pass all inputs to dear imgui,
      // and hide them from your application based on those two flags.
      glfwPollEvents();

      // Start the Dear ImGui frame
      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();

      // Main loop of the underlying app
      Update();

      // Rendering
      ImGui::Render();
      int display_w, display_h;
      glfwGetFramebufferSize(_window, &display_w, &display_h);
      glViewport(0, 0, display_w, display_h);
      glClearColor(_clear_color.x * _clear_color.w,
                   _clear_color.y * _clear_color.w,
                   _clear_color.z * _clear_color.w, _clear_color.w);
      glClear(GL_COLOR_BUFFER_BIT);
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

      /// docking version
      // Update and Render additional Platform Windows
      // (Platform functions may change the current OpenGL context, so we
      // save/restore it to make it easier to paste this code elsewhere.
      //  For this specific demo app we could also call
      //  glfwMakeContextCurrent(window) directly)
      if (_io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
      }
      /// docking version

      glfwSwapBuffers(_window);
    }
  }

  void Update() { static_cast<Derived*>(this)->Update(); }

  void StartUp() { static_cast<Derived*>(this)->StartUp(); }

  // virtual void Update() {}  // this function is called repeatly . virtual
  //                           // function lead to more overhead

  // virtual void StartUp() {}

 private:
  GLFWwindow* _window = nullptr;
  ImVec4 _clear_color = ImVec4(0.1058, 0.1137f, 0.1255f, 1.00f);

 protected:
  ImGuiIO _io;
};
