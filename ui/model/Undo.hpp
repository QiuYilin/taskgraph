#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "WorkbenchClipboard.hpp"

namespace ui {

/** \brief 一条连边，两端都用图内 identifier 定位。
 *
 *  撤销/重做会重建节点，tg::Node 地址会变，所以命令里不存节点指针或 NodeId。
 */
struct LinkSnapshot {
  std::string from;
  unsigned int port_out = 0;
  std::string to;
  unsigned int port_in = 0;
};

/** \brief 命令落到某个前端上的入口（Command 的目标）。
 *
 *  命令本身不读 ImGui / Qt；前端实现这个接口，把调用转到自己的模型与画布记录。
 */
class UndoTarget {
 public:
  virtual ~UndoTarget() = default;

  /** \brief 按快照加回一个节点（种类、名字、属性、露面、坐标）；已存在则不改。 */
  virtual bool AddNode(const ClipboardNode& snapshot) = 0;

  /** \brief 按 identifier 删除节点及其连边。 */
  virtual bool RemoveNode(const std::string& name) = 0;

  /** \brief 连一条已有节点之间的边。 */
  virtual bool Connect(const LinkSnapshot& link) = 0;

  /** \brief 断开一条已有节点之间的边。 */
  virtual bool Disconnect(const LinkSnapshot& link) = 0;

  /** \brief 设置一个节点的画布坐标。 */
  virtual bool MoveNode(const std::string& name, tg::NodePose pose) = 0;

  /** \brief 按可编辑属性下标写一个属性值。 */
  virtual bool SetProperty(const std::string& node, int property_index,
                           const tg::PropertyValue& value) = 0;

  /** \brief 改一个节点的 identifier；撞名时返回 false。 */
  virtual bool RenameNode(const std::string& from, const std::string& to) = 0;

  /** \brief 切换固定输入槽的露面方式。 */
  virtual bool SetPresentation(const std::string& node, unsigned int index,
                               tg::InputPresentation presentation) = 0;

  /** \brief 新建一张图（含节点/边/坐标）；工程级命令用。 */
  virtual bool AddGraph(const GraphClipboard& graph) {
    (void)graph;
    return false;
  }

  /** \brief 按图名删除一张图；工程级命令用。 */
  virtual bool RemoveGraph(const std::string& name) {
    (void)name;
    return false;
  }

  /** \brief 改图名；工程级命令用。 */
  virtual bool RenameGraph(const std::string& from, const std::string& to) {
    (void)from;
    (void)to;
    return false;
  }
};

/** \brief 命令（Command）：一次用户操作；Apply 可重放，Revert 回到前一份状态。 */
class Command {
 public:
  virtual ~Command() = default;

  virtual bool Apply(UndoTarget& target) = 0;
  virtual bool Revert(UndoTarget& target) = 0;
  virtual const char* Label() const = 0;
};

namespace detail {

inline const char* CopyLabel(const char* label) {
  return label != nullptr ? label : "Edit";
}

}  // namespace detail

/** \brief 加节点（落点、粘贴、原位复制）；内部边按 ClipboardEdge 下标连接。 */
class AddNodesCommand : public Command {
 public:
  AddNodesCommand(std::vector<ClipboardNode> nodes,
                  std::vector<ClipboardEdge> edges, const char* label)
      : _nodes(std::move(nodes)),
        _edges(std::move(edges)),
        _label(detail::CopyLabel(label)) {}

  bool Apply(UndoTarget& target) override {
    for (const ClipboardNode& node : _nodes) {
      target.AddNode(node);
    }
    for (const ClipboardEdge& edge : _edges) {
      if (edge.from >= _nodes.size() || edge.to >= _nodes.size()) {
        continue;
      }
      target.Connect(LinkSnapshot{_nodes[edge.from].name, edge.port_out,
                                  _nodes[edge.to].name, edge.port_in});
    }
    return true;
  }

  bool Revert(UndoTarget& target) override {
    for (std::size_t index = _nodes.size(); index > 0; --index) {
      target.RemoveNode(_nodes[index - 1].name);
    }
    return true;
  }

  const char* Label() const override { return _label.c_str(); }

 private:
  std::vector<ClipboardNode> _nodes;
  std::vector<ClipboardEdge> _edges;
  std::string _label;
};

/** \brief 删节点；快照含内部边和跨出被删集合的外部边。 */
class DeleteNodesCommand : public Command {
 public:
  DeleteNodesCommand(std::vector<ClipboardNode> nodes,
                     std::vector<LinkSnapshot> links, const char* label)
      : _nodes(std::move(nodes)),
        _links(std::move(links)),
        _label(detail::CopyLabel(label)) {}

