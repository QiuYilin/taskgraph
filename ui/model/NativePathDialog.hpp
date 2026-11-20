#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <portable-file-dialogs.h>

/** \brief 原生路径对话框 Facade；钩子可注入，供 ctest 不弹系统窗。 */
struct NativePathDialog {
  using DirectoryPicker = std::function<std::optional<std::string>(
      const std::string& title, const std::string& default_path)>;
  using FilePicker = std::function<std::optional<std::string>(
      const std::string& title, const std::string& default_path)>;

  static DirectoryPicker& directoryPicker() {
    static DirectoryPicker picker = DefaultDirectoryPicker();
    return picker;
  }

  static FilePicker& filePicker() {
    static FilePicker picker = DefaultFilePicker();
    return picker;
  }

  static void setDirectoryPicker(DirectoryPicker picker) {
    directoryPicker() = std::move(picker);
  }

  static void setFilePicker(FilePicker picker) {
    filePicker() = std::move(picker);
  }

  static void resetPickers() {
    directoryPicker() = DefaultDirectoryPicker();
    filePicker() = DefaultFilePicker();
  }

  static std::optional<std::string> pickDirectory(
      const std::string& title, const std::string& default_path = {}) {
    return directoryPicker()(title, default_path);
  }

  static std::optional<std::string> pickOpenFile(
      const std::string& title, const std::string& default_path = {}) {
    return filePicker()(title, default_path);
  }

  /** \brief 把选中路径写回缓冲；取消或空路径则不变。 */
  static bool applyPickedPath(std::string& target,
                              const std::optional<std::string>& picked) {
    if (!picked.has_value() || picked->empty()) {
      return false;
    }
    target = *picked;
    return true;
  }

  /**
   * \brief Browse 初始目录：当前上下文 → 上次 Browse 目录 → 空（交给 OS）。
   * 上下文为文件路径时取其父目录；已是目录则原样用。
   * 交给系统对话框前由 pick* 转成本机分隔符，并在有路径时 force_path，
   * 否则 Windows 会用「最近文件夹」（常为文档）盖掉 SetDefaultFolder。
   */
  static std::string resolveDefaultPath(const std::string& context_path,
                                        const std::string& last_browse_dir) {
    if (!context_path.empty()) {
      const std::filesystem::path context(context_path);
      std::error_code error;
      // 已存在的文件取父目录；目录或尚未创建的路径原样作起点。
      if (std::filesystem::is_regular_file(context, error)) {
        const std::filesystem::path parent = context.parent_path();
        if (!parent.empty()) {
          return parent.generic_string();
        }
      }
      return context.generic_string();
    }
    return last_browse_dir;
  }

  /** \brief 系统对话框要本机分隔符；空路径保持空。 */
  static std::string nativeDialogPath(const std::string& path) {
    if (path.empty()) {
      return {};
    }
    std::filesystem::path native(path);
    native.make_preferred();
    return native.string();
  }

  /** \brief 选中路径对应的目录：目录原样；已存在文件取其父目录。 */
  static std::string directoryOfPickedPath(const std::string& picked_path) {
    if (picked_path.empty()) {
      return {};
    }
    const std::filesystem::path picked(picked_path);
    std::error_code error;
    if (std::filesystem::is_regular_file(picked, error)) {
      const std::filesystem::path parent = picked.parent_path();
      if (!parent.empty()) {
        return parent.generic_string();
      }
    }
    return picked.generic_string();
  }

 private:
  static DirectoryPicker DefaultDirectoryPicker() {
    return [](const std::string& title, const std::string& default_path) {
      const std::string native_default = nativeDialogPath(default_path);
      // force_path → IFileDialog::SetFolder；否则 Windows 用最近目录盖掉默认。
      const pfd::opt options =
          native_default.empty() ? pfd::opt::none : pfd::opt::force_path;
      const std::string selected =
          pfd::select_folder(title, native_default, options).result();
      if (selected.empty()) {
        return std::optional<std::string>{};
      }
      return std::optional<std::string>{selected};
    };
  }

  static FilePicker DefaultFilePicker() {
    return [](const std::string& title, const std::string& default_path) {
      const std::vector<std::string> filters = {"All Files", "*"};
      const std::string native_default = nativeDialogPath(default_path);
      const std::vector<std::string> selected =
          pfd::open_file(title, native_default, filters).result();
      if (selected.empty() || selected.front().empty()) {
        return std::optional<std::string>{};
      }
      return std::optional<std::string>{selected.front()};
    };
  }
};
