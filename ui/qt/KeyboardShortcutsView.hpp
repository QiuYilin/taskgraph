#pragma once

#include <QWidget>

class QLineEdit;
class QTableWidget;

/** \brief 编辑区里的只读快捷键页签。
 *
 *  数据来自 ui/model 的 EditorShortcutCatalog，与 imgui 版共用同一份清单；
 *  过滤规则也走 MatchingShortcutRows。
 */
class KeyboardShortcutsView : public QWidget {
  Q_OBJECT
 public:
  explicit KeyboardShortcutsView(QWidget* parent = nullptr);

 private:
  void Refresh();

  QLineEdit* _filter = nullptr;
  QTableWidget* _table = nullptr;
};
