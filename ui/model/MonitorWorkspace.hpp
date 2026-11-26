#pragma once

#include "MonitorContainer.hpp"
#include "UiSession.hpp"
#include "WorkbenchParts.hpp"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

/** \brief 一个 Monitor 窗口及其页签分组。 */
struct MonitorWindow {
  std::string id;
  std::string title;
  MonitorContainer tabs;
  // 当前应用未注册的 kind；不渲染，但保存时带回 session。
  std::vector<MonitorTabRecord> dormant_tabs;
  // 只有 dormant tab 的窗口不创建运行时窗口。
  bool visible = true;
};

/** \brief Monitor 多窗口模型：窗口顺序、页签移动和 session 迁移。 */
class MonitorWorkspace {
 public:
  void Restore(const std::vector<MonitorView>& kinds, UiSession& session) {
    _windows.clear();
    _restored = true;
    _next_window_ordinal = (std::max)(1, session.monitor_next_window_ordinal);

    if (!session.monitor_windows.empty()) {
      for (const MonitorWindowRecord& record : session.monitor_windows) {
        RestoreWindow(record, kinds);
      }
      _next_window_ordinal =
          (std::max)(_next_window_ordinal, NextOrdinalFromWindows());
      return;
    }

    const bool has_legacy =
        !session.monitor_tabs.empty() ||
        !session.monitor_active_tab.empty() ||
        session.monitor_container_open_known;
    if (!has_legacy) {
      _next_window_ordinal = 1;
      return;
    }

    MonitorWindow window;
    window.id = "monitor";
    window.title = "Monitor";
    for (const auto& entry : session.monitor_tabs) {
      for (const MonitorTabRecord& record : entry.second) {
        MonitorTabRecord tab = record;
        tab.kind = entry.first;
        AddRestoredTab(window, tab, kinds);
      }
    }
    if (FindInstance(window, session.monitor_active_tab) != nullptr) {
      window.tabs.SetActive(session.monitor_active_tab);
    } else if (!window.tabs.instances().empty()) {
      window.tabs.SetActive(window.tabs.instances().front().id);
    }
    window.visible = !window.tabs.instances().empty() ||
                     (!window.dormant_tabs.empty() ? false
                                                   : session.monitor_container_open);
    if (!window.tabs.instances().empty() || !window.dormant_tabs.empty()) {
      _windows.push_back(std::move(window));
    }
    _next_window_ordinal = (std::max)(2, NextOrdinalFromWindows());
  }

  void Save(const std::vector<MonitorView>& kinds, UiSession& session) const {
    if (!_restored) {
      return;
    }
    session.monitor_windows.clear();
    for (const MonitorWindow& window : _windows) {
      MonitorWindowRecord record;
      record.id = window.id;
      record.title = window.title;
      record.active_tab = window.tabs.active_instance();
      for (const MonitorInstance& instance : window.tabs.instances()) {
        MonitorTabRecord tab;
        tab.id = instance.id;
        tab.kind = instance.kind_id;
        tab.title = instance.title;
        record.tabs.push_back(std::move(tab));
      }
      for (const MonitorTabRecord& tab : window.dormant_tabs) {
        record.tabs.push_back(tab);
      }
      session.monitor_windows.push_back(std::move(record));
    }
    session.monitor_next_window_ordinal = _next_window_ordinal;
    // 新 schema 不再写旧单容器字段。
    session.monitor_tabs.clear();
    session.monitor_active_tab.clear();
    session.monitor_container_open = false;
    session.monitor_container_open_known = false;
    (void)kinds;
  }

  bool CreateTab(std::string_view kind_id,
                 const std::vector<MonitorView>& kinds) {
    const MonitorView* kind = FindKind(kinds, kind_id);
    // 没注册的 kind 不建窗口。
    if (kind == nullptr) {
      return false;
    }
    // 每次新建都是独立窗口，不往已有窗口追加实例。
    MonitorWindow* window = CreateWindow();
    if (window == nullptr) {
      return false;
    }
    std::string title;
    const std::string id = NextInstanceId(*kind, &title);
    MonitorInstance instance;
    instance.id = id;
    instance.kind_id = kind->id;
    instance.title = std::move(title);
    window->tabs.Insert(std::move(instance), window->tabs.instances().size());
    window->visible = true;
    return true;
  }

  bool CloseTab(std::string_view window_id, std::string_view instance_id) {
    MonitorWindow* window = FindWindow(window_id);
    if (window == nullptr) {
      return false;
    }
    if (!window->tabs.Take(instance_id, nullptr)) {
      return false;
    }
    if (window->tabs.instances().empty()) {
      if (window->dormant_tabs.empty()) {
        RemoveWindow(window_id);
      } else {
        // 当前应用没有可见页签，但保留其它应用的 dormant 记录。
        window->visible = false;
      }
    }
    return true;
  }

