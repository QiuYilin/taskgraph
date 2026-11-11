#pragma once
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// VS Code 默认 editor.fontSize。
inline constexpr float kBodyFontSizeEpx = 14.0f;
inline constexpr float kControlCornerRadiusEpx = 4.0f;
inline constexpr float kOverlayCornerRadiusEpx = 8.0f;
inline constexpr float kSurfaceTextMarginEpx = 16.0f;
inline constexpr float kBetweenControlsEpx = 8.0f;
inline constexpr float kControlLabelSpacingEpx = 12.0f;
inline constexpr float kStandardControlHeightEpx = 32.0f;
inline constexpr float kFramePaddingSideCount = 2.0f;
// https://github.com/microsoft/vscode/blob/main/src/vs/workbench/browser/parts/statusbar/statusbarPart.ts
// StatusbarPart.HEIGHT
inline constexpr float kStatusBarHeightEpx = 22.0f;
// https://github.com/microsoft/vscode/blob/main/src/vs/workbench/browser/parts/titlebar/titlebarPart.ts
// 命令中心和窗口控件叠加都没开时，标题栏高度是 30。
inline constexpr float kTitleBarHeightEpx = 30.0f;
// https://github.com/microsoft/vscode/blob/main/src/vs/workbench/browser/parts/editor/editor.ts
// 默认编辑器页签高度（window.density.editorTabHeight = default）。
inline constexpr float kEditorTabHeightEpx = 35.0f;
inline constexpr float kEditorTabHorizontalPaddingEpx = 10.0f;
inline constexpr float kEditorTabCloseSizeEpx = 16.0f;
inline constexpr float kEditorTabLabelCloseGapEpx = 6.0f;
inline constexpr float kEditorTabCloseGlyphHalfEpx = 3.5f;
inline constexpr float kEditorTabSeparatorThicknessEpx = 1.0f;
// Explorer 就地重命名：贴紧文字，细边框，对照 VS Code 侧栏改名框。
inline constexpr float kExplorerRenameFramePadXEpx = 4.0f;
inline constexpr float kExplorerRenameFramePadYEpx = 1.0f;
inline constexpr float kExplorerRenameRoundingEpx = 0.0f;
inline constexpr float kExplorerRenameBorderSizeEpx = 1.0f;
// https://github.com/microsoft/vscode/blob/main/src/vs/workbench/browser/parts/titlebar/media/titlebarpart.css
// Windows 标题栏里每个窗口按钮的宽度。
inline constexpr float kWindowCaptionButtonWidthEpx = 46.0f;
inline constexpr int kWindowCaptionButtonCount = 3;
// 无边框窗口拖边缘时，边缘感应宽度。
inline constexpr float kUndecoratedResizeBorderEpx = 6.0f;
inline constexpr int kMinClientWidth = 640;
inline constexpr int kMinClientHeight = 480;
inline constexpr int kRestoredClientWidth = 1280;
inline constexpr int kRestoredClientHeight = 720;
inline constexpr const char* kWindowTitle = "Default Ui";

inline constexpr const char* kSegoeUiVariablePath = "C:\\Windows\\Fonts\\SegUIVar.ttf";
inline constexpr const char* kSegoeUiPath = "C:\\Windows\\Fonts\\segoeui.ttf";

// https://code.visualstudio.com/api/references/theme-color
// VS Code Dark Modern 色板（按 0–255 通道写成 ImVec4）。
inline constexpr float kRgbChannelMax = 255.0f;

// 把 0–255 的 RGB 转成 ImGui 用的 0–1 颜色。
static ImVec4 Rgb255(unsigned int red, unsigned int green, unsigned int blue,
                     float alpha = 1.0f) {
  return ImVec4(static_cast<float>(red) / kRgbChannelMax,
                static_cast<float>(green) / kRgbChannelMax,
                static_cast<float>(blue) / kRgbChannelMax, alpha);
}

