#pragma once

#include <QStyledItemDelegate>

/** \brief 属性表 Value 列的编辑委托：int→QSpinBox / Enum→QComboBox、double→QDoubleSpinBox（读 hints 范围/精度）、bool→QCheckBox、string→QLineEdit。 */
class PropertyValueDelegate : public QStyledItemDelegate {
 public:
  explicit PropertyValueDelegate(QObject* parent = nullptr);

  QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                        const QModelIndex& index) const override;
  void setEditorData(QWidget* editor,
                     const QModelIndex& index) const override;
  void setModelData(QWidget* editor, QAbstractItemModel* model,
                    const QModelIndex& index) const override;
};