  bool CloseWindow(std::string_view window_id) {
    return RemoveWindow(window_id);
  }

  bool MoveInstance(std::string_view source_window,
                    std::string_view instance_id,
                    std::string_view target_window, int target_index) {
    MonitorWindow* source = FindWindow(source_window);
    MonitorWindow* target = FindWindow(target_window);
    if (source == nullptr || target == nullptr) {
      return false;
    }
    if (source == target) {
      const int from = source->tabs.IndexOf(instance_id);
      if (from < 0) {
        return false;
      }
      int to = target_index;
      if (to > from) {
        --to;
      }
      return source->tabs.Move(static_cast<std::size_t>(from),
                               static_cast<std::size_t>((std::max)(0, to)));
    }
    MonitorInstance instance;
    if (!source->tabs.Take(instance_id, &instance)) {
      return false;
    }
    const int insert_at =
        (std::max)(0, (std::min)(target_index,
                                 static_cast<int>(target->tabs.instances().size())));
    target->tabs.Insert(std::move(instance),
                        static_cast<std::size_t>(insert_at));
    target->visible = true;
    if (source->tabs.instances().empty()) {
      if (source->dormant_tabs.empty()) {
        RemoveWindow(source_window);
      } else {
        source->visible = false;
      }
    }
    return true;
  }

  /** \brief 把页签拆到一个新窗口；返回新窗口 id，失败返回空。 */
  std::string DetachInstance(std::string_view source_window,
                             std::string_view instance_id) {
    MonitorWindow* source = FindWindow(source_window);
    if (source == nullptr) {
      return {};
    }
    // CreateWindow 可能让 source 指针失效，先记下源窗口删除条件。
    const bool source_last =
        source->tabs.instances().size() == std::size_t{1};
    const bool source_has_dormant = !source->dormant_tabs.empty();
    MonitorInstance instance;
    if (!source->tabs.Take(instance_id, &instance)) {
      return {};
    }
    if (source_last) {
      if (source_has_dormant) {
        source->visible = false;
      } else {
        RemoveWindow(source_window);
      }
    }
    MonitorWindow* created = CreateWindow();
    created->tabs.Insert(std::move(instance), 0);
    created->visible = true;
    return created->id;
  }

  /** \brief 已有可见实例则返回所在窗口；否则新建独立窗口。未注册返回空。 */
  std::string FocusOrCreate(std::string_view id,
                            const std::vector<MonitorView>& kinds) {
    for (MonitorWindow& window : _windows) {
      if (!window.visible) {
        continue;
      }
      const MonitorInstance* instance =
          window.tabs.FindInstance(id);
      if (instance != nullptr) {
        window.tabs.SetActive(instance->id);
        window.tabs.SetSelect(instance->id);
        return window.id;
      }
      for (const MonitorInstance& candidate : window.tabs.instances()) {
        if (candidate.kind_id == id) {
          window.tabs.SetActive(candidate.id);
          window.tabs.SetSelect(candidate.id);
          return window.id;
        }
      }
    }
    // 未注册的 kind 不建空窗口。
    if (FindKind(kinds, id) == nullptr) {
      return {};
    }
    if (!CreateTab(id, kinds)) {
      return {};
    }
    return _windows.back().id;
  }

  std::string FocusPrimary() {
    MonitorWindow* window = VisiblePrimary();
    if (window == nullptr) {
      window = CreateWindow();
    }
    return window->id;
  }

  bool IsOpen(std::string_view id) const {
    for (const MonitorWindow& window : _windows) {
      if (!window.visible) {
        continue;
      }
      for (const MonitorInstance& instance : window.tabs.instances()) {
        if (instance.id == id || instance.kind_id == id) {
          return true;
        }
      }
    }
    return false;
  }

  bool has_windows() const {
    for (const MonitorWindow& window : _windows) {
      if (window.visible) {
        return true;
      }
    }
    return false;
  }

  bool has_instances() const {
    for (const MonitorWindow& window : _windows) {
      if (!window.tabs.instances().empty()) {
        return true;
      }
    }
    return false;
  }

  std::vector<MonitorWindow>& windows() { return _windows; }
  const std::vector<MonitorWindow>& windows() const { return _windows; }

  MonitorWindow* FindWindow(std::string_view id) {
    for (MonitorWindow& window : _windows) {
      if (window.id == id) {
        return &window;
      }
    }
    return nullptr;
  }

  const MonitorWindow* FindWindow(std::string_view id) const {
    for (const MonitorWindow& window : _windows) {
      if (window.id == id) {
        return &window;
      }
    }
    return nullptr;
  }

  MonitorWindow* FindWindowForInstance(std::string_view instance_id) {
    for (MonitorWindow& window : _windows) {
      if (window.tabs.FindInstance(instance_id) != nullptr) {
        return &window;
      }
    }
    return nullptr;
  }