// editor.background
inline const ImVec4 kVsCodeEditorBackground = Rgb255(31, 31, 31);
// sideBar.background / editorGroupHeader.tabsBackground / panel.background
inline const ImVec4 kVsCodeSidebarBackground = Rgb255(24, 24, 24);
// tab.inactiveBackground（与页签栏同色）
inline const ImVec4 kVsCodeTabInactiveBackground = kVsCodeSidebarBackground;
// tab.activeBackground（与编辑区同色）
inline const ImVec4 kVsCodeTabActiveBackground = kVsCodeEditorBackground;
// list.hoverBackground
inline const ImVec4 kVsCodeListHoverBackground = Rgb255(42, 45, 46);
// panel.border 低对比描边
inline const ImVec4 kVsCodePanelBorder = Rgb255(255, 255, 255, 0.09f);
// focusBorder / 控件强调
inline const ImVec4 kVsCodeFocusBorder = Rgb255(0, 120, 212);
// input.background
inline const ImVec4 kVsCodeInputBackground = Rgb255(60, 60, 60);
// 画布网格线：略亮于编辑区底
inline const ImVec4 kVsCodeEditorGrid = Rgb255(255, 255, 255, 0.06f);
// foreground
inline const ImVec4 kVsCodeForeground = Rgb255(204, 204, 204);

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

// 按现有控件的圆角、间距和高度设置样式。数值是有效像素，调用方再按缩放放大。
static void ApplyWindowsMetrics(ImGuiStyle& style) {
  style.WindowPadding = ImVec2(kSurfaceTextMarginEpx, kSurfaceTextMarginEpx);
  style.FramePadding = ImVec2(
      kControlLabelSpacingEpx,
      (kStandardControlHeightEpx - kBodyFontSizeEpx) / kFramePaddingSideCount);
  style.ItemSpacing = ImVec2(kBetweenControlsEpx, kBetweenControlsEpx);
  style.ItemInnerSpacing = ImVec2(kBetweenControlsEpx, kBetweenControlsEpx);
  style.CellPadding = ImVec2(kBetweenControlsEpx, kBetweenControlsEpx);
  style.FrameRounding = kControlCornerRadiusEpx;
  style.GrabRounding = kControlCornerRadiusEpx;
  style.ScrollbarRounding = kControlCornerRadiusEpx;
  style.TabRounding = kControlCornerRadiusEpx;
  style.PopupRounding = kOverlayCornerRadiusEpx;
}

