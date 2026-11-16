#pragma once

#include <imgui.h>

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// 工作台可扩展部件的数据模型（Facade）：命令、活动栏项、视图动作、面板视图。
// 默认不注册任何可见项；只有注册后对应槽位才画内容。

// 图标绘制回调；不引入图标字体，未来可用线框或字体图标实现。
struct IconPainter {
  std::function<void(ImDrawList* draw, ImVec2 center, float size,
                     ImU32 color)>
      draw;
};

// 一条可执行命令；命令面板、菜单、动作按钮都引用同一个 id。
struct Command {
  std::string id;
  std::string label;
  std::string category;
  std::string shortcut;          // 只用于显示，不绑定快捷键
  std::function<bool()> enabled;  // 空 = 始终可用
  std::function<bool()> checked;  // 可选，供开关类命令显示勾选
  std::function<void()> run;
};

// 命令注册表：同 id 覆盖，避免重复项。
class CommandRegistry {
 public:
  void Set(Command command) {
    for (Command& existing : _commands) {
      // 同 id 覆盖已有命令，保留注册顺序。
      if (existing.id == command.id) {
        existing = std::move(command);
        return;
      }
    }
    _commands.push_back(std::move(command));
  }

  bool Remove(const std::string& id) {
    for (auto it = _commands.begin(); it != _commands.end(); ++it) {
      // 找到同 id 就删掉。
      if (it->id == id) {
        _commands.erase(it);
        return true;
      }
    }
    return false;
  }

  const Command* Find(std::string_view id) const {
    for (const Command& command : _commands) {
      // id 相同就是目标命令。
      if (std::string_view(command.id) == id) {
        return &command;
      }
    }
    return nullptr;
  }

  bool Enabled(std::string_view id) const {
    const Command* command = Find(id);
    // 未注册的命令视为不可用。
    if (command == nullptr) {
      return false;
    }
    return !command->enabled || command->enabled();
  }

  bool Execute(std::string_view id) const {
    const Command* command = Find(id);
    // 未注册或当前不可用时不执行。
    if (command == nullptr || !Enabled(id) || !command->run) {
      return false;
    }
    command->run();
    return true;
  }

  const std::vector<Command>& commands() const { return _commands; }

 private:
  std::vector<Command> _commands;
};

// 标题栏 / 面板工具栏上的动作；只引用命令 id。
struct ViewAction {
  std::string command;
  std::string tooltip;
  IconPainter icon;
};

// 活动栏的一项；点击执行命令，is_active 每帧 pull。
struct ActivityBarItem {
  std::string id;
  std::string tooltip;
  std::string command;
  std::function<bool()> is_active;
  IconPainter icon;
};

// 面板视图；draw 只画内容，窗口由 Ui 开。
struct PanelView {
  std::string id;
  std::string title;
  std::string docking_id;
  int order = 0;
  std::function<void()> draw;
  std::vector<ViewAction> toolbar_actions;

  std::string window_name() const { return title + "###" + docking_id; }
};

// 命令中心内容；draw 为空时仍画工程名文字。
struct CommandCenter {
  std::function<void(const ImVec2& size)> draw;
  float preferred_width_epx = 0.0f;
};

// 工作台部件集合；Ui 只读这些槽位。
struct WorkbenchParts {
  CommandRegistry commands;
  std::vector<ActivityBarItem> activity_bar_items;
  bool activity_bar_visible = false;
  std::vector<ViewAction> explorer_title_actions;
  std::vector<PanelView> panel_views;
  CommandCenter command_center;
};
