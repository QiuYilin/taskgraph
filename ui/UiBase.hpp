#pragma once
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>

#include "Theme.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// 无边框窗口拖边缘时，边缘感应宽度。
inline constexpr float kUndecoratedResizeBorderEpx = 6.0f;
inline constexpr int kMinClientWidth = 640;
inline constexpr int kMinClientHeight = 480;
inline constexpr int kRestoredClientWidth = 1280;
inline constexpr int kRestoredClientHeight = 720;
inline constexpr const char* kWindowTitle = "Default Ui";

inline constexpr const char* kSegoeUiVariablePath =
    "C:\\Windows\\Fonts\\SegUIVar.ttf";
inline constexpr const char* kSegoeUiPath = "C:\\Windows\\Fonts\\segoeui.ttf";

// GLFW 出错时把错误码和说明打印到标准错误。
static void ErrorCallback(int error, const char* description) {
  std::fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

// 判断这个字体文件能不能打开。
static bool FontFileExists(const char* path) {
  std::FILE* file = std::fopen(path, "rb");
  // 打不开就没有这份字体。
  if (file == nullptr) {
    return false;
  }
  std::fclose(file);
  return true;
}

// 装入 Windows 正文用的 Segoe 字体。两种都不在时保持 ImGui 默认字体。
static void LoadBodyFont(ImGuiIO& io) {
  const char* chosen = nullptr;
  // 系统里有 Segoe UI Variable 就用它做正文。
  if (FontFileExists(kSegoeUiVariablePath)) {
    chosen = kSegoeUiVariablePath;
  }
  // 没有可变字体时，改用普通 Segoe UI。
  if (chosen == nullptr && FontFileExists(kSegoeUiPath)) {
    chosen = kSegoeUiPath;
  }
  // 两种字体都不在，保持 ImGui 默认字体。
  if (chosen == nullptr) {
    return;
  }
  ImFont* font = io.Fonts->AddFontFromFileTTF(chosen, kBodyFontSizeEpx);
  // 字体文件打得开但装不进图集时，仍用默认字体。
  if (font != nullptr) {
    io.FontDefault = font;
  }
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
    int work_w = kRestoredClientWidth;
    int work_h = kRestoredClientHeight;
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

    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    const int restored_w = kRestoredClientWidth;
    const int restored_h = kRestoredClientHeight;
    _window =
        glfwCreateWindow(restored_w, restored_h, kWindowTitle, nullptr, nullptr);
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
    _restored_x = content_x;
    _restored_y = content_y;
    _restored_w = restored_w;
    _restored_h = restored_h;
    CoverWorkArea();
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

    LoadBodyFont(io);

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    ApplyTheme(style);
    _clear_color = style.Colors[ImGuiCol_WindowBg];
    // 开了多视口时，窗口圆角和外框背景要去掉，否则弹出窗口带黑边。
    // 窗口最大化或面板边缘相接时，圆角也是 0。
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

 protected:
  GLFWwindow* Window() const { return _window; }

  bool CoveringWorkArea() const { return _covering_work_area; }

  // 运行中切主题：只换颜色和清除色，不重新缩放 metrics / 布局。
  void ApplyWorkbenchTheme(const Theme& theme) {
    ImGuiStyle& style = ImGui::GetStyle();
    theme_detail::SetActiveTheme(theme);

    theme_detail::ApplyThemeColors(style, theme.tokens);
    _clear_color = theme.tokens.editor_background;
  }

  void CoverWorkArea() {
    // 已经铺满工作区就不用再记一次还原位置。
    if (_covering_work_area) {
      return;
    }
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    // 没有主显示器就保持当前大小。
    if (monitor == nullptr) {
      return;
    }
    int area_x = 0;
    int area_y = 0;
    int area_w = 0;
    int area_h = 0;
    glfwGetMonitorWorkarea(monitor, &area_x, &area_y, &area_w, &area_h);
    // 工作区无效时不铺满，避免把窗口收成 0。
    if (area_w <= 0 || area_h <= 0) {
      return;
    }
    glfwGetWindowPos(_window, &_restored_x, &_restored_y);
    glfwGetWindowSize(_window, &_restored_w, &_restored_h);
    glfwSetWindowPos(_window, area_x, area_y);
    glfwSetWindowSize(_window, area_w, area_h);
    _covering_work_area = true;
  }

  void RestoreFramedWindow() {
    // 没铺满就不用还原。
    if (!_covering_work_area) {
      return;
    }
    glfwSetWindowPos(_window, _restored_x, _restored_y);
    glfwSetWindowSize(_window, _restored_w, _restored_h);
    _covering_work_area = false;
  }

  void ToggleWorkAreaCover() {
    // 铺满和还原来回切，对应标题栏上的最大化按钮。
    if (_covering_work_area) {
      RestoreFramedWindow();
    } else {
      CoverWorkArea();
    }
  }

 private:
  GLFWwindow* _window = nullptr;
  ImVec4 _clear_color{};
  bool _covering_work_area = false;
  int _restored_x = 0;
  int _restored_y = 0;
  int _restored_w = 0;
  int _restored_h = 0;

 protected:
  ImGuiIO _io;
};
