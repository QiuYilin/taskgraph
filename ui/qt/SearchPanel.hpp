#pragma once

#include <QWidget>

#include <functional>

#include "WorkbenchEdits.hpp"

namespace tg {
class Controller;
}
class QLineEdit;
class QListWidget;

/** \brief 主侧栏的 Search 视图：按图名和节点 identifier 列结果。
 *
 *  结果来自 ui/model 的 CollectSearchHits；面板只展示，点击交给上层打开
 *  对应的图/节点。
 */
class SearchPanel : public QWidget {
  Q_OBJECT
 public:
  explicit SearchPanel(tg::Controller& controller, QWidget* parent = nullptr);

  void Refresh();
  void FocusFilter();
  std::function<void(const SearchHit&)> on_activate;

 private:
  void RebuildResults();

  tg::Controller& _controller;
  QLineEdit* _filter = nullptr;
  QListWidget* _list = nullptr;
  std::vector<SearchHit> _hits;
};
