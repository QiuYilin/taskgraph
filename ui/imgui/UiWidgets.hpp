#pragma once

#include "Theme.hpp"
#include "ThemeStyle.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

// 控件高度按字号的比例表达；宽度仍可用有效像素固定。
inline constexpr float kStandardControlHeightRatio =
    static_cast<float>(kStandardControlHeightEpx) /
    static_cast<float>(kBodyFontSizeEpx);
inline constexpr float kOutputControlHeightRatio =
    static_cast<float>(kOutputToolbarHeightEpx) /
    static_cast<float>(kBodyFontSizeEpx);
inline constexpr float kCheckboxSizeRatio =
    static_cast<float>(kOutputCheckboxSizeEpx) /
    static_cast<float>(kBodyFontSizeEpx);

// 统一复选框：勾线按方框边长的比例取点，边框与勾线用有效像素。
inline constexpr float kOutputCheckboxLabelGapEpx = 4.0f;
inline constexpr float kOutputCheckboxCheckStartX = 0.22f;
inline constexpr float kOutputCheckboxCheckStartY = 0.52f;
inline constexpr float kOutputCheckboxCheckMidX = 0.42f;
inline constexpr float kOutputCheckboxCheckMidY = 0.72f;
inline constexpr float kOutputCheckboxCheckEndX = 0.78f;
inline constexpr float kOutputCheckboxCheckEndY = 0.28f;
inline constexpr float kOutputCheckboxBorderEpx = 1.0f;
inline constexpr float kOutputCheckboxLineThicknessEpx = 2.0f;
// 浮动工具窗口：离视口边留两倍边距，默认位置居中。
inline constexpr float kViewWindowMarginCount = 2.0f;
inline constexpr float kViewWindowCenterRatio = 0.5f;
// VS Code 按钮（.monaco-button）：2epx 圆角、1epx 描边。
inline constexpr float kButtonCornerRadiusEpx = 2.0f;
inline constexpr float kButtonBorderSizeEpx = 1.0f;
// 下拉框 chevron：以框高中心为原点的小折线。
inline constexpr float kThemedComboChevronHalfEpx = 3.0f;
inline constexpr float kThemedComboChevronThicknessEpx = 1.5f;

struct WidgetMetrics {
  float height_ratio = kStandardControlHeightRatio;
  float width_epx = 0.0f;
  bool fill_width = false;
};

// Push/Pop 成对绑定，析构时必定 Pop，避免中途返回漏掉颜色。
class ScopedStyle {
 public:
  ScopedStyle(void (*push)(), void (*pop)()) : _pop(pop) { push(); }
  ~ScopedStyle() { _pop(); }
  ScopedStyle(const ScopedStyle&) = delete;
  ScopedStyle& operator=(const ScopedStyle&) = delete;

 private:
  void (*_pop)();
};

// 文字动作：标题栏、菜单、Output 工具栏和 Monitor 标题动作共用。
inline bool TextAction(const char* label, const WidgetMetrics& metrics,
                       bool active = false, const char* tooltip = nullptr) {
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  const float width =
      metrics.width_epx > 0.0f
          ? metrics.width_epx * ui_scale
          : ImGui::CalcTextSize(label).x +
                ImGui::GetStyle().FramePadding.x * kFramePaddingSideCount;
  const bool pressed =
      ImGui::Selectable(label, active, ImGuiSelectableFlags_None,
                        ImVec2(width, 0.0f));
  // 固定宽度会截断文字，悬停补完整内容。
  if (tooltip != nullptr && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tooltip);
  }
  return pressed;
}

// 算出按钮宽度，供同行扣宽和右对齐复用。
inline float ButtonWidth(const char* label) {
  return ImGui::CalcTextSize(label).x +
         ImGui::GetStyle().FramePadding.x * kFramePaddingSideCount;
}

enum class ButtonKind : unsigned char { Secondary, Primary };

