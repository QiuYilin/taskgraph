// core 测试：图的节点与连线。每个操作自己建图。
//
// 增加节点
// - 空图加入一个节点后，数量与加入的节点一致，且就是该节点。
// - 再加入第二个，先加入的在前。
// - 加入空指针，图不变。
// - 节点已经在某张图里时，再加入同一张图或另一张图，图不变。
// - renameNode 拒绝空名和空节点。
// - createNode 创建节点并加入图，返回的节点能连边。
//
// 删除节点，没有连线
// - 删掉无边节点，数量减一，其余节点还在。
// - 删不在图中的节点，图不变。
// - 删空指针，图不变。
// - 重复删除同一个已删节点，图不变。
// - 全部删光后图为空。
//
// 删除节点，已经连线
// - 删掉只有出边的节点：下游入边不再指向它，相关调度边消失，
//   再 computeAndUpdateData 不会送到已删节点。
// - 删掉只有入边的节点：上游出边不再指向它，相关调度边消失。
// - 删掉既有入边又有出边的中间节点：上下游指向它的边都清除；
//   上下游之间没有直连时，调度边为 0。
// - 删掉其中一个已连线节点后，其余节点和其余边还在。
//
// 增加连线
// - 首次 connect 成功，两端都有 ConnectionInfo，调度边为 1。
// - 同一对端口再连一次，拒绝。
// - Port* 连接成功；同一对再连拒绝；空 Port* 拒绝。
// - connect<DataT> 成功并写入数据边；同一对再连、以及输入改成属性后，由 canConnect 拒绝。
// - 空 Port* 拒绝。
// - 越界下标取不到端口（nullptr），connect 拒绝。
// - 同一数据 id、不同 name 可以连；不同 id、空 id 拒绝。
// - 同一输出连到另一个输入口可以。
// - 自环拒绝。
// - 两个源汇入同一节点的两个输入不是环，两条都成功，调度边为 2。
// - 同一个输出连到两个不同节点，两条都成功，调度边数等于成功条数。
// - 菱形 A→B、A→C、B→D、C→D 不是环，四条都成功。
// - 属性露面的输入拒绝连线；改回端口后可以再连。
// - 第二个上游接到同一个输入口，connect 返回 false，第一条边还在。
// - 两节点连上后再连回边，拒绝。
// - 三节点 A→B→C 后再连 C→A，拒绝。
// - 四节点 A→B→C→D 后再连 D→A，拒绝。
// - 同一对节点的两个不同端口各连一条：两条数据边，调度边仍为 1。
//
// 删除连线
// - 断开指定的一条端口边后，只有这一对端口的 ConnectionInfo 消失，另一对还在。
// - 删掉某个输入口的入边后，该口两端的 ConnectionInfo 消失，其它口的边还在。
// - 改成属性等于删掉该口入边并清空该口数据；该节点自己的出边还在。
// - 删掉入边后，上游 computeAndUpdateData 不再把数据送到该口。
// - 同一对节点有两条数据边时，disconnectInputsTo 只删其中一条，调度边仍为 1。
// - disconnect 只删其中一条端口边时，另一条数据边还在，调度边仍为 1。
// - 删掉这对节点的最后一条数据边后，调度边为 0。disconnect 与 disconnectInputsTo 都一样。
// - 删掉后再连上同一口，调度边仍为 1。disconnect 后再 connect 也一样。
// - 扇出到两个节点后，disconnect 掉其中一条，剩下那条的数据边还在，调度边总数为 1。
// - 唯一一条入边改成属性后，调度边为 0。
//
// 一次 run
// - setInPortData 把 int 同步进绑定属性；清空后属性为 0。
// - 两个整数源连到加法节点的两个输入，Controller::run 之后和为两数之和。
//   手写侧用 Out::nodeData() 取出同一份和，不再按下标转型。
//
// 手写字面量
// - setProperty<TestIntData>(In, 值) 断开该口入边和调度边，露面改为属性并写入；别人的 In 返回 false；再 connect 被拒绝。
// - 图销毁后回指清空，setProperty 仍写入字面量。
// - Prop 声明即出现在 getProperties；setProperty 后 get() 与属性表那份值一致。别人的 Prop 返回 false。

#include "taskgraph/Controller.hpp"
#include "taskgraph/Error.hpp"
#include "taskgraph/Graph.hpp"
#include "taskgraph/Log.hpp"
#include "taskgraph/NodeData.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <list>
#include <thread>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

constexpr unsigned int kFixedPortCount = 2;
constexpr unsigned int kOutOfRangePortIndex = kFixedPortCount;
constexpr unsigned int kFirstPort = 0;
constexpr unsigned int kSecondPort = 1;

constexpr char kTestIntId[] = "IntData";
constexpr char kTestIntName[] = "Int";
using TestIntData = tg::NodeData_<int, kTestIntId, kTestIntName>;

constexpr int kSyncedIntValue = 42;
constexpr int kLeftInt = 2;
constexpr int kRightInt = 3;
constexpr const char* kFilePathName = "file_path";
constexpr const char* kScenePath = "scene.png";

// taskflow 普通任务的调度边在 dump 里写成 "p... -> p...;"。
const std::string kScheduleEdgeMark = " -> ";

class PortNode : public tg::Node {
 public:
  PortNode(std::string data_id = "test", std::string data_name = "Test")
      : _data_id(std::move(data_id)), _data_name(std::move(data_name)) {}

  unsigned int nFixedPorts(tg::PortType port_type) const override {
    switch (port_type) {
      case tg::PortType::In:
      case tg::PortType::Out:
        return kFixedPortCount;
    }
    return 0;
  }

  tg::NodeDataType fixedDataType(tg::PortType, unsigned int) const override {
    return tg::NodeDataType{_data_id, _data_name};
  }

