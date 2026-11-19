// ui 测试：选择路径。
// 选中路径写回、空路径拒绝、目录与文件选择、取消、默认路径优先级。

#include "NativePathDialog.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

namespace {


struct ScopedPickerReset {
  ~ScopedPickerReset() { NativePathDialog::resetPickers(); }
};

TEST(PathDialog, ApplyPickedPathWritesAndRejectsEmpty) {
  std::string target = "old";
  EXPECT_TRUE(NativePathDialog::applyPickedPath(
             target, std::optional<std::string>{"C:/projects/demo"})) << "apply non-empty path";
  EXPECT_EQ(target, "C:/projects/demo") << "target updated";

  EXPECT_FALSE(NativePathDialog::applyPickedPath(target, std::nullopt)) << "reject nullopt";
  EXPECT_EQ(target, "C:/projects/demo") << "target unchanged on nullopt";

  EXPECT_FALSE(NativePathDialog::applyPickedPath(
             target, std::optional<std::string>{""})) << "reject empty string";
  EXPECT_EQ(target, "C:/projects/demo") << "target unchanged on empty";
}

TEST(PathDialog, InjectedDirectoryPicker) {
  ScopedPickerReset reset;
  NativePathDialog::setDirectoryPicker(
      [](const std::string&, const std::string& default_path) {
        return std::optional<std::string>{default_path + "/picked"};
      });
  const std::optional<std::string> picked =
      NativePathDialog::pickDirectory("Select Project", "C:/root");
  EXPECT_TRUE(picked.has_value()) << "injected directory returns value";
  EXPECT_EQ(picked, std::optional<std::string>{"C:/root/picked"}) << "injected directory uses default_path";
}

TEST(PathDialog, InjectedFilePickerCancel) {
  ScopedPickerReset reset;
  NativePathDialog::setFilePicker(
      [](const std::string&, const std::string&) {
        return std::optional<std::string>{};
      });
  std::string target = "keep-me";
  const std::optional<std::string> picked =
      NativePathDialog::pickOpenFile("Select File", target);
  EXPECT_FALSE(picked.has_value()) << "cancel yields nullopt";
  EXPECT_FALSE(NativePathDialog::applyPickedPath(target, picked)) << "cancel does not apply";
  EXPECT_EQ(target, "keep-me") << "path kept after cancel";
}

TEST(PathDialog, BrowseFlowWritesProjectPathBuffer) {
  ScopedPickerReset reset;
  NativePathDialog::setDirectoryPicker(
      [](const std::string&, const std::string&) {
        return std::optional<std::string>{"D:/workspace/alpha"};
      });
  std::string path_buffer;
  const std::optional<std::string> picked =
      NativePathDialog::pickDirectory("Select Project", path_buffer);
  EXPECT_TRUE(NativePathDialog::applyPickedPath(path_buffer, picked)) << "browse writes project path buffer";
  EXPECT_EQ(path_buffer, "D:/workspace/alpha") << "project path buffer matches";
}

TEST(PathDialog, ResolveDefaultPathPriority) {
  constexpr const char* kLastBrowse = "D:/last-browse";
  EXPECT_EQ(NativePathDialog::resolveDefaultPath("C:/projects/demo", kLastBrowse), "C:/projects/demo") << "directory context wins over last browse";
  EXPECT_EQ(NativePathDialog::resolveDefaultPath("", kLastBrowse), kLastBrowse) << "empty context falls back to last browse";
  EXPECT_TRUE(NativePathDialog::resolveDefaultPath("", "").empty()) << "no context and no last browse yields empty for OS default";

  const std::filesystem::path temp_root =
      std::filesystem::temp_directory_path() / "taskgraph-browse-default";
  std::error_code error;
  std::filesystem::remove_all(temp_root, error);
  std::filesystem::create_directories(temp_root, error);
  EXPECT_FALSE(error) << "create temp browse dir";
  const std::filesystem::path file_path = temp_root / "a.png";
  {
    std::ofstream output(file_path, std::ios::binary);
    output << 'x';
    EXPECT_TRUE(static_cast<bool>(output)) << "write temp property file";
  }
  const std::string resolved = NativePathDialog::resolveDefaultPath(
      file_path.generic_string(), kLastBrowse);
  EXPECT_EQ(resolved, temp_root.generic_string()) << "file context uses parent directory";
  std::filesystem::remove_all(temp_root, error);
}

TEST(PathDialog, DirectoryOfPickedPath) {
  EXPECT_EQ(NativePathDialog::directoryOfPickedPath("D:/workspace/alpha"), "D:/workspace/alpha") << "directory pick remembered as itself";
  EXPECT_TRUE(NativePathDialog::directoryOfPickedPath("").empty()) << "empty pick yields empty directory";

  const std::filesystem::path temp_root =
      std::filesystem::temp_directory_path() / "taskgraph-browse-picked";
  std::error_code error;
  std::filesystem::remove_all(temp_root, error);
  std::filesystem::create_directories(temp_root, error);
  EXPECT_FALSE(error) << "create temp picked dir";
  const std::filesystem::path file_path = temp_root / "a.png";
  {
    std::ofstream output(file_path, std::ios::binary);
    output << 'x';
    EXPECT_TRUE(static_cast<bool>(output)) << "write temp picked file";
  }
  EXPECT_EQ(NativePathDialog::directoryOfPickedPath(file_path.generic_string()), temp_root.generic_string()) << "file pick remembered as parent";
  std::filesystem::remove_all(temp_root, error);
}

TEST(PathDialog, BrowseUsesResolvedDefaultPath) {
  ScopedPickerReset reset;
  std::string seen_default;
  NativePathDialog::setDirectoryPicker(
      [&seen_default](const std::string&, const std::string& default_path) {
        seen_default = default_path;
        return std::optional<std::string>{default_path + "/picked"};
      });
  const std::string default_path = NativePathDialog::resolveDefaultPath(
      "", "E:/recent-browse");
  const std::optional<std::string> picked =
      NativePathDialog::pickDirectory("Select Project", default_path);
  EXPECT_EQ(seen_default, "E:/recent-browse") << "picker receives resolved last browse dir";
  EXPECT_EQ(picked, std::optional<std::string>{"E:/recent-browse/picked"}) << "picker result uses resolved default";
}

}  // namespace
