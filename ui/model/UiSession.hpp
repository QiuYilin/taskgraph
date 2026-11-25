#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

/** \brief Monitor 页签在 session 里的记录；只保存重建窗口所需的最小信息。 */
struct MonitorTabRecord {
  std::string id;
  std::string title;
};

/** \brief 会话 Facade：读写用户配置目录下的 session.json（Recent / 上次工程 / 打开的图）。 */
struct UiSession {
  static constexpr std::size_t kMaxRecentRoots = 10;
  static constexpr const char* kAppConfigDirName = "taskgraph";
  static constexpr const char* kSessionFileName = "session.json";
  static constexpr const char* kJsonKeyRecentRoots = "recent_roots";
  static constexpr const char* kJsonKeyLastRoot = "last_root";
  static constexpr const char* kJsonKeyOpenGraphNames = "open_graph_names";
  static constexpr const char* kJsonKeyViewWindows = "view_windows";
  static constexpr const char* kJsonKeyMonitorTabs = "monitor_tabs";
  static constexpr const char* kJsonKeyMonitorActiveTab = "monitor_active_tab";
  static constexpr const char* kJsonKeyMonitorContainerOpen =
      "monitor_container_open";
  static constexpr const char* kJsonKeyLogLevel = "log_level";
  static constexpr const char* kJsonKeyLogCategories = "log_categories";
  static constexpr const char* kJsonKeyLogAutoScroll = "log_auto_scroll";
  static constexpr const char* kJsonKeyLastBrowseDir = "last_browse_dir";
  static constexpr const char* kJsonKeyThemeId = "theme_id";
  static constexpr const char* kEnvAppData = "APPDATA";
  static constexpr const char* kEnvXdgConfigHome = "XDG_CONFIG_HOME";
  static constexpr const char* kEnvHome = "HOME";
  static constexpr const char* kMacOsApplicationSupportRelative =
      "Library/Application Support";
  static constexpr const char* kLinuxConfigDirName = ".config";

  std::vector<std::string> recent_roots;
  std::string last_root;
  std::vector<std::string> open_graph_names;
  /** \brief 独立视图窗口的开关状态；没有记录表示从未开关过。 */
  std::map<std::string, bool> view_windows;
  /** \brief Monitor 页签：kind id -> 该 kind 当前存在的实例；空数组表示被关完。 */
  std::map<std::string, std::vector<MonitorTabRecord>> monitor_tabs;
  /** \brief 当前活动的 Monitor 实例 id；不在当前 kind 列表时忽略。 */
  std::string monitor_active_tab;
  /** \brief Monitor 容器是否打开；没有记录时由是否有页签决定。 */
  bool monitor_container_open = false;
  /** \brief 运行时标记：session 里是否真的写过容器开关；不落盘。 */
  bool monitor_container_open_known = false;
  /** \brief Output 日志级别；-1 表示未设置，使用固定 Summary。 */
  int log_level = -1;
  /** \brief Output 日志类别位；0xFFFFFFFF 表示未设置。 */
  std::uint32_t log_categories = 0xFFFFFFFFu;
  /** \brief Output 是否自动滚到末尾。 */
  bool log_auto_scroll = true;
  /** \brief 上次 Browse 落点目录（工程选目录或属性选文件的父目录）。 */
  std::string last_browse_dir;
  /** \brief 当前 VS Code 主题 id；空表示使用默认主题。 */
  std::string theme_id;

  /** \brief 测试用：非空则 configDirectory / session 读写都走该目录。 */
  static std::filesystem::path& configDirectoryOverride() {
    static std::filesystem::path override_path;
    return override_path;
  }

  static void setConfigDirectoryOverride(std::filesystem::path path) {
    configDirectoryOverride() = std::move(path);
  }

  static void clearConfigDirectoryOverride() { configDirectoryOverride().clear(); }

