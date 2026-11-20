#pragma once

#include <QPalette>
#include <QString>

#include "Theme.hpp"

// 把 ui/model 的主题 token 落成 Qt 的调色板与局部 QSS。
// 调色板覆盖菜单、对话框和 ADS 停靠；QSS 只管工作台自己的控件，不写
// 全局 QWidget 规则，避免把按钮/输入框的默认外观整个吃掉。
QPalette BuildThemePalette(const ThemeTokens& tokens);
QString BuildThemeStyleSheet(const ThemeTokens& tokens);