  bool Apply(UndoTarget& target) override {
    for (std::size_t index = _nodes.size(); index > 0; --index) {
      target.RemoveNode(_nodes[index - 1].name);
    }
    return true;
  }

  bool Revert(UndoTarget& target) override {
    for (const ClipboardNode& node : _nodes) {
      target.AddNode(node);
    }
    for (const LinkSnapshot& link : _links) {
      target.Connect(link);
    }
    return true;
  }

  const char* Label() const override { return _label.c_str(); }

 private:
  std::vector<ClipboardNode> _nodes;
  std::vector<LinkSnapshot> _links;
  std::string _label;
};

/** \brief 移动一个或多个节点；一次拖动只记一条。 */
class MoveNodesCommand : public Command {
 public:
  struct Move {
    std::string name;
    tg::NodePose before;
    tg::NodePose after;
  };

  explicit MoveNodesCommand(std::vector<Move> moves, const char* label)
      : _moves(std::move(moves)), _label(detail::CopyLabel(label)) {}

  bool Apply(UndoTarget& target) override {
    for (const Move& move : _moves) {
      target.MoveNode(move.name, move.after);
    }
    return true;
  }

  bool Revert(UndoTarget& target) override {
    for (const Move& move : _moves) {
      target.MoveNode(move.name, move.before);
    }
    return true;
  }

  const char* Label() const override { return _label.c_str(); }

 private:
  std::vector<Move> _moves;
  std::string _label;
};

/** \brief 连一条边。 */
class ConnectCommand : public Command {
 public:
  explicit ConnectCommand(LinkSnapshot link)
      : _link(std::move(link)) {}

  bool Apply(UndoTarget& target) override { return target.Connect(_link); }
  bool Revert(UndoTarget& target) override { return target.Disconnect(_link); }
  const char* Label() const override { return "Connect Nodes"; }

 private:
  LinkSnapshot _link;
};

/** \brief 断一条边。 */
class DisconnectCommand : public Command {
 public:
  explicit DisconnectCommand(LinkSnapshot link)
      : _link(std::move(link)) {}

  bool Apply(UndoTarget& target) override { return target.Disconnect(_link); }
  bool Revert(UndoTarget& target) override { return target.Connect(_link); }
  const char* Label() const override { return "Disconnect Nodes"; }

 private:
  LinkSnapshot _link;
};

/** \brief 改一个属性值。 */
class SetPropertyCommand : public Command {
 public:
  SetPropertyCommand(std::string node, int property_index,
                     tg::PropertyValue before, tg::PropertyValue after)
      : _node(std::move(node)),
        _property_index(property_index),
        _before(std::move(before)),
        _after(std::move(after)) {}

  bool Apply(UndoTarget& target) override {
    return target.SetProperty(_node, _property_index, _after);
  }
  bool Revert(UndoTarget& target) override {
    return target.SetProperty(_node, _property_index, _before);
  }
  const char* Label() const override { return "Edit Property"; }

 private:
  std::string _node;
  int _property_index = 0;
  tg::PropertyValue _before;
  tg::PropertyValue _after;
};

/** \brief 改节点 identifier。 */
class RenameNodeCommand : public Command {
 public:
  RenameNodeCommand(std::string from, std::string to)
      : _from(std::move(from)), _to(std::move(to)) {}

  bool Apply(UndoTarget& target) override {
    return target.RenameNode(_from, _to);
  }
  bool Revert(UndoTarget& target) override {
    return target.RenameNode(_to, _from);
  }
  const char* Label() const override { return "Rename Node"; }

 private:
  std::string _from;
  std::string _to;
};

/** \brief 切换输入槽露面；撤销时先恢复露面再连回被断开的入边。 */
class SetPresentationCommand : public Command {
 public:
  SetPresentationCommand(std::string node, unsigned int input_index,
                         tg::InputPresentation before,
                         tg::InputPresentation after,
                         std::vector<LinkSnapshot> broken_links)
      : _node(std::move(node)),
        _input_index(input_index),
        _before(before),
        _after(after),
        _broken_links(std::move(broken_links)) {}

  bool Apply(UndoTarget& target) override {
    return target.SetPresentation(_node, _input_index, _after);
  }

