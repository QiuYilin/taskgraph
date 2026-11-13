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

inline constexpr tg::PortTypeColor kDemoColor{0.2f, 0.4f, 0.6f, 1.0f};

using IntData = tg::NodeData_<int, kIntDataId, kIntDataName>;
using TextData = tg::NodeData_<std::string, kTextDataId, kTextDataName>;
using SquareTinted =
    tg::NodeData_<int, kIntDataId, kIntDataName, tg::PinShape::Square,
                  &kDemoColor>;

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
  expect(first.type().shape == tg::PinShape::Circle, "默认针脚形应为圆");
  expect(!first.type().has_color, "默认不应带显式色");
  expect(std::string(IntData::displayName()) == kIntDataName,
         "静态名称应等于模板参数");
  expect(!first.sameType(text), "不同 id 的 NodeData 不应匹配");
  expect(text.type().id == kTextDataId, "另一类型的 id 应等于自己的模板参数");

  const SquareTinted tinted;
  expect(tinted.type().shape == tg::PinShape::Square, "模板可指定方针脚");
  expect(tinted.type().has_color, "模板可指定显式色");
  expect(tinted.type().color.r == kDemoColor.r, "显式色 r 应透传");

  const FixedTypeData shared_name{"shared-id", "shared-name"};
  const FixedTypeData same_id{"shared-id", "other-name"};
  const FixedTypeData other_id{"other-id", "shared-name"};
  const FixedTypeData empty_left{"", "empty"};
  const FixedTypeData empty_right{"", "other-empty"};
  expect(shared_name.sameType(same_id), "id 相同、name 不同时应匹配");
  expect(!shared_name.sameType(other_id), "name 相同、id 不同时不应匹配");
  expect(!empty_left.sameType(empty_right), "空 id 不应互相匹配");

  return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
