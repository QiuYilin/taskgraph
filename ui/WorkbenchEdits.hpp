#pragma once

#include "WorkbenchFocus.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "taskgraph/ProjectFormat.hpp"

// 第一张新建图不带序号；之后用 "Graph 2"、"Graph 3"。
inline constexpr std::size_t kFirstGraphNameSuffix = 1;
inline constexpr const char* kDefaultGraphName = "Graph";

// label 是否包含 filter（大小写不敏感）；空 filter 视为全匹配。
inline bool LabelMatchesFilter(const std::string& label,
                               const std::string& filter) {
  if (filter.empty()) {
    return true;
  }
  auto to_lower = [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  };
  std::string haystack = label;
  std::string needle = filter;
  std::transform(haystack.begin(), haystack.end(), haystack.begin(), to_lower);
  std::transform(needle.begin(), needle.end(), needle.begin(), to_lower);
  return haystack.find(needle) != std::string::npos;
}

// 搜索命中的一行。node 为空时命中的是图名。
struct SearchHit {
  std::shared_ptr<tg::Graph> graph;
  std::shared_ptr<tg::Node> node;
};

// 空输入不列结果。只匹配图名和节点 identifier，不匹配 kind。
inline std::vector<SearchHit> CollectSearchHits(
    const std::vector<std::shared_ptr<tg::Graph>>& graphs,
    const std::string& filter) {
  std::vector<SearchHit> hits;
  if (filter.empty()) {
    return hits;
  }
  for (const std::shared_ptr<tg::Graph>& graph : graphs) {
    if (graph == nullptr) {
      continue;
    }
    if (LabelMatchesFilter(graph->getName(), filter)) {
      hits.push_back(SearchHit{graph, nullptr});
    }
    for (const std::shared_ptr<tg::Node>& node : graph->getNodes()) {
      if (node != nullptr && LabelMatchesFilter(node->getName(), filter)) {
        hits.push_back(SearchHit{graph, node});
      }
    }
  }
  return hits;
}