  bool restored() const { return _restored; }

 private:
  static const MonitorView* FindKind(const std::vector<MonitorView>& kinds,
                                     std::string_view id) {
    for (const MonitorView& kind : kinds) {
      if (kind.id == id) {
        return &kind;
      }
    }
    return nullptr;
  }

  static const MonitorInstance* FindInstance(const MonitorWindow& window,
                                             std::string_view id) {
    return window.tabs.FindInstance(id);
  }

  MonitorWindow* VisiblePrimary() {
    for (MonitorWindow& window : _windows) {
      if (window.visible) {
        return &window;
      }
    }
    return nullptr;
  }

  MonitorWindow* CreateWindow() {
    MonitorWindow window;
    int ordinal = (std::max)(1, _next_window_ordinal);
    // 避免 session 里的旧 id 和新建窗口冲突。
    while (FindWindow(ordinal == 1 ? "monitor"
                                   : "monitor-" + std::to_string(ordinal)) !=
           nullptr) {
      ++ordinal;
    }
    window.id =
        ordinal == 1 ? "monitor" : "monitor-" + std::to_string(ordinal);
    window.title = "Monitor";
    window.visible = true;
    _next_window_ordinal = ordinal + 1;
    _windows.push_back(std::move(window));
    return &_windows.back();
  }

  bool RemoveWindow(std::string_view id) {
    const auto found = std::find_if(
        _windows.begin(), _windows.end(),
        [id](const MonitorWindow& window) { return window.id == id; });
    if (found == _windows.end()) {
      return false;
    }
    _windows.erase(found);
    return true;
  }

  int NextOrdinalFromWindows() const {
    int next = 1;
    for (const MonitorWindow& window : _windows) {
      const std::size_t dash = window.id.rfind('-');
      if (dash == std::string::npos) {
        continue;
      }
      try {
        const int ordinal = std::stoi(window.id.substr(dash + 1));
        next = (std::max)(next, ordinal + 1);
      } catch (const std::exception&) {
        // 非法 id 不参与序号计算。
      }
    }
    return next;
  }

  std::string NextInstanceId(const MonitorView& kind, std::string* title) const {
    const std::string instance_title = MonitorInstanceTitle(kind.title);
    for (int ordinal = 1;; ++ordinal) {
      const std::string id = MonitorInstanceId(kind.id, ordinal);
      bool id_used = false;
      for (const MonitorWindow& window : _windows) {
        for (const MonitorInstance& instance : window.tabs.instances()) {
          id_used = id_used || instance.id == id;
        }
        for (const MonitorTabRecord& tab : window.dormant_tabs) {
          id_used = id_used || tab.id == id;
        }
      }
      // 标题可以重复；id 仍按序号唯一，ImGui 用 ###id 区分窗口。
      if (!id_used) {
        if (title != nullptr) {
          *title = instance_title;
        }
        return id;
      }
    }
  }

  void AddRestoredTab(MonitorWindow& window, const MonitorTabRecord& record,
                      const std::vector<MonitorView>& kinds) {
    if (record.id.empty()) {
      return;
    }
    const MonitorView* kind = FindKind(kinds, record.kind);
    // 旧 session 的 tab 可能没有 kind；只有一个注册 kind 时按它恢复。
    if (kind == nullptr && record.kind.empty() && kinds.size() == 1) {
      kind = &kinds.front();
    }
    // 仍未注册的 kind 留给其它应用，当前应用不渲染。
    if (kind == nullptr) {
      window.dormant_tabs.push_back(record);
      return;
    }
    MonitorInstance instance;
    instance.id = record.id;
    instance.kind_id = kind->id;
    // 已注册的 kind 用 kind 标题，不沿用 session 里带序号的旧标题。
    instance.title = MonitorInstanceTitle(kind->title);
    window.tabs.Insert(std::move(instance), window.tabs.instances().size());
  }

  void RestoreWindow(const MonitorWindowRecord& record,
                     const std::vector<MonitorView>& kinds) {
    MonitorWindow window;
    window.id = record.id;
    window.title = "Monitor";
    for (const MonitorTabRecord& tab : record.tabs) {
      AddRestoredTab(window, tab, kinds);
    }
    if (FindInstance(window, record.active_tab) != nullptr) {
      window.tabs.SetActive(record.active_tab);
    } else if (!window.tabs.instances().empty()) {
      window.tabs.SetActive(window.tabs.instances().front().id);
    }
    // 空窗口也要恢复；只有 dormant tab 的窗口不创建运行时窗口。
    window.visible = !window.tabs.instances().empty() ||
                     (record.tabs.empty() && window.dormant_tabs.empty());
    _windows.push_back(std::move(window));
  }

  std::vector<MonitorWindow> _windows;
  int _next_window_ordinal = 1;
  bool _restored = false;
};