  bool Revert(UndoTarget& target) override {
    if (!target.SetPresentation(_node, _input_index, _before)) {
      return false;
    }
    for (const LinkSnapshot& link : _broken_links) {
      target.Connect(link);
    }
    return true;
  }

  const char* Label() const override { return "Convert Input"; }

 private:
  std::string _node;
  unsigned int _input_index = 0;
  tg::InputPresentation _before = tg::InputPresentation::Port;
  tg::InputPresentation _after = tg::InputPresentation::Port;
  std::vector<LinkSnapshot> _broken_links;
};

/** \brief 新建一张图（粘贴图、新建空图）；撤销即删。 */
class AddGraphCommand : public Command {
 public:
  AddGraphCommand(GraphClipboard graph, const char* label)
      : _graph(std::move(graph)), _label(detail::CopyLabel(label)) {}

  bool Apply(UndoTarget& target) override { return target.AddGraph(_graph); }
  bool Revert(UndoTarget& target) override {
    return target.RemoveGraph(_graph.name);
  }
  const char* Label() const override { return _label.c_str(); }

 private:
  GraphClipboard _graph;
  std::string _label;
};

/** \brief 删除一张图（删图、剪切图）；撤销按快照整图恢复。 */
class RemoveGraphCommand : public Command {
 public:
  RemoveGraphCommand(GraphClipboard graph, const char* label)
      : _graph(std::move(graph)), _label(detail::CopyLabel(label)) {}

  bool Apply(UndoTarget& target) override {
    return target.RemoveGraph(_graph.name);
  }
  bool Revert(UndoTarget& target) override { return target.AddGraph(_graph); }
  const char* Label() const override { return _label.c_str(); }

 private:
  GraphClipboard _graph;
  std::string _label;
};

/** \brief 改图名。 */
class RenameGraphCommand : public Command {
 public:
  RenameGraphCommand(std::string from, std::string to)
      : _from(std::move(from)), _to(std::move(to)) {}

  bool Apply(UndoTarget& target) override {
    return target.RenameGraph(_from, _to);
  }
  bool Revert(UndoTarget& target) override {
    return target.RenameGraph(_to, _from);
  }
  const char* Label() const override { return "Rename Graph"; }

 private:
  std::string _from;
  std::string _to;
};

/** \brief 一次用户操作包含多条子命令（粘贴、多选删除等）。 */
class CompositeCommand : public Command {
 public:
  CompositeCommand(std::vector<std::unique_ptr<Command>> commands,
                   const char* label)
      : _commands(std::move(commands)), _label(detail::CopyLabel(label)) {}

  bool Apply(UndoTarget& target) override {
    for (const auto& command : _commands) {
      if (!command->Apply(target)) {
        return false;
      }
    }
    return true;
  }

  bool Revert(UndoTarget& target) override {
    for (std::size_t index = _commands.size(); index > 0; --index) {
      if (!_commands[index - 1]->Revert(target)) {
        return false;
      }
    }
    return true;
  }

  const char* Label() const override { return _label.c_str(); }

 private:
  std::vector<std::unique_ptr<Command>> _commands;
  std::string _label;
};

/** \brief 撤销栈（Memento 的历史）：按页签的图各存一份，不写盘。
 *
 *  每条命令带一个由调用方给的全局序号；多个栈按序号可做严格跨栈撤销排序。
 */
class UndoStack {
 public:
  /** \brief 推一条命令；already_applied 为真时不再执行 Apply。
   *
   *  未应用的新命令 Apply 失败（如改名撞名）时不入栈，保持当前状态。
   *  新命令会截断 redo 段。sequence 为 0 表示不参与全局排序（测试用）。
   */
  bool Push(std::unique_ptr<Command> command, UndoTarget& target,
            bool already_applied, std::uint64_t sequence = 0) {
    if (command == nullptr) {
      return false;
    }
    if (!already_applied && !command->Apply(target)) {
      return false;
    }
    _entries.resize(_cursor);
    _entries.push_back(Entry{std::move(command), sequence});
    _cursor = _entries.size();
    return true;
  }

  bool Undo(UndoTarget& target) {
    if (_cursor == 0) {
      return false;
    }
    if (!_entries[_cursor - 1].command->Revert(target)) {
      return false;
    }
    --_cursor;
    return true;
  }

  bool Redo(UndoTarget& target) {
    if (_cursor >= _entries.size()) {
      return false;
    }
    if (!_entries[_cursor].command->Apply(target)) {
      return false;
    }
    ++_cursor;
    return true;
  }

