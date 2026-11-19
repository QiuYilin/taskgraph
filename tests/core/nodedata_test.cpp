// core 测试：节点数据类型。
// 同一 id 匹配，name 不参与匹配，空 id 不匹配，针脚形与显式色由模板指定。

#include "taskgraph/NodeData.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

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

TEST(NodeData, SameIdMatches) {
  const IntData first;
  const IntData second;
  const TextData text;

  EXPECT_TRUE(first.sameType(second)) << "同一 id 的 NodeData 应匹配";
  EXPECT_EQ(first.type().id, kIntDataId) << "类型 id 应等于模板参数";
  EXPECT_EQ(first.type().name, kIntDataName) << "类型 name 应等于模板参数";
  EXPECT_EQ(std::string(IntData::displayName()), kIntDataName)
      << "静态名称应等于模板参数";
  EXPECT_FALSE(first.sameType(text)) << "不同 id 的 NodeData 不应匹配";
  EXPECT_EQ(text.type().id, kTextDataId) << "另一类型的 id 应等于自己的模板参数";
}

TEST(NodeData, NameDoesNotMatch) {
  const FixedTypeData shared_name{"shared-id", "shared-name"};
  const FixedTypeData same_id{"shared-id", "other-name"};
  const FixedTypeData other_id{"other-id", "shared-name"};

  EXPECT_TRUE(shared_name.sameType(same_id)) << "id 相同、name 不同时应匹配";
  EXPECT_FALSE(shared_name.sameType(other_id)) << "name 相同、id 不同时不应匹配";
}

TEST(NodeData, EmptyIdDoesNotMatch) {
  const FixedTypeData empty_left{"", "empty"};
  const FixedTypeData empty_right{"", "other-empty"};
  EXPECT_FALSE(empty_left.sameType(empty_right)) << "空 id 不应互相匹配";
}

TEST(NodeData, ShapeAndColorComeFromTemplate) {
  const IntData plain;
  const SquareTinted tinted;

  EXPECT_EQ(plain.type().shape, tg::PinShape::Circle) << "默认针脚形应为圆";
  EXPECT_FALSE(plain.type().has_color) << "默认不应带显式色";
  EXPECT_EQ(tinted.type().shape, tg::PinShape::Square) << "模板可指定方针脚";
  EXPECT_TRUE(tinted.type().has_color) << "模板可指定显式色";
  EXPECT_EQ(tinted.type().color.r, kDemoColor.r) << "显式色 r 应透传";
}
