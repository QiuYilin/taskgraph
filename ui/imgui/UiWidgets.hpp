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
inline constexpr float kFloatingWindowMarginCount = 2.0f;
inline constexpr float kFloatingWindowCenterRatio = 0.5f;
inline constexpr float kFloatingWindowDefaultWidthEpx = 720.0f;
inline constexpr float kFloatingWindowDefaultHeightEpx = 540.0f;
inline constexpr float kFloatingWindowMarginEpx = 24.0f;
// 按钮：2epx 圆角、1epx 描边。
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

inline constexpr WidgetMetrics kCheckboxMetrics{kCheckboxSizeRatio, 0.0f,
                                                false};

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

// 文字动作：标题栏、菜单、Output/Monitor 工具栏共用；占满当前 FrameHeight 并垂直居中。
inline bool TextAction(const char* label, const WidgetMetrics& metrics,
                       bool active = false, const char* tooltip = nullptr) {
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  const bool fixed_width = metrics.width_epx > 0.0f;
  const float width =
      fixed_width
          ? metrics.width_epx * ui_scale
          : ImGui::CalcTextSize(label).x +
                ImGui::GetStyle().FramePadding.x * kFramePaddingSideCount;
  const float height = ImGui::GetFrameHeight();
  ImGui::PushStyleColor(
      ImGuiCol_Button,
      active ? ImGui::GetStyleColorVec4(ImGuiCol_Header)
             : ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                        ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,
                      ImGui::GetStyle().SelectableRounding);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign,
                      ImVec2(fixed_width ? 0.0f : 0.5f, 0.5f));
  const bool pressed = ImGui::Button(label, ImVec2(width, height));
  ImGui::PopStyleVar(3);
  ImGui::PopStyleColor(3);
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

// 按钮：secondary 用于 Browse / Cancel，primary 用于对话框确认。
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
inline float CheckboxWidth(const char* label, const WidgetMetrics& metrics) {
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  const float box_size = metrics.height_ratio * kBodyFontSizeEpx * ui_scale;
  const bool has_label = label[0] != '\0';
  const float text_width = has_label ? ImGui::CalcTextSize(label).x : 0.0f;
  const float gap = has_label ? kOutputCheckboxLabelGapEpx * ui_scale : 0.0f;
  return box_size + gap + text_width;
}

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
      id, ImVec2(CheckboxWidth(label, metrics), row_height));
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

// 工具栏降级：可伸缩项先缩短，可紧凑项再收标签，最后低优先级项进 “...” 菜单。
inline constexpr const char* kToolbarOverflowLabel = "...";
inline constexpr const char* kToolbarOverflowTooltip = "More";

enum class ToolbarItemMode { Full, Truncate, Compact };

struct ToolbarItemSpec {
  ToolbarItemMode mode = ToolbarItemMode::Full;
  float full_width = 0.0f;
  float compact_width = 0.0f;
  float min_width = 0.0f;
  float max_width = 0.0f;
  int priority = 0;
  bool can_overflow = true;
};

struct ToolbarItemLayout {
  bool visible = true;
  bool compact = false;
  bool overflow = false;
  float width = 0.0f;
};

struct ToolbarLayout {
  std::vector<ToolbarItemLayout> items;
  bool show_overflow = false;
  float overflow_width = 0.0f;
};

inline float ToolbarOverflowWidth() {
  return ImGui::CalcTextSize(kToolbarOverflowLabel).x +
         ImGui::GetStyle().FramePadding.x * kFramePaddingSideCount;
}