  bool CanUndo() const { return _cursor > 0; }
  bool CanRedo() const { return _cursor < _entries.size(); }
  bool empty() const { return _entries.empty(); }
  std::size_t depth() const { return _entries.size(); }

  /** \brief 当前游标位置：下一步 Undo 撤 index-1，下一步 Redo 做 index。 */
  std::size_t index() const { return _cursor; }

  /** \brief 下一步 Undo 会撤的命令序号；没有时为 0。 */
  std::uint64_t NextUndoSequence() const {
    return CanUndo() ? _entries[_cursor - 1].sequence : 0;
  }

  /** \brief 下一步 Redo 会重做的命令序号；没有时为 0。 */
  std::uint64_t NextRedoSequence() const {
    return CanRedo() ? _entries[_cursor].sequence : 0;
  }

  /** \brief 下一步 Undo 会撤的命令名；没有时为 nullptr。 */
  const char* UndoLabel() const {
    return CanUndo() ? _entries[_cursor - 1].command->Label() : nullptr;
  }

  /** \brief 下一步 Redo 会重做的命令名；没有时为 nullptr。 */
  const char* RedoLabel() const {
    return CanRedo() ? _entries[_cursor].command->Label() : nullptr;
  }

  void Clear() {
    _entries.clear();
    _cursor = 0;
  }

 private:
  struct Entry {
    std::unique_ptr<Command> command;
    std::uint64_t sequence = 0;
  };

  std::vector<Entry> _entries;
  std::size_t _cursor = 0;
};

/** \brief 把节点的种类、名字、属性、露面与坐标收成快照。 */
inline ClipboardNode CaptureNodeSnapshot(const tg::Node& node,
                                         tg::NodePose pose) {
  ClipboardNode stored;
  stored.type_id = node.typeId();
  stored.name = node.getName();
  stored.x = pose.x;
  stored.y = pose.y;
  for (const auto& property : node.getProperties()) {
    if (property == nullptr) {
      continue;
    }
    stored.properties.push_back(
        ClipboardProperty{property->name, property->value});
  }
  const unsigned int input_count = node.inputPortCount();
  for (unsigned int index = 0; index < input_count; ++index) {
    ClipboardInput input;
    input.index = index;
    input.presentation = node.inputPresentation(index);
    if (const auto bound = node.inputProperty(index)) {
      input.has_value = true;
      input.value = bound->value;
    }
    stored.inputs.push_back(std::move(input));
  }
  return stored;
}

/** \brief 收一条入边：从上游 identifier 到下游 identifier。 */
inline LinkSnapshot CaptureLink(const tg::Node& from, unsigned int port_out,
                                const tg::Node& to, unsigned int port_in) {
  return LinkSnapshot{from.getName(), port_out, to.getName(), port_in};
}

/** \brief 收节点上的全部入边；删除与会话切换时用。 */
inline void CaptureIncomingLinks(const tg::Node& node,
                                 std::vector<LinkSnapshot>& links) {
  for (const tg::ConnectionInfo& edge : node.incoming()) {
    if (edge.adj == nullptr) {
      continue;
    }
    links.push_back(CaptureLink(*edge.adj, edge.port_out, node, edge.port_in));
  }
}

/** \brief 收删除前的连边：入边全收，出边只收伸到被删集合外的。
 *
 *  内部边已经作为某个被删节点的入边收过一次，出边再收会重复。
 */
inline void CaptureLinksForDeletion(const std::vector<ClipboardSample>& samples,
                                    std::vector<LinkSnapshot>& links) {
  std::unordered_set<const tg::Node*> removed;
  removed.reserve(samples.size());
  for (const ClipboardSample& sample : samples) {
    if (sample.node != nullptr) {
      removed.insert(sample.node.get());
    }
  }
  for (const ClipboardSample& sample : samples) {
    if (sample.node == nullptr) {
      continue;
    }
    for (const tg::ConnectionInfo& edge : sample.node->incoming()) {
      if (edge.adj == nullptr) {
        continue;
      }
      links.push_back(
          CaptureLink(*edge.adj, edge.port_out, *sample.node, edge.port_in));
    }
    for (const tg::ConnectionInfo& edge : sample.node->outgoing()) {
      if (edge.adj == nullptr || removed.count(edge.adj.get()) != 0) {
        continue;
      }
      links.push_back(
          CaptureLink(*sample.node, edge.port_out, *edge.adj, edge.port_in));
    }
  }
}

}  // namespace ui
