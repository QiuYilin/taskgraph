#pragma once

#include "WorkbenchParts.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/** \brief 单个 Monitor 窗口内的页签列表。
 *
 *  窗口分组和 session 由 MonitorWorkspace 负责；这里只维护实例顺序和活动页签。
 */
class MonitorContainer {
 public:
  const std::vector<MonitorInstance>& instances() const { return _instances; }

  const MonitorInstance* FindInstance(std::string_view id) const {
    for (const MonitorInstance& instance : _instances) {
      if (instance.id == id) {
        return &instance;
      }
    }
    return nullptr;
  }

  int IndexOf(std::string_view id) const {
    for (int index = 0; index < static_cast<int>(_instances.size()); ++index) {
      if (_instances[index].id == id) {
        return index;
      }
    }
    return -1;
  }

  bool Insert(MonitorInstance instance, std::size_t index) {
    const std::size_t at = (std::min)(index, _instances.size());
    const std::string id = instance.id;
    _instances.insert(_instances.begin() + static_cast<std::ptrdiff_t>(at),
                      std::move(instance));
    _active = id;
    _select = id;
    return true;
  }

  bool Take(std::string_view id, MonitorInstance* out) {
    const auto found = std::find_if(
        _instances.begin(), _instances.end(),
        [id](const MonitorInstance& instance) { return instance.id == id; });
    // 页签已经不在列表里。
    if (found == _instances.end()) {
      return false;
    }
    const std::size_t index =
        static_cast<std::size_t>(found - _instances.begin());
    const std::string taken_id = found->id;
    if (out != nullptr) {
      *out = std::move(*found);
    }
    _instances.erase(found);
    // 删除活动页签后优先选中原位置或前一个。
    if (_active == taken_id) {
      if (_instances.empty()) {
        _active.clear();
        _select.clear();
      } else {
        const std::size_t next =
            (std::min)(index, _instances.size() - 1);
        _active = _instances[next].id;
        _select = _active;
      }
    }
    return true;
  }

  bool Move(std::size_t from, std::size_t to) {
    // 下标无效或原位不动。
    if (from >= _instances.size()) {
      return false;
    }
    to = (std::min)(to, _instances.size() - 1);
    if (from == to) {
      return true;
    }
    MonitorInstance instance = std::move(_instances[from]);
    const std::string id = instance.id;
    _instances.erase(_instances.begin() + static_cast<std::ptrdiff_t>(from));
    _instances.insert(_instances.begin() + static_cast<std::ptrdiff_t>(to),
                      std::move(instance));
    _active = id;
    _select = id;
    return true;
  }

  const std::string& active_instance() const { return _active; }
  const std::string& select_instance() const { return _select; }

  void SetActive(std::string_view id) { _active = std::string(id); }
  void SetSelect(std::string_view id) { _select = std::string(id); }
  void ClearSelect() { _select.clear(); }

 private:
  std::vector<MonitorInstance> _instances;
  std::string _active;
  std::string _select;
};