// 在 StyleColorsDark 之上覆盖 VS Code Dark Modern 色板。
static void ApplyVsCodeDarkModernColors(ImGuiStyle& style) {
  ImVec4* colors = style.Colors;
  colors[ImGuiCol_Text] = kVsCodeForeground;
  colors[ImGuiCol_TextDisabled] = Rgb255(128, 128, 128);
  colors[ImGuiCol_WindowBg] = kVsCodeEditorBackground;
  colors[ImGuiCol_ChildBg] = kVsCodeEditorBackground;
  colors[ImGuiCol_PopupBg] = kVsCodeSidebarBackground;
  colors[ImGuiCol_Border] = kVsCodePanelBorder;
  colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
  colors[ImGuiCol_FrameBg] = kVsCodeInputBackground;
  colors[ImGuiCol_FrameBgHovered] = kVsCodeListHoverBackground;
  colors[ImGuiCol_FrameBgActive] = Rgb255(55, 55, 55);
  colors[ImGuiCol_TitleBg] = kVsCodeSidebarBackground;
  colors[ImGuiCol_TitleBgActive] = kVsCodeSidebarBackground;
  colors[ImGuiCol_TitleBgCollapsed] = kVsCodeSidebarBackground;
  colors[ImGuiCol_MenuBarBg] = kVsCodeSidebarBackground;
  colors[ImGuiCol_ScrollbarBg] = kVsCodeEditorBackground;
  colors[ImGuiCol_ScrollbarGrab] = Rgb255(70, 70, 70);
  colors[ImGuiCol_ScrollbarGrabHovered] = Rgb255(90, 90, 90);
  colors[ImGuiCol_ScrollbarGrabActive] = Rgb255(110, 110, 110);
  colors[ImGuiCol_CheckMark] = kVsCodeFocusBorder;
  colors[ImGuiCol_SliderGrab] = kVsCodeFocusBorder;
  colors[ImGuiCol_SliderGrabActive] = Rgb255(0, 150, 255);
  colors[ImGuiCol_Button] = Rgb255(60, 60, 60);
  colors[ImGuiCol_ButtonHovered] = kVsCodeListHoverBackground;
  colors[ImGuiCol_ButtonActive] = Rgb255(55, 55, 55);
  colors[ImGuiCol_Header] = Rgb255(60, 60, 60, 0.5f);
  colors[ImGuiCol_HeaderHovered] = kVsCodeListHoverBackground;
  colors[ImGuiCol_HeaderActive] = Rgb255(55, 55, 55);
  colors[ImGuiCol_Separator] = kVsCodePanelBorder;
  colors[ImGuiCol_SeparatorHovered] = kVsCodeFocusBorder;
  colors[ImGuiCol_SeparatorActive] = kVsCodeFocusBorder;
  colors[ImGuiCol_ResizeGrip] = Rgb255(255, 255, 255, 0.2f);
  colors[ImGuiCol_ResizeGripHovered] = kVsCodeFocusBorder;
  colors[ImGuiCol_ResizeGripActive] = kVsCodeFocusBorder;
  colors[ImGuiCol_Tab] = kVsCodeTabInactiveBackground;
  colors[ImGuiCol_TabHovered] = kVsCodeListHoverBackground;
  colors[ImGuiCol_TabSelected] = kVsCodeTabActiveBackground;
  colors[ImGuiCol_TabSelectedOverline] = kVsCodeFocusBorder;
  colors[ImGuiCol_TabDimmed] = kVsCodeTabInactiveBackground;
  colors[ImGuiCol_TabDimmedSelected] = kVsCodeTabActiveBackground;
  colors[ImGuiCol_DockingPreview] = Rgb255(0, 120, 212, 0.4f);
  colors[ImGuiCol_DockingEmptyBg] = kVsCodeSidebarBackground;
  colors[ImGuiCol_TextSelectedBg] = Rgb255(0, 120, 212, 0.35f);
  colors[ImGuiCol_NavHighlight] = kVsCodeFocusBorder;
}

inline constexpr int kExplorerRenameStyleVarCount = 3;
inline constexpr int kExplorerRenameStyleColorCount = 4;

// 主侧栏就地重命名：压成贴行高的扁框，对照 VS Code Explorer 改名。
static void PushExplorerRenameStyle() {
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  ImGui::PushStyleVar(
      ImGuiStyleVar_FramePadding,
      ImVec2(kExplorerRenameFramePadXEpx * ui_scale,
             kExplorerRenameFramePadYEpx * ui_scale));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,
                      kExplorerRenameRoundingEpx);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,
                      kExplorerRenameBorderSizeEpx * ui_scale);
  ImGui::PushStyleColor(ImGuiCol_FrameBg, kVsCodeInputBackground);
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, kVsCodeInputBackground);
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive, kVsCodeInputBackground);
  ImGui::PushStyleColor(ImGuiCol_Border, kVsCodeFocusBorder);
}

static void PopExplorerRenameStyle() {
  ImGui::PopStyleColor(kExplorerRenameStyleColorCount);
  ImGui::PopStyleVar(kExplorerRenameStyleVarCount);
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
    ApplyWindowsMetrics(style);
    ApplyVsCodeDarkModernColors(style);
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
  ImVec4 _clear_color = kVsCodeEditorBackground;
  bool _covering_work_area = false;
  int _restored_x = 0;
  int _restored_y = 0;
  int _restored_w = 0;
  int _restored_h = 0;

 protected:
  ImGuiIO _io;
};
