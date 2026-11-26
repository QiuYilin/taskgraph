#include "PropertyModel.hpp"

#include <QString>
#include <QStringList>

#include <string>
#include <utility>
#include <vector>

#include "NativePathDialog.hpp"

namespace {

// 按当前存储类型把委托给的值收成 PropertyValue。
tg::PropertyValue UpdatedValue(const tg::PropertyValue& current,
                               const QVariant& value) {
  if (std::holds_alternative<int>(current)) {
    return value.toInt();
  }
  if (std::holds_alternative<double>(current)) {
    return value.toDouble();
  }
  if (std::holds_alternative<bool>(current)) {
    return value.toBool();
  }
  return value.toString().toStdString();
}

}  // namespace

PropertyModel::PropertyModel(QObject* parent) : QAbstractTableModel(parent) {}

void PropertyModel::setNode(GraphModel* graph, std::shared_ptr<tg::Node> node) {
  if (_graph == graph && _node == node) {
    Refresh();
    return;
  }

  beginResetModel();
  DisconnectGraph();
  _graph = graph;
  _node = std::move(node);
  _properties.clear();
  if (_node != nullptr) {
    _properties = _node->getEditableProperties();
  }

  if (_graph != nullptr) {
    _connections.push_back(connect(
        _graph, &GraphModel::propertyChanged, this, [this](int index) {
          if (_graph == nullptr || _graph->nodeAt(index) != _node ||
              _properties.empty()) {
            return;
          }
          emit dataChanged(
              this->index(0, 0),
              this->index(static_cast<int>(_properties.size()) - 1,
                          columnCount() - 1));
        }));
    _connections.push_back(connect(
        _graph, &GraphModel::portsChanged, this, [this](int index) {
          if (_graph != nullptr && _graph->nodeAt(index) == _node) {
            Refresh();
          }
        }));
    _connections.push_back(connect(
        _graph, &GraphModel::nodeAboutToBeRemoved, this, [this](int index) {
          if (_graph != nullptr && _graph->nodeAt(index) == _node) {
            setNode(nullptr, nullptr);
          }
        }));
  }
  endResetModel();
}

void PropertyModel::DisconnectGraph() {
  for (const auto& connection : _connections) {
    disconnect(connection);
  }
  _connections.clear();
}

void PropertyModel::Refresh() {
  beginResetModel();
  _properties =
      _node != nullptr ? _node->getEditableProperties()
                       : std::vector<std::shared_ptr<tg::Node::Property>>();
  endResetModel();
}

int PropertyModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : static_cast<int>(_properties.size());
}

int PropertyModel::columnCount(const QModelIndex&) const { return 3; }

std::shared_ptr<tg::Node::Property> PropertyModel::PropertyAt(int row) const {
  if (row < 0 || row >= static_cast<int>(_properties.size())) {
    return nullptr;
  }
  return _properties[static_cast<std::size_t>(row)];
}

QString PropertyModel::TypeName(
    const std::shared_ptr<tg::Node::Property>& property) const {
  if (property == nullptr) {
    return {};
  }
  // Enum 是语义类型：native 与 bound 都优先显示 enum。
  if (property->hints.editor == tg::PropertyEditor::Enum) {
    return QStringLiteral("enum");
  }
  if (_node != nullptr) {
    const int bound = property->bound_input_index;
    if (bound != tg::Node::kNativePropertyInputIndex) {
      const tg::Node::Port* port =
          _node->inputPort(static_cast<unsigned int>(bound));
      if (port != nullptr) {
        return QString::fromStdString(port->data_type.name);
      }
    }
  }
  // 原生属性按 hints.editor 显示语义类型；Auto 回退存储类型。
  switch (property->hints.editor) {
    case tg::PropertyEditor::Integer:
      return QStringLiteral("int");
    case tg::PropertyEditor::Double:
      return QStringLiteral("double");
    case tg::PropertyEditor::Text:
      return QStringLiteral("string");
    case tg::PropertyEditor::File:
      return QStringLiteral("file");
    case tg::PropertyEditor::Directory:
      return QStringLiteral("directory");
    case tg::PropertyEditor::Enum:
      return QStringLiteral("enum");
    case tg::PropertyEditor::Auto:
      break;
  }
  if (std::holds_alternative<int>(property->value)) {
    return QStringLiteral("int");
  }
  if (std::holds_alternative<double>(property->value)) {
    return QStringLiteral("double");
  }
  if (std::holds_alternative<std::string>(property->value)) {
    return QStringLiteral("string");
  }
  if (std::holds_alternative<bool>(property->value)) {
    return QStringLiteral("bool");
  }
  return {};
}

