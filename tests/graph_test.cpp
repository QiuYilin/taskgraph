#include "Graph.hpp"
#include "NodeData.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    ++g_failures;
  }
}

class PortNode : public tg::Node {
 public:
  PortNode(std::string data_id = "test", std::string data_name = "Test")
      : _data_id(std::move(data_id)), _data_name(std::move(data_name)) {}

  unsigned int nFixedPorts(tg::PortType port_type) const override {
    constexpr unsigned int kPortCount = 2;
    switch (port_type) {
      case tg::PortType::In:
      case tg::PortType::Out:
        return kPortCount;
    }
    return 0;
  }

  tg::NodeDataType fixedDataType(tg::PortType, unsigned int) const override {
    return tg::NodeDataType{_data_id, _data_name};
  }

  const char* fixedPortId(tg::PortType, unsigned int index) const override {
    return index == 0 ? "a" : "b";
  }

  const char* fixedPortCaption(tg::PortType port_type,
                               unsigned int index) const override {
    return fixedPortId(port_type, index);
  }

  void onSetInPortData(unsigned int index, std::shared_ptr<tg::NodeData>) override {
    received_ports.push_back(index);
  }

  std::shared_ptr<tg::NodeData> getOutPortData(unsigned int) override {
    return nullptr;
  }

  std::vector<unsigned int> received_ports;

 private:
  std::string _data_id;
  std::string _data_name;
};

// 与 examples 的 IntData type id 一致，供 tryAsPropertyValue 同步属性。
constexpr char kTestIntId[] = "IntData";
constexpr char kTestIntName[] = "Int";
using TestIntData = tg::NodeData_<int, kTestIntId, kTestIntName>;

class IntSinkNode : public tg::Node {
 public:
  unsigned int nFixedPorts(tg::PortType port_type) const override {
    return port_type == tg::PortType::In ? 1 : 0;
  }

  tg::NodeDataType fixedDataType(tg::PortType port_type,
                                 unsigned int) const override {
    if (port_type == tg::PortType::In) {
      return TestIntData{}.type();
    }
    return {};
  }

  const char* fixedPortId(tg::PortType port_type,
                          unsigned int) const override {
    return port_type == tg::PortType::In ? "value" : "";
  }

  const char* fixedPortCaption(tg::PortType port_type,
                               unsigned int index) const override {
    return fixedPortId(port_type, index);
  }

  void onSetInPortData(unsigned int, std::shared_ptr<tg::NodeData>) override {}

  std::shared_ptr<tg::NodeData> getOutPortData(unsigned int) override {
    return nullptr;
  }
};

}  // namespace

