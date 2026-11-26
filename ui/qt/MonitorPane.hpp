#pragma once

#include "MonitorSources.hpp"

#include <QWidget>

namespace tg {
class Controller;
class Node;
}

#include <string>
#include <vector>

class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QToolButton;

// 与 ImGui 视图窗口首次尺寸一致（ui/imgui/UiWidgets.hpp）。
inline constexpr int kMonitorDefaultWidth = 720;
inline constexpr int kMonitorDefaultHeight = 540;
inline constexpr int kMonitorEdgeMargin = 24;
inline constexpr int kMonitorCascadeStep = 24;
inline constexpr int kMonitorSourceMinWidth = 40;
inline constexpr int kMonitorSourceMaxWidth = 180;
inline constexpr int kMonitorSourcePopupHeight = 270;
inline constexpr int kMonitorMinContentHeight = 80;

/** \brief 一个 Monitor 实例的内容：Source / Follow 工具栏和正文。
 *
 *  来源状态在 MonitorWindowState；这里只观察当前选中节点并刷新。
 */
class MonitorPane : public QWidget {
  Q_OBJECT

 public:
  MonitorPane(const tg::Controller& project,
              const std::vector<MonitorView>& kinds, std::string kind_id,
              MonitorWindowState& state, QWidget* parent = nullptr);

  void Refresh(tg::Node* selected);

 protected:
  void resizeEvent(QResizeEvent* event) override;

 private:
  const MonitorView* Kind() const;
  void ShowSourcePopup();
  void ApplyFollowToggle();
  void RebuildSourceList();
  void UpdateToolbarFit();
  void PinSource(const MonitorSourceEntry& source);

  const tg::Controller& _project;
  const std::vector<MonitorView>& _kinds;
  std::string _kind_id;
  MonitorWindowState& _state;
  tg::Node* _selected = nullptr;
  MonitorSourceEntry _resolved;
  std::vector<MonitorSourceEntry> _popup_sources;

  QPushButton* _source = nullptr;
  QCheckBox* _follow = nullptr;
  QToolButton* _overflow = nullptr;
  QLabel* _message = nullptr;
  QPlainTextEdit* _body = nullptr;
  QLabel* _status = nullptr;
  QWidget* _popup = nullptr;
  QLineEdit* _filter = nullptr;
  QListWidget* _source_list = nullptr;
};