// VS Code 按钮：secondary 用于 Browse / Cancel，primary 用于对话框确认。
// width_epx 为 0 时按文字宽；height_ratio 为 0 时用当前 FrameHeight，和同排输入框对齐。
inline bool Button(const char* label, ButtonKind kind = ButtonKind::Secondary,
                   float width_epx = 0.0f, float height_ratio = 0.0f) {
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  const theme_detail::ThemeTokens& tokens = theme_detail::CurrentTokens();
  const bool primary = kind == ButtonKind::Primary;
  const Color& background =
      primary ? tokens.button_primary_background : tokens.button_background;
  const Color& hover = primary ? tokens.button_primary_hover_background
                               : tokens.button_hover_background;
  const Color& foreground =
      primary ? tokens.button_primary_foreground : tokens.button_foreground;
  ImGui::PushStyleColor(ImGuiCol_Button, ToImVec4(background));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToImVec4(hover));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ToImVec4(hover));
  ImGui::PushStyleColor(ImGuiCol_Text, ToImVec4(foreground));
  ImGui::PushStyleColor(ImGuiCol_Border, ToImVec4(tokens.button_border));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,
                      kButtonCornerRadiusEpx * ui_scale);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,
                      kButtonBorderSizeEpx * ui_scale);
  bool pressed = false;
  // 没给高度比例就用当前行高，避免把属性表行撑高。
  if (height_ratio > 0.0f) {
    pressed = ImGui::Button(
        label, ImVec2(width_epx * ui_scale,
                      height_ratio * kBodyFontSizeEpx * ui_scale));
  } else if (width_epx > 0.0f) {
    pressed = ImGui::Button(label, ImVec2(width_epx * ui_scale, 0.0f));
  } else {
    pressed = ImGui::Button(label);
  }
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(5);
  return pressed;
}

// 统一紧凑复选框：Output、Properties bool、Categories popup 共用。
inline bool Checkbox(const char* id, const char* label, bool* value,
                     const WidgetMetrics& metrics) {
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  const float box_size = metrics.height_ratio * kBodyFontSizeEpx * ui_scale;
  const float label_gap = kOutputCheckboxLabelGapEpx * ui_scale;
  const bool has_label = label[0] != '\0';
  const float text_width = has_label ? ImGui::CalcTextSize(label).x : 0.0f;
  const float gap = has_label ? label_gap : 0.0f;
  const float text_height = ImGui::GetTextLineHeight();
  const float row_height = (std::max)(box_size, ImGui::GetFrameHeight());
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const bool pressed = ImGui::InvisibleButton(
      id, ImVec2(box_size + gap + text_width, row_height));
  if (pressed) {
    *value = !*value;
  }
  const bool hovered = ImGui::IsItemHovered();
  const auto& tokens = theme_detail::CurrentTokens();
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImVec2 box_min(pos.x, pos.y + (row_height - box_size) * 0.5f);
  const ImVec2 box_max(box_min.x + box_size, box_min.y + box_size);
  const ImU32 background = ImGui::GetColorU32(ToImVec4(
      hovered ? tokens.input_active_background : tokens.input_background));
  const ImU32 border = ImGui::GetColorU32(ToImVec4(tokens.input_border));
  const ImGuiStyle& style = ImGui::GetStyle();
  draw->AddRectFilled(box_min, box_max, background, style.FrameRounding);
  draw->AddRect(box_min, box_max, border, style.FrameRounding, 0,
                kControlCornerRadiusEpx * ui_scale);
  if (*value) {
    const ImU32 mark = ImGui::GetColorU32(ToImVec4(tokens.focus_border));
    const ImVec2 start(box_min.x + box_size * kOutputCheckboxCheckStartX,
                       box_min.y + box_size * kOutputCheckboxCheckStartY);
    const ImVec2 middle(box_min.x + box_size * kOutputCheckboxCheckMidX,
                        box_min.y + box_size * kOutputCheckboxCheckMidY);
    const ImVec2 end(box_min.x + box_size * kOutputCheckboxCheckEndX,
                     box_min.y + box_size * kOutputCheckboxCheckEndY);
    const float thickness = kOutputCheckboxLineThicknessEpx * ui_scale;
    draw->AddLine(start, middle, mark, thickness);
    draw->AddLine(middle, end, mark, thickness);
  }
  const ImVec2 text_pos(pos.x + box_size + gap,
                        pos.y + (row_height - text_height) * 0.5f);
  draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), text_pos,
                ImGui::GetColorU32(ImGuiCol_Text), label);
  return pressed;
}

// 全工作台统一下拉框：去掉 ImGui 默认灰色箭头块，只画小 chevron。
struct ThemedComboState {
  bool open = false;
  ImVec2 min;
  float width = 0.0f;
  float height = 0.0f;
};

