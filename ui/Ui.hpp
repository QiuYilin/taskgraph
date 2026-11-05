#include "UiBase.hpp"
#include "imgui_internal.h"

#include <imgui_stdlib.h>
#include <string>

#include <taskgraph.hpp>

// 在当前表格里画一个节点，展开后编辑它的属性。
static void ShowPlaceholderObject(const char* prefix,
                                  std::shared_ptr<tg::Node> node) {
  auto uid = std::hash<std::shared_ptr<tg::Node>>{}(node);
  ImGui::PushID(static_cast<int>(uid));

  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::AlignTextToFramePadding();
  bool node_open = ImGui::TreeNode(node->getName().c_str(), "%s_%zu", prefix,
                                   static_cast<unsigned int>(uid));
  ImGui::TableSetColumnIndex(1);

  if (node_open) {
    for (int i = 0; i < node->getProperties().size(); i++) {
      auto properties = node->getProperties();
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
      if (node->getProperties()[i]->value.index() == 0) {
        ImGui::InputInt("input int", &std::get<int>(properties[i]->value));
      }
      if (node->getProperties()[i]->value.index() == 1) {
        std::get<std::string>(properties[i]->value).resize(1024);
        ImGui::InputText("input text",
                         &std::get<std::string>(properties[i]->value));
      }
      if (node->getProperties()[i]->value.index() == 2) {
        ImGui::Checkbox("enable", &std::get<bool>(properties[i]->value));
      }
      ImGui::NextColumn();
      ImGui::PopID();
    }

    ImGui::TreePop();
  }
  ImGui::PopID();
}

// 在当前表格里画一张图，展开后画出其中的节点。
static void ShowPlaceholderObject(const char*,
                                  std::shared_ptr<tg::Graph> graph) {
  auto uid = std::hash<std::shared_ptr<tg::Graph>>{}(graph);
  ImGui::PushID(static_cast<int>(uid));

  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::AlignTextToFramePadding();
  bool node_open = ImGui::TreeNode(graph->getName().c_str());
  ImGui::TableSetColumnIndex(1);

  if (node_open) {
    for (const auto& node : graph->getNodes()) {
      ShowPlaceholderObject(node->getName().c_str(), node);
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

      for (const auto& graph : _controller.getGraphs()) {
        ShowPlaceholderObject("Graphs", graph);
      }

      ImGui::EndTable();
    }
    ImGui::PopStyleVar();
    ImGui::End();

    // Begin / End：选择一张图并运行。
    ImGui::Begin("Toolbar");
    ImGui::SameLine();
    static int selected_item = 0;
    if (ImGui::Button("Run")) {
      _controller.run(*_controller.getGraphs()[selected_item]);
    }

    ImGui::SameLine();
    std::vector<std::string> graph_names;
    for (const auto& graph : _controller.getGraphs()) {
      graph_names.push_back(graph->getName());
    }

    std::vector<const char*> items(graph_names.size());
    for (int i = 0; i < graph_names.size(); i++) {
      items[i] = graph_names[i].c_str();
    }
    ImGui::Combo("##combo", &selected_item, items.data(),
                 static_cast<int>(items.size()));
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
};
