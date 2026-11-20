#pragma once

#include <QMainWindow>
#include <QPoint>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>

namespace tg {
class Controller;
}
struct UiSession;
class ProjectModel;
class EditorModel;
class EditorTabsView;
class GraphModel;
class PropertyModel;
class ActivityBar;
class SearchPanel;
class QAction;
class QLabel;
class QMenu;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QTableView;
class QTimer;
class QTreeView;
enum class PrimarySideBarView;
namespace ads {
class CDockManager;
class CDockWidget;
}

class Ui : public QMainWindow {
 public:
  explicit Ui(tg::Controller& project);
  ~Ui() override;

  void Run() { show(); }

 private:
  void BuildMenuBar();
  void BuildWorkbench();
  void BuildStatusBar();
  void ApplyTheme();
  void RestoreThemeFromSession();
  void SelectTheme(const std::string& theme_id);
  void SetPrimarySideBarView(PrimarySideBarView view);
  void ShowNodeKindMenu(GraphModel* graph, const QPoint& global_pos);
  void ArmNode(GraphModel* graph, const std::string& type_id);
  void ClearArmedNode();
  void RunActiveGraph();
  void UpdateStatusBar();
  void PollLogLines();
  void NewProject();
  void OpenProject();
  void OpenProjectFromPath(const std::filesystem::path& root);
  void SaveProject();
  void SaveProjectAs();
  void RebuildRecentMenu();
  void UpdateProjectUi();
  void RememberOpenedProject(const std::filesystem::path& root);
  void PersistSession();

  tg::Controller& _project;
  std::unique_ptr<UiSession> _session;
  ads::CDockManager* _dock_manager = nullptr;
  ProjectModel* _project_model = nullptr;
  EditorModel* _editor_model = nullptr;
  QTreeView* _tree = nullptr;
  QStackedWidget* _sidebar_stack = nullptr;
  ActivityBar* _activity_bar = nullptr;
  SearchPanel* _search_panel = nullptr;
  ads::CDockWidget* _explorer_dock = nullptr;
  EditorTabsView* _tabs = nullptr;
  PropertyModel* _property_model = nullptr;
  QTableView* _property_table = nullptr;
  ads::CDockWidget* _properties_dock = nullptr;
  QPlainTextEdit* _output = nullptr;
  ads::CDockWidget* _output_dock = nullptr;
  QAction* _run_action = nullptr;
  QPushButton* _run_button = nullptr;
  QLabel* _status_project = nullptr;
  QLabel* _status_graph = nullptr;
  QLabel* _status_message = nullptr;
  QLabel* _status_selected = nullptr;
  QTimer* _status_timer = nullptr;
  QMenu* _recent_menu = nullptr;
  std::size_t _log_cursor = 0;
  std::string _armed_node_type;
  GraphModel* _armed_graph = nullptr;
};
