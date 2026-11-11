#include "UiBase.hpp"
#include "imgui_internal.h"
#include <imgui-node-editor/imgui_node_editor.h>

#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include <taskgraph.hpp>

constexpr const char* kNodeKindPayload = "TG_NODE_KIND";

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
    ax::NodeEditor::Config config;
    config.SettingsFile = nullptr;
    _editor = ax::NodeEditor::CreateEditor(&config);
  }

  void Update() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const ImGuiWindowFlags host_flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBackground;
    // Begin / End：铺满工作区的停靠宿主，Toolbar、Nodes、Canvas 和 Properties 停在里面。
    ImGui::Begin("MainDockHost", nullptr, host_flags);
    ImGui::PopStyleVar(3);

    const ImGuiID dockspace_id = ImGui::GetID("MainDockCommandBar");
    const ImGuiWindowSettings* property_settings =
        ImGui::FindWindowSettingsByID(ImHashStr("Properties"));
    const bool property_docked =
        property_settings != nullptr && property_settings->DockId != 0;
    // 停靠还不存在，或属性表没有停进任何格子时重建，否则旧布局会让它浮在窗口外面。
    if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr || !property_docked) {
      ImGui::DockBuilderRemoveNode(dockspace_id);
      ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
      ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);
      constexpr int kGridColumnCount = 12;
      constexpr int kProjectPaneColumnCount = 2;
      constexpr int kPropertyPaneColumnCount = 2;
      constexpr int kCenterColumnCount =
          kGridColumnCount - kProjectPaneColumnCount;
      constexpr float kProjectPaneWidthRatio =
          static_cast<float>(kProjectPaneColumnCount) /
          static_cast<float>(kGridColumnCount);
      constexpr float kPropertyPaneWidthRatio =
          static_cast<float>(kPropertyPaneColumnCount) /
          static_cast<float>(kCenterColumnCount);
      ImGuiID dock_toolbar = 0;
      ImGuiID dock_main = 0;
      ImGuiID dock_project = 0;
      ImGuiID dock_center = 0;
      ImGuiID dock_property = 0;
      ImGuiID dock_canvas = 0;
      const float command_bar_height =
          kCommandBarHeightEpx * ImGui::GetStyle().FontScaleMain;
      const float work_height = viewport->WorkSize.y;
      // 命令栏按一行控件的高度切，窗口比这一行还矮时整窗都给它。
      const float toolbar_height_ratio = work_height > command_bar_height
                                             ? command_bar_height / work_height
                                             : 1.0f;
      ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Up, toolbar_height_ratio,
                                  &dock_toolbar, &dock_main);
      ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, kProjectPaneWidthRatio,
                                  &dock_project, &dock_center);
      ImGui::DockBuilderSplitNode(dock_center, ImGuiDir_Right,
                                  kPropertyPaneWidthRatio, &dock_property,
                                  &dock_canvas);
      ImGui::DockBuilderDockWindow("Toolbar", dock_toolbar);
      ImGui::DockBuilderDockWindow("Project", dock_project);
      ImGui::DockBuilderDockWindow("Properties", dock_property);
      ImGui::DockBuilderDockWindow("Canvas", dock_canvas);
      ImGui::DockBuilderFinish(dockspace_id);
    }
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    ImGui::End();

    // Begin / End：节点种类列表。折叠时不画种类，工具条和画布继续画。
    if (ImGui::Begin("Nodes###Project")) {
      for (int kind_index = 0;
           kind_index < static_cast<int>(_node_kinds.size()); ++kind_index) {
        ImGui::PushID(kind_index);
        ImGui::Selectable(_node_kinds[kind_index].label.c_str());
        // 这一项正在被拖走时，载荷里只放种类下标，松手才创建节点。
        if (ImGui::BeginDragDropSource()) {
          ImGui::SetDragDropPayload(kNodeKindPayload, &kind_index,
                                    sizeof(kind_index));
          ImGui::TextUnformatted(_node_kinds[kind_index].label.c_str());
          ImGui::EndDragDropSource();
        }
        ImGui::PopID();
      }
    }
    ImGui::End();

    // Begin / End：选择一张图并运行。命令栏上下留白，让 32 epx 的按钮落在 48 epx 的一行里。
    const float ui_scale = ImGui::GetStyle().FontScaleMain;
    const float command_bar_padding_y =
        (kCommandBarHeightEpx - kStandardControlHeightEpx) /
        kFramePaddingSideCount * ui_scale;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(kSurfaceTextMarginEpx * ui_scale,
                               command_bar_padding_y));
    ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoTitleBar);
    // 每帧只取一次图列表，判空和画按钮都用这同一份。
    const auto& graphs = _controller.getGraphs();
    // 图可能被删空或变短，先把选中下标夹回有效范围，后面取图才不会越界。
    if (!graphs.empty()) {
      _selected_item = std::clamp(_selected_item, 0,
                                  static_cast<int>(graphs.size()) - 1);
    }
    // 有图才画 Run，列表为空时按下也无图可跑。
    if (!graphs.empty() && ImGui::Button("Run")) {
      _controller.run(*graphs[_selected_item]);
    }
    ImGui::End();
    ImGui::PopStyleVar();

    // Begin / End：当前图的流视图。折叠时没有网格可画，编辑器也不打开。
    if (ImGui::Begin("Canvas")) {
      ShowFlowCanvas();
    }
    ImGui::End();

    // Begin / End：右侧属性表。点中画布上的节点后编辑它的属性。
    if (ImGui::Begin("Properties")) {
      const auto& graphs = _controller.getGraphs();
      std::shared_ptr<tg::Node> selected_node;
      // 选中地址还在当前图里才编辑，节点删掉后不再写悬空指针。
      if (!graphs.empty() && _selected_node != nullptr) {
        for (const auto& node : graphs[_selected_item]->getNodes()) {
          // 地址相同就是画布上点中的那个节点。
          if (node.get() == _selected_node) {
            selected_node = node;
            break;
          }
        }
      }
      constexpr int kPropertyTableColumnCount = 2;
      constexpr float kFillColumnWidth = -FLT_MIN;
      // 有选中节点才画表，点在空白处时没有属性可改。
      if (selected_node != nullptr) {
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
      }
    }
    ImGui::End();
  }

  void ShowFlowCanvas() {
    constexpr float kInitialNodeStepX = 220.0f;
    const ImVec2 canvas_origin = ImGui::GetCursorScreenPos();
    const ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    const auto& graphs = _controller.getGraphs();

    ax::NodeEditor::SetCurrentEditor(_editor);
    // Begin / End：节点编辑器。图里已有的节点画在网格上。
    ax::NodeEditor::Begin("CanvasEditor");
    // 没有选中的图时不画节点，拖放也无处可放。
    if (!graphs.empty()) {
      int column = 0;
      for (const auto& node : graphs[_selected_item]->getNodes()) {
        const ax::NodeEditor::NodeId node_id(node.get());
        const bool place_now = _placed_nodes.insert(node.get()).second;
        // 节点第一次出现才落位，之后改由编辑器记住拖动结果。
        if (place_now) {
          ImVec2 position{static_cast<float>(column) * kInitialNodeStepX, 0.0f};
          const auto pending = _pending_node_positions.find(node.get());
          // 从列表拖进来的节点用松手位置，原有节点按列排开。
          if (pending != _pending_node_positions.end()) {
            position = pending->second;
            _pending_node_positions.erase(pending);
          }
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
    }
    ax::NodeEditor::Suspend();
    // 只有拖着种类经过时才铺接收层，平时不挡住画布平移。
    if (ImGui::IsDragDropActive()) {
      ImGui::SetCursorScreenPos(canvas_origin);
      ImGui::InvisibleButton("canvas_drop", canvas_size);
      // Begin / End：画布接收从种类列表拖来的节点。
      if (ImGui::BeginDragDropTarget()) {
        const ImGuiPayload* payload =
            ImGui::AcceptDragDropPayload(kNodeKindPayload);
        // 松手时要有载荷，并且工具条里选中了一张图。
        if (payload != nullptr && !graphs.empty()) {
          const int kind_index = *static_cast<const int*>(payload->Data);
          // 下标必须落在种类表里，避免载荷被写坏后越界。
          if (kind_index >= 0 &&
              kind_index < static_cast<int>(_node_kinds.size())) {
            auto node = _node_kinds[kind_index].create();
            // 工厂没交出节点时，不往图里放空指针。
            if (node != nullptr) {
              graphs[_selected_item]->addNode(node);
              _pending_node_positions.emplace(
                  node.get(),
                  ax::NodeEditor::ScreenToCanvas(ImGui::GetMousePos()));
            }
          }
        }
        ImGui::EndDragDropTarget();
      }
    }
    ax::NodeEditor::Resume();
    ax::NodeEditor::End();
    constexpr int kSelectedNodeReadCount = 1;
    ax::NodeEditor::NodeId selected_id;
    const int selected_count =
        ax::NodeEditor::GetSelectedNodes(&selected_id, kSelectedNodeReadCount);
    _selected_node = nullptr;
    // 只取一个选中节点来显示属性，没选中就清空。
    if (selected_count > 0) {
      _selected_node = selected_id.AsPointer<tg::Node>();
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
  struct NodeKind {
    std::string label;
    std::function<std::shared_ptr<tg::Node>()> create;
  };

  tg::Controller& _controller;
  std::vector<NodeKind> _node_kinds;
  std::unordered_map<tg::Node*, ImVec2> _pending_node_positions;
  std::unordered_set<tg::Node*> _placed_nodes;
  ax::NodeEditor::EditorContext* _editor = nullptr;
  tg::Node* _selected_node = nullptr;
  int _selected_item = 0;
};
