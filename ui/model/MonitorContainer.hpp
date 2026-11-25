#pragma once

#include "UiSession.hpp"
#include "WorkbenchParts.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/** \brief Monitor 页签模型：实例列表、活动页签、容器开关和 session 迁移。
 *
 *  只保存页签身份和开关状态；Source/Follow、窗口几何和绘制由 imgui 层持有。
 */
class MonitorContainer {
 public:
  /** \brief 按 session 恢复页签；旧 view_windows 键只迁移一次。 */
  void Restore(const std::vector<MonitorView>& kinds, UiSession& session) {
    _instances.clear();
    _active.clear();
    _select.clear();
    _restored = true;

    for (const MonitorView& kind : kinds) {
      const auto saved = session.monitor_tabs.find(kind.id);
      if (saved != session.monitor_tabs.end()) {
        for (const MonitorTabRecord& record : saved->second) {
          // 空 id 或重复 id 不能重建。
          if (record.id.empty() || FindInstance(record.id) != nullptr) {
            continue;
          }
          MonitorInstance instance;
          instance.id = record.id;
          instance.kind_id = kind.id;
          instance.title =
              record.title.empty() ? kind.title : record.title;
          _instances.push_back(std::move(instance));
        }
        continue;
      }
      const auto legacy = session.view_windows.find(kind.id);
      if (legacy != session.view_windows.end()) {
        session.monitor_tabs[kind.id];
        if (legacy->second) {
          Create(kind.id, kinds);
        }
        session.view_windows.erase(legacy);
        continue;
      }
      if (kind.open_on_first_run) {
        Create(kind.id, kinds);
      }
    }

    std::string active = session.monitor_active_tab;
    if (FindInstance(active) == nullptr) {
      active = _instances.empty() ? std::string() : _instances.front().id;
    }
    _active = std::move(active);
    _select = _active;
    // 没有页签时不自动开空容器；共享 session 里别的应用的开关也不带进来。
    _open = !_instances.empty() &&
            (session.monitor_container_open_known
                 ? session.monitor_container_open
                 : true);
  }

  /** \brief 把当前页签写回 session；未注册 kind 的记录原样保留。 */
  void Save(const std::vector<MonitorView>& kinds, UiSession& session) const {
    if (!_restored) {
      return;
    }
    for (const MonitorView& kind : kinds) {
      // 迁移后不再保留旧的逐个 monitor 开关。
      session.view_windows.erase(kind.id);
      std::vector<MonitorTabRecord> records;
      for (const MonitorInstance& instance : _instances) {
        if (instance.kind_id != kind.id) {
          continue;
        }
        MonitorTabRecord record;
        record.id = instance.id;
        record.title = instance.title;
        records.push_back(std::move(record));
      }
      // 空数组表示这个 kind 的页签已被用户关完，不再种默认页签。
      session.monitor_tabs[kind.id] = std::move(records);
    }
    session.monitor_active_tab = _active;
    session.monitor_container_open = _open;
    session.monitor_container_open_known = true;
  }

  /** \brief 为 kind 新建一个页签并设为活动页签。 */
  bool Create(std::string_view kind_id,
              const std::vector<MonitorView>& kinds) {
    const MonitorView* kind = FindKind(kinds, kind_id);
    // 没注册的 kind 不建页签。
    if (kind == nullptr) {
      return false;
    }
    std::string title;
    const std::string id = NextInstanceId(*kind, &title);
    MonitorInstance instance;
    instance.id = id;
    instance.kind_id = kind->id;
    instance.title = std::move(title);
    _instances.push_back(std::move(instance));
    _active = id;
    _select = id;
    _open = true;
    return true;
  }

  /** \brief 关闭一个页签；最后一个关闭时隐藏容器但保留空 kind 记录。 */
  bool Close(std::string_view instance_id) {
    const auto found = std::find_if(
        _instances.begin(), _instances.end(),
        [instance_id](const MonitorInstance& instance) {
          return instance.id == instance_id;
        });
    // 页签已经不在列表里。
    if (found == _instances.end()) {
      return false;
    }
    const std::size_t index =
        static_cast<std::size_t>(found - _instances.begin());
    const std::string closed_id = found->id;
    _instances.erase(found);
    // 关的是活动页签时，优先选原位置或前一个。
    if (_active == closed_id) {
      if (_instances.empty()) {
        _active.clear();
        _select.clear();
        _open = false;
      } else {
        const std::size_t next =
            (std::min)(index, _instances.size() - 1);
        _active = _instances[next].id;
        _select = _active;
      }
    }
    return true;
  }

  void ToggleOpen() {
    _open = !_open;
    // 从 View 菜单打开且有页签时，恢复上次活动页签。
    if (_open && _active.empty() && !_instances.empty()) {
      _active = _instances.front().id;
      _select = _active;
    }
  }

  /** \brief 兼容旧入口：已有该 kind/实例就聚焦，否则新建。 */
  bool FocusOrCreate(std::string_view id,
                     const std::vector<MonitorView>& kinds) {
    for (const MonitorInstance& instance : _instances) {
      if (instance.id == id || instance.kind_id == id) {
        _active = instance.id;
        _select = instance.id;
        _open = true;
        return true;
      }
    }
    return Create(id, kinds);
  }

  bool IsOpen(std::string_view id) const {
    if (!_open) {
      return false;
    }
    for (const MonitorInstance& instance : _instances) {
      if (instance.id == id || instance.kind_id == id) {
        return true;
      }
    }
    return false;
  }

  const std::vector<MonitorInstance>& instances() const { return _instances; }

  const std::string& active_instance() const { return _active; }
  const std::string& select_instance() const { return _select; }

  void SetActive(std::string_view id) { _active = std::string(id); }
  void SetSelect(std::string_view id) { _select = std::string(id); }
  void ClearSelect() { _select.clear(); }

  bool open() const { return _open; }
  bool* open_ptr() { return &_open; }
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

  const MonitorInstance* FindInstance(std::string_view id) const {
    for (const MonitorInstance& instance : _instances) {
      if (instance.id == id) {
        return &instance;
      }
    }
    return nullptr;
  }

  // 生成一个同 kind 下未被占用的实例 id 和可见标题。
  std::string NextInstanceId(const MonitorView& kind,
                             std::string* title) const {
    for (int ordinal = 1;; ++ordinal) {
      const std::string id = MonitorInstanceId(kind.id, ordinal);
      const std::string candidate = MonitorInstanceTitle(kind.title, ordinal);
      bool id_used = false;
      bool title_used = false;
      for (const MonitorInstance& instance : _instances) {
        id_used = id_used || instance.id == id;
        title_used = title_used || instance.title == candidate;
      }
      // 同 id 或同标题都跳过，页签标题保持可区分。
      if (!id_used && !title_used) {
        if (title != nullptr) {
          *title = candidate;
        }
        return id;
      }
    }
  }

  std::vector<MonitorInstance> _instances;
  std::string _active;
  std::string _select;
  bool _open = false;
  bool _restored = false;
};
