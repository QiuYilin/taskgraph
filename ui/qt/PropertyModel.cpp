#include "PropertyModel.hpp"

#include <QString>

#include <string>
#include <utility>

#include "NativePathDialog.hpp"

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
  if (property != nullptr && _node != nullptr) {
    const int bound = property->bound_input_index;
    if (bound != tg::Node::kNativePropertyInputIndex) {
      const tg::Node::Port* port =
          _node->inputPort(static_cast<unsigned int>(bound));
      if (port != nullptr) {
        return QString::fromStdString(port->data_type.name);
      }
    }
  }
  if (property != nullptr) {
    if (std::holds_alternative<int>(property->value)) {
      return QStringLiteral("int");
    }
    if (std::holds_alternative<std::string>(property->value)) {
      return QStringLiteral("string");
    }
    if (std::holds_alternative<bool>(property->value)) {
      return QStringLiteral("bool");
    }
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

  if (role == Qt::DisplayRole) {
    if (index.column() == 0) {
      return QString::fromStdString(property->name);
    }
    if (index.column() == 1) {
      return TypeName(property);
    }
    if (index.column() == 2) {
      if (const auto* number = std::get_if<int>(&property->value)) {
        return *number;
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
    if (const auto* text = std::get_if<std::string>(&property->value)) {
      return QString::fromStdString(*text);
    }
    if (const auto* flag = std::get_if<bool>(&property->value)) {
      return *flag;
    }
    return {};
  }

  if (role == Qt::TextAlignmentRole && index.column() == 2 &&
      std::holds_alternative<int>(property->value)) {
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
  if (IsInputAsPort(index.row())) {
    return false;
  }
  const auto property = PropertyAt(index.row());
  if (property == nullptr) {
    return false;
  }
  if (std::holds_alternative<int>(property->value)) {
    return _graph->setProperty(_node, index.row(), value.toInt());
  }
  if (std::holds_alternative<bool>(property->value)) {
    return _graph->setProperty(_node, index.row(), value.toBool());
  }
  if (std::holds_alternative<std::string>(property->value)) {
    return _graph->setProperty(_node, index.row(),
                              value.toString().toStdString());
  }
  return false;
}

Qt::ItemFlags PropertyModel::flags(const QModelIndex& index) const {
  if (!index.isValid()) {
    return Qt::NoItemFlags;
  }
  Qt::ItemFlags result = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
  if (index.column() == 2 && !IsInputAsPort(index.row())) {
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

bool PropertyModel::ConvertPresentation(int row, bool to_property) {
  if (_graph == nullptr || _node == nullptr) {
    return false;
  }
  const auto property = PropertyAt(row);
  if (property == nullptr ||
      property->bound_input_index == tg::Node::kNativePropertyInputIndex) {
    return false;
  }
  return _graph->setInputPresentation(
      _node, static_cast<unsigned int>(property->bound_input_index),
      to_property ? tg::InputPresentation::Property
                  : tg::InputPresentation::Port);
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
  const std::string default_path =
      NativePathDialog::resolveDefaultPath(*text, {});
  const std::optional<std::string> picked =
      NativePathDialog::pickOpenFile("Select file", default_path);
  std::string target = *text;
  if (!NativePathDialog::applyPickedPath(target, picked)) {
    return false;
  }
  return _graph->setProperty(_node, row, target);
}