// 给新建的图起一个还没占用的名字。第一张叫 Graph，重名再加序号。
inline std::string NextGraphName(
    const std::vector<std::shared_ptr<tg::Graph>>& graphs) {
  for (std::size_t suffix = kFirstGraphNameSuffix;; ++suffix) {
    const std::string candidate =
        suffix == kFirstGraphNameSuffix
            ? kDefaultGraphName
            : std::string(kDefaultGraphName) + " " + std::to_string(suffix);
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

// 从哪一端拉起都认成输出→输入。两端同向或空指针则没有方向。
struct OrientedLink {
  const tg::Node::Port* output = nullptr;
  const tg::Node::Port* input = nullptr;
};

inline OrientedLink OrientNewLink(const tg::Node::Port* start,
                                  const tg::Node::Port* end) {
  OrientedLink link;
  if (start != nullptr && end != nullptr && start->is_output &&
      !end->is_output) {
    link.output = start;
    link.input = end;
  } else if (start != nullptr && end != nullptr && !start->is_output &&
             end->is_output) {
    link.output = end;
    link.input = start;
  }
  return link;
}

// 种类名预览的文字左上角。包围盒右下角对齐鼠标尖。
struct CaptionPreviewAnchor {
  float text_x = 0.0f;
  float text_y = 0.0f;
};

inline CaptionPreviewAnchor CaptionPreviewTextOrigin(float mouse_x,
                                                     float mouse_y,
                                                     float text_width,
                                                     float text_height) {
  return {mouse_x - text_width, mouse_y - text_height};
}

// 点击判定的四种结果：继续等松开、取消、在给定坐标创建、保持武装且不创建。
enum class ArmedPlacementAction {
  WaitRelease,
  Cancel,
  Place,
  Stay,
};

// 这一帧的键鼠，由界面读入；判定本身不读 ImGui。
struct ArmedPlacementInput {
  bool escape_pressed = false;
  bool text_input_owns_key = false;
  bool pointer_over_editor = false;
  bool right_clicked = false;
  bool left_button_down = false;
  bool left_clicked = false;
  bool armed_graph_is_current = false;
  bool waits_for_release = false;
};

struct ArmedPlacementDecision {
  ArmedPlacementAction action = ArmedPlacementAction::Stay;
  bool waits_for_release = false;
};

// 选中种类只进入落点：打开这张图的页签，不创建节点。
inline bool ArmPickedNodeKind(
    const std::vector<std::shared_ptr<tg::Graph>>& graphs,
    WorkbenchFocus& focus, const std::string& type_id,
    const std::string& caption) {
  const int graph_index = focus.pick_request().graph_index;
  if (graph_index < 0 || graph_index >= static_cast<int>(graphs.size()) ||
      graphs[static_cast<std::size_t>(graph_index)] == nullptr) {
    return false;
  }
  focus.ArmNodePlacement(graph_index, type_id, caption);
  focus.OpenGraphInEditor(graphs[static_cast<std::size_t>(graph_index)],
                          graphs);
  return true;
}

// 编辑区确认点击：在武装的那张图上创建节点，然后解除武装。
inline std::shared_ptr<tg::Node> PlaceArmedNode(
    tg::Controller& controller,
    const std::vector<std::shared_ptr<tg::Graph>>& graphs,
    WorkbenchFocus& focus, tg::NodePose pose,
    std::unordered_map<tg::Node*, tg::NodePose>& poses) {
  const ArmedNodePlacement placement = focus.armed_placement();
  if (!placement.armed) {
    return nullptr;
  }
  const int graph_index = placement.graph_index;
  if (graph_index < 0 || graph_index >= static_cast<int>(graphs.size()) ||
      graphs[static_cast<std::size_t>(graph_index)] == nullptr) {
    focus.ClearArmedPlacement();
    return nullptr;
  }
  auto node = controller.createNode(placement.type_id);
  if (node == nullptr) {
    focus.ClearArmedPlacement();
    return nullptr;
  }
  poses[node.get()] = pose;
  graphs[static_cast<std::size_t>(graph_index)]->addNode(node);
  controller.markDirty();
  focus.OpenGraphInEditor(graphs[static_cast<std::size_t>(graph_index)],
                          graphs);
  focus.SelectNode(node.get());
  focus.ClearArmedPlacement();
  return node;
}

// 落点点击判定。松开的那一帧只结束等待，不创建。
inline ArmedPlacementDecision DecideArmedPlacement(
    const ArmedPlacementInput& input) {
  if (input.escape_pressed && !input.text_input_owns_key) {
    return {ArmedPlacementAction::Cancel, false};
  }
  if (input.pointer_over_editor && input.right_clicked) {
    return {ArmedPlacementAction::Cancel, false};
  }
  if (input.waits_for_release) {
    if (input.left_button_down) {
      return {ArmedPlacementAction::WaitRelease, true};
    }
    return {ArmedPlacementAction::Stay, false};
  }
  if (input.pointer_over_editor && input.left_clicked &&
      input.armed_graph_is_current) {
    return {ArmedPlacementAction::Place, false};
  }
  return {ArmedPlacementAction::Stay, false};
}

struct ArmedPlacementStep {
  std::shared_ptr<tg::Node> node;
  ArmedPlacementDecision decision;
  bool suppress_editor_context_menu = false;
};

// 执行一次落点判定。真正创建只发生在 Place。
inline ArmedPlacementStep ApplyArmedPlacement(
    tg::Controller& controller,
    const std::vector<std::shared_ptr<tg::Graph>>& graphs,
    WorkbenchFocus& focus,
    std::unordered_map<tg::Node*, tg::NodePose>& poses,
    const ArmedPlacementInput& input, tg::NodePose pose) {
  ArmedPlacementStep step;
  if (!focus.armed_placement().armed) {
    return step;
  }
  step.decision = DecideArmedPlacement(input);
  if (step.decision.action == ArmedPlacementAction::Cancel) {
    focus.ClearArmedPlacement();
    step.suppress_editor_context_menu =
        input.pointer_over_editor && input.right_clicked;
    return step;
  }
  if (step.decision.action == ArmedPlacementAction::Place) {
    step.node = PlaceArmedNode(controller, graphs, focus, pose, poses);
  }
  return step;
}

// 主侧栏 + Graph：新建空图并选中，不打开编辑区页签。
inline std::shared_ptr<tg::Graph> AddEmptyGraph(tg::Controller& controller,
                                                WorkbenchFocus& focus) {
  auto graph =
      std::make_shared<tg::Graph>(NextGraphName(controller.getGraphs()));
  controller.addGraph(graph);
  focus.SelectGraph(static_cast<int>(controller.getGraphs().size()) - 1);
  return graph;
}
