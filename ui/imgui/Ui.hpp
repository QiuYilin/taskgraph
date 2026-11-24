#include "CanvasViewLock.hpp"
#include "WorkbenchClipboard.hpp"
#include "ExplorerProjectActions.hpp"
#include "LinkVariable.hpp"
#include "LogView.hpp"
#include "NativePathDialog.hpp"
#include "PortTypeVisual.hpp"
#include "ThemeStyle.hpp"
#include "UiBase.hpp"
#include "UiSession.hpp"
#include "UiWidgets.hpp"
#include "Undo.hpp"
#include "WorkbenchEdits.hpp"
#include "WorkbenchFocus.hpp"
#include "WorkbenchParts.hpp"
#include "WorkbenchIcons.hpp"
#include "imgui_internal.h"
#include <imgui-node-editor/imgui_node_editor.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "taskgraph/taskgraph.hpp"

constexpr float kProjectPathInputWidthEpx = 420.0f;
constexpr float kPortTypeSelectorPaddingEpx = 3.0f;
constexpr float kPortTypeSelectorRoundingEpx = 3.0f;
constexpr float kPortTypeChevronWidthEpx = 10.0f;
constexpr float kPortTypeChevronHalfEpx = 2.5f;
constexpr float kPortTypeChevronArmEpx = 3.0f;
constexpr int kPortTypeMenuColumns = 3;
constexpr float kOpenTypeMenuNodeZ = 1.0f;
constexpr const char* kUntitledProjectLabel = "Untitled";
constexpr const char* kWelcomeTabLabel = "Welcome";
constexpr const char* kKeyboardShortcutsTabLabel = "Keyboard Shortcuts";
constexpr const char* kShortcutFilterHint = "Type to search in keybindings";
constexpr const char* kShortcutEmptyLabel = "No matching keybindings";
constexpr const char* kSearchSideBarTitle = "SEARCH";
constexpr const char* kSearchFilterHint = "Search";
constexpr const char* kSearchEmptyLabel = "No matching results";
constexpr const char* kSearchHitSeparator = " / ";
constexpr int kShortcutColumnCount = 3;
constexpr float kShortcutCommandColumnWeight = 2.0f;
constexpr float kShortcutKeyColumnWeight = 1.0f;
constexpr float kShortcutWhenColumnWeight = 2.0f;
constexpr const char* kOutputWindowName = "Output###Output View";
constexpr const char* kOutputPanelId = "output";
constexpr float kLogDetailComboWidthEpx = 120.0f;
constexpr float kLogCategoriesPopupWidthEpx = 140.0f;
constexpr float kLogLevelColumnWidthEpx = 56.0f;
constexpr float kLogCategoryColumnWidthEpx = 96.0f;
constexpr float kLogContextColumnWidthEpx = 180.0f;
constexpr std::size_t kLogFilterMinCapacity = 64;
constexpr std::size_t kLogTextTerminatorSize = 1;
constexpr const char* kLogDetailLabel = "Detail";
constexpr const char* kLogCategoriesLabel = "Categories";
constexpr const char* kLogCategoriesPopupId = "##LogCategories";
constexpr const char* kLogClearLabel = "Clear";
constexpr const char* kLogAutoScrollLabel = "Auto-scroll";
constexpr const char* kLogFilterHint = "Filter context or message";
constexpr const char* kLogCustomPresetLabel = "Custom";
constexpr int kDockTitleButtonCount = 2;
constexpr float kDockTitleButtonCenterRatio = 0.5f;
constexpr const char* kOutputMaximizeTooltip = "Maximize Panel Size";
constexpr const char* kOutputRestoreTooltip = "Restore Panel Size";
constexpr const char* kOutputCloseTooltip = "Close Panel";
constexpr const char* kPropertiesMaximizeTooltip =
    "Maximize Secondary Side Bar";
constexpr const char* kPropertiesRestoreTooltip =
    "Restore Secondary Side Bar";
constexpr const char* kPropertiesCloseTooltip = "Close Secondary Side Bar";
constexpr float kViewWindowDefaultWidthEpx = 720.0f;
constexpr float kViewWindowDefaultHeightEpx = 540.0f;
constexpr float kViewWindowMarginEpx = 24.0f;
constexpr float kMonitorSourceActionWidthEpx = 180.0f;
constexpr float kMonitorMinContentHeightEpx = 80.0f;
constexpr const char* kMonitorSourceLabel = "Source";
constexpr const char* kMonitorFollowLabel = "Follow Selection";
constexpr const char* kMonitorSourcePopupId = "##MonitorSourcePicker";
constexpr const char* kMonitorSourceFilterHint = "Filter outputs";
constexpr const char* kMonitorNoSourceMessage = "Run the graph to compute an output.";
constexpr const char* kMonitorNoSelectionMessage = "No node selected.";
constexpr const char* kMonitorSelectedNoMatchMessage =
    "Selected node has no matching output.";
constexpr const char* kMonitorPinnedNoMatchMessage =
    "Pinned node has no matching output.";
constexpr const char* kNoProjectOpenedLabel = "NO PROJECT OPENED";
constexpr const char* kWelcomeStartHeading = "Start";
constexpr const char* kWelcomeRecentHeading = "Recent";
// 弹窗 ID 不能用 "Project"：与 Explorer 的 Nodes###Project 同 ID，会触发 EndPopup 断言。
constexpr const char* kProjectPathDialogId = "Project Path###ProjectPathDialog";
constexpr const char* kBrowseActionLabel = "Browse...";
constexpr const char* kPickProjectDirectoryTitle = "Select Project";
constexpr const char* kPickPropertyFileTitle = "Select File";
constexpr const char* kPickPropertyDirectoryTitle = "Select Folder";
constexpr const char* kPickNodeKindPopupId = "PickNodeKind";
constexpr const char* kPickNodeKindFilterHint = "Type to filter";
constexpr const char* kPickNodeKindEmptyLabel = "No matching kinds";
constexpr std::size_t kPickNodeKindFilterMinCapacity = 64;
// 编辑区工具栏：画布端口标签在功能名与类型名之间切换（英文，避缺字）。
constexpr const char* kShowTypeNamesActionLabel = "Show Type Names";
constexpr float kTypeChipGapEpx = 2.0f;

// 关掉新建停靠格上的标签，窄条才不会被标签盖住。
static void HideBuiltDockTab(ImGuiID node_id) {
  ImGuiDockNode* node = ImGui::DockBuilderGetNode(node_id);
  // 这一格还没建出来。
  if (node == nullptr) {
    return;
  }
  node->SetLocalFlags(node->LocalFlags | ImGuiDockNodeFlags_NoTabBar);
}

// 无标签的格子不能再接收页签，否则窗口会和隐藏标签的 Web 画布叠死。
// 拖到边缘仍可劈出新停靠格。
static void DisallowDockingOver(ImGuiID node_id) {
  ImGuiDockNode* node = ImGui::DockBuilderGetNode(node_id);
  if (node == nullptr) {
    return;
  }
  node->SetLocalFlags(node->LocalFlags | ImGuiDockNodeFlags_NoDockingOverMe);
}

// 算出标题栏或状态栏里一个文字项的宽度，含左右内边距。
static float BarActionWidth(const char* label) {
  return ImGui::CalcTextSize(label).x +
         ImGui::GetStyle().FramePadding.x * kFramePaddingSideCount;
}

// 在标题栏或状态栏里画一个可点的文字项，返回这一帧是否按下。
static bool DrawBarAction(const char* label) {
  return ImGui::Selectable(label, false, ImGuiSelectableFlags_None,
                           ImVec2(BarActionWidth(label), 0.0f));
}

// 统一紧凑复选框：不依赖 ImGui checkbox 的全局 FrameBg 样式。
static bool DrawThemedCheckbox(const char* id, const char* label, bool* value,
                               float ui_scale) {
  const float box_size = kOutputCheckboxSizeEpx * ui_scale;
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
                kOutputCheckboxBorderEpx * ui_scale);
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

// 端口类型下拉的展开状态：记录当前展开的是哪个端口。
struct TypeSelectorPopupState {
  const tg::Node::Port* port = nullptr;
  int property_index = -1;
};

// 画端口类型选择器；返回是否改了属性值。
static bool DrawPortTypeSelector(const tg::Node::Port& port,
                                 int property_index,
                                 TypeSelectorPopupState& popup) {
  const auto& properties = port.node->getProperties();
  if (property_index < 0 ||
      static_cast<std::size_t>(property_index) >= properties.size()) {
    return false;
  }
  const auto& property = properties[static_cast<std::size_t>(property_index)];
  if (property == nullptr ||
      property->hints.editor != tg::PropertyEditor::Enum ||
      property->hints.choices.empty()) {
    return false;
  }
  auto* number = std::get_if<int>(&property->value);
  if (number == nullptr) {
    return false;
  }
  const int choice_count = static_cast<int>(property->hints.choices.size());
  const char* preview =
      (*number >= 0 && *number < choice_count)
          ? property->hints.choices[static_cast<std::size_t>(*number)].c_str()
          : port.data_type.name.c_str();

  const ImGuiStyle& style = ImGui::GetStyle();
  const float ui_scale = style.FontScaleMain;
  const float pad_x = kPortTypeSelectorPaddingEpx * ui_scale;
  const float chevron_width = kPortTypeChevronWidthEpx * ui_scale;
  const ImVec2 text_size = ImGui::CalcTextSize(preview);
  const ImVec2 size(text_size.x + chevron_width + pad_x * 2.0f,
                    ImGui::GetTextLineHeight());
  // 让框内文字与下方的功能名左对齐：框向左让出内边距。
  const ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
  const ImVec2 pos(cursor_pos.x - pad_x, cursor_pos.y);
  ImGui::SetCursorScreenPos(pos);

  bool changed = false;
  // 点空白处关闭；同时把抬高过的节点放回普通层级。
  if (popup.port == &port && ax::NodeEditor::IsBackgroundClicked()) {
    ax::NodeEditor::SetNodeZPosition(ax::NodeEditor::NodeId(port.node), 0.0f);
    popup.port = nullptr;
  }
  const bool is_open = popup.port == &port;
  // 用端口对象地址做 ID：同一节点上输入/输出可能绑同一个类型属性。
  ImGui::PushID(&port);
  const bool pressed = ImGui::InvisibleButton("##port_type", size);
  if (pressed) {
    if (popup.port != nullptr && popup.port != &port) {
      ax::NodeEditor::SetNodeZPosition(
          ax::NodeEditor::NodeId(popup.port->node), 0.0f);
    }
    if (!is_open) {
      popup.port = &port;
      popup.property_index = property_index;
      // 展开的列表属于节点内容，把节点抬到最上层，避免被其它节点遮住。
      ax::NodeEditor::SetNodeZPosition(ax::NodeEditor::NodeId(port.node),
                                       kOpenTypeMenuNodeZ);
    } else {
      ax::NodeEditor::SetNodeZPosition(ax::NodeEditor::NodeId(port.node), 0.0f);
      popup.port = nullptr;
    }
  }

  ImDrawList* draw = ImGui::GetWindowDrawList();
  const theme_detail::ThemeTokens& tokens = theme_detail::CurrentTokens();
  const ImU32 text_color =
      ImGui::GetColorU32(ToImVec4(tokens.editor_type_foreground));
  if (ImGui::IsItemHovered() || is_open) {
    draw->AddRectFilled(pos,
                        ImVec2(pos.x + size.x, pos.y + size.y),
                        ImGui::GetColorU32(ImGuiCol_FrameBgHovered),
                        kPortTypeSelectorRoundingEpx * ui_scale);
  }
  draw->AddText(ImVec2(pos.x + pad_x, pos.y), text_color, preview);
  const ImVec2 chevron_center(pos.x + size.x - pad_x - chevron_width * 0.5f,
                              pos.y + size.y * 0.5f);
  const float half = kPortTypeChevronHalfEpx * ui_scale;
  const float arm = kPortTypeChevronArmEpx * ui_scale;
  draw->AddLine(ImVec2(chevron_center.x - arm, chevron_center.y - half),
                ImVec2(chevron_center.x, chevron_center.y + half), text_color);
  draw->AddLine(ImVec2(chevron_center.x + arm, chevron_center.y - half),
                ImVec2(chevron_center.x, chevron_center.y + half), text_color);

  // 展开时在类型名下面直接画选项行，属于节点内容，没有独立窗口。
  if (is_open) {
    float column_width = size.x;
    for (const auto& choice : property->hints.choices) {
      column_width =
          (std::max)(column_width,
                     ImGui::CalcTextSize(choice.c_str()).x + pad_x * 2.0f);
    }
    for (int choice = 0; choice < choice_count; ++choice) {
      if (ImGui::Selectable(
              property->hints.choices[static_cast<std::size_t>(choice)].c_str(),
              choice == *number, ImGuiSelectableFlags_None,
              ImVec2(column_width, 0.0f))) {
        *number = choice;
        changed = true;
        ax::NodeEditor::SetNodeZPosition(ax::NodeEditor::NodeId(port.node),
                                         0.0f);
        popup.port = nullptr;
      }
      const bool row_end = ((choice + 1) % kPortTypeMenuColumns) == 0;
      if (!row_end && choice + 1 < choice_count) {
        ImGui::SameLine();
      }
    }
  }
  ImGui::PopID();

  // 下一行（功能名）回到组起点，文字才对得上。
  const ImVec2 next_cursor = ImGui::GetCursorScreenPos();
  ImGui::SetCursorScreenPos(ImVec2(cursor_pos.x, next_cursor.y));
  return changed;
}

// 把类型 chip 锚到被悬停文字上方；item_min/max 传屏幕坐标。
static void ShowAnchoredTypeChip(const char* text, const ImVec2& item_min,
                                 const ImVec2& item_max, float ui_scale) {
  const ImGuiStyle& style = ImGui::GetStyle();
  const float gap = kTypeChipGapEpx * ui_scale;
  const float chip_height = ImGui::GetTextLineHeight() +
                            style.WindowPadding.y * 2.0f +
                            style.WindowBorderSize * 2.0f;
  const float viewport_top = ImGui::GetMainViewport()->WorkPos.y;
  // 顶部放不下时翻到文字下方，避免浮层跑出主视口。
  if (item_min.y - gap - chip_height >= viewport_top) {
    ImGui::SetNextWindowPos(ImVec2(item_min.x, item_min.y - gap),
                            ImGuiCond_Always, ImVec2(0.0f, 1.0f));
  } else {
    ImGui::SetNextWindowPos(ImVec2(item_min.x, item_max.y + gap),
                            ImGuiCond_Always, ImVec2(0.0f, 0.0f));
  }
  // BeginTooltip 返回 true 才允许 EndTooltip。
  if (!ImGui::BeginTooltip()) {
    return;
  }
  ImGui::PushStyleColor(
      ImGuiCol_Text,
      ToImVec4(theme_detail::CurrentTokens().editor_type_foreground));
  ImGui::TextUnformatted(text);
  ImGui::PopStyleColor();
  ImGui::EndTooltip();
}

// 端口类型 chip 的悬停信息；等整行布局完成后再画，避免 tooltip 抢走 last item。
struct PortTypeChipRequest {
  const char* text = nullptr;
  ImVec2 min;
  ImVec2 max;
};

// 画布端口旁文字：类型名（或类型下拉）在上、功能名在下。
static bool DrawPortPinLabels(const tg::Node::Port& port, bool show_type_names,
                              float& caption_top,
                              TypeSelectorPopupState& popup,
                              PortTypeChipRequest& chip) {
  const bool has_caption = !port.caption.empty();
  const int selector_index =
      show_type_names && port.node != nullptr
          ? port.node->portTypeProperty(
                port.is_output ? tg::PortType::Out : tg::PortType::In,
                port.index)
          : -1;
  const bool has_type_selector = selector_index >= 0;
  const bool has_type_name = show_type_names && !port.data_type.name.empty();
  if (!has_caption && !has_type_name && !has_type_selector) {
    return false;
  }
  const theme_detail::ThemeTokens& tokens = theme_detail::CurrentTokens();
  bool changed = false;
  ImGui::BeginGroup();
  if (has_type_selector) {
    changed = DrawPortTypeSelector(port, selector_index, popup);
  } else if (has_type_name) {
    ImGui::PushStyleColor(ImGuiCol_Text, ToImVec4(tokens.editor_type_foreground));
    ImGui::TextUnformatted(port.data_type.name.c_str());
    ImGui::PopStyleColor();
  }
  if (has_caption) {
    caption_top = ImGui::GetCursorScreenPos().y;
    ImGui::PushStyleColor(ImGuiCol_Text, ToImVec4(tokens.editor_foreground));
    ImGui::SetNextItemAllowOverlap();
    ImGui::TextUnformatted(port.caption.c_str());
    ImGui::PopStyleColor();
    // 关闭类型名时，悬停功能名才把类型贴到文字上方。
    if (!show_type_names && !port.data_type.name.empty() &&
        port.data_type.name != port.caption && ImGui::IsItemHovered()) {
      chip.text = port.data_type.name.c_str();
      chip.min = ImGui::GetItemRectMin();
      chip.max = ImGui::GetItemRectMax();
    }
  }
  ImGui::EndGroup();
  return changed;
}

// 画一行端口：色形中心和功能名（最后一行）对齐，类型名叠在上方。
static bool DrawPortRow(const tg::Node::Port& port, bool input,
                        bool show_type_names, float ui_scale,
                        TypeSelectorPopupState& popup) {
  const int selector_index =
      show_type_names && port.node != nullptr
          ? port.node->portTypeProperty(
                port.is_output ? tg::PortType::Out : tg::PortType::In,
                port.index)
          : -1;
  const bool has_labels =
      !port.caption.empty() || selector_index >= 0 ||
      (show_type_names && !port.data_type.name.empty());
  const ImVec2 row_min = ImGui::GetCursorScreenPos();
  ImVec2 labels_max = row_min;
  float caption_top = row_min.y;
  bool changed = false;
  PortTypeChipRequest chip;
  const float line_height = ImGui::GetTextLineHeight();
  const float slot_width = kPortPinSlotWidthEpx * ui_scale;
  const float slot_height = kPortPinSlotHeightEpx * ui_scale;
  const float gap = kPortPinCaptionGapEpx * ui_scale;
  if (has_labels) {
    // 输入引脚在左，给色形留出槽位再画文字。
    if (input) {
      ImGui::SetCursorScreenPos(
          ImVec2(row_min.x + slot_width + gap, row_min.y));
    }
    changed = DrawPortPinLabels(port, show_type_names, caption_top, popup,
                                chip);
    labels_max = ImGui::GetItemRectMax();
    if (!input) {
      ImGui::SameLine(0.0f, gap);
    }
  }
  const float pin_y =
      (std::max)(row_min.y,
                 caption_top + (line_height - slot_height) * 0.5f);
  ImGui::SetCursorScreenPos(
      ImVec2(input ? row_min.x : ImGui::GetCursorScreenPos().x, pin_y));
  DrawPortPinGlyph(port.data_type, ui_scale);
  const float row_bottom =
      (std::max)(labels_max.y, pin_y + slot_height);
  ImGui::SetCursorScreenPos(ImVec2(row_min.x, row_bottom));
  // 行布局完成后再画 chip，避免 tooltip 抢走 SameLine/GetItemRectMax 的最后一项。
  if (chip.text != nullptr) {
    // 节点编辑器把画布坐标当作布局坐标，tooltip 要的是屏幕坐标。
    ShowAnchoredTypeChip(chip.text,
                         ax::NodeEditor::CanvasToScreen(chip.min),
                         ax::NodeEditor::CanvasToScreen(chip.max), ui_scale);
  }
  return changed;
}

enum class CaptionGlyph { Minimize, Maximize, Restore, Close };

// Windows 标题栏关闭按钮悬停色 #E81123。
constexpr ImU32 kCloseCaptionHover = IM_COL32(232, 17, 35, 255);
constexpr float kCaptionGlyphHalfEpx = 5.0f;
constexpr float kCaptionRestoreOffsetEpx = 2.0f;

// 画窗口标题栏右侧的一个按钮，按下时返回 true。
static bool DrawCaptionButton(const char* id, CaptionGlyph glyph,
                              const ImVec2& size) {
  const bool pressed = ImGui::InvisibleButton(id, size);
  const ImVec2 min = ImGui::GetItemRectMin();
  const ImVec2 max = ImGui::GetItemRectMax();
  ImDrawList* draw = ImGui::GetWindowDrawList();
  // 悬停时铺底：关闭用系统红色，其余用控件悬停色。
  if (ImGui::IsItemHovered()) {
    const ImU32 hover = glyph == CaptionGlyph::Close
                            ? kCloseCaptionHover
                            : ImGui::GetColorU32(ImGuiCol_ButtonHovered);
    draw->AddRectFilled(min, max, hover);
  }
  constexpr float kHalfSpan = 0.5f;
  const ImU32 glyph_color = ImGui::GetColorU32(ImGuiCol_Text);
  const ImVec2 center((min.x + max.x) * kHalfSpan, (min.y + max.y) * kHalfSpan);
  const float half = kCaptionGlyphHalfEpx * ImGui::GetStyle().FontScaleMain;
  const float offset = kCaptionRestoreOffsetEpx * ImGui::GetStyle().FontScaleMain;
  // 最小化画一条横线。
  if (glyph == CaptionGlyph::Minimize) {
    draw->AddLine(ImVec2(center.x - half, center.y),
                  ImVec2(center.x + half, center.y), glyph_color);
  }
  // 最大化画一个方框。
  if (glyph == CaptionGlyph::Maximize) {
    draw->AddRect(ImVec2(center.x - half, center.y - half),
                  ImVec2(center.x + half, center.y + half), glyph_color);
  }
  // 还原画两个错开的方框。
  if (glyph == CaptionGlyph::Restore) {
    draw->AddRect(ImVec2(center.x - half + offset, center.y - half - offset),
                  ImVec2(center.x + half + offset, center.y + half - offset),
                  glyph_color);
    draw->AddRect(ImVec2(center.x - half - offset, center.y - half + offset),
                  ImVec2(center.x + half - offset, center.y + half + offset),
                  glyph_color);
  }
  // 关闭画一个叉。
  if (glyph == CaptionGlyph::Close) {
    draw->AddLine(ImVec2(center.x - half, center.y - half),
                  ImVec2(center.x + half, center.y + half), glyph_color);
    draw->AddLine(ImVec2(center.x + half, center.y - half),
                  ImVec2(center.x - half, center.y + half), glyph_color);
  }
  return pressed;
}

// 关掉已经停靠的窗口上的标签。
static void HideDockTab(const char* window_name) {
  ImGuiWindow* window = ImGui::FindWindowByName(window_name);
  // 窗口还没出现，或这一格已经没有标签。
  if (window == nullptr || window->DockNode == nullptr ||
      (window->DockNode->LocalFlags & ImGuiDockNodeFlags_NoTabBar) != 0) {
    return;
  }
  window->DockNode->SetLocalFlags(window->DockNode->LocalFlags |
                                  ImGuiDockNodeFlags_NoTabBar);
}

// 显示已停靠窗口的标签：去掉 NoTabBar / HiddenTabBar，并关掉「隐藏标签栏」菜单钮。
static void ShowDockTab(const char* window_name) {
  ImGuiWindow* window = ImGui::FindWindowByName(window_name);
  if (window == nullptr || window->DockNode == nullptr) {
    return;
  }
  ImGuiDockNodeFlags flags = window->DockNode->LocalFlags;
  flags &= ~(ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_HiddenTabBar);
  flags |= ImGuiDockNodeFlags_NoWindowMenuButton;
  window->DockNode->SetLocalFlags(flags);
}

struct DockTitleActionPress {
  bool toggle_maximize = false;
  bool close = false;
};

// 在停靠页签栏右侧画最大化/还原和关闭；只给当前选中的页签画一次。
static DockTitleActionPress DrawDockTitleActions(ImGuiDockNode* node,
                                                 bool maximized,
                                                 const char* max_tooltip,
                                                 const char* restore_tooltip,
                                                 const char* close_tooltip) {
  DockTitleActionPress press;
  // 窗口还没停靠，或当前页签不是这一格选中的页签。
  if (node == nullptr || node->TabBar == nullptr ||
      node->SelectedTabId != ImGui::GetCurrentWindow()->TabId) {
    return press;
  }
  if (!ImGui::DockNodeBeginAmendTabBar(node)) {
    return press;
  }
  const ImGuiStyle& style = ImGui::GetStyle();
  const ImRect bar = node->TabBar->BarRect;
  const float button_size = ImGui::GetFrameHeight();
  const float buttons_width =
      button_size * static_cast<float>(kDockTitleButtonCount) +
      style.ItemSpacing.x * static_cast<float>(kDockTitleButtonCount);
  ImGui::SetCursorScreenPos(
      ImVec2(bar.Max.x - buttons_width,
             bar.Min.y + (bar.GetHeight() - button_size) *
                              kDockTitleButtonCenterRatio));
  press.toggle_maximize = DrawCaptionButton(
      "##dock-max", maximized ? CaptionGlyph::Restore : CaptionGlyph::Maximize,
      ImVec2(button_size, button_size));
  const bool max_hovered = ImGui::IsItemHovered();
  ImGui::SameLine();
  press.close = DrawCaptionButton("##dock-close", CaptionGlyph::Close,
                                  ImVec2(button_size, button_size));
  const bool close_hovered = ImGui::IsItemHovered();
  ImGui::DockNodeEndAmendTabBar();
  if (max_hovered) {
    ImGui::SetTooltip("%s", maximized ? restore_tooltip : max_tooltip);
  }
  if (close_hovered) {
    ImGui::SetTooltip("%s", close_tooltip);
  }
  return press;
}

// 窗口在其父拆分里的实际比例；拆分轴不符或没有兄弟时返回 0。
static float DockSplitRatioOfWindow(const char* window_name, ImGuiAxis axis) {
  const ImGuiWindow* window = ImGui::FindWindowByName(window_name);
  // 窗口还没出现，或还没停靠在水平/垂直拆分里。
  if (window == nullptr || window->DockNode == nullptr ||
      window->DockNode->ParentNode == nullptr ||
      window->DockNode->ParentNode->SplitAxis != axis) {
    return 0.0f;
  }
  const ImGuiDockNode* parent = window->DockNode->ParentNode;
  const ImGuiDockNode* sibling = parent->ChildNodes[0] == window->DockNode
                                     ? parent->ChildNodes[1]
                                     : parent->ChildNodes[0];
  // 没有兄弟节点就量不到比例。
  if (sibling == nullptr) {
    return 0.0f;
  }
  return DockSplitRatioFromSizeRef(window->DockNode->SizeRef[axis],
                                   sibling->SizeRef[axis]);
}

enum class ExplorerRowAction { None, Toggle, Activate };

// 画 Explorer 整宽行底与缩进参考线；选中优先于悬停。
static void DrawExplorerRowBackground(ImDrawList* draw, const ImVec2& row_min,
                                      const ImVec2& row_max, int depth,
                                      bool selected, bool hovered,
                                      bool window_focused,
                                      const ExplorerTreePalette& palette) {
  constexpr float kHalfSpan = 0.5f;
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  // 选中底跟随窗口焦点，未聚焦时用 VS Code 的非活动选中色。
  if (selected) {
    draw->AddRectFilled(row_min, row_max,
                        window_focused ? palette.active_selection_bg
                                       : palette.inactive_selection_bg);
  } else if (hovered) {
    draw->AddRectFilled(row_min, row_max, palette.row_hover_bg);
  }
  // 每个带 chevron 的祖先在它的中心画一条参考线；工程名根行没有箭头。
  for (int level = 1; level < depth; ++level) {
    const float guide_x =
        row_min.x + (kExplorerTreeBasePaddingEpx +
                     static_cast<float>(level) * kExplorerTreeIndentEpx +
                     kExplorerTreeChevronBoxEpx * kHalfSpan) * ui_scale;
    draw->AddLine(ImVec2(guide_x, row_min.y), ImVec2(guide_x, row_max.y),
                  palette.indent_guide);
  }
}

// 画 VS Code 风格的细线 chevron：展开朝下，收起朝右。
static void DrawExplorerChevron(ImDrawList* draw, const ImVec2& center,
                                bool open, ImU32 color) {
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  const float arm = kExplorerChevronArmEpx * ui_scale;
  const float depth = kExplorerChevronDepthEpx * ui_scale;
  const float stroke = kExplorerChevronStrokeEpx * ui_scale;
  const ImVec2 first =
      open ? ImVec2(center.x - arm, center.y - depth)
           : ImVec2(center.x - depth, center.y - arm);
  const ImVec2 tip = open ? ImVec2(center.x, center.y + depth)
                          : ImVec2(center.x + depth, center.y);
  const ImVec2 last =
      open ? ImVec2(center.x + arm, center.y - depth)
           : ImVec2(center.x - depth, center.y + arm);
  draw->AddLine(first, tip, color, stroke);
  draw->AddLine(tip, last, color, stroke);
}

// 画 Explorer 行文字；按行右边界裁剪，避免盖住滚动条。
static void DrawExplorerRowText(ImDrawList* draw, const ImVec2& pos,
                                const ImVec2& clip_max, const char* text,
                                ImU32 color) {
  const float font_size =
      kExplorerTreeFontSizeEpx * ImGui::GetStyle().FontScaleMain;
  draw->PushClipRect(pos, clip_max, true);
  draw->AddText(ImGui::GetFont(), font_size, pos, color, text);
  draw->PopClipRect();
}

// 画一条 Explorer 树行，返回左键落点：chevron 切换展开，其余激活。
static ExplorerRowAction ShowExplorerRow(const char* id, const char* label,
                                         int depth, bool selected,
                                         bool has_chevron, bool open) {
  constexpr float kHalfSpan = 0.5f;
  const float ui_scale = ImGui::GetStyle().FontScaleMain;
  const float row_height = kExplorerTreeRowHeightEpx * ui_scale;
  const ImVec2 row_min = ImGui::GetCursorScreenPos();
  const float row_width = ImGui::GetContentRegionAvail().x;
  // 主侧栏被拖到 0 宽时没有可点的行，直接跳过。
  if (row_width <= 0.0f) {
    return ExplorerRowAction::None;
  }
  const ImVec2 row_max(row_min.x + row_width, row_min.y + row_height);
  ImGui::InvisibleButton(id, ImVec2(row_width, row_height));
  const bool hovered = ImGui::IsItemHovered();
  const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
  const bool window_focused = ImGui::IsWindowFocused();
  const ExplorerTreePalette palette = MakeExplorerTreePalette();
  ImDrawList* draw = ImGui::GetWindowDrawList();
  DrawExplorerRowBackground(draw, row_min, row_max, depth, selected, hovered,
                            window_focused, palette);
  const ImU32 text_color =
      selected ? (window_focused ? palette.active_selection_text
                                 : palette.inactive_selection_text)
               : palette.text;
  const float box_left =
      row_min.x + (kExplorerTreeBasePaddingEpx +
                   static_cast<float>(depth) * kExplorerTreeIndentEpx) *
                      ui_scale;
  const float box_width = kExplorerTreeChevronBoxEpx * ui_scale;
  if (has_chevron) {
    DrawExplorerChevron(draw,
                        ImVec2(box_left + box_width * kHalfSpan,
                               row_min.y + row_height * kHalfSpan),
                        open, text_color);
  }
  const float text_x = box_left + box_width;
  const float font_size = kExplorerTreeFontSizeEpx * ui_scale;
  const ImVec2 text_size =
      ImGui::GetFont()->CalcTextSizeA(font_size, FLT_MAX, 0.0f, label);
  const ImVec2 text_pos(text_x,
                        row_min.y + (row_height - text_size.y) * kHalfSpan);
  const ImVec2 clip_max(row_max.x - kExplorerTreeBasePaddingEpx * ui_scale,
                        row_max.y);
  // 行太窄时文字没有可画区域，不 Push 空裁剪框。
  if (clip_max.x > text_pos.x) {
    DrawExplorerRowText(draw, text_pos, clip_max, label, text_color);
  }
  // 左键落在 chevron 框里切换展开，落在行内其余位置激活。
  if (clicked) {
    const float mouse_x = ImGui::GetIO().MousePos.x;
    if (has_chevron && mouse_x >= box_left && mouse_x <= box_left + box_width) {
      return ExplorerRowAction::Toggle;
    }
    return ExplorerRowAction::Activate;
  }
  return ExplorerRowAction::None;
}