  const char* fixedPortId(tg::PortType, unsigned int index) const override {
    return index == kFirstPort ? "a" : "b";
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

class IntSinkNode : public tg::Node {
 public:
  unsigned int nFixedPorts(tg::PortType port_type) const override {
    constexpr unsigned int kInputCount = 1;
    return port_type == tg::PortType::In ? kInputCount : 0;
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

// 整数源和加法只放在本测试里，不放进示例。
class IntSourceNode : public tg::Node {
 public:
  explicit IntSourceNode(int value) { _value = addProperty("value", value); }

  unsigned int nFixedPorts(tg::PortType port_type) const override {
    constexpr unsigned int kOutputCount = 1;
    return port_type == tg::PortType::Out ? kOutputCount : 0;
  }

  tg::NodeDataType fixedDataType(tg::PortType port_type,
                                 unsigned int) const override {
    if (port_type == tg::PortType::Out) {
      return TestIntData{}.type();
    }
    return {};
  }

  const char* fixedPortId(tg::PortType port_type,
                          unsigned int) const override {
    return port_type == tg::PortType::Out ? "value" : "";
  }

  const char* fixedPortCaption(tg::PortType port_type,
                               unsigned int index) const override {
    return fixedPortId(port_type, index);
  }

  void compute() override {
    const int value = std::get<int>(_value->value);
    _out = std::make_shared<TestIntData>(value);
  }

  void onSetInPortData(unsigned int, std::shared_ptr<tg::NodeData>) override {}

  std::shared_ptr<tg::NodeData> getOutPortData(unsigned int) override {
    return _out;
  }

 private:
  std::shared_ptr<Property> _value;
  std::shared_ptr<tg::NodeData> _out;
};

class IntAddNode : public tg::Node {
 public:
  unsigned int nFixedPorts(tg::PortType port_type) const override {
    constexpr unsigned int kInputCount = 2;
    constexpr unsigned int kOutputCount = 1;
    switch (port_type) {
      case tg::PortType::In:
        return kInputCount;
      case tg::PortType::Out:
        return kOutputCount;
    }
    return 0;
  }

  tg::NodeDataType fixedDataType(tg::PortType, unsigned int) const override {
    return TestIntData{}.type();
  }

  const char* fixedPortId(tg::PortType port_type,
                          unsigned int index) const override {
    if (port_type == tg::PortType::In && index == kFirstPort) {
      return "lhs";
    }
    if (port_type == tg::PortType::In && index == kSecondPort) {
      return "rhs";
    }
    if (port_type == tg::PortType::Out && index == kFirstPort) {
      return "sum";
    }
    return "";
  }

  const char* fixedPortCaption(tg::PortType port_type,
                               unsigned int index) const override {
    return fixedPortId(port_type, index);
  }

  void compute() override {
    const int lhs = readInt(_lhs);
    const int rhs = readInt(_rhs);
    sum = lhs + rhs;
    _out = std::make_shared<TestIntData>(sum);
  }

  void onSetInPortData(unsigned int index,
                       std::shared_ptr<tg::NodeData> data) override {
    if (index == kFirstPort) {
      _lhs = data;
    } else if (index == kSecondPort) {
      _rhs = data;
    }
  }

  std::shared_ptr<tg::NodeData> getOutPortData(unsigned int) override {
    return _out;
  }

  int sum = 0;

 private:
  static int readInt(const std::weak_ptr<tg::NodeData>& weak) {
    auto data = weak.lock();
    auto typed = std::dynamic_pointer_cast<TestIntData>(data);
    if (typed == nullptr) {
      throw tg::NodeError("add input has no int");
    }
    return typed->data();
  }

  std::weak_ptr<tg::NodeData> _lhs;
  std::weak_ptr<tg::NodeData> _rhs;
  std::shared_ptr<tg::NodeData> _out;
};

class PathNode : public tg::Node {
 public:
  PathNode() : file_path(this, kFilePathName, std::string{}) {}

  unsigned int nFixedPorts(tg::PortType) const override { return 0; }

  void onSetInPortData(unsigned int, std::shared_ptr<tg::NodeData>) override {}

  std::shared_ptr<tg::NodeData> getOutPortData(unsigned int) override {
    return nullptr;
  }

  tg::Prop<std::string> file_path;
};

// 一 run 就抛 NodeError 的源节点，用来验证异常从 taskflow 传到调用方。
class ThrowingNode : public tg::Node {
 public:
  explicit ThrowingNode(std::string message) : _message(std::move(message)) {}

  unsigned int nFixedPorts(tg::PortType port_type) const override {
    constexpr unsigned int kOutputCount = 1;
    return port_type == tg::PortType::Out ? kOutputCount : 0;
  }

  tg::NodeDataType fixedDataType(tg::PortType port_type,
                                 unsigned int) const override {
    if (port_type == tg::PortType::Out) {
      return TestIntData{}.type();
    }
    return {};
  }

  const char* fixedPortId(tg::PortType port_type,
                          unsigned int) const override {
    return port_type == tg::PortType::Out ? "out" : "";
  }

  const char* fixedPortCaption(tg::PortType port_type,
                               unsigned int index) const override {
    return fixedPortId(port_type, index);
  }

  void compute() override { throw tg::NodeError(_message); }

  void onSetInPortData(unsigned int, std::shared_ptr<tg::NodeData>) override {}

  std::shared_ptr<tg::NodeData> getOutPortData(unsigned int) override {
    return nullptr;
  }

 private:
  std::string _message;
};

// 只记录 compute 是否被调用的下游，用来验证失败节点的下游被取消。
class ComputeRecorderNode : public tg::Node {
 public:
  unsigned int nFixedPorts(tg::PortType port_type) const override {
    constexpr unsigned int kInputCount = 1;
    return port_type == tg::PortType::In ? kInputCount : 0;
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
    return port_type == tg::PortType::In ? "in" : "";
  }

  const char* fixedPortCaption(tg::PortType port_type,
                               unsigned int index) const override {
    return fixedPortId(port_type, index);
  }

  void compute() override { computed = true; }

  void onSetInPortData(unsigned int, std::shared_ptr<tg::NodeData>) override {}

  std::shared_ptr<tg::NodeData> getOutPortData(unsigned int) override {
    return nullptr;
  }

  bool computed = false;
};

std::size_t scheduleEdgeCount(tg::Graph& graph) {
  const std::string dumped = graph.dump();
  std::size_t count = 0;
  std::size_t pos = 0;
  while ((pos = dumped.find(kScheduleEdgeMark, pos)) != std::string::npos) {
    ++count;
    pos += kScheduleEdgeMark.size();
  }
  return count;
}

bool hasConnection(const std::list<tg::ConnectionInfo>& edges,
                   const std::shared_ptr<tg::Node>& other,
                   unsigned int port_out, unsigned int port_in) {
  return std::any_of(edges.begin(), edges.end(),
                     [&](const tg::ConnectionInfo& conn) {
                       return conn.adj == other && conn.port_out == port_out &&
                              conn.port_in == port_in;
                     });
}

bool graphHas(const tg::Graph& graph, const std::shared_ptr<tg::Node>& node) {
  const auto& nodes = graph.getNodes();
  return std::find(nodes.begin(), nodes.end(), node) != nodes.end();
}

bool sameNodes(const tg::Graph& graph,
               const std::vector<std::shared_ptr<tg::Node>>& expected) {
  const auto& nodes = graph.getNodes();
  return nodes.size() == expected.size() &&
         std::equal(nodes.begin(), nodes.end(), expected.begin());
}

bool propertyIsInt(const std::shared_ptr<tg::Node::Property>& property,
                   int value) {
  return property != nullptr && std::holds_alternative<int>(property->value) &&
         std::get<int>(property->value) == value;
}

bool receivedPort(const PortNode& node, unsigned int index) {
  return std::find(node.received_ports.begin(), node.received_ports.end(),
                   index) != node.received_ports.end();
}

// 记录 debugSummary 调用次数，用来验证关掉 Data 后不取摘要。
class CountingIntData : public TestIntData {
 public:
  CountingIntData(int value, std::shared_ptr<std::atomic<int>> calls)
      : TestIntData(value), _calls(std::move(calls)) {}

  std::string debugSummary() const override {
    ++*_calls;
    return "counted";
  }

 private:
  std::shared_ptr<std::atomic<int>> _calls;
};

class CountingSourceNode : public tg::Node {
 public:
  explicit CountingSourceNode(std::shared_ptr<std::atomic<int>> calls)
      : _calls(std::move(calls)) {}

  unsigned int nFixedPorts(tg::PortType port_type) const override {
    constexpr unsigned int kOutputCount = 1;
    return port_type == tg::PortType::Out ? kOutputCount : 0;
  }

  tg::NodeDataType fixedDataType(tg::PortType port_type,
                                 unsigned int) const override {
    if (port_type == tg::PortType::Out) {
      return TestIntData{}.type();
    }
    return {};
  }

  const char* fixedPortId(tg::PortType port_type,
                          unsigned int) const override {
    return port_type == tg::PortType::Out ? "value" : "";
  }

  const char* fixedPortCaption(tg::PortType port_type,
                               unsigned int index) const override {
    return fixedPortId(port_type, index);
  }

  void compute() override {
    _out = std::make_shared<CountingIntData>(kSyncedIntValue, _calls);
  }

  void onSetInPortData(unsigned int, std::shared_ptr<tg::NodeData>) override {}

  std::shared_ptr<tg::NodeData> getOutPortData(unsigned int) override {
    return _out;
  }

 private:
  std::shared_ptr<std::atomic<int>> _calls;
  std::shared_ptr<tg::NodeData> _out;
};

struct GraphLogConfigGuard {
  tg::LogConfig original = tg::GetLogConfig();
  ~GraphLogConfigGuard() { tg::SetLogConfig(original); }
};

std::vector<tg::LogRecord> CollectGraphLogs() {
  std::vector<tg::LogRecord> records;
  tg::copyLogRecordsSince(0, records);
  return records;
}

bool HasLogContaining(const std::vector<tg::LogRecord>& records,
                      const std::string& needle) {
  return std::any_of(records.begin(), records.end(),
                     [&](const tg::LogRecord& record) {
                       return record.text.find(needle) != std::string::npos;
                     });
}

TEST(GraphLog, AutoLogsNodePropertiesInputsOutputsAndTiming) {
  GraphLogConfigGuard guard;
  tg::SetLogConfig({tg::LogLevel::Info, tg::kAllLogCategories});
  tg::clearLogRecords();

  tg::Graph graph;
  graph.setName("LogGraph");
  auto source = std::make_shared<IntSourceNode>(kLeftInt);
  source->setTypeId("IntSource");
  source->setName("Source");
  auto add = std::make_shared<IntAddNode>();
  add->setTypeId("IntAdd");
  add->setName("Add");
  graph.addNode(source);
  graph.addNode(add);
  ASSERT_TRUE(graph.connect(source->outputPort(kFirstPort),
                            add->inputPort(kFirstPort)));
  ASSERT_TRUE(graph.connect(source->outputPort(kFirstPort),
                            add->inputPort(kSecondPort)));

  tg::Controller controller;
  controller.run(graph);
  ASSERT_TRUE(controller.lastRunMilliseconds().has_value())
      << "总耗时始终记录，不依赖 Timing 日志";
  EXPECT_GE(*controller.lastRunMilliseconds(), 0.0);

  const std::vector<tg::LogRecord> records = CollectGraphLogs();
  EXPECT_TRUE(HasLogContaining(records,
                               "graph 'LogGraph' node 'Source': start "
                               "(IntSource)"))
      << "节点 start 带图名/节点名/type";
  EXPECT_TRUE(HasLogContaining(records, "property 'value'=2"))
      << "属性摘要";
  EXPECT_TRUE(HasLogContaining(records, "in[0] 'lhs' <- Int"))
      << "输入摘要带端口和上游摘要";
  const auto text_index = [&](const std::string& needle) {
    return std::find_if(records.begin(), records.end(),
                        [&](const tg::LogRecord& record) {
                          return record.text.find(needle) != std::string::npos;
                        }) -
           records.begin();
  };
  EXPECT_LT(text_index("in[0]"), text_index("in[1]"))
      << "输入按端口下标记录";
  EXPECT_TRUE(HasLogContaining(records, "out[0] 'value' = Int"))
      << "输出摘要";
  EXPECT_TRUE(HasLogContaining(records, "took ")) << "单节点耗时";
  EXPECT_TRUE(HasLogContaining(records,
                               "graph 'LogGraph' node 'Add': end"))
      << "节点 end";
}

TEST(GraphLog, DisabledDataSkipsSummary) {
  GraphLogConfigGuard guard;
  tg::SetLogConfig({tg::LogLevel::Info,
                    static_cast<tg::LogCategoryMask>(
                        tg::LogCategory::Node) |
                        static_cast<tg::LogCategoryMask>(
                            tg::LogCategory::Timing)});
  tg::clearLogRecords();

  auto calls = std::make_shared<std::atomic<int>>(0);
  tg::Graph graph;
  graph.setName("NoDataGraph");
  auto source = std::make_shared<CountingSourceNode>(calls);
  source->setTypeId("CountingSource");
  source->setName("Count");
  graph.addNode(source);

  tg::Controller controller;
  controller.run(graph);

  const std::vector<tg::LogRecord> records = CollectGraphLogs();
  EXPECT_FALSE(HasLogContaining(records, "counted"))
      << "关 Data 时不应调用 debugSummary";
  EXPECT_EQ(calls->load(), 0);
  EXPECT_TRUE(HasLogContaining(records, "node 'Count': start (CountingSource)"))
      << "Node 类别仍记录";
  EXPECT_TRUE(HasLogContaining(records, "took ")) << "Timing 类别仍记录";
}

TEST(GraphLog, TimingOnlyPresetKeepsTimingRecords) {
  GraphLogConfigGuard guard;
  tg::SetLogConfig(
      {tg::LogLevel::Info,
       static_cast<tg::LogCategoryMask>(tg::LogCategory::Timing)});
  tg::clearLogRecords();

  tg::Graph graph;
  graph.setName("TimingGraph");
  auto source = std::make_shared<IntSourceNode>(kLeftInt);
  source->setTypeId("IntSource");
  source->setName("Timed");
  graph.addNode(source);

  tg::Controller controller;
  controller.run(graph);

  const std::vector<tg::LogRecord> records = CollectGraphLogs();
  ASSERT_FALSE(records.empty());
  for (const tg::LogRecord& record : records) {
    EXPECT_EQ(record.category, tg::LogCategory::Timing)
        << "Timing 预设只保留 Timing 记录";
  }
  EXPECT_TRUE(HasLogContaining(records, "node 'Timed': took "));
  EXPECT_TRUE(HasLogContaining(records, "graph 'TimingGraph': took "));
}

TEST(GraphLog, NodeFailureLogsErrorAndRethrows) {
  GraphLogConfigGuard guard;
  tg::SetLogConfig({tg::LogLevel::Error, tg::kAllLogCategories});
  tg::clearLogRecords();

  tg::Graph graph;
  graph.setName("ErrorGraph");
  auto throwing = std::make_shared<ThrowingNode>("boom");
  throwing->setTypeId("ThrowingNode");
  throwing->setName("Boom");
  graph.addNode(throwing);

  tg::Controller controller;
  EXPECT_THROW(controller.run(graph), tg::NodeError);
  EXPECT_FALSE(controller.lastRunMilliseconds().has_value())
      << "失败 run 不留下旧耗时";

  const std::vector<tg::LogRecord> records = CollectGraphLogs();
  EXPECT_TRUE(HasLogContaining(
      records, "graph 'ErrorGraph' node 'Boom': failed: boom"))
      << "节点 Error 记录带限定名和异常文本";
  EXPECT_TRUE(HasLogContaining(records, "graph 'ErrorGraph': failed"))
      << "图级 Error 记录";
}

TEST(GraphLog, ParallelNodesKeepTheirOwnOrderAndIdentity) {
  GraphLogConfigGuard guard;
  tg::SetLogConfig({tg::LogLevel::Info, tg::kAllLogCategories});
  tg::clearLogRecords();

  tg::Graph graph;
  graph.setName("ParallelGraph");
  auto first = std::make_shared<IntSourceNode>(kLeftInt);
  first->setTypeId("IntSource");
  first->setName("P1");
  auto second = std::make_shared<IntSourceNode>(kRightInt);
  second->setTypeId("IntSource");
  second->setName("P2");
  graph.addNode(first);
  graph.addNode(second);

  tg::Controller controller;
  controller.run(graph);

  const std::vector<tg::LogRecord> records = CollectGraphLogs();
  const auto find_text = [&](const std::string& needle) {
    return std::find_if(records.begin(), records.end(),
                        [&](const tg::LogRecord& record) {
                          return record.text.find(needle) != std::string::npos;
                        });
  };
  const auto p1_start = find_text("graph 'ParallelGraph' node 'P1'");
  const auto p2_start = find_text("graph 'ParallelGraph' node 'P2'");
  EXPECT_NE(p1_start, records.end());
  EXPECT_NE(p2_start, records.end());
  EXPECT_TRUE(HasLogContaining(records, "node 'P1': start (IntSource)"));
  EXPECT_TRUE(HasLogContaining(records, "node 'P1': end"));
  EXPECT_TRUE(HasLogContaining(records, "node 'P2': start (IntSource)"));
  EXPECT_TRUE(HasLogContaining(records, "node 'P2': end"));
}

TEST(GraphLog, SameNodeNameInDifferentGraphsIsQualified) {
  GraphLogConfigGuard guard;
  tg::SetLogConfig({tg::LogLevel::Info, tg::kAllLogCategories});
  tg::clearLogRecords();

  tg::Graph first_graph;
  first_graph.setName("G1");
  auto first = std::make_shared<IntSourceNode>(kLeftInt);
  first->setTypeId("IntSource");
  first->setName("A");
  first_graph.addNode(first);

  tg::Graph second_graph;
  second_graph.setName("G2");
  auto second = std::make_shared<IntSourceNode>(kRightInt);
  second->setTypeId("IntSource");
  second->setName("A");
  second_graph.addNode(second);

  tg::Controller controller;
  controller.run(first_graph);
  controller.run(second_graph);

  const std::vector<tg::LogRecord> records = CollectGraphLogs();
  EXPECT_TRUE(HasLogContaining(records,
                               "graph 'G1' node 'A': start (IntSource)"))
      << "第一张图的 A";
  EXPECT_TRUE(HasLogContaining(records,
                               "graph 'G2' node 'A': start (IntSource)"))
      << "第二张图的 A 不混淆";
}

TEST(GraphLog, DetachedNodeUsesDetachedGraphLabel) {
  GraphLogConfigGuard guard;
  tg::SetLogConfig({tg::LogLevel::Info, tg::kAllLogCategories});
  tg::clearLogRecords();

  auto source = std::make_shared<IntSourceNode>(kLeftInt);
  source->setTypeId("IntSource");
  source->setName("Alone");
  source->computeAndUpdateData();

  const std::vector<tg::LogRecord> records = CollectGraphLogs();
  EXPECT_TRUE(HasLogContaining(records, "graph '(detached)' node 'Alone'"));
}


}  // namespace

TEST(GraphAdd, AddsInOrder) {
  tg::Graph graph;
  auto first = std::make_shared<PortNode>();
  graph.addNode(first);
  const std::vector<std::shared_ptr<tg::Node>> one{first};
  EXPECT_TRUE(sameNodes(graph, one)) << "空图加入一个节点后应只有该节点";

  auto second = std::make_shared<PortNode>();
  graph.addNode(second);
  EXPECT_EQ(graph.getNodes().front(), first) << "先加入的节点应在前";
  EXPECT_EQ(graph.getNodes().back(), second) << "后加入的节点应在后";
  const std::vector<std::shared_ptr<tg::Node>> both{first, second};
  EXPECT_TRUE(sameNodes(graph, both)) << "两个节点都应留在图里";

  graph.addNode(nullptr);
  EXPECT_TRUE(sameNodes(graph, both)) << "加入空指针后图应不变";
}

TEST(GraphCreateNode, AddsAndConnects) {
  tg::Graph graph;
  auto source = graph.createNode<PortNode>();
  auto sink = graph.createNode<PortNode>();
  ASSERT_NE(source, nullptr) << "createNode 应返回非空节点";
  ASSERT_NE(sink, nullptr) << "createNode 应返回非空节点";
  EXPECT_EQ(graph.getNodes().size(), std::size_t{2}) << "两个节点都应入图";

  EXPECT_TRUE(graph.connect(source->outputPort(kFirstPort), sink->inputPort(kFirstPort)))
      << "createNode 创建的节点应能连边";
}

TEST(GraphRemove, WithoutEdges) {
  tg::Graph plain;
  auto kept = std::make_shared<PortNode>();
  auto removed = std::make_shared<PortNode>();
  auto also_kept = std::make_shared<PortNode>();
  plain.addNode(kept);
  plain.addNode(removed);
  plain.addNode(also_kept);
  plain.removeNode(removed);
  const std::vector<std::shared_ptr<tg::Node>> after_one{kept, also_kept};
  EXPECT_TRUE(sameNodes(plain, after_one)) << "删掉无边节点后其余节点应还在";

  const std::vector<std::shared_ptr<tg::Node>> before_outsider = plain.getNodes();
  plain.removeNode(std::make_shared<PortNode>());
  EXPECT_TRUE(sameNodes(plain, before_outsider)) << "删不在图中的节点后图应不变";

  plain.removeNode(nullptr);
  EXPECT_TRUE(sameNodes(plain, before_outsider)) << "删空指针后图应不变";

  plain.removeNode(removed);
  EXPECT_TRUE(sameNodes(plain, before_outsider)) << "重复删除已删节点后图应不变";

  plain.removeNode(kept);
  plain.removeNode(also_kept);
  EXPECT_TRUE(plain.getNodes().empty()) << "全部删光后图应为空";
}

TEST(GraphRemove, OutOnlyNode) {
  tg::Graph out_only;
  auto source = std::make_shared<PortNode>();
  auto sink = std::make_shared<PortNode>();
  out_only.addNode(source);
  out_only.addNode(sink);
  ASSERT_TRUE(out_only.connect(source->outputPort(kFirstPort), sink->inputPort(kFirstPort)))
      << "只有出边的节点应能连到下游";
  source->computeAndUpdateData();
  const auto sink_received = sink->received_ports.size();
  const auto source_received = source->received_ports.size();
  out_only.removeNode(source);
  EXPECT_FALSE(graphHas(out_only, source)) << "只有出边的节点应已离开图";
  EXPECT_TRUE(graphHas(out_only, sink)) << "下游应还在图里";
  EXPECT_FALSE(hasConnection(sink->incoming(), source, kFirstPort, kFirstPort))
      << "删掉只有出边的节点后，下游入边不应再指向它";
  EXPECT_EQ(scheduleEdgeCount(out_only), std::size_t{0})
      << "删掉只有出边的节点后相关调度边应消失";
  source->computeAndUpdateData();
  EXPECT_EQ(sink->received_ports.size(), sink_received)
      << "已删的出边节点不应再往下游送数据";
  sink->computeAndUpdateData();
  EXPECT_EQ(source->received_ports.size(), source_received)
      << "再 computeAndUpdateData 不应送到已删节点";
}

TEST(GraphRemove, InOnlyNode) {
  tg::Graph in_only;
  auto upstream = std::make_shared<PortNode>();
  auto downstream = std::make_shared<PortNode>();
  in_only.addNode(upstream);
  in_only.addNode(downstream);
  ASSERT_TRUE(in_only.connect(upstream->outputPort(kFirstPort), downstream->inputPort(kSecondPort)))
      << "只有入边的节点应能接上上游";
  upstream->computeAndUpdateData();
  const auto downstream_received = downstream->received_ports.size();
  in_only.removeNode(downstream);
  EXPECT_FALSE(hasConnection(upstream->outgoing(), downstream, kFirstPort,
                             kSecondPort))
      << "删掉只有入边的节点后，上游出边不应再指向它";
  EXPECT_EQ(scheduleEdgeCount(in_only), std::size_t{0})
      << "删掉只有入边的节点后相关调度边应消失";
  upstream->computeAndUpdateData();
  EXPECT_EQ(downstream->received_ports.size(), downstream_received)
      << "删掉只有入边的节点后，上游不应再向它送数据";
}

TEST(GraphRemove, MiddleNode) {
  tg::Graph middle_graph;
  auto head = std::make_shared<PortNode>();
  auto middle = std::make_shared<PortNode>();
  auto tail = std::make_shared<PortNode>();
  middle_graph.addNode(head);
  middle_graph.addNode(middle);
  middle_graph.addNode(tail);
  ASSERT_TRUE(middle_graph.connect(head->outputPort(kFirstPort), middle->inputPort(kFirstPort)))
      << "中间节点的入边应连上";
  ASSERT_TRUE(middle_graph.connect(middle->outputPort(kFirstPort), tail->inputPort(kFirstPort)))
      << "中间节点的出边应连上";
  middle_graph.removeNode(middle);
  EXPECT_FALSE(hasConnection(head->outgoing(), middle, kFirstPort, kFirstPort))
      << "删掉中间节点后上游出边应清除";
  EXPECT_FALSE(hasConnection(tail->incoming(), middle, kFirstPort, kFirstPort))
      << "删掉中间节点后下游入边应清除";
  EXPECT_EQ(scheduleEdgeCount(middle_graph), std::size_t{0})
      << "上下游没有直连时，删掉中间节点后调度边应为 0";
}

TEST(GraphRemove, KeepsOtherNodesAndEdges) {
  tg::Graph rest;
  auto left = std::make_shared<PortNode>();
  auto dropped = std::make_shared<PortNode>();
  auto right = std::make_shared<PortNode>();
  rest.addNode(left);
  rest.addNode(dropped);
  rest.addNode(right);
  ASSERT_TRUE(rest.connect(left->outputPort(kFirstPort), right->inputPort(kFirstPort)))
      << "保留的边应连上";
  ASSERT_TRUE(rest.connect(dropped->outputPort(kFirstPort), right->inputPort(kSecondPort)))
      << "将被删节点的边应连上";
  rest.removeNode(dropped);
  const std::vector<std::shared_ptr<tg::Node>> survivors{left, right};
  EXPECT_TRUE(sameNodes(rest, survivors))
      << "删掉其中一个已连线节点后其余节点应还在";
  EXPECT_TRUE(hasConnection(left->outgoing(), right, kFirstPort, kFirstPort))
      << "删掉其中一个已连线节点后其余边应还在";
  EXPECT_TRUE(hasConnection(right->incoming(), left, kFirstPort, kFirstPort))
      << "其余边的下游入边应还在";
}

TEST(GraphConnect, FirstEdgeAndDuplicate) {
  tg::Graph first_edge;
  auto from = std::make_shared<PortNode>();
  auto to = std::make_shared<PortNode>();
  first_edge.addNode(from);
  first_edge.addNode(to);
  ASSERT_TRUE(first_edge.connect(from->outputPort(kFirstPort), to->inputPort(kSecondPort)))
      << "首次 connect 应成功";
  EXPECT_TRUE(hasConnection(from->outgoing(), to, kFirstPort, kSecondPort))
      << "首次 connect 后上游应有 ConnectionInfo";
  EXPECT_TRUE(hasConnection(to->incoming(), from, kFirstPort, kSecondPort))
      << "首次 connect 后下游应有 ConnectionInfo";
  EXPECT_EQ(scheduleEdgeCount(first_edge), std::size_t{1})
      << "首次 connect 后调度边应为 1";
  EXPECT_FALSE(first_edge.connect(from->outputPort(kFirstPort), to->inputPort(kSecondPort)))
      << "同一对端口再连一次应拒绝";
}

TEST(GraphConnect, ByPortPointer) {
  tg::Graph by_port;
  auto port_from = std::make_shared<PortNode>();
  auto port_to = std::make_shared<PortNode>();
  by_port.addNode(port_from);
  by_port.addNode(port_to);
  ASSERT_TRUE(by_port.connect(port_from->outputPort(kSecondPort),
                              port_to->inputPort(kFirstPort)))
      << "Port* connect 应成功";
  EXPECT_FALSE(by_port.connect(port_from->outputPort(kSecondPort),
                               port_to->inputPort(kFirstPort)))
      << "Port* 同一对再连应拒绝";
  EXPECT_FALSE(by_port.connect(static_cast<const tg::Node::Port*>(nullptr),
                               port_to->inputPort(kFirstPort)))
      << "空输出 Port* 应拒绝";
  EXPECT_FALSE(by_port.connect(port_from->outputPort(kSecondPort),
                               static_cast<const tg::Node::Port*>(nullptr)))
      << "空输入 Port* 应拒绝";
}

TEST(GraphConnect, TypedData) {
  tg::Graph typed;
  auto source = std::make_shared<IntSourceNode>(kLeftInt);
  auto add = std::make_shared<IntAddNode>();
  typed.addNode(source);
  typed.addNode(add);
  const tg::Out<TestIntData> from(source.get(), kFirstPort);
  const tg::In<TestIntData> lhs(add.get(), kFirstPort);
  ASSERT_TRUE(typed.connect<TestIntData>(from, lhs))
      << "同一 DataT 的 connect 应成功";
  EXPECT_TRUE(hasConnection(source->outgoing(), add, kFirstPort, kFirstPort))
      << "typed connect 应写入这条数据边";
  EXPECT_FALSE(typed.connect<TestIntData>(from, lhs))
      << "同一对端口再连应被 canConnect 拒绝";
  ASSERT_TRUE(typed.setInputPresentation(add, kFirstPort,
                                         tg::InputPresentation::Property))
      << "输入应能改为属性露面";
  EXPECT_FALSE(typed.connect<TestIntData>(from, lhs))
      << "属性露面的输入应拒绝 typed connect";
}

TEST(GraphConnect, NullNodes) {
  tg::Graph null_nodes;
  auto present = std::make_shared<PortNode>();
  null_nodes.addNode(present);
  EXPECT_FALSE(null_nodes.connect(nullptr, present->inputPort(kFirstPort)))
      << "空上游端口应拒绝";
  EXPECT_FALSE(null_nodes.connect(present->outputPort(kFirstPort), nullptr))
      << "空下游端口应拒绝";
}

TEST(GraphConnect, OutOfRangePort) {
  tg::Graph out_of_range;
  auto range_from = std::make_shared<PortNode>();
  auto range_to = std::make_shared<PortNode>();
  out_of_range.addNode(range_from);
  out_of_range.addNode(range_to);
  const tg::Node::Port* missing = range_from->outputPort(kOutOfRangePortIndex);
  EXPECT_EQ(missing, nullptr) << "越界下标应取不到端口";
  EXPECT_FALSE(out_of_range.connect(missing, range_to->inputPort(kFirstPort)))
      << "越界端口应拒绝";
}

TEST(GraphConnect, DataId) {
  tg::Graph typed;
  auto typed_source = std::make_shared<PortNode>("Vec3f", "Point");
  auto same_data = std::make_shared<PortNode>("Vec3f", "Vector");
  auto other_data = std::make_shared<PortNode>("Mat4", "Point");
  auto empty_data = std::make_shared<PortNode>("", "");
  typed.addNode(typed_source);
  typed.addNode(same_data);
  typed.addNode(other_data);
  typed.addNode(empty_data);
  EXPECT_TRUE(typed.connect(typed_source->outputPort(kFirstPort),
                            same_data->inputPort(kFirstPort)))
      << "同一数据 id、不同 name 应能连接";
  EXPECT_FALSE(typed.connect(typed_source->outputPort(kFirstPort),
                             other_data->inputPort(kFirstPort)))
      << "不同数据 id 应拒绝";
  EXPECT_FALSE(typed.connect(typed_source->outputPort(kFirstPort),
                             empty_data->inputPort(kFirstPort)))
      << "空数据 id 应拒绝";
}

TEST(GraphConnect, SameOutputAnotherInput) {
  tg::Graph another_input;
  auto fan_from = std::make_shared<PortNode>();
  auto fan_to = std::make_shared<PortNode>();
  another_input.addNode(fan_from);
  another_input.addNode(fan_to);
  ASSERT_TRUE(another_input.connect(fan_from->outputPort(kFirstPort),
                                    fan_to->inputPort(kFirstPort)))
      << "同一输出连第一个输入应成功";
  EXPECT_TRUE(another_input.connect(fan_from->outputPort(kFirstPort),
                                    fan_to->inputPort(kSecondPort)))
      << "同一输出连到另一个输入口应成功";
}

TEST(GraphConnect, SelfLoop) {
  tg::Graph loop;
  auto self = std::make_shared<PortNode>();
  loop.addNode(self);
  EXPECT_FALSE(loop.connect(self->outputPort(kFirstPort), self->inputPort(kSecondPort)))
      << "自环应拒绝";
}

TEST(GraphConnect, TwoSourcesTwoInputs) {
  tg::Graph two_sources;
  auto left_source = std::make_shared<PortNode>();
  auto right_source = std::make_shared<PortNode>();
  auto joined = std::make_shared<PortNode>();
  two_sources.addNode(left_source);
  two_sources.addNode(right_source);
  two_sources.addNode(joined);
  const bool left_ok =
      two_sources.connect(left_source->outputPort(kFirstPort), joined->inputPort(kFirstPort));
  const bool right_ok =
      two_sources.connect(right_source->outputPort(kFirstPort), joined->inputPort(kSecondPort));
  EXPECT_TRUE(left_ok) << "第一个源汇入应成功";
  EXPECT_TRUE(right_ok) << "第二个源汇入另一个输入应成功";
  std::size_t joined_edges = 0;
  if (left_ok) {
    ++joined_edges;
  }
  if (right_ok) {
    ++joined_edges;
  }
  EXPECT_EQ(scheduleEdgeCount(two_sources), joined_edges)
      << "两个源汇入两个输入应各有一条调度边";
}

TEST(GraphConnect, PropertyMode) {
  tg::Graph property_mode;
  auto property_from = std::make_shared<PortNode>();
  auto property_to = std::make_shared<PortNode>();
  property_mode.addNode(property_from);
  property_mode.addNode(property_to);
  ASSERT_TRUE(property_mode.setInputPresentation(
      property_to, kFirstPort, tg::InputPresentation::Property))
      << "输入应能改为属性露面";
  EXPECT_FALSE(property_mode.connect(property_from->outputPort(kFirstPort),
                                     property_to->inputPort(kFirstPort)))
      << "属性露面的输入应拒绝连线";
  ASSERT_TRUE(property_mode.setInputPresentation(property_to, kFirstPort,
                                                 tg::InputPresentation::Port))
      << "输入应能改回端口露面";
  EXPECT_TRUE(property_mode.connect(property_from->outputPort(kFirstPort),
                                    property_to->inputPort(kFirstPort)))
      << "改回端口后应能再连";
}

TEST(GraphConnect, SecondUpstreamSameInput) {
  tg::Graph single_source;
  auto first_up = std::make_shared<PortNode>();
  auto second_up = std::make_shared<PortNode>();
  auto shared_in = std::make_shared<PortNode>();
  single_source.addNode(first_up);
  single_source.addNode(second_up);
  single_source.addNode(shared_in);
  ASSERT_TRUE(single_source.connect(first_up->outputPort(kFirstPort),
                                    shared_in->inputPort(kFirstPort)))
      << "同一个输入口的第一条边应成功";
  EXPECT_FALSE(single_source.connect(second_up->outputPort(kFirstPort),
                                     shared_in->inputPort(kFirstPort)))
      << "第二个上游接到同一个输入口应拒绝";
  EXPECT_TRUE(hasConnection(first_up->outgoing(), shared_in, kFirstPort, kFirstPort))
      << "拒绝第二个上游后第一条边应还在";
}

TEST(GraphConnect, BackEdge) {
  tg::Graph back_edge;
  auto node_a = std::make_shared<PortNode>();
  auto node_b = std::make_shared<PortNode>();
  back_edge.addNode(node_a);
  back_edge.addNode(node_b);
  ASSERT_TRUE(back_edge.connect(node_a->outputPort(kFirstPort), node_b->inputPort(kFirstPort)))
      << "回边测试的正向边应成功";
  EXPECT_FALSE(back_edge.connect(node_b->outputPort(kFirstPort), node_a->inputPort(kFirstPort)))
      << "两节点连上后再连回边应拒绝";
}

TEST(GraphConnect, Triangle) {
  tg::Graph triangle;
  auto cycle_a = std::make_shared<PortNode>();
  auto cycle_b = std::make_shared<PortNode>();
  auto cycle_c = std::make_shared<PortNode>();
  triangle.addNode(cycle_a);
  triangle.addNode(cycle_b);
  triangle.addNode(cycle_c);
  ASSERT_TRUE(triangle.connect(cycle_a->outputPort(kFirstPort), cycle_b->inputPort(kFirstPort)))
      << "三节点环的 A→B 应成功";
  ASSERT_TRUE(triangle.connect(cycle_b->outputPort(kFirstPort), cycle_c->inputPort(kFirstPort)))
      << "三节点环的 B→C 应成功";
  EXPECT_FALSE(triangle.connect(cycle_c->outputPort(kFirstPort), cycle_a->inputPort(kFirstPort)))
      << "三节点 A→B→C 后再连 C→A 应拒绝";
}

TEST(GraphConnect, TwoDataEdgesOneSchedule) {
  tg::Graph one_schedule;
  auto pair_from = std::make_shared<PortNode>();
  auto pair_to = std::make_shared<PortNode>();
  one_schedule.addNode(pair_from);
  one_schedule.addNode(pair_to);
  ASSERT_TRUE(one_schedule.connect(pair_from->outputPort(kFirstPort),
                                   pair_to->inputPort(kFirstPort)))
      << "同一对节点的第一条端口对应成功";
  ASSERT_TRUE(one_schedule.connect(pair_from->outputPort(kSecondPort),
                                   pair_to->inputPort(kSecondPort)))
      << "同一对节点的第二条端口对应成功";
  EXPECT_TRUE(hasConnection(pair_from->outgoing(), pair_to, kFirstPort, kFirstPort))
      << "第一条数据边应在";
  EXPECT_TRUE(
      hasConnection(pair_from->outgoing(), pair_to, kSecondPort, kSecondPort))
      << "第二条数据边应在";
  EXPECT_EQ(scheduleEdgeCount(one_schedule), std::size_t{1})
      << "同一对节点的两条数据边只应有一条调度边";
}

TEST(GraphDisconnect, OneEdgeKeepsTheOther) {
  tg::Graph one_edge;
  auto from = std::make_shared<PortNode>();
  auto to = std::make_shared<PortNode>();
  one_edge.addNode(from);
  one_edge.addNode(to);
  ASSERT_TRUE(one_edge.connect(from->outputPort(kFirstPort), to->inputPort(kFirstPort)))
      << "待删的那条边应连上";
  ASSERT_TRUE(one_edge.connect(from->outputPort(kSecondPort), to->inputPort(kSecondPort)))
      << "保留的那条边应连上";
  EXPECT_TRUE(one_edge.disconnect(from->outputPort(kFirstPort), to->inputPort(kFirstPort)))
      << "指定的那条边应删掉";
  EXPECT_FALSE(hasConnection(from->outgoing(), to, kFirstPort, kFirstPort))
      << "删掉后上游不应再有这一对端口";
  EXPECT_FALSE(hasConnection(to->incoming(), from, kFirstPort, kFirstPort))
      << "删掉后下游不应再有这一对端口";
  EXPECT_TRUE(hasConnection(from->outgoing(), to, kSecondPort, kSecondPort))
      << "另一对端口的上游边应还在";
  EXPECT_TRUE(hasConnection(to->incoming(), from, kSecondPort, kSecondPort))
      << "另一对端口的下游边应还在";
  EXPECT_FALSE(one_edge.disconnect(from->outputPort(kFirstPort), to->inputPort(kFirstPort)))
      << "已经没有的边应拒绝";
  EXPECT_FALSE(one_edge.disconnect(nullptr, to->inputPort(kFirstPort)))
      << "空上游应拒绝";
}

TEST(GraphDisconnect, OneInputKeepsOthers) {
  tg::Graph one_input;
  auto from = std::make_shared<PortNode>();
  auto to = std::make_shared<PortNode>();
  one_input.addNode(from);
  one_input.addNode(to);
  ASSERT_TRUE(one_input.connect(from->outputPort(kFirstPort), to->inputPort(kFirstPort)))
      << "待删入边应连上";
  ASSERT_TRUE(one_input.connect(from->outputPort(kSecondPort), to->inputPort(kSecondPort)))
      << "保留的入边应连上";
  one_input.disconnectInputsTo(to, kFirstPort);
  EXPECT_FALSE(hasConnection(from->outgoing(), to, kFirstPort, kFirstPort))
      << "删掉入边后上游 ConnectionInfo 应消失";
  EXPECT_FALSE(hasConnection(to->incoming(), from, kFirstPort, kFirstPort))
      << "删掉入边后下游 ConnectionInfo 应消失";
  EXPECT_TRUE(hasConnection(from->outgoing(), to, kSecondPort, kSecondPort))
      << "其它口的上游边应还在";
  EXPECT_TRUE(hasConnection(to->incoming(), from, kSecondPort, kSecondPort))
      << "其它口的下游边应还在";
}

TEST(GraphDisconnect, PropertyClearsInput) {
  tg::Graph as_property;
  auto property_from = std::make_shared<PortNode>(kTestIntId, kTestIntName);
  auto property_node = std::make_shared<PortNode>(kTestIntId, kTestIntName);
  auto property_out = std::make_shared<PortNode>(kTestIntId, kTestIntName);
  as_property.addNode(property_from);
  as_property.addNode(property_node);
  as_property.addNode(property_out);
  ASSERT_TRUE(as_property.connect(property_from->outputPort(kFirstPort),
                                  property_node->inputPort(kFirstPort)))
      << "改为属性前的入边应连上";
  ASSERT_TRUE(
      as_property.connect(property_node->outputPort(kFirstPort),
                          property_out->inputPort(kFirstPort)))
      << "改为属性前的出边应连上";
  property_node->setInPortData(kFirstPort,
                               std::make_shared<TestIntData>(kSyncedIntValue));
  ASSERT_TRUE(as_property.setInputPresentation(
      property_node, kFirstPort, tg::InputPresentation::Property))
      << "改为属性应成功";
  EXPECT_FALSE(hasConnection(property_from->outgoing(), property_node,
                             kFirstPort, kFirstPort))
      << "改为属性后该口入边应消失";
  EXPECT_TRUE(propertyIsInt(property_node->inputProperty(kFirstPort), 0))
      << "改为属性后该口数据应清空";
  EXPECT_TRUE(hasConnection(property_node->outgoing(), property_out, kFirstPort,
                            kFirstPort))
      << "改为属性后该节点自己的出边应还在";
}

TEST(GraphDisconnect, StopsDelivery) {
  tg::Graph delivery;
  auto sender = std::make_shared<PortNode>();
  auto receiver = std::make_shared<PortNode>();
  delivery.addNode(sender);
  delivery.addNode(receiver);
  ASSERT_TRUE(delivery.connect(sender->outputPort(kFirstPort), receiver->inputPort(kFirstPort)))
      << "送达测试的第一条边应连上";
  ASSERT_TRUE(delivery.connect(sender->outputPort(kSecondPort), receiver->inputPort(kSecondPort)))
      << "送达测试的第二条边应连上";
  delivery.disconnectInputsTo(receiver, kFirstPort);
  receiver->received_ports.clear();
  sender->computeAndUpdateData();
  EXPECT_FALSE(receivedPort(*receiver, kFirstPort))
      << "删掉入边后上游不应再把数据送到该口";
  EXPECT_TRUE(receivedPort(*receiver, kSecondPort)) << "其它口仍应收到数据";
}

TEST(GraphDisconnect, DropScheduleEdges) {
  tg::Graph drop_one_schedule;
  auto pair_from = std::make_shared<PortNode>();
  auto pair_to = std::make_shared<PortNode>();
  drop_one_schedule.addNode(pair_from);
  drop_one_schedule.addNode(pair_to);
  ASSERT_TRUE(
      drop_one_schedule.connect(pair_from->outputPort(kFirstPort), pair_to->inputPort(kFirstPort)))
      << "调度边测试的第一条数据边应连上";
  ASSERT_TRUE(
      drop_one_schedule.connect(pair_from->outputPort(kSecondPort),
                                pair_to->inputPort(kSecondPort)))
      << "调度边测试的第二条数据边应连上";
  drop_one_schedule.disconnectInputsTo(pair_to, kFirstPort);
  EXPECT_FALSE(hasConnection(pair_from->outgoing(), pair_to, kFirstPort, kFirstPort))
      << "只删一条后该数据边应消失";
  EXPECT_TRUE(
      hasConnection(pair_from->outgoing(), pair_to, kSecondPort, kSecondPort))
      << "只删一条后另一条数据边应还在";
  EXPECT_EQ(scheduleEdgeCount(drop_one_schedule), std::size_t{1})
      << "同一对节点删掉一条数据边后调度边仍应为 1";
  drop_one_schedule.disconnectInputsTo(pair_to, kSecondPort);
  EXPECT_TRUE(pair_from->outgoing().empty()) << "最后一条数据边应消失";
  EXPECT_EQ(scheduleEdgeCount(drop_one_schedule), std::size_t{0})
      << "删掉最后一条数据边后调度边应为 0";
}

TEST(GraphDisconnect, DisconnectOneOfTwoKeepsOneSchedule) {
  tg::Graph graph;
  auto from = std::make_shared<PortNode>();
  auto to = std::make_shared<PortNode>();
  graph.addNode(from);
  graph.addNode(to);
  ASSERT_TRUE(graph.connect(from->outputPort(kFirstPort), to->inputPort(kFirstPort)))
      << "第一条端口边应连上";
  ASSERT_TRUE(graph.connect(from->outputPort(kSecondPort), to->inputPort(kSecondPort)))
      << "第二条端口边应连上";
  EXPECT_TRUE(graph.disconnect(from->outputPort(kFirstPort), to->inputPort(kFirstPort)))
      << "disconnect 应删掉指定的那一条";
  EXPECT_FALSE(hasConnection(from->outgoing(), to, kFirstPort, kFirstPort))
      << "被删的数据边应消失";
  EXPECT_TRUE(hasConnection(from->outgoing(), to, kSecondPort, kSecondPort))
      << "另一条数据边应还在";
  EXPECT_EQ(scheduleEdgeCount(graph), std::size_t{1})
      << "同一对节点还留着一条数据边时，调度边应为 1";
}

TEST(GraphDisconnect, DisconnectLastEdgeDropsSchedule) {
  tg::Graph graph;
  auto from = std::make_shared<PortNode>();
  auto to = std::make_shared<PortNode>();
  graph.addNode(from);
  graph.addNode(to);
  ASSERT_TRUE(graph.connect(from->outputPort(kFirstPort), to->inputPort(kFirstPort)))
      << "唯一一条端口边应连上";
  EXPECT_TRUE(graph.disconnect(from->outputPort(kFirstPort), to->inputPort(kFirstPort)))
      << "disconnect 应删掉这唯一一条";
  EXPECT_TRUE(from->outgoing().empty()) << "数据边应消失";
  EXPECT_TRUE(to->incoming().empty()) << "下游入边应消失";
  EXPECT_EQ(scheduleEdgeCount(graph), std::size_t{0})
      << "最后一条数据边删掉后调度边应为 0";
}

TEST(GraphDisconnect, DisconnectThenReconnectKeepsOneSchedule) {
  tg::Graph graph;
  auto from = std::make_shared<PortNode>();
  auto to = std::make_shared<PortNode>();
  graph.addNode(from);
  graph.addNode(to);
  ASSERT_TRUE(graph.connect(from->outputPort(kFirstPort), to->inputPort(kFirstPort)))
      << "重连前的边应连上";
  EXPECT_TRUE(graph.disconnect(from->outputPort(kFirstPort), to->inputPort(kFirstPort)))
      << "disconnect 应先删掉这条边";
  EXPECT_TRUE(graph.connect(from->outputPort(kFirstPort), to->inputPort(kFirstPort)))
      << "删掉后再连上同一口应成功";
  EXPECT_TRUE(hasConnection(from->outgoing(), to, kFirstPort, kFirstPort))
      << "重连后的数据边应在";
  EXPECT_EQ(scheduleEdgeCount(graph), std::size_t{1})
      << "disconnect 后再连上同一口，调度边仍应为 1";
}

TEST(GraphDisconnect, DisconnectOneFanOutKeepsTheOther) {
  tg::Graph graph;
  auto from = std::make_shared<PortNode>();
  auto left = std::make_shared<PortNode>();
  auto right = std::make_shared<PortNode>();
  graph.addNode(from);
  graph.addNode(left);
  graph.addNode(right);
  ASSERT_TRUE(graph.connect(from->outputPort(kFirstPort), left->inputPort(kFirstPort)))
      << "扇出到左节点应连上";
  ASSERT_TRUE(graph.connect(from->outputPort(kSecondPort), right->inputPort(kFirstPort)))
      << "扇出到右节点应连上";
  EXPECT_TRUE(graph.disconnect(from->outputPort(kFirstPort), left->inputPort(kFirstPort)))
      << "disconnect 应只删掉左边这一条";
  EXPECT_FALSE(hasConnection(from->outgoing(), left, kFirstPort, kFirstPort))
      << "左边的数据边应消失";
  EXPECT_TRUE(hasConnection(from->outgoing(), right, kSecondPort, kFirstPort))
      << "右边的数据边应还在";
  EXPECT_EQ(scheduleEdgeCount(graph), std::size_t{1})
      << "只删掉一个下游后，调度边应只剩右边这一条";
}

TEST(GraphDisconnect, ReconnectSamePort) {
  tg::Graph reconnect;
  auto again_from = std::make_shared<PortNode>();
  auto again_to = std::make_shared<PortNode>();
  reconnect.addNode(again_from);
  reconnect.addNode(again_to);
  ASSERT_TRUE(reconnect.connect(again_from->outputPort(kFirstPort),
                                again_to->inputPort(kFirstPort)))
      << "重连前的边应连上";
  reconnect.disconnectInputsTo(again_to, kFirstPort);
  EXPECT_TRUE(reconnect.connect(again_from->outputPort(kFirstPort),
                                again_to->inputPort(kFirstPort)))
      << "删掉后再连上同一口应成功";
  EXPECT_EQ(scheduleEdgeCount(reconnect), std::size_t{1})
      << "删掉后再连上同一口，调度边仍应为 1";
}

TEST(GraphRun, SyncsIntProperty) {
  auto sink = std::make_shared<IntSinkNode>();
  sink->setInPortData(kFirstPort, std::make_shared<TestIntData>(kSyncedIntValue));
  auto synced = sink->inputProperty(kFirstPort);
  EXPECT_TRUE(propertyIsInt(synced, kSyncedIntValue))
      << "setInPortData 应把 int 同步进绑定属性";
  sink->setInPortData(kFirstPort, nullptr);
  EXPECT_TRUE(propertyIsInt(synced, 0)) << "清空入端口后绑定属性应为 0";
}

TEST(GraphRun, SumsTwoSources) {
  tg::Graph graph;
  auto left = std::make_shared<IntSourceNode>(kLeftInt);
  auto right = std::make_shared<IntSourceNode>(kRightInt);
  auto add = std::make_shared<IntAddNode>();
  graph.addNode(left);
  graph.addNode(right);
  graph.addNode(add);
  ASSERT_TRUE(graph.connect(left->outputPort(kFirstPort), add->inputPort(kFirstPort)))
      << "左整数源应连上";
  ASSERT_TRUE(graph.connect(right->outputPort(kFirstPort), add->inputPort(kSecondPort)))
      << "右整数源应连上";
  tg::Controller controller;
  controller.run(graph);
  EXPECT_EQ(add->sum, kLeftInt + kRightInt) << "一次 run 后应为两数之和";
  const tg::Out<TestIntData> sum_port(add.get(), kFirstPort);
  const auto sum_data = sum_port.nodeData();
  ASSERT_NE(sum_data, nullptr) << "输出端口应能按 DataT 取出数据";
  EXPECT_EQ(sum_data->data(), kLeftInt + kRightInt)
      << "nodeData() 的载荷应与加法结果相同";
  const std::vector<std::shared_ptr<tg::Node>> ran{left, right, add};
  EXPECT_TRUE(sameNodes(graph, ran)) << "一次 run 后节点应仍是这次留下的节点";
}

TEST(GraphRun, RethrowsNodeError) {
  tg::Graph graph;
  graph.createNode<ThrowingNode>("boom");
  tg::Controller controller;
  EXPECT_THROW(controller.run(graph), tg::NodeError)
      << "节点 compute 抛出的 NodeError 应原样传播给调用方";
}

TEST(GraphRun, TryRunReturnsError) {
  tg::Graph graph;
  graph.createNode<ThrowingNode>("boom");
  tg::Controller controller;
  const auto error = controller.tryRun(graph);
  ASSERT_TRUE(error.has_value()) << "tryRun 应返回错误消息";
  EXPECT_EQ(*error, "boom") << "错误消息应与节点抛出的一致";
}

TEST(GraphRun, SkipsDownstreamAfterThrow) {
  tg::Graph graph;
  auto boom = graph.createNode<ThrowingNode>("boom");
  auto sink = graph.createNode<ComputeRecorderNode>();
  ASSERT_TRUE(graph.connect(boom->outputPort(kFirstPort), sink->inputPort(kFirstPort)))
      << "失败节点应能连到下游";
  tg::Controller controller;
  EXPECT_THROW(controller.run(graph), tg::NodeError)
      << "失败节点的异常应传播";
  EXPECT_FALSE(sink->computed) << "失败节点的下游应被 taskflow 取消不执行";
}

TEST(GraphAdd, RejectsDuplicateOnSameGraph) {
  tg::Graph graph;
  auto node = std::make_shared<PortNode>();
  graph.addNode(node);
  const std::vector<std::shared_ptr<tg::Node>> once{node};
  graph.addNode(node);
  EXPECT_TRUE(sameNodes(graph, once)) << "同一张图再加入同一节点后图应不变";
}

TEST(GraphAdd, RejectsAttachToAnotherGraph) {
  tg::Graph first;
  tg::Graph second;
  auto node = std::make_shared<PortNode>();
  first.addNode(node);
  const std::vector<std::shared_ptr<tg::Node>> on_first{node};
  second.addNode(node);
  EXPECT_TRUE(second.getNodes().empty()) << "节点不应挂到另一张图";
  EXPECT_TRUE(sameNodes(first, on_first)) << "节点应仍只留在第一次加入的图";
}

TEST(GraphRename, RejectsEmptyAndNull) {
  tg::Graph graph;
  auto node = std::make_shared<PortNode>();
  graph.addNode(node);
  const std::string original = node->getName();
  EXPECT_FALSE(graph.renameNode(node, "")) << "空名应拒绝";
  EXPECT_EQ(node->getName(), original) << "拒绝空名后名字应不变";
  EXPECT_FALSE(graph.renameNode(nullptr, "Other")) << "空节点应拒绝";
}

TEST(GraphConnect, FanOutTwoNodes) {
  tg::Graph graph;
  auto from = std::make_shared<PortNode>();
  auto left = std::make_shared<PortNode>();
  auto right = std::make_shared<PortNode>();
  graph.addNode(from);
  graph.addNode(left);
  graph.addNode(right);
  const bool to_left = graph.connect(from->outputPort(kFirstPort), left->inputPort(kFirstPort));
  const bool to_right = graph.connect(from->outputPort(kFirstPort), right->inputPort(kFirstPort));
  EXPECT_TRUE(to_left) << "扇出到第一个节点应成功";
  EXPECT_TRUE(to_right) << "扇出到第二个节点应成功";
  std::size_t connected = 0;
  if (to_left) {
    ++connected;
  }
  if (to_right) {
    ++connected;
  }
  EXPECT_EQ(scheduleEdgeCount(graph), connected)
      << "扇出的调度边数应等于成功条数";
}

TEST(GraphConnect, DiamondIsNotCycle) {
  tg::Graph graph;
  auto node_a = std::make_shared<PortNode>();
  auto node_b = std::make_shared<PortNode>();
  auto node_c = std::make_shared<PortNode>();
  auto node_d = std::make_shared<PortNode>();
  graph.addNode(node_a);
  graph.addNode(node_b);
  graph.addNode(node_c);
  graph.addNode(node_d);
  EXPECT_TRUE(graph.connect(node_a->outputPort(kFirstPort), node_b->inputPort(kFirstPort)))
      << "菱形 A→B 应成功";
  EXPECT_TRUE(graph.connect(node_a->outputPort(kSecondPort), node_c->inputPort(kFirstPort)))
      << "菱形 A→C 应成功";
  EXPECT_TRUE(graph.connect(node_b->outputPort(kFirstPort), node_d->inputPort(kFirstPort)))
      << "菱形 B→D 应成功";
  EXPECT_TRUE(graph.connect(node_c->outputPort(kFirstPort), node_d->inputPort(kSecondPort)))
      << "菱形 C→D 应成功";
}

TEST(GraphConnect, FourNodeCycle) {
  tg::Graph graph;
  auto node_a = std::make_shared<PortNode>();
  auto node_b = std::make_shared<PortNode>();
  auto node_c = std::make_shared<PortNode>();
  auto node_d = std::make_shared<PortNode>();
  graph.addNode(node_a);
  graph.addNode(node_b);
  graph.addNode(node_c);
  graph.addNode(node_d);
  ASSERT_TRUE(graph.connect(node_a->outputPort(kFirstPort), node_b->inputPort(kFirstPort)))
      << "四节点环的 A→B 应成功";
  ASSERT_TRUE(graph.connect(node_b->outputPort(kFirstPort), node_c->inputPort(kFirstPort)))
      << "四节点环的 B→C 应成功";
  ASSERT_TRUE(graph.connect(node_c->outputPort(kFirstPort), node_d->inputPort(kFirstPort)))
      << "四节点环的 C→D 应成功";
  EXPECT_FALSE(graph.connect(node_d->outputPort(kFirstPort), node_a->inputPort(kFirstPort)))
      << "四节点 A→B→C→D 后再连 D→A 应拒绝";
}

TEST(GraphDisconnect, PropertyDropsSchedule) {
  tg::Graph graph;
  auto from = std::make_shared<PortNode>();
  auto to = std::make_shared<PortNode>();
  graph.addNode(from);
  graph.addNode(to);
  ASSERT_TRUE(graph.connect(from->outputPort(kFirstPort), to->inputPort(kFirstPort)))
      << "改为属性前的唯一入边应连上";
  ASSERT_EQ(scheduleEdgeCount(graph), std::size_t{1})
      << "改为属性前应有一条调度边";
  ASSERT_TRUE(graph.setInputPresentation(to, kFirstPort,
                                         tg::InputPresentation::Property))
      << "唯一入边应能改为属性";
  EXPECT_EQ(scheduleEdgeCount(graph), std::size_t{0})
      << "唯一一条入边改成属性后调度边应为 0";
}

TEST(NodeSet, SetPropertyDisconnects) {
  tg::Graph graph;
  auto source = std::make_shared<IntSourceNode>(kLeftInt);
  auto add = std::make_shared<IntAddNode>();
  graph.addNode(source);
  graph.addNode(add);
  ASSERT_TRUE(graph.connect(source->outputPort(kFirstPort), add->inputPort(kFirstPort)))
      << "setProperty 前的入边应连上";
  ASSERT_EQ(scheduleEdgeCount(graph), std::size_t{1})
      << "setProperty 前应有一条调度边";
  const tg::In<TestIntData> lhs(add.get(), kFirstPort);
  EXPECT_FALSE(source->setProperty<TestIntData>(lhs, kSyncedIntValue))
      << "In 不属于该节点时应返回 false";
  EXPECT_TRUE(hasConnection(add->incoming(), source, kFirstPort, kFirstPort))
      << "写错节点不应拆掉入边";
  ASSERT_TRUE(add->setProperty<TestIntData>(lhs, kSyncedIntValue))
      << "setProperty 应写入这只输入";
  EXPECT_FALSE(hasConnection(add->incoming(), source, kFirstPort, kFirstPort))
      << "setProperty 后入边应消失";
  EXPECT_FALSE(hasConnection(source->outgoing(), add, kFirstPort, kFirstPort))
      << "setProperty 后上游出边应消失";
  EXPECT_EQ(scheduleEdgeCount(graph), std::size_t{0})
      << "唯一入边断开后调度边应为 0";
  EXPECT_EQ(add->inputPresentation(kFirstPort), tg::InputPresentation::Property)
      << "setProperty 后该口应以属性露面";
  EXPECT_TRUE(propertyIsInt(add->inputProperty(kFirstPort), kSyncedIntValue))
      << "绑定属性值应为写入的字面量";
  EXPECT_FALSE(graph.connect(source->outputPort(kFirstPort), add->inputPort(kFirstPort)))
      << "属性露面后再 connect 应拒绝";
}

TEST(NodeSet, SetPropertyAfterGraphGone) {
  std::shared_ptr<IntAddNode> kept;
  {
    tg::Graph graph;
    kept = std::make_shared<IntAddNode>();
    graph.addNode(kept);
  }
  const tg::In<TestIntData> lhs(kept.get(), kFirstPort);
  ASSERT_TRUE(kept->setProperty<TestIntData>(lhs, kSyncedIntValue))
      << "图销毁后 setProperty 仍应写入字面量";
  EXPECT_EQ(kept->inputPresentation(kFirstPort), tg::InputPresentation::Property)
      << "图销毁后写入仍改为属性露面";
  EXPECT_TRUE(propertyIsInt(kept->inputProperty(kFirstPort), kSyncedIntValue))
      << "图销毁后绑定属性值应为写入的字面量";
}

TEST(NodeProp, DeclaresAndAssigns) {
  auto node = std::make_shared<PathNode>();
  auto other = std::make_shared<PathNode>();
  const auto& props = node->getProperties();
  ASSERT_EQ(props.size(), std::size_t{1}) << "Prop 声明后应有一条原生属性";
  ASSERT_NE(props[0], nullptr);
  EXPECT_EQ(props[0]->name, kFilePathName);
  EXPECT_FALSE(other->setProperty(node->file_path, std::string(kScenePath)))
      << "Prop 不属于该节点时应返回 false";
  EXPECT_TRUE(node->file_path.get().empty()) << "写错节点不应改值";
  ASSERT_TRUE(node->setProperty(node->file_path, std::string(kScenePath)))
      << "setProperty 应写入 Prop";
  EXPECT_EQ(node->file_path.get(), kScenePath);
  EXPECT_EQ(std::get<std::string>(node->getProperties()[0]->value), kScenePath)
      << "get() 应与属性表那份值一致";
}

namespace {

// 输入不允许改成属性露面的节点，用来验证拒绝时不断边。
class LockedInputNode : public tg::Node {
 public:
  LockedInputNode() { setTypeId("LockedInputNode"); }

  unsigned int nFixedPorts(tg::PortType port_type) const override {
    constexpr unsigned int kInputCount = 1;
    return port_type == tg::PortType::In ? kInputCount : 0;
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
    return port_type == tg::PortType::In ? "locked" : "";
  }

  const char* fixedPortCaption(tg::PortType port_type,
                               unsigned int index) const override {
    return fixedPortId(port_type, index);
  }

  bool allowsPropertyPresentation(unsigned int) const override { return false; }

  void onSetInPortData(unsigned int, std::shared_ptr<tg::NodeData>) override {}

  std::shared_ptr<tg::NodeData> getOutPortData(unsigned int) override {
    return nullptr;
  }
};

}  // namespace

TEST(NodeSet, AllowsPropertyPresentationRejects) {
  tg::Graph graph;
  auto source = std::make_shared<IntSourceNode>(kLeftInt);
  auto locked = std::make_shared<LockedInputNode>();
  graph.addNode(source);
  graph.addNode(locked);
  ASSERT_TRUE(graph.connect(source->outputPort(kFirstPort), locked->inputPort(kFirstPort)))
      << "拒绝测试的入边应先连上";

  EXPECT_FALSE(graph.setInputPresentation(locked, kFirstPort,
                                          tg::InputPresentation::Property))
      << "allowsPropertyPresentation=false 时应拒绝改属性";
  EXPECT_TRUE(hasConnection(locked->incoming(), source, kFirstPort, kFirstPort))
      << "被拒时入边应仍在，不能被提前拆掉";
  EXPECT_EQ(locked->inputPresentation(kFirstPort), tg::InputPresentation::Port)
      << "被拒后露面方式应保持端口";
  EXPECT_FALSE(
      locked->setInputPresentation(kFirstPort, tg::InputPresentation::Property))
      << "直接调 Node::setInputPresentation 也应拒绝";
}

TEST(NodeSet, DefaultInputPropertyValueByType) {
  auto int_node = std::make_shared<IntSinkNode>();
  const tg::PropertyValue int_default =
      int_node->defaultInputPropertyValue(kFirstPort);
  ASSERT_TRUE(std::holds_alternative<int>(int_default)) << "IntData 默认 int";
  EXPECT_EQ(std::get<int>(int_default), 0);

  auto double_node = std::make_shared<PortNode>("DoubleData", "Double");
  const tg::PropertyValue double_default =
      double_node->defaultInputPropertyValue(kFirstPort);
  ASSERT_TRUE(std::holds_alternative<double>(double_default))
      << "DoubleData 默认 double";
  EXPECT_DOUBLE_EQ(std::get<double>(double_default), 0.0);

  auto bool_node = std::make_shared<PortNode>("BoolData", "Bool");
  const tg::PropertyValue bool_default =
      bool_node->defaultInputPropertyValue(kFirstPort);
  ASSERT_TRUE(std::holds_alternative<bool>(bool_default)) << "BoolData 默认 bool";
  EXPECT_FALSE(std::get<bool>(bool_default));

  auto string_node = std::make_shared<PortNode>("StringData", "String");
  const tg::PropertyValue string_default =
      string_node->defaultInputPropertyValue(kFirstPort);
  ASSERT_TRUE(std::holds_alternative<std::string>(string_default))
      << "其余类型默认空串";
  EXPECT_TRUE(std::get<std::string>(string_default).empty());
}

namespace {

// 输入端口类型随 int 属性切换，用来测 Node/Graph 的端口类型刷新。
class SwitchInputNode : public tg::Node {
 public:
  SwitchInputNode() {
    setTypeId("SwitchInputNode");
    _kind = addProperty("kind", 0);
  }

  unsigned int nFixedPorts(tg::PortType port_type) const override {
    constexpr unsigned int kInputCount = 1;
    return port_type == tg::PortType::In ? kInputCount : 0;
  }

  tg::NodeDataType fixedDataType(tg::PortType port_type,
                                 unsigned int) const override {
    if (port_type != tg::PortType::In) {
      return {};
    }
    tg::NodeDataType type;
    type.id = KindId();
    type.name = KindId();
    return type;
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

 private:
  std::string KindId() const {
    return std::get<int>(_kind->value) == 0 ? "TypeA" : "TypeB";
  }

  std::shared_ptr<Property> _kind;
};

// 进入 compute 后停住，直到 gate 变为放行值。用来看图还在跑时的输出。
class HoldPassNode : public tg::Node {
 public:
  explicit HoldPassNode(std::shared_ptr<std::atomic<int>> gate)
      : _gate(std::move(gate)) {}

  unsigned int nFixedPorts(tg::PortType port_type) const override {
    constexpr unsigned int kPortCount = 1;
    if (port_type == tg::PortType::In || port_type == tg::PortType::Out) {
      return kPortCount;
    }
    return 0;
  }

  tg::NodeDataType fixedDataType(tg::PortType, unsigned int) const override {
    return TestIntData{}.type();
  }

  const char* fixedPortId(tg::PortType, unsigned int) const override {
    return "value";
  }

  const char* fixedPortCaption(tg::PortType port_type,
                               unsigned int index) const override {
    return fixedPortId(port_type, index);
  }

  void onSetInPortData(unsigned int,
                       std::shared_ptr<tg::NodeData> data) override {
    _in = std::move(data);
  }

  std::shared_ptr<tg::NodeData> getOutPortData(unsigned int) override {
    return _out;
  }

  void compute() override {
    constexpr int kEntered = 1;
    constexpr int kRelease = 2;
    constexpr int kHoldSliceMs = 1;
    constexpr int kMaxHoldMs = 5000;
    _gate->store(kEntered);
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(kMaxHoldMs);
    while (_gate->load() < kRelease &&
           std::chrono::steady_clock::now() < deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds(kHoldSliceMs));
    }
    _out = _in;
  }

 private:
  std::shared_ptr<std::atomic<int>> _gate;
  std::shared_ptr<tg::NodeData> _in;
  std::shared_ptr<tg::NodeData> _out;
};

}  // namespace

TEST(GraphPortTypes, RefreshesAndDropsIncompatibleEdges) {
  tg::Graph graph;
  auto source = std::make_shared<PortNode>("TypeA", "TypeA");
  auto switch_node = std::make_shared<SwitchInputNode>();
  graph.addNode(source);
  graph.addNode(switch_node);
  ASSERT_TRUE(graph.connect(source->outputPort(kFirstPort), switch_node->inputPort(kFirstPort)))
      << "TypeA 应能连上";
  ASSERT_EQ(switch_node->inputPort(kFirstPort)->data_type.id, "TypeA");

  switch_node->setProperty(0, 1);
  EXPECT_EQ(switch_node->inputPort(kFirstPort)->data_type.id, "TypeB")
      << "setProperty 后端口类型应原地刷新";
  EXPECT_TRUE(
      hasConnection(switch_node->incoming(), source, kFirstPort, kFirstPort))
      << "Node::setProperty 只刷新类型，不断边";

  EXPECT_TRUE(graph.refreshPortTypes(switch_node));
  EXPECT_FALSE(
      hasConnection(switch_node->incoming(), source, kFirstPort, kFirstPort))
      << "类型改为 TypeB 后不兼容的边应被断开";
}

TEST(GraphPortTypes, ResetsPropertyPresentationOnTypeChange) {
  tg::Graph graph;
  auto node = std::make_shared<SwitchInputNode>();
  graph.addNode(node);
  ASSERT_TRUE(graph.setInputPresentation(
      node, kFirstPort, tg::InputPresentation::Property));
  auto property = node->inputProperty(kFirstPort);
  ASSERT_NE(property, nullptr);
  property->value = std::string("x");
  ASSERT_FALSE(std::get<std::string>(property->value).empty());

  node->getProperties()[0]->value = 1;  // 直接改，模拟 UI 的草稿值
  EXPECT_TRUE(graph.refreshPortTypes(node));
  EXPECT_TRUE(
      std::get<std::string>(node->inputProperty(kFirstPort)->value).empty())
      << "类型变化后 Property 露面的输入应重置默认";
}

TEST(GraphRun, PublishesFinishedNodeWhileTheGraphStillRuns) {
  constexpr int kRelease = 2;
  constexpr int kWaitMs = 2000;
  constexpr int kSliceMs = 1;
  const auto wait_until = [kWaitMs, kSliceMs](const auto& done) {
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(kWaitMs);
    while (std::chrono::steady_clock::now() < deadline) {
      if (done()) {
        return true;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(kSliceMs));
    }
    return done();
  };

  auto first_gate = std::make_shared<std::atomic<int>>(0);
  auto second_gate = std::make_shared<std::atomic<int>>(0);
  tg::Graph graph;
  auto source = std::make_shared<IntSourceNode>(kLeftInt);
  auto first = std::make_shared<HoldPassNode>(first_gate);
  auto second = std::make_shared<HoldPassNode>(second_gate);
  graph.addNode(source);
  graph.addNode(first);
  graph.addNode(second);
  ASSERT_TRUE(graph.connect(source->outputPort(kFirstPort),
                            first->inputPort(kFirstPort)));
  ASSERT_TRUE(graph.connect(first->outputPort(kFirstPort),
                            second->inputPort(kFirstPort)));

  tg::Controller controller;
  ASSERT_TRUE(controller.startRun(graph));
  EXPECT_FALSE(controller.startRun(graph));
  const auto already = controller.tryRun(graph);
  ASSERT_TRUE(already.has_value());
  EXPECT_EQ(*already, "Graph is already running");
  EXPECT_FALSE(graph.connect(source->outputPort(kFirstPort),
                             second->inputPort(kFirstPort)));
  const std::size_t node_count = graph.getNodes().size();
  graph.addNode(std::make_shared<IntSourceNode>(kRightInt));
  EXPECT_EQ(graph.getNodes().size(), node_count);

  ASSERT_TRUE(wait_until(
      [&] { return first_gate->load() == 1; }));
  controller.pollRuns();
  ASSERT_TRUE(graph.isRunning());
  const auto source_data = std::dynamic_pointer_cast<TestIntData>(
      graph.visibleOutput(*source, kFirstPort));
  ASSERT_NE(source_data, nullptr);
  EXPECT_EQ(source_data->data(), kLeftInt);
  EXPECT_EQ(graph.visibleOutput(*first, kFirstPort), nullptr);

  first_gate->store(kRelease);
  ASSERT_TRUE(wait_until([&] {
    controller.pollRuns();
    const auto data = std::dynamic_pointer_cast<TestIntData>(
        graph.visibleOutput(*first, kFirstPort));
    return graph.isRunning() && data != nullptr && data->data() == kLeftInt;
  }));

  tg::Graph other;
  auto other_source = std::make_shared<IntSourceNode>(kRightInt);
  other.addNode(other_source);
  ASSERT_TRUE(controller.startRun(other));
  ASSERT_TRUE(wait_until([&] {
    controller.pollRuns();
    return !other.isRunning() && other.lastRunMilliseconds().has_value();
  }));
  EXPECT_TRUE(graph.isRunning());

  second_gate->store(kRelease);
  ASSERT_TRUE(wait_until([&] {
    controller.pollRuns();
    return !graph.isRunning();
  }));
  ASSERT_TRUE(graph.lastRunMilliseconds().has_value());
  const auto finished = std::dynamic_pointer_cast<TestIntData>(
      second->getOutPortData(kFirstPort));
  ASSERT_NE(finished, nullptr);
  EXPECT_EQ(finished->data(), kLeftInt);
}

