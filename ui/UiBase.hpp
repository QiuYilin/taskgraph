#pragma once
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// GLFW 出错时把错误码和说明打印到标准错误。
static void ErrorCallback(int error, const char* description) {
  std::fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

template <typename Derived>
class UiBase {
 public:
  UiBase() {
    glfwSetErrorCallback(ErrorCallback);

    // GLFW 初始化失败就没法建窗口，直接退出。
    if (!glfwInit())
      std::exit(1);

#if defined(IMGUI_IMPL_OPENGL_ES2)
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    int work_x = 0;
    int work_y = 0;
    int work_w = 1280;
    int work_h = 720;
    // 有主显示器就按它的可用工作区放窗口，没有就退回默认的 1280x720。
    if (GLFWmonitor* monitor = glfwGetPrimaryMonitor()) {
      int area_x = 0;
      int area_y = 0;
      int area_w = 0;
      int area_h = 0;
      glfwGetMonitorWorkarea(monitor, &area_x, &area_y, &area_w, &area_h);
      // 工作区宽高要有效才采用。
      if (area_w > 0 && area_h > 0) {
        work_x = area_x;
        work_y = area_y;
        work_w = area_w;
        work_h = area_h;
      }
    }

    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    const int restored_w = 1280;
    const int restored_h = 720;
    _window =
        glfwCreateWindow(restored_w, restored_h, "Default Ui", nullptr, nullptr);
    // 窗口建不出来，后续所有 GL 调用都没意义，直接退出。
    if (_window == nullptr) {
      std::exit(1);
    }

    int frame_left = 0;
    int frame_top = 0;
    int frame_right = 0;
    int frame_bottom = 0;
    glfwGetWindowFrameSize(_window, &frame_left, &frame_top, &frame_right,
                           &frame_bottom);
    const int outer_w = restored_w + frame_left + frame_right;
    const int outer_h = restored_h + frame_top + frame_bottom;
    int content_x = work_x + frame_left + (work_w - outer_w) / 2;
    int content_y = work_y + frame_top + (work_h - outer_h) / 2;
    const int min_x = work_x + frame_left;
    const int min_y = work_y + frame_top;
    const int max_x = work_x + work_w - frame_right - restored_w;
    const int max_y = work_y + work_h - frame_bottom - restored_h;
    // 把内容区居中到工作区里，再夹到工作区边界内，避免窗口跑到屏幕外。
    if (content_x < min_x || max_x < min_x) {
      content_x = min_x;
    } else if (content_x > max_x) {
      content_x = max_x;
    }
    if (content_y < min_y || max_y < min_y) {
      content_y = min_y;
    } else if (content_y > max_y) {
      content_y = max_y;
    }
    glfwSetWindowPos(_window, content_x, content_y);
    glfwMaximizeWindow(_window);
    glfwMakeContextCurrent(_window);
    glfwSwapInterval(1);

    float content_scale_x = 1.0f;
    float content_scale_y = 1.0f;
    glfwGetWindowContentScale(_window, &content_scale_x, &content_scale_y);
    float content_scale =
        content_scale_x > content_scale_y ? content_scale_x : content_scale_y;
    // 缩放比低于 1 时按 1 算，界面不会被缩小。
    if (content_scale < 1.0f) {
      content_scale = 1.0f;
    }

    glfwSetMouseButtonCallback(_window, &Derived::MouseButtonCallback);
    glfwSetCursorPosCallback(_window, &Derived::CursorPosCallback);
    glfwSetKeyCallback(_window, &Derived::KeyCallback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigDpiScaleViewports = true;
    _io = io;

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    // 开了多视口时，窗口圆角和外框背景要去掉，否则弹出窗口带黑边。
    if (_io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
      style.WindowRounding = 0.0f;
      style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
    style.ScaleAllSizes(content_scale);
    style.FontScaleMain = content_scale;

    ImGui_ImplGlfw_InitForOpenGL(_window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
  }

  virtual ~UiBase() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(_window);
    glfwTerminate();
  }

  UiBase(const UiBase&) = delete;
  UiBase& operator=(const UiBase&) = delete;
  UiBase(UiBase&&) = delete;
  UiBase& operator=(UiBase&&) = delete;

  void Run() {
    StartUp();

    // 主循环：窗口没被要求关闭就一直画下一帧。
    while (!glfwWindowShouldClose(_window)) {
      glfwPollEvents();

      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();

      Update();

      ImGui::Render();
      int display_w{}, display_h{};
      glfwGetFramebufferSize(_window, &display_w, &display_h);
      glViewport(0, 0, display_w, display_h);
      glClearColor(_clear_color.x * _clear_color.w,
                   _clear_color.y * _clear_color.w,
                   _clear_color.z * _clear_color.w, _clear_color.w);
      glClear(GL_COLOR_BUFFER_BIT);
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

      // 开了多视口时，平台窗口要在各自上下文里画完再切回来。
      if (_io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
      }

      glfwSwapBuffers(_window);
    }
  }

  void Update() { static_cast<Derived*>(this)->Update(); }

  void StartUp() { static_cast<Derived*>(this)->StartUp(); }

 private:
  GLFWwindow* _window = nullptr;
  ImVec4 _clear_color = ImVec4(0.1058f, 0.1137f, 0.1255f, 1.00f);

 protected:
  ImGuiIO _io;
};
