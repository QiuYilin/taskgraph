#include "NodeData.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    ++g_failures;
  }
}

inline constexpr char kIntDataId[] = "IntData";
inline constexpr char kIntDataName[] = "Int";
inline constexpr char kTextDataId[] = "TextData";
inline constexpr char kTextDataName[] = "Text";

using IntData = tg::NodeData_<int, kIntDataId, kIntDataName>;
using TextData = tg::NodeData_<std::string, kTextDataId, kTextDataName>;

class FixedTypeData : public tg::NodeData {
 public:
  FixedTypeData(std::string id, std::string name)
      : _type{std::move(id), std::move(name)} {}

  tg::NodeDataType type() const override { return _type; }

 private:
  tg::NodeDataType _type;
};

}  // namespace

int main() {
  const IntData first;
  const IntData second;
  const TextData text;

  expect(first.sameType(second), "同一 id 的 NodeData 应匹配");
  expect(first.type().id == kIntDataId, "类型 id 应等于模板参数");
  expect(first.type().name == kIntDataName, "类型 name 应等于模板参数");
  expect(!first.sameType(text), "不同 id 的 NodeData 不应匹配");
  expect(text.type().id == kTextDataId, "另一类型的 id 应等于自己的模板参数");

  const FixedTypeData shared_name{"shared-id", "shared-name"};
  const FixedTypeData same_id{"shared-id", "other-name"};
  const FixedTypeData other_id{"other-id", "shared-name"};
  expect(shared_name.sameType(same_id), "id 相同、name 不同时应匹配");
  expect(!shared_name.sameType(other_id), "name 相同、id 不同时不应匹配");

  return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
