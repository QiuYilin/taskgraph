#pragma once

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

/** \brief 会话 Facade：读写用户配置目录下的 session.json（Recent / 上次工程 / 打开的图）。 */
struct UiSession {
  static constexpr std::size_t kMaxRecentRoots = 10;
  static constexpr const char* kAppConfigDirName = "taskgraph";
  static constexpr const char* kSessionFileName = "session.json";
  static constexpr const char* kJsonKeyRecentRoots = "recent_roots";
  static constexpr const char* kJsonKeyLastRoot = "last_root";
  static constexpr const char* kJsonKeyOpenGraphNames = "open_graph_names";
  static constexpr const char* kJsonKeyLastBrowseDir = "last_browse_dir";
  static constexpr const char* kEnvAppData = "APPDATA";
  static constexpr const char* kEnvXdgConfigHome = "XDG_CONFIG_HOME";
  static constexpr const char* kEnvHome = "HOME";
  static constexpr const char* kMacOsApplicationSupportRelative =
      "Library/Application Support";
  static constexpr const char* kLinuxConfigDirName = ".config";

  std::vector<std::string> recent_roots;
  std::string last_root;
  std::vector<std::string> open_graph_names;
  /** \brief 上次 Browse 落点目录（工程选目录或属性选文件的父目录）。 */
  std::string last_browse_dir;

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
    if (document.contains(kJsonKeyLastBrowseDir) &&
        document.at(kJsonKeyLastBrowseDir).is_string()) {
      session.last_browse_dir =
          document.at(kJsonKeyLastBrowseDir).get<std::string>();
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
    document[kJsonKeyLastBrowseDir] = last_browse_dir;
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