inline void DrawThemedComboChevron(const ThemedComboState& state,
                                   float ui_scale) {
  const float half = kThemedComboChevronHalfEpx * ui_scale;
  const float center_x = state.min.x + state.width - state.height * 0.5f;
  const float center_y = state.min.y + state.height * 0.5f;
  const float direction = state.open ? -1.0f : 1.0f;
  const ImU32 color = ImGui::GetColorU32(
      ToImVec4(theme_detail::CurrentTokens().editor_foreground));
  ImDrawList* draw = ImGui::GetForegroundDrawList();
  const ImVec2 left(center_x - half, center_y - half * direction);
  const ImVec2 middle(center_x, center_y + half * direction);
  const ImVec2 right(center_x + half, center_y - half * direction);
  const float thickness = kThemedComboChevronThicknessEpx * ui_scale;
  draw->AddLine(left, middle, color, thickness);
  draw->AddLine(middle, right, color, thickness);
}

inline ThemedComboState BeginThemedCombo(const char* id,
                                         const char* preview) {
  ThemedComboState state;
  state.min = ImGui::GetCursorScreenPos();
  state.width = ImGui::CalcItemWidth();
  state.height = ImGui::GetFrameHeight();
  state.open = ImGui::BeginCombo(id, preview, ImGuiComboFlags_NoArrowButton);
  return state;
}

inline void EndThemedCombo(const ThemedComboState& state) {
  if (state.open) {
    ImGui::EndCombo();
  }
  DrawThemedComboChevron(state, ImGui::GetStyle().FontScaleMain);
}

// 属性字符串变长时，把输入缓冲扩到这份字符串上。
inline int ResizePropertyText(ImGuiInputTextCallbackData* data) {
  // 只处理变长请求，别的输入事件这里不改字符串。
  if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
    auto* text = static_cast<std::string*>(data->UserData);
    text->resize(static_cast<std::size_t>(data->BufTextLen));
    data->Buf = text->data();
  }
  return 0;
}

// 过滤输入：统一 buffer 扩展、focus 和高度比例。
struct FilterInputState {
  std::string* buffer = nullptr;
  bool* focus_needed = nullptr;
  std::size_t min_capacity = 64;
};

inline bool FilterInput(const char* id, const char* hint,
                        FilterInputState state, const WidgetMetrics& metrics) {
  if (state.buffer == nullptr) {
    return false;
  }
  if (state.buffer->capacity() < state.min_capacity) {
    state.buffer->reserve(state.min_capacity);
  }
  const std::size_t buffer_size = state.buffer->capacity() + 1;
  const ImGuiStyle& style = ImGui::GetStyle();
  const float ui_scale = style.FontScaleMain;
  const float target_height =
      metrics.height_ratio * kBodyFontSizeEpx * ui_scale;
  const float padding_y =
      (std::max)((target_height - ImGui::GetFontSize()) / kFramePaddingSideCount,
                 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                      ImVec2(style.FramePadding.x, padding_y));
  if (metrics.fill_width) {
    ImGui::SetNextItemWidth(-FLT_MIN);
  } else if (metrics.width_epx > 0.0f) {
    ImGui::SetNextItemWidth(metrics.width_epx * ui_scale);
  }
  if (state.focus_needed != nullptr && *state.focus_needed) {
    ImGui::SetKeyboardFocusHere();
    *state.focus_needed = false;
  }
  const bool changed = ImGui::InputTextWithHint(
      id, hint, state.buffer->data(), buffer_size,
      ImGuiInputTextFlags_CallbackResize, ResizePropertyText, state.buffer);
  ImGui::PopStyleVar();
  return changed;
}

struct QuickPickSpec {
  const char* popup_id = nullptr;
  const char* filter_hint = nullptr;
  const char* empty_label = nullptr;
  float min_width_epx = 0.0f;
  std::string* filter = nullptr;
  bool* focus_needed = nullptr;
  std::function<bool()> draw_rows;
};

