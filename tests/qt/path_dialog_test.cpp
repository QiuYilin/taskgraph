// ui 测试：选择路径（QtTest）。
// 选中路径写回、空路径拒绝、注入选择器、取消、默认路径优先级、目录归属。

#include "NativePathDialog.hpp"

#include <QtTest>

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace {

struct ScopedPickerReset {
  ~ScopedPickerReset() { NativePathDialog::resetPickers(); }
};

}  // namespace

class PathDialogTest : public QObject {
  Q_OBJECT

 private slots:
  void applyPickedPathWritesAndRejectsEmpty();
  void injectedDirectoryPicker();
  void injectedFilePickerCancel();
  void browseFlowWritesProjectPathBuffer();
  void resolveDefaultPathPriority();
  void directoryOfPickedPath();
  void browseUsesResolvedDefaultPath();
};

void PathDialogTest::applyPickedPathWritesAndRejectsEmpty() {
  std::string target = "old";
  QVERIFY(NativePathDialog::applyPickedPath(
      target, std::optional<std::string>{"C:/projects/demo"}));
  QVERIFY(target == "C:/projects/demo");

  QVERIFY(!NativePathDialog::applyPickedPath(target, std::nullopt));
  QVERIFY(target == "C:/projects/demo");

  QVERIFY(!NativePathDialog::applyPickedPath(
      target, std::optional<std::string>{""}));
  QVERIFY(target == "C:/projects/demo");
}

void PathDialogTest::injectedDirectoryPicker() {
  ScopedPickerReset reset;
  NativePathDialog::setDirectoryPicker(
      [](const std::string&, const std::string& default_path)
          -> std::optional<std::string> {
        return std::optional<std::string>{default_path + "/picked"};
      });
  const std::optional<std::string> picked =
      NativePathDialog::pickDirectory("Select Project", "C:/root");
  QVERIFY(picked.has_value());
  QVERIFY(*picked == "C:/root/picked");
}

void PathDialogTest::injectedFilePickerCancel() {
  ScopedPickerReset reset;
  NativePathDialog::setFilePicker(
      [](const std::string&, const std::string&)
          -> std::optional<std::string> { return std::optional<std::string>{}; });
  std::string target = "keep-me";
  const std::optional<std::string> picked =
      NativePathDialog::pickOpenFile("Select File", target);
  QVERIFY(!picked.has_value());
  QVERIFY(!NativePathDialog::applyPickedPath(target, picked));
  QVERIFY(target == "keep-me");
}

void PathDialogTest::browseFlowWritesProjectPathBuffer() {
  ScopedPickerReset reset;
  NativePathDialog::setDirectoryPicker(
      [](const std::string&, const std::string&)
          -> std::optional<std::string> {
        return std::optional<std::string>{"D:/workspace/alpha"};
      });
  std::string path_buffer;
  const std::optional<std::string> picked =
      NativePathDialog::pickDirectory("Select Project", path_buffer);
  QVERIFY(NativePathDialog::applyPickedPath(path_buffer, picked));
  QVERIFY(path_buffer == "D:/workspace/alpha");
}

void PathDialogTest::resolveDefaultPathPriority() {
  const std::string kLastBrowse = "D:/last-browse";
  QVERIFY(NativePathDialog::resolveDefaultPath("C:/projects/demo",
                                               kLastBrowse) ==
          "C:/projects/demo");
  QVERIFY(NativePathDialog::resolveDefaultPath("", kLastBrowse) == kLastBrowse);
  QVERIFY(NativePathDialog::resolveDefaultPath("", "").empty());

  const std::filesystem::path temp_root =
      std::filesystem::temp_directory_path() / "taskgraph-browse-default";
  std::error_code error;
  std::filesystem::remove_all(temp_root, error);
  std::filesystem::create_directories(temp_root, error);
  QVERIFY(!error);
  const std::filesystem::path file_path = temp_root / "a.png";
  {
    std::ofstream output(file_path, std::ios::binary);
    output << 'x';
    QVERIFY(static_cast<bool>(output));
  }
  const std::string resolved = NativePathDialog::resolveDefaultPath(
      file_path.generic_string(), kLastBrowse);
  QVERIFY(resolved == temp_root.generic_string());
  std::filesystem::remove_all(temp_root, error);
}

void PathDialogTest::directoryOfPickedPath() {
  QVERIFY(NativePathDialog::directoryOfPickedPath("D:/workspace/alpha") ==
          "D:/workspace/alpha");
  QVERIFY(NativePathDialog::directoryOfPickedPath("").empty());

  const std::filesystem::path temp_root =
      std::filesystem::temp_directory_path() / "taskgraph-browse-picked";
  std::error_code error;
  std::filesystem::remove_all(temp_root, error);
  std::filesystem::create_directories(temp_root, error);
  QVERIFY(!error);
  const std::filesystem::path file_path = temp_root / "a.png";
  {
    std::ofstream output(file_path, std::ios::binary);
    output << 'x';
    QVERIFY(static_cast<bool>(output));
  }
  QVERIFY(NativePathDialog::directoryOfPickedPath(file_path.generic_string()) ==
          temp_root.generic_string());
  std::filesystem::remove_all(temp_root, error);
}

void PathDialogTest::browseUsesResolvedDefaultPath() {
  ScopedPickerReset reset;
  std::string seen_default;
  NativePathDialog::setDirectoryPicker(
      [&seen_default](const std::string&, const std::string& default_path)
          -> std::optional<std::string> {
        seen_default = default_path;
        return std::optional<std::string>{default_path + "/picked"};
      });
  const std::string default_path =
      NativePathDialog::resolveDefaultPath("", "E:/recent-browse");
  const std::optional<std::string> picked =
      NativePathDialog::pickDirectory("Select Project", default_path);
  QVERIFY(seen_default == "E:/recent-browse");
  QVERIFY(picked.has_value());
  QVERIFY(*picked == "E:/recent-browse/picked");
}

QTEST_GUILESS_MAIN(PathDialogTest)
#include "path_dialog_test.moc"