  static std::filesystem::path configDirectory() {
    if (!configDirectoryOverride().empty()) {
      return configDirectoryOverride();
    }
#if defined(_WIN32)
    if (const char* app_data = std::getenv(kEnvAppData);
        app_data != nullptr && app_data[0] != '\0') {
      return std::filesystem::path(app_data) / kAppConfigDirName;
    }
#elif defined(__APPLE__)
    if (const char* home = std::getenv(kEnvHome);
        home != nullptr && home[0] != '\0') {
      return std::filesystem::path(home) / kMacOsApplicationSupportRelative /
             kAppConfigDirName;
    }
#else
    if (const char* xdg = std::getenv(kEnvXdgConfigHome);
        xdg != nullptr && xdg[0] != '\0') {
      return std::filesystem::path(xdg) / kAppConfigDirName;
    }
    if (const char* home = std::getenv(kEnvHome);
        home != nullptr && home[0] != '\0') {
      return std::filesystem::path(home) / kLinuxConfigDirName / kAppConfigDirName;
    }
#endif
    return std::filesystem::path(kAppConfigDirName);
  }

  static std::filesystem::path sessionFilePath() {
    return configDirectory() / kSessionFileName;
  }

  static UiSession load() {
    UiSession session;
    const std::filesystem::path path = sessionFilePath();
    std::ifstream input(path);
    if (!input) {
      return session;
    }
    nlohmann::json document;
    try {
      input >> document;
    } catch (const nlohmann::json::exception&) {
      return session;
    }
    if (document.contains(kJsonKeyRecentRoots) &&
        document.at(kJsonKeyRecentRoots).is_array()) {
      for (const auto& entry : document.at(kJsonKeyRecentRoots)) {
        if (entry.is_string()) {
          session.recent_roots.push_back(entry.get<std::string>());
        }
      }
    }
    if (document.contains(kJsonKeyLastRoot) &&
        document.at(kJsonKeyLastRoot).is_string()) {
      session.last_root = document.at(kJsonKeyLastRoot).get<std::string>();
    }
    if (document.contains(kJsonKeyOpenGraphNames) &&
        document.at(kJsonKeyOpenGraphNames).is_array()) {
      for (const auto& entry : document.at(kJsonKeyOpenGraphNames)) {
        if (entry.is_string()) {
          session.open_graph_names.push_back(entry.get<std::string>());
        }
      }
    }
    if (document.contains(kJsonKeyViewWindows) &&
        document.at(kJsonKeyViewWindows).is_object()) {
      for (auto entry = document.at(kJsonKeyViewWindows).begin();
           entry != document.at(kJsonKeyViewWindows).end(); ++entry) {
        if (entry.value().is_boolean()) {
          session.view_windows[entry.key()] = entry.value().get<bool>();
        }
      }
    }
    if (document.contains(kJsonKeyMonitorTabs) &&
        document.at(kJsonKeyMonitorTabs).is_object()) {
      for (auto kind = document.at(kJsonKeyMonitorTabs).begin();
           kind != document.at(kJsonKeyMonitorTabs).end(); ++kind) {
        if (!kind.value().is_array()) {
          continue;
        }
        std::vector<MonitorTabRecord>& records = session.monitor_tabs[kind.key()];
        for (const auto& entry : kind.value()) {
          if (!entry.is_object() || !entry.contains("id") ||
              !entry.at("id").is_string()) {
            continue;
          }
          MonitorTabRecord record;
          record.id = entry.at("id").get<std::string>();
          if (entry.contains("title") && entry.at("title").is_string()) {
            record.title = entry.at("title").get<std::string>();
          }
          records.push_back(std::move(record));
        }
      }
    }
    if (document.contains(kJsonKeyMonitorActiveTab) &&
        document.at(kJsonKeyMonitorActiveTab).is_string()) {
      session.monitor_active_tab =
          document.at(kJsonKeyMonitorActiveTab).get<std::string>();
    }
    if (document.contains(kJsonKeyMonitorContainerOpen) &&
        document.at(kJsonKeyMonitorContainerOpen).is_boolean()) {
      session.monitor_container_open =
          document.at(kJsonKeyMonitorContainerOpen).get<bool>();
      session.monitor_container_open_known = true;
    }
    if (document.contains(kJsonKeyLogLevel) &&
        document.at(kJsonKeyLogLevel).is_number_integer()) {
      session.log_level = document.at(kJsonKeyLogLevel).get<int>();
    }
    if (document.contains(kJsonKeyLogCategories) &&
        (document.at(kJsonKeyLogCategories).is_number_integer() ||
         document.at(kJsonKeyLogCategories).is_number_unsigned())) {
      session.log_categories = static_cast<std::uint32_t>(
          document.at(kJsonKeyLogCategories).get<std::int64_t>());
    }
    if (document.contains(kJsonKeyLogAutoScroll) &&
        document.at(kJsonKeyLogAutoScroll).is_boolean()) {
      session.log_auto_scroll =
          document.at(kJsonKeyLogAutoScroll).get<bool>();
    }
    if (document.contains(kJsonKeyLastBrowseDir) &&
        document.at(kJsonKeyLastBrowseDir).is_string()) {
      session.last_browse_dir =
          document.at(kJsonKeyLastBrowseDir).get<std::string>();
    }
    if (document.contains(kJsonKeyThemeId) &&
        document.at(kJsonKeyThemeId).is_string()) {
      session.theme_id = document.at(kJsonKeyThemeId).get<std::string>();
    }
    return session;
  }

