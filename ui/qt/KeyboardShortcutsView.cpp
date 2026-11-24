#include "KeyboardShortcutsView.hpp"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QLineEdit>
#include <QTableWidget>
#include <QVBoxLayout>

#include <string>
#include <vector>

#include "WorkbenchClipboard.hpp"

namespace {

enum Column { kCommand = 0, kKeybinding = 1, kWhen = 2, kColumnCount = 3 };
inline constexpr int kRowHeight = 22;

// Qt 画布只接了一部分共享清单；没绑定的键位不列，免得页签说谎。
bool IsQtShortcutCommand(const char* command) {
  if (command == nullptr) {
    return false;
  }
  const std::string name(command);
  return name == "Undo" || name == "Redo" || name == "Cut" ||
         name == "Copy" || name == "Paste" || name == "Delete" ||
         name == "Rename" || name == "Delete Node or Graph" ||
         name == "Cancel Placement";
}

// Qt 把同一组剪贴板键也绑到 Explorer；这组行只属于 Qt 页签，不进共享清单。
const ShortcutRow kQtExplorerRows[] = {
    {"Cut", kEditorCutShortcut, kWhenExplorerFocused},
    {"Copy", kEditorCopyShortcut, kWhenExplorerFocused},
    {"Paste Graph", kEditorPasteShortcut, kWhenExplorerFocused},
};

}  // namespace

KeyboardShortcutsView::KeyboardShortcutsView(QWidget* parent)
    : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(6);

  _filter = new QLineEdit(this);
  _filter->setPlaceholderText("Filter by command, key or when");
  _filter->setClearButtonEnabled(true);
  layout->addWidget(_filter);

  _table = new QTableWidget(this);
  _table->setColumnCount(kColumnCount);
  _table->setHorizontalHeaderLabels({"Command", "Keybinding", "When"});
  _table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  _table->setSelectionBehavior(QAbstractItemView::SelectRows);
  _table->setSelectionMode(QAbstractItemView::SingleSelection);
  _table->verticalHeader()->setVisible(false);
  _table->horizontalHeader()->setSectionResizeMode(kCommand,
                                                   QHeaderView::Stretch);
  _table->horizontalHeader()->setSectionResizeMode(kKeybinding,
                                                   QHeaderView::ResizeToContents);
  _table->horizontalHeader()->setSectionResizeMode(kWhen,
                                                   QHeaderView::Stretch);
  _table->verticalHeader()->setDefaultSectionSize(kRowHeight);
  layout->addWidget(_table, 1);

  connect(_filter, &QLineEdit::textChanged, this, [this] { Refresh(); });
  Refresh();
}

void KeyboardShortcutsView::Refresh() {
  const std::string filter = _filter->text().toStdString();
  std::vector<ShortcutRow> rows;
  std::size_t catalog_size = 0;
  const ShortcutRow* const catalog = EditorShortcutCatalog(catalog_size);
  for (std::size_t index = 0; index < catalog_size; ++index) {
    if (!IsQtShortcutCommand(catalog[index].command) ||
        !ShortcutRowMatches(catalog[index], filter)) {
      continue;
    }
    rows.push_back(catalog[index]);
  }
  for (const ShortcutRow& row : kQtExplorerRows) {
    if (ShortcutRowMatches(row, filter)) {
      rows.push_back(row);
    }
  }

  _table->setRowCount(static_cast<int>(rows.size()));
  for (int index = 0; index < static_cast<int>(rows.size()); ++index) {
    const ShortcutRow& row = rows[static_cast<std::size_t>(index)];
    _table->setItem(index, kCommand,
                    new QTableWidgetItem(QString::fromUtf8(
                        row.command != nullptr ? row.command : "")));
    _table->setItem(index, kKeybinding,
                    new QTableWidgetItem(QString::fromUtf8(
                        row.keybinding != nullptr ? row.keybinding : "")));
    _table->setItem(index, kWhen,
                    new QTableWidgetItem(QString::fromUtf8(
                        row.when_clause != nullptr ? row.when_clause : "")));
  }
}
