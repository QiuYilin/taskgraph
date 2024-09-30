#include "UiBase.hpp"
// #include "implot_internal.h"
#include <taskgraph.hpp>
#include <iostream>
#include <string>
#include <imgui_stdlib.h>

//#include <graphviz/gvc.h>

// Helper to display a little (?) mark which shows a tooltip when hovered.
// In your own code you may want to display an actual icon if you are using a
// merged icon fonts (see docs/FONTS.md)
static void HelpMarker(const char* desc) {
  ImGui::TextDisabled("(?)");
  if (ImGui::BeginItemTooltip()) {
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
    ImGui::TextUnformatted(desc);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}

static void ShowPlaceholderObject(const char* prefix,
                                  std::shared_ptr<tg::Node> node) {
  // Use object uid as identifier. Most commonly you could also use the object
  // pointer as a base ID.
  auto uid = std::hash<std::shared_ptr<tg::Node>>{}(node);
  ImGui::PushID(static_cast<int>(uid));

  // Text and Tree nodes are less high than framed widgets, using
  // AlignTextToFramePadding() we add vertical spacing to make the tree lines
  // equal high.
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::AlignTextToFramePadding();
  bool node_open =
      ImGui::TreeNode(node->getName().c_str(), "%s_%zu", prefix,static_cast<unsigned int>(uid));
  ImGui::TableSetColumnIndex(1);
  // ImGui::Text("my sailor is rich");

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
      // ImGui::DragFloat("##value", &placeholder_members, 0.01f);
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

static void ShowPlaceholderObject(const char* prefix,
                                  std::shared_ptr<tg::Graph> graph) {
  // Use object uid as identifier. Most commonly you could also use the object
  // pointer as a base ID.
  auto uid = std::hash<std::shared_ptr<tg::Graph>>{}(graph);
  ImGui::PushID(static_cast<int>(uid));

  // Text and Tree nodes are less high than framed widgets, using
  // AlignTextToFramePadding() we add vertical spacing to make the tree lines
  // equal high.
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::AlignTextToFramePadding();
  bool node_open = ImGui::TreeNode(graph->getName().c_str());
  ImGui::TableSetColumnIndex(1);
  // ImGui::Text("my sailor is rich");

  if (node_open) {
    // for (int i = 0; i < 8; i++) {
    //   ImGui::PushID(i);  // Use field index as identifier.
    for (const auto& node : graph->getNodes()) {
      ShowPlaceholderObject(node->getName().c_str(), node);
    }
    //   ImGui::PopID();
    // }
    ImGui::TreePop();
  }
  ImGui::PopID();
}

class Ui : public UiBase<Ui> {
  static std::vector<std::vector<tg::SfrVariant>> properties_;

 public:
  explicit Ui(tg::Controller& controller) : _controller(controller){};

  // Anything that needs to be called once OUTSIDE of the main application loop
  void StartUp() {}

  // Anything that needs to be called cyclically INSIDE of the main application
  // loop
  void Update() {
    //ImGui::ShowDemoWindow();
    // 画出controller中的graph列表，单击某个graph，显示graph中的node列表，单击某个node，显示node的属性列表
    ImGui::SetNextWindowSize(ImVec2(430, 450), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Project")) {
      ImGui::End();
      return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
    if (ImGui::BeginTable("##split", 2,
                          ImGuiTableFlags_BordersOuter |
                              ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_ScrollY)) {
      ImGui::TableSetupScrollFreeze(0, 1);
      ImGui::TableSetupColumn("Graphs");
      ImGui::TableSetupColumn("Contents");
      ImGui::TableHeadersRow();

      // Iterate placeholder objects (all the same data)

      for (const auto& graph : _controller.getGraphs()) {
        ShowPlaceholderObject("Graphs", graph);
      }

      ImGui::EndTable();
    }
    ImGui::PopStyleVar();
    ImGui::End();

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
    if(ImGui::Combo("##combo", &selected_item, items.data(), static_cast<int>(items.size()))){
        //画出当前的graviz图
    }
    ImGui::End();

  }

  // The callbacks are updated and called BEFORE the Update loop is entered
  // It can be assumed that inside the Update loop all callbacks have already
  // been processed
  static void MouseButtonCallback(GLFWwindow* window, int button, int action,
                                  int mods) {
    // For Dear ImGui to work it is necessary to queue if the mouse signal is
    // already processed by Dear ImGui Only if the mouse is not already captured
    // it should be used here.
    ImGuiIO& _io = ImGui::GetIO();
    if (!_io.WantCaptureMouse) {
    }
  }

  //TODO(qiuyilin) Use macros to avoid duplication of function parameter types 
  static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    // For Dear ImGui to work it is necessary to queue if the mouse signal is
    // already processed by Dear ImGui Only if the mouse is not already captured
    // it should be used here.
    ImGuiIO& _io = ImGui::GetIO();
    if (!_io.WantCaptureMouse) {
    }
  }

  //TODO(qiuyilin) Use macros to avoid duplication of function parameter types 
  static void KeyCallback(GLFWwindow* window, int key, int scancode,
                          int actions, int mods) {
    // For Dear ImGui to work it is necessary to queue if the keyboard signal is
    // already processed by Dear ImGui Only if the keyboard is not already
    // captured it should be used here.
    ImGuiIO& _io = ImGui::GetIO();
    if (!_io.WantCaptureKeyboard) {
    }
  }

 private:
  tg::Controller& _controller;
};