inline ToolbarLayout LayoutToolbarRow(
    float available_width, float gap,
    const std::vector<ToolbarItemSpec>& specs) {
  ToolbarLayout layout;
  layout.items.resize(specs.size());
  layout.overflow_width = ToolbarOverflowWidth();
  for (std::size_t i = 0; i < specs.size(); ++i) {
    layout.items[i].width =
        specs[i].mode == ToolbarItemMode::Truncate ? specs[i].max_width
                                                   : specs[i].full_width;
  }
  auto total_width = [&]() {
    float total = 0.0f;
    std::size_t count = 0;
    for (const ToolbarItemLayout& item : layout.items) {
      if (!item.visible) {
        continue;
      }
      total += item.width;
      ++count;
    }
    if (count > 1) {
      total += gap * static_cast<float>(count - 1);
    }
    if (layout.show_overflow) {
      total += layout.overflow_width + (count > 0 ? gap : 0.0f);
    }
    return total;
  };
  available_width = (std::max)(available_width, 0.0f);

  // 1) 可伸缩项按最低优先级缩到 min。
  while (total_width() > available_width) {
    std::size_t best = specs.size();
    for (std::size_t i = 0; i < specs.size(); ++i) {
      const ToolbarItemLayout& item = layout.items[i];
      if (!item.visible || specs[i].mode != ToolbarItemMode::Truncate ||
          item.width <= specs[i].min_width) {
        continue;
      }
      if (best == specs.size() || specs[i].priority < specs[best].priority) {
        best = i;
      }
    }
    if (best == specs.size()) {
      break;
    }
    const float deficit = total_width() - available_width;
    const float next =
        (std::max)(specs[best].min_width, layout.items[best].width - deficit);
    if (next >= layout.items[best].width) {
      break;
    }
    layout.items[best].width = next;
  }

  // 2) 可紧凑项按最低优先级去掉标签。
  while (total_width() > available_width) {
    std::size_t best = specs.size();
    for (std::size_t i = 0; i < specs.size(); ++i) {
      const ToolbarItemLayout& item = layout.items[i];
      if (!item.visible || item.compact ||
          specs[i].mode != ToolbarItemMode::Compact ||
          specs[i].compact_width >= item.width) {
        continue;
      }
      if (best == specs.size() || specs[i].priority < specs[best].priority) {
        best = i;
      }
    }
    if (best == specs.size()) {
      break;
    }
    layout.items[best].compact = true;
    layout.items[best].width = specs[best].compact_width;
  }

  // 3) 低优先级项收进 “...”；按钮比项还宽时第一条不划算，跳过。
  while (total_width() > available_width) {
    std::size_t best = specs.size();
    for (std::size_t i = 0; i < specs.size(); ++i) {
      const ToolbarItemLayout& item = layout.items[i];
      if (!item.visible || !specs[i].can_overflow) {
        continue;
      }
      if (!layout.show_overflow &&
          item.width <= layout.overflow_width) {
        continue;
      }
      if (best == specs.size() || specs[i].priority < specs[best].priority) {
        best = i;
      }
    }
    if (best == specs.size()) {
      break;
    }
    layout.items[best].visible = false;
    layout.items[best].overflow = true;
    layout.show_overflow = true;
  }

  // 4) 极端窄时允许可伸缩项低于 min，避免负宽和重叠。
  while (total_width() > available_width) {
    std::size_t best = specs.size();
    for (std::size_t i = 0; i < specs.size(); ++i) {
      const ToolbarItemLayout& item = layout.items[i];
      if (!item.visible || specs[i].mode != ToolbarItemMode::Truncate ||
          item.width <= 1.0f) {
        continue;
      }
      if (best == specs.size() || specs[i].priority < specs[best].priority) {
        best = i;
      }
    }
    if (best == specs.size()) {
      break;
    }
    const float deficit = total_width() - available_width;
    const float next = (std::max)(1.0f, layout.items[best].width - deficit);
    if (next >= layout.items[best].width) {
      break;
    }
    layout.items[best].width = next;
  }
  return layout;
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
inline constexpr std::size_t kFilterMinCapacity = 64;
inline constexpr std::size_t kTextTerminatorSize = 1;

struct FilterInputState {
  std::string* buffer = nullptr;
  bool* focus_needed = nullptr;
  std::size_t min_capacity = kFilterMinCapacity;
};

inline bool FilterInput(const char* id, const char* hint,
                        FilterInputState state, const WidgetMetrics& metrics) {
  if (state.buffer == nullptr) {
    return false;
  }
  if (state.buffer->capacity() < state.min_capacity) {
    state.buffer->reserve(state.min_capacity);
  }
  const std::size_t buffer_size =
      state.buffer->capacity() + kTextTerminatorSize;
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

