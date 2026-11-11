#include "UiBase.hpp"
#include "imgui_internal.h"
#include <imgui-node-editor/imgui_node_editor.h>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <string>
#include <unordered_set>
#include <variant>
#include <vector>

#include <taskgraph.hpp>

// 属性字符串变长时，把输入缓冲扩到这份字符串上。
static int ResizePropertyText(ImGuiInputTextCallbackData* data) {
  // 只处理变长请求，别的输入事件这里不改字符串。
  if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
    auto* text = static_cast<std::string*>(data->UserData);
    text->resize(static_cast<std::size_t>(data->BufTextLen));
    data->Buf = text->data();
  }
  return 0;
}

// 关掉新建停靠格上的标签，窄条才不会被标签盖住。
static void HideBuiltDockTab(ImGuiID node_id) {
  ImGuiDockNode* node = ImGui::DockBuilderGetNode(node_id);
  // 这一格还没建出来。
  if (node == nullptr) {
    return;
  }
  node->SetLocalFlags(node->LocalFlags | ImGuiDockNodeFlags_NoTabBar);
}

// 给新建的图起一个还没占用的名字。第一张叫 Graph，重名再加序号。
static std::string NextGraphName(
    const std::vector<std::shared_ptr<tg::Graph>>& graphs) {
  const std::string base_name = "Graph";
  for (std::size_t suffix = 1;; ++suffix) {
    const std::string candidate =
        suffix == 1 ? base_name : base_name + " " + std::to_string(suffix);
    bool name_taken = false;
    for (const auto& graph : graphs) {
      // 空图指针不算占用名字。
      if (graph != nullptr && graph->getName() == candidate) {
        name_taken = true;
        break;
      }
    }
    // 这个序号还没有同名图，就用它。
    if (!name_taken) {
      return candidate;
    }
  }
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

class Ui : public UiBase<Ui> {
 public:
  explicit Ui(tg::Controller& controller) : _controller(controller) {}

  void addNodeKind(std::string label,
                   std::function<std::shared_ptr<tg::Node>()> create) {
    _node_kinds.push_back(NodeKind{std::move(label), std::move(create)});
  }

  ~Ui() {
    if (_editor != nullptr) {
      ax::NodeEditor::DestroyEditor(_editor);
      _editor = nullptr;
    }
  }

  void StartUp() {
    tg::initLog();
    ax::NodeEditor::Config config;
    config.SettingsFile = nullptr;
    _editor = ax::NodeEditor::CreateEditor(&config);
  }

  void Update() {
    const auto& graphs = _controller.getGraphs();
    // 图可能被删空或变短，先把选中下标夹回有效范围，后面取图才不会越界。
    if (!graphs.empty()) {
      _selected_item = std::clamp(_selected_item, 0,
                                  static_cast<int>(graphs.size()) - 1);
    }
    PruneOpenEditors();
    std::shared_ptr<tg::Node> selected_node;
    // 选中地址还在当前图里才用，节点删掉后不再写悬空指针。
    if (!graphs.empty() && _selected_node != nullptr) {
      for (const auto& node : graphs[_selected_item]->getNodes()) {
        // 地址相同就是画布上点中的那个节点。
        if (node.get() == _selected_node) {
          selected_node = node;
          break;
        }
      }
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ShowWindowTitleBar(graphs);
    const float ui_scale = ImGui::GetStyle().FontScaleMain;
    const float title_bar_height = kTitleBarHeightEpx * ui_scale;
    const float dock_height = viewport->WorkSize.y > title_bar_height
                                  ? viewport->WorkSize.y - title_bar_height
                                  : 0.0f;
    const ImVec2 dock_pos(viewport->WorkPos.x,
                          viewport->WorkPos.y + title_bar_height);
    const ImVec2 dock_size(viewport->WorkSize.x, dock_height);
    ImGui::SetNextWindowPos(dock_pos);
    ImGui::SetNextWindowSize(dock_size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const ImGuiWindowFlags host_flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBackground;
    // Begin / End：铺满标题栏下面的工作区。主侧栏、编辑区、辅助侧栏、面板和状态栏停在里面。
    ImGui::Begin("MainDockHost", nullptr, host_flags);
    ImGui::PopStyleVar(3);

    const ImGuiID dockspace_id = ImGui::GetID("MainDockCustomTitle");
    const ImGuiWindowSettings* property_settings =
        ImGui::FindWindowSettingsByID(ImHashStr("Properties"));
    const bool property_docked =
        property_settings != nullptr && property_settings->DockId != 0;
    const ImGuiWindowSettings* output_settings =
        ImGui::FindWindowSettingsByID(ImHashStr("Output View"));
    const bool output_docked =
        output_settings != nullptr && output_settings->DockId != 0;
    const ImGuiWindowSettings* status_settings =
        ImGui::FindWindowSettingsByID(ImHashStr("Status Bar"));
    const bool status_docked =
        status_settings != nullptr && status_settings->DockId != 0;
    // 停靠还不存在，或侧栏、面板、状态栏没有停进格子时重建。
    if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr || !property_docked ||
        !output_docked || !status_docked) {
      ImGui::DockBuilderRemoveNode(dockspace_id);
      ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
      ImGui::DockBuilderSetNodeSize(dockspace_id, dock_size);
      constexpr int kGridColumnCount = 12;
      constexpr int kPrimarySidebarColumnCount = 2;
      constexpr int kSecondarySidebarColumnCount = 2;
      constexpr int kCenterColumnCount =
          kGridColumnCount - kPrimarySidebarColumnCount;
      constexpr float kPrimarySidebarWidthRatio =
          static_cast<float>(kPrimarySidebarColumnCount) /
          static_cast<float>(kGridColumnCount);
      constexpr float kSecondarySidebarWidthRatio =
          static_cast<float>(kSecondarySidebarColumnCount) /
          static_cast<float>(kCenterColumnCount);
      constexpr int kOutputPaneShare = 1;
      constexpr int kEditorPaneShare = 3;
      constexpr float kOutputPaneHeightRatio =
          static_cast<float>(kOutputPaneShare) /
          static_cast<float>(kOutputPaneShare + kEditorPaneShare);
      const float status_bar_height = kStatusBarHeightEpx * ui_scale;
      const float window_height = dock_size.y;
      // 状态栏按固定高度从底下切，宿主比这一条还矮时整段都给它。
      const float status_bar_height_ratio =
          window_height > status_bar_height ? status_bar_height / window_height
                                            : 1.0f;
      ImGuiID dock_status = 0;
      ImGuiID dock_work = 0;
      ImGuiID dock_project = 0;
      ImGuiID dock_center = 0;
      ImGuiID dock_property = 0;
      ImGuiID dock_canvas = 0;
      ImGuiID dock_output = 0;
      ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down,
                                  status_bar_height_ratio, &dock_status,
                                  &dock_work);
      ImGui::DockBuilderSplitNode(dock_work, ImGuiDir_Left,
                                  kPrimarySidebarWidthRatio, &dock_project,
                                  &dock_center);
      ImGui::DockBuilderSplitNode(dock_center, ImGuiDir_Right,
                                  kSecondarySidebarWidthRatio, &dock_property,
                                  &dock_canvas);
      ImGui::DockBuilderSplitNode(dock_canvas, ImGuiDir_Down,
                                  kOutputPaneHeightRatio, &dock_output,
                                  &dock_canvas);
      ImGui::DockBuilderDockWindow("Status Bar", dock_status);
      ImGui::DockBuilderDockWindow("Project", dock_project);
      ImGui::DockBuilderDockWindow("Properties", dock_property);
      ImGui::DockBuilderDockWindow("Canvas", dock_canvas);
      ImGui::DockBuilderDockWindow("Output View", dock_output);
      HideBuiltDockTab(dock_status);
      HideBuiltDockTab(dock_project);
      HideBuiltDockTab(dock_property);
      HideBuiltDockTab(dock_canvas);
      HideBuiltDockTab(dock_output);
      ImGui::DockBuilderFinish(dockspace_id);
    }
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    ImGui::End();

    // Begin / End：主侧栏里的 Explorer。折叠时不画图和节点，编辑区继续画。
    HideDockTab("Nodes###Project");
    if (ImGui::Begin("Nodes###Project")) {
      constexpr const char* kNewNodeActionLabel = "+ Node";
      constexpr const char* kNewGraphActionLabel = "+ Graph";
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted("Explorer");
      const float frame_pad_x = ImGui::GetStyle().FramePadding.x;
      const float node_button_width =
          ImGui::CalcTextSize(kNewNodeActionLabel).x +
          frame_pad_x * kFramePaddingSideCount;
      const float graph_button_width =
          ImGui::CalcTextSize(kNewGraphActionLabel).x +
          frame_pad_x * kFramePaddingSideCount;
      const float action_gap = ImGui::GetStyle().ItemSpacing.x;
      const float actions_width =
          node_button_width + action_gap + graph_button_width;
      const float actions_x =
          ImGui::GetWindowContentRegionMax().x - actions_width;
      ImGui::SameLine();
      ImGui::SetCursorPosX(actions_x);
      // Begin / End：没有图时禁用新建节点，节点必须落在一张图上。
      ImGui::BeginDisabled(graphs.empty());
      // 点 Node 才弹出种类，节点加到当前图。文件对应节点。
      if (ImGui::Button(kNewNodeActionLabel, ImVec2(node_button_width, 0.0f))) {
        _create_on_graph = _selected_item;
        ImGui::OpenPopup("PickNodeKind");
      }
      // 悬停才说明这个按钮会弹出节点种类。
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("New Node");
      }
      ImGui::EndDisabled();
      ImGui::SameLine();
      // 点 Graph 才新建一张空图，建完立刻命名，不自动打开编辑区页签。
      if (ImGui::Button(kNewGraphActionLabel,
                        ImVec2(graph_button_width, 0.0f))) {
        auto graph =
            std::make_shared<tg::Graph>(NextGraphName(graphs));
        _controller.addGraph(graph);
        _selected_item = static_cast<int>(graphs.size()) - 1;
        selected_node.reset();
        _selected_node = nullptr;
        _select_node_in_editor = nullptr;
        BeginRenameGraph(graph);
      }
      // 悬停才说明这个按钮会新建一张空图。
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("New Graph");
      }

      // 每张已创建的图都占一行，没有节点也留着。
      for (int graph_index = 0; graph_index < static_cast<int>(graphs.size());
           ++graph_index) {
        const auto& graph = graphs[graph_index];
        ImGui::PushID(graph.get());
        ImGuiTreeNodeFlags graph_flags = ImGuiTreeNodeFlags_SpanAvailWidth |
                                         ImGuiTreeNodeFlags_OpenOnArrow |
                                         ImGuiTreeNodeFlags_DefaultOpen;
        // 编辑区正在显示的图，或仅在侧栏选中的图，在树上高亮。
        if (graph_index == _selected_item) {
          graph_flags |= ImGuiTreeNodeFlags_Selected;
        }
        const bool renaming_graph = _rename_graph.get() == graph.get();
        bool graph_open = false;
        // 正在重命名时用空标签树节点 + 输入框，对照 Explorer 就地改名。
        if (renaming_graph) {
          PushExplorerRenameStyle();
          graph_flags |= ImGuiTreeNodeFlags_AllowOverlap;
          graph_open = ImGui::TreeNodeEx("##GraphRenameRow", graph_flags);
          ImGui::SameLine();
          DrawRenameInput();
          PopExplorerRenameStyle();
        } else {
          graph_open =
              ImGui::TreeNodeEx(graph->getName().c_str(), graph_flags);
          // 点在图名上打开编辑区页签，并清掉节点选中，Delete 才删图而不是删节点。
          if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            OpenGraphInEditor(graph);
            selected_node.reset();
            _selected_node = nullptr;
            _select_node_in_editor = nullptr;
          }
          // Begin / End：图右键菜单。New Node、Rename、Delete。
          if (ImGui::BeginPopupContextItem("GraphContext")) {
            OpenGraphInEditor(graph);
            selected_node.reset();
            _selected_node = nullptr;
            _select_node_in_editor = nullptr;
            // 选 New Node 才记下目标图，稍后打开种类列表。
            if (ImGui::MenuItem("New Node")) {
              _create_on_graph = graph_index;
              _open_pick_node_kind = true;
            }
            // 选 Rename 就地改图名。
            if (ImGui::MenuItem("Rename")) {
              BeginRenameGraph(graph);
            }
            // 选 Delete 才记下要删的图，遍历结束后再删，避免边扫边改列表。
            if (ImGui::MenuItem("Delete")) {
              _graph_pending_remove = graph;
            }
            ImGui::EndPopup();
          }
        }
        // 展开后才列出这张图上已经创建的节点。
        if (graph_open) {
          bool node_removed = false;
          for (const auto& node : graph->getNodes()) {
            ImGui::PushID(node.get());
            const bool node_is_selected = node.get() == _selected_node;
            ImGui::Selectable(node->getName().c_str(), node_is_selected);
            // 点中侧栏里的节点就打开该图编辑区，并让属性表跟着它。
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
              OpenGraphInEditor(graph);
              selected_node = node;
              _selected_node = node.get();
              _select_node_in_editor = node.get();
            }
            // Begin / End：节点右键菜单。New Node、Delete。节点名固定，不能重命名。
            if (ImGui::BeginPopupContextItem("NodeContext")) {
              OpenGraphInEditor(graph);
              selected_node = node;
              _selected_node = node.get();
              _select_node_in_editor = node.get();
              // 选 New Node 才记下目标图，稍后打开种类列表。
              if (ImGui::MenuItem("New Node")) {
                _create_on_graph = graph_index;
                _open_pick_node_kind = true;
              }
              // 选 Delete 才从图里删掉这个节点，并结束本轮节点遍历。
              if (ImGui::MenuItem("Delete")) {
                RemoveNodeFromUi(graph, node, &selected_node);
                node_removed = true;
              }
              ImGui::EndPopup();
            }
            ImGui::PopID();
            // 刚删过节点，节点列表迭代器不能再往下用。
            if (node_removed) {
              break;
            }
          }
          ImGui::TreePop();
        }
        ImGui::PopID();
      }

      // 图菜单里选了 Delete，在扫完树之后再删。
      if (_graph_pending_remove != nullptr) {
        RemoveGraphFromUi(_graph_pending_remove, &selected_node);
        _graph_pending_remove.reset();
      }
      // 右键菜单里选了 New Node，在 Explorer 窗口上打开种类列表。
      if (_open_pick_node_kind) {
        ImGui::OpenPopup("PickNodeKind");
        _open_pick_node_kind = false;
      }
      // Begin / End：节点种类选择。选出一种就创建到目标图上。
      if (ImGui::BeginPopup("PickNodeKind")) {
        for (int kind_index = 0;
             kind_index < static_cast<int>(_node_kinds.size()); ++kind_index) {
          // 点中一种才创建，没点中就留在列表里。
          if (ImGui::Selectable(_node_kinds[kind_index].label.c_str())) {
            // 目标图还在列表里才创建，图被删掉后不写越界下标。
            if (_create_on_graph >= 0 &&
                _create_on_graph < static_cast<int>(graphs.size())) {
              auto node = _node_kinds[kind_index].create();
              // 工厂没交出节点时，不往图里放空指针。
              if (node != nullptr) {
                graphs[_create_on_graph]->addNode(node);
                OpenGraphInEditor(graphs[_create_on_graph]);
                selected_node = node;
                _selected_node = node.get();
                _select_node_in_editor = node.get();
              }
            }
          }
        }
        ImGui::EndPopup();
      }
    }
    ImGui::End();

    // Begin / End：编辑区。有打开的 graph 页签才画对应画布，否则留空。
    HideDockTab("Canvas");
    if (ImGui::Begin("Canvas")) {
      DrawEditorTabs();
      // 至少打开了一张图才创建节点编辑器画布。
      if (_active_editor >= 0 &&
          _active_editor < static_cast<int>(_open_editors.size())) {
        ShowFlowCanvas();
      }
    }
    ImGui::End();

    // 画布读完选中后，再按最终节点找属性，侧栏点的节点不会被画布清掉。
    if (_selected_node != nullptr &&
        (selected_node == nullptr || selected_node.get() != _selected_node) &&
        !graphs.empty()) {
      selected_node.reset();
      for (const auto& node : graphs[_selected_item]->getNodes()) {
        // 地址相同就是当前要显示属性的节点。
        if (node.get() == _selected_node) {
          selected_node = node;
          break;
        }
      }
    }

    // Begin / End：辅助侧栏里的属性表。点中 Explorer 或编辑区上的节点后编辑它的属性。
    HideDockTab("Properties");
    if (ImGui::Begin("Properties")) {
      constexpr int kPropertyTableColumnCount = 2;
      constexpr float kFillColumnWidth = -FLT_MIN;
      // 有选中节点才画表，点在空白处时没有属性可改。
      if (selected_node != nullptr) {
        ImGui::PushID(selected_node.get());
        // Begin / End：两列属性表，左列名称，右列可编辑的值。
        if (ImGui::BeginTable("node_properties", kPropertyTableColumnCount,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
          ImGui::TableSetupColumn("Name");
          ImGui::TableSetupColumn("Value");
          ImGui::TableHeadersRow();
          int property_index = 0;
          for (const auto& property : selected_node->getProperties()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(property->name.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::PushID(property_index);
            // 属性值是整数，整数框直接改节点上的那一份。
            if (auto* number = std::get_if<int>(&property->value)) {
              ImGui::SetNextItemWidth(kFillColumnWidth);
              ImGui::InputInt("##value", number);
            }
            // 属性值是字符串，输入框直接改节点上的那一份。
            else if (auto* text = std::get_if<std::string>(&property->value)) {
              constexpr std::size_t kTextTerminatorSize = 1;
              const std::size_t text_buffer_size =
                  text->capacity() + kTextTerminatorSize;
              ImGui::SetNextItemWidth(kFillColumnWidth);
              ImGui::InputText("##value", text->data(), text_buffer_size,
                               ImGuiInputTextFlags_CallbackResize,
                               ResizePropertyText, text);
            }
            // 属性值是布尔，勾选框直接改节点上的那一份。
            else if (auto* flag = std::get_if<bool>(&property->value)) {
              ImGui::Checkbox("##value", flag);
            }
            ImGui::PopID();
            ++property_index;
          }
          ImGui::EndTable();
        }
        ImGui::PopID();
      }
    }
    ImGui::End();

    // 没在改文本时处理快捷键：F2 重命名图，Delete / Backspace 删除。
    if (!ImGui::GetIO().WantTextInput) {
      // F2：没有选中节点时才改当前图名；节点名固定，不能重命名。
      if (ImGui::IsKeyPressed(ImGuiKey_F2) && !graphs.empty() &&
          _selected_node == nullptr) {
        BeginRenameGraph(graphs[_selected_item]);
      }
      // Delete 或 Backspace：有选中节点就删节点，否则删当前图。
      if (ImGui::IsKeyPressed(ImGuiKey_Delete) ||
          ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
        if (_selected_node != nullptr) {
          RemoveSelectedNode(&selected_node);
        } else if (!graphs.empty()) {
          RemoveGraphFromUi(graphs[_selected_item], &selected_node);
        }
      }
    }

    // Begin / End：面板里的输出视图。只读显示节点的执行状态和结果。
    HideDockTab("Output View");
    if (ImGui::Begin("Output View")) {
      const std::size_t previous_cursor = _log_cursor;
      _log_cursor = tg::copyLogLinesSince(_log_cursor, _log_lines);
      // Begin / End：日志行。横向可滚动，长行不被窗口截断。
      ImGui::BeginChild("OutputLog", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None,
                        ImGuiWindowFlags_HorizontalScrollbar);
      for (const std::string& line : _log_lines) {
        ImGui::TextUnformatted(line.c_str());
      }
      // 有新行才滚到末尾，没有新行时保留用户往上翻的位置。
      if (_log_cursor != previous_cursor) {
        ImGui::SetScrollHereY(1.0f);
      }
      ImGui::EndChild();
    }
    ImGui::End();

    // Begin / End：状态栏。左侧是 Run 和当前图，右侧是选中的节点。
    HideDockTab("Status Bar");
    const float status_padding_y =
        (kStatusBarHeightEpx - kBodyFontSizeEpx) / kFramePaddingSideCount *
        ui_scale;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(kBetweenControlsEpx * ui_scale, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(kBetweenControlsEpx * ui_scale, status_padding_y));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    if (ImGui::Begin("Status Bar", nullptr, ImGuiWindowFlags_NoTitleBar)) {
      // Begin / End：没有图时禁用 Run，列表为空没有任务可跑。
      ImGui::BeginDisabled(graphs.empty());
      // 点状态栏上的 Run 才跑当前图。
      if (DrawBarAction("Run") && !graphs.empty()) {
        _controller.run(*graphs[_selected_item]);
      }
      ImGui::EndDisabled();
      // 有图才显示图名，点开后改当前图。
      if (!graphs.empty()) {
        ImGui::SameLine();
        // 点图名才弹出列表，用来改状态栏上的当前图。
        if (DrawBarAction(graphs[_selected_item]->getName().c_str())) {
          ImGui::OpenPopup("PickStatusGraph");
        }
        // Begin / End：状态栏上的图列表。点中一张就切到它。
        if (ImGui::BeginPopup("PickStatusGraph")) {
          for (int graph_index = 0;
               graph_index < static_cast<int>(graphs.size()); ++graph_index) {
            ImGui::PushID(graph_index);
            // 点中一张图才切换，节点不属于这张图时清掉选中。
            if (ImGui::Selectable(graphs[graph_index]->getName().c_str(),
                                  graph_index == _selected_item)) {
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
                _selected_node = nullptr;
                _select_node_in_editor = nullptr;
              }
            }
            ImGui::PopID();
          }
          ImGui::EndPopup();
        }
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
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
    HandleUndecoratedWindowChrome();
  }

  // 画窗口自己的标题栏：标题、左侧菜单栏 Run、最小化、最大化、关闭，不进停靠。
  void ShowWindowTitleBar(
      const std::vector<std::shared_ptr<tg::Graph>>& graphs) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float ui_scale = ImGui::GetStyle().FontScaleMain;
    const float title_bar_height = kTitleBarHeightEpx * ui_scale;
    const float title_padding_y =
        (kTitleBarHeightEpx - kBodyFontSizeEpx) / kFramePaddingSideCount *
        ui_scale;
    const float caption_width =
        kWindowCaptionButtonWidthEpx * ui_scale *
        static_cast<float>(kWindowCaptionButtonCount);
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, title_bar_height));
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(kBetweenControlsEpx * ui_scale, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(kBetweenControlsEpx * ui_scale, title_padding_y));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    const ImGuiWindowFlags title_flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBringToFrontOnFocus;
    // Begin / End：窗口标题栏。左侧是标题和菜单栏 Run，右侧是窗口按钮，整条不进停靠。
    ImGui::Begin("Window Title Bar", nullptr, title_flags);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(kWindowTitle);
    ImGui::SameLine(0.0f, kBetweenControlsEpx * ui_scale);
    // 点左侧菜单名 Run 打开菜单，对应 VS Code 标题栏菜单栏。
    if (DrawBarAction("Run")) {
      ImGui::OpenPopup("##TitleRunMenu");
    }
    // Begin / End：Run 菜单。选 Run Graph 才跑当前图。
    if (ImGui::BeginPopup("##TitleRunMenu")) {
      // 列表为空没有任务可跑。
      if (ImGui::MenuItem("Run Graph", nullptr, false, !graphs.empty()) &&
          !graphs.empty()) {
        _controller.run(*graphs[_selected_item]);
      }
      ImGui::EndPopup();
    }
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
    ImGui::End();
    ImGui::PopStyleVar(5);
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

  void ShowFlowCanvas() {
    constexpr float kInitialNodeStepX = 220.0f;
    const auto& graphs = _controller.getGraphs();

    ax::NodeEditor::SetCurrentEditor(_editor);
    ax::NodeEditor::Style& editor_style = ax::NodeEditor::GetStyle();
    editor_style.Colors[ax::NodeEditor::StyleColor_Bg] = kVsCodeEditorBackground;
    editor_style.Colors[ax::NodeEditor::StyleColor_Grid] = kVsCodeEditorGrid;
    // Begin / End：节点编辑器。图里已有的节点画在网格上。
    ax::NodeEditor::Begin("CanvasEditor");
    // 没有选中的图时不画节点。
    if (!graphs.empty()) {
      int column = 0;
      for (const auto& node : graphs[_selected_item]->getNodes()) {
        const ax::NodeEditor::NodeId node_id(node.get());
        const bool place_now = _placed_nodes.insert(node.get()).second;
        // 节点第一次出现才按列落位，之后改由编辑器记住拖动结果。
        if (place_now) {
          constexpr float kInitialNodePositionY = 0.0f;
          const ImVec2 position{static_cast<float>(column) * kInitialNodeStepX,
                                kInitialNodePositionY};
          ax::NodeEditor::SetNodePosition(node_id, position);
        }
        // Begin / End：一个图节点。端口和连线都来自节点基类。
        ax::NodeEditor::BeginNode(node_id);
        ImGui::TextUnformatted(node->getName().c_str());
        ImGui::BeginGroup();
        for (unsigned int port_index = 0; port_index < node->inputPortCount();
             ++port_index) {
          const tg::Node::Port* port = node->inputPort(port_index);
          // Begin / End：输入引脚。身份就是基类里的端口对象。
          ax::NodeEditor::BeginPin(ax::NodeEditor::PinId(port),
                                   ax::NodeEditor::PinKind::Input);
          // 有数据类型名才画字，没有对应数据的端口留空。
          if (!port->name.empty()) {
            ImGui::TextUnformatted(port->name.c_str());
          }
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
          // 有数据类型名才画字，没有对应数据的端口留空。
          if (!port->name.empty()) {
            ImGui::TextUnformatted(port->name.c_str());
          }
          ax::NodeEditor::EndPin();
        }
        ImGui::EndGroup();
        ax::NodeEditor::EndNode();
        ++column;
      }
      for (const auto& node : graphs[_selected_item]->getNodes()) {
        for (const auto& connection : node->outgoing()) {
          const tg::Node::Port* output = node->outputPort(connection.port_out);
          const tg::Node::Port* input =
              connection.adj == nullptr
                  ? nullptr
                  : connection.adj->inputPort(connection.port_in);
          // 两端端口都还在，才把这条已有连边画出来。
          if (output == nullptr || input == nullptr) {
            continue;
          }
          ax::NodeEditor::Link(ax::NodeEditor::LinkId(&connection),
                               ax::NodeEditor::PinId(output),
                               ax::NodeEditor::PinId(input));
        }
      }
      // Begin / End：处理正在拉的新连线。合法时只调用图已有的 connect。
      if (ax::NodeEditor::BeginCreate()) {
        ax::NodeEditor::PinId start_pin;
        ax::NodeEditor::PinId end_pin;
        // 两端都落到引脚上才判断能不能连。
        if (ax::NodeEditor::QueryNewLink(&start_pin, &end_pin) && start_pin &&
            end_pin) {
          auto* const start = start_pin.AsPointer<tg::Node::Port>();
          auto* const end = end_pin.AsPointer<tg::Node::Port>();
          const tg::Node::Port* output = nullptr;
          const tg::Node::Port* input = nullptr;
          // 从哪一端拉起都认，只要最后是输出连到输入。
          if (start != nullptr && end != nullptr && start->is_output &&
              !end->is_output) {
            output = start;
            input = end;
          } else if (start != nullptr && end != nullptr && !start->is_output &&
                     end->is_output) {
            output = end;
            input = start;
          }
          // 必须是一个输出对一个输入，并且不是同一个节点。
          if (output == nullptr || input == nullptr ||
              output->node == nullptr || input->node == nullptr ||
              output->node == input->node) {
            ax::NodeEditor::RejectNewItem();
          } else if (ax::NodeEditor::AcceptNewItem()) {
            graphs[_selected_item]->connect(output->node->shared_from_this(),
                                            output->index,
                                            input->node->shared_from_this(),
                                            input->index);
          }
        }
      }
      ax::NodeEditor::EndCreate();
      // Begin / End：处理删除。接受删节点，拒绝删连边。
      if (ax::NodeEditor::BeginDelete()) {
        ax::NodeEditor::NodeId deleted_node_id;
        while (ax::NodeEditor::QueryDeletedNode(&deleted_node_id)) {
          tg::Node* const raw = deleted_node_id.AsPointer<tg::Node>();
          std::shared_ptr<tg::Node> owned;
          for (const auto& candidate : graphs[_selected_item]->getNodes()) {
            // 地址相同就是编辑器要删的那个节点。
            if (candidate.get() == raw) {
              owned = candidate;
              break;
            }
          }
          // 节点还在图里才接受删除并调用 removeNode。
          if (owned != nullptr && ax::NodeEditor::AcceptDeletedItem()) {
            RemoveNodeFromUi(graphs[_selected_item], owned, nullptr);
          } else {
            ax::NodeEditor::RejectDeletedItem();
          }
        }
        ax::NodeEditor::LinkId deleted_link_id;
        while (ax::NodeEditor::QueryDeletedLink(&deleted_link_id)) {
          ax::NodeEditor::RejectDeletedItem();
        }
      }
      ax::NodeEditor::EndDelete();
      // 侧栏点过的节点这一帧才画出来，先在编辑器里选中。
      if (_select_node_in_editor != nullptr) {
        ax::NodeEditor::SelectNode(
            ax::NodeEditor::NodeId(_select_node_in_editor));
      }
    }
    ax::NodeEditor::End();
    tg::Node* const picked_in_explorer = _select_node_in_editor;
    // 编辑器结束时可能清掉侧栏这次点击，再选一次，属性表才跟得上。
    if (picked_in_explorer != nullptr) {
      ax::NodeEditor::SelectNode(ax::NodeEditor::NodeId(picked_in_explorer));
      _select_node_in_editor = nullptr;
    }
    constexpr int kSelectedNodeReadCount = 1;
    ax::NodeEditor::NodeId selected_id;
    const int selected_count =
        ax::NodeEditor::GetSelectedNodes(&selected_id, kSelectedNodeReadCount);
    _selected_node = nullptr;
    // 侧栏点的节点以侧栏为准，画布没选中时也不要把它清掉。
    if (picked_in_explorer != nullptr) {
      _selected_node = picked_in_explorer;
    } else if (selected_count > 0 && !graphs.empty()) {
      tg::Node* const candidate = selected_id.AsPointer<tg::Node>();
      bool still_in_graph = false;
      for (const auto& node : graphs[_selected_item]->getNodes()) {
        // 刚删掉的节点不能再当成选中。
        if (node.get() == candidate) {
          still_in_graph = true;
          break;
        }
      }
      // 还在当前图里才记下选中。
      if (still_in_graph) {
        _selected_node = candidate;
      }
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
  // 开始就地重命名一张图。
  void BeginRenameGraph(const std::shared_ptr<tg::Graph>& graph) {
    // 空图不改名。
    if (graph == nullptr) {
      return;
    }
    _rename_graph = graph;
    _rename_buffer = graph->getName();
    _rename_focus = true;
  }

  // 取消就地重命名，不写回名字。
  void CancelRename() {
    _rename_graph.reset();
    _rename_buffer.clear();
    _rename_focus = false;
  }

  // 把输入框里的名字写回图；空名不提交。节点名固定，不走这里。
  void CommitRename() {
    // 还在改图名。
    if (_rename_graph != nullptr) {
      // 空名保持原名，不写成空白。
      if (!_rename_buffer.empty()) {
        _rename_graph->setName(_rename_buffer);
      }
      CancelRename();
    }
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
    const bool enter_pressed = ImGui::InputText(
        "##ExplorerRename", _rename_buffer.data(), text_buffer_size,
        ImGuiInputTextFlags_CallbackResize |
            ImGuiInputTextFlags_EnterReturnsTrue |
            ImGuiInputTextFlags_AutoSelectAll,
        ResizePropertyText, &_rename_buffer);
    // Esc 取消，不改原来的名字。
    if (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      CancelRename();
      return;
    }
    // 回车或点到别处失焦时提交。
    if (enter_pressed || ImGui::IsItemDeactivatedAfterEdit()) {
      CommitRename();
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
    // 删的就是当前选中时，属性表和状态栏不再指向它。
    if (_selected_node == raw) {
      _selected_node = nullptr;
    }
    // 删的就是这一帧要在编辑器里选中的节点。
    if (_select_node_in_editor == raw) {
      _select_node_in_editor = nullptr;
    }
    // 本帧局部选中也指向它时一起清掉。
    if (selected_node != nullptr && selected_node->get() == raw) {
      selected_node->reset();
    }
  }

  // 按当前选中地址，在当前图里找到节点并删掉。
  void RemoveSelectedNode(std::shared_ptr<tg::Node>* selected_node) {
    const auto& graphs = _controller.getGraphs();
    // 没有图或没有选中就不删。
    if (graphs.empty() || _selected_node == nullptr) {
      return;
    }
    std::shared_ptr<tg::Node> owned;
    for (const auto& candidate : graphs[_selected_item]->getNodes()) {
      // 地址相同就是要删的选中节点。
      if (candidate.get() == _selected_node) {
        owned = candidate;
        break;
      }
    }
    // 选中地址已经不在当前图里。
    if (owned == nullptr) {
      return;
    }
    RemoveNodeFromUi(graphs[_selected_item], owned, selected_node);
  }

  // 把工程里已有的图打开成编辑区页签；已打开则只激活。
  void OpenGraphInEditor(const std::shared_ptr<tg::Graph>& graph) {
    // 空图不打开。
    if (graph == nullptr) {
      return;
    }
    for (int editor_index = 0;
         editor_index < static_cast<int>(_open_editors.size()); ++editor_index) {
      // 已经打开过就只切到这一页。
      if (_open_editors[editor_index] == graph) {
        _active_editor = editor_index;
        SyncSelectedItemToGraph(graph);
        return;
      }
    }
    _open_editors.push_back(graph);
    _active_editor = static_cast<int>(_open_editors.size()) - 1;
    SyncSelectedItemToGraph(graph);
  }

  // 按图指针把 _selected_item 对到工程列表里的下标。
  void SyncSelectedItemToGraph(const std::shared_ptr<tg::Graph>& graph) {
    const auto& graphs = _controller.getGraphs();
    for (int graph_index = 0; graph_index < static_cast<int>(graphs.size());
         ++graph_index) {
      // 找到同一张图就记下下标。
      if (graphs[graph_index] == graph) {
        _selected_item = graph_index;
        return;
      }
    }
  }

  // 关掉编辑区里某一页；没有页签时编辑区留空。
  void CloseEditorAt(int editor_index) {
    // 下标越界不关。
    if (editor_index < 0 ||
        editor_index >= static_cast<int>(_open_editors.size())) {
      return;
    }
    const std::shared_ptr<tg::Graph> closed = _open_editors[editor_index];
    // 关掉的图上若还有选中节点，属性表不再挂着它。
    if (closed != nullptr && _selected_node != nullptr) {
      for (const auto& node : closed->getNodes()) {
        // 选中就在这一页上。
        if (node.get() == _selected_node) {
          _selected_node = nullptr;
          _select_node_in_editor = nullptr;
          break;
        }
      }
    }
    _open_editors.erase(_open_editors.begin() + editor_index);
    // 全部叉掉后没有活动页。
    if (_open_editors.empty()) {
      _active_editor = -1;
      return;
    }
    // 关掉的页在活动页左边或就是活动页时，活动下标往前收。
    if (_active_editor >= editor_index) {
      --_active_editor;
    }
    // 活动下标不能小于 0。
    if (_active_editor < 0) {
      _active_editor = 0;
    }
    // 活动下标不能超出最后一页。
    if (_active_editor >= static_cast<int>(_open_editors.size())) {
      _active_editor = static_cast<int>(_open_editors.size()) - 1;
    }
    SyncSelectedItemToGraph(_open_editors[_active_editor]);
  }

  // 工程里已经没有的图，从打开的页签里清掉。
  void PruneOpenEditors() {
    const auto& graphs = _controller.getGraphs();
    for (int editor_index = static_cast<int>(_open_editors.size()) - 1;
         editor_index >= 0; --editor_index) {
      bool still_present = false;
      for (const auto& graph : graphs) {
        // 还在工程列表里就保留页签。
        if (graph == _open_editors[editor_index]) {
          still_present = true;
          break;
        }
      }
      // 图已被删掉，页签也关掉。
      if (!still_present) {
        CloseEditorAt(editor_index);
      }
    }
  }

  // 画编辑区页签条：整块自绘，对照 VS Code 编辑器页签（标题和关闭在同一底上）。
  void DrawEditorTabs() {
    // 没有打开的图就不画页签条。
    if (_open_editors.empty()) {
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
    // 页签色与 ApplyVsCodeDarkModernColors 同一套，避免栏/页/画布各用默认色。
    const ImU32 tab_bar_bg =
        ImGui::ColorConvertFloat4ToU32(kVsCodeSidebarBackground);
    const ImU32 active_bg =
        ImGui::ColorConvertFloat4ToU32(kVsCodeTabActiveBackground);
    const ImU32 inactive_bg =
        ImGui::ColorConvertFloat4ToU32(kVsCodeTabInactiveBackground);
    const ImU32 hover_bg =
        ImGui::ColorConvertFloat4ToU32(kVsCodeListHoverBackground);
    const ImU32 close_hover_bg =
        ImGui::ColorConvertFloat4ToU32(kVsCodeListHoverBackground);
    const ImU32 text_color = ImGui::ColorConvertFloat4ToU32(kVsCodeForeground);
    const ImU32 border_color =
        ImGui::ColorConvertFloat4ToU32(kVsCodePanelBorder);
    draw->AddRectFilled(bar_min,
                        ImVec2(bar_min.x + bar_width, bar_min.y + tab_height),
                        tab_bar_bg);
    int close_index = -1;
    float tab_x = bar_min.x;
    for (int editor_index = 0;
         editor_index < static_cast<int>(_open_editors.size()); ++editor_index) {
      const auto& graph = _open_editors[editor_index];
      // 空页签跳过。
      if (graph == nullptr) {
        continue;
      }
      const char* const name = graph->getName().c_str();
      const ImVec2 label_size = ImGui::CalcTextSize(name);
      const float tab_width =
          pad_x + label_size.x + label_close_gap + close_size + pad_x;
      ImGui::PushID(graph.get());
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
      const bool is_active = editor_index == _active_editor;
      ImU32 tab_bg = inactive_bg;
      // 活动页用编辑区底色，悬停用高亮，其余用页签栏色。
      if (is_active) {
        tab_bg = active_bg;
      } else if (tab_hovered) {
        tab_bg = hover_bg;
      }
      draw->AddRectFilled(tab_min, tab_max, tab_bg);
      // 非活动页右侧画一条细线，隔开相邻页签。
      if (!is_active) {
        const float separator = kEditorTabSeparatorThicknessEpx * ui_scale;
        draw->AddLine(ImVec2(tab_max.x - separator, tab_min.y),
                      ImVec2(tab_max.x - separator, tab_max.y), border_color);
      }
      const ImVec2 text_pos(
          tab_min.x + pad_x,
          tab_min.y + (tab_height - label_size.y) * kHalfSpan);
      draw->AddText(text_pos, text_color, name);
      // 关闭钮只在悬停时铺底，和 VS Code 一样嵌在页签里。
      if (close_hovered) {
        draw->AddRectFilled(close_min, close_max, close_hover_bg);
      }
      const ImVec2 close_center(
          (close_min.x + close_max.x) * kHalfSpan,
          (close_min.y + close_max.y) * kHalfSpan);
      draw->AddLine(
          ImVec2(close_center.x - close_glyph_half,
                 close_center.y - close_glyph_half),
          ImVec2(close_center.x + close_glyph_half,
                 close_center.y + close_glyph_half),
          text_color);
      draw->AddLine(
          ImVec2(close_center.x + close_glyph_half,
                 close_center.y - close_glyph_half),
          ImVec2(close_center.x - close_glyph_half,
                 close_center.y + close_glyph_half),
          text_color);
      // 点在关闭区就叉掉，否则激活这一页。
      if (tab_clicked) {
        if (close_hovered) {
          close_index = editor_index;
        } else {
          _active_editor = editor_index;
          SyncSelectedItemToGraph(graph);
        }
      }
      ImGui::PopID();
      tab_x += tab_width;
    }
    ImGui::SetCursorScreenPos(
        ImVec2(bar_min.x, bar_min.y + tab_height));
    ImGui::Dummy(ImVec2(bar_width, 0.0f));
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
         editor_index < static_cast<int>(_open_editors.size()); ++editor_index) {
      // 编辑区里打开了这张图就先关掉页签。
      if (_open_editors[editor_index] == graph) {
        CloseEditorAt(editor_index);
        break;
      }
    }
    for (const auto& node : graph->getNodes()) {
      _placed_nodes.erase(node.get());
      // 选中落在这张图的节点上时一起清掉。
      if (_selected_node == node.get()) {
        _selected_node = nullptr;
      }
      if (_select_node_in_editor == node.get()) {
        _select_node_in_editor = nullptr;
      }
      if (selected_node != nullptr && selected_node->get() == node.get()) {
        selected_node->reset();
      }
    }
    _controller.removeGraph(graph);
    const auto& graphs_after = _controller.getGraphs();
    // 删空后下标归零。
    if (graphs_after.empty()) {
      _selected_item = 0;
      _create_on_graph = 0;
      return;
    }
    // 当前下标落在被删位置之后时往前挪一格。
    if (_selected_item > removed_index) {
      --_selected_item;
    }
    // 当前下标超出新长度时夹到最后一张。
    if (_selected_item >= static_cast<int>(graphs_after.size())) {
      _selected_item = static_cast<int>(graphs_after.size()) - 1;
    }
    // 创建目标图被删或下标偏后时跟着修正。
    if (_create_on_graph == removed_index) {
      _create_on_graph = _selected_item;
    } else if (_create_on_graph > removed_index) {
      --_create_on_graph;
    }
  }

  struct NodeKind {
    std::string label;
    std::function<std::shared_ptr<tg::Node>()> create;
  };

  tg::Controller& _controller;
  std::vector<NodeKind> _node_kinds;
  std::unordered_set<tg::Node*> _placed_nodes;
  ax::NodeEditor::EditorContext* _editor = nullptr;
  tg::Node* _selected_node = nullptr;
  tg::Node* _select_node_in_editor = nullptr;
  int _selected_item = 0;
  int _create_on_graph = 0;
  bool _open_pick_node_kind = false;
  std::shared_ptr<tg::Graph> _graph_pending_remove;
  std::shared_ptr<tg::Graph> _rename_graph;
  std::string _rename_buffer;
  bool _rename_focus = false;
  std::vector<std::shared_ptr<tg::Graph>> _open_editors;
  int _active_editor = -1;
  std::vector<std::string> _log_lines;
  std::size_t _log_cursor = 0;
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
};
