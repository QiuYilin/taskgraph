#include "UiBase.hpp"
#include "imgui_internal.h"

#include <algorithm>
#include <imgui_stdlib.h>
#include <string>

#include <taskgraph.hpp>

// 按属性值存的类型画对应控件，取到哪个类型的指针就画哪个。
static void ShowPropertyValueEditor(tg::PropertyValue& value) {
  // 属性值是 int，画整数输入框。
  if (auto* v = std::get_if<int>(&value)) {
    ImGui::InputInt("input int", v);
  }
  // 属性值是 string，画文本输入框。
  else if (auto* v = std::get_if<std::string>(&value)) {
    ImGui::InputText("input text", v);
  }
  // 属性值是 bool，画勾选框。
  else if (auto* v = std::get_if<bool>(&value)) {
    ImGui::Checkbox("enable", v);
  }
}

// 在当前表格里画一个节点，展开后编辑它的属性。
static void ShowNodeRow(const char* prefix, std::shared_ptr<tg::Node> node) {
  auto uid = std::hash<std::shared_ptr<tg::Node>>{}(node);
  ImGui::PushID(static_cast<int>(uid));

  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::AlignTextToFramePadding();
  bool node_open = ImGui::TreeNode(node->getName().c_str(), "%s_%zu", prefix,
                                   static_cast<unsigned int>(uid));
  ImGui::TableSetColumnIndex(1);

  // 节点展开才画属性行，收起时右列空着。
  if (node_open) {
    // 属性表只取一次引用，循环里不再每次拷贝整个 vector。
    const auto& properties = node->getProperties();
    // 每个属性单独 PushID，否则同名的控件会共用一个 ImGui ID。
    for (int i = 0; i < static_cast<int>(properties.size()); i++) {
      ImGui::PushID(i);
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf |
                                 ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                 ImGuiTreeNodeFlags_Bullet;
      ImGui::TreeNodeEx(properties[i]->name.c_str(), flags);

      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(-FLT_MIN);
      ShowPropertyValueEditor(properties[i]->value);
      ImGui::NextColumn();
      ImGui::PopID();
    }

    ImGui::TreePop();
  }
  ImGui::PopID();
}

// 在当前表格里画一张图，展开后画出其中的节点。
static void ShowGraphRow(std::shared_ptr<tg::Graph> graph) {
  auto uid = std::hash<std::shared_ptr<tg::Graph>>{}(graph);
  ImGui::PushID(static_cast<int>(uid));

  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::AlignTextToFramePadding();
  bool graph_open = ImGui::TreeNode(graph->getName().c_str());
  ImGui::TableSetColumnIndex(1);

  // 图展开才画它的节点行，收起时右列空着。
  if (graph_open) {
    // 图里的每个节点各占一行，画法交给 ShowNodeRow。
    for (const auto& node : graph->getNodes()) {
      ShowNodeRow(node->getName().c_str(), node);
    }
    ImGui::TreePop();
  }
  ImGui::PopID();
}

class Ui : public UiBase<Ui> {
 public:
  explicit Ui(tg::Controller& controller) : _controller(controller) {}

  void StartUp() {}

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
    // Begin / End：铺满工作区的停靠宿主，Toolbar 和 Project 停在里面。
    ImGui::Begin("MainDockHost", nullptr, host_flags);
    ImGui::PopStyleVar(3);

    const ImGuiID dockspace_id = ImGui::GetID("MainDock");
    // 布局节点只在第一次没有时建，重建会丢掉用户拖好的停靠位置。
    if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
      ImGui::DockBuilderRemoveNode(dockspace_id);
      ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
      ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);
      ImGuiID dock_toolbar = 0;
      ImGuiID dock_main = 0;
      ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Up, 0.08f,
                                  &dock_toolbar, &dock_main);
      ImGui::DockBuilderDockWindow("Toolbar", dock_toolbar);
      ImGui::DockBuilderDockWindow("Project", dock_main);
      ImGui::DockBuilderFinish(dockspace_id);
    }
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    ImGui::End();

    // Begin / End：工程树。窗口折叠时 Begin 返回假，仍然要 End。
    if (!ImGui::Begin("Project")) {
      ImGui::End();
      return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
    // BeginTable / EndTable：左列是树，右列是内容。
    if (ImGui::BeginTable("##split", 2,
                          ImGuiTableFlags_BordersOuter |
                              ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_ScrollY)) {
      ImGui::TableSetupScrollFreeze(0, 1);
      ImGui::TableSetupColumn("Graphs");
      ImGui::TableSetupColumn("Contents");
      ImGui::TableHeadersRow();

      // 每张图占一行，图内节点交给 ShowGraphRow 展开后递归画。
      for (const auto& graph : _controller.getGraphs()) {
        ShowGraphRow(graph);
      }

      ImGui::EndTable();
    }
    ImGui::PopStyleVar();
    ImGui::End();

    // Begin / End：选择一张图并运行。
    ImGui::Begin("Toolbar");
    ImGui::SameLine();
    // 每帧只取一次图列表，判空、画按钮和下拉框都用这同一份。
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

    ImGui::SameLine();
    // 有图才画下拉框，空列表没有可选项。
    if (!graphs.empty()) {
      std::vector<std::string> graph_names;
      // 先收集所有图名，再转成 Combo 需要的 const char* 数组。
      for (const auto& graph : graphs) {
        graph_names.push_back(graph->getName());
      }

      std::vector<const char*> items(graph_names.size());
      // 逐个取 c_str 填进指针数组，Combo 只认这种列表。
      for (int i = 0; i < static_cast<int>(graph_names.size()); i++) {
        items[i] = graph_names[i].c_str();
      }
      ImGui::Combo("##combo", &_selected_item, items.data(),
                   static_cast<int>(items.size()));
    }
    ImGui::End();
  }

  // 鼠标按键。ImGui 的 GLFW 后端会先收到事件，这里目前不处理。
  static void MouseButtonCallback(GLFWwindow*, int, int, int) {}

  // 光标移动。ImGui 的 GLFW 后端会先收到事件，这里目前不处理。
  static void CursorPosCallback(GLFWwindow*, double, double) {}

  // 键盘。ImGui 的 GLFW 后端会先收到事件，这里目前不处理。
  static void KeyCallback(GLFWwindow*, int, int, int, int) {}

 private:
  tg::Controller& _controller;
  int _selected_item = 0;
};
