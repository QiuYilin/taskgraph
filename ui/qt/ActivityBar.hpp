#pragma once

#include <QWidget>

#include "WorkbenchFocus.hpp"

class QToolButton;

/** \brief 主侧栏左边的活动栏：Explorer / Search 两个图标。
 *
 *  只发切换请求；当前视图由上层 SetView 回写，避免按钮和内容各记一份。
 */
class ActivityBar : public QWidget {
  Q_OBJECT
 public:
  explicit ActivityBar(QWidget* parent = nullptr);

  void SetView(PrimarySideBarView view);
  PrimarySideBarView view() const { return _view; }
  void ApplyTheme();

 signals:
  void viewRequested(PrimarySideBarView view);

 private:
  void UpdateIcons();

  QToolButton* _explorer = nullptr;
  QToolButton* _search = nullptr;
  PrimarySideBarView _view = PrimarySideBarView::Explorer;
};
