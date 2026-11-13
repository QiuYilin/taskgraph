#include "ExplorerHeaderActions.hpp"
#include "NativePathDialog.hpp"
#include "UiBase.hpp"
#include "UiSession.hpp"
#include "WorkbenchFocus.hpp"
#include "imgui_internal.h"
#include <imgui-node-editor/imgui_node_editor.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include <taskgraph.hpp>

// 红色专留给类型不符；合法连线不得用红。
inline const ImVec4 kTypeMismatchLinkColor = ImVec4(1.0f, 0.25f, 0.25f, 1.0f);
// 与示例 NodeData_ 模板 id 对齐的固定线色。
inline const ImVec4 kIntDataLinkColor = ImVec4(0.35f, 0.75f, 0.45f, 1.0f);
inline const ImVec4 kImageDataLinkColor = ImVec4(0.95f, 0.75f, 0.2f, 1.0f);
inline const ImVec4 kPointCloudDataLinkColor = ImVec4(0.35f, 0.65f, 0.95f, 1.0f);
inline const ImVec4 kUntypedLinkColor = ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
constexpr float kLinkColorSaturation = 0.65f;
constexpr float kLinkColorValue = 0.95f;
constexpr float kLinkColorMinHue = 0.08f;
constexpr float kLinkColorMaxHue = 0.92f;
constexpr unsigned int kLinkColorHashBuckets = 1000;
constexpr const char* kIntDataTypeId = "IntData";
constexpr const char* kImageDataTypeId = "ImageData";
constexpr const char* kPointCloudXyzDataTypeId = "PointCloudXyzData";
constexpr float kDefaultLinkThickness = 2.0f;
constexpr float kProjectPathInputWidthEpx = 420.0f;
constexpr const char* kUntitledProjectLabel = "Untitled";
constexpr const char* kWelcomeTabLabel = "Welcome";
constexpr const char* kNoProjectOpenedLabel = "NO PROJECT OPENED";
constexpr const char* kWelcomeStartHeading = "Start";
constexpr const char* kWelcomeRecentHeading = "Recent";
// 弹窗 ID 不能用 "Project"：与 Explorer 的 Nodes###Project 同 ID，会触发 EndPopup 断言。
constexpr const char* kProjectPathDialogId = "Project Path###ProjectPathDialog";
constexpr const char* kBrowseActionLabel = "Browse...";
constexpr const char* kPickProjectDirectoryTitle = "Select Project";
constexpr const char* kPickPropertyFileTitle = "Select File";
constexpr const char* kPickNodeKindPopupId = "PickNodeKind";
constexpr const char* kPickNodeKindFilterHint = "Type to filter";
constexpr const char* kPickNodeKindEmptyLabel = "No matching kinds";
constexpr std::size_t kPickNodeKindFilterMinCapacity = 64;