QVariant PropertyModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid()) {
    return {};
  }
  const auto property = PropertyAt(index.row());
  if (property == nullptr) {
    return {};
  }

  // 属性提示：委托按这些 role 选编辑器、范围与枚举项。
  if (index.column() == 2 && role >= HintsEditorRole &&
      role <= HintsChoicesRole) {
    switch (role) {
      case HintsEditorRole:
        return static_cast<int>(property->hints.editor);
      case HintsMinRole:
        return property->hints.min;
      case HintsMaxRole:
        return property->hints.max;
      case HintsStepRole:
        return property->hints.step;
      case HintsDecimalsRole:
        return property->hints.decimals;
      case HintsChoicesRole: {
        QStringList choices;
        for (const std::string& choice : property->hints.choices) {
          choices << QString::fromStdString(choice);
        }
        return choices;
      }
      default:
        break;
    }
  }

  if (role == Qt::DisplayRole) {    if (index.column() == 0) {
      return QString::fromStdString(property->name);
    }
    if (index.column() == 1) {
      return TypeName(property);
    }
    if (index.column() == 2) {
      if (const auto* number = std::get_if<int>(&property->value)) {
        return *number;
      }
      if (const auto* real = std::get_if<double>(&property->value)) {
        return *real;
      }
      if (const auto* text = std::get_if<std::string>(&property->value)) {
        return QString::fromStdString(*text);
      }
      if (const auto* flag = std::get_if<bool>(&property->value)) {
        return *flag ? QStringLiteral("true") : QStringLiteral("false");
      }
    }
    return {};
  }

  if (role == Qt::EditRole && index.column() == 2) {
    if (const auto* number = std::get_if<int>(&property->value)) {
      return *number;
    }
    if (const auto* real = std::get_if<double>(&property->value)) {
      return *real;
    }
    if (const auto* text = std::get_if<std::string>(&property->value)) {
      return QString::fromStdString(*text);
    }
    if (const auto* flag = std::get_if<bool>(&property->value)) {
      return *flag;
    }
    return {};
  }

  if (role == Qt::TextAlignmentRole && index.column() == 2 &&
      (std::holds_alternative<int>(property->value) ||
       std::holds_alternative<double>(property->value))) {
    return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
  }
  return {};
}

bool PropertyModel::setData(const QModelIndex& index, const QVariant& value,
                            int role) {
  if (!index.isValid() || index.column() != 2 || role != Qt::EditRole ||
      _graph == nullptr || _node == nullptr) {
    return false;
  }
  if (IsInputAsPort(index.row()) ||
      (_graph->graph() != nullptr && _graph->graph()->isRunning())) {
    return false;
  }
  const auto property = PropertyAt(index.row());
  if (property == nullptr) {
    return false;
  }
  const tg::PropertyValue updated = UpdatedValue(property->value, value);
  // 值没变就不占撤销栈。
  if (updated == property->value) {
    return true;
  }
  // 打开的页签把这次修改记进同一个撤销栈；没有画布时直接写图。
  if (_undo_push) {
    auto command = std::make_unique<ui::SetPropertyCommand>(
        _node->getName(), index.row(), property->value, updated);
    if (_undo_push(_graph, std::move(command))) {
      return true;
    }
  }
  return _graph->setProperty(_node, index.row(), updated);
}

Qt::ItemFlags PropertyModel::flags(const QModelIndex& index) const {
  if (!index.isValid()) {
    return Qt::NoItemFlags;
  }
  Qt::ItemFlags result = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
  const bool graph_running =
      _graph != nullptr && _graph->graph() != nullptr &&
      _graph->graph()->isRunning();
  if (index.column() == 2 && !IsInputAsPort(index.row()) && !graph_running) {
    result |= Qt::ItemIsEditable;
  }
  return result;
}

QVariant PropertyModel::headerData(int section, Qt::Orientation orientation,
                                   int role) const {
  if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
    return {};
  }
  switch (section) {
    case 0:
      return QStringLiteral("Name");
    case 1:
      return QStringLiteral("Type");
    case 2:
      return QStringLiteral("Value");
    default:
      return {};
  }
}