struct MonitorSourceEntry {
  std::string label;
  std::shared_ptr<tg::Node> node;
  unsigned int port = 0;
  std::shared_ptr<tg::NodeData> data;
};

struct MonitorWindowState {
  bool follow = true;
  std::weak_ptr<tg::Node> pinned_node;
  unsigned int pinned_port = 0;
  bool maximized = false;
  ImVec2 restore_pos{};
  ImVec2 restore_size{};
  bool restore_next_frame = false;
  std::string source_filter;
  bool focus_source_filter = false;
};

/** \brief 撤销命令落到 ImGui 前端：core 写图，再同步画布的落位与选中。
 *
 *  已画进编辑器的节点直接 SetNodePosition；撤销重加的新节点只写 _node_poses，
 *  下一帧按首帧落位走同一条路径。
 */
class ImGuiUndoTarget : public ui::UndoTarget {
 public:
  tg::Controller* controller = nullptr;
  tg::Graph* graph = nullptr;
  std::unordered_set<tg::Node*>* placed_nodes = nullptr;
  std::unordered_map<tg::Node*, tg::NodePose>* node_poses = nullptr;
  std::vector<tg::Node*>* select_pasted_nodes = nullptr;
  WorkbenchFocus* focus = nullptr;
  ax::NodeEditor::EditorContext* editor = nullptr;

  bool AddNode(const ClipboardNode& snapshot) override {
    // 命令重放时同名节点已经存在，不再加第二个。
    if (FindNode(snapshot.name) != nullptr) {
      return false;
    }
    auto node = controller->createNode(snapshot.type_id);
    if (node == nullptr) {
      return false;
    }
    graph->addNode(node);
    if (!snapshot.name.empty() && node->getName() != snapshot.name) {
      graph->renameNode(node, snapshot.name);
    }
    ApplyClipboardProperties(*node, snapshot.properties);
    // 改成属性会先清默认值，所以露面方式在值之前恢复。
    for (const ClipboardInput& input : snapshot.inputs) {
      graph->setInputPresentation(node, input.index, input.presentation);
      if (!input.has_value) {
        continue;
      }
      if (const auto bound = node->inputProperty(input.index)) {
        bound->value = input.value;
      }
    }
    (*node_poses)[node.get()] = tg::NodePose{snapshot.x, snapshot.y};
    placed_nodes->erase(node.get());
    controller->markDirty();
    return true;
  }

  bool RemoveNode(const std::string& name) override {
    auto node = FindNode(name);
    if (node == nullptr) {
      return false;
    }
    tg::Node* const raw = node.get();
    graph->removeNode(node);
    placed_nodes->erase(raw);
    node_poses->erase(raw);
    select_pasted_nodes->erase(
        std::remove(select_pasted_nodes->begin(), select_pasted_nodes->end(),
                    raw),
        select_pasted_nodes->end());
    focus->ClearSelectionIfNode(raw);
    controller->markDirty();
    return true;
  }

  bool Connect(const ui::LinkSnapshot& link) override {
    auto from = FindNode(link.from);
    auto to = FindNode(link.to);
    if (from == nullptr || to == nullptr) {
      return false;
    }
    if (!graph->connect(from->outputPort(link.port_out),
                        to->inputPort(link.port_in))) {
      return false;
    }
    controller->markDirty();
    return true;
  }

  bool Disconnect(const ui::LinkSnapshot& link) override {
    auto from = FindNode(link.from);
    auto to = FindNode(link.to);
    if (from == nullptr || to == nullptr) {
      return false;
    }
    if (!graph->disconnect(from->outputPort(link.port_out),
                           to->inputPort(link.port_in))) {
      return false;
    }
    controller->markDirty();
    return true;
  }

  bool MoveNode(const std::string& name, tg::NodePose pose) override {
    auto node = FindNode(name);
    if (node == nullptr) {
      return false;
    }
    tg::Node* const raw = node.get();
    (*node_poses)[raw] = pose;
    // 已画进编辑器的节点直接搬；还没画的留给下一帧首帧落位。
    if (placed_nodes->count(raw) != 0 && editor != nullptr) {
      ax::NodeEditor::EditorContext* const previous =
          ax::NodeEditor::GetCurrentEditor();
      ax::NodeEditor::SetCurrentEditor(editor);
      ax::NodeEditor::SetNodePosition(ax::NodeEditor::NodeId(raw),
                                      ImVec2(pose.x, pose.y));
      ax::NodeEditor::SetCurrentEditor(previous);
    }
    controller->markDirty();
    return true;
  }

  bool SetProperty(const std::string& node_name, int property_index,
                   const tg::PropertyValue& value) override {
    auto node = FindNode(node_name);
    if (node == nullptr) {
      return false;
    }
    const auto properties = node->getEditableProperties();
    if (property_index < 0 ||
        property_index >= static_cast<int>(properties.size())) {
      return false;
    }
    properties[static_cast<std::size_t>(property_index)]->value = value;
    graph->refreshPortTypes(node);
    controller->markDirty();
    return true;
  }

  bool RenameNode(const std::string& from, const std::string& to) override {
    auto node = FindNode(from);
    if (node == nullptr) {
      return false;
    }
    if (!graph->renameNode(node, to)) {
      return false;
    }
    controller->markDirty();
    return true;
  }

  bool SetPresentation(const std::string& node_name, unsigned int index,
                       tg::InputPresentation presentation) override {
    auto node = FindNode(node_name);
    if (node == nullptr) {
      return false;
    }
    if (!graph->setInputPresentation(node, index, presentation)) {
      return false;
    }
    controller->markDirty();
    return true;
  }

 private:
  std::shared_ptr<tg::Node> FindNode(const std::string& name) const {
    if (graph == nullptr) {
      return nullptr;
    }
    for (const auto& node : graph->getNodes()) {
      if (node != nullptr && node->getName() == name) {
        return node;
      }
    }
    return nullptr;
  }
};

/** \brief 工程级命令落到 ImGui：图的新建/删除/改名。
 *
 *  图管理涉及关页签、清 _collapsed_graphs/_undo_stacks/选中，放在 Ui 里做；
 *  这里只留回调，避免与 Ui 类的循环依赖。
 */
class ImGuiProjectUndoTarget : public ui::UndoTarget {
 public:
  std::function<bool(const GraphClipboard&)> add_graph;
  std::function<bool(const std::string&)> remove_graph;
  std::function<bool(const std::string&, const std::string&)> rename_graph;

  bool AddNode(const ClipboardNode&) override { return false; }
  bool RemoveNode(const std::string&) override { return false; }
  bool Connect(const ui::LinkSnapshot&) override { return false; }
  bool Disconnect(const ui::LinkSnapshot&) override { return false; }
  bool MoveNode(const std::string&, tg::NodePose) override { return false; }
  bool SetProperty(const std::string&, int,
                   const tg::PropertyValue&) override {
    return false;
  }
  bool RenameNode(const std::string&, const std::string&) override {
    return false;
  }
  bool SetPresentation(const std::string&, unsigned int,
                       tg::InputPresentation) override {
    return false;
  }

  bool AddGraph(const GraphClipboard& graph) override {
    return add_graph != nullptr ? add_graph(graph) : false;
  }
  bool RemoveGraph(const std::string& name) override {
    return remove_graph != nullptr ? remove_graph(name) : false;
  }
  bool RenameGraph(const std::string& from, const std::string& to) override {
    return rename_graph != nullptr ? rename_graph(from, to) : false;
  }
};

/** \brief 属性框激活时记下的旧值；提交时先回旧值再 Push，避免写两次。 */
struct ImGuiPropertyEditState {
  tg::Node* node = nullptr;
  int property_index = 0;
  tg::PropertyValue old_value;
  bool active = false;
};

/** \brief 鼠标按下时的节点坐标；松开后比对，一次拖动只记一条命令。 */
struct ImGuiMoveSnapshot {
  tg::Graph* graph = nullptr;
  bool active = false;

  struct Sample {
    tg::Node* node = nullptr;
    std::string name;
    tg::NodePose before;
  };

  std::vector<Sample> samples;
};

class Ui : public UiBase<Ui> {
 public:
  explicit Ui(tg::Controller& controller) : _controller(controller) {
    RegisterCoreCommands();
    RegisterDefaultPanels();
    RegisterDefaultActivityBar();
    // 工程级撤销目标的图操作落到 Ui 自己的图管理（含关页签、清各缓存）。
    _project_undo_target.add_graph = [this](const GraphClipboard& graph) {
      return AddGraphFromClipboard(graph);
    };
    _project_undo_target.remove_graph = [this](const std::string& name) {
      return RemoveGraphByName(name);
    };
    _project_undo_target.rename_graph =
        [this](const std::string& from, const std::string& to) {
          return RenameGraphByName(from, to);
        };
  }

  // 运行中切到内置 VS Code 主题；立即生效并写入会话。
  void SetTheme(const std::string& id) {
    const Theme* theme = ::FindTheme(id);
    // 未知主题 id 不改当前主题。
    if (theme == nullptr) {
      return;
    }
    ApplyWorkbenchTheme(*theme);
    // 只有选择变了才写会话，避免每帧保存。
    if (_session.theme_id != theme->id) {
      _session.theme_id = theme->id;
      _session.save();
    }
  }

  const std::vector<Theme>& AvailableThemes() const {
    return ::AvailableThemes();
  }

  const std::string& ActiveThemeId() const { return ::CurrentThemeId(); }

  WorkbenchParts& workbench() { return _parts; }

  // 画布/Explorer 当前选中的节点；供外部面板只读跟随。
  tg::Node* SelectedNode() const { return _focus.selected_node(); }

  void AddCommand(Command command) {
    _parts.commands.Set(std::move(command));
  }

  bool ExecuteCommand(std::string_view id) {
    return _parts.commands.Execute(id);
  }

  void AddActivityBarItem(ActivityBarItem item) {
    _parts.activity_bar_items.push_back(std::move(item));
  }

  void SetActivityBarVisible(bool visible) {
    _parts.activity_bar_visible = visible;
    _workbench_layout_dirty = true;
  }

  void SetPrimarySideBarVisible(bool visible) {
    // 可见性没变就不重建停靠，避免每次“显示”都重置分割。
    if (_parts.primary_sidebar_visible == visible) {
      return;
    }
    _parts.primary_sidebar_visible = visible;
    ClearMaximizedView();
    _workbench_layout_dirty = true;
  }

  void SetSecondarySideBarVisible(bool visible) {
    // 可见性没变就不重建停靠。
    if (_parts.secondary_sidebar_visible == visible) {
      return;
    }
    _parts.secondary_sidebar_visible = visible;
    ClearMaximizedView();
    _workbench_layout_dirty = true;
  }

  void SetPanelVisible(bool visible) {
    // 可见性没变就不重建停靠。
    if (_parts.panel_visible == visible) {
      return;
    }
    _parts.panel_visible = visible;
    ClearMaximizedView();
    _workbench_layout_dirty = true;
  }

  // 最大化/还原某个视图：两个视图共用同一块区域。
  void SetMaximizedView(MaximizedView view) {
    if (_maximized_view == view) {
      return;
    }
    _maximized_view = view;
    _workbench_layout_dirty = true;
  }

  // 退出最大化，回到常规停靠。
  void ClearMaximizedView() {
    if (_maximized_view == MaximizedView::None) {
      return;
    }
    _maximized_view = MaximizedView::None;
    _workbench_layout_dirty = true;
  }

  void SetStatusBarVisible(bool visible) {
    _parts.status_bar_visible = visible;
  }

  void AddExplorerTitleAction(ViewAction action) {
    _parts.explorer_title_actions.push_back(std::move(action));
  }

  void AddPanelView(PanelView view) {
    // 没写稳定 ID 时用 id 兜底，避免页签重排后丢停靠。
    if (view.docking_id.empty()) {
      view.docking_id = view.id;
    }
    for (PanelView& existing : _parts.panel_views) {
      // 同 id 覆盖，避免停靠节点里出现两个同名视图。
      if (existing.id == view.id) {
        existing = std::move(view);
        _workbench_layout_dirty = true;
        return;
      }
    }
    _parts.panel_views.push_back(std::move(view));
    _workbench_layout_dirty = true;
  }

  void AddViewWindow(ViewWindow view) {
    if (view.id.empty()) {
      view.id = view.title;
    }
    const auto saved = _session.view_windows.find(view.id);
    view.open = saved != _session.view_windows.end()
                     ? saved->second
                     : view.open_on_first_run;
    const std::string id = view.id;
    AddCommand({view.command_id(), view.title, "View", "", {},
                [this, id] { return IsViewWindowOpen(id); },
                [this, id] { ToggleViewWindow(id); }});
    for (ViewWindow& existing : _parts.view_windows) {
      // 同 id 覆盖，窗口名和命令 id 都保持稳定。
      if (existing.id == view.id) {
        existing = std::move(view);
        return;
      }
    }
    _parts.view_windows.push_back(std::move(view));
  }

  void ToggleViewWindow(std::string_view id) {
    for (ViewWindow& view : _parts.view_windows) {
      if (view.id == id) {
        view.open = !view.open;
        return;
      }
    }
  }

  void AddMonitorView(MonitorView view) {
    if (view.id.empty()) {
      view.id = view.title;
    }
    const auto saved = _session.view_windows.find(view.id);
    view.open = saved != _session.view_windows.end()
                     ? saved->second
                     : view.open_on_first_run;
    const std::string id = view.id;
    AddCommand({view.command_id(), view.title, "View", "", {},
                [this, id] { return IsMonitorOpen(id); },
                [this, id] { ToggleMonitorView(id); }});
    for (MonitorView& existing : _parts.monitor_views) {
      if (existing.id == view.id) {
        existing = std::move(view);
        return;
      }
    }
    _parts.monitor_views.push_back(std::move(view));
  }

  void ToggleMonitorView(std::string_view id) {
    for (MonitorView& view : _parts.monitor_views) {
      if (view.id == id) {
        view.open = !view.open;
        return;
      }
    }
  }

  bool IsMonitorOpen(std::string_view id) const {
    for (const MonitorView& view : _parts.monitor_views) {
      if (view.id == id) {
        return view.open;
      }
    }
    return false;
  }

  bool IsViewWindowOpen(std::string_view id) const {
    for (const ViewWindow& view : _parts.view_windows) {
      if (view.id == id) {
        return view.open;
      }
    }
    return false;
  }

  void SetCommandCenter(CommandCenter center) {
    _parts.command_center = std::move(center);
  }

  void RegisterCoreCommands() {
    AddCommand({"project.new", "New Project...", "File", "", {}, {},
                [this] { BeginProjectPathDialog(ProjectPathAction::New); }});
    AddCommand({"project.open", "Open Project...", "File", "", {}, {},
                [this] { BeginProjectPathDialog(ProjectPathAction::Open); }});
    AddCommand({"project.save", "Save", "File", "",
                [this] { return _controller.hasProjectRoot(); }, {},
                [this] { SaveCurrentProject(); }});
    AddCommand({"project.saveAs", "Save As...", "File", "", {}, {},
                [this] { BeginProjectPathDialog(ProjectPathAction::SaveAs); }});
    AddCommand({"graph.run", "Run Graph", "Run", "",
                [this] { return !_controller.getGraphs().empty(); }, {},
                [this] {
                  const auto& graphs = _controller.getGraphs();
                  // 命令执行与菜单使用同一份选中下标。
                  if (!graphs.empty()) {
                    _controller.tryRun(*graphs[_focus.selected_graph_index()]);
                  }
                }});
    AddCommand({"help.welcome", "Welcome", "Help", "", {}, {},
                [this] { OpenWelcomeInEditor(); }});
    AddCommand({"explorer.newNode", "New Node", "Explorer", "",
                [this] { return !_controller.getGraphs().empty(); }, {},
                [this] {
                  RequestOpenPickNodeKind(_focus.selected_graph_index());
                }});
    AddCommand({"explorer.newGraph", "New Graph", "Explorer", "", {}, {},
                [this] { NewGraphUndoable(); }});
    AddCommand({"editor.showTypeNames", "Show Type Names", "View", "", {},
                [this] { return _show_port_type_names; },
                [this] { _show_port_type_names = !_show_port_type_names; }});
    AddCommand({kShowExplorerCommand, kExplorerActivityTooltip, "View", "", {},
                {}, [this] {
                  _focus.ShowPrimarySideBar(PrimarySideBarView::Explorer);
                  SetPrimarySideBarVisible(true);
                }});
    AddCommand({kShowSearchCommand, kSearchActivityTooltip, "View", "", {}, {},
                [this] {
                  _focus.ShowPrimarySideBar(PrimarySideBarView::Search);
                  SetPrimarySideBarVisible(true);
                }});
    AddCommand({"view.toggleActivityBar", "Activity Bar", "View", "", {},
                [this] { return _parts.activity_bar_visible; },
                [this] {
                  SetActivityBarVisible(!_parts.activity_bar_visible);
                }});
    AddCommand({kTogglePrimarySideBarCommand, "Primary Side Bar", "View", "", {},
                [this] { return _parts.primary_sidebar_visible; },
                [this] {
                  SetPrimarySideBarVisible(!_parts.primary_sidebar_visible);
                }});
    AddCommand({"view.toggleProperties", "Properties", "View", "", {},
                [this] { return _parts.secondary_sidebar_visible; },
                [this] {
                  SetSecondarySideBarVisible(!_parts.secondary_sidebar_visible);
                }});
    AddCommand({"view.toggleOutput", "Output", "View", "", {},
                [this] { return _parts.panel_visible; },
                [this] { SetPanelVisible(!_parts.panel_visible); }});
    AddCommand({"view.toggleStatusBar", "Status Bar", "View", "", {},
                [this] { return _parts.status_bar_visible; },
                [this] { SetStatusBarVisible(!_parts.status_bar_visible); }});
  }

  // 活动栏默认可见。is_active 每帧读主侧栏视图。
  void RegisterDefaultActivityBar() {
    InstallDefaultActivityBar(_parts);
    for (ActivityBarItem& item : _parts.activity_bar_items) {
      if (item.id == kExplorerActivityId) {
        item.is_active = [this] {
          return _focus.primary_side_bar() == PrimarySideBarView::Explorer;
        };
      } else if (item.id == kSearchActivityId) {
        item.is_active = [this] {
          return _focus.primary_side_bar() == PrimarySideBarView::Search;
        };
      }
    }
    _workbench_layout_dirty = true;
  }

  void RegisterDefaultPanels() {
    PanelView output;
    output.id = "output";
    output.title = "Output";
    output.docking_id = "Output View";
    output.order = 0;
    output.draw = [this] { ShowOutputContent(); };
    AddPanelView(std::move(output));
  }

  ~Ui() {
    PersistSession();
    if (_editor != nullptr) {
      ax::NodeEditor::DestroyEditor(_editor);
      _editor = nullptr;
    }
  }

  void StartUp() {
    tg::initLog();
    _session = UiSession::load();
    ApplyLogSessionConfig();
    ApplyViewWindowSessionState();
    // session 里记了主题就恢复；未知 id 保持 UiBase 已应用的默认主题。
    if (!_session.theme_id.empty()) {
      if (const Theme* theme = ::FindTheme(_session.theme_id)) {
        ApplyWorkbenchTheme(*theme);
      }
    }
    TryRestoreOrShowWelcome();
  }

  // 画布尺寸稳定后再创建，避免第一次 Begin 拿停靠前的尺寸把视图原点挪走。
  void EnsureFlowEditor() {
    if (_editor != nullptr) {
      return;
    }
    ax::NodeEditor::Config config;
    config.SettingsFile = nullptr;
    // 尺寸变化时不按高度重算可见宽度。原点仍由 HoldIdentityCanvasView 写回。
    config.CanvasSizeMode = ax::NodeEditor::CanvasSizeMode::CenterOnly;
    // 与 NoteUserCanvasView 里认的平移键一致。
    config.NavigateButtonIndex = ImGuiMouseButton_Right;
    _editor = ax::NodeEditor::CreateEditor(&config);
  }

  // 画一个标题动作；没有图标时退回命令 label 首字符。
  bool DrawViewAction(const ViewAction& action, float size) {
    const bool enabled = _parts.commands.Enabled(action.command);
    const Command* command = _parts.commands.Find(action.command);
    const char* fallback_label =
        command != nullptr ? command->label.c_str() : "?";
    ImGui::PushID(action.command.c_str());
    ImGui::InvisibleButton("##ViewAction", ImVec2(size, size));
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    // 悬停才铺动作底，保持标题行干净。
    if (hovered) {
      draw->AddRectFilled(min, max, ImGui::GetColorU32(ImGuiCol_HeaderHovered));
    }
    const ImU32 color = ImGui::GetColorU32(
        enabled ? ImGuiCol_Text : ImGuiCol_TextDisabled);
    const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    // 有图标就画图标，没有就先画首字符占位。
    const IconPainter action_painter = ImGuiIconPainter(action.icon);
    if (action_painter.draw) {
      action_painter.draw(draw, center, size * 0.7f, color);
    } else if (fallback_label[0] != '\0') {
      const char glyph[2] = {fallback_label[0], '\0'};
      const ImVec2 glyph_size = ImGui::CalcTextSize(glyph);
      draw->AddText(ImVec2(center.x - glyph_size.x * 0.5f,
                           center.y - glyph_size.y * 0.5f),
                    color, glyph);
    }
    // 悬停显示动作说明。
    if (hovered && !action.tooltip.empty()) {
      ImGui::SetTooltip("%s", action.tooltip.c_str());
    }
    ImGui::PopID();
    // 只有可用动作才执行命令。
    return clicked && enabled && _parts.commands.Execute(action.command);
  }

  // 面板工具栏：只有注册动作时才占一行。
  void DrawPanelToolbar(const PanelView& view) {
    if (view.toolbar_actions.empty()) {
      return;
    }
    const float ui_scale = ImGui::GetStyle().FontScaleMain;
    const float size = kViewActionSizeEpx * ui_scale;
    const float gap = 2.0f * ui_scale;
    const float total_width =
        size * static_cast<float>(view.toolbar_actions.size()) +
        gap * static_cast<float>(view.toolbar_actions.size() - 1);
    const float y = ImGui::GetCursorScreenPos().y;
    const float right =
        ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    ImGui::SetCursorScreenPos(ImVec2(right - total_width, y));
    for (const ViewAction& action : view.toolbar_actions) {
      DrawViewAction(action, size);
      ImGui::SameLine(0.0f, gap);
    }
    // 动作行占满高度，避免内容叠在按钮上。
    ImGui::SetCursorScreenPos(
        ImVec2(ImGui::GetCursorScreenPos().x, y + size));
  }