// label 是否包含 filter（大小写不敏感）；空 filter 视为全匹配。
static bool LabelMatchesFilter(const std::string& label,
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

// 同类型 id 得到同一颜色；空类型用灰色；避开红色相。
static ImVec4 ColorForTypeId(const std::string& type_id) {
  if (type_id.empty()) {
    return kUntypedLinkColor;
  }
  if (type_id == kIntDataTypeId) {
    return kIntDataLinkColor;
  }
  if (type_id == kImageDataTypeId) {
    return kImageDataLinkColor;
  }
  if (type_id == kPointCloudXyzDataTypeId) {
    return kPointCloudDataLinkColor;
  }
  unsigned int hash = 0;
  for (unsigned char ch : type_id) {
    constexpr unsigned int kHashShift = 5;
    hash = hash * kHashShift + ch;
  }
  const float hue_span = kLinkColorMaxHue - kLinkColorMinHue;
  const float hue =
      kLinkColorMinHue +
      static_cast<float>(hash % kLinkColorHashBuckets) /
          static_cast<float>(kLinkColorHashBuckets) * hue_span;
  float red = 0.0f;
  float green = 0.0f;
  float blue = 0.0f;
  ImGui::ColorConvertHSVtoRGB(hue, kLinkColorSaturation, kLinkColorValue, red,
                              green, blue);
  return ImVec4(red, green, blue, 1.0f);
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

  ~Ui() {
    PersistSession();
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
    _session = UiSession::load();
    TryRestoreOrShowWelcome();
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
      // 显隐用几何条带 + 按下保持，不用 IsWindowHovered（Tooltip 会抢走悬停）。
      const ImVec2 header_row_min = ImGui::GetCursorScreenPos();
      const ImVec2 window_pos = ImGui::GetWindowPos();
      ExplorerHeaderActionBand header_band;
      header_band.content_min_x =
          window_pos.x + ImGui::GetWindowContentRegionMin().x;
      header_band.content_max_x =
          window_pos.x + ImGui::GetWindowContentRegionMax().x;
      header_band.header_min_y = header_row_min.y;
      header_band.header_max_y =
          header_row_min.y + ImGui::GetFrameHeight();
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted("Explorer");
      const bool has_project = _controller.hasProjectRoot();
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
      const ImVec2 mouse = ImGui::GetIO().MousePos;
      const bool show_explorer_actions = ShouldShowExplorerHeaderActions(
          header_band.contains(mouse.x, mouse.y),
          ImGui::IsMouseDown(ImGuiMouseButton_Left),
          _explorer_actions_press_hold);
      if (show_explorer_actions) {
        ImGui::SameLine();
        ImGui::SetCursorPosX(actions_x);
        PushExplorerTitleActionStyle();
        // Begin / End：没有工程或没有图时禁用新建节点。
        ImGui::BeginDisabled(!has_project || graphs.empty());
        // 点 Node 才弹出种类，节点加到当前图。文件对应节点。
        if (ImGui::Button(kNewNodeActionLabel,
                          ImVec2(node_button_width, 0.0f))) {
          RequestOpenPickNodeKind(_focus.selected_graph_index(), false);
          ImGui::OpenPopup(kPickNodeKindPopupId);
        }
        // 悬停才说明这个按钮会弹出节点种类。
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
          ImGui::SetTooltip("New Node");
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        // Begin / End：没有工程时禁用新建图。
        ImGui::BeginDisabled(!has_project);
        // 点 Graph 才新建一张空图，建完立刻命名，不自动打开编辑区页签。
        if (ImGui::Button(kNewGraphActionLabel,
                          ImVec2(graph_button_width, 0.0f))) {
          auto graph =
              std::make_shared<tg::Graph>(NextGraphName(graphs));
          _controller.addGraph(graph);
          _focus.SelectGraph(static_cast<int>(graphs.size()) - 1);
          selected_node.reset();
          BeginRenameGraph(graph);
        }
        // 悬停才说明这个按钮会新建一张空图。
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
          ImGui::SetTooltip("New Graph");
        }
        ImGui::EndDisabled();
        PopExplorerTitleActionStyle();
      }

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

      // 有工程时画工程名作为 Explorer 树根。
      if (has_project) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(_controller.projectName().c_str());
      }

      // 每张已创建的图都占一行，没有节点也留着；无工程时不画树。
      for (int graph_index = 0;
           has_project && graph_index < static_cast<int>(graphs.size());
           ++graph_index) {
        const auto& graph = graphs[graph_index];
        ImGui::PushID(graph.get());
        ImGuiTreeNodeFlags graph_flags = ImGuiTreeNodeFlags_SpanAvailWidth |
                                         ImGuiTreeNodeFlags_OpenOnArrow |
                                         ImGuiTreeNodeFlags_DefaultOpen;
        // 编辑区正在显示的图，或仅在侧栏选中的图，在树上高亮。
        if (graph_index == _focus.selected_graph_index()) {
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
            _focus.ClearNodeSelection();
          }
          // Begin / End：图右键菜单。New Node、Rename、Delete。
          if (ImGui::BeginPopupContextItem("GraphContext")) {
            OpenGraphInEditor(graph);
            selected_node.reset();
            _focus.ClearNodeSelection();
            // 选 New Node 才记下目标图，稍后打开种类列表。
            if (ImGui::MenuItem("New Node")) {
              RequestOpenPickNodeKind(graph_index, false);
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
            const bool node_is_selected = node.get() == _focus.selected_node();
            ImGui::Selectable(node->getName().c_str(), node_is_selected);
            // 点中侧栏里的节点就打开该图编辑区，并让属性表跟着它。
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
              OpenGraphInEditor(graph);
              selected_node = node;
              _focus.SelectNode(node.get());
            }
            // Begin / End：节点右键菜单。New Node、Delete。节点名固定，不能重命名。
            if (ImGui::BeginPopupContextItem("NodeContext")) {
              OpenGraphInEditor(graph);
              selected_node = node;
              _focus.SelectNode(node.get());
              // 选 New Node 才记下目标图，稍后打开种类列表。
              if (ImGui::MenuItem("New Node")) {
                RequestOpenPickNodeKind(graph_index, false);
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
      // 右键菜单或画布里选了 New Node，在 Explorer 窗口上打开种类列表。
      if (_focus.TakeOpenPickPopup()) {
        ImGui::OpenPopup(kPickNodeKindPopupId);
      }
      // Begin / End：节点种类选择浮层。
      ShowPickNodeKindPopup(graphs, selected_node);
    }
    ImGui::End();

    // Begin / End：编辑区。Welcome 或 graph 页签；全关则留空。
    HideDockTab("Canvas");
    if (ImGui::Begin("Canvas")) {
      DrawEditorTabs();
      if (_focus.active_editor() >= 0 &&
          _focus.active_editor() < static_cast<int>(_focus.open_editors().size())) {
        if (_focus.open_editors()[_focus.active_editor()].kind == EditorTab::Kind::Welcome) {
          ShowWelcomePage();
        } else {
          ShowFlowCanvas();
        }
      }
    }
    ImGui::End();

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

    // Begin / End：辅助侧栏属性表。
    HideDockTab("Properties");
    PushPropertiesPanelStyle();
    if (ImGui::Begin("Properties")) {
      constexpr int kPropertyTableColumnCount = 3;
      constexpr float kFillColumnWidth = -FLT_MIN;
      constexpr float kPropertyNameColumnRatio = 0.34f;
      constexpr float kPropertyTypeColumnRatio = 0.22f;
      constexpr float kPropertyValueColumnRatio =
          1.0f - kPropertyNameColumnRatio - kPropertyTypeColumnRatio;
      PushPropertiesContentStyle();
      // 有选中节点才画表，点在空白处时没有属性可改。
      if (selected_node != nullptr) {
        ImGui::PushID(selected_node.get());
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
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
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
                                  ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            // 输入槽用 Port::data_type.name；原生属性按 PropertyValue 变体名。
            if (bound_input) {
              const tg::Node::Port* port = selected_node->inputPort(
                  static_cast<unsigned int>(property->bound_input_index));
              if (port != nullptr) {
                ImGui::TextUnformatted(port->data_type.name.c_str());
              }
            } else if (std::holds_alternative<int>(property->value)) {
              ImGui::TextUnformatted("int");
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
              } else if (const auto* text =
                             std::get_if<std::string>(&property->value)) {
                ImGui::TextUnformatted(text->c_str());
              } else if (const auto* flag =
                             std::get_if<bool>(&property->value)) {
                ImGui::TextUnformatted(*flag ? "true" : "false");
              }
              ImGui::PopStyleColor();
            } else if (auto* number = std::get_if<int>(&property->value)) {
              // 整数：无 +/- 步进按钮。
              constexpr int kIntInputNoStep = 0;
              ImGui::SetNextItemWidth(kFillColumnWidth);
              ImGui::InputInt("##value", number, kIntInputNoStep);
              if (ImGui::IsItemDeactivatedAfterEdit()) {
                _controller.markDirty();
              }
            } else if (auto* text = std::get_if<std::string>(&property->value)) {
              // 字符串（图像/点云为导入路径）：可手输，也可 Browse 调原生选文件。
              constexpr std::size_t kTextTerminatorSize = 1;
              const float browse_width =
                  ImGui::CalcTextSize(kBrowseActionLabel).x +
                  ImGui::GetStyle().FramePadding.x * kFramePaddingSideCount;
              const float path_width =
                  ImGui::GetContentRegionAvail().x - browse_width -
                  ImGui::GetStyle().ItemSpacing.x;
              const std::size_t text_buffer_size =
                  text->capacity() + kTextTerminatorSize;
              ImGui::SetNextItemWidth(path_width > 0.0f ? path_width
                                                       : kFillColumnWidth);
              ImGui::InputText("##value", text->data(), text_buffer_size,
                               ImGuiInputTextFlags_CallbackResize,
                               ResizePropertyText, text);
              if (ImGui::IsItemDeactivatedAfterEdit()) {
                _controller.markDirty();
              }
              ImGui::SameLine();
              if (ImGui::Button(kBrowseActionLabel)) {
                std::string context = *text;
                if (context.empty() && _controller.hasProjectRoot()) {
                  context = _controller.projectRoot().string();
                }
                const std::string default_path =
                    NativePathDialog::resolveDefaultPath(
                        context, _session.last_browse_dir);
                const std::optional<std::string> picked =
                    NativePathDialog::pickOpenFile(kPickPropertyFileTitle,
                                                   default_path);
                if (NativePathDialog::applyPickedPath(*text, picked)) {
                  RememberBrowseDir(*picked);
                  _controller.markDirty();
                }
              }
            } else if (auto* flag = std::get_if<bool>(&property->value)) {
              if (ImGui::Checkbox("##value", flag)) {
                _controller.markDirty();
              }
            }
            // 绑定输入槽：口模式右键改为属性，属性模式右键改为端口。
            if (bound_input &&
                ImGui::BeginPopupContextItem("input_presentation_menu")) {
              if (input_as_port) {
                // 菜单用英文：当前字体无中文 glyph，中文会显示成问号。
                if (ImGui::MenuItem("Convert to Property") && !graphs.empty()) {
                  graphs[_focus.selected_graph_index()]->setInputPresentation(
                      selected_node,
                      static_cast<unsigned int>(property->bound_input_index),
                      tg::InputPresentation::Property);
                  _controller.markDirty();
                }
              } else if (ImGui::MenuItem("Convert to Port") &&
                         !graphs.empty()) {
                graphs[_focus.selected_graph_index()]->setInputPresentation(
                    selected_node,
                    static_cast<unsigned int>(property->bound_input_index),
                    tg::InputPresentation::Port);
                _controller.markDirty();
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

    // 没在改文本时处理快捷键：F2 重命名图，Delete / Backspace 删除。
    if (!ImGui::GetIO().WantTextInput) {
      // F2：没有选中节点时才改当前图名；节点名固定，不能重命名。
      if (ImGui::IsKeyPressed(ImGuiKey_F2) && !graphs.empty() &&
          _focus.selected_node() == nullptr) {
        BeginRenameGraph(graphs[_focus.selected_graph_index()]);
      }
      // Delete 或 Backspace：有选中节点就删节点，否则删当前图。
      if (ImGui::IsKeyPressed(ImGuiKey_Delete) ||
          ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
        if (_focus.selected_node() != nullptr) {
          RemoveSelectedNode(&selected_node);
        } else if (!graphs.empty()) {
          RemoveGraphFromUi(graphs[_focus.selected_graph_index()], &selected_node);
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

    // Begin / End：状态栏。左=工作区（工程名）+ Run + 当前图，右=选中节点。
    HideDockTab("Status Bar");
    PushStatusBarStyle();
    if (ImGui::Begin("Status Bar", nullptr, ImGuiWindowFlags_NoTitleBar)) {
      // 有工程时左侧先画工程名。
      if (_controller.hasProjectRoot()) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(_controller.projectName().c_str());
        ImGui::SameLine(0.0f, kBetweenControlsEpx * ui_scale);
      }
      // Begin / End：没有图时禁用 Run，列表为空没有任务可跑。
      ImGui::BeginDisabled(graphs.empty());
      // 点状态栏上的 Run 才跑当前图。
      if (DrawBarAction("Run") && !graphs.empty()) {
        _controller.run(*graphs[_focus.selected_graph_index()]);
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
    PopStatusBarStyle();
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
    // Begin / End：窗口标题栏。左 File→Run→Help，中命令中心，右窗口按钮。
    ImGui::Begin("Window Title Bar", nullptr, title_flags);
    ImGui::AlignTextToFramePadding();
    // 标题栏菜单栏从左到右：File、Run、Help。不要活动栏，不要第二条工具栏。
    if (DrawBarAction("File")) {
      ImGui::OpenPopup("##TitleFileMenu");
    }
    if (ImGui::BeginPopup("##TitleFileMenu")) {
      if (ImGui::MenuItem("New Project...")) {
        BeginProjectPathDialog(ProjectPathAction::New);
      }
      if (ImGui::MenuItem("Open Project...")) {
        BeginProjectPathDialog(ProjectPathAction::Open);
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
                          _controller.hasProjectRoot())) {
        SaveCurrentProject();
      }
      if (ImGui::MenuItem("Save As...")) {
        BeginProjectPathDialog(ProjectPathAction::SaveAs);
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
      if (ImGui::MenuItem("Run Graph", nullptr, false, !graphs.empty()) &&
          !graphs.empty()) {
        _controller.run(*graphs[_focus.selected_graph_index()]);
      }
      ImGui::EndPopup();
    }
    ImGui::SameLine(0.0f, kBetweenControlsEpx * ui_scale);
    if (DrawBarAction("Help")) {
      ImGui::OpenPopup("##TitleHelpMenu");
    }
    if (ImGui::BeginPopup("##TitleHelpMenu")) {
      if (ImGui::MenuItem("Welcome")) {
        OpenWelcomeInEditor();
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
    // 无底无描边，只露工程名文字，不画成可见的框。
    PushCommandCenterStyle();
    // 命令中心本轮只展示工程名，点击无行为（不接 Quick Pick）。
    ImGui::Button(command_center_label,
                  ImVec2(command_center_width, command_center_height));
    PopCommandCenterStyle();
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

  void ShowFlowCanvas() {
    constexpr float kInitialNodeStepX = 220.0f;
    const auto& graphs = _controller.getGraphs();

    ax::NodeEditor::SetCurrentEditor(_editor);
    ax::NodeEditor::Style& editor_style = ax::NodeEditor::GetStyle();
    ApplyFlowCanvasTheme(editor_style);
    // Begin / End：节点编辑器。图里已有的节点画在网格上。
    ax::NodeEditor::Begin("CanvasEditor");
    // 没有选中的图时不画节点。
    if (!graphs.empty()) {
      int column = 0;
      for (const auto& node : graphs[_focus.selected_graph_index()]->getNodes()) {
        const ax::NodeEditor::NodeId node_id(node.get());
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
        ImGui::TextUnformatted(node->getName().c_str());
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
          // 画功能名；没有 caption 的端口留空。
          if (!port->caption.empty()) {
            ImGui::TextUnformatted(port->caption.c_str());
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
          // 画功能名；没有 caption 的端口留空。
          if (!port->caption.empty()) {
            ImGui::TextUnformatted(port->caption.c_str());
          }
          ax::NodeEditor::EndPin();
        }
        ImGui::EndGroup();
        ax::NodeEditor::EndNode();
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
          // 类型不符用红；相符用类型色。
          const ImVec4 link_color =
              output->data_type.id == input->data_type.id
                  ? ColorForTypeId(output->data_type.id)
                  : kTypeMismatchLinkColor;
          ax::NodeEditor::Link(ax::NodeEditor::LinkId(&connection),
                               ax::NodeEditor::PinId(output),
                               ax::NodeEditor::PinId(input), link_color,
                               kDefaultLinkThickness);
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
          // 必须是一个输出对一个输入，并且不是同一个节点；属性模式入口拒连。
          if (output == nullptr || input == nullptr ||
              output->node == nullptr || input->node == nullptr ||
              output->node == input->node ||
              !input->node->isInputShownAsPort(input->index)) {
            ax::NodeEditor::RejectNewItem(kTypeMismatchLinkColor,
                                          kDefaultLinkThickness);
          } else if (output->data_type.id != input->data_type.id ||
                     output->data_type.id.empty()) {
            // 类型不符或无类型：红线预览并拒绝。
            ax::NodeEditor::RejectNewItem(kTypeMismatchLinkColor,
                                          kDefaultLinkThickness);
          } else if (ax::NodeEditor::AcceptNewItem(
                         ColorForTypeId(output->data_type.id),
                         kDefaultLinkThickness)) {
            graphs[_focus.selected_graph_index()]->connect(output, input);
            _controller.markDirty();
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
          for (const auto& candidate : graphs[_focus.selected_graph_index()]->getNodes()) {
            // 地址相同就是编辑器要删的那个节点。
            if (candidate.get() == raw) {
              owned = candidate;
              break;
            }
          }
          // 节点还在图里才接受删除并调用 removeNode。
          if (owned != nullptr && ax::NodeEditor::AcceptDeletedItem()) {
            RemoveNodeFromUi(graphs[_focus.selected_graph_index()], owned, nullptr);
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
      // 画布右键：引脚 / 节点 / 背景菜单（Suspend 后弹 ImGui）。
      // PinId 只在 ShowPinContextMenu 为真的那一帧有效，必须存进成员供后续帧画菜单。
      ax::NodeEditor::Suspend();
      ax::NodeEditor::PinId context_pin_id;
      if (ax::NodeEditor::ShowPinContextMenu(&context_pin_id)) {
        auto* const port = context_pin_id.AsPointer<tg::Node::Port>();
        if (port != nullptr && port->node != nullptr && !port->is_output) {
          _pin_context_port = port;
          ImGui::OpenPopup("Pin Context Menu");
        }
      }
      ax::NodeEditor::NodeId context_node_id;
      if (ax::NodeEditor::ShowNodeContextMenu(&context_node_id) &&
          context_node_id.AsPointer<tg::Node>() != nullptr) {
        _canvas_context_spawn_pos =
            ax::NodeEditor::ScreenToCanvas(ImGui::GetIO().MousePos);
        ImGui::OpenPopup("Canvas Node Menu");
      }
      if (ax::NodeEditor::ShowBackgroundContextMenu()) {
        _canvas_context_spawn_pos =
            ax::NodeEditor::ScreenToCanvas(ImGui::GetIO().MousePos);
        ImGui::OpenPopup("Canvas Background Menu");
      }
      if (ImGui::BeginPopup("Pin Context Menu")) {
        if (_pin_context_port != nullptr &&
            _pin_context_port->node != nullptr &&
            !_pin_context_port->is_output) {
          // 菜单用英文：当前字体无中文 glyph，中文会显示成问号。
          if (ImGui::MenuItem("Convert to Property")) {
            auto owned = _pin_context_port->node->shared_from_this();
            graphs[_focus.selected_graph_index()]->setInputPresentation(
                owned, _pin_context_port->index,
                tg::InputPresentation::Property);
            _controller.markDirty();
            _pin_context_port = nullptr;
          }
        }
        ImGui::EndPopup();
      } else {
        _pin_context_port = nullptr;
      }
      if (ImGui::BeginPopup("Canvas Node Menu")) {
        // 与 Explorer 节点菜单对齐：New Node；落点用打开菜单时的画布坐标。
        if (ImGui::MenuItem("New Node")) {
          RequestOpenPickNodeKind(_focus.selected_graph_index(), true);
        }
        ImGui::EndPopup();
      }
      if (ImGui::BeginPopup("Canvas Background Menu")) {
        // 空白处右键也能弹出节点种类选择。
        if (ImGui::MenuItem("New Node")) {
          RequestOpenPickNodeKind(_focus.selected_graph_index(), true);
        }
        ImGui::EndPopup();
      }
      ax::NodeEditor::Resume();
      // 侧栏点过的节点这一帧才画出来，先在编辑器里选中。
      if (_focus.pending_editor_select() != nullptr) {
        ax::NodeEditor::SelectNode(
            ax::NodeEditor::NodeId(_focus.pending_editor_select()));
      }
    }
    ax::NodeEditor::End();
    tg::Node* const picked_in_explorer = _focus.ConsumePendingEditorSelect();
    // 编辑器结束时可能清掉侧栏这次点击，再选一次，属性表才跟得上。
    if (picked_in_explorer != nullptr) {
      ax::NodeEditor::SelectNode(ax::NodeEditor::NodeId(picked_in_explorer));
    }
    constexpr int kSelectedNodeReadCount = 1;
    ax::NodeEditor::NodeId selected_id;
    const int selected_count =
        ax::NodeEditor::GetSelectedNodes(&selected_id, kSelectedNodeReadCount);
    // 侧栏点的节点以侧栏为准，画布没选中时也不要把它清掉。
    if (picked_in_explorer != nullptr) {
      _focus.SetSelectedNodeFromCanvas(picked_in_explorer);
    } else if (selected_count > 0 && !graphs.empty()) {
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
    } else {
      _focus.SetSelectedNodeFromCanvas(nullptr);
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
  // 记下目标图并打开种类 Quick Pick；画布入口可带落点坐标。
  void RequestOpenPickNodeKind(int graph_index, bool spawn_at_canvas) {
    if (spawn_at_canvas) {
      _focus.RequestPickNodeKind(graph_index, true, _canvas_context_spawn_pos);
    } else {
      _focus.RequestPickNodeKind(graph_index, false);
    }
  }

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
        _controller.markDirty();
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
    _node_poses.erase(raw);
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
    RemoveNodeFromUi(graphs[_focus.selected_graph_index()], owned, selected_node);
  }

  // 把工程里已有的图打开成编辑区页签；已打开则只激活。
  void OpenGraphInEditor(const std::shared_ptr<tg::Graph>& graph) {
    _focus.OpenGraphInEditor(graph, _controller.getGraphs());
  }

  // 打开或激活 Welcome 页签。
  void OpenWelcomeInEditor() {
    _focus.OpenWelcomeInEditor();
  }

  void CloseWelcomeIfOpen() {
    _focus.CloseWelcomeIfOpen(_controller.getGraphs());
    PersistSession();
  }

  // 按图指针把当前图下标对到工程列表里。
  void SyncSelectedItemToGraph(const std::shared_ptr<tg::Graph>& graph) {
    _focus.SyncSelectedItemToGraph(graph, _controller.getGraphs());
  }

  // 关掉编辑区里某一页；没有页签时编辑区留空。
  void CloseEditorAt(int editor_index) {
    if (_focus.CloseEditorAt(editor_index, _controller.getGraphs())) {
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
    draw->AddRectFilled(bar_min,
                        ImVec2(bar_min.x + bar_width, bar_min.y + tab_height),
                        tab_palette.tab_bar_bg);
    int close_index = -1;
    float tab_x = bar_min.x;
    for (int editor_index = 0;
         editor_index < static_cast<int>(_focus.open_editors().size()); ++editor_index) {
      const EditorTab& tab = _focus.open_editors()[editor_index];
      const bool is_welcome = tab.kind == EditorTab::Kind::Welcome;
      if (!is_welcome && tab.graph == nullptr) {
        continue;
      }
      const char* const name =
          is_welcome ? kWelcomeTabLabel : tab.graph->getName().c_str();
      const ImVec2 label_size = ImGui::CalcTextSize(name);
      const float tab_width =
          pad_x + label_size.x + label_close_gap + close_size + pad_x;
      ImGui::PushID(is_welcome ? nullptr : tab.graph.get());
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
      draw->AddText(text_pos, tab_palette.text, name);
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
          tab_palette.text);
      draw->AddLine(
          ImVec2(close_center.x + close_glyph_half,
                 close_center.y - close_glyph_half),
          ImVec2(close_center.x - close_glyph_half,
                 close_center.y + close_glyph_half),
          tab_palette.text);
      // 点在关闭区就叉掉，否则激活这一页。
      if (tab_clicked) {
        if (close_hovered) {
          close_index = editor_index;
        } else {
          _focus.active_editor() = editor_index;
          if (!is_welcome) {
            SyncSelectedItemToGraph(tab.graph);
          }
        }
      }
      ImGui::PopID();
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
    _pin_context_port = nullptr;
    _rename_graph.reset();
    _graph_pending_remove.reset();
    _rename_buffer.clear();
    _rename_focus = false;
  }

  void PersistSession() {
    _session.open_graph_names.clear();
    for (const EditorTab& tab : _focus.open_editors()) {
      if (tab.kind == EditorTab::Kind::Graph && tab.graph != nullptr) {
        _session.open_graph_names.push_back(tab.graph->getName());
      }
    }
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
      const std::vector<std::shared_ptr<tg::Graph>>& graphs,
      std::shared_ptr<tg::Node>& selected_node) {
    const float ui_scale = ImGui::GetStyle().FontScaleMain;
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(kPickNodeKindMinWidthEpx * ui_scale, 0.0f),
        ImVec2(FLT_MAX, FLT_MAX));
    PushPickNodeKindStyle();
    if (!ImGui::BeginPopup(kPickNodeKindPopupId)) {
      PopPickNodeKindStyle();
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
    for (int kind_index = 0;
         kind_index < static_cast<int>(node_types.size()); ++kind_index) {
      const tg::NodeTypeInfo& kind = node_types[kind_index];
      if (!LabelMatchesFilter(kind.caption, _focus.pick_filter())) {
        continue;
      }
      ++visible_count;
      if (!ImGui::Selectable(kind.caption.c_str(), false,
                             ImGuiSelectableFlags_SpanAllColumns)) {
        continue;
      }
      PickNodeKindRequest& pick = _focus.pick_request();
      if (pick.graph_index >= 0 &&
          pick.graph_index < static_cast<int>(graphs.size())) {
        auto node = _controller.createNode(kind.type_id);
        if (node != nullptr) {
          // 画布右键打开种类时，下一帧按当时的画布坐标落位。
          if (pick.spawn_at_canvas) {
            _node_poses[node.get()] =
                tg::NodePose{pick.canvas_pos.x, pick.canvas_pos.y};
            pick.spawn_at_canvas = false;
          }
          graphs[pick.graph_index]->addNode(node);
          _controller.markDirty();
          OpenGraphInEditor(graphs[pick.graph_index]);
          selected_node = node;
          _focus.SelectNode(node.get());
        }
      }
      ImGui::CloseCurrentPopup();
    }
    if (visible_count == 0) {
      ImGui::TextDisabled("%s", kPickNodeKindEmptyLabel);
    }
    ImGui::EndPopup();
    PopPickNodeKindStyle();
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
    const float browse_width =
        ImGui::CalcTextSize(kBrowseActionLabel).x +
        ImGui::GetStyle().FramePadding.x * kFramePaddingSideCount;
    const float path_input_width =
        kProjectPathInputWidthEpx * ui_scale - browse_width -
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
    if (ImGui::Button(kBrowseActionLabel)) {
      const std::string default_path = NativePathDialog::resolveDefaultPath(
          _path_buffer, _session.last_browse_dir);
      const std::optional<std::string> picked = NativePathDialog::pickDirectory(
          kPickProjectDirectoryTitle, default_path);
      if (NativePathDialog::applyPickedPath(_path_buffer, picked)) {
        RememberBrowseDir(*picked);
      }
    }
    if (ImGui::Button("OK") || enter) {
      ApplyProjectPathDialog();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      _path_action = ProjectPathAction::None;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  tg::Controller& _controller;
  std::unordered_set<tg::Node*> _placed_nodes;
  std::unordered_map<tg::Node*, tg::NodePose> _node_poses;
  ProjectPathAction _path_action = ProjectPathAction::None;
  std::string _path_buffer;
  std::string _last_window_title;
  bool _path_dialog_open = false;
  bool _path_focus = false;
  bool _explorer_actions_press_hold = false;
  ax::NodeEditor::EditorContext* _editor = nullptr;
  // 工作台焦点：当前图、节点选中、页签、新建节点请求（Explorer/画布 pull）。
  WorkbenchFocus _focus;
  // 引脚右键菜单跨帧记住端口；ShowPinContextMenu 只在打开当帧给出 PinId。
  tg::Node::Port* _pin_context_port = nullptr;
  // 画布右键当帧落点；选 New Node 时写入 Focus 的 PickNodeKindRequest。
  ImVec2 _canvas_context_spawn_pos{};
  std::shared_ptr<tg::Graph> _graph_pending_remove;
  std::shared_ptr<tg::Graph> _rename_graph;
  std::string _rename_buffer;
  bool _rename_focus = false;
  UiSession _session;
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