  bool save() const {
    std::error_code error;
    std::filesystem::create_directories(configDirectory(), error);
    if (error) {
      return false;
    }
    nlohmann::json document;
    document[kJsonKeyRecentRoots] = recent_roots;
    document[kJsonKeyLastRoot] = last_root;
    document[kJsonKeyOpenGraphNames] = open_graph_names;
    document[kJsonKeyViewWindows] = view_windows;
    nlohmann::json monitor_tabs_json = nlohmann::json::object();
    for (const auto& entry : monitor_tabs) {
      nlohmann::json records = nlohmann::json::array();
      for (const MonitorTabRecord& record : entry.second) {
        nlohmann::json item = nlohmann::json::object();
        item["id"] = record.id;
        item["title"] = record.title;
        records.push_back(std::move(item));
      }
      monitor_tabs_json[entry.first] = std::move(records);
    }
    document[kJsonKeyMonitorTabs] = std::move(monitor_tabs_json);
    document[kJsonKeyMonitorActiveTab] = monitor_active_tab;
    document[kJsonKeyMonitorContainerOpen] = monitor_container_open;
    document[kJsonKeyLogLevel] = log_level;
    document[kJsonKeyLogCategories] = log_categories;
    document[kJsonKeyLogAutoScroll] = log_auto_scroll;
    document[kJsonKeyLastBrowseDir] = last_browse_dir;
    document[kJsonKeyThemeId] = theme_id;
    std::ofstream output(sessionFilePath(), std::ios::trunc);
    if (!output) {
      return false;
    }
    output << document.dump(2);
    return static_cast<bool>(output);
  }

  void touchRecent(const std::filesystem::path& root) {
    if (root.empty()) {
      return;
    }
    const std::string key = root.generic_string();
    recent_roots.erase(
        std::remove(recent_roots.begin(), recent_roots.end(), key),
        recent_roots.end());
    recent_roots.insert(recent_roots.begin(), key);
    while (recent_roots.size() > kMaxRecentRoots) {
      recent_roots.pop_back();
    }
    last_root = key;
  }

  void removeRecent(const std::filesystem::path& root) {
    if (root.empty()) {
      return;
    }
    const std::string key = root.generic_string();
    recent_roots.erase(
        std::remove(recent_roots.begin(), recent_roots.end(), key),
        recent_roots.end());
    if (last_root == key) {
      last_root.clear();
    }
  }
};
