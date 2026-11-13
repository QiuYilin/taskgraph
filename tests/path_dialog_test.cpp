#include "NativePathDialog.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

namespace {

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    ++g_failures;
  }
}

struct ScopedPickerReset {
  ~ScopedPickerReset() { NativePathDialog::resetPickers(); }
};

void TestApplyPickedPathWritesAndRejectsEmpty() {
  std::string target = "old";
  expect(NativePathDialog::applyPickedPath(
             target, std::optional<std::string>{"C:/projects/demo"}),
         "apply non-empty path");
  expect(target == "C:/projects/demo", "target updated");

  expect(!NativePathDialog::applyPickedPath(target, std::nullopt),
         "reject nullopt");
  expect(target == "C:/projects/demo", "target unchanged on nullopt");

  expect(!NativePathDialog::applyPickedPath(
             target, std::optional<std::string>{""}),
         "reject empty string");
  expect(target == "C:/projects/demo", "target unchanged on empty");
}

void TestInjectedDirectoryPicker() {
  ScopedPickerReset reset;
  NativePathDialog::setDirectoryPicker(
      [](const std::string&, const std::string& default_path) {
        return std::optional<std::string>{default_path + "/picked"};
      });
  const std::optional<std::string> picked =
      NativePathDialog::pickDirectory("Select Project", "C:/root");
  expect(picked.has_value(), "injected directory returns value");
  expect(picked == std::optional<std::string>{"C:/root/picked"},
         "injected directory uses default_path");
}

void TestInjectedFilePickerCancel() {
  ScopedPickerReset reset;
  NativePathDialog::setFilePicker(
      [](const std::string&, const std::string&) {
        return std::optional<std::string>{};
      });
  std::string target = "keep-me";
  const std::optional<std::string> picked =
      NativePathDialog::pickOpenFile("Select File", target);
  expect(!picked.has_value(), "cancel yields nullopt");
  expect(!NativePathDialog::applyPickedPath(target, picked),
         "cancel does not apply");
  expect(target == "keep-me", "path kept after cancel");
}

void TestBrowseFlowWritesProjectPathBuffer() {
  ScopedPickerReset reset;
  NativePathDialog::setDirectoryPicker(
      [](const std::string&, const std::string&) {
        return std::optional<std::string>{"D:/workspace/alpha"};
      });
  std::string path_buffer;
  const std::optional<std::string> picked =
      NativePathDialog::pickDirectory("Select Project", path_buffer);
  expect(NativePathDialog::applyPickedPath(path_buffer, picked),
         "browse writes project path buffer");
  expect(path_buffer == "D:/workspace/alpha", "project path buffer matches");
}

void TestResolveDefaultPathPriority() {
  constexpr const char* kLastBrowse = "D:/last-browse";
  expect(NativePathDialog::resolveDefaultPath("C:/projects/demo", kLastBrowse) ==
             "C:/projects/demo",
         "directory context wins over last browse");
  expect(NativePathDialog::resolveDefaultPath("", kLastBrowse) == kLastBrowse,
         "empty context falls back to last browse");
  expect(NativePathDialog::resolveDefaultPath("", "").empty(),
         "no context and no last browse yields empty for OS default");

  const std::filesystem::path temp_root =
      std::filesystem::temp_directory_path() / "taskgraph-browse-default";
  std::error_code error;
  std::filesystem::remove_all(temp_root, error);
  std::filesystem::create_directories(temp_root, error);
  expect(!error, "create temp browse dir");
  const std::filesystem::path file_path = temp_root / "a.png";
  {
    std::ofstream output(file_path, std::ios::binary);
    output << 'x';
    expect(static_cast<bool>(output), "write temp property file");
  }
  const std::string resolved = NativePathDialog::resolveDefaultPath(
      file_path.generic_string(), kLastBrowse);
  expect(resolved == temp_root.generic_string(),
         "file context uses parent directory");
  std::filesystem::remove_all(temp_root, error);
}

void TestDirectoryOfPickedPath() {
  expect(NativePathDialog::directoryOfPickedPath("D:/workspace/alpha") ==
             "D:/workspace/alpha",
         "directory pick remembered as itself");
  expect(NativePathDialog::directoryOfPickedPath("").empty(),
         "empty pick yields empty directory");

  const std::filesystem::path temp_root =
      std::filesystem::temp_directory_path() / "taskgraph-browse-picked";
  std::error_code error;
  std::filesystem::remove_all(temp_root, error);
  std::filesystem::create_directories(temp_root, error);
  expect(!error, "create temp picked dir");
  const std::filesystem::path file_path = temp_root / "a.png";
  {
    std::ofstream output(file_path, std::ios::binary);
    output << 'x';
    expect(static_cast<bool>(output), "write temp picked file");
  }
  expect(NativePathDialog::directoryOfPickedPath(file_path.generic_string()) ==
             temp_root.generic_string(),
         "file pick remembered as parent");
  std::filesystem::remove_all(temp_root, error);
}

void TestBrowseUsesResolvedDefaultPath() {
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
  expect(seen_default == "E:/recent-browse",
         "picker receives resolved last browse dir");
  expect(picked == std::optional<std::string>{"E:/recent-browse/picked"},
         "picker result uses resolved default");
}

}  // namespace

int main() {
  TestApplyPickedPathWritesAndRejectsEmpty();
  TestInjectedDirectoryPicker();
  TestInjectedFilePickerCancel();
  TestBrowseFlowWritesProjectPathBuffer();
  TestResolveDefaultPathPriority();
  TestDirectoryOfPickedPath();
  TestBrowseUsesResolvedDefaultPath();

  if (g_failures != 0) {
    std::cerr << g_failures << " failure(s)\n";
    return 1;
  }
  return 0;
}
