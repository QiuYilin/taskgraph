#pragma once

#include <QUndoCommand>

#include <memory>
#include <string>

#include "Undo.hpp"

class GraphModel;
class ProjectModel;

namespace ui {

/** \brief 把共享 Command 包成 QUndoCommand，推进 Qt 的同一个 QUndoStack。
 *
 *  画布页签与工程级撤销都用它；目标由传入的 UndoTarget 决定。
 */
class UiCommandAdapter : public QUndoCommand {
 public:
  UiCommandAdapter(std::unique_ptr<ui::Command> command, ui::UndoTarget& target,
                   bool already_applied)
      : _command(std::move(command)),
        _target(target),
        _skip_first_redo(already_applied) {
    setText(QString::fromStdString(_command->Label()));
  }

  void redo() override {
    // 命令在入栈前已经落图时，栈的首次 redo 不再写一遍。
    if (_skip_first_redo) {
      _skip_first_redo = false;
      return;
    }
    _command->Apply(_target);
  }

  void undo() override { _command->Revert(_target); }

 private:
  std::unique_ptr<ui::Command> _command;
  ui::UndoTarget& _target;
  bool _skip_first_redo = false;
};

/** \brief 撤销命令落到 Qt 前端：只经 GraphModel 写图，让信号继续刷新各视图。
 *
 *  不在这里触发 QtNodes 的 action；画布原生命令仍在同一个 QUndoStack 里。
 */
class QtUndoTarget : public UndoTarget {
 public:
  explicit QtUndoTarget(GraphModel& graph);

  bool AddNode(const ClipboardNode& snapshot) override;
  bool RemoveNode(const std::string& name) override;
  bool Connect(const LinkSnapshot& link) override;
  bool Disconnect(const LinkSnapshot& link) override;
  bool MoveNode(const std::string& name, tg::NodePose pose) override;
  bool SetProperty(const std::string& node, int property_index,
                   const tg::PropertyValue& value) override;
  bool RenameNode(const std::string& from, const std::string& to) override;
  bool SetPresentation(const std::string& node, unsigned int index,
                       tg::InputPresentation presentation) override;

 private:
  std::shared_ptr<tg::Node> FindNode(const std::string& name) const;

  GraphModel& _graph;
};

/** \brief 工程级命令落到 ProjectModel：图的新建/删除/改名；节点方法不实现。 */
class ProjectUndoTarget : public UndoTarget {
 public:
  explicit ProjectUndoTarget(ProjectModel& model);

  bool AddNode(const ClipboardNode&) override { return false; }
  bool RemoveNode(const std::string&) override { return false; }
  bool Connect(const LinkSnapshot&) override { return false; }
  bool Disconnect(const LinkSnapshot&) override { return false; }
  bool MoveNode(const std::string&, tg::NodePose) override { return false; }
  bool SetProperty(const std::string&, int,
                   const tg::PropertyValue&) override {
    return false;
  }
  bool RenameNode(const std::string&, const std::string&) override {
    return false;
  }
  bool SetPresentation(const std::string&, unsigned int,
                       tg::InputPresentation) override {
    return false;
  }

  bool AddGraph(const GraphClipboard& graph) override;
  bool RemoveGraph(const std::string& name) override;
  bool RenameGraph(const std::string& from, const std::string& to) override;

 private:
  GraphModel* FindGraph(const std::string& name) const;

  ProjectModel& _model;
};

}  // namespace ui