inline void ShowQuickPick(const QuickPickSpec& spec) {
  if (spec.popup_id == nullptr || !ImGui::IsPopupOpen(spec.popup_id)) {
    return;
  }
  ScopedStyle style(PushQuickPickStyle, PopQuickPickStyle);
  ImGui::SetNextWindowSizeConstraints(
      ImVec2(spec.min_width_epx * ImGui::GetStyle().FontScaleMain, 0.0f),
      ImVec2(FLT_MAX, FLT_MAX));
  if (!ImGui::BeginPopup(spec.popup_id)) {
    return;
  }
  FilterInput("##QuickPickFilter", spec.filter_hint,
              FilterInputState{spec.filter, spec.focus_needed, 64},
              WidgetMetrics{kStandardControlHeightRatio, 0.0f, true});
  ImGui::Separator();
  const bool has_rows = spec.draw_rows ? spec.draw_rows() : false;
  if (!has_rows && spec.empty_label != nullptr) {
    ImGui::TextDisabled("%s", spec.empty_label);
  }
  ImGui::EndPopup();
}

struct ToolWindowSpec {
  float default_width_epx = 0.0f;
  float default_height_epx = 0.0f;
  float margin_epx = 0.0f;
  ImGuiWindowFlags extra_flags = 0;
  bool float_first_use = true;
  std::optional<ImVec2> override_pos;
  std::optional<ImVec2> override_size;
};

inline bool BeginToolWindow(const char* name, bool* open,
                            const ToolWindowSpec& spec) {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  const float margin = spec.margin_epx * ui_scale;
  const float width =
      (std::min)(spec.default_width_epx * ui_scale,
                 (std::max)(viewport->WorkSize.x - margin * kViewWindowMarginCount,
                            1.0f));
  const float height =
      (std::min)(spec.default_height_epx * ui_scale,
                 (std::max)(viewport->WorkSize.y - margin * kViewWindowMarginCount,
                            1.0f));
  if (spec.float_first_use) {
    ImGui::SetNextWindowDockID(0, ImGuiCond_FirstUseEver);
  }
  if (spec.override_pos.has_value()) {
    ImGui::SetNextWindowPos(*spec.override_pos, ImGuiCond_Always);
  } else {
    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x +
                   (viewport->WorkSize.x - width) * kViewWindowCenterRatio,
               viewport->WorkPos.y +
                   (viewport->WorkSize.y - height) * kViewWindowCenterRatio),
        ImGuiCond_FirstUseEver);
  }
  if (spec.override_size.has_value()) {
    ImGui::SetNextWindowSize(*spec.override_size, ImGuiCond_Always);
  } else {
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_FirstUseEver);
  }
  return ImGui::Begin(name, open, spec.extra_flags);
}

struct WindowAction {
  const char* label = nullptr;
  bool active = false;
  bool enabled = true;
  float width_epx = 0.0f;
  const char* tooltip = nullptr;
  std::function<void()> on_press;
};

// 浮动窗口标题栏右侧动作；停靠窗口由调用方在内容首行复用 TextAction。
inline void DrawWindowTitleActions(const char* id, const ImVec2& window_pos,
                                   const ImVec2& window_size,
                                   float titlebar_height,
                                   std::vector<WindowAction> actions,
                                   float ui_scale) {
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const float close_width = titlebar_height;
  float actions_width = 0.0f;
  for (WindowAction& action : actions) {
    const float item_width =
        action.width_epx > 0.0f
            ? action.width_epx * ui_scale
            : ImGui::CalcTextSize(action.label).x +
                  ImGui::GetStyle().FramePadding.x * kFramePaddingSideCount;
    action.width_epx = item_width / ui_scale;
    actions_width += item_width + spacing;
  }
  const float total_width = actions_width - spacing;
  ImGui::SetNextWindowPos(
      ImVec2(window_pos.x + window_size.x - close_width - total_width,
             window_pos.y));
  ImGui::SetNextWindowSize(ImVec2(total_width, titlebar_height));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  const std::string overlay_name = std::string("##WindowActions_") + id;
  ImGui::Begin(overlay_name.c_str(), nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoScrollWithMouse |
                   ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNav |
                   ImGuiWindowFlags_NoFocusOnAppearing |
                   ImGuiWindowFlags_NoBackground);
  for (WindowAction& action : actions) {
    ImGui::BeginDisabled(!action.enabled);
    if (TextAction(action.label,
                   WidgetMetrics{kOutputControlHeightRatio, action.width_epx,
                                 false},
                   action.active, action.tooltip)) {
      if (action.on_press) {
        action.on_press();
      }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
  }
  ImGui::End();
  ImGui::PopStyleVar(4);
}