int main() {
  tg::Graph graph;
  auto upstream = std::make_shared<PortNode>();
  auto downstream = std::make_shared<PortNode>();
  const std::vector<std::shared_ptr<PortNode>> added{upstream, downstream};
  for (const auto& node : added) {
    graph.addNode(node);
  }
  expect(graph.getNodes().size() == added.size(), "addNode 后节点数量不对");
  expect(graph.getNodes().front() == upstream, "第一个节点应为上游");
  expect(graph.getNodes().back() == downstream, "第二个节点应为下游");

  std::size_t connected_edges = 0;
  if (graph.connect(upstream, 0, downstream, 1)) {
    ++connected_edges;
  }
  expect(connected_edges == 1, "首次 connect 应成功");
  expect(!graph.connect(upstream, 0, downstream, 1), "重复边应被拒绝");
  expect(!graph.connect(upstream, 0, upstream, 0), "自环应被拒绝");
  expect(!graph.connect(nullptr, 0, downstream, 1), "空上游应被拒绝");
  expect(!graph.connect(upstream, 0, nullptr, 1), "空下游应被拒绝");
  const std::size_t edges_before_second = connected_edges;
  if (graph.connect(upstream, 0, downstream, 0)) {
    ++connected_edges;
  }
  expect(connected_edges == edges_before_second + 1, "不同输入端口应能再连一条");

  auto other = std::make_shared<PortNode>();
  graph.addNode(other);
  expect(graph.connect(upstream->outputPort(1), other->inputPort(0)),
         "Port* connect 应成功");
  expect(!graph.connect(upstream->outputPort(1), other->inputPort(0)),
         "Port* 重复边应被拒绝");
  expect(!graph.connect(static_cast<const tg::Node::Port*>(nullptr),
                        other->inputPort(0)),
         "空 Port* 应被拒绝");

  expect(other->isInputShownAsPort(0), "默认输入应为端口露面");
  expect(graph.setInputPresentation(other, 0, tg::InputPresentation::Property),
         "有输入口的节点应能改为属性");
  expect(other->isInputShownAsProperty(0), "切换后应为属性露面");
  expect(other->getEditableProperties().size() == 2,
         "两个输入槽都应出现在属性表里");
  expect(!graph.connect(upstream->outputPort(1), other->inputPort(0)),
         "属性模式输入应拒绝连边");
  expect(graph.setInputPresentation(other, 0, tg::InputPresentation::Port),
         "应能改回端口露面");
  expect(graph.connect(upstream->outputPort(1), other->inputPort(0)),
         "改回端口后应能再连");
  // 改为属性时 setInPortData(nullptr) 会记一次，不影响后面的推送计数。
  other->received_ports.clear();

  auto presentation_probe = std::make_shared<PortNode>();
  graph.addNode(presentation_probe);
  expect(graph.connect(upstream, 1, presentation_probe, 1),
         "切换前连到输入 1 应成功");
  expect(graph.setInputPresentation(presentation_probe, 1,
                                    tg::InputPresentation::Property),
         "改为属性应成功并断边");
  expect(presentation_probe->outgoing().empty(),
         "下游改属性不断自己的出边");
  expect(std::none_of(upstream->outgoing().begin(), upstream->outgoing().end(),
                      [&](const tg::ConnectionInfo& conn) {
                        return conn.adj == presentation_probe &&
                               conn.port_in == 1;
                      }),
         "改为属性后上游入边应清除");
  presentation_probe->received_ports.clear();

  upstream->computeAndUpdateData();
  expect(downstream->received_ports.size() == connected_edges,
         "原下游收到的次数应等于按下标成功连边数");
  expect(other->received_ports.size() == 1, "Port* 下游应收到一次");
  expect(downstream->received_ports.front() == 1, "第一条边应送到输入端口 1");
  expect(downstream->received_ports.back() == 0, "第二条边应送到输入端口 0");

  graph.removeNode(downstream);
  expect(graph.getNodes().size() == 3,
         "删除原下游后应剩上游、other 与 presentation_probe");

  const std::size_t downstream_before = downstream->received_ports.size();
  upstream->computeAndUpdateData();
  expect(downstream->received_ports.size() == downstream_before,
         "删除下游后上游不应再向它送数据");
  expect(other->received_ports.size() == 2, "删边后 other 仍应再收到一次");

  graph.removeNode(std::make_shared<PortNode>());
  graph.removeNode(nullptr);
  graph.removeNode(downstream);
  expect(graph.getNodes().size() == 3, "删除不在图中的节点不应改变图");

  graph.removeNode(presentation_probe);
  graph.removeNode(other);
  graph.removeNode(upstream);
  expect(graph.getNodes().empty(), "删除全部节点后图应为空");

  auto int_sink = std::make_shared<IntSinkNode>();
  constexpr int kSyncedIntValue = 42;
  int_sink->setInPortData(0, std::make_shared<TestIntData>(kSyncedIntValue));
  auto synced = int_sink->inputProperty(0);
  expect(synced != nullptr, "整数输入应有绑定属性");
  expect(synced != nullptr && std::holds_alternative<int>(synced->value) &&
             std::get<int>(synced->value) == kSyncedIntValue,
         "setInPortData 应把 int 同步进绑定属性");
  int_sink->setInPortData(0, nullptr);
  expect(synced != nullptr && std::holds_alternative<int>(synced->value) &&
             std::get<int>(synced->value) == 0,
         "清空入端口应把绑定属性恢复为默认 int 0");

  // 数据 id 表示底层身份；同一数据可以有不同的端口解释名。
  tg::Graph typed_graph;
  auto typed_source = std::make_shared<PortNode>("Vec3f", "Point");
  auto same_data = std::make_shared<PortNode>("Vec3f", "Vector");
  auto other_data = std::make_shared<PortNode>("Mat4", "Point");
  auto empty_data = std::make_shared<PortNode>("", "");
  for (const auto& node : {typed_source, same_data, other_data, empty_data}) {
    typed_graph.addNode(node);
  }
  expect(typed_graph.canConnect(typed_source->outputPort(0),
                                same_data->inputPort(0)),
         "同数据 id、不同 name 应可连接");
  expect(typed_graph.connect(typed_source->outputPort(0),
                             same_data->inputPort(0)),
         "同数据 id、不同 name 应能落边");
  expect(!typed_graph.canConnect(typed_source->outputPort(0),
                                 other_data->inputPort(0)),
         "不同数据 id 不应相容");
  expect(!typed_graph.connect(typed_source, 0, other_data, 0),
         "不同数据 id 不应落边");
  expect(!typed_graph.connect(typed_source, 0, empty_data, 0),
         "空数据 id 不应落边");
  expect(!typed_graph.canConnect(typed_source->outputPort(0),
                                 same_data->inputPort(0)),
         "已连接的同端口对不应再通过预览");
  expect(!typed_graph.connect(typed_source, 99, same_data, 0),
         "越界输出端口不应落边");

  return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
