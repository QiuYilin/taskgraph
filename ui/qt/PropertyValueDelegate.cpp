#include "PropertyValueDelegate.hpp"

#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QMetaType>
#include <QSpinBox>
#include <QVariant>

#include <limits>

PropertyValueDelegate::PropertyValueDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

QWidget* PropertyValueDelegate::createEditor(
    QWidget* parent, const QStyleOptionViewItem&, const QModelIndex& index) const {
  if (!(index.flags() & Qt::ItemIsEditable)) {
    return nullptr;
  }
  const QVariant value = index.data(Qt::EditRole);
  if (value.typeId() == QMetaType::Int) {
    auto* editor = new QSpinBox(parent);
    editor->setButtonSymbols(QAbstractSpinBox::NoButtons);
    editor->setRange(std::numeric_limits<int>::min(),
                     std::numeric_limits<int>::max());
    return editor;
  }
  if (value.typeId() == QMetaType::Bool) {
    return new QCheckBox(parent);
  }
  return new QLineEdit(parent);
}

void PropertyValueDelegate::setEditorData(QWidget* editor,
                                          const QModelIndex& index) const {
  const QVariant value = index.data(Qt::EditRole);
  if (auto* spin = qobject_cast<QSpinBox*>(editor)) {
    spin->setValue(value.toInt());
  } else if (auto* check = qobject_cast<QCheckBox*>(editor)) {
    check->setChecked(value.toBool());
  } else if (auto* line = qobject_cast<QLineEdit*>(editor)) {
    line->setText(value.toString());
  }
}

void PropertyValueDelegate::setModelData(QWidget* editor,
                                         QAbstractItemModel* model,
                                         const QModelIndex& index) const {
  if (auto* spin = qobject_cast<QSpinBox*>(editor)) {
    model->setData(index, spin->value(), Qt::EditRole);
  } else if (auto* check = qobject_cast<QCheckBox*>(editor)) {
    model->setData(index, check->isChecked(), Qt::EditRole);
  } else if (auto* line = qobject_cast<QLineEdit*>(editor)) {
    model->setData(index, line->text(), Qt::EditRole);
  }
}