  // 活动栏：固定列，不进停靠。没开启或没有高度时不建窗口。
  void ShowActivityBar(ImVec2 pos, ImVec2 size) {
    if (!_parts.activity_bar_visible || size.x <= 0.0f || size.y <= 0.0f) {
      return;
    }
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGui::SetNextWindowViewport(viewport->ID);
    const theme_detail::ThemeTokens& tokens = theme_detail::CurrentTokens();
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ToImVec4(tokens.activity_bar_background));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ToImVec4(tokens.activity_bar_background));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    // 不自带标题、滚动条和停靠，避免浮回主侧栏上面。
    const ImGuiWindowFlags activity_flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (ImGui::Begin(kActivityBarWindowName, nullptr, activity_flags)) {
      constexpr float kActivityActiveBorderEpx = 2.0f;
      const float ui_scale = ImGui::GetStyle().FontScaleMain;
      const float cell = kActivityBarWidthEpx * ui_scale;
      const float icon = kActivityIconEpx * ui_scale;
      const ImVec2 clip_min = ImGui::GetWindowPos();
      const ImVec2 clip_max(clip_min.x + ImGui::GetWindowSize().x,
                            clip_min.y + ImGui::GetWindowSize().y);
      ImGui::PushClipRect(clip_min, clip_max, true);
      for (const ActivityBarItem& item : _parts.activity_bar_items) {
        ImGui::PushID(item.id.c_str());
        ImGui::InvisibleButton("##ActivityBarItem", ImVec2(cell, cell));
        const bool hovered = ImGui::IsItemHovered();
        const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        const bool active = item.is_active && item.is_active();
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        // 悬停/活动底色用主题 token，活动时左侧画焦点条。
        if (active) {
          draw->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(
                                             ToImVec4(tokens.activity_bar_active_background)));
        } else if (hovered) {
          draw->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(
                                             ToImVec4(tokens.activity_bar_hover_background)));
        }
        if (active) {
          const float bar_width = kActivityActiveBorderEpx * ui_scale;
          draw->AddRectFilled(
              min, ImVec2(min.x + bar_width, max.y),
              ImGui::ColorConvertFloat4ToU32(
                  ToImVec4(tokens.activity_bar_active_border)));
        }
        const ImU32 icon_color = ImGui::ColorConvertFloat4ToU32(
            active ? ToImVec4(tokens.activity_bar_foreground)
                   : ToImVec4(tokens.activity_bar_inactive_foreground));
        const ImVec2 center((min.x + max.x) * 0.5f,
                            (min.y + max.y) * 0.5f);
        // 有图标就画图标；没有就先画说明首字符。
        const IconPainter item_painter = ImGuiIconPainter(item.icon);
        if (item_painter.draw) {
          item_painter.draw(draw, center, icon, icon_color);
        } else if (!item.tooltip.empty()) {
          const char glyph[2] = {item.tooltip[0], '\0'};
          const ImVec2 glyph_size = ImGui::CalcTextSize(glyph);
          draw->AddText(
              ImVec2(center.x - glyph_size.x * 0.5f,
                     center.y - glyph_size.y * 0.5f),
              icon_color, glyph);
        }
        // 悬停显示活动栏项说明。
        if (hovered && !item.tooltip.empty()) {
          ImGui::SetTooltip("%s", item.tooltip.c_str());
        }
        // 点击执行注册的命令；点已经是当前项的图标则收起/展开主侧栏。
        if (clicked && !item.command.empty()) {
          // 最大化时先还原布局，再显示点中的视图，不按“点当前项收起”处理。
          if (_maximized_view != MaximizedView::None) {
            ClearMaximizedView();
            ExecuteCommand(item.command);
          } else {
            ExecuteCommand(ActivityClickCommand(item, active));
          }
        }
        ImGui::PopID();
      }
      ImGui::PopClipRect();
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
  }

  void ApplyViewWindowSessionState() {
    for (ViewWindow& view : _parts.view_windows) {
      const auto saved = _session.view_windows.find(view.id);
      if (saved != _session.view_windows.end()) {
        view.open = saved->second;
      }
    }
    for (MonitorView& view : _parts.monitor_views) {
      const auto saved = _session.view_windows.find(view.id);
      if (saved != _session.view_windows.end()) {
        view.open = saved->second;
      }
    }
  }

  // 启动时应用 session；未设置或非法时回退固定 Summary。
  void ApplyLogSessionConfig() {
    tg::LogConfig config;
    if (_session.log_level >= 0 &&
        _session.log_level <= static_cast<int>(tg::LogLevel::Off)) {
      config.level = static_cast<tg::LogLevel>(_session.log_level);
      config.categories = _session.log_categories == kUnsetLogCategories
                              ? tg::kAllLogCategories
                              : _session.log_categories;
    }
    config.write_file = tg::GetLogConfig().write_file;
    tg::SetLogConfig(config);
    _log_auto_scroll = _session.log_auto_scroll;
  }

  // UI 改级别/类别后立即生效，并写回 session 字段。
  void ApplyLogConfig(tg::LogConfig config) {
    // UI 不暴露写文件开关，保持调用方/启动时设置的值。
    config.write_file = tg::GetLogConfig().write_file;
    tg::SetLogConfig(config);
    const tg::LogConfig applied = tg::GetLogConfig();
    _session.log_level = static_cast<int>(applied.level);
    _session.log_categories = applied.categories;
  }

  // 面板容器：按注册顺序画所有视图，同一 dock 节点里形成多个页签。
  void ShowPanels() {
    // 最大化属性表时面板让位，整块不画。
    if (_maximized_view == MaximizedView::Properties) {
      return;
    }
    std::vector<const PanelView*> views;
    views.reserve(_parts.panel_views.size());
    for (const PanelView& view : _parts.panel_views) {
      views.push_back(&view);
    }
    std::stable_sort(views.begin(), views.end(),
                     [](const PanelView* left, const PanelView* right) {
                       return left->order < right->order;
                     });
    for (const PanelView* view : views) {
      const std::string window_name = view->window_name();
      ShowDockTab(window_name.c_str());
      PushPanelStyle();
      if (ImGui::Begin(window_name.c_str())) {
        // Output 页签栏右侧：最大化/还原与关闭。
        if (view->id == kOutputPanelId) {
          const DockTitleActionPress press = DrawDockTitleActions(
              ImGui::GetWindowDockNode(),
              _maximized_view == MaximizedView::Output, kOutputMaximizeTooltip,
              kOutputRestoreTooltip, kOutputCloseTooltip);
          if (press.toggle_maximize) {
            SetMaximizedView(ToggleMaximizedView(_maximized_view,
                                                 MaximizedView::Output));
          }
          if (press.close) {
            SetPanelVisible(false);
          }
        }
        DrawPanelToolbar(*view);
        // 视图只画内容；窗口生命周期由这里管。
        if (view->draw) {
          view->draw();
        }
      }
      ImGui::End();
      PopPanelStyle();
    }
  }

  // 独立视图窗口：默认浮动，拖到边缘可停靠；位置和大小由 imgui.ini 记忆。
  void ShowViewWindows() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float ui_scale = ImGui::GetStyle().FontScaleMain;
    const float margin = kViewWindowMarginEpx * ui_scale;
    const float max_width =
        (std::max)(viewport->WorkSize.x - margin * kViewWindowMarginCount,
                   1.0f);
    const float max_height =
        (std::max)(viewport->WorkSize.y - margin * kViewWindowMarginCount,
                   1.0f);
    const float width =
        (std::min)(kViewWindowDefaultWidthEpx * ui_scale, max_width);
    const float height =
        (std::min)(kViewWindowDefaultHeightEpx * ui_scale, max_height);
    for (ViewWindow& view : _parts.view_windows) {
      if (!view.open) {
        continue;
      }
      const std::string window_name = view.window_name();
      ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_FirstUseEver);
      ImGui::SetNextWindowPos(
          ImVec2(viewport->WorkPos.x +
                     (viewport->WorkSize.x - width) * kViewWindowCenterRatio,
                 viewport->WorkPos.y +
                     (viewport->WorkSize.y - height) * kViewWindowCenterRatio),
          ImGuiCond_FirstUseEver);
      if (ImGui::Begin(window_name.c_str(), &view.open)) {
        if (view.draw) {
          view.draw();
        }
      }
      ImGui::End();
    }
  }

  std::shared_ptr<tg::Node> FindNodeShared(tg::Node* target) const {
    if (target == nullptr) {
      return nullptr;
    }
    for (const auto& graph : _controller.getGraphs()) {
      for (const auto& node : graph->getNodes()) {
        if (node.get() == target) {
          return node;
        }
      }
    }
    return nullptr;
  }

  std::vector<MonitorSourceEntry> CollectMonitorSources(
      const MonitorView& view) const {
    std::vector<MonitorSourceEntry> sources;
    for (const auto& graph : _controller.getGraphs()) {
      for (const auto& node : graph->getNodes()) {
        const unsigned int count = node->nFixedPorts(tg::PortType::Out);
        for (unsigned int index = 0; index < count; ++index) {
          std::shared_ptr<tg::NodeData> data = node->getOutPortData(index);
          // 没有算出的输出不进入来源列表。
          if (data == nullptr) {
            continue;
          }
          if (view.accepts && !view.accepts(*data)) {
            continue;
          }
          const char* caption = node->fixedPortCaption(tg::PortType::Out, index);
          const std::string port =
              caption != nullptr && *caption != '\0'
                  ? std::string(caption)
                  : "out[" + std::to_string(index) + "]";
          MonitorSourceEntry entry;
          entry.label =
              MonitorSourceLabel(graph->getName(), node->getName(), port);
          entry.node = node;
          entry.port = index;
          entry.data = std::move(data);
          sources.push_back(std::move(entry));
        }
      }
    }
    return sources;
  }

  MonitorSourceEntry ResolveMonitorSource(
      const MonitorView& view, MonitorWindowState& state,
      const std::vector<MonitorSourceEntry>& sources, const char** message) {
    *message = nullptr;
    const std::shared_ptr<tg::Node> selected = FindNodeShared(SelectedNode());
    if (state.follow) {
      if (selected == nullptr) {
        *message = sources.empty() ? kMonitorNoSourceMessage
                                   : kMonitorNoSelectionMessage;
        return {};
      }
      for (const MonitorSourceEntry& source : sources) {
        if (source.node == selected) {
          return source;
        }
      }
      *message = kMonitorSelectedNoMatchMessage;
      return {};
    }
    const std::shared_ptr<tg::Node> pinned = state.pinned_node.lock();
    if (pinned == nullptr) {
      state.pinned_node.reset();
      *message = kMonitorPinnedNoMatchMessage;
      return {};
    }
    for (const MonitorSourceEntry& source : sources) {
      if (source.node == pinned && source.port == state.pinned_port) {
        return source;
      }
    }
    *message = kMonitorPinnedNoMatchMessage;
    return {};
  }

  // 画 Source / Follow / Maximize 三个标题动作；返回值只用于调试调用方无义。
  void ShowMonitorActions(MonitorView& view, MonitorWindowState& state,
                          const std::vector<MonitorSourceEntry>& sources,
                          const MonitorSourceEntry& resolved, bool docked,
                          float source_width, float ui_scale) {
    const std::string source_text =
        resolved.label.empty() ? std::string(kMonitorSourceLabel)
                               : "Source: " + resolved.label;
    const std::string popup_id = "##MonitorSource_" + view.id;
    const float follow_width = BarActionWidth(kMonitorFollowLabel);
    const float max_width = ImGui::GetFrameHeight();
    const bool max_requested =
        ImGui::Selectable(source_text.c_str(), false,
                          ImGuiSelectableFlags_None,
                          ImVec2(source_width, 0.0f));
    // 标题栏空间不足时 Source 会截断，悬停仍给完整来源名。
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", source_text.c_str());
    }
    if (max_requested) {
      state.focus_source_filter = true;
      ImGui::OpenPopup(popup_id.c_str());
    }
    ImGui::SameLine();
    if (ImGui::Selectable(kMonitorFollowLabel, state.follow,
                          ImGuiSelectableFlags_None,
                          ImVec2(follow_width, 0.0f))) {
      const MonitorFollowDecision follow =
          DecideMonitorFollowToggle(state.follow, !resolved.label.empty());
      if (follow.pin_current) {
        state.pinned_node = resolved.node;
        state.pinned_port = resolved.port;
      }
      if (follow.clear_pin) {
        state.pinned_node.reset();
      }
      state.follow = follow.follow;
    }
    ImGui::SameLine();
    const bool maximize_available = MonitorMaximizeAvailable(docked);
    ImGui::BeginDisabled(!maximize_available);
    if (ImGui::Selectable(state.maximized ? "Restore" : "Maximize", false,
                          ImGuiSelectableFlags_None,
                          ImVec2(max_width, 0.0f))) {
      if (maximize_available) {
        state.maximized = !state.maximized;
        if (state.maximized) {
          state.restore_pos = ImGui::GetWindowPos();
          state.restore_size = ImGui::GetWindowSize();
        } else {
          state.restore_next_frame = true;
        }
      }
    }
    ImGui::EndDisabled();
    if (!maximize_available &&
        ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      ImGui::SetTooltip("Undock the monitor before maximizing.");
    }

    ImGui::SetNextWindowSizeConstraints(
        ImVec2(kPickNodeKindMinWidthEpx * ui_scale, 0.0f),
        ImVec2(FLT_MAX, FLT_MAX));
    PushQuickPickStyle();
    if (ImGui::BeginPopup(popup_id.c_str())) {
      if (state.source_filter.capacity() < kLogFilterMinCapacity) {
        state.source_filter.reserve(kLogFilterMinCapacity);
      }
      const std::size_t buffer_size =
          state.source_filter.capacity() + kLogTextTerminatorSize;
      ImGui::SetNextItemWidth(-FLT_MIN);
      if (state.focus_source_filter) {
        ImGui::SetKeyboardFocusHere();
        state.focus_source_filter = false;
      }
      ImGui::InputTextWithHint("##MonitorSourceFilter", kMonitorSourceFilterHint,
                               state.source_filter.data(), buffer_size,
                               ImGuiInputTextFlags_CallbackResize,
                               ResizePropertyText, &state.source_filter);
      bool has_visible = false;
      for (const MonitorSourceEntry& source : sources) {
        if (!MonitorSourceMatchesFilter(source.label, state.source_filter)) {
          continue;
        }
        has_visible = true;
        if (ImGui::Selectable(source.label.c_str(), false)) {
          state.follow = false;
          state.pinned_node = source.node;
          state.pinned_port = source.port;
          ImGui::CloseCurrentPopup();
        }
      }
      if (!has_visible) {
        ImGui::TextDisabled("%s", kMonitorNoSourceMessage);
      }
      ImGui::EndPopup();
    }
    PopQuickPickStyle();
  }

  void ShowMonitorContent(MonitorView& view, MonitorWindowState& state,
                          const std::vector<MonitorSourceEntry>& sources,
                          float ui_scale) {
    const char* message = nullptr;
    const MonitorSourceEntry source =
        ResolveMonitorSource(view, state, sources, &message);
    if (source.data == nullptr) {
      ImGui::TextDisabled("%s", message != nullptr ? message
                                                   : kMonitorNoSourceMessage);
      return;
    }
    std::string status;
    if (view.status) {
      status = view.status(source.data.get());
    }
    const float min_content_height = kMonitorMinContentHeightEpx * ui_scale;
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float status_height = status.empty() ? 0.0f
                                         : ImGui::GetTextLineHeightWithSpacing();
    // 高度不足时省掉状态行，content 仍拿到完整可用高度。
    if (avail.y - status_height < min_content_height) {
      status.clear();
      status_height = 0.0f;
    }
    const float content_height = (std::max)(avail.y - status_height, 0.0f);
    if (view.draw) {
      view.draw(*source.data, avail.x, content_height);
    }
    if (!status.empty()) {
      ImGui::TextDisabled("%s", status.c_str());
    }
  }

  // 监视器窗口：Source / Follow / Maximize 在浮动标题栏右侧；停靠时放在内容首行。
  void ShowMonitors(const WorkbenchChrome& chrome) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float ui_scale = ImGui::GetStyle().FontScaleMain;
    const float margin = kViewWindowMarginEpx * ui_scale;
    const float default_width =
        (std::min)(kViewWindowDefaultWidthEpx * ui_scale,
                   (std::max)(viewport->WorkSize.x - margin * kViewWindowMarginCount,
                              1.0f));
    const float default_height =
        (std::min)(kViewWindowDefaultHeightEpx * ui_scale,
                   (std::max)(viewport->WorkSize.y - margin * kViewWindowMarginCount,
                              1.0f));
    for (MonitorView& view : _parts.monitor_views) {
      if (!view.open) {
        continue;
      }
      MonitorWindowState& state = _monitor_states[view.id];
      const std::vector<MonitorSourceEntry> sources =
          CollectMonitorSources(view);
      const std::string window_name = view.window_name();
      const ImGuiWindowFlags flags =
          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
      if (state.maximized && !state.restore_next_frame) {
        ImGui::SetNextWindowPos(ToImVec2(chrome.dock_pos));
        ImGui::SetNextWindowSize(ToImVec2(chrome.dock_size));
      } else if (state.restore_next_frame) {
        ImGui::SetNextWindowPos(state.restore_pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(state.restore_size, ImGuiCond_Always);
        state.restore_next_frame = false;
      } else {
        ImGui::SetNextWindowPos(
            ImVec2(viewport->WorkPos.x +
                       (viewport->WorkSize.x - default_width) *
                           kViewWindowCenterRatio,
                   viewport->WorkPos.y +
                       (viewport->WorkSize.y - default_height) *
                           kViewWindowCenterRatio),
            ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(default_width, default_height),
                                 ImGuiCond_FirstUseEver);
      }
      bool docked = false;
      ImVec2 window_pos;
      ImVec2 window_size;
      // 首次打开保持浮动，不自动并进 Output/其它停靠页签。
      ImGui::SetNextWindowDockID(0, ImGuiCond_FirstUseEver);
      if (ImGui::Begin(window_name.c_str(), &view.open, flags)) {
        docked = ImGui::IsWindowDocked();
        if (docked) {
          state.maximized = false;
        }
        window_pos = ImGui::GetWindowPos();
        window_size = ImGui::GetWindowSize();
        const char* message = nullptr;
        const MonitorSourceEntry resolved =
            ResolveMonitorSource(view, state, sources, &message);
        if (docked) {
          const float source_width = kMonitorSourceActionWidthEpx * ui_scale;
          ShowMonitorActions(view, state, sources, resolved, true, source_width,
                             ui_scale);
        }
        ShowMonitorContent(view, state, sources, ui_scale);
      }
      ImGui::End();
      // 浮动窗口：标题栏动作用独立透明窗口叠在右侧，避免和默认关闭/拖动冲突。
      if (view.open && !docked) {
        const float titlebar_height = ImGui::GetFrameHeight();
        const float follow_width = BarActionWidth(kMonitorFollowLabel);
        const float max_width = titlebar_height;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float close_width = titlebar_height;
        float source_width = kMonitorSourceActionWidthEpx * ui_scale;
        const float available =
            window_size.x - close_width - spacing * 3.0f - follow_width -
            max_width;
        source_width = (std::min)(source_width,
                                  (std::max)(available, 40.0f * ui_scale));
        const float actions_width =
            source_width + follow_width + max_width + spacing * 3.0f;
        ImGui::SetNextWindowPos(
            ImVec2(window_pos.x + window_size.x - close_width - actions_width,
                   window_pos.y));
        ImGui::SetNextWindowSize(ImVec2(actions_width, titlebar_height));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                            ImVec2(spacing, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        const std::string overlay_name = "##MonitorActions_" + view.id;
        ImGui::Begin(overlay_name.c_str(), nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoScrollbar |
                         ImGuiWindowFlags_NoScrollWithMouse |
                         ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoDocking |
                         ImGuiWindowFlags_NoNav |
                         ImGuiWindowFlags_NoFocusOnAppearing |
                         ImGuiWindowFlags_NoBackground);
        const char* message = nullptr;
        const MonitorSourceEntry resolved =
            ResolveMonitorSource(view, state, sources, &message);
        ShowMonitorActions(view, state, sources, resolved, false, source_width,
                           ui_scale);
        ImGui::End();
        ImGui::PopStyleVar(4);
      }
    }
  }

  // 日志行颜色：错误/警告用 VS Code 语义色，Info 正文，Debug/Trace 次要文字。
  static ImVec4 LogLevelColor(tg::LogLevel level) {
    const theme_detail::ThemeTokens& tokens = theme_detail::CurrentTokens();
    switch (level) {
      case tg::LogLevel::Error:
        return ToImVec4(tokens.error_foreground);
      case tg::LogLevel::Warn:
        return ToImVec4(tokens.warning_foreground);
      case tg::LogLevel::Info:
        return ToImVec4(tokens.editor_foreground);
      case tg::LogLevel::Debug:
      case tg::LogLevel::Trace:
        return ToImVec4(tokens.description_foreground);
      case tg::LogLevel::Off:
        break;
    }
    return ToImVec4(tokens.editor_foreground);
  }

  // Output 视图内容：日志行；窗口和页签由 ShowPanels 管。
  void ShowOutputContent() {
    const float ui_scale = ImGui::GetStyle().FontScaleMain;
    const std::size_t previous_cursor = _log_cursor;
    _log_cursor = tg::copyLogRecordsSince(_log_cursor, _log_records);

    PushOutputToolbarStyle();
    const tg::LogConfig config = tg::GetLogConfig();
    const std::optional<LogDetailPreset> preset =
        LogDetailPresetForConfig(config);
    const char* preset_label =
        preset.has_value() ? LogDetailPresetLabel(*preset)
                           : kLogCustomPresetLabel;
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(kLogDetailLabel);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(kLogDetailComboWidthEpx * ui_scale);
    const ThemedComboState combo = BeginThemedCombo("##LogDetail", preset_label);
    if (combo.open) {
      for (const LogDetailPreset candidate :
           {LogDetailPreset::Off, LogDetailPreset::Errors,
            LogDetailPreset::Timing, LogDetailPreset::Summary,
            LogDetailPreset::Detailed, LogDetailPreset::Everything}) {
        const bool selected = preset.has_value() && *preset == candidate;
        if (ImGui::Selectable(LogDetailPresetLabel(candidate), selected)) {
          ApplyLogConfig(LogConfigForDetailPreset(candidate));
        }
      }
    }
    EndThemedCombo(combo);
    ImGui::SameLine();
    if (DrawBarAction(kLogCategoriesLabel)) {
      ImGui::OpenPopup(kLogCategoriesPopupId);
    }
    ImGui::SetNextWindowSize(ImVec2(kLogCategoriesPopupWidthEpx * ui_scale,
                                    0.0f),
                             ImGuiCond_Appearing);
    if (ImGui::BeginPopup(kLogCategoriesPopupId)) {
      for (const tg::LogCategory category : kLogCategories) {
        bool enabled =
            (config.categories & static_cast<std::uint32_t>(category)) != 0;
        if (DrawThemedCheckbox(LogCategoryLabel(category),
                               LogCategoryLabel(category), &enabled,
                               ui_scale)) {
          tg::LogConfig next = config;
          if (enabled) {
            next.categories |= static_cast<std::uint32_t>(category);
          } else {
            next.categories &= ~static_cast<std::uint32_t>(category);
          }
          ApplyLogConfig(next);
        }
      }
      ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (DrawBarAction(kLogClearLabel)) {
      tg::clearLogRecords();
      _log_records.clear();
      _log_cursor = 0;
    }
    ImGui::SameLine();
    if (DrawThemedCheckbox(kLogAutoScrollLabel, kLogAutoScrollLabel,
                           &_log_auto_scroll, ui_scale)) {
      _session.log_auto_scroll = _log_auto_scroll;
    }
    const std::size_t dropped = tg::droppedLogRecordCount();
    // 有丢弃时明确显示，避免并行日志洪水静默缺行。
    if (dropped > 0) {
      ImGui::SameLine();
      ImGui::TextDisabled("Dropped %zu", dropped);
    }

    if (_log_filter.capacity() < kLogFilterMinCapacity) {
      _log_filter.reserve(kLogFilterMinCapacity);
    }
    const std::size_t filter_buffer_size =
        _log_filter.capacity() + kLogTextTerminatorSize;
    const float filter_preferred = kOutputFilterWidthEpx * ui_scale;
    const float filter_min = kOutputFilterMinWidthEpx * ui_scale;
    const float content_right =
        ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    const float controls_right = ImGui::GetItemRectMax().x;
    const float available =
        content_right - controls_right - ImGui::GetStyle().ItemSpacing.x;
    // 过滤框优先放在右侧；剩余宽度不足时换到下一行占满。
    if (available >= filter_min) {
      const float filter_width = (std::min)(filter_preferred, available);
      ImGui::SameLine();
      ImGui::SetCursorScreenPos(
          ImVec2(content_right - filter_width,
                 ImGui::GetCursorScreenPos().y));
      ImGui::SetNextItemWidth(filter_width);
    } else {
      ImGui::SetNextItemWidth(-FLT_MIN);
    }
    ImGui::InputTextWithHint("##LogFilter", kLogFilterHint,
                             _log_filter.data(), filter_buffer_size,
                             ImGuiInputTextFlags_CallbackResize,
                             ResizePropertyText, &_log_filter);
    PopOutputToolbarStyle();

    // Begin / End：日志表格。Message 列自动换行，纵向滚动。
    ImGui::BeginChild("OutputLog", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    const ImGuiTableFlags log_table_flags =
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_SizingStretchProp;
    if (ImGui::BeginTable("OutputLogTable", 4, log_table_flags)) {
      ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed,
                              kLogLevelColumnWidthEpx * ui_scale);
      ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed,
                              kLogCategoryColumnWidthEpx * ui_scale);
      ImGui::TableSetupColumn("Context", ImGuiTableColumnFlags_WidthFixed,
                              kLogContextColumnWidthEpx * ui_scale);
      ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
      for (const tg::LogRecord& record : _log_records) {
        if (!LogRecordMatchesFilter(record, _log_filter)) {
          continue;
        }
        const LogRecordParts parts = SplitLogRecord(record);
        const ImVec4 level_color = LogLevelColor(record.level);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextColored(level_color, "%s", LogLevelLabel(record.level));
        ImGui::TableSetColumnIndex(1);
        ImGui::TextDisabled("%s", LogCategoryLabel(record.category));
        ImGui::TableSetColumnIndex(2);
        ImGui::TextDisabled("%s",
                            parts.context.empty() ? "-" : parts.context.c_str());
        ImGui::TableSetColumnIndex(3);
        ImGui::PushStyleColor(ImGuiCol_Text, level_color);
        ImGui::TextWrapped("%s", parts.message.c_str());
        ImGui::PopStyleColor();
      }
      ImGui::EndTable();
    }
    // 有新行且勾选 Auto-scroll 才滚到末尾，否则保留用户往上翻的位置。
    if (_log_auto_scroll && _log_cursor != previous_cursor) {
      ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
  }

  // 主侧栏 Search 视图：输入框在上，命中的图名和节点 identifier 在下。
  void ShowSearchSideBar(
      const std::vector<std::shared_ptr<tg::Graph>>& graphs) {
    constexpr std::size_t kTextTerminatorSize = 1;
    const float margin = kSurfaceTextMarginEpx * ImGui::GetStyle().FontScaleMain;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + margin);
    if (_search_filter.capacity() < kPickNodeKindFilterMinCapacity) {
      _search_filter.reserve(kPickNodeKindFilterMinCapacity);
    }
    const std::size_t filter_buffer_size =
        _search_filter.capacity() + kTextTerminatorSize;
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##SearchFilter", kSearchFilterHint,
                             _search_filter.data(), filter_buffer_size,
                             ImGuiInputTextFlags_CallbackResize,
                             ResizePropertyText, &_search_filter);
    const std::vector<SearchHit> hits =
        CollectSearchHits(graphs, _search_filter);
    if (!_search_filter.empty() && hits.empty()) {
      ImGui::TextDisabled("%s", kSearchEmptyLabel);
      return;
    }
    for (std::size_t hit_index = 0; hit_index < hits.size(); ++hit_index) {
      const SearchHit& hit = hits[hit_index];
      if (hit.graph == nullptr) {
        continue;
      }
      std::string label = hit.graph->getName();
      if (hit.node != nullptr) {
        label += kSearchHitSeparator;
        label += hit.node->getName();
      }
      ImGui::PushID(static_cast<int>(hit_index));
      if (ImGui::Selectable(label.c_str())) {
        OpenGraphInEditor(hit.graph);
        if (hit.node == nullptr) {
          _focus.ClearNodeSelection();
        } else {
          _focus.SelectNode(hit.node.get());
        }
      }
      ImGui::PopID();
    }
  }

  void Update() {
    const auto& graphs = _controller.getGraphs();
    // 图可能被删空或变短，先把选中下标夹回有效范围，后面取图才不会越界。
    if (!graphs.empty()) {
      _focus.selected_graph_index() = std::clamp(_focus.selected_graph_index(), 0,
                                  static_cast<int>(graphs.size()) - 1);
    }
    PruneOpenEditors();
    std::shared_ptr<tg::Node> selected_node;
    // 选中地址还在当前图里才用，节点删掉后不再写悬空指针。
    if (!graphs.empty() && _focus.selected_node() != nullptr) {
      for (const auto& node : graphs[_focus.selected_graph_index()]->getNodes()) {
        // 地址相同就是画布上点中的那个节点。
        if (node.get() == _focus.selected_node()) {
          selected_node = node;
          break;
        }
      }
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ShowWindowTitleBar(graphs);
    ShowProjectPathDialog();
    const float ui_scale = ImGui::GetStyle().FontScaleMain;
    const float title_bar_height = kTitleBarHeightEpx * ui_scale;
    // 状态栏隐藏时不占高度，编辑区和面板往下长满。
    const float status_bar_height =
        _parts.status_bar_visible ? kStatusBarHeightEpx * ui_scale : 0.0f;
    const float activity_width = kActivityBarWidthEpx * ui_scale;
    const WorkbenchChrome chrome = LayoutWorkbenchChrome(
        ToVec2(viewport->WorkPos), ToVec2(viewport->WorkSize), title_bar_height,
        status_bar_height, activity_width, _parts.activity_bar_visible);
    const bool dock_open =
        chrome.dock_size.x > 0.0f && chrome.dock_size.y > 0.0f;
    if (dock_open) {
      ImGui::SetNextWindowPos(ToImVec2(chrome.dock_pos));
      ImGui::SetNextWindowSize(ToImVec2(chrome.dock_size));
      ImGui::SetNextWindowViewport(viewport->ID);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
      const ImGuiWindowFlags host_flags =
          ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
          ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
          ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
          ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBackground;
      // Begin / End：活动栏右边、状态栏上面。主侧栏、编辑区、辅助侧栏和面板停在里面。
      ImGui::Begin("MainDockHost", nullptr, host_flags);
      ImGui::PopStyleVar(3);

      const ImGuiID dockspace_id = ImGui::GetID("MainDockCustomTitle");
      const ImGuiWindowSettings* property_settings =
          ImGui::FindWindowSettingsByID(ImHashStr("Properties"));
      const bool property_docked =
          property_settings != nullptr && property_settings->DockId != 0;
      const ImGuiWindowSettings* output_settings =
          ImGui::FindWindowSettingsByID(ImHashStr(kOutputWindowName));
      const bool output_docked =
          output_settings != nullptr && output_settings->DockId != 0;
      // 最大化时其它停靠让位；只有目标视图应停靠。
      const bool property_expected =
          _parts.secondary_sidebar_visible &&
          _maximized_view != MaximizedView::Output;
      const bool output_expected =
          _parts.panel_visible &&
          _maximized_view != MaximizedView::Properties;
      // 隐藏的或让位的属性/面板不算缺失，避免每帧重建。
      const bool property_missing = property_expected && !property_docked;
      const bool output_missing = output_expected && !output_docked;
      // 记住三块停靠的比例；最大化时布局不是常规位置，跳过。
      if (_maximized_view == MaximizedView::None) {
        if (_parts.primary_sidebar_visible) {
          const float ratio =
              DockSplitRatioOfWindow("Nodes###Project", ImGuiAxis_X);
          // 没量到有效宽度时保留上一次的记忆值。
          if (ratio > 0.0f) {
            _primary_sidebar_ratio = ratio;
          }
        }
        if (_parts.secondary_sidebar_visible) {
          const float ratio = DockSplitRatioOfWindow("Properties", ImGuiAxis_X);
          if (ratio > 0.0f) {
            _secondary_sidebar_ratio = ratio;
          }
        }
        if (_parts.panel_visible) {
          const float ratio =
              DockSplitRatioOfWindow(kOutputWindowName, ImGuiAxis_Y);
          if (ratio > 0.0f) {
            _panel_height_ratio = ratio;
          }
        }
      }
      // 停靠还不存在，或应停靠的属性、面板没停进格子时重建。活动栏和状态栏不进这里。
      if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr || property_missing ||
          output_missing || _workbench_layout_dirty) {
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, ToImVec2(chrome.dock_size));
        // 剩余节点从整个停靠宿主开始，每步劈完再往中间收。
        ImGuiID dock_canvas = dockspace_id;
        // 最大化时只停目标视图，其它停靠全部隐藏。
        if (_maximized_view != MaximizedView::None) {
          if (_maximized_view == MaximizedView::Output) {
            for (const PanelView& view : _parts.panel_views) {
              const std::string window_name = view.window_name();
              ImGui::DockBuilderDockWindow(window_name.c_str(), dock_canvas);
            }
          } else {
            ImGui::DockBuilderDockWindow("Properties", dock_canvas);
          }
          if (ImGuiDockNode* maximized_node =
                  ImGui::DockBuilderGetNode(dock_canvas)) {
            maximized_node->SetLocalFlags(
                ImGuiDockNodeFlags_NoWindowMenuButton);
          }
        } else {
          // 主侧栏可见时才劈出左列。
          if (_parts.primary_sidebar_visible) {
            ImGuiID dock_project = 0;
            const float primary_sidebar_ratio =
                _primary_sidebar_ratio > 0.0f
                    ? _primary_sidebar_ratio
                    : InitialPrimarySidebarRatio(viewport->WorkSize.x,
                                                 chrome.dock_size.x,
                                                 _parts.activity_bar_visible);
            ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left,
                                        primary_sidebar_ratio, &dock_project,
                                        &dock_canvas);
            ImGui::DockBuilderDockWindow("Project", dock_project);
            HideBuiltDockTab(dock_project);
            DisallowDockingOver(dock_project);
          }
          // 辅助侧栏可见时才劈出右列。
          if (_parts.secondary_sidebar_visible) {
            ImGuiID dock_property = 0;
            ImGuiID dock_center = 0;
            const float secondary_sidebar_ratio =
                _secondary_sidebar_ratio > 0.0f
                    ? _secondary_sidebar_ratio
                    : SecondarySidebarWidthRatio();
            ImGui::DockBuilderSplitNode(dock_canvas, ImGuiDir_Right,
                                        secondary_sidebar_ratio, &dock_property,
                                        &dock_center);
            dock_canvas = dock_center;
            ImGui::DockBuilderDockWindow("Properties", dock_property);
            if (ImGuiDockNode* property_node =
                    ImGui::DockBuilderGetNode(dock_property)) {
              property_node->SetLocalFlags(
                  ImGuiDockNodeFlags_NoWindowMenuButton);
            }
          }
          // 面板可见时才在编辑列下劈出输出。
          if (_parts.panel_visible) {
            ImGuiID dock_output = 0;
            ImGuiID dock_editor = 0;
            const float panel_height_ratio =
                _panel_height_ratio > 0.0f ? _panel_height_ratio
                                           : OutputPaneHeightRatio();
            ImGui::DockBuilderSplitNode(dock_canvas, ImGuiDir_Down,
                                        panel_height_ratio, &dock_output,
                                        &dock_editor);
            dock_canvas = dock_editor;
            for (const PanelView& view : _parts.panel_views) {
              const std::string window_name = view.window_name();
              ImGui::DockBuilderDockWindow(window_name.c_str(), dock_output);
            }
            if (ImGuiDockNode* output_node =
                    ImGui::DockBuilderGetNode(dock_output)) {
              output_node->SetLocalFlags(
                  ImGuiDockNodeFlags_NoWindowMenuButton);
            }
          }
          // Explorer / 编辑区自绘顶栏，不显示停靠标签；属性与输出保留页签。
          ImGui::DockBuilderDockWindow("Canvas", dock_canvas);
          HideBuiltDockTab(dock_canvas);
          DisallowDockingOver(dock_canvas);
        }
        ImGui::DockBuilderFinish(dockspace_id);
        _workbench_layout_dirty = false;
      }
      ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
      ImGui::End();
    }

    ShowActivityBar(ToImVec2(chrome.activity_pos),
                    ToImVec2(chrome.activity_size));

    // Begin / End：主侧栏里的 Explorer。折叠时不画图和节点，编辑区继续画。
    // 顶边与编辑区页签条对齐：WindowPadding 顶为 0，标题行高度同 kEditorTabHeightEpx。
    bool explorer_focused = false;
    // 最大化时主侧栏让位给目标视图。
    if (_parts.primary_sidebar_visible &&
        _maximized_view == MaximizedView::None) {
      HideDockTab("Nodes###Project");
      PushSideBarStyle();
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
      if (ImGui::Begin("Nodes###Project")) {
        explorer_focused =
            ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        PushExplorerTreeStyle();
        constexpr const char* kExplorerTitleLabel = "EXPLORER";
        const bool search_view =
            _focus.primary_side_bar() == PrimarySideBarView::Search;
        const char* const side_bar_title =
            search_view ? kSearchSideBarTitle : kExplorerTitleLabel;
        constexpr const char* kNewNodeActionLabel = "+ Node";
        constexpr const char* kNewGraphActionLabel = "+ Graph";
        constexpr const char* kPasteGraphActionLabel = "+ Paste";
        constexpr float kHalfSpan = 0.5f;
        const float ui_scale = ImGui::GetStyle().FontScaleMain;
        const float header_height = kEditorTabHeightEpx * ui_scale;
        const float header_pad_x = kEditorTabHorizontalPaddingEpx * ui_scale;
        const float project_row_height = ImGui::GetFrameHeight();
        const ImVec2 header_min = ImGui::GetCursorScreenPos();
        const float header_width = ImGui::GetContentRegionAvail().x;
        const ImVec2 header_max(header_min.x + header_width,
                                header_min.y + header_height);
        const ExplorerTreePalette palette = MakeExplorerTreePalette();
        const float title_font_size = kExplorerViewTitleFontSizeEpx * ui_scale;
        const ImVec2 title_size = ImGui::GetFont()->CalcTextSizeA(
            title_font_size, FLT_MAX, 0.0f, side_bar_title);
        ImGui::GetWindowDrawList()->AddText(
            ImGui::GetFont(), title_font_size,
            ImVec2(header_min.x + kSurfaceTextMarginEpx * ui_scale,
                   header_min.y + (header_height - title_size.y) * kHalfSpan),
            palette.title_text, side_bar_title);
        // Explorer 标题行右侧动作槽；Search 视图不画。默认空。
        if (!search_view && !_parts.explorer_title_actions.empty()) {
          const float action_size = kViewActionSizeEpx * ui_scale;
          const float action_gap = 2.0f * ui_scale;
          const float actions_width =
              action_size * static_cast<float>(_parts.explorer_title_actions.size()) +
              action_gap *
                  static_cast<float>(_parts.explorer_title_actions.size() - 1);
          const float actions_x = header_max.x - kSurfaceTextMarginEpx * ui_scale -
                                  actions_width;
          const float title_right =
              header_min.x + kSurfaceTextMarginEpx * ui_scale + title_size.x;
          // 标题和动作放不下时先不画动作，避免压字。
          if (actions_x > title_right + kBetweenControlsEpx * ui_scale) {
            ImGui::SetCursorScreenPos(
                ImVec2(actions_x,
                       header_min.y + (header_height - action_size) * kHalfSpan));
            for (const ViewAction& action : _parts.explorer_title_actions) {
              DrawViewAction(action, action_size);
              ImGui::SameLine(0.0f, action_gap);
            }
          }
        }
        const bool has_project = _controller.hasProjectRoot();
        ImGui::SetCursorScreenPos(ImVec2(header_min.x, header_max.y));
        if (search_view) {
          ShowSearchSideBar(graphs);
        } else {
        // 有工程时在工程名树根行显示新建动作，悬停该行时才显示。
        if (has_project) {
          const ImVec2 project_row_min = ImGui::GetCursorScreenPos();
          const float project_row_width = ImGui::GetContentRegionAvail().x;
          const float frame_pad_x = ImGui::GetStyle().FramePadding.x;
          const float node_button_width =
              ImGui::CalcTextSize(kNewNodeActionLabel).x +
              frame_pad_x * kFramePaddingSideCount;
          const float graph_button_width =
              ImGui::CalcTextSize(kNewGraphActionLabel).x +
              frame_pad_x * kFramePaddingSideCount;
          const float paste_button_width =
              ImGui::CalcTextSize(kPasteGraphActionLabel).x +
              frame_pad_x * kFramePaddingSideCount;
          const float action_gap = ImGui::GetStyle().ItemSpacing.x;
          const float actions_width = node_button_width + action_gap +
                                     graph_button_width + action_gap +
                                     paste_button_width;
          const float row_right_x = project_row_min.x + project_row_width;
          const float actions_screen_x =
              row_right_x - header_pad_x - actions_width;
          const ImVec2 mouse = ImGui::GetIO().MousePos;
          ExplorerProjectActionBand project_action_band;
          project_action_band.content_min_x = project_row_min.x;
          project_action_band.content_max_x = row_right_x;
          project_action_band.row_min_y = project_row_min.y;
          project_action_band.row_max_y =
              project_row_min.y + project_row_height;
          const bool actions_fit =
              project_row_width >= actions_width + header_pad_x + action_gap;
          const bool project_actions_requested =
              ShouldShowExplorerProjectActions(
                  project_action_band.contains(mouse.x, mouse.y),
                  ImGui::IsMouseDown(ImGuiMouseButton_Left),
                  _explorer_actions_press_hold);
          const bool show_project_actions =
              actions_fit && project_actions_requested;
          const ImVec2 project_row_max(project_row_min.x + project_row_width,
                                       project_row_min.y + project_row_height);
          ImDrawList* draw = ImGui::GetWindowDrawList();
          // 悬停工程名行时铺整宽底，与 Explorer 树行一致。
          if (project_action_band.contains(mouse.x, mouse.y)) {
            draw->AddRectFilled(project_row_min, project_row_max,
                                palette.row_hover_bg);
          }
          const float label_right_x =
              show_project_actions ? actions_screen_x - action_gap : row_right_x;
          const float project_font_size = kExplorerTreeFontSizeEpx * ui_scale;
          const ImVec2 project_name_size = ImGui::GetFont()->CalcTextSizeA(
              project_font_size, FLT_MAX, 0.0f,
              _controller.projectName().c_str());
          const float project_name_y =
              project_row_min.y + (project_row_height - project_name_size.y) *
                                      kHalfSpan;
          // 行太窄时标签没有可画区域，不 Push 空裁剪框。
          if (label_right_x > project_row_min.x) {
            draw->PushClipRect(project_row_min,
                               ImVec2(label_right_x, project_row_max.y), true);
            draw->AddText(
                ImGui::GetFont(), project_font_size,
                ImVec2(project_row_min.x +
                           (kExplorerTreeBasePaddingEpx +
                            kExplorerTreeChevronBoxEpx) *
                               ui_scale,
                       project_name_y),
                palette.text, _controller.projectName().c_str());
            draw->PopClipRect();
          }
          if (show_project_actions) {
            const float button_height = ImGui::GetFrameHeight();
            const float actions_screen_y =
                project_row_min.y + (project_row_height - button_height) *
                                        kHalfSpan;
            ImGui::SetCursorScreenPos(
                ImVec2(actions_screen_x, actions_screen_y));
            PushExplorerProjectActionStyle();
            // Begin / End：没有工程或没有图时禁用新建节点。
            ImGui::BeginDisabled(graphs.empty());
            // 点 Node 才弹出种类，节点加到当前图。文件对应节点。
            if (ImGui::Button(kNewNodeActionLabel,
                              ImVec2(node_button_width, button_height))) {
              ExecuteCommand("explorer.newNode");
            }
            // 悬停才说明这个按钮会弹出节点种类。
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
              ImGui::SetTooltip("New Node");
            }
            ImGui::EndDisabled();
            ImGui::SameLine(0.0f, action_gap);
            // 点 Graph 才新建一张空图，建完立刻命名，不自动打开编辑区页签。
            if (ImGui::Button(kNewGraphActionLabel,
                              ImVec2(graph_button_width, button_height))) {
              selected_node.reset();
              NewGraphUndoable();
            }
            // 悬停才说明这个按钮会新建一张空图。
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
              ImGui::SetTooltip("New Graph");
            }
            ImGui::SameLine(0.0f, action_gap);
            // 点 Paste 把图剪贴板里的整图贴成新图；剪贴板空时禁用。
            ImGui::BeginDisabled(_graph_clipboard.empty());
            if (ImGui::Button(kPasteGraphActionLabel,
                              ImVec2(paste_button_width, button_height))) {
              selected_node.reset();
              PasteGraphFromUi();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
              ImGui::SetTooltip("Paste Graph");
            }
            ImGui::EndDisabled();
            PopExplorerProjectActionStyle();
          }
          ImGui::SetCursorScreenPos(ImVec2(header_min.x, project_row_max.y));
        }
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

        // 无工程：主侧栏空状态，文案用本仓库的 project。
        if (!has_project) {
          ImGui::Spacing();
          ImGui::TextUnformatted(kNoProjectOpenedLabel);
          ImGui::Spacing();
          if (ImGui::Button("Open Project...")) {
            BeginProjectPathDialog(ProjectPathAction::Open);
          }
          if (!_session.recent_roots.empty()) {
            ImGui::Spacing();
            ImGui::TextUnformatted(kWelcomeRecentHeading);
            for (std::size_t recent_index = 0;
                 recent_index < _session.recent_roots.size(); ++recent_index) {
              ImGui::PushID(static_cast<int>(recent_index));
              const std::string& recent_root = _session.recent_roots[recent_index];
              if (ImGui::Selectable(recent_root.c_str())) {
                OpenProjectFromPath(std::filesystem::path(recent_root));
              }
              ImGui::PopID();
            }
          }
        }

        // 每张已创建的图都占一行，没有节点也留着；无工程时不画树。
        const float row_height = kExplorerTreeRowHeightEpx * ui_scale;
        float row_y = header_max.y;
        if (has_project) {
          row_y += project_row_height;
        }
        for (int graph_index = 0;
             has_project && graph_index < static_cast<int>(graphs.size());
             ++graph_index) {
          const auto& graph = graphs[graph_index];
          ImGui::PushID(graph.get());
          // 活动页签显示这张图、且没选节点时才高亮图行。
          const bool graph_selected =
              ActiveEditorGraph() == graph && _focus.selected_node() == nullptr;
          const bool renaming_graph = _rename_graph.get() == graph.get();
          const bool graph_open =
              _collapsed_graphs.find(graph.get()) == _collapsed_graphs.end();
          ImGui::SetCursorScreenPos(ImVec2(header_min.x, row_y));
          // 正在重命名时用整宽行 + 输入框，对照 Explorer 就地改名。
          if (renaming_graph) {
            const ImVec2 rename_min = ImGui::GetCursorScreenPos();
            const float rename_width = ImGui::GetContentRegionAvail().x;
            const ImVec2 rename_max(rename_min.x + rename_width,
                                    rename_min.y + row_height);
            ImDrawList* draw = ImGui::GetWindowDrawList();
            DrawExplorerRowBackground(draw, rename_min, rename_max, 1,
                                      graph_selected, false,
                                      ImGui::IsWindowFocused(), palette);
            const ImU32 rename_text_color =
                graph_selected ? palette.active_selection_text : palette.text;
            const float rename_box_left =
                rename_min.x +
                (kExplorerTreeBasePaddingEpx + kExplorerTreeIndentEpx) *
                    ui_scale;
            const float rename_box_width =
                kExplorerTreeChevronBoxEpx * ui_scale;
            DrawExplorerChevron(
                draw,
                ImVec2(rename_box_left + rename_box_width * kHalfSpan,
                       rename_min.y + row_height * kHalfSpan),
                graph_open, rename_text_color);
            ImGui::SetCursorScreenPos(
                ImVec2(rename_box_left + rename_box_width, rename_min.y));
            PushExplorerRenameStyle();
            DrawRenameInput();
            PopExplorerRenameStyle();
          } else {
            const ExplorerRowAction action = ShowExplorerRow(
                "##ExplorerGraphRow", graph->getName().c_str(), 1,
                graph_selected, true, graph_open);
            // 点 chevron 只切换展开。
            if (action == ExplorerRowAction::Toggle) {
              if (graph_open) {
                _collapsed_graphs.insert(graph.get());
              } else {
                _collapsed_graphs.erase(graph.get());
              }
            }
            // 点图名打开编辑区页签，并清掉节点选中，Delete 才删图而不是删节点。
            if (action == ExplorerRowAction::Activate) {
              OpenGraphInEditor(graph);
              selected_node.reset();
              _focus.ClearNodeSelection();
            }
            // Begin / End：图右键菜单。New Node、Cut、Copy、Paste、Run、Rename、Delete。
            if (ImGui::BeginPopupContextItem("GraphContext")) {
              OpenGraphInEditor(graph);
              selected_node.reset();
              _focus.ClearNodeSelection();
              // 选 New Node 才记下目标图，稍后打开种类列表。
              if (ImGui::MenuItem("New Node")) {
                RequestOpenPickNodeKind(graph_index);
              }
              ImGui::Separator();
              // 剪切/删除/粘贴改图列表，遍历结束后再执行，避免边扫边改。
              if (ImGui::MenuItem("Cut")) {
                _graph_pending_remove = graph;
                _graph_pending_cut = true;
              }
              if (ImGui::MenuItem("Copy")) {
                _graph_clipboard = CaptureGraph(graph);
                _graph_clipboard.was_cut = false;
              }
              if (ImGui::MenuItem("Paste", nullptr, false,
                                  !_graph_clipboard.empty())) {
                _graph_paste_pending = true;
              }
              ImGui::Separator();
              // 选 Run 跑这张图（不必是当前选中的图）。
              if (ImGui::MenuItem("Run")) {
                RunGraph(graph);
              }
              // 选 Rename 就地改图名。
              if (ImGui::MenuItem("Rename", kEditorRenameShortcut)) {
                BeginRenameGraph(graph);
              }
              // 选 Delete 才记下要删的图，遍历结束后再删。
              if (ImGui::MenuItem("Delete", kEditorDeleteShortcut)) {
                _graph_pending_remove = graph;
                _graph_pending_cut = false;
              }
              ImGui::EndPopup();
            }
          }
          row_y += row_height;
          // 展开后才列出这张图上已经创建的节点。
          if (graph_open) {
            bool node_removed = false;
            for (const auto& node : graph->getNodes()) {
              ImGui::PushID(node.get());
              const bool node_is_selected =
                  node.get() == _focus.selected_node();
              ImGui::SetCursorScreenPos(ImVec2(header_min.x, row_y));
              // 正在改这个节点的名字时画整宽输入框。
              if (_rename_node.get() == node.get()) {
                const ImVec2 rename_min = ImGui::GetCursorScreenPos();
                const float rename_width = ImGui::GetContentRegionAvail().x;
                const ImVec2 rename_max(rename_min.x + rename_width,
                                        rename_min.y + row_height);
                ImDrawList* draw = ImGui::GetWindowDrawList();
                DrawExplorerRowBackground(draw, rename_min, rename_max, 2,
                                          node_is_selected, false,
                                          ImGui::IsWindowFocused(), palette);
                const float rename_box_left =
                    rename_min.x +
                    (kExplorerTreeBasePaddingEpx +
                     kExplorerTreeIndentEpx * 2) * ui_scale;
                ImGui::SetCursorScreenPos(ImVec2(rename_box_left, rename_min.y));
                PushExplorerRenameStyle();
                DrawRenameInput();
                PopExplorerRenameStyle();
                ImGui::PopID();
                row_y += row_height;
                continue;
              }
              const ExplorerRowAction node_action = ShowExplorerRow(
                  "##ExplorerNodeRow", node->getName().c_str(), 2,
                  node_is_selected, false, false);
              // 点中侧栏里的节点就打开该图编辑区，并让属性表跟着它。
              if (node_action == ExplorerRowAction::Activate) {
                OpenGraphInEditor(graph);
                selected_node = node;
                _focus.SelectNode(node.get());
              }
              // identifier 与 kind 不同时，悬停行提示 kind，改名后仍能看出功能。
              if (node->typeId() != node->getName() && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", node->typeId().c_str());
              }
              // Begin / End：节点右键菜单。Cut、Copy、Rename、Delete。
              if (ImGui::BeginPopupContextItem("NodeContext")) {
                OpenGraphInEditor(graph);
                selected_node = node;
                _focus.SelectNode(node.get());
                // 选 Cut 复制到剪贴板再删，随后结束本轮节点遍历。
                if (ImGui::MenuItem("Cut", kEditorCutShortcut)) {
                  ApplyClipboardWithSamples(NodeClipboardCommand::Cut, graph,
                                            {SampleNode(node)}, {});
                  node_removed = true;
                }
                // 选 Copy 只写剪贴板，不改图。
                if (ImGui::MenuItem("Copy", kEditorCopyShortcut)) {
                  ApplyClipboardWithSamples(NodeClipboardCommand::Copy, graph,
                                            {SampleNode(node)}, {});
                }
                // 选 Rename 就地改节点名。
                if (ImGui::MenuItem("Rename", kEditorRenameShortcut)) {
                  BeginRenameNode(graph, node);
                }
                // 选 Delete 才从图里删掉这个节点，并结束本轮节点遍历。
                if (ImGui::MenuItem("Delete", kEditorDeleteShortcut)) {
                  DeleteSamplesFromUi(graph, {SampleNode(node)}, &selected_node);
                  node_removed = true;
                }
                ImGui::EndPopup();
              }
              ImGui::PopID();
              row_y += row_height;
              // 刚删过节点，节点列表迭代器不能再往下用。
              if (node_removed) {
                break;
              }
            }
          }
          ImGui::PopID();
        }
        // 让光标停在树下面，后续控件不叠在最后一行上。超出已提交底边时补一项，End 才不断言。
        if (has_project) {
          const float content_max_y = ImGui::GetCurrentWindow()->DC.CursorMaxPos.y;
          ImGui::SetCursorScreenPos(ImVec2(header_min.x, row_y));
          if (ExplorerCursorNeedsTrailingItem(row_y, content_max_y)) {
            ImGui::Dummy(ImVec2(0.0f, 0.0f));
          }
        }

        // 图菜单里选了 Paste / Cut / Delete，扫完树之后再改图列表。
        if (_graph_paste_pending) {
          _graph_paste_pending = false;
          PasteGraphFromUi();
        }
        if (_graph_pending_remove != nullptr) {
          if (_graph_pending_cut) {
            CutGraphFromUi(_graph_pending_remove);
          } else {
            DeleteGraphFromUi(_graph_pending_remove);
          }
          _graph_pending_remove.reset();
          _graph_pending_cut = false;
        }
        // 右键菜单或画布里选了 New Node，在 Explorer 窗口上打开种类列表。
        if (_focus.TakeOpenPickPopup()) {
          ImGui::OpenPopup(kPickNodeKindPopupId);
        }
        // Begin / End：节点种类选择浮层。
        ShowPickNodeKindPopup(graphs);
        ImGui::PopItemWidth();
        }
        PopExplorerTreeStyle();
      }
      ImGui::End();
      ImGui::PopStyleVar();
      PopSideBarStyle();
    }

    // Begin / End：编辑区。Welcome 或 graph 页签；全关则显示打开指引。
    // 对照 VS Code：编辑区内容贴边，不用全局 WindowPadding（kSurfaceTextMargin）。
    // 最大化时编辑区让位给目标视图。
    if (_maximized_view == MaximizedView::None) {
      HideDockTab("Canvas");
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
      if (ImGui::Begin("Canvas")) {
        DrawEditorTabs();
        if (_focus.open_editors().empty()) {
          ShowEmptyEditorPrompt();
        } else if (_focus.active_editor() >= 0 &&
                   _focus.active_editor() <
                       static_cast<int>(_focus.open_editors().size())) {
          const EditorTab::Kind active_kind =
              _focus.open_editors()[_focus.active_editor()].kind;
          if (active_kind == EditorTab::Kind::Welcome) {
            ShowWelcomePage();
          } else if (active_kind == EditorTab::Kind::KeyboardShortcuts) {
            ShowKeyboardShortcutsPage();
          } else {
            ++_canvas_content_wait_frames;
            const bool canvas_settled = NoteCanvasContentSize(
                ToVec2(ImGui::GetContentRegionAvail()), _canvas_content_size,
                _canvas_content_stable_frames);
            if (CanvasEditorMayBegin(canvas_settled,
                                     _canvas_content_wait_frames,
                                     _editor != nullptr)) {
              EnsureFlowEditor();
              ShowFlowCanvas();
            }
          }
        }
      }
      ImGui::End();
      ImGui::PopStyleVar();
    }

    // 画布读完选中后，再按最终节点找属性，侧栏点的节点不会被画布清掉。
    if (_focus.selected_node() != nullptr &&
        (selected_node == nullptr ||
         selected_node.get() != _focus.selected_node()) &&
        !graphs.empty()) {
      selected_node.reset();
      for (const auto& node : graphs[_focus.selected_graph_index()]->getNodes()) {
        // 地址相同就是当前要显示属性的节点。
        if (node.get() == _focus.selected_node()) {
          selected_node = node;
          break;
        }
      }
    }

    // Begin / End：辅助侧栏属性表（对照 Secondary Side Bar，保留停靠页签）。
    // 最大化 Output 时属性表让位。
    if (_parts.secondary_sidebar_visible &&
        _maximized_view != MaximizedView::Output) {
      ShowDockTab("Properties");
      PushPropertiesPanelStyle();
      if (ImGui::Begin("Properties")) {
        // 页签栏右侧：最大化/还原与关闭。
        const DockTitleActionPress title_press = DrawDockTitleActions(
            ImGui::GetWindowDockNode(),
            _maximized_view == MaximizedView::Properties,
            kPropertiesMaximizeTooltip, kPropertiesRestoreTooltip,
            kPropertiesCloseTooltip);
        if (title_press.toggle_maximize) {
          SetMaximizedView(ToggleMaximizedView(_maximized_view,
                                               MaximizedView::Properties));
        }
        if (title_press.close) {
          SetSecondarySideBarVisible(false);
        }
        constexpr int kPropertyTableColumnCount = 3;
        constexpr float kFillColumnWidth = -FLT_MIN;
        constexpr float kPropertyNameColumnRatio = 0.34f;
        constexpr float kPropertyTypeColumnRatio = 0.22f;
        constexpr float kPropertyValueColumnRatio =
            1.0f - kPropertyNameColumnRatio - kPropertyTypeColumnRatio;
        PushPropertiesContentStyle();
        const theme_detail::ThemeTokens& property_tokens =
            theme_detail::CurrentTokens();
        std::shared_ptr<tg::Node> link_source;
        const tg::ConnectionInfo* const selected_link = _focus.selected_link();
        const bool show_link =
            selected_link != nullptr && !graphs.empty() &&
            FindOutgoingConnection(*graphs[_focus.selected_graph_index()],
                                   selected_link, link_source);
        if (selected_link != nullptr && !show_link) {
          _focus.ClearSelectedLink();
        }
        // 选中连线时显示它代表的变量；否则有选中节点才画表。
        if (show_link) {
          const tg::Node::Port* const output =
              link_source->outputPort(selected_link->port_out);
          const std::shared_ptr<tg::NodeData> data =
              link_source->getOutPortData(selected_link->port_out);
          const LinkVariableFields fields =
              DescribeLinkVariable(output, data.get());
          constexpr int kLinkVariableColumnCount = 2;
          constexpr float kLinkVariableValueColumnRatio =
              1.0f - kPropertyTypeColumnRatio;
          if (ImGui::BeginTable(
                  "link_variable", kLinkVariableColumnCount,
                  ImGuiTableFlags_SizingStretchProp |
                      ImGuiTableFlags_BordersInnerH |
                      ImGuiTableFlags_NoPadOuterX)) {
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch,
                                    kPropertyTypeColumnRatio);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch,
                                    kLinkVariableValueColumnRatio);
            ImGui::TableHeadersRow();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ToImVec4(property_tokens.editor_type_foreground));
            ImGui::TextUnformatted(fields.type_name.c_str());
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(1);
            ImGui::AlignTextToFramePadding();
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            if (fields.has_value) {
              if (const auto* number = std::get_if<int>(&fields.value)) {
                ImGui::Text("%d", *number);
              } else if (const auto* text =
                             std::get_if<std::string>(&fields.value)) {
                ImGui::TextUnformatted(text->c_str());
              } else if (const auto* flag = std::get_if<bool>(&fields.value)) {
                ImGui::TextUnformatted(*flag ? "true" : "false");
              }
            }
            ImGui::PopStyleColor();
            ImGui::EndTable();
          }
        } else if (selected_node != nullptr) {
          ImGui::PushID(selected_node.get());
          // 属性提交后刷新端口类型；类型变化会断开不兼容的边。
          const auto commit_property_edit = [&]() {
            if (!graphs.empty()) {
              graphs[_focus.selected_graph_index()]->refreshPortTypes(
                  selected_node);
            }
            _controller.markDirty();
          };
          // 立即生效的控件（勾选、枚举、Browse）先回旧值再交给命令写新值。
          const auto record_property_edit = [&](int property_index,
                                                const tg::PropertyValue& before) {
            if (graphs.empty()) {
              commit_property_edit();
              return;
            }
            const auto properties = selected_node->getEditableProperties();
            if (property_index < 0 ||
                property_index >= static_cast<int>(properties.size())) {
              return;
            }
            RecordPropertyEdit(
                *graphs[_focus.selected_graph_index()], selected_node,
                property_index, before,
                properties[static_cast<std::size_t>(property_index)]->value);
          };
          // 文本/数字框失焦；旧值在 IsItemActivated 时已记进 _property_edit。
          const auto finish_property_edit = [&]() {
            if (graphs.empty()) {
              _property_edit = ImGuiPropertyEditState{};
              return;
            }
            FinishPropertyEdit(graphs[_focus.selected_graph_index()]);
          };
          // 换了选中节点，上一条未提交的编辑快照作废。
          if (_property_edit.active &&
              _property_edit.node != selected_node.get()) {
            _property_edit = ImGuiPropertyEditState{};
          }
          // 表头 + 行间发丝线；列含 Name / Type / Value。
          if (ImGui::BeginTable(
                  "node_properties", kPropertyTableColumnCount,
                  ImGuiTableFlags_SizingStretchProp |
                      ImGuiTableFlags_BordersInnerH |
                      ImGuiTableFlags_NoPadOuterX)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch,
                                    kPropertyNameColumnRatio);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch,
                                    kPropertyTypeColumnRatio);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch,
                                    kPropertyValueColumnRatio);
            ImGui::TableHeadersRow();
            int property_index = 0;
            // 原生属性 + 全部输入槽：口模式只读，属性模式可改。
            for (const auto& property : selected_node->getEditableProperties()) {
              // 端口类型选择器不进属性表。
              if (property->hints.placement == tg::PropertyPlacement::Port) {
                ++property_index;
                continue;
              }
              ImGui::TableNextRow();
              ImGui::TableSetColumnIndex(0);
              ImGui::AlignTextToFramePadding();
              ImGui::PushStyleColor(ImGuiCol_Text,
                                    ToImVec4(property_tokens.editor_foreground));
              ImGui::TextUnformatted(property->name.c_str());
              ImGui::PopStyleColor();
              const bool bound_input =
                  property->bound_input_index !=
                  tg::Node::kNativePropertyInputIndex;
              const bool input_as_port =
                  bound_input &&
                  selected_node->isInputShownAsPort(
                      static_cast<unsigned int>(property->bound_input_index));
              ImGui::TableSetColumnIndex(1);
              ImGui::AlignTextToFramePadding();
              ImGui::PushStyleColor(ImGuiCol_Text,
                                    ToImVec4(property_tokens.editor_type_foreground));
              // Enum 优先；原生属性按 hints.editor，其余回退存储/端口类型。
              if (property->hints.editor == tg::PropertyEditor::Enum) {
                ImGui::TextUnformatted("enum");
              } else if (bound_input) {
                const tg::Node::Port* port = selected_node->inputPort(
                    static_cast<unsigned int>(property->bound_input_index));
                if (port != nullptr) {
                  ImGui::TextUnformatted(port->data_type.name.c_str());
                }
              } else if (property->hints.editor ==
                         tg::PropertyEditor::Integer) {
                ImGui::TextUnformatted("int");
              } else if (property->hints.editor ==
                         tg::PropertyEditor::Double) {
                ImGui::TextUnformatted("double");
              } else if (property->hints.editor == tg::PropertyEditor::Text) {
                ImGui::TextUnformatted("string");
              } else if (property->hints.editor == tg::PropertyEditor::File) {
                ImGui::TextUnformatted("file");
              } else if (property->hints.editor ==
                         tg::PropertyEditor::Directory) {
                ImGui::TextUnformatted("directory");
              } else if (std::holds_alternative<int>(property->value)) {
                ImGui::TextUnformatted("int");
              } else if (std::holds_alternative<double>(property->value)) {
                ImGui::TextUnformatted("double");
              } else if (std::holds_alternative<std::string>(property->value)) {
                ImGui::TextUnformatted("string");
              } else if (std::holds_alternative<bool>(property->value)) {
                ImGui::TextUnformatted("bool");
              }
              ImGui::PopStyleColor();
              ImGui::TableSetColumnIndex(2);
              ImGui::PushID(property_index);
              // 端口露面：属性表仍列出，但不可改（对照输入口即属性）。
              if (input_as_port) {
                ImGui::AlignTextToFramePadding();
                ImGui::PushStyleColor(
                    ImGuiCol_Text,
                    ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                if (const auto* number = std::get_if<int>(&property->value)) {
                  ImGui::Text("%d", *number);
                } else if (const auto* real =
                               std::get_if<double>(&property->value)) {
                  ImGui::Text("%g", *real);
                } else if (const auto* text =
                               std::get_if<std::string>(&property->value)) {
                  ImGui::TextUnformatted(text->c_str());
                } else if (const auto* flag =
                               std::get_if<bool>(&property->value)) {
                  ImGui::TextUnformatted(*flag ? "true" : "false");
                }
                ImGui::PopStyleColor();
              } else if (auto* number = std::get_if<int>(&property->value)) {
                const tg::PropertyHints& hints = property->hints;
                const bool is_enum =
                    hints.editor == tg::PropertyEditor::Enum &&
                    !hints.choices.empty();
                // 枚举：choices 拉成组合框，值仍以 int 下标存储。
                if (is_enum) {
                  const int choice_count =
                      static_cast<int>(hints.choices.size());
                  const char* preview =
                      (*number >= 0 && *number < choice_count)
                          ? hints.choices[static_cast<std::size_t>(*number)]
                                .c_str()
                          : "";
                  ImGui::SetNextItemWidth(kFillColumnWidth);
                  const ThemedComboState value_combo =
                      BeginThemedCombo("##value", preview);
                  if (value_combo.open) {
                    for (int choice = 0; choice < choice_count; ++choice) {
                      const bool selected = choice == *number;
                      if (ImGui::Selectable(
                              hints.choices[static_cast<std::size_t>(choice)]
                                  .c_str(),
                              selected)) {
                        const tg::PropertyValue before = property->value;
                        *number = choice;
                        record_property_edit(property_index, before);
                      }
                      if (selected) {
                        ImGui::SetItemDefaultFocus();
                      }
                    }
                  }
                  EndThemedCombo(value_combo);
                } else {
                  // 整数：无 +/- 步进按钮。
                  constexpr int kIntInputNoStep = 0;
                  ImGui::SetNextItemWidth(kFillColumnWidth);
                  ImGui::InputInt("##value", number, kIntInputNoStep);
                  // 进入编辑时记下旧值，失焦后提交给撤销栈。
                  if (ImGui::IsItemActivated()) {
                    _property_edit.node = selected_node.get();
                    _property_edit.property_index = property_index;
                    _property_edit.old_value = property->value;
                    _property_edit.active = true;
                  }
                  if (ImGui::IsItemDeactivatedAfterEdit()) {
                    finish_property_edit();
                  }
                }
              } else if (auto* real = std::get_if<double>(&property->value)) {
                const tg::PropertyHints& hints = property->hints;
                // 0 = 不画步进按钮；VS Code 的数字框只有输入。
                constexpr double kDoubleNoStep = 0.0;
                constexpr int kDoubleFormatCapacity = 32;
                constexpr const char* kDoubleAutoFormat = "%g";
                // decimals >= 0 时用定点格式，否则沿用 %g。
                char format[kDoubleFormatCapacity];
                if (hints.decimals >= 0) {
                  std::snprintf(format, sizeof(format), "%%.%df",
                                hints.decimals);
                } else {
                  std::snprintf(format, sizeof(format), "%s",
                                kDoubleAutoFormat);
                }
                ImGui::SetNextItemWidth(kFillColumnWidth);
                ImGui::InputDouble("##value", real, kDoubleNoStep,
                                   kDoubleNoStep, format);
                // 进入编辑时记下旧值，失焦后提交给撤销栈。
                if (ImGui::IsItemActivated()) {
                  _property_edit.node = selected_node.get();
                  _property_edit.property_index = property_index;
                  _property_edit.old_value = property->value;
                  _property_edit.active = true;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                  // 只有给了正区间（max > min）时才夹到提示范围。
                  if (hints.max > hints.min) {
                    *real = std::clamp(*real, hints.min, hints.max);
                  }
                  finish_property_edit();
                }
              } else if (auto* text = std::get_if<std::string>(&property->value)) {
                // 字符串：Text 不显示 Browse；File/Directory/Auto 可按提示浏览。
                const tg::PropertyHints& hints = property->hints;
                const bool allow_browse =
                    hints.editor != tg::PropertyEditor::Text;
                const bool pick_directory =
                    hints.editor == tg::PropertyEditor::Directory;
                constexpr std::size_t kTextTerminatorSize = 1;
                const std::size_t text_buffer_size =
                    text->capacity() + kTextTerminatorSize;
                // 路径框独占值列，不让同行按钮把它压窄。
                ImGui::SetNextItemWidth(kFillColumnWidth);
                ImGui::InputText("##value", text->data(), text_buffer_size,
                                 ImGuiInputTextFlags_CallbackResize,
                                 ResizePropertyText, text);
                // 进入编辑时记下旧值，失焦后提交给撤销栈。
                if (ImGui::IsItemActivated()) {
                  _property_edit.node = selected_node.get();
                  _property_edit.property_index = property_index;
                  _property_edit.old_value = property->value;
                  _property_edit.active = true;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                  finish_property_edit();
                }
                if (allow_browse) {
                  // 浏览按钮换到下一行右对齐，窄侧栏下也留得下完整路径框。
                  const float button_width = ButtonWidth(kBrowseActionLabel);
                  const float cell_x = ImGui::GetCursorPosX();
                  const float right_x = cell_x +
                                        ImGui::GetContentRegionAvail().x -
                                        button_width;
                  ImGui::SetCursorPosX((std::max)(right_x, cell_x));
                  if (Button(kBrowseActionLabel)) {
                    const tg::PropertyValue before = property->value;
                    std::string context = *text;
                    if (context.empty() && _controller.hasProjectRoot()) {
                      context = _controller.projectRoot().string();
                    }
                    const std::string default_path =
                        NativePathDialog::resolveDefaultPath(
                            context, _session.last_browse_dir);
                    const std::optional<std::string> picked =
                        pick_directory
                            ? NativePathDialog::pickDirectory(
                                  kPickPropertyDirectoryTitle, default_path)
                            : NativePathDialog::pickOpenFile(
                                  kPickPropertyFileTitle, default_path);
                    if (NativePathDialog::applyPickedPath(*text, picked)) {
                      RememberBrowseDir(*picked);
                      record_property_edit(property_index, before);
                    }
                  }
                }
              } else if (auto* flag = std::get_if<bool>(&property->value)) {
                const tg::PropertyValue before = property->value;
                if (DrawThemedCheckbox("##value", "", flag,
                                       ImGui::GetStyle().FontScaleMain)) {
                  record_property_edit(property_index, before);
                }
              }
              // 绑定输入槽：口模式右键改为属性，属性模式右键改为端口。
              if (bound_input &&
                  ImGui::BeginPopupContextItem("input_presentation_menu")) {
                if (input_as_port) {
                  const bool can_convert =
                      selected_node->allowsPropertyPresentation(
                          static_cast<unsigned int>(property->bound_input_index));
                  // 菜单用英文：当前字体无中文 glyph，中文会显示成问号。
                  if (can_convert && ImGui::MenuItem("Convert to Property") &&
                      !graphs.empty()) {
                    ConvertInputPresentation(
                        graphs[_focus.selected_graph_index()], selected_node,
                        static_cast<unsigned int>(property->bound_input_index),
                        tg::InputPresentation::Property);
                  }
                } else if (ImGui::MenuItem("Convert to Port") &&
                           !graphs.empty()) {
                  ConvertInputPresentation(
                      graphs[_focus.selected_graph_index()], selected_node,
                      static_cast<unsigned int>(property->bound_input_index),
                      tg::InputPresentation::Port);
                }
                ImGui::EndPopup();
              }
              ImGui::PopID();
              ++property_index;
            }
            ImGui::EndTable();
          }
          ImGui::PopID();
        }
        PopPropertiesContentStyle();
      }
      ImGui::End();
      PopPropertiesPanelStyle();
    }

    // 主侧栏有焦点、且不在改文本时：F2 改名，Delete/Backspace 删除，
    // Ctrl+C/X/V 剪贴板（节点与图按选中项区分，与右键菜单一致）。
    // 编辑区里的 Delete 留给画布，不能在这里删图。
    const ImGuiIO& explorer_io = ImGui::GetIO();
    const bool clipboard_modifier = explorer_io.KeyCtrl || explorer_io.KeySuper;
    const ExplorerShortcutInput explorer_keys{
        explorer_focused,
        explorer_io.WantTextInput,
        ImGui::IsKeyPressed(ImGuiKey_F2),
        ImGui::IsKeyPressed(ImGuiKey_Delete) ||
            ImGui::IsKeyPressed(ImGuiKey_Backspace),
        clipboard_modifier && ImGui::IsKeyPressed(ImGuiKey_X),
        clipboard_modifier && ImGui::IsKeyPressed(ImGuiKey_C),
        clipboard_modifier && ImGui::IsKeyPressed(ImGuiKey_V),
        _focus.selected_node() != nullptr,
        !graphs.empty(),
    };
    switch (DecideExplorerShortcut(explorer_keys)) {
      case ExplorerShortcutAction::RenameNode:
        BeginRenameSelectedNode();
        break;
      case ExplorerShortcutAction::RenameGraph:
        BeginRenameGraph(graphs[_focus.selected_graph_index()]);
        break;
      case ExplorerShortcutAction::DeleteNode:
        RemoveSelectedNode(&selected_node);
        break;
      case ExplorerShortcutAction::DeleteGraph:
        DeleteGraphFromUi(graphs[_focus.selected_graph_index()]);
        break;
      case ExplorerShortcutAction::CopyNode:
        ApplyNodeClipboardShortcut(NodeClipboardCommand::Copy);
        break;
      case ExplorerShortcutAction::CutNode:
        ApplyNodeClipboardShortcut(NodeClipboardCommand::Cut);
        break;
      case ExplorerShortcutAction::CopyGraph:
        CopyGraphShortcut();
        break;
      case ExplorerShortcutAction::CutGraph:
        CutGraphFromUi(graphs[_focus.selected_graph_index()]);
        break;
      case ExplorerShortcutAction::PasteGraph:
        PasteGraphFromUi();
        break;
      case ExplorerShortcutAction::None:
        break;
    }

    // Begin / End：面板视图。默认只有 Output，多注册即多个停靠页签；面板可见时才画。
    if (_parts.panel_visible) {
      ShowPanels();
    }
    // Begin / End：独立视图窗口，不占面板停靠格。
    ShowViewWindows();
    // Begin / End：节点输出监视窗口；浮动时标题栏叠动作，停靠时动作放内容首行。
    ShowMonitors(chrome);

    // Begin / End：状态栏。通栏贴底，不进停靠。左=工程名 + Run + 当前图，右=选中节点。
    if (chrome.status_size.y > 0.0f) {
      ImGui::SetNextWindowPos(ToImVec2(chrome.status_pos));
      ImGui::SetNextWindowSize(ToImVec2(chrome.status_size));
      ImGui::SetNextWindowViewport(viewport->ID);
      PushStatusBarStyle();
      const ImGuiWindowFlags status_flags =
          ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
          ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
          ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
          ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBringToFrontOnFocus;
      if (ImGui::Begin("Status Bar", nullptr, status_flags)) {
      // 有工程时左侧先画工程名。
      if (_controller.hasProjectRoot()) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(_controller.projectName().c_str());
        ImGui::SameLine(0.0f, kBetweenControlsEpx * ui_scale);
      }
      // Begin / End：没有图时禁用 Run，列表为空没有任务可跑。
      ImGui::BeginDisabled(!_parts.commands.Enabled("graph.run"));
      // 点状态栏上的 Run 才跑当前图。
      if (DrawBarAction("Run")) {
        ExecuteCommand("graph.run");
      }
      ImGui::EndDisabled();
      // 有图才显示图名，点开后改当前图。
      if (!graphs.empty()) {
        ImGui::SameLine();
        // 点图名才弹出列表，用来改状态栏上的当前图。
        if (DrawBarAction(graphs[_focus.selected_graph_index()]->getName().c_str())) {
          ImGui::OpenPopup("PickStatusGraph");
        }
        // Begin / End：状态栏上的图列表。点中一张就切到它。
        if (ImGui::BeginPopup("PickStatusGraph")) {
          for (int graph_index = 0;
               graph_index < static_cast<int>(graphs.size()); ++graph_index) {
            ImGui::PushID(graph_index);
            // 点中一张图才切换，节点不属于这张图时清掉选中。
            if (ImGui::Selectable(graphs[graph_index]->getName().c_str(),
                                  graph_index == _focus.selected_graph_index())) {
              OpenGraphInEditor(graphs[graph_index]);
              bool node_on_this_graph = false;
              // 选中节点还在这张图里才保留，否则属性表会显示别的图。
              if (selected_node != nullptr) {
                for (const auto& node : graphs[graph_index]->getNodes()) {
                  // 地址相同就是当前选中的节点。
                  if (node.get() == selected_node.get()) {
                    node_on_this_graph = true;
                    break;
                  }
                }
              }
              // 这张图里没有刚才选中的节点。
              if (!node_on_this_graph) {
                selected_node.reset();
                _focus.ClearNodeSelection();
              }
            }
            ImGui::PopID();
          }
          ImGui::EndPopup();
        }
      }
      // 最近一次成功 run 的总耗时；没有成功记录时不显示。
      if (const std::optional<double> last_run =
              _controller.lastRunMilliseconds()) {
        ImGui::SameLine(0.0f, kBetweenControlsEpx * ui_scale);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Last run: %.2f ms", *last_run);
      }
      // 选中了节点才把名字放到右侧。
      if (selected_node != nullptr) {
        const std::string& node_name = selected_node->getName();
        const float name_width = ImGui::CalcTextSize(node_name.c_str()).x;
        const float name_x = ImGui::GetWindowContentRegionMax().x - name_width;
        ImGui::SameLine();
        ImGui::SetCursorPosX(name_x);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(node_name.c_str());
      }
      // 状态栏顶边 1px，切主题时跟着 statusBar.border 变。
      const ImVec2 status_bar_min = ImGui::GetWindowPos();
      ImGui::GetWindowDrawList()->AddRectFilled(
          status_bar_min,
          ImVec2(status_bar_min.x + ImGui::GetWindowWidth(),
                 status_bar_min.y + kEditorTabSeparatorThicknessEpx * ui_scale),
          ImGui::ColorConvertFloat4ToU32(
              ToImVec4(theme_detail::CurrentTokens().status_bar_border)));
    }
      ImGui::End();
      PopStatusBarStyle();
    }
    // 撤销/重做每帧末尾统一处理一次，覆盖主侧栏与画布（图 + 节点统一排序）。
    HandleGlobalUndoRedoShortcuts();
    HandleUndecoratedWindowChrome();
  }

  // 画窗口自己的标题栏：标题、左侧菜单栏 Run、最小化、最大化、关闭，不进停靠。
  void ShowWindowTitleBar(
      const std::vector<std::shared_ptr<tg::Graph>>& graphs) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float ui_scale = ImGui::GetStyle().FontScaleMain;
    const float title_bar_height = kTitleBarHeightEpx * ui_scale;
    const float caption_width =
        kWindowCaptionButtonWidthEpx * ui_scale *
        static_cast<float>(kWindowCaptionButtonCount);
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, title_bar_height));
    ImGui::SetNextWindowViewport(viewport->ID);
    PushTitleBarStyle();
    const ImGuiWindowFlags title_flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBringToFrontOnFocus;
    // Begin / End：窗口标题栏。左 File→View→Run→Help，中命令中心，右窗口按钮。
    ImGui::Begin("Window Title Bar", nullptr, title_flags);
    ImGui::AlignTextToFramePadding();
    // 标题栏菜单栏从左到右：File、View、Run、Help。不要活动栏，不要第二条工具栏。
    if (DrawBarAction("File")) {
      ImGui::OpenPopup("##TitleFileMenu");
    }
    if (ImGui::BeginPopup("##TitleFileMenu")) {
      if (ImGui::MenuItem("New Project...")) {
        ExecuteCommand("project.new");
      }
      if (ImGui::MenuItem("Open Project...")) {
        ExecuteCommand("project.open");
      }
      if (ImGui::BeginMenu("Open Recent", !_session.recent_roots.empty())) {
        for (std::size_t recent_index = 0;
             recent_index < _session.recent_roots.size(); ++recent_index) {
          ImGui::PushID(static_cast<int>(recent_index));
          const std::string& recent_root = _session.recent_roots[recent_index];
          if (ImGui::MenuItem(recent_root.c_str())) {
            OpenProjectFromPath(std::filesystem::path(recent_root));
          }
          ImGui::PopID();
        }
        ImGui::EndMenu();
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Save", nullptr, false,
                          _parts.commands.Enabled("project.save"))) {
        ExecuteCommand("project.save");
      }
      if (ImGui::MenuItem("Save As...")) {
        ExecuteCommand("project.saveAs");
      }
      ImGui::Separator();
      if (ImGui::BeginMenu("Preferences")) {
        if (ImGui::BeginMenu("Theme")) {
          for (const Theme& theme : ::AvailableThemes()) {
            const bool selected = theme.id == ::CurrentThemeId();
            // 点当前主题不重复写会话。
            if (ImGui::MenuItem(theme.display_name.c_str(), nullptr, selected)) {
              SetTheme(theme.id);
            }
          }
          ImGui::EndMenu();
        }
        if (ImGui::MenuItem(kKeyboardShortcutsTabLabel)) {
          OpenKeyboardShortcutsInEditor();
        }
        ImGui::EndMenu();
      }
      ImGui::EndPopup();
    }
    ImGui::SameLine(0.0f, kBetweenControlsEpx * ui_scale);
    // 点菜单名 Edit 打开菜单。
    if (DrawBarAction("Edit")) {
      ImGui::OpenPopup("##TitleEditMenu");
    }
    // Begin / End：Edit 菜单。撤销/重做走全局栈（图 + 节点统一排序），其余作用于选中节点。
    if (ImGui::BeginPopup("##TitleEditMenu")) {
      std::shared_ptr<tg::Graph> edit_graph = ActiveEditorGraph();
      if (ImGui::MenuItem("Undo", kEditorUndoShortcut, false, CanUndoAny())) {
        Undo();
      }
      if (ImGui::MenuItem("Redo", kEditorRedoShortcut, false, CanRedoAny())) {
        Redo();
      }
      ImGui::Separator();
      std::vector<ClipboardSample> menu_samples;
      // 菜单没有画布的多选，只用主侧栏/画布共同的那一个选中节点。
      if (edit_graph != nullptr && _focus.selected_node() != nullptr) {
        const std::shared_ptr<tg::Node> owned =
            FindNodeInGraph(edit_graph, _focus.selected_node());
        if (owned != nullptr) {
          menu_samples.push_back(SampleNode(owned));
        }
      }
      const bool has_selection = !menu_samples.empty();
      if (ImGui::MenuItem("Cut", kEditorCutShortcut, false, has_selection)) {
        ApplyClipboardWithSamples(NodeClipboardCommand::Cut, edit_graph,
                                  menu_samples, {});
      }
      if (ImGui::MenuItem("Copy", kEditorCopyShortcut, false, has_selection)) {
        ApplyClipboardWithSamples(NodeClipboardCommand::Copy, edit_graph,
                                  menu_samples, {});
      }
      if (ImGui::MenuItem("Paste", kEditorPasteShortcut, false,
                          edit_graph != nullptr && !_node_clipboard.empty())) {
        ApplyClipboardWithSamples(NodeClipboardCommand::Paste, edit_graph, {},
                                  {});
      }
      if (ImGui::MenuItem("Delete", kEditorDeleteShortcut, false,
                          has_selection)) {
        DeleteSamplesFromUi(edit_graph, menu_samples);
      }
      ImGui::EndPopup();
    }
    ImGui::SameLine(0.0f, kBetweenControlsEpx * ui_scale);
    // 点菜单名 View 打开菜单。
    if (DrawBarAction("View")) {
      ImGui::OpenPopup("##TitleViewMenu");
    }
    // Begin / End：View 菜单。对照 VS Code：Appearance、侧栏视图、区域可见性、显示开关。
    if (ImGui::BeginPopup("##TitleViewMenu")) {
      if (ImGui::BeginMenu("Appearance")) {
        if (ImGui::MenuItem("Activity Bar", nullptr,
                            _parts.activity_bar_visible)) {
          ExecuteCommand("view.toggleActivityBar");
        }
        ImGui::EndMenu();
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Explorer", nullptr,
                          _focus.primary_side_bar() ==
                              PrimarySideBarView::Explorer)) {
        ExecuteCommand(kShowExplorerCommand);
      }
      if (ImGui::MenuItem("Search", nullptr,
                          _focus.primary_side_bar() ==
                              PrimarySideBarView::Search)) {
        ExecuteCommand(kShowSearchCommand);
      }
      if (ImGui::MenuItem("Properties", nullptr,
                          _parts.secondary_sidebar_visible)) {
        ExecuteCommand("view.toggleProperties");
      }
      if (ImGui::MenuItem("Output", nullptr, _parts.panel_visible)) {
        ExecuteCommand("view.toggleOutput");
      }
      if (!_parts.view_windows.empty()) {
        ImGui::Separator();
        for (const ViewWindow& view : _parts.view_windows) {
          const std::string command_id = view.command_id();
          if (ImGui::MenuItem(view.title.c_str(), nullptr, view.open,
                              _parts.commands.Enabled(command_id))) {
            ExecuteCommand(command_id);
          }
        }
      }
      if (!_parts.monitor_views.empty()) {
        ImGui::Separator();
        for (const MonitorView& view : _parts.monitor_views) {
          const std::string command_id = view.command_id();
          if (ImGui::MenuItem(view.title.c_str(), nullptr, view.open,
                              _parts.commands.Enabled(command_id))) {
            ExecuteCommand(command_id);
          }
        }
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Primary Side Bar", nullptr,
                          _parts.primary_sidebar_visible)) {
        ExecuteCommand(kTogglePrimarySideBarCommand);
      }
      if (ImGui::MenuItem("Status Bar", nullptr, _parts.status_bar_visible)) {
        ExecuteCommand("view.toggleStatusBar");
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Show Type Names", nullptr, _show_port_type_names)) {
        ExecuteCommand("editor.showTypeNames");
      }
      ImGui::EndPopup();
    }
    ImGui::SameLine(0.0f, kBetweenControlsEpx * ui_scale);
    // 点菜单名 Run 打开菜单。
    if (DrawBarAction("Run")) {
      ImGui::OpenPopup("##TitleRunMenu");
    }
    // Begin / End：Run 菜单。选 Run Graph 才跑当前图。
    if (ImGui::BeginPopup("##TitleRunMenu")) {
      // 列表为空没有任务可跑。
      if (ImGui::MenuItem("Run Graph", nullptr, false,
                          _parts.commands.Enabled("graph.run"))) {
        ExecuteCommand("graph.run");
      }
      ImGui::EndPopup();
    }
    ImGui::SameLine(0.0f, kBetweenControlsEpx * ui_scale);
    if (DrawBarAction("Help")) {
      ImGui::OpenPopup("##TitleHelpMenu");
    }
    if (ImGui::BeginPopup("##TitleHelpMenu")) {
      if (ImGui::MenuItem("Welcome")) {
        ExecuteCommand("help.welcome");
      }
      ImGui::EndPopup();
    }
    const float menu_right_x =
        ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x +
        kBetweenControlsEpx * ui_scale;
    const ImVec2 caption_size(kWindowCaptionButtonWidthEpx * ui_scale,
                              title_bar_height);
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - caption_width);
    ImGui::SetCursorPosY(0.0f);
    // 点最小化。
    if (DrawCaptionButton("##CaptionMinimize", CaptionGlyph::Minimize,
                          caption_size)) {
      glfwIconifyWindow(Window());
    }
    ImGui::SameLine(0.0f, 0.0f);
    const CaptionGlyph maximize_glyph =
        CoveringWorkArea() ? CaptionGlyph::Restore : CaptionGlyph::Maximize;
    // 点最大化或还原。
    if (DrawCaptionButton("##CaptionMaximize", maximize_glyph, caption_size)) {
      ToggleWorkAreaCover();
    }
    ImGui::SameLine(0.0f, 0.0f);
    // 点关闭。
    if (DrawCaptionButton("##CaptionClose", CaptionGlyph::Close,
                          caption_size)) {
      glfwSetWindowShouldClose(Window(), GLFW_TRUE);
    }
    // 任务栏窗口标题仍用完整文案；命令中心只显示工程名。
    const std::string window_title = BuildWindowTitleText();
    if (window_title != _last_window_title) {
      glfwSetWindowTitle(Window(), window_title.c_str());
      _last_window_title = window_title;
    }
    const char* command_center_label =
        _controller.hasProjectRoot() ? _controller.projectName().c_str()
                                     : kUntitledProjectLabel;
    const float content_min_x = ImGui::GetWindowContentRegionMin().x;
    const float content_max_x = ImGui::GetWindowContentRegionMax().x;
    const float title_right_limit =
        content_max_x - caption_width - kBetweenControlsEpx * ui_scale;
    const float frame_pad_x = ImGui::GetStyle().FramePadding.x;
    const float label_width = ImGui::CalcTextSize(command_center_label).x +
                              frame_pad_x * kFramePaddingSideCount;
    float command_center_width =
        (std::max)(kCommandCenterMinWidthEpx * ui_scale, label_width);
    // 命令中心接管者可以指定更宽的内容区（例如未来搜索框）。
    if (_parts.command_center.preferred_width_epx > 0.0f) {
      command_center_width =
          _parts.command_center.preferred_width_epx * ui_scale;
    }
    command_center_width =
        (std::min)(command_center_width, kCommandCenterMaxWidthEpx * ui_scale);
    const float available_width = title_right_limit - menu_right_x;
    if (available_width > 0.0f && command_center_width > available_width) {
      command_center_width = available_width;
    }
    constexpr float kHalf = 0.5f;
    float command_center_x =
        (content_min_x + content_max_x) * kHalf - command_center_width * kHalf;
    if (command_center_x < menu_right_x) {
      command_center_x = menu_right_x;
    }
    if (command_center_x + command_center_width > title_right_limit) {
      command_center_x = title_right_limit - command_center_width;
    }
    const float command_center_height = kCommandCenterHeightEpx * ui_scale;
    const float command_center_y =
        (title_bar_height - command_center_height) * kHalf;
    ImGui::SetCursorPos(ImVec2(command_center_x, command_center_y));
    // 有接管者就让它画内容；默认仍是无底无描边的工程名文字。
    if (_parts.command_center.draw) {
      _parts.command_center.draw(
          Vec2{command_center_width, command_center_height});
    } else {
      // 无底无描边，只露工程名文字，不画成可见的框。
      PushCommandCenterStyle();
      // 命令中心本轮只展示工程名，点击无行为（不接 Quick Pick）。
      ImGui::Button(command_center_label,
                    ImVec2(command_center_width, command_center_height));
      PopCommandCenterStyle();
    }
    const float border = kUndecoratedResizeBorderEpx * ui_scale;
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool on_title_edge =
        mouse.x <= viewport->WorkPos.x + border ||
        mouse.x >= viewport->WorkPos.x + viewport->WorkSize.x - border ||
        mouse.y <= viewport->WorkPos.y + border;
    // 标题文字和空白区可拖动，压在边缘时留给缩放，空白区双击切换铺满。
    if (!on_title_edge && !ImGui::IsAnyItemHovered() &&
        ImGui::IsWindowHovered(ImGuiHoveredFlags_None)) {
      // 双击空白区切换铺满和还原。
      if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        ToggleWorkAreaCover();
      } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                 !CoveringWorkArea()) {
        // 按下左键开始拖窗口，铺满时不拖。
        _dragging_window = true;
        double cursor_x = 0.0;
        double cursor_y = 0.0;
        glfwGetCursorPos(Window(), &cursor_x, &cursor_y);
        _drag_grab_x = static_cast<int>(cursor_x);
        _drag_grab_y = static_cast<int>(cursor_y);
      }
    }
    // 标题栏底边 1px，切主题时跟着 titleBar.border 变。
    const ImVec2 title_bar_min = ImGui::GetWindowPos();
    const float title_border_thickness =
        kEditorTabSeparatorThicknessEpx * ui_scale;
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(title_bar_min.x, title_bar_min.y + title_bar_height -
                                    title_border_thickness),
        ImVec2(title_bar_min.x + ImGui::GetWindowWidth(),
               title_bar_min.y + title_bar_height),
        ImGui::ColorConvertFloat4ToU32(
            ToImVec4(theme_detail::CurrentTokens().title_bar_border)));
    ImGui::End();
    PopTitleBarStyle();
  }

  // 无边框窗口：拖动标题栏移动，边缘拖动改大小。
  void HandleUndecoratedWindowChrome() {
    GLFWwindow* window = Window();
    // 松开左键就结束拖动和缩放。
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      _dragging_window = false;
      _resizing_window = false;
      _resize_left = false;
      _resize_right = false;
      _resize_top = false;
      _resize_bottom = false;
    }
    double cursor_x = 0.0;
    double cursor_y = 0.0;
    glfwGetCursorPos(window, &cursor_x, &cursor_y);
    int window_w = 0;
    int window_h = 0;
    glfwGetWindowSize(window, &window_w, &window_h);
    const float border = kUndecoratedResizeBorderEpx * ImGui::GetStyle().FontScaleMain;
    const float cursor_xf = static_cast<float>(cursor_x);
    const float cursor_yf = static_cast<float>(cursor_y);
    // 正在拖窗口时让抓取点跟着光标，不在这一帧再开缩放。
    if (_dragging_window) {
      int window_x = 0;
      int window_y = 0;
      glfwGetWindowPos(window, &window_x, &window_y);
      glfwSetWindowPos(window, window_x + static_cast<int>(cursor_x) - _drag_grab_x,
                       window_y + static_cast<int>(cursor_y) - _drag_grab_y);
      return;
    }
    // 正在缩放时按抓住的边改位置和大小。
    if (_resizing_window) {
      int window_x = 0;
      int window_y = 0;
      glfwGetWindowPos(window, &window_x, &window_y);
      int new_x = window_x;
      int new_y = window_y;
      int new_w = window_w;
      int new_h = window_h;
      const int screen_x = window_x + static_cast<int>(cursor_x);
      const int screen_y = window_y + static_cast<int>(cursor_y);
      // 左边：改 x 和宽度。
      if (_resize_left) {
        new_w = _resize_origin_w + (_resize_origin_x - screen_x);
        new_x = screen_x;
      }
      // 右边：只改宽度。
      if (_resize_right) {
        new_w = screen_x - window_x;
      }
      // 上边：改 y 和高度。
      if (_resize_top) {
        new_h = _resize_origin_h + (_resize_origin_y - screen_y);
        new_y = screen_y;
      }
      // 下边：只改高度。
      if (_resize_bottom) {
        new_h = screen_y - window_y;
      }
      // 宽度太窄时钉在最小值。
      if (new_w < kMinClientWidth) {
        // 从左边拉窄时，把左边钉住，别把右边也拖进来。
        if (_resize_left) {
          new_x = window_x + window_w - kMinClientWidth;
        }
        new_w = kMinClientWidth;
      }
      // 高度太矮时钉在最小值。
      if (new_h < kMinClientHeight) {
        // 从上边拉矮时，把上边钉住。
        if (_resize_top) {
          new_y = window_y + window_h - kMinClientHeight;
        }
        new_h = kMinClientHeight;
      }
      glfwSetWindowPos(window, new_x, new_y);
      glfwSetWindowSize(window, new_w, new_h);
      return;
    }
    // 铺满工作区时不响应边缘缩放。
    if (CoveringWorkArea()) {
      return;
    }
    const bool on_left = cursor_xf <= border;
    const bool on_right = cursor_xf >= static_cast<float>(window_w) - border;
    const bool on_top = cursor_yf <= border;
    const bool on_bottom = cursor_yf >= static_cast<float>(window_h) - border;
    const bool on_edge = on_left || on_right || on_top || on_bottom;
    // 没压在窗口外缘就不开始缩放。
    if (!on_edge) {
      return;
    }
    // 左键按下才开始按当前边缩放。
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      int window_x = 0;
      int window_y = 0;
      glfwGetWindowPos(window, &window_x, &window_y);
      _resizing_window = true;
      _resize_left = on_left;
      _resize_right = on_right;
      _resize_top = on_top;
      _resize_bottom = on_bottom;
      _resize_origin_x = window_x;
      _resize_origin_y = window_y;
      _resize_origin_w = window_w;
      _resize_origin_h = window_h;
    }
  }

  // Begin 里鼠标已是画布坐标，直接当作落点。
  tg::NodePose CanvasLocalMousePose() const {
    const ImVec2 canvas = ImGui::GetIO().MousePos;
    return {canvas.x, canvas.y};
  }

  // Suspend 之后鼠标回到屏幕坐标，再换回画布坐标。
  tg::NodePose ScreenMouseCanvasPose() const {
    const ImVec2 canvas = ax::NodeEditor::ScreenToCanvas(ImGui::GetIO().MousePos);
    return {canvas.x, canvas.y};
  }

  std::shared_ptr<tg::Node> FindNodeInGraph(
      const std::shared_ptr<tg::Graph>& graph, const tg::Node* raw) const {
    if (graph == nullptr || raw == nullptr) {
      return nullptr;
    }
    for (const auto& candidate : graph->getNodes()) {
      if (candidate.get() == raw) {
        return candidate;
      }
    }
    return nullptr;
  }

  // 已画过的节点读编辑器坐标，否则用记下的落点。
  ClipboardSample SampleNode(const std::shared_ptr<tg::Node>& node) const {
    ClipboardSample sample;
    sample.node = node;
    if (node == nullptr) {
      return sample;
    }
    if (_placed_nodes.find(node.get()) != _placed_nodes.end() &&
        ax::NodeEditor::GetCurrentEditor() != nullptr) {
      const ImVec2 position = ax::NodeEditor::GetNodePosition(
          ax::NodeEditor::NodeId(node.get()));
      sample.x = position.x;
      sample.y = position.y;
      return sample;
    }
    const auto pose = _node_poses.find(node.get());
    if (pose != _node_poses.end()) {
      sample.x = pose->second.x;
      sample.y = pose->second.y;
    }
    return sample;
  }

  // 当前图的撤销栈；不存在的图返回 nullptr。
  ui::UndoStack* FindUndoStack(tg::Graph* graph) {
    const auto found = _undo_stacks.find(graph);
    return found != _undo_stacks.end() ? &found->second : nullptr;
  }

  ui::UndoStack& UndoStackFor(tg::Graph& graph) { return _undo_stacks[&graph]; }

  // 命令目标（Command Target）：core 写图，同时同步画布落位与选中。
  ImGuiUndoTarget MakeUndoTarget(tg::Graph& graph) {
    ImGuiUndoTarget target;
    target.controller = &_controller;
    target.graph = &graph;
    target.placed_nodes = &_placed_nodes;
    target.node_poses = &_node_poses;
    target.select_pasted_nodes = &_select_pasted_nodes;
    target.focus = &_focus;
    target.editor = _editor;
    return target;
  }

  void PushUndo(tg::Graph& graph, std::unique_ptr<ui::Command> command,
                bool already_applied) {
    ImGuiUndoTarget target = MakeUndoTarget(graph);
    UndoStackFor(graph).Push(std::move(command), target, already_applied,
                             ++_next_edit_seq);
  }

  void UndoGraph(tg::Graph& graph) {
    ImGuiUndoTarget target = MakeUndoTarget(graph);
    UndoStackFor(graph).Undo(target);
  }

  void RedoGraph(tg::Graph& graph) {
    ImGuiUndoTarget target = MakeUndoTarget(graph);
    UndoStackFor(graph).Redo(target);
  }

  // 工程级命令入工程撤销栈（图的新建/删除/改名）。
  void PushProjectUndo(std::unique_ptr<ui::Command> command,
                       bool already_applied = false) {
    if (command == nullptr) {
      return;
    }
    _project_undo_stack.Push(std::move(command), _project_undo_target,
                             already_applied, ++_next_edit_seq);
  }

  // 全局撤销：各图栈 + 工程栈按全局序号，撤最近一次 push。
  void Undo() {
    tg::Graph* best_graph = nullptr;
    std::uint64_t best_seq = 0;
    for (auto& entry : _undo_stacks) {
      ui::UndoStack& stack = entry.second;
      if (!stack.CanUndo()) {
        continue;
      }
      const std::uint64_t seq = stack.NextUndoSequence();
      if (best_graph == nullptr || seq > best_seq) {
        best_graph = entry.first;
        best_seq = seq;
      }
    }
    if (_project_undo_stack.CanUndo() &&
        (best_graph == nullptr ||
         _project_undo_stack.NextUndoSequence() > best_seq)) {
      _project_undo_stack.Undo(_project_undo_target);
      return;
    }
    // 每个图栈都对应一张已打开的图。
    if (best_graph != nullptr) {
      UndoGraph(*best_graph);
    }
  }

  // 全局重做：重做序号最小的（最早被撤的）。
  void Redo() {
    tg::Graph* best_graph = nullptr;
    std::uint64_t best_seq = 0;
    for (auto& entry : _undo_stacks) {
      ui::UndoStack& stack = entry.second;
      if (!stack.CanRedo()) {
        continue;
      }
      const std::uint64_t seq = stack.NextRedoSequence();
      if (best_graph == nullptr || seq < best_seq) {
        best_graph = entry.first;
        best_seq = seq;
      }
    }
    if (_project_undo_stack.CanRedo() &&
        (best_graph == nullptr ||
         _project_undo_stack.NextRedoSequence() < best_seq)) {
      _project_undo_stack.Redo(_project_undo_target);
      return;
    }
    if (best_graph != nullptr) {
      RedoGraph(*best_graph);
    }
  }

  // 任一栈能撤销/重做（给菜单项置灰用）。
  bool CanUndoAny() const {
    if (_project_undo_stack.CanUndo()) {
      return true;
    }
    for (const auto& entry : _undo_stacks) {
      if (entry.second.CanUndo()) {
        return true;
      }
    }
    return false;
  }

  bool CanRedoAny() const {
    if (_project_undo_stack.CanRedo()) {
      return true;
    }
    for (const auto& entry : _undo_stacks) {
      if (entry.second.CanRedo()) {
        return true;
      }
    }
    return false;
  }

  // 把整张图收成图剪贴板：图名 + 全部节点/边/坐标（坐标取 _node_poses）。
  GraphClipboard CaptureGraph(const std::shared_ptr<tg::Graph>& graph) {
    GraphClipboard captured;
    if (graph == nullptr) {
      return captured;
    }
    captured.name = graph->getName();
    captured.valid = true;
    std::vector<ClipboardSample> samples;
    samples.reserve(graph->getNodes().size());
    for (const auto& node : graph->getNodes()) {
      if (node == nullptr) {
        continue;
      }
      ClipboardSample sample;
      sample.node = node;
      const auto found = _node_poses.find(node.get());
      if (found != _node_poses.end()) {
        sample.x = found->second.x;
        sample.y = found->second.y;
      }
      samples.push_back(std::move(sample));
    }
    captured.content = CaptureNodeClipboard(samples);
    return captured;
  }

  // 图粘贴最终名：剪切复用源名，复制 <源>_copy。
  std::string PasteGraphName(const GraphClipboard& clipboard) {
    const auto& graphs = _controller.getGraphs();
    if (clipboard.was_cut && !clipboard.name.empty()) {
      bool taken = false;
      for (const auto& graph : graphs) {
        if (graph != nullptr && graph->getName() == clipboard.name) {
          taken = true;
          break;
        }
      }
      if (!taken) {
        return clipboard.name;
      }
    }
    return CopiedGraphName(graphs, clipboard.name);
  }

  // 跑指定的一张图（图右键 Run；不必是当前选中的图）。
  void RunGraph(const std::shared_ptr<tg::Graph>& graph) {
    if (graph == nullptr) {
      return;
    }
    _controller.tryRun(*graph);
  }

  // 工程级撤销目标：新建一张图（含节点/边/坐标）。
  bool AddGraphFromClipboard(const GraphClipboard& clipboard) {
    const std::string name =
        clipboard.name.empty() ? NextGraphName(_controller.getGraphs())
                               : clipboard.name;
    auto graph = std::make_shared<tg::Graph>(name);
    _controller.addGraph(graph);
    ImGuiUndoTarget node_target = MakeUndoTarget(*graph);
    ui::AddNodesCommand command(clipboard.content.nodes, clipboard.content.edges,
                                "Restore Graph");
    command.Apply(node_target);
    _controller.markDirty();
    return true;
  }

  // 工程级撤销目标：按名删图（关页签、清各缓存由 RemoveGraphFromUi 做）。
  bool RemoveGraphByName(const std::string& name) {
    std::shared_ptr<tg::Graph> found;
    for (const auto& graph : _controller.getGraphs()) {
      if (graph != nullptr && graph->getName() == name) {
        found = graph;
        break;
      }
    }
    if (found == nullptr) {
      return false;
    }
    RemoveGraphFromUi(found, nullptr);
    return true;
  }

  // 工程级撤销目标：按名改图名。
  bool RenameGraphByName(const std::string& from, const std::string& to) {
    for (const auto& graph : _controller.getGraphs()) {
      if (graph != nullptr && graph->getName() == from) {
        graph->setName(to);
        _controller.markDirty();
        return true;
      }
    }
    return false;
  }

  // 新建空图并进工程撤销栈；建完选中并就地改名（改名也走撤销）。
  void NewGraphUndoable() {
    GraphClipboard graph;
    graph.name = NextGraphName(_controller.getGraphs());
    graph.valid = true;
    PushProjectUndo(
        std::make_unique<ui::AddGraphCommand>(graph, "New Graph"));
    const auto& graphs = _controller.getGraphs();
    for (int index = 0; index < static_cast<int>(graphs.size()); ++index) {
      if (graphs[index] != nullptr && graphs[index]->getName() == graph.name) {
        _focus.SelectGraph(index);
        BeginRenameGraph(graphs[index]);
        break;
      }
    }
  }

  // 图操作入工程撤销栈：剪切/删除用整图快照，粘贴用图剪贴板，改名用命令。
  void CutGraphFromUi(const std::shared_ptr<tg::Graph>& graph) {
    if (graph == nullptr) {
      return;
    }
    GraphClipboard captured = CaptureGraph(graph);
    captured.was_cut = true;
    _graph_clipboard = captured;
    PushProjectUndo(
        std::make_unique<ui::RemoveGraphCommand>(captured, "Cut Graph"));
  }

  void DeleteGraphFromUi(const std::shared_ptr<tg::Graph>& graph) {
    if (graph == nullptr) {
      return;
    }
    PushProjectUndo(std::make_unique<ui::RemoveGraphCommand>(
        CaptureGraph(graph), "Delete Graph"));
  }

  void PasteGraphFromUi() {
    if (_graph_clipboard.empty()) {
      return;
    }
    GraphClipboard graph = _graph_clipboard;
    graph.name = PasteGraphName(graph);
    PushProjectUndo(
        std::make_unique<ui::AddGraphCommand>(std::move(graph), "Paste Graph"));
  }

  // 主侧栏 Ctrl+C/X：对选中节点复制/剪切（节点剪贴板）。
  void ApplyNodeClipboardShortcut(NodeClipboardCommand command) {
    const auto& graphs = _controller.getGraphs();
    if (graphs.empty() || _focus.selected_node() == nullptr) {
      return;
    }
    const std::shared_ptr<tg::Graph>& graph =
        graphs[_focus.selected_graph_index()];
    auto owned = FindNodeInGraph(graph, _focus.selected_node());
    if (owned == nullptr) {
      return;
    }
    ApplyClipboardWithSamples(command, graph, {SampleNode(owned)}, {});
  }

  // 主侧栏 Ctrl+C 在图选中时：复制整图到图剪贴板。
  void CopyGraphShortcut() {
    const auto& graphs = _controller.getGraphs();
    if (graphs.empty()) {
      return;
    }
    _graph_clipboard = CaptureGraph(graphs[_focus.selected_graph_index()]);
    _graph_clipboard.was_cut = false;
  }

  // 一次用户删除：先捕节点与连边快照，再删，最后整组入栈。
  void DeleteSamplesFromUi(const std::shared_ptr<tg::Graph>& graph,
                           const std::vector<ClipboardSample>& samples,
                           std::shared_ptr<tg::Node>* selected_node = nullptr) {
    // 空图或没有要删的节点。
    if (graph == nullptr || samples.empty()) {
      return;
    }
    std::vector<ClipboardNode> nodes;
    nodes.reserve(samples.size());
    for (const ClipboardSample& sample : samples) {
      // 已不在图里的样本跳过。
      if (sample.node == nullptr) {
        continue;
      }
      nodes.push_back(ui::CaptureNodeSnapshot(
          *sample.node, tg::NodePose{sample.x, sample.y}));
    }
    if (nodes.empty()) {
      return;
    }
    // 入边全收，出边只收伸到被删集合外的；撤销整组连回来。
    std::vector<ui::LinkSnapshot> links;
    ui::CaptureLinksForDeletion(samples, links);
    tg::Node* const first_removed = samples.front().node.get();
    for (const ClipboardSample& sample : samples) {
      RemoveNodeFromUi(graph, sample.node, nullptr);
    }
    PushUndo(*graph,
             std::make_unique<ui::DeleteNodesCommand>(
                 std::move(nodes), std::move(links), "Delete Nodes"),
             true);
    if (selected_node != nullptr && selected_node->get() == first_removed) {
      selected_node->reset();
    }
  }

  // 提交就地对属性的修改：先恢复旧值，再让命令写入新值，避免写两次。
  void RecordPropertyEdit(tg::Graph& graph, const std::shared_ptr<tg::Node>& node,
                          int property_index,
                          const tg::PropertyValue& before,
                          const tg::PropertyValue& after) {
    // 值没变就不占撤销栈。
    if (node == nullptr || before == after) {
      return;
    }
    const auto properties = node->getEditableProperties();
    if (property_index < 0 ||
        property_index >= static_cast<int>(properties.size())) {
      return;
    }
    properties[static_cast<std::size_t>(property_index)]->value = before;
    PushUndo(graph,
             std::make_unique<ui::SetPropertyCommand>(
                 node->getName(), property_index, before, after),
             false);
  }

  // 文本/数字框失焦后提交；旧值在 IsItemActivated 时已记下。
  void FinishPropertyEdit(const std::shared_ptr<tg::Graph>& graph) {
    // 没有激活中的属性编辑。
    if (!_property_edit.active) {
      return;
    }
    _property_edit.active = false;
    // 图或节点已经不在，旧记录作废。
    if (graph == nullptr || _property_edit.node == nullptr) {
      return;
    }
    std::shared_ptr<tg::Node> node;
    for (const auto& candidate : graph->getNodes()) {
      if (candidate.get() == _property_edit.node) {
        node = candidate;
        break;
      }
    }
    // 节点已不在当前图里。
    if (node == nullptr) {
      return;
    }
    const auto properties = node->getEditableProperties();
    if (_property_edit.property_index < 0 ||
        _property_edit.property_index >=
            static_cast<int>(properties.size())) {
      return;
    }
    const tg::PropertyValue after =
        properties[static_cast<std::size_t>(_property_edit.property_index)]
            ->value;
    RecordPropertyEdit(*graph, node, _property_edit.property_index,
                       _property_edit.old_value, after);
  }

  // 切换输入露面：改为属性前记下会被断开的入边，撤销时先露面再连回。
  void ConvertInputPresentation(const std::shared_ptr<tg::Graph>& graph,
                                const std::shared_ptr<tg::Node>& node,
                                unsigned int index,
                                tg::InputPresentation presentation) {
    // 空图、空节点或没有变化。
    if (graph == nullptr || node == nullptr ||
        node->inputPresentation(index) == presentation) {
      return;
    }
    const tg::InputPresentation before = node->inputPresentation(index);
    std::vector<ui::LinkSnapshot> broken;
    // 转成属性会断开这个输入上的入边。
    if (presentation == tg::InputPresentation::Property) {
      for (const tg::ConnectionInfo& edge : node->incoming()) {
        // 只收这个输入槽上的边。
        if (edge.adj != nullptr && edge.port_in == index) {
          broken.push_back(ui::CaptureLink(*edge.adj, edge.port_out, *node,
                                          edge.port_in));
        }
      }
    }
    if (!graph->setInputPresentation(node, index, presentation)) {
      return;
    }
    PushUndo(*graph,
             std::make_unique<ui::SetPresentationCommand>(
                 node->getName(), index, before, presentation,
                 std::move(broken)),
             true);
  }

  // Ctrl+Z / Ctrl+Y（mac 用 Cmd）；每帧全局处理一次，文字框自己的撤销优先。
  void HandleGlobalUndoRedoShortcuts() {
    // 正在输入文字、有控件在编辑或正在落点时都不抢键。不再要求某个窗口聚焦，
    // 这样主侧栏和画布都能撤，图删空（没有页签）时也能撤销。
    if (ImGui::GetIO().WantTextInput || ImGui::IsAnyItemActive() ||
        _focus.armed_placement().armed) {
      return;
    }
    const ImGuiIO& io = ImGui::GetIO();
    // 只认编辑快捷键的修饰键。
    if (!io.KeyCtrl && !io.KeySuper) {
      return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
      if (io.KeyShift) {
        Redo();
      } else {
        Undo();
      }
      return;
    }
    // Ctrl+Y 只在 Windows/Linux 习惯上重做；mac 用 Shift+Cmd+Z。
    if (!io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
      Redo();
    }
  }

  // 鼠标按下时记下所有已画节点的坐标，松开后比对。
  void BeginMoveSnapshot(const std::shared_ptr<tg::Graph>& graph) {
    // 没有图或正在落点时不记。
    if (graph == nullptr || _focus.armed_placement().armed) {
      return;
    }
    _move_snapshot = ImGuiMoveSnapshot{};
    _move_snapshot.graph = graph.get();
    _move_snapshot.active = true;
    for (tg::Node* node : _placed_nodes) {
      // 记录里只剩已经画进编辑器的节点。
      if (node == nullptr) {
        continue;
      }
      const ImVec2 position =
          ax::NodeEditor::GetNodePosition(ax::NodeEditor::NodeId(node));
      _move_snapshot.samples.push_back(ImGuiMoveSnapshot::Sample{
          node, node->getName(), tg::NodePose{position.x, position.y}});
    }
  }

  // 松开左键后，把有位移的节点合成一条 MoveNodesCommand。
  void FinishMoveSnapshot(const std::shared_ptr<tg::Graph>& graph) {
    // 没有进行中的快照或换图了。
    if (!_move_snapshot.active || graph == nullptr ||
        _move_snapshot.graph != graph.get()) {
      _move_snapshot = ImGuiMoveSnapshot{};
      return;
    }
    std::vector<ui::MoveNodesCommand::Move> moves;
    for (const ImGuiMoveSnapshot::Sample& sample : _move_snapshot.samples) {
      // 节点已经不在图里（例如刚删掉）就跳过。
      if (sample.node == nullptr ||
          _placed_nodes.find(sample.node) == _placed_nodes.end()) {
        continue;
      }
      const ImVec2 position =
          ax::NodeEditor::GetNodePosition(ax::NodeEditor::NodeId(sample.node));
      const tg::NodePose after{position.x, position.y};
      // 位置没动就不入栈。
      if (after.x == sample.before.x && after.y == sample.before.y) {
        continue;
      }
      moves.push_back(ui::MoveNodesCommand::Move{sample.name, sample.before,
                                                 after});
      _node_poses[sample.node] = after;
    }
    _move_snapshot = ImGuiMoveSnapshot{};
    // 只有真的移动过才记一条。
    if (!moves.empty()) {
      PushUndo(*graph,
               std::make_unique<ui::MoveNodesCommand>(std::move(moves),
                                                      "Move Nodes"),
               true);
    }
  }

  std::vector<ClipboardSample> SamplesForEditorCommand(
      const std::shared_ptr<tg::Graph>& graph, const tg::Node* context_node) {
    std::vector<const tg::Node*> selected;
    const int selected_count = ax::NodeEditor::GetSelectedNodes(nullptr, 0);
    if (selected_count > 0) {
      std::vector<ax::NodeEditor::NodeId> ids(
          static_cast<std::size_t>(selected_count));
      ax::NodeEditor::GetSelectedNodes(ids.data(), selected_count);
      selected.reserve(ids.size());
      for (const ax::NodeEditor::NodeId& id : ids) {
        selected.push_back(id.AsPointer<tg::Node>());
      }
    }
    std::vector<ClipboardSample> samples;
    for (const tg::Node* raw : NodesForEditorCommand(selected, context_node)) {
      const std::shared_ptr<tg::Node> owned = FindNodeInGraph(graph, raw);
      if (owned == nullptr) {
        continue;
      }
      samples.push_back(SampleNode(owned));
    }
    return samples;
  }

  void FinishClipboardEdits(const NodeClipboardResult& result) {
    for (const auto& node : result.removed) {
      if (node == nullptr) {
        continue;
      }
      tg::Node* const raw = node.get();
      _placed_nodes.erase(raw);
      _focus.ClearSelectionIfNode(raw);
      _select_pasted_nodes.erase(
          std::remove(_select_pasted_nodes.begin(), _select_pasted_nodes.end(),
                      raw),
          _select_pasted_nodes.end());
    }
    if (result.created.empty()) {
      return;
    }
    _select_pasted_nodes.clear();
    for (const auto& node : result.created) {
      if (node != nullptr) {
        _select_pasted_nodes.push_back(node.get());
      }
    }
    _focus.SetSelectedNodeFromCanvas(
        _select_pasted_nodes.empty() ? nullptr : _select_pasted_nodes.front());
  }

  void ApplyClipboardCommand(NodeClipboardCommand command,
                                   const std::shared_ptr<tg::Graph>& graph,
                                   const tg::Node* context_node,
                                   tg::NodePose anchor) {
    if (graph == nullptr) {
      return;
    }
    ApplyClipboardWithSamples(command, graph,
                              SamplesForEditorCommand(graph, context_node),
                              anchor);
  }

  // 带样本执行剪贴板命令；剪贴板写入、删除与新增都从这里进撤销栈。
  void ApplyClipboardWithSamples(NodeClipboardCommand command,
                                 const std::shared_ptr<tg::Graph>& graph,
                                 const std::vector<ClipboardSample>& samples,
                                 tg::NodePose anchor) {
    if (graph == nullptr) {
      return;
    }
    // 剪切在删之前捕快照；删完再捕就拿不到坐标和连边了。
    std::vector<ClipboardNode> cut_nodes;
    std::vector<ui::LinkSnapshot> cut_links;
    if (command == NodeClipboardCommand::Cut) {
      for (const ClipboardSample& sample : samples) {
        if (sample.node == nullptr) {
          continue;
        }
        cut_nodes.push_back(ui::CaptureNodeSnapshot(
            *sample.node, tg::NodePose{sample.x, sample.y}));
      }
      // 剪切的删除撤销要恢复入边和外部出边。
      ui::CaptureLinksForDeletion(samples, cut_links);
    }
    const NodeClipboardResult result = ApplyNodeClipboard(
        command, _node_clipboard, samples, _controller, *graph, anchor,
        _node_poses);
    FinishClipboardEdits(result);
    if (command == NodeClipboardCommand::Cut) {
      // 没有选中节点时剪切不改图。
      if (!cut_nodes.empty()) {
        PushUndo(*graph,
                 std::make_unique<ui::DeleteNodesCommand>(
                     std::move(cut_nodes), std::move(cut_links), "Cut"),
                 true);
      }
      return;
    }
    // 粘贴：加节点和内部边合成一条命令。
    if (command == NodeClipboardCommand::Paste && !result.created.empty()) {
      std::vector<ClipboardNode> added;
      std::unordered_map<tg::Node*, std::size_t> index_by_node;
      added.reserve(result.created.size());
      for (const auto& node : result.created) {
        // 创建失败时不会是空指针，这里仍按防御处理。
        if (node == nullptr) {
          continue;
        }
        tg::NodePose pose;
        const auto pose_it = _node_poses.find(node.get());
        if (pose_it != _node_poses.end()) {
          pose = pose_it->second;
        }
        index_by_node.emplace(node.get(), added.size());
        added.push_back(ui::CaptureNodeSnapshot(*node, pose));
      }
      std::vector<ClipboardEdge> edges;
      for (std::size_t from_index = 0; from_index < added.size(); ++from_index) {
        tg::Node* const from = result.created[from_index].get();
        if (from == nullptr) {
          continue;
        }
        for (const tg::ConnectionInfo& edge : from->outgoing()) {
          // 只收两端都在这次新增里的边。
          const auto to_it = index_by_node.find(edge.adj.get());
          if (to_it == index_by_node.end()) {
            continue;
          }
          edges.push_back(
              ClipboardEdge{from_index, edge.port_out, to_it->second,
                            edge.port_in});
        }
      }
      PushUndo(*graph,
               std::make_unique<ui::AddNodesCommand>(
                   std::move(added), std::move(edges), "Paste"),
               true);
    }
  }

  void DeleteEditorNodes(const std::shared_ptr<tg::Graph>& graph,
                         const tg::Node* context_node) {
    if (graph == nullptr) {
      return;
    }
    DeleteSamplesFromUi(graph, SamplesForEditorCommand(graph, context_node));
  }

  // 新节点要等下一帧 BeginNode 之后，编辑器才认得出，才能选中。
  void SelectPastedNodes() {
    if (_select_pasted_nodes.empty()) {
      return;
    }
    for (tg::Node* node : _select_pasted_nodes) {
      if (node == nullptr || _placed_nodes.find(node) == _placed_nodes.end()) {
        return;
      }
    }
    ax::NodeEditor::ClearSelection();
    bool append = false;
    for (tg::Node* node : _select_pasted_nodes) {
      ax::NodeEditor::SelectNode(ax::NodeEditor::NodeId(node), append);
      append = true;
    }
    _select_pasted_nodes.clear();
  }

  // 库在上一帧 End 里认出快捷键，这一帧 BeginShortcut 才读得到。
  // 每一条是否执行，问清单上该命令的 When。
  void HandleEditorShortcuts(const std::shared_ptr<tg::Graph>& graph) {
    if (!ax::NodeEditor::BeginShortcut()) {
      return;
    }
    ShortcutContext context;
    context.editor_focused = true;
    context.placing = _focus.armed_placement().armed;
    const tg::NodePose anchor = CanvasLocalMousePose();
    if (ax::NodeEditor::AcceptCut()) {
      if (ShortcutCommandApplies("Cut", context)) {
        ApplyClipboardCommand(NodeClipboardCommand::Cut, graph, nullptr,
                                    anchor);
      }
    } else if (ax::NodeEditor::AcceptCopy()) {
      if (ShortcutCommandApplies("Copy", context)) {
        ApplyClipboardCommand(NodeClipboardCommand::Copy, graph, nullptr,
                                    anchor);
      }
    } else if (ax::NodeEditor::AcceptPaste()) {
      if (ShortcutCommandApplies("Paste", context)) {
        ApplyClipboardCommand(NodeClipboardCommand::Paste, graph, nullptr,
                                    anchor);
      }
    } else if (ax::NodeEditor::AcceptCreateNode()) {
      if (ShortcutCommandApplies("New Node", context)) {
        OpenEditorKindList(_focus, _focus.selected_graph_index(),
                           context.placing);
      }
    }
    ax::NodeEditor::EndShortcut();
  }

  // 右键平移、滚轮和 Frame 之后，不再把镜头拉回画布原点。
  void NoteUserCanvasView(bool frame_shortcut_enabled) {
    const bool over_canvas = ImGui::IsWindowHovered(
        ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    const bool canvas_focused =
        ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    const ImGuiIO& io = ImGui::GetIO();
    if ((over_canvas || canvas_focused) &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
      _canvas_view_user_owned = true;
    }
    if (over_canvas && io.MouseWheel != 0.0f) {
      _canvas_view_user_owned = true;
    }
    if (over_canvas && io.MouseWheelH != 0.0f) {
      _canvas_view_user_owned = true;
    }
    if (frame_shortcut_enabled && !io.WantTextInput &&
        ImGui::IsKeyPressed(ImGuiKey_F)) {
      _canvas_view_user_owned = true;
    }
  }

  void ShowFlowCanvas() {
    if (_editor == nullptr) {
      return;
    }
    constexpr float kInitialNodeStepX = 220.0f;
    const auto& graphs = _controller.getGraphs();
    bool hold_new_node = false;

    // 主视口 Memento：画布 Begin 会改写 Pos / WorkPos，End 后写回。
    struct ViewportMemento {
      ImGuiViewport* main = nullptr;
      ImGuiViewport* window = nullptr;
      ImVec2 main_pos{};
      ImVec2 main_size{};
      ImVec2 main_work_pos{};
      ImVec2 main_work_size{};
      ImVec2 window_pos{};
      ImVec2 window_size{};
      ImVec2 window_work_pos{};
      ImVec2 window_work_size{};
      ImGuiWindow* canvas = nullptr;
      ImVec2 canvas_pos{};
      bool window_is_main = true;

      ViewportMemento() {
        main = ImGui::GetMainViewport();
        window = ImGui::GetWindowViewport();
        window_is_main = window == main;
        if (main != nullptr) {
          main_pos = main->Pos;
          main_size = main->Size;
          main_work_pos = main->WorkPos;
          main_work_size = main->WorkSize;
        }
        if (window != nullptr && !window_is_main) {
          window_pos = window->Pos;
          window_size = window->Size;
          window_work_pos = window->WorkPos;
          window_work_size = window->WorkSize;
        }
        canvas = ImGui::GetCurrentWindow();
        if (canvas != nullptr) {
          canvas_pos = canvas->Pos;
        }
      }

      ~ViewportMemento() {
        if (canvas != nullptr) {
          canvas->Pos = canvas_pos;
        }
        if (window != nullptr && !window_is_main) {
          window->Pos = window_pos;
          window->Size = window_size;
          window->WorkPos = window_work_pos;
          window->WorkSize = window_work_size;
        }
        if (main != nullptr) {
          main->Pos = main_pos;
          main->Size = main_size;
          main->WorkPos = main_work_pos;
          main->WorkSize = main_work_size;
        }
      }
    };
    [[maybe_unused]] ViewportMemento viewport_memento;

    ax::NodeEditor::SetCurrentEditor(_editor);
    // Frame、Shift+F、Delete 跟清单同一条 When：编辑区有焦点且不在落点。
    ShortcutContext library_keys;
    library_keys.editor_focused =
        ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    library_keys.placing = _focus.armed_placement().armed;
    const bool frame_shortcut_enabled =
        LibraryKeyShortcutsEnabled(library_keys);
    NoteUserCanvasView(frame_shortcut_enabled);
    ax::NodeEditor::EnableShortcuts(frame_shortcut_enabled);
    if (!_canvas_view_user_owned) {
      HoldIdentityCanvasView(_editor);
    }
    ax::NodeEditor::Style& editor_style = ax::NodeEditor::GetStyle();
    ApplyFlowCanvasTheme(editor_style, library_keys.editor_focused);
    // Begin / End：节点编辑器。图里已有的节点画在网格上。
    ax::NodeEditor::Begin("CanvasEditor");
    // 没有选中的图时不画节点。
    if (!graphs.empty()) {
      // 左键按下时记下节点坐标，松开后比对；一次拖动只入一条命令。
      if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
          ImGui::IsWindowHovered(
              ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
        BeginMoveSnapshot(graphs[_focus.selected_graph_index()]);
      }
      int column = 0;
      for (const auto& node : graphs[_focus.selected_graph_index()]->getNodes()) {
        const ax::NodeEditor::NodeId node_id(node.get());
        bool type_changed = false;
        const bool place_now = _placed_nodes.insert(node.get()).second;
        // 节点第一次出现才落位：有工程里的坐标用它，否则按列排。
        if (place_now) {
          constexpr float kInitialNodePositionY = 0.0f;
          ImVec2 position{static_cast<float>(column) * kInitialNodeStepX,
                          kInitialNodePositionY};
          const auto pose_it = _node_poses.find(node.get());
          if (pose_it != _node_poses.end()) {
            position.x = pose_it->second.x;
            position.y = pose_it->second.y;
          }
          ax::NodeEditor::SetNodePosition(node_id, position);
        }
        // Begin / End：一个图节点。端口和连线都来自节点基类。
        ax::NodeEditor::BeginNode(node_id);
        // 打开 Show Type Names 时，节点标题上方补一行 kind（type token 色）。
        if (_show_port_type_names && !node->typeId().empty()) {
          ImGui::PushStyleColor(ImGuiCol_Text,
                                ToImVec4(theme_detail::CurrentTokens().editor_type_foreground));
          ImGui::TextUnformatted(node->typeId().c_str());
          ImGui::PopStyleColor();
        }
        ImGui::SetNextItemAllowOverlap();
        ImGui::TextUnformatted(node->getName().c_str());
        // 关闭时悬停标题就给 kind；名称与 kind 相同时也保留，和端口行为一致。
        if (!_show_port_type_names && !node->typeId().empty() &&
            ImGui::IsItemHovered()) {
          ShowAnchoredTypeChip(
              node->typeId().c_str(),
              ax::NodeEditor::CanvasToScreen(ImGui::GetItemRectMin()),
              ax::NodeEditor::CanvasToScreen(ImGui::GetItemRectMax()),
              ImGui::GetStyle().FontScaleMain);
        }
        ImGui::BeginGroup();
        for (unsigned int port_index = 0; port_index < node->inputPortCount();
             ++port_index) {
          // 属性模式下的输入槽不画引脚（输入口即属性）。
          if (!node->isInputShownAsPort(port_index)) {
            continue;
          }
          const tg::Node::Port* port = node->inputPort(port_index);
          // Begin / End：输入引脚。身份就是基类里的端口对象。
          ax::NodeEditor::BeginPin(ax::NodeEditor::PinId(port),
                                   ax::NodeEditor::PinKind::Input);
          const float ui_scale = ImGui::GetStyle().FontScaleMain;
          type_changed |= DrawPortRow(*port, true, _show_port_type_names,
                                      ui_scale, _type_popup);
          ax::NodeEditor::EndPin();
        }
        ImGui::EndGroup();
        ImGui::SameLine();
        ImGui::BeginGroup();
        for (unsigned int port_index = 0; port_index < node->outputPortCount();
             ++port_index) {
          const tg::Node::Port* port = node->outputPort(port_index);
          // Begin / End：输出引脚。身份就是基类里的端口对象。
          ax::NodeEditor::BeginPin(ax::NodeEditor::PinId(port),
                                   ax::NodeEditor::PinKind::Output);
          const float ui_scale = ImGui::GetStyle().FontScaleMain;
          type_changed |= DrawPortRow(*port, false, _show_port_type_names,
                                      ui_scale, _type_popup);
          ax::NodeEditor::EndPin();
        }
        ImGui::EndGroup();
        ax::NodeEditor::EndNode();
        if (type_changed) {
          graphs[_focus.selected_graph_index()]->refreshPortTypes(node);
          _controller.markDirty();
        }
        ++column;
      }
      for (const auto& node : graphs[_focus.selected_graph_index()]->getNodes()) {
        for (const auto& connection : node->outgoing()) {
          const tg::Node::Port* output = node->outputPort(connection.port_out);
          const tg::Node::Port* input =
              connection.adj == nullptr
                  ? nullptr
                  : connection.adj->inputPort(connection.port_in);
          // 两端端口都还在，且下游仍以端口露面，才把这条已有连边画出来。
          if (output == nullptr || input == nullptr ||
              connection.adj == nullptr ||
              !connection.adj->isInputShownAsPort(connection.port_in)) {
            continue;
          }
          // 连线跟随端口类型色；类型不一致才用错误色。
          const ImVec4 link_color =
              output->data_type.id == input->data_type.id
                  ? ColorForPortType(output->data_type)
                  : LinkFailureColor();
          ax::NodeEditor::Link(ax::NodeEditor::LinkId(&connection),
                               ax::NodeEditor::PinId(output),
                               ax::NodeEditor::PinId(input), link_color,
                               kDefaultLinkThickness);
        }
      }
      // 上一帧贴上的节点这一帧才画进编辑器，这时再选中。
      SelectPastedNodes();
      // Begin / End：处理正在拉的新连线。合法时只调用图已有的 connect。
      if (ax::NodeEditor::BeginCreate()) {
        ax::NodeEditor::PinId start_pin;
        ax::NodeEditor::PinId end_pin;
        // 两端都落到引脚上才判断能不能连。
        if (ax::NodeEditor::QueryNewLink(&start_pin, &end_pin) && start_pin &&
            end_pin) {
          const OrientedLink link = OrientNewLink(
              start_pin.AsPointer<tg::Node::Port>(),
              end_pin.AsPointer<tg::Node::Port>());
          // 方向、自环、露面方式、类型与重复边都由 Graph::canConnect 判断。
          if (!graphs[_focus.selected_graph_index()]->canConnect(link.output,
                                                                 link.input)) {
            ax::NodeEditor::RejectNewItem(LinkFailureColor(),
                                          kDefaultLinkThickness);
          } else if (ax::NodeEditor::AcceptNewItem(
                         link.output != nullptr
                             ? ColorForPortType(link.output->data_type)
                             : LinkFailureColor(),
                         kDefaultLinkThickness)) {
            auto& graph = graphs[_focus.selected_graph_index()];
            if (graph->connect(link.output, link.input)) {
              PushUndo(*graph,
                       std::make_unique<ui::ConnectCommand>(ui::CaptureLink(
                           *link.output->node, link.output->index,
                           *link.input->node, link.input->index)),
                       true);
              _controller.markDirty();
            }
          }
        }
      }
      ax::NodeEditor::EndCreate();
      // Begin / End：处理删除。接受删节点，接受删当前这一条连边。
      if (ax::NodeEditor::BeginDelete()) {
        std::shared_ptr<tg::Graph> delete_graph =
            graphs[_focus.selected_graph_index()];
        std::vector<std::shared_ptr<tg::Node>> deleted_nodes;
        ax::NodeEditor::NodeId deleted_node_id;
        while (ax::NodeEditor::QueryDeletedNode(&deleted_node_id)) {
          const std::shared_ptr<tg::Node> owned =
              FindNodeInGraph(delete_graph,
                              deleted_node_id.AsPointer<tg::Node>());
          // 节点还在图里才接受删除；先攒着，整组删完只入一条命令。
          if (owned != nullptr && ax::NodeEditor::AcceptDeletedItem()) {
            deleted_nodes.push_back(owned);
          } else {
            ax::NodeEditor::RejectDeletedItem();
          }
        }
        if (!deleted_nodes.empty()) {
          std::vector<ClipboardSample> samples;
          samples.reserve(deleted_nodes.size());
          for (const auto& node : deleted_nodes) {
            samples.push_back(SampleNode(node));
          }
          DeleteSamplesFromUi(delete_graph, samples);
        }
        ax::NodeEditor::LinkId deleted_link_id;
        while (ax::NodeEditor::QueryDeletedLink(&deleted_link_id)) {
          const tg::ConnectionInfo* const link =
              deleted_link_id.AsPointer<tg::ConnectionInfo>();
          std::shared_ptr<tg::Node> source;
          tg::Graph* const graph = graphs.empty()
                                       ? nullptr
                                       : graphs[_focus.selected_graph_index()].get();
          // 连线身份是上游 outgoing 里的那条边；先抄出两端，再删，指针随后失效。
          if (link == nullptr || graph == nullptr ||
              !FindOutgoingConnection(*graph, link, source) ||
              source == nullptr || link->adj == nullptr ||
              !ax::NodeEditor::AcceptDeletedItem(false)) {
            ax::NodeEditor::RejectDeletedItem();
            continue;
          }
          const std::shared_ptr<tg::Node> downstream = link->adj;
          const unsigned int port_out = link->port_out;
          const unsigned int port_in = link->port_in;
          const ui::LinkSnapshot snapshot = ui::CaptureLink(
              *source, port_out, *downstream, port_in);
          if (_focus.selected_link() == link) {
            _focus.ClearSelectedLink();
          }
          if (graph->disconnect(source->outputPort(port_out),
                                downstream->inputPort(port_in))) {
            PushUndo(*graph,
                     std::make_unique<ui::DisconnectCommand>(snapshot), true);
            _controller.markDirty();
          }
        }
      }
      ax::NodeEditor::EndDelete();
      // 剪切、复制、粘贴、空格新建。落点中忽略。
      HandleEditorShortcuts(graphs[_focus.selected_graph_index()]);
      // 落点判定在菜单之前：编辑区右键取消时不再打开引脚、节点、空白菜单。
      const bool cancel_armed_place = UpdateArmedPlacement(graphs);
      // 画布右键：引脚 / 节点 / 背景菜单（Suspend 后弹 ImGui）。
      // PinId 只在 ShowPinContextMenu 为真的那一帧有效，必须存进成员供后续帧画菜单。
      ax::NodeEditor::Suspend();
      ax::NodeEditor::PinId context_pin_id;
      if (ax::NodeEditor::ShowPinContextMenu(&context_pin_id)) {
        auto* const port = context_pin_id.AsPointer<tg::Node::Port>();
        if (port != nullptr && port->node != nullptr && !port->is_output &&
            port->node->allowsPropertyPresentation(port->index)) {
          _pin_context_port = port;
          if (!cancel_armed_place) {
            ImGui::OpenPopup("Pin Context Menu");
          }
        }
      }
      ax::NodeEditor::NodeId context_node_id;
      if (ax::NodeEditor::ShowNodeContextMenu(&context_node_id) &&
          context_node_id.AsPointer<tg::Node>() != nullptr &&
          !cancel_armed_place) {
        _canvas_menu_node = context_node_id.AsPointer<tg::Node>();
        ImGui::OpenPopup("Canvas Node Menu");
      }
      if (ax::NodeEditor::ShowBackgroundContextMenu() && !cancel_armed_place) {
        ImGui::OpenPopup("Canvas Background Menu");
      }
      if (ImGui::BeginPopup("Pin Context Menu")) {
        if (_pin_context_port != nullptr &&
            _pin_context_port->node != nullptr &&
            !_pin_context_port->is_output) {
          // 菜单用英文：当前字体无中文 glyph，中文会显示成问号。
          if (ImGui::MenuItem("Convert to Property")) {
            auto owned = _pin_context_port->node->shared_from_this();
            ConvertInputPresentation(graphs[_focus.selected_graph_index()],
                                     owned, _pin_context_port->index,
                                     tg::InputPresentation::Property);
            _pin_context_port = nullptr;
          }
        }
        ImGui::EndPopup();
      } else {
        _pin_context_port = nullptr;
      }
      if (ImGui::BeginPopup("Canvas Node Menu")) {
        const std::shared_ptr<tg::Graph> menu_graph =
            graphs[_focus.selected_graph_index()];
        const tg::NodePose menu_anchor = ScreenMouseCanvasPose();
        if (ImGui::MenuItem("Cut", kEditorCutShortcut)) {
          ApplyClipboardCommand(NodeClipboardCommand::Cut, menu_graph,
                                      _canvas_menu_node, menu_anchor);
        }
        if (ImGui::MenuItem("Copy", kEditorCopyShortcut)) {
          ApplyClipboardCommand(NodeClipboardCommand::Copy, menu_graph,
                                      _canvas_menu_node, menu_anchor);
        }
        if (ImGui::MenuItem("Rename")) {
          BeginCanvasRename(menu_graph, _canvas_menu_node);
        }
        if (ImGui::MenuItem("Delete", kEditorDeleteShortcut)) {
          DeleteEditorNodes(menu_graph, _canvas_menu_node);
        }
        ImGui::EndPopup();
      } else {
        _canvas_menu_node = nullptr;
      }
      if (ImGui::BeginPopup("Canvas Background Menu")) {
        // 与 Explorer / Qt 一致：空白右键先 New Node 再 Paste。
        if (ImGui::MenuItem("New Node", kEditorNewNodeShortcut)) {
          RequestOpenPickNodeKind(_focus.selected_graph_index());
        }
        if (ImGui::MenuItem("Paste", kEditorPasteShortcut, false,
                            !_node_clipboard.empty())) {
          ApplyClipboardCommand(
              NodeClipboardCommand::Paste, graphs[_focus.selected_graph_index()],
              nullptr, ScreenMouseCanvasPose());
        }
        ImGui::EndPopup();
      }
      // Begin / End：画布节点改名弹窗（主侧栏/画布都能触发；确认进撤销栈）。
      ShowCanvasRenamePopup();
      ax::NodeEditor::Resume();
      // 松开左键后比对坐标，有位移才把这次拖动记成一条命令。
      if (_move_snapshot.active &&
          ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        FinishMoveSnapshot(graphs[_focus.selected_graph_index()]);
      }
      // 刚落下的节点这一帧还没画进编辑器，先按住选中。
      hold_new_node = _focus.pending_editor_select() != nullptr &&
                      _placed_nodes.find(_focus.pending_editor_select()) ==
                          _placed_nodes.end();
      if (!hold_new_node && _focus.pending_editor_select() != nullptr) {
        ax::NodeEditor::SelectNode(
            ax::NodeEditor::NodeId(_focus.pending_editor_select()));
      }
    }
    ax::NodeEditor::End();
    tg::Node* picked_in_explorer = nullptr;
    if (!hold_new_node) {
      picked_in_explorer = _focus.ConsumePendingEditorSelect();
      // 编辑器结束时可能清掉侧栏这次点击，再选一次，属性表才跟得上。
      if (picked_in_explorer != nullptr) {
        ax::NodeEditor::SelectNode(ax::NodeEditor::NodeId(picked_in_explorer));
      }
    }
    constexpr int kSelectedNodeReadCount = 1;
    constexpr int kSelectedLinkReadCount = 1;
    ax::NodeEditor::NodeId selected_id;
    const int selected_count =
        ax::NodeEditor::GetSelectedNodes(&selected_id, kSelectedNodeReadCount);
    ax::NodeEditor::LinkId selected_link_id;
    const int selected_link_count = ax::NodeEditor::GetSelectedLinks(
        &selected_link_id, kSelectedLinkReadCount);
    const tg::ConnectionInfo* selected_link = nullptr;
    std::shared_ptr<tg::Node> link_source;
    if (selected_link_count > 0 && !graphs.empty()) {
      const tg::ConnectionInfo* const candidate =
          selected_link_id.AsPointer<tg::ConnectionInfo>();
      if (FindOutgoingConnection(*graphs[_focus.selected_graph_index()],
                                 candidate, link_source)) {
        selected_link = candidate;
      }
    }
    // 刚落下的节点还没进 _placed_nodes，这一帧不要用空的画布选中把它清掉。
    // 刚贴上的节点同样还没画进编辑器。侧栏点的节点以侧栏为准。
    // 只选中连线时主侧栏没有对应项，节点选中保持不动。
    if (!_select_pasted_nodes.empty()) {
      _focus.SetSelectedNodeFromCanvas(_select_pasted_nodes.front());
    } else if (!hold_new_node && picked_in_explorer != nullptr) {
      _focus.SetSelectedNodeFromCanvas(picked_in_explorer);
    } else if (!hold_new_node && selected_count > 0 && !graphs.empty()) {
      tg::Node* const candidate = selected_id.AsPointer<tg::Node>();
      bool still_in_graph = false;
      for (const auto& node : graphs[_focus.selected_graph_index()]->getNodes()) {
        // 刚删掉的节点不能再当成选中。
        if (node.get() == candidate) {
          still_in_graph = true;
          break;
        }
      }
      // 还在当前图里才记下选中。
      if (still_in_graph) {
        _focus.SetSelectedNodeFromCanvas(candidate);
      } else {
        _focus.SetSelectedNodeFromCanvas(nullptr);
      }
    } else if (!hold_new_node && selected_link != nullptr) {
      _focus.SetSelectedLink(selected_link);
    } else if (!hold_new_node) {
      _focus.SetSelectedNodeFromCanvas(nullptr);
    }
    if (!_canvas_view_user_owned) {
      HoldIdentityCanvasView(_editor);
    }
    ax::NodeEditor::SetCurrentEditor(nullptr);
  }

  // 鼠标按键。ImGui 的 GLFW 后端会先收到事件，这里目前不处理。
  static void MouseButtonCallback(GLFWwindow*, int, int, int) {}

  // 光标移动。ImGui 的 GLFW 后端会先收到事件，这里目前不处理。
  static void CursorPosCallback(GLFWwindow*, double, double) {}

  // 键盘。ImGui 的 GLFW 后端会先收到事件，这里目前不处理。
  static void KeyCallback(GLFWwindow*, int, int, int, int) {}

 private:
  // 记下目标图并打开种类 Quick Pick。选中种类后才进入落点。
  void RequestOpenPickNodeKind(int graph_index) {
    _focus.RequestPickNodeKind(graph_index);
  }

  // 编辑区里画种类名，最后一个字的右下角对齐指针。这不是节点，也不是工具栏。
  // screen_mouse 必须是屏幕坐标：画布内部的 MousePos 已被改成画布坐标。
  void DrawArmedPlacementPreview(const std::string& caption,
                                 ImVec2 screen_mouse) const {
    constexpr float kPreviewPad = 4.0f;
    const ImVec2 text_size = ImGui::CalcTextSize(caption.c_str());
    const CaptionPreviewAnchor origin = CaptionPreviewTextOrigin(
        screen_mouse.x, screen_mouse.y, text_size.x, text_size.y);
    const ImVec2 text_pos(origin.text_x, origin.text_y);
    const ImVec2 box_min(text_pos.x - kPreviewPad, text_pos.y - kPreviewPad);
    const ImVec2 box_max(screen_mouse.x, screen_mouse.y);
    ImDrawList* const draw = ImGui::GetForegroundDrawList();
    draw->AddRectFilled(box_min, box_max, ImGui::GetColorU32(ImGuiCol_PopupBg));
    draw->AddText(text_pos, ImGui::GetColorU32(ImGuiCol_Text), caption.c_str());
  }

  // 读这一帧的键鼠并执行落点判定。真正落下时清掉左键，避免编辑器再框选。
  bool UpdateArmedPlacement(
      const std::vector<std::shared_ptr<tg::Graph>>& graphs) {
    if (!_focus.armed_placement().armed) {
      _armed_placement_waits_for_release = false;
      return false;
    }
    ArmedPlacementInput input;
    input.escape_pressed = ImGui::IsKeyPressed(ImGuiKey_Escape);
    input.text_input_owns_key = ImGui::GetIO().WantTextInput;
    input.pointer_over_editor = ImGui::IsWindowHovered(
        ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    input.right_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    input.left_button_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    input.left_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    input.armed_graph_is_current = _focus.armed_placement().graph_index ==
                                   _focus.selected_graph_index();
    input.waits_for_release = _armed_placement_waits_for_release;
    // 画布 Begin 之后 MousePos 已是画布坐标，再 ScreenToCanvas 会偏到指针左下。
    const ImVec2 canvas = ImGui::GetIO().MousePos;
    const ImVec2 screen = ax::NodeEditor::CanvasToScreen(canvas);
    const ArmedPlacementStep step = ApplyArmedPlacement(
        _controller, graphs, _focus, _node_poses, input,
        tg::NodePose{canvas.x, canvas.y});
    _armed_placement_waits_for_release = step.decision.waits_for_release;
    if (step.decision.action == ArmedPlacementAction::Place) {
      ImGui::GetIO().MouseClicked[ImGuiMouseButton_Left] = false;
    }
    // 落点已创建节点；把这次新增记进当前图页签的撤销栈。
    if (step.node != nullptr && !graphs.empty()) {
      tg::NodePose pose{canvas.x, canvas.y};
      const auto pose_it = _node_poses.find(step.node.get());
      if (pose_it != _node_poses.end()) {
        pose = pose_it->second;
      }
      PushUndo(*graphs[_focus.selected_graph_index()],
               std::make_unique<ui::AddNodesCommand>(
                   std::vector<ClipboardNode>{
                       ui::CaptureNodeSnapshot(*step.node, pose)},
                   std::vector<ClipboardEdge>{}, "Add Node"),
               true);
    }
    if (_focus.armed_placement().armed && input.armed_graph_is_current) {
      DrawArmedPlacementPreview(_focus.armed_placement().caption, screen);
    }
    return step.suppress_editor_context_menu;
  }

  // 开始就地重命名一张图。
  void BeginRenameGraph(const std::shared_ptr<tg::Graph>& graph) {
    // 空图不改名。
    if (graph == nullptr) {
      return;
    }
    _rename_node.reset();
    _rename_node_graph.reset();
    _rename_graph = graph;
    _rename_buffer = graph->getName();
    _rename_focus = true;
  }

  // 开始就地重命名一个节点。
  void BeginRenameNode(const std::shared_ptr<tg::Graph>& graph,
                       const std::shared_ptr<tg::Node>& node) {
    // 没有归属图或节点就不改。
    if (graph == nullptr || node == nullptr) {
      return;
    }
    _rename_graph.reset();
    _rename_node_graph = graph;
    _rename_node = node;
    _rename_buffer = node->getName();
    _rename_focus = true;
  }

  // 按当前选中地址，在当前图里找到节点并开始改名。
  void BeginRenameSelectedNode() {
    const auto& graphs = _controller.getGraphs();
    // 没有图或没有选中就不改。
    if (graphs.empty() || _focus.selected_node() == nullptr) {
      return;
    }
    auto& graph = graphs[_focus.selected_graph_index()];
    for (const auto& candidate : graph->getNodes()) {
      // 地址相同就是要改名的选中节点。
      if (candidate.get() == _focus.selected_node()) {
        BeginRenameNode(graph, candidate);
        return;
      }
    }
  }

  // 取消就地重命名，不写回名字。
  void CancelRename() {
    _rename_graph.reset();
    _rename_node_graph.reset();
    _rename_node.reset();
    _rename_buffer.clear();
    _rename_focus = false;
  }

  // identifier 是否已被同层其它实体占用；空名不参与比较。
  bool RenameNameTaken() const {
    // 改节点：图内跨 kind 唯一，排除自己。
    if (_rename_node != nullptr && _rename_node_graph != nullptr) {
      return _rename_node_graph->isNodeNameTaken(_rename_buffer,
                                                 _rename_node.get());
    }
    // 改图：工程内唯一，排除自己。
    if (_rename_graph != nullptr) {
      for (const auto& graph : _controller.getGraphs()) {
        if (graph != nullptr && graph != _rename_graph &&
            graph->getName() == _rename_buffer) {
          return true;
        }
      }
    }
    return false;
  }

  // 撞名提示语按改节点还是改图区分。
  const char* RenameTakenHint() const {
    return _rename_node != nullptr ? "Name already exists in this graph"
                                   : "Name already exists in this project";
  }

  // 提交就地改名（图或节点）；调用前输入已通过合法性检查。
  void CommitRename() {
    // 还在改节点名；名字没变不算改动。
    if (_rename_node != nullptr && _rename_node_graph != nullptr) {
      const std::string old_name = _rename_node->getName();
      if (_rename_buffer != old_name &&
          _rename_node_graph->renameNode(_rename_node, _rename_buffer)) {
        PushUndo(*_rename_node_graph,
                 std::make_unique<ui::RenameNodeCommand>(old_name,
                                                         _rename_buffer),
                 true);
        _controller.markDirty();
      }
      CancelRename();
      return;
    }
    // 还在改图名；空名或原名保持不动。改名进工程撤销栈。
    if (_rename_graph != nullptr) {
      if (!_rename_buffer.empty() &&
          _rename_buffer != _rename_graph->getName()) {
        const std::string from = _rename_graph->getName();
        const std::string to = _rename_buffer;
        _rename_graph->setName(to);
        PushProjectUndo(
            std::make_unique<ui::RenameGraphCommand>(from, to), true);
        _controller.markDirty();
      }
      CancelRename();
    }
  }

  // 画布节点改名弹窗：Begin... 记录目标并请求打开，Show... 每帧画。
  void BeginCanvasRename(const std::shared_ptr<tg::Graph>& graph,
                         tg::Node* node) {
    if (graph == nullptr || node == nullptr) {
      return;
    }
    _canvas_rename_graph = graph;
    _canvas_rename_node = node;
    _canvas_rename_buffer = node->getName();
    _canvas_rename_focus = true;
    // 在节点菜单里不直接 OpenPopup（ID 会挂到父 popup 上），交给顶层画。
    _canvas_rename_requested = true;
  }

  void CancelCanvasRename() {
    _canvas_rename_graph.reset();
    _canvas_rename_node = nullptr;
    _canvas_rename_buffer.clear();
    _canvas_rename_focus = false;
    _canvas_rename_requested = false;
  }

  bool CanvasRenameTaken() const {
    return !_canvas_rename_buffer.empty() && _canvas_rename_graph != nullptr &&
           _canvas_rename_node != nullptr &&
           _canvas_rename_graph->isNodeNameTaken(_canvas_rename_buffer,
                                                 _canvas_rename_node);
  }

  void CommitCanvasRename() {
    if (_canvas_rename_graph != nullptr && _canvas_rename_node != nullptr &&
        !_canvas_rename_buffer.empty() &&
        _canvas_rename_buffer != _canvas_rename_node->getName() &&
        !CanvasRenameTaken()) {
      const std::string old_name = _canvas_rename_node->getName();
      std::shared_ptr<tg::Node> owned;
      for (const auto& candidate : _canvas_rename_graph->getNodes()) {
        if (candidate.get() == _canvas_rename_node) {
          owned = candidate;
          break;
        }
      }
      if (owned != nullptr &&
          _canvas_rename_graph->renameNode(owned, _canvas_rename_buffer)) {
        PushUndo(*_canvas_rename_graph,
                 std::make_unique<ui::RenameNodeCommand>(old_name,
                                                         _canvas_rename_buffer),
                 true);
        _controller.markDirty();
      }
    }
    CancelCanvasRename();
  }

  void ShowCanvasRenamePopup() {
    // 在顶层打开，避免 popup ID 挂到已经在开的节点菜单上。
    if (_canvas_rename_requested) {
      _canvas_rename_requested = false;
      ImGui::OpenPopup("Canvas Node Rename");
    }
    if (!ImGui::BeginPopup("Canvas Node Rename")) {
      return;
    }
    ImGui::TextUnformatted("Rename Node");
    constexpr std::size_t kTextTerminatorSize = 1;
    const std::size_t text_buffer_size =
        _canvas_rename_buffer.capacity() + kTextTerminatorSize;
    constexpr float kCanvasRenameWidthEpx = 220.0f;
    ImGui::SetNextItemWidth(kCanvasRenameWidthEpx *
                            ImGui::GetStyle().FontScaleMain);
    if (_canvas_rename_focus) {
      ImGui::SetKeyboardFocusHere();
      _canvas_rename_focus = false;
    }
    const bool empty = _canvas_rename_buffer.empty();
    const bool taken = CanvasRenameTaken();
    if (taken) {
      ImGui::PushStyleColor(
          ImGuiCol_Border,
          ToImVec4(theme_detail::CurrentTokens().error_foreground));
    }
    const bool enter = ImGui::InputText(
        "##CanvasRename", _canvas_rename_buffer.data(), text_buffer_size,
        ImGuiInputTextFlags_CallbackResize |
            ImGuiInputTextFlags_EnterReturnsTrue |
            ImGuiInputTextFlags_AutoSelectAll,
        ResizePropertyText, &_canvas_rename_buffer);
    if (taken) {
      ImGui::PopStyleColor();
      ImGui::TextUnformatted("Name already exists in this graph");
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      CancelCanvasRename();
      ImGui::CloseCurrentPopup();
    } else if ((enter || ImGui::Button("OK")) && !empty && !taken) {
      CommitCanvasRename();
      ImGui::CloseCurrentPopup();
    } else {
      ImGui::SameLine();
      if (ImGui::Button("Cancel")) {
        CancelCanvasRename();
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::EndPopup();
  }

  // 画 Explorer 就地重命名输入框；回车或失焦提交，Esc 取消。
  // 调用前须已 PushExplorerRenameStyle，树行高度才和输入框一致。
  void DrawRenameInput() {
    constexpr float kFillWidth = -FLT_MIN;
    ImGui::SetNextItemWidth(kFillWidth);
    // 刚进入重命名时把焦点放到输入框并全选。
    if (_rename_focus) {
      ImGui::SetKeyboardFocusHere();
      _rename_focus = false;
    }
    constexpr std::size_t kTextTerminatorSize = 1;
    const std::size_t text_buffer_size =
        _rename_buffer.capacity() + kTextTerminatorSize;
    const bool empty = _rename_buffer.empty();
    const bool taken = !empty && RenameNameTaken();
    if (taken) {
      ImGui::PushStyleColor(ImGuiCol_Border,
                            ToImVec4(theme_detail::CurrentTokens().error_foreground));
    }
    const bool enter_pressed = ImGui::InputText(
        "##ExplorerRename", _rename_buffer.data(), text_buffer_size,
        ImGuiInputTextFlags_CallbackResize |
            ImGuiInputTextFlags_EnterReturnsTrue |
            ImGuiInputTextFlags_AutoSelectAll,
        ResizePropertyText, &_rename_buffer);
    const bool item_hovered = ImGui::IsItemHovered();
    const bool item_focused = ImGui::IsItemFocused();
    if (taken) {
      ImGui::PopStyleColor();
    }
    // Esc 取消，不改原来的名字。
    if (item_focused && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      CancelRename();
      return;
    }
    // 空名回车＝取消，保留原名。
    if (enter_pressed && empty) {
      CancelRename();
      return;
    }
    // 撞名回车＝停在编辑态，下一帧重新聚焦。
    if (enter_pressed && taken) {
      _rename_focus = true;
      if (item_hovered || item_focused) {
        ImGui::SetTooltip("%s", RenameTakenHint());
      }
      return;
    }
    // 回车提交。
    if (enter_pressed) {
      CommitRename();
      return;
    }
    // 失焦：撞名则回退，否则提交。
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      if (taken) {
        CancelRename();
      } else {
        CommitRename();
      }
      return;
    }
    // 悬停或聚焦在撞名输入框上时提示原因。
    if (taken && (item_hovered || item_focused)) {
      ImGui::SetTooltip("%s", RenameTakenHint());
    }
  }

  // 从图里删掉一个节点，并清掉选中和落位记录。
  void RemoveNodeFromUi(const std::shared_ptr<tg::Graph>& graph,
                        const std::shared_ptr<tg::Node>& node,
                        std::shared_ptr<tg::Node>* selected_node) {
    // 空图或空节点不删。
    if (graph == nullptr || node == nullptr) {
      return;
    }
    tg::Node* const raw = node.get();
    graph->removeNode(node);
    _placed_nodes.erase(raw);
    _node_poses.erase(raw);
    _select_pasted_nodes.erase(
        std::remove(_select_pasted_nodes.begin(), _select_pasted_nodes.end(),
                    raw),
        _select_pasted_nodes.end());
    _controller.markDirty();
    // 删的就是当前选中时，属性表和状态栏不再指向它。
    _focus.ClearSelectionIfNode(raw);
    // 本帧局部选中也指向它时一起清掉。
    if (selected_node != nullptr && selected_node->get() == raw) {
      selected_node->reset();
    }
  }

  // 按当前选中地址，在当前图里找到节点并删掉。
  void RemoveSelectedNode(std::shared_ptr<tg::Node>* selected_node) {
    const auto& graphs = _controller.getGraphs();
    // 没有图或没有选中就不删。
    if (graphs.empty() || _focus.selected_node() == nullptr) {
      return;
    }
    std::shared_ptr<tg::Node> owned;
    for (const auto& candidate : graphs[_focus.selected_graph_index()]->getNodes()) {
      // 地址相同就是要删的选中节点。
      if (candidate.get() == _focus.selected_node()) {
        owned = candidate;
        break;
      }
    }
    // 选中地址已经不在当前图里。
    if (owned == nullptr) {
      return;
    }
    DeleteSamplesFromUi(graphs[_focus.selected_graph_index()],
                        {SampleNode(owned)}, selected_node);
  }

  // 活动页签里的图；Welcome、快捷键页或没有页签时为 nullptr。
  std::shared_ptr<tg::Graph> ActiveEditorGraph() const {
    const auto& editors = _focus.open_editors();
    const int active = _focus.active_editor();
    if (active < 0 || active >= static_cast<int>(editors.size())) {
      return nullptr;
    }
    if (editors[active].kind == EditorTab::Kind::Graph) {
      return editors[active].graph;
    }
    return nullptr;
  }

  // 把工程里已有的图打开成编辑区页签；已打开则只激活。
  void OpenGraphInEditor(const std::shared_ptr<tg::Graph>& graph) {
    _focus.OpenGraphInEditor(graph, _controller.getGraphs());
  }

  // 打开或激活 Welcome 页签。
  void OpenWelcomeInEditor() {
    _focus.OpenWelcomeInEditor();
  }

  // 打开或激活快捷键页签。不创建节点。
  void OpenKeyboardShortcutsInEditor() {
    _focus.OpenKeyboardShortcuts();
  }

  void CloseWelcomeIfOpen() {
    _focus.CloseWelcomeIfOpen(_controller.getGraphs());
    PersistSession();
  }

  // 按图指针把当前图下标对到工程列表里。
  void SyncSelectedItemToGraph(const std::shared_ptr<tg::Graph>& graph) {
    _focus.SyncSelectedItemToGraph(graph, _controller.getGraphs());
  }

  // 关掉编辑区里某一页；没有页签时显示打开指引。
  void CloseEditorAt(int editor_index) {
    tg::Graph* closing_graph = nullptr;
    // 关的是 graph 页签时，历史跟着页签一起丢。
    if (editor_index >= 0 &&
        editor_index < static_cast<int>(_focus.open_editors().size())) {
      const EditorTab& tab = _focus.open_editors()[editor_index];
      if (tab.kind == EditorTab::Kind::Graph && tab.graph != nullptr) {
        closing_graph = tab.graph.get();
      }
    }
    if (_focus.CloseEditorAt(editor_index, _controller.getGraphs())) {
      if (closing_graph != nullptr) {
        _undo_stacks.erase(closing_graph);
      }
      PersistSession();
    }
  }

  // 工程里已经没有的图，从打开的页签里清掉；Welcome 保留。
  void PruneOpenEditors() {
    const int before = static_cast<int>(_focus.open_editors().size());
    _focus.PruneOpenEditors(_controller.getGraphs());
    if (static_cast<int>(_focus.open_editors().size()) != before) {
      PersistSession();
    }
    // 已经不在任何页签里的图不再保留历史。
    std::unordered_set<tg::Graph*> open_graphs;
    for (const EditorTab& tab : _focus.open_editors()) {
      if (tab.kind == EditorTab::Kind::Graph && tab.graph != nullptr) {
        open_graphs.insert(tab.graph.get());
      }
    }
    for (auto it = _undo_stacks.begin(); it != _undo_stacks.end();) {
      if (open_graphs.count(it->first) == 0) {
        it = _undo_stacks.erase(it);
      } else {
        ++it;
      }
    }
  }

  // 在编辑区没有打开页签时居中显示引导。
  static void ShowEmptyEditorPrompt() {
    constexpr const char* kHeading = "No editor is open";
    constexpr const char* kOpenGraphHint =
        "Select a graph in Explorer to open it.";
    constexpr const char* kCreateGraphHint =
        "Use + Graph to create a graph.";
    constexpr float kHalfSpan = 0.5f;
    const ImVec2 content_min = ImGui::GetCursorScreenPos();
    const ImVec2 content_size = ImGui::GetContentRegionAvail();
    const ImVec2 heading_size = ImGui::CalcTextSize(kHeading);
    const ImVec2 open_graph_size = ImGui::CalcTextSize(kOpenGraphHint);
    const ImVec2 create_graph_size = ImGui::CalcTextSize(kCreateGraphHint);
    const float line_spacing = ImGui::GetStyle().ItemSpacing.y;
    const float content_height =
        heading_size.y + line_spacing + open_graph_size.y + line_spacing +
        create_graph_size.y;
    float line_y =
        content_min.y + (content_size.y - content_height) * kHalfSpan;
    auto draw_centered_line = [&](const char* text, const ImVec2& text_size,
                                  bool disabled) {
      ImGui::SetCursorScreenPos(ImVec2(
          content_min.x + (content_size.x - text_size.x) * kHalfSpan, line_y));
      if (disabled) {
        ImGui::TextDisabled("%s", text);
      } else {
        ImGui::TextUnformatted(text);
      }
      line_y += text_size.y + line_spacing;
    };
    draw_centered_line(kHeading, heading_size, false);
    draw_centered_line(kOpenGraphHint, open_graph_size, true);
    draw_centered_line(kCreateGraphHint, create_graph_size, true);
  }

  // 画编辑区页签条：整块自绘，标题和关闭在同一底上。
  void DrawEditorTabs() {
    // 没有打开的页签就不画页签条。
    if (_focus.open_editors().empty()) {
      return;
    }
    constexpr float kHalfSpan = 0.5f;
    const float ui_scale = ImGui::GetStyle().FontScaleMain;
    const float tab_height = kEditorTabHeightEpx * ui_scale;
    const float pad_x = kEditorTabHorizontalPaddingEpx * ui_scale;
    const float close_size = kEditorTabCloseSizeEpx * ui_scale;
    const float label_close_gap = kEditorTabLabelCloseGapEpx * ui_scale;
    const float close_glyph_half = kEditorTabCloseGlyphHalfEpx * ui_scale;
    const ImVec2 bar_min = ImGui::GetCursorScreenPos();
    const float bar_width = ImGui::GetContentRegionAvail().x;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const EditorTabPalette tab_palette = MakeEditorTabPalette();
    const bool tab_bar_focused = ImGui::IsWindowFocused();
    draw->AddRectFilled(bar_min,
                        ImVec2(bar_min.x + bar_width, bar_min.y + tab_height),
                        tab_palette.tab_bar_bg);
    int close_index = -1;
    float tab_x = bar_min.x;
    for (int editor_index = 0;
         editor_index < static_cast<int>(_focus.open_editors().size()); ++editor_index) {
      const EditorTab& tab = _focus.open_editors()[editor_index];
      const bool is_graph = tab.kind == EditorTab::Kind::Graph;
      if (is_graph && tab.graph == nullptr) {
        continue;
      }
      const char* name = kWelcomeTabLabel;
      if (tab.kind == EditorTab::Kind::KeyboardShortcuts) {
        name = kKeyboardShortcutsTabLabel;
      } else if (is_graph) {
        name = tab.graph->getName().c_str();
      }
      const ImVec2 label_size = ImGui::CalcTextSize(name);
      const float tab_width =
          pad_x + label_size.x + label_close_gap + close_size + pad_x;
      ImGui::PushID(is_graph ? tab.graph.get() : nullptr);
      ImGui::PushID(editor_index);
      ImGui::SetCursorScreenPos(ImVec2(tab_x, bar_min.y));
      ImGui::InvisibleButton("##EditorTab", ImVec2(tab_width, tab_height));
      const bool tab_hovered = ImGui::IsItemHovered();
      const bool tab_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
      const ImVec2 tab_min = ImGui::GetItemRectMin();
      const ImVec2 tab_max = ImGui::GetItemRectMax();
      const float close_left = tab_max.x - pad_x - close_size;
      const float close_top =
          tab_min.y + (tab_height - close_size) * kHalfSpan;
      const ImVec2 close_min(close_left, close_top);
      const ImVec2 close_max(close_left + close_size, close_top + close_size);
      const ImVec2 mouse = ImGui::GetIO().MousePos;
      const bool close_hovered =
          tab_hovered && mouse.x >= close_min.x && mouse.x < close_max.x &&
          mouse.y >= close_min.y && mouse.y < close_max.y;
      const bool is_active = editor_index == _focus.active_editor();
      ImU32 tab_bg = tab_palette.inactive_bg;
      // 活动页用编辑区底色，悬停用高亮，其余用页签栏色。
      if (is_active) {
        tab_bg = tab_palette.active_bg;
      } else if (tab_hovered) {
        tab_bg = tab_palette.hover_bg;
      }
      draw->AddRectFilled(tab_min, tab_max, tab_bg);
      // 活动页顶部 1px 焦点线；窗口未聚焦时用 unfocused 色。
      if (is_active) {
        const float top_line = kEditorTabSeparatorThicknessEpx * ui_scale;
        draw->AddRectFilled(
            tab_min, ImVec2(tab_max.x, tab_min.y + top_line),
            tab_bar_focused ? tab_palette.active_top_border
                            : tab_palette.unfocused_active_top_border);
      }
      // 非活动页右侧画一条细线，隔开相邻页签。
      if (!is_active) {
        const float separator = kEditorTabSeparatorThicknessEpx * ui_scale;
        draw->AddLine(ImVec2(tab_max.x - separator, tab_min.y),
                      ImVec2(tab_max.x - separator, tab_max.y),
                      tab_palette.border);
      }
      const ImVec2 text_pos(
          tab_min.x + pad_x,
          tab_min.y + (tab_height - label_size.y) * kHalfSpan);
      const ImU32 tab_text =
          is_active ? tab_palette.active_text : tab_palette.inactive_text;
      draw->AddText(text_pos, tab_text, name);
      // 关闭钮只在悬停时铺底，嵌在页签里。
      if (close_hovered) {
        draw->AddRectFilled(close_min, close_max, tab_palette.close_hover_bg);
      }
      const ImVec2 close_center(
          (close_min.x + close_max.x) * kHalfSpan,
          (close_min.y + close_max.y) * kHalfSpan);
      draw->AddLine(
          ImVec2(close_center.x - close_glyph_half,
                 close_center.y - close_glyph_half),
          ImVec2(close_center.x + close_glyph_half,
                 close_center.y + close_glyph_half),
          tab_text);
      draw->AddLine(
          ImVec2(close_center.x + close_glyph_half,
                 close_center.y - close_glyph_half),
          ImVec2(close_center.x - close_glyph_half,
                 close_center.y + close_glyph_half),
          tab_text);
      // 点在关闭区就叉掉，否则激活这一页。
      if (tab_clicked) {
        if (close_hovered) {
          close_index = editor_index;
        } else {
          _focus.active_editor() = editor_index;
          if (is_graph) {
            SyncSelectedItemToGraph(tab.graph);
          }
        }
      }
      ImGui::PopID();
      ImGui::PopID();
      tab_x += tab_width;
    }
    // 编辑区工具栏：仅 Graph 页签；与页签条同高、文字垂直居中（对照 Editor Actions）。
    const bool graph_editor_active =
        _focus.active_editor() >= 0 &&
        _focus.active_editor() <
            static_cast<int>(_focus.open_editors().size()) &&
        _focus.open_editors()[_focus.active_editor()].kind ==
            EditorTab::Kind::Graph;
    if (graph_editor_active) {
      const ImVec2 action_label_size =
          ImGui::CalcTextSize(kShowTypeNamesActionLabel);
      const float action_width = action_label_size.x + pad_x * kFramePaddingSideCount;
      const float action_x = bar_min.x + bar_width - pad_x - action_width;
      ImGui::SetCursorScreenPos(ImVec2(action_x, bar_min.y));
      ImGui::InvisibleButton("##ShowTypeNames",
                             ImVec2(action_width, tab_height));
      const bool action_hovered = ImGui::IsItemHovered();
      if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        ExecuteCommand("editor.showTypeNames");
      }
      const ImVec2 action_min = ImGui::GetItemRectMin();
      const ImVec2 action_max = ImGui::GetItemRectMax();
      // 按下态 / 悬停用列表高亮，不另起一块「假页签」底。
      if (_show_port_type_names || action_hovered) {
        draw->AddRectFilled(action_min, action_max, tab_palette.hover_bg);
      }
      const ImVec2 action_text_pos(
          action_min.x + pad_x,
          action_min.y + (tab_height - action_label_size.y) * kHalfSpan);
      draw->AddText(action_text_pos, tab_palette.active_text,
                    kShowTypeNamesActionLabel);
    }
    // 占位后把光标钉回页签条底边，避免再吃一层 ItemSpacing 与画布留缝。
    ImGui::SetCursorScreenPos(
        ImVec2(bar_min.x, bar_min.y + tab_height));
    ImGui::Dummy(ImVec2(bar_width, 0.0f));
    ImGui::SetCursorScreenPos(
        ImVec2(bar_min.x, bar_min.y + tab_height));
    // 点过叉再关，避免边扫边改页签列表。
    if (close_index >= 0) {
      CloseEditorAt(close_index);
    }
  }

  // 删掉一张图，清掉其上节点的选中和落位，并夹回当前下标。
  void RemoveGraphFromUi(const std::shared_ptr<tg::Graph>& graph,
                         std::shared_ptr<tg::Node>* selected_node) {
    // 空图指针不删。
    if (graph == nullptr) {
      return;
    }
    const auto& graphs = _controller.getGraphs();
    int removed_index = -1;
    for (int graph_index = 0; graph_index < static_cast<int>(graphs.size());
         ++graph_index) {
      // 地址相同就是要删的那一张。
      if (graphs[graph_index] == graph) {
        removed_index = graph_index;
        break;
      }
    }
    // 图已经不在工程里。
    if (removed_index < 0) {
      return;
    }
    for (int editor_index = 0;
         editor_index < static_cast<int>(_focus.open_editors().size());
         ++editor_index) {
      // 编辑区里打开了这张图就先关掉页签。
      if (_focus.open_editors()[editor_index].kind == EditorTab::Kind::Graph &&
          _focus.open_editors()[editor_index].graph == graph) {
        CloseEditorAt(editor_index);
        break;
      }
    }
    for (const auto& node : graph->getNodes()) {
      _placed_nodes.erase(node.get());
      // 选中落在这张图的节点上时一起清掉。
      _focus.ClearSelectionIfNode(node.get());
      if (selected_node != nullptr && selected_node->get() == node.get()) {
        selected_node->reset();
      }
    }
    _collapsed_graphs.erase(graph.get());
    _undo_stacks.erase(graph.get());
    _controller.removeGraph(graph);
    const auto& graphs_after = _controller.getGraphs();
    _focus.AdjustAfterGraphRemoved(removed_index,
                                   static_cast<int>(graphs_after.size()));
  }

  enum class ProjectPathAction { None, New, Open, SaveAs };

  // 窗口标题文案：无工程为应用名；有工程为「工程名[*] - 应用名」。
  std::string BuildWindowTitleText() const {
    constexpr const char* kTitleSeparator = " - ";
    constexpr const char* kDirtyMarker = "*";
    if (!_controller.hasProjectRoot()) {
      return kWindowTitle;
    }
    std::string title = _controller.projectName();
    if (_controller.isDirty()) {
      title += kDirtyMarker;
    }
    title += kTitleSeparator;
    title += kWindowTitle;
    return title;
  }

  void ResetUiProjectState() {
    _focus.Reset();
    _placed_nodes.clear();
    _node_poses.clear();
    _undo_stacks.clear();
    _project_undo_stack.Clear();
    _move_snapshot = ImGuiMoveSnapshot{};
    _property_edit = ImGuiPropertyEditState{};
    _node_clipboard = NodeClipboard{};
    _graph_clipboard = GraphClipboard{};
    _select_pasted_nodes.clear();
    _canvas_menu_node = nullptr;
    _pin_context_port = nullptr;
    _rename_graph.reset();
    _rename_node_graph.reset();
    _rename_node.reset();
    _graph_pending_remove.reset();
    _collapsed_graphs.clear();
    _rename_buffer.clear();
    _rename_focus = false;
    _armed_placement_waits_for_release = false;
    _shortcut_filter.clear();
    _search_filter.clear();
    _log_filter.clear();
    _canvas_view_user_owned = false;
  }

  void PersistSession() {
    _session.open_graph_names.clear();
    for (const EditorTab& tab : _focus.open_editors()) {
      if (tab.kind == EditorTab::Kind::Graph && tab.graph != nullptr) {
        _session.open_graph_names.push_back(tab.graph->getName());
      }
    }
    _session.view_windows.clear();
    for (const ViewWindow& view : _parts.view_windows) {
      _session.view_windows[view.id] = view.open;
    }
    for (const MonitorView& view : _parts.monitor_views) {
      _session.view_windows[view.id] = view.open;
    }
    _session.log_auto_scroll = _log_auto_scroll;
    if (_controller.hasProjectRoot()) {
      _session.last_root = _controller.projectRoot().generic_string();
    }
    _session.save();
  }

  void RememberOpenedProject(const std::filesystem::path& root) {
    _session.touchRecent(root);
    PersistSession();
  }

  void RememberBrowseDir(const std::string& picked_path) {
    const std::string directory =
        NativePathDialog::directoryOfPickedPath(picked_path);
    if (directory.empty()) {
      return;
    }
    _session.last_browse_dir = directory;
    PersistSession();
  }

  void RestoreOpenGraphTabs() {
    const auto& graphs = _controller.getGraphs();
    for (const std::string& name : _session.open_graph_names) {
      for (const auto& graph : graphs) {
        if (graph != nullptr && graph->getName() == name) {
          OpenGraphInEditor(graph);
          break;
        }
      }
    }
  }

  // 路径是否指向同一工程根（会话恢复与 main 已打开的根比对）。
  static bool SameProjectRoot(const std::filesystem::path& left,
                              const std::filesystem::path& right) {
    if (left.empty() || right.empty()) {
      return false;
    }
    std::error_code error;
    if (std::filesystem::equivalent(left, right, error)) {
      return true;
    }
    return left.lexically_normal() == right.lexically_normal();
  }

  void TryRestoreOrShowWelcome() {
    // main 已用 core 打开工程：再开一次以灌入画布坐标，并按需恢复页签。
    if (_controller.hasProjectRoot()) {
      const std::filesystem::path root = _controller.projectRoot();
      const bool restore_tabs =
          !_session.last_root.empty() &&
          SameProjectRoot(std::filesystem::path(_session.last_root), root);
      OpenProjectFromPath(root, restore_tabs);
      return;
    }
    if (!_session.last_root.empty()) {
      const std::filesystem::path root(_session.last_root);
      if (OpenProjectFromPath(root, true)) {
        return;
      }
      _session.removeRecent(root);
      _session.save();
    }
    OpenWelcomeInEditor();
  }

  // restore_session_tabs：启动恢复时按 session 打开图；菜单打开则不自动开图。
  bool OpenProjectFromPath(const std::filesystem::path& root,
                           bool restore_session_tabs = false) {
    if (root.empty()) {
      return false;
    }
    const std::vector<std::string> graphs_to_restore =
        restore_session_tabs ? _session.open_graph_names
                             : std::vector<std::string>{};
    std::unordered_map<tg::Node*, tg::NodePose> pending_poses;
    const tg::NodePoseSetter pose_setter =
        [&pending_poses](tg::Node& node, const tg::NodePose& pose) {
          pending_poses[&node] = pose;
        };
    if (!_controller.openProject(root, pose_setter)) {
      tg::logError("Open project failed: {}", root.string());
      return false;
    }
    ResetUiProjectState();
    _node_poses = std::move(pending_poses);
    _session.touchRecent(root);
    _session.open_graph_names = graphs_to_restore;
    if (restore_session_tabs) {
      RestoreOpenGraphTabs();
    }
    // 打开成功即锚定 Browse，避免 last_browse_dir 空时落到盘符根。
    RememberBrowseDir(root.string());
    PersistSession();
    return true;
  }

  // Welcome 页：Start（New/Open）与 Recent。
  // 编辑区快捷键页：过滤框在内容顶部，下面三列。不另做工具栏。
  void ShowKeyboardShortcutsPage() {
    const float ui_scale = ImGui::GetStyle().FontScaleMain;
    const float margin = kSurfaceTextMarginEpx * ui_scale;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(ImVec2(origin.x + margin, origin.y + margin));
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("##KeyboardShortcuts",
                      ImVec2(avail.x - margin, avail.y - margin), false);
    constexpr std::size_t kTextTerminatorSize = 1;
    if (_shortcut_filter.capacity() < kPickNodeKindFilterMinCapacity) {
      _shortcut_filter.reserve(kPickNodeKindFilterMinCapacity);
    }
    const std::size_t filter_buffer_size =
        _shortcut_filter.capacity() + kTextTerminatorSize;
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##ShortcutFilter", kShortcutFilterHint,
                             _shortcut_filter.data(), filter_buffer_size,
                             ImGuiInputTextFlags_CallbackResize,
                             ResizePropertyText, &_shortcut_filter);
    int visible_count = 0;
    const float table_height = ImGui::GetContentRegionAvail().y;
    if (ImGui::BeginTable(
            "##ShortcutTable", kShortcutColumnCount,
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp,
            ImVec2(0.0f, table_height))) {
      ImGui::TableSetupColumn("Command", ImGuiTableColumnFlags_WidthStretch,
                              kShortcutCommandColumnWeight);
      ImGui::TableSetupColumn("Keybinding", ImGuiTableColumnFlags_WidthStretch,
                              kShortcutKeyColumnWeight);
      ImGui::TableSetupColumn("When", ImGuiTableColumnFlags_WidthStretch,
                              kShortcutWhenColumnWeight);
      ImGui::TableSetupScrollFreeze(0, 1);
      ImGui::TableHeadersRow();
      std::size_t row_count = 0;
      const ShortcutRow* const rows = EditorShortcutCatalog(row_count);
      for (std::size_t row_index = 0; row_index < row_count; ++row_index) {
        const ShortcutRow& row = rows[row_index];
        if (!ShortcutRowMatches(row, _shortcut_filter)) {
          continue;
        }
        ++visible_count;
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(row.command);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(row.keybinding);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(row.when_clause);
      }
      if (visible_count == 0) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextDisabled("%s", kShortcutEmptyLabel);
      }
      ImGui::EndTable();
    }
    ImGui::EndChild();
  }

  void ShowWelcomePage() {
    ImGui::Spacing();
    ImGui::TextUnformatted(kWelcomeStartHeading);
    ImGui::Spacing();
    if (ImGui::Selectable("New Project...")) {
      BeginProjectPathDialog(ProjectPathAction::New);
    }
    if (ImGui::Selectable("Open Project...")) {
      BeginProjectPathDialog(ProjectPathAction::Open);
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextUnformatted(kWelcomeRecentHeading);
    ImGui::Spacing();
    if (_session.recent_roots.empty()) {
      ImGui::TextDisabled("No recent projects");
      return;
    }
    for (std::size_t recent_index = 0;
         recent_index < _session.recent_roots.size(); ++recent_index) {
      ImGui::PushID(static_cast<int>(recent_index));
      const std::string& recent_root = _session.recent_roots[recent_index];
      if (ImGui::Selectable(recent_root.c_str())) {
        OpenProjectFromPath(std::filesystem::path(recent_root));
      }
      ImGui::PopID();
    }
  }

  // 保存前把画布坐标刷进 _node_poses；必须先 SetCurrentEditor，标题栏菜单时当前 editor 为空。
  void SyncNodePosesFromEditor() {
    if (_editor == nullptr || _placed_nodes.empty()) {
      return;
    }
    ax::NodeEditor::SetCurrentEditor(_editor);
    for (tg::Node* node : _placed_nodes) {
      if (node == nullptr) {
        continue;
      }
      const ImVec2 position =
          ax::NodeEditor::GetNodePosition(ax::NodeEditor::NodeId(node));
      _node_poses[node] = tg::NodePose{position.x, position.y};
    }
    ax::NodeEditor::SetCurrentEditor(nullptr);
  }

  // 只读缓存；序列化回调里不再调 GetNodePosition，避免无当前 editor 闪退。
  tg::NodePoseGetter MakePoseGetter() {
    return [this](const tg::Node& node, tg::NodePose& out_pose) {
      tg::Node* const raw = const_cast<tg::Node*>(&node);
      const auto it = _node_poses.find(raw);
      if (it == _node_poses.end()) {
        return false;
      }
      out_pose = it->second;
      return true;
    };
  }

  tg::NodePoseSetter MakePoseSetter() {
    return [this](tg::Node& node, const tg::NodePose& pose) {
      _node_poses[&node] = pose;
      _placed_nodes.erase(&node);
    };
  }

  void BeginProjectPathDialog(ProjectPathAction action) {
    constexpr std::size_t kPathBufferMinCapacity = 256;
    _path_action = action;
    _path_buffer.clear();
    _path_buffer.reserve(kPathBufferMinCapacity);
    // SaveAs / Open / New：有工程则预填工程根，否则用上次 Browse。对照打开文件夹靠近当前工作区。
    if (_controller.hasProjectRoot()) {
      _path_buffer = _controller.projectRoot().string();
    } else if (!_session.last_browse_dir.empty()) {
      _path_buffer = _session.last_browse_dir;
    }
    _path_dialog_open = true;
    _path_focus = true;
  }

  void SaveCurrentProject() {
    if (!_controller.hasProjectRoot()) {
      BeginProjectPathDialog(ProjectPathAction::SaveAs);
      return;
    }
    SyncNodePosesFromEditor();
    if (!_controller.saveProject(MakePoseGetter())) {
      tg::logError("Save project failed");
    }
  }

  void ApplyProjectPathDialog() {
    const std::filesystem::path root(_path_buffer);
    if (root.empty()) {
      return;
    }
    if (_path_action == ProjectPathAction::New) {
      if (!_controller.newProject(root, root.filename().string())) {
        tg::logError("New project failed: {}", root.string());
        return;
      }
      ResetUiProjectState();
      RememberBrowseDir(root.string());
      RememberOpenedProject(root);
    } else if (_path_action == ProjectPathAction::Open) {
      // Browse 锚定在 OpenProjectFromPath 内完成。
      if (!OpenProjectFromPath(root)) {
        return;
      }
    } else if (_path_action == ProjectPathAction::SaveAs) {
      SyncNodePosesFromEditor();
      if (!_controller.saveProjectAs(root, MakePoseGetter())) {
        tg::logError("Save project as failed: {}", root.string());
        return;
      }
      RememberBrowseDir(root.string());
      RememberOpenedProject(root);
    }
    _path_action = ProjectPathAction::None;
  }

  // 节点种类选择浮层：过滤框与紧凑整行 Selectable。
  void ShowPickNodeKindPopup(
      const std::vector<std::shared_ptr<tg::Graph>>& graphs) {
    const float ui_scale = ImGui::GetStyle().FontScaleMain;
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(kPickNodeKindMinWidthEpx * ui_scale, 0.0f),
        ImVec2(FLT_MAX, FLT_MAX));
    PushQuickPickStyle();
    if (!ImGui::BeginPopup(kPickNodeKindPopupId)) {
      PopQuickPickStyle();
      return;
    }
    if (_focus.pick_filter_focus()) {
      ImGui::SetKeyboardFocusHere();
      _focus.pick_filter_focus() = false;
    }
    constexpr std::size_t kTextTerminatorSize = 1;
    if (_focus.pick_filter().capacity() < kPickNodeKindFilterMinCapacity) {
      _focus.pick_filter().reserve(kPickNodeKindFilterMinCapacity);
    }
    const std::size_t filter_buffer_size =
        _focus.pick_filter().capacity() + kTextTerminatorSize;
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint(
        "##PickNodeKindFilter", kPickNodeKindFilterHint,
        _focus.pick_filter().data(), filter_buffer_size,
        ImGuiInputTextFlags_CallbackResize, ResizePropertyText,
        &_focus.pick_filter());
    ImGui::Separator();
    int visible_count = 0;
    const auto& node_types = _controller.nodeTypes();
    // 先按首次出现顺序收集类别，再逐组输出；同类别可交错注册。
    std::vector<std::string> categories;
    for (const tg::NodeTypeInfo& kind : node_types) {
      if (std::find(categories.begin(), categories.end(), kind.category) ==
          categories.end()) {
        categories.push_back(kind.category);
      }
    }
    for (const std::string& category : categories) {
      int group_matches = 0;
      for (const tg::NodeTypeInfo& kind : node_types) {
        if (kind.category == category &&
            LabelMatchesFilter(kind.caption, _focus.pick_filter())) {
          ++group_matches;
        }
      }
      // 过滤后为空的组连标题都不画。
      if (group_matches == 0) {
        continue;
      }
      // 空类别是默认组，不画标题。
      if (!category.empty()) {
        ImGui::TextDisabled("%s", category.c_str());
      }
      for (int kind_index = 0;
           kind_index < static_cast<int>(node_types.size()); ++kind_index) {
        const tg::NodeTypeInfo& kind = node_types[kind_index];
        if (kind.category != category) {
          continue;
        }
        if (!LabelMatchesFilter(kind.caption, _focus.pick_filter())) {
          continue;
        }
        ++visible_count;
        if (!ImGui::Selectable(kind.caption.c_str(), false,
                               ImGuiSelectableFlags_SpanAllColumns)) {
          continue;
        }
        if (ArmPickedNodeKind(graphs, _focus, kind.type_id, kind.caption)) {
          _armed_placement_waits_for_release = true;
        }
        ImGui::CloseCurrentPopup();
      }
    }
    if (visible_count == 0) {
      ImGui::TextDisabled("%s", kPickNodeKindEmptyLabel);
    }
    ImGui::EndPopup();
    PopQuickPickStyle();
  }

  void ShowProjectPathDialog() {
    if (_path_dialog_open) {
      ImGui::OpenPopup(kProjectPathDialogId);
      _path_dialog_open = false;
    }
    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (!ImGui::BeginPopupModal(kProjectPathDialogId, nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
      return;
    }
    const char* prompt = "Project path";
    if (_path_action == ProjectPathAction::New) {
      prompt = "New project path";
    } else if (_path_action == ProjectPathAction::Open) {
      prompt = "Open project path";
    } else if (_path_action == ProjectPathAction::SaveAs) {
      prompt = "Save project path";
    }
    ImGui::TextUnformatted(prompt);
    if (_path_focus) {
      ImGui::SetKeyboardFocusHere();
      _path_focus = false;
    }
    const float ui_scale = ImGui::GetStyle().FontScaleMain;
    const float browse_width = ButtonWidth(kBrowseActionLabel);
    const float path_input_width = kProjectPathInputWidthEpx * ui_scale -
                                   browse_width -
                                   ImGui::GetStyle().ItemSpacing.x;
    ImGui::SetNextItemWidth(path_input_width > 0.0f
                                ? path_input_width
                                : kProjectPathInputWidthEpx * ui_scale);
    constexpr std::size_t kTextTerminatorSize = 1;
    const std::size_t text_buffer_size =
        _path_buffer.capacity() + kTextTerminatorSize;
    const bool enter = ImGui::InputText(
        "##ProjectPath", _path_buffer.data(), text_buffer_size,
        ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_EnterReturnsTrue,
        ResizePropertyText, &_path_buffer);
    ImGui::SameLine();
    if (Button(kBrowseActionLabel)) {
      const std::string default_path = NativePathDialog::resolveDefaultPath(
          _path_buffer, _session.last_browse_dir);
      const std::optional<std::string> picked = NativePathDialog::pickDirectory(
          kPickProjectDirectoryTitle, default_path);
      if (NativePathDialog::applyPickedPath(_path_buffer, picked)) {
        RememberBrowseDir(*picked);
      }
    }
    if (Button("OK", ButtonKind::Primary) || enter) {
      ApplyProjectPathDialog();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (Button("Cancel")) {
      _path_action = ProjectPathAction::None;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  tg::Controller& _controller;
  std::unordered_set<tg::Node*> _placed_nodes;
  std::unordered_map<tg::Node*, tg::NodePose> _node_poses;
  // 按打开的 graph 页签各存一份撤销历史；关页签、删图、切工程时清掉。
  std::unordered_map<tg::Graph*, ui::UndoStack> _undo_stacks;
  // 工程级撤销（图的新建/删除/改名），与各图栈按全局序号统一排序。
  ImGuiProjectUndoTarget _project_undo_target;
  ui::UndoStack _project_undo_stack;
  std::uint64_t _next_edit_seq = 0;
  ImGuiMoveSnapshot _move_snapshot;
  ImGuiPropertyEditState _property_edit;
  // 进程内节点剪贴板。不进系统剪贴板，换工程时丢掉。
  NodeClipboard _node_clipboard;
  // 进程内图剪贴板（整图：图名 + 节点/边/坐标），与节点剪贴板分开。
  GraphClipboard _graph_clipboard;
  // 粘贴出的节点要等下一帧画进编辑器后再选中。
  std::vector<tg::Node*> _select_pasted_nodes;
  // 画布节点菜单跨帧记住被右键的节点。
  tg::Node* _canvas_menu_node = nullptr;
  ProjectPathAction _path_action = ProjectPathAction::None;
  std::string _path_buffer;
  std::string _last_window_title;
  bool _path_dialog_open = false;
  bool _path_focus = false;
  bool _explorer_actions_press_hold = false;
  ax::NodeEditor::EditorContext* _editor = nullptr;
  // 工作台焦点：当前图、节点选中、页签、新建节点请求（Explorer/画布 pull）。
  WorkbenchFocus _focus;
  // 编辑区工具栏：画布端口在功能名下叠类型名（不进工程文件）。
  bool _show_port_type_names = false;
  // 端口类型下拉：普通浮层窗口，避开 ImGui 弹窗的自动定位。
  TypeSelectorPopupState _type_popup;
  // 引脚右键菜单跨帧记住端口；ShowPinContextMenu 只在打开当帧给出 PinId。
  tg::Node::Port* _pin_context_port = nullptr;
  // 选种类的那一击还按着时不落下，松开的那一帧也不创建。
  bool _armed_placement_waits_for_release = false;
  std::shared_ptr<tg::Graph> _graph_pending_remove;
  bool _graph_pending_cut = false;
  bool _graph_paste_pending = false;
  std::unordered_set<tg::Graph*> _collapsed_graphs;
  std::shared_ptr<tg::Graph> _rename_graph;
  std::shared_ptr<tg::Graph> _rename_node_graph;
  std::shared_ptr<tg::Node> _rename_node;
  std::string _rename_buffer;
  bool _rename_focus = false;
  // 画布节点改名的独立弹窗状态（不与主侧栏就地改名共用）。
  std::shared_ptr<tg::Graph> _canvas_rename_graph;
  tg::Node* _canvas_rename_node = nullptr;
  std::string _canvas_rename_buffer;
  bool _canvas_rename_focus = false;
  bool _canvas_rename_requested = false;
  // 快捷键页的过滤文字。不进工程文件。
  std::string _shortcut_filter;
  // Search 视图的输入。不进工程文件。
  std::string _search_filter;
  UiSession _session;
  WorkbenchParts _parts;
  bool _workbench_layout_dirty = false;
  MaximizedView _maximized_view = MaximizedView::None;
  float _primary_sidebar_ratio = 0.0f;
  float _secondary_sidebar_ratio = 0.0f;
  float _panel_height_ratio = 0.0f;
  std::vector<tg::LogRecord> _log_records;
  std::size_t _log_cursor = 0;
  std::string _log_filter;
  bool _log_auto_scroll = true;
  std::unordered_map<std::string, MonitorWindowState> _monitor_states;
  bool _dragging_window = false;
  bool _resizing_window = false;
  bool _resize_left = false;
  bool _resize_right = false;
  bool _resize_top = false;
  bool _resize_bottom = false;
  int _drag_grab_x = 0;
  int _drag_grab_y = 0;
  int _resize_origin_x = 0;
  int _resize_origin_y = 0;
  int _resize_origin_w = 0;
  int _resize_origin_h = 0;
  // 画布内容区最近一次非零尺寸，以及它已经连续相同的帧数。
  Vec2 _canvas_content_size;
  int _canvas_content_stable_frames = 0;
  int _canvas_content_wait_frames = 0;
  // 用户右键平移、滚轮或 Frame 之后，不再把视图原点拉回 (0, 0)。
  bool _canvas_view_user_owned = false;
};
