#include "ThemeStyleQt.hpp"

#include <QColor>
#include <QString>

#include "UiColors.hpp"

namespace {

QString Hex(const Color& color) { return ToQColor(color).name(); }

void SetGroup(QPalette& palette, QPalette::ColorGroup group,
              QPalette::ColorRole role, const Color& color) {
  palette.setColor(group, role, ToQColor(color));
}

}  // namespace

QPalette BuildThemePalette(const ThemeTokens& tokens) {
  QPalette palette;
  palette.setColor(QPalette::Window, ToQColor(tokens.editor_background));
  palette.setColor(QPalette::WindowText, ToQColor(tokens.editor_foreground));
  palette.setColor(QPalette::Base, ToQColor(tokens.sidebar_background));
  palette.setColor(QPalette::AlternateBase, ToQColor(tokens.panel_background));
  palette.setColor(QPalette::Text, ToQColor(tokens.editor_foreground));
  palette.setColor(QPalette::Button, ToQColor(tokens.button_background));
  palette.setColor(QPalette::ButtonText, ToQColor(tokens.button_foreground));
  palette.setColor(QPalette::BrightText, ToQColor(tokens.error_foreground));
  palette.setColor(QPalette::Highlight,
                   ToQColor(tokens.list_active_selection_background));
  palette.setColor(QPalette::HighlightedText,
                   ToQColor(tokens.list_active_selection_foreground));
  palette.setColor(QPalette::ToolTipBase, ToQColor(tokens.input_background));
  palette.setColor(QPalette::ToolTipText, ToQColor(tokens.input_foreground));
  palette.setColor(QPalette::PlaceholderText,
                   ToQColor(tokens.disabled_foreground));
  for (QPalette::ColorGroup group :
       {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
    SetGroup(palette, group, QPalette::Window, tokens.editor_background);
    SetGroup(palette, group, QPalette::WindowText, tokens.editor_foreground);
    SetGroup(palette, group, QPalette::Base, tokens.sidebar_background);
    SetGroup(palette, group, QPalette::Text, tokens.sidebar_foreground);
    SetGroup(palette, group, QPalette::Button, tokens.button_background);
    SetGroup(palette, group, QPalette::ButtonText, tokens.button_foreground);
    SetGroup(palette, group, QPalette::Highlight,
             tokens.list_active_selection_background);
    SetGroup(palette, group, QPalette::HighlightedText,
             tokens.list_active_selection_foreground);
  }
  // 禁用态文字用同一套 token，避免系统默认的灰色和暗色主题不搭。
  for (QPalette::ColorRole role :
       {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
    SetGroup(palette, QPalette::Disabled, role, tokens.disabled_foreground);
  }
  return palette;
}

QString BuildThemeStyleSheet(const ThemeTokens& tokens) {
  const QString editor_bg = Hex(tokens.editor_background);
  const QString editor_fg = Hex(tokens.editor_foreground);
  const QString title_bg = Hex(tokens.title_bar_background);
  const QString title_fg = Hex(tokens.title_bar_foreground);
  const QString title_border = Hex(tokens.title_bar_border);
  const QString button_hover = Hex(tokens.button_hover_background);
  const QString button_bg = Hex(tokens.button_background);
  const QString button_fg = Hex(tokens.button_foreground);
  const QString selection_bg = Hex(tokens.list_active_selection_background);
  const QString selection_fg = Hex(tokens.list_active_selection_foreground);
  const QString status_bg = Hex(tokens.status_bar_background);
  const QString status_fg = Hex(tokens.status_bar_foreground);
  const QString status_border = Hex(tokens.status_bar_border);
  const QString status_hover = Hex(tokens.status_bar_item_hover);
  const QString sidebar_bg = Hex(tokens.sidebar_background);
  const QString sidebar_fg = Hex(tokens.sidebar_foreground);
  const QString sidebar_border = Hex(tokens.sidebar_border);
  const QString hover_bg = Hex(tokens.list_hover_background);
  const QString panel_bg = Hex(tokens.panel_background);
  const QString editor_border = Hex(tokens.editor_group_border);
  const QString tab_inactive_bg = Hex(tokens.tab_inactive_background);
  const QString tab_inactive_fg = Hex(tokens.tab_inactive_foreground);
  const QString tab_border = Hex(tokens.tab_border);
  const QString tab_active_bg = Hex(tokens.tab_active_background);
  const QString tab_active_fg = Hex(tokens.tab_active_foreground);
  const QString tab_active_border = Hex(tokens.tab_active_border_top);
  const QString tab_hover_bg = Hex(tokens.tab_hover_background);
  const QString input_bg = Hex(tokens.input_background);
  const QString input_fg = Hex(tokens.input_foreground);
  const QString input_border = Hex(tokens.input_border);
  const QString text_selection = Hex(tokens.text_selection_background);
  const QString focus_border = Hex(tokens.focus_border);

  QString qss;
  qss += "QMainWindow { background: " + editor_bg + "; } ";
  qss += "QMenuBar { background: " + title_bg + "; color: " + title_fg +
         "; border-bottom: 1px solid " + title_border + "; } ";
  qss += "QMenuBar::item { background: transparent; padding: 3px 8px; } ";
  qss += "QMenuBar::item:selected { background: " + button_hover + "; } ";
  qss += "QMenu { background: " + title_bg + "; color: " + title_fg +
         "; border: 1px solid " + title_border + "; } ";
  qss += "QMenu::item:selected { background: " + selection_bg + "; color: " +
         selection_fg + "; } ";
  qss += "QStatusBar { background: " + status_bg + "; color: " + status_fg +
         "; border-top: 1px solid " + status_border + "; } ";
  qss += "QStatusBar::item { border: none; } ";
  qss += "QStatusBar QLabel { color: " + status_fg + "; } ";
  qss += "QStatusBar QPushButton { background: transparent; color: " +
         status_fg + "; border: none; padding: 0 6px; } ";
  qss += "QStatusBar QPushButton:hover { background: " + status_hover + "; } ";
  qss += "QTreeView, QTableView, QListView { background: " + sidebar_bg +
         "; color: " + sidebar_fg + "; border: none; "
         "selection-background-color: " +
         selection_bg + "; selection-color: " + selection_fg + "; } ";
  qss += "QTreeView::item:hover, QTableView::item:hover, "
         "QListView::item:hover { background: " +
         hover_bg + "; } ";
  qss += "QHeaderView::section { background: " + panel_bg + "; color: " +
         sidebar_fg + "; border: none; border-bottom: 1px solid " +
         sidebar_border + "; padding: 3px; } ";
  qss += "QTabWidget::pane { border: 1px solid " + editor_border +
         "; background: " + editor_bg + "; } ";
  qss += "QTabBar::tab { background: " + tab_inactive_bg + "; color: " +
         tab_inactive_fg + "; border: 1px solid " + tab_border +
         "; border-bottom: none; padding: 4px 10px; } ";
  qss += "QTabBar::tab:selected { background: " + tab_active_bg + "; color: " +
         tab_active_fg + "; border-top: 2px solid " + tab_active_border +
         "; } ";
  qss += "QTabBar::tab:hover { background: " + tab_hover_bg + "; } ";
  qss += "QPlainTextEdit { background: " + panel_bg + "; color: " + editor_fg +
         "; border: none; } ";
  qss += "QLineEdit, QSpinBox { background: " + input_bg + "; color: " +
         input_fg + "; border: 1px solid " + input_border +
         "; padding: 2px 4px; selection-background-color: " + text_selection +
         "; } ";
  qss += "QPushButton { background: " + button_bg + "; color: " + button_fg +
         "; border: none; padding: 3px 10px; } ";
  qss += "QPushButton:hover { background: " + button_hover + "; } ";
  qss += "QToolTip { background: " + input_bg + "; color: " + input_fg +
         "; border: 1px solid " + input_border + "; padding: 2px; } ";
  qss += "#ActivityBar { background: " + sidebar_bg +
         "; border-right: 1px solid " + sidebar_border + "; } ";
  qss += "#ActivityBar QToolButton { background: transparent; border: none; "
         "border-left: 2px solid transparent; padding: 4px; } ";
  qss += "#ActivityBar QToolButton:hover { background: " + hover_bg + "; } ";
  qss += "#ActivityBar QToolButton:checked { border-left: 2px solid " +
         focus_border + "; }";
  // 页签栏右侧的 Show Type Names：checked 用页签选中底色，和 ImGui 一致。
  qss += "QToolButton#showTypeNamesButton { background: transparent; color: " +
         tab_inactive_fg + "; border: 1px solid transparent; padding: 0 8px; "
         "margin: 0 4px; } ";
  qss += "QToolButton#showTypeNamesButton:hover { background: " + tab_hover_bg +
         "; } ";
  qss += "QToolButton#showTypeNamesButton:checked { background: " +
         tab_active_bg + "; color: " + tab_active_fg + "; border: 1px solid " +
         tab_border + "; } ";
  return qss;
}
