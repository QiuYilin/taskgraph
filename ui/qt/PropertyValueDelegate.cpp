#include "PropertyValueDelegate.hpp"

#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QMetaType>
#include <QSpinBox>
#include <QStringList>
#include <QVariant>

#include <limits>

#include "PropertyModel.hpp"

namespace {
constexpr int kDoubleSpinDecimals = 6;
constexpr double kDoubleSpinLimit = 1.0e9;
}  // namespace

PropertyValueDelegate::PropertyValueDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

QWidget* PropertyValueDelegate::createEditor(
    QWidget* parent, const QStyleOptionViewItem&, const QModelIndex& index) const {
  if (!(index.flags() & Qt::ItemIsEditable)) {
    return nullptr;
  }
  const QVariant value = index.data(Qt::EditRole);
  const int editor_kind = index.data(PropertyModel::HintsEditorRole).toInt();
  if (value.typeId() == QMetaType::Int) {
    // 枚举：按 choices 出下拉，提交仍然是 int 下标。
    const QStringList choices =
        index.data(PropertyModel::HintsChoicesRole).toStringList();
    if (editor_kind == static_cast<int>(tg::PropertyEditor::Enum) &&
        !choices.isEmpty()) {
      auto* editor = new QComboBox(parent);
      editor->addItems(choices);
      return editor;
    }
    auto* editor = new QSpinBox(parent);
    editor->setButtonSymbols(QAbstractSpinBox::NoButtons);
    editor->setRange(std::numeric_limits<int>::min(),
                     std::numeric_limits<int>::max());
    return editor;
  }
  if (value.typeId() == QMetaType::Double) {
    auto* editor = new QDoubleSpinBox(parent);
    editor->setButtonSymbols(QAbstractSpinBox::NoButtons);
    const double min = index.data(PropertyModel::HintsMinRole).toDouble();
    const double max = index.data(PropertyModel::HintsMaxRole).toDouble();
    const double step = index.data(PropertyModel::HintsStepRole).toDouble();
    const int decimals =
        index.data(PropertyModel::HintsDecimalsRole).toInt();
    // 只有提示给了正区间时才收紧范围。
    editor->setRange(max > min ? min : -kDoubleSpinLimit,
                     max > min ? max : kDoubleSpinLimit);
    editor->setDecimals(decimals >= 0 ? decimals : kDoubleSpinDecimals);
    if (step > 0.0) {
      editor->setSingleStep(step);
    }
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
  } else if (auto* real_spin = qobject_cast<QDoubleSpinBox*>(editor)) {
    real_spin->setValue(value.toDouble());
  } else if (auto* combo = qobject_cast<QComboBox*>(editor)) {
    combo->setCurrentIndex(value.toInt());
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
  } else if (auto* real_spin = qobject_cast<QDoubleSpinBox*>(editor)) {
    model->setData(index, real_spin->value(), Qt::EditRole);
  } else if (auto* combo = qobject_cast<QComboBox*>(editor)) {
    model->setData(index, combo->currentIndex(), Qt::EditRole);
  } else if (auto* check = qobject_cast<QCheckBox*>(editor)) {
    model->setData(index, check->isChecked(), Qt::EditRole);
  } else if (auto* line = qobject_cast<QLineEdit*>(editor)) {
    model->setData(index, line->text(), Qt::EditRole);
  }
}
