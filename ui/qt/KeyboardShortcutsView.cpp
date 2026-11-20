#include "KeyboardShortcutsView.hpp"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QLineEdit>
#include <QTableWidget>
#include <QVBoxLayout>

#include <string>
#include <vector>

#include "EditorClipboard.hpp"

namespace {

enum Column { kCommand = 0, kKeybinding = 1, kWhen = 2, kColumnCount = 3 };
inline constexpr int kRowHeight = 22;
inline constexpr const char* kUndoCommand = "Undo";
inline constexpr const char* kRedoCommand = "Redo";
#if defined(__APPLE__)
inline constexpr const char* kUndoKey = "Cmd+Z";
inline constexpr const char* kRedoKey = "Cmd+Shift+Z";
#else
inline constexpr const char* kUndoKey = "Ctrl+Z";
inline constexpr const char* kRedoKey = "Ctrl+Y";
#endif

// Qt 画布只接了一部分共享清单；没绑定的键位不列，免得页签说谎。
bool IsQtShortcutCommand(const char* command) {
  if (command == nullptr) {
    return false;
  }
  const std::string name(command);
  return name == "Cut" || name == "Copy" || name == "Paste" ||
         name == "Duplicate" || name == "Delete" || name == "Rename" ||
         name == "Delete Node or Graph" || name == "Cancel Placement";
}

// QtNodes 自带撤销/重做，共享清单里没有这两行。
constexpr ShortcutRow kQtExtraRows[] = {
    {kUndoCommand, kUndoKey, kWhenEditorFocused},
    {kRedoCommand, kRedoKey, kWhenEditorFocused},
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
  for (const ShortcutRow& row : kQtExtraRows) {
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