bool PropertyModel::IsInputAsPort(int row) const {
  const auto property = PropertyAt(row);
  if (property == nullptr || _node == nullptr ||
      property->bound_input_index == tg::Node::kNativePropertyInputIndex) {
    return false;
  }
  return _node->isInputShownAsPort(
      static_cast<unsigned int>(property->bound_input_index));
}

int PropertyModel::BoundInputIndex(int row) const {
  const auto property = PropertyAt(row);
  return property != nullptr ? property->bound_input_index
                             : tg::Node::kNativePropertyInputIndex;
}

bool PropertyModel::IsStringValue(int row) const {
  const auto property = PropertyAt(row);
  return property != nullptr &&
         std::holds_alternative<std::string>(property->value);
}

bool PropertyModel::AllowsPropertyConversion(int row) const {
  const auto property = PropertyAt(row);
  if (property == nullptr || _node == nullptr ||
      property->bound_input_index == tg::Node::kNativePropertyInputIndex) {
    return false;
  }
  return _node->allowsPropertyPresentation(
      static_cast<unsigned int>(property->bound_input_index));
}

bool PropertyModel::AllowsBrowse(int row) const {
  const auto property = PropertyAt(row);
  if (property == nullptr || IsInputAsPort(row) ||
      !std::holds_alternative<std::string>(property->value)) {
    return false;
  }
  // Text 是纯文本；Auto/File/Directory 仍按路径提供浏览。
  return property->hints.editor != tg::PropertyEditor::Text;
}

bool PropertyModel::ConvertPresentation(int row, bool to_property) {
  if (_graph == nullptr || _node == nullptr) {
    return false;
  }
  const auto property = PropertyAt(row);
  if (property == nullptr ||
      property->bound_input_index == tg::Node::kNativePropertyInputIndex) {
    return false;
  }
  const unsigned int input_index =
      static_cast<unsigned int>(property->bound_input_index);
  const tg::InputPresentation target = to_property
                                           ? tg::InputPresentation::Property
                                           : tg::InputPresentation::Port;
  const tg::InputPresentation before = _node->inputPresentation(input_index);
  // 没有变化就不占撤销栈。
  if (before == target) {
    return true;
  }
  std::vector<ui::LinkSnapshot> broken;
  // 转成属性会断开这个输入上的入边，撤销时要先露面再连回。
  if (target == tg::InputPresentation::Property) {
    for (const tg::ConnectionInfo& edge : _node->incoming()) {
      if (edge.adj != nullptr && edge.port_in == input_index) {
        broken.push_back(ui::CaptureLink(*edge.adj, edge.port_out, *_node,
                                        edge.port_in));
      }
    }
  }
  if (_undo_push) {
    auto command = std::make_unique<ui::SetPresentationCommand>(
        _node->getName(), input_index, before, target, std::move(broken));
    if (_undo_push(_graph, std::move(command))) {
      return true;
    }
  }
  return _graph->setInputPresentation(_node, input_index, target);
}

bool PropertyModel::BrowseValue(int row) {
  if (_graph == nullptr || _node == nullptr || IsInputAsPort(row)) {
    return false;
  }
  const auto property = PropertyAt(row);
  if (property == nullptr) {
    return false;
  }
  auto* text = std::get_if<std::string>(&property->value);
  if (text == nullptr) {
    return false;
  }
  const bool pick_directory =
      property->hints.editor == tg::PropertyEditor::Directory;
  const std::string default_path =
      NativePathDialog::resolveDefaultPath(*text, {});
  const std::optional<std::string> picked =
      pick_directory
          ? NativePathDialog::pickDirectory("Select folder", default_path)
          : NativePathDialog::pickOpenFile("Select file", default_path);
  std::string target = *text;
  if (!NativePathDialog::applyPickedPath(target, picked)) {
    return false;
  }
  // 路径改动也走属性命令；没有画布时直接写图。
  if (_undo_push) {
    auto command = std::make_unique<ui::SetPropertyCommand>(
        _node->getName(), row, property->value, tg::PropertyValue{target});
    if (_undo_push(_graph, std::move(command))) {
      return true;
    }
  }
  return _graph->setProperty(_node, row, std::move(target));
}
