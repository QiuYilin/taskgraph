#pragma once

// 编辑区画布上的端口行：类型下拉、类型 chip 和引脚标签。
#include "PortPin.hpp"

#include <algorithm>
#include <cstddef>
#include <variant>

constexpr float kPortTypeSelectorPaddingEpx = 3.0f;
constexpr float kPortTypeSelectorRoundingEpx = 3.0f;
constexpr float kPortTypeChevronWidthEpx = 10.0f;
constexpr float kPortTypeChevronHalfEpx = 2.5f;
constexpr float kPortTypeChevronArmEpx = 3.0f;
constexpr int kPortTypeMenuColumns = 3;
constexpr float kOpenTypeMenuNodeZ = 1.0f;

constexpr float kTypeChipGapEpx = 2.0f;

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
