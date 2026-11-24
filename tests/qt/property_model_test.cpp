// Qt 专有测试：PropertyModel。
// 行列与类型；端口露面只读、属性露面可改；int/bool/string 写回；
// ConvertPresentation；BrowseValue（注入文件选择器）；外部改属性发 dataChanged。

#include "PropertyModel.hpp"

#include <QSignalSpy>
#include <QtTest>

#include <optional>
#include <memory>
#include <string>

#include "GraphModel.hpp"
#include "NativePathDialog.hpp"
#include "TestNodes.hpp"

namespace {

constexpr int kEditablePropertyCount = 5;
constexpr int kValueRow = 0;
constexpr int kEnabledRow = 1;
constexpr int kPathRow = 2;
constexpr int kFirstInputRow = 3;
constexpr int kSecondInputRow = 4;

struct Fixture {
  Fixture() {
    tgtest::RegisterTestTypes(controller);
    auto graph = controller.createGraph("Graph");
    graph_model = std::make_unique<GraphModel>(graph, controller);
    node = graph_model->addNode(tgtest::kTestNodeTypeId);
    model = std::make_unique<PropertyModel>();
    model->setNode(graph_model.get(), node);
  }

  tg::Controller controller;
  std::unique_ptr<GraphModel> graph_model;
  std::shared_ptr<tg::Node> node;
  std::unique_ptr<PropertyModel> model;
};

struct ScopedPickerReset {
  ~ScopedPickerReset() { NativePathDialog::resetPickers(); }
};

}  // namespace

class PropertyModelTest : public QObject {
  Q_OBJECT

 private slots:
  void rowsAndTypes();
  void boundInputsAndPresentation();
  void editNativeValues();
  void convertPresentation();
  void browseValue();
  void hintRolesAndBrowse();
  void externalChangeEmitsDataChanged();
  void nodeRemovalClearsModel();
};

void PropertyModelTest::rowsAndTypes() {
  Fixture fixture;
  QCOMPARE(fixture.model->rowCount(), kEditablePropertyCount);
  QCOMPARE(fixture.model->columnCount(), 3);

  QVERIFY(fixture.model->data(fixture.model->index(kValueRow, 0)).toString() ==
          QString::fromStdString(tgtest::kValueProperty));
  QVERIFY(fixture.model->data(fixture.model->index(kValueRow, 1)).toString() ==
          QString("int"));
  QCOMPARE(fixture.model->data(fixture.model->index(kValueRow, 2)).toInt(), 0);

  QVERIFY(fixture.model->data(fixture.model->index(kEnabledRow, 1)).toString() ==
          QString("bool"));
  QVERIFY(fixture.model->data(fixture.model->index(kPathRow, 1)).toString() ==
          QString("string"));

  QVERIFY(
      fixture.model->data(fixture.model->index(kFirstInputRow, 0)).toString() ==
      QString::fromStdString(tgtest::kFirstInputCaption));
  QVERIFY(
      fixture.model->data(fixture.model->index(kFirstInputRow, 1)).toString() ==
      QString::fromStdString(tgtest::kTestDataName));

  QVERIFY(fixture.model->headerData(0, Qt::Horizontal).toString() ==
          QString("Name"));
  QVERIFY(fixture.model->headerData(1, Qt::Horizontal).toString() ==
          QString("Type"));
  QVERIFY(fixture.model->headerData(2, Qt::Horizontal).toString() ==
          QString("Value"));
  QVERIFY(!fixture.model->headerData(0, Qt::Vertical).isValid());
}

void PropertyModelTest::boundInputsAndPresentation() {
  Fixture fixture;
  QCOMPARE(fixture.model->BoundInputIndex(kValueRow),
           tg::Node::kNativePropertyInputIndex);
  QCOMPARE(fixture.model->BoundInputIndex(kFirstInputRow), 0);
  QCOMPARE(fixture.model->BoundInputIndex(kSecondInputRow), 1);

  QVERIFY(fixture.model->IsInputAsPort(kFirstInputRow));
  QVERIFY(!(fixture.model->flags(fixture.model->index(kFirstInputRow, 2)) &
            Qt::ItemIsEditable));
  QVERIFY(fixture.model->flags(fixture.model->index(kValueRow, 2)) &
          Qt::ItemIsEditable);
  QVERIFY(fixture.model->IsStringValue(kPathRow));
  QVERIFY(!fixture.model->IsStringValue(kValueRow));
  QVERIFY(fixture.model->node() == fixture.node);
}

void PropertyModelTest::editNativeValues() {
  Fixture fixture;
  QSignalSpy changed(fixture.model.get(), &QAbstractItemModel::dataChanged);

  QVERIFY(fixture.model->setData(fixture.model->index(kValueRow, 2), 42,
                                 Qt::EditRole));
  QCOMPARE(std::get<int>(fixture.node->getProperties()[0]->value), 42);

  QVERIFY(fixture.model->setData(fixture.model->index(kEnabledRow, 2), true,
                                 Qt::EditRole));
  QVERIFY(std::get<bool>(fixture.node->getProperties()[1]->value));

  QVERIFY(fixture.model->setData(fixture.model->index(kPathRow, 2),
                                 QString("C:/assets/a.png"), Qt::EditRole));
  QVERIFY(std::get<std::string>(fixture.node->getProperties()[2]->value) ==
          "C:/assets/a.png");

  QVERIFY(changed.count() >= 3);

  QVERIFY(!fixture.model->setData(fixture.model->index(kValueRow, 0), 1,
                                  Qt::EditRole));
  QVERIFY(!fixture.model->setData(fixture.model->index(kFirstInputRow, 2),
                                  QString("x"), Qt::EditRole));
}

void PropertyModelTest::convertPresentation() {
  Fixture fixture;
  QVERIFY(fixture.model->AllowsPropertyConversion(kFirstInputRow));
  QVERIFY(!fixture.model->AllowsPropertyConversion(kValueRow));
  QVERIFY(!fixture.model->ConvertPresentation(kValueRow, true));
  QVERIFY(!fixture.model->ConvertPresentation(99, true));

  QVERIFY(fixture.model->ConvertPresentation(kFirstInputRow, true));
  QCOMPARE(fixture.node->inputPresentation(0), tg::InputPresentation::Property);
  QVERIFY(!fixture.model->IsInputAsPort(kFirstInputRow));
  QVERIFY(fixture.model->flags(fixture.model->index(kFirstInputRow, 2)) &
          Qt::ItemIsEditable);

  QVERIFY(fixture.model->setData(fixture.model->index(kFirstInputRow, 2),
                                 QString("5"), Qt::EditRole));
  QVERIFY(std::get<std::string>(fixture.node->inputProperty(0)->value) == "5");

  QVERIFY(fixture.model->ConvertPresentation(kFirstInputRow, false));
  QCOMPARE(fixture.node->inputPresentation(0), tg::InputPresentation::Port);
  QVERIFY(fixture.model->IsInputAsPort(kFirstInputRow));
}

void PropertyModelTest::browseValue() {
  Fixture fixture;
  ScopedPickerReset reset;
  NativePathDialog::setFilePicker(
      [](const std::string&, const std::string&)
          -> std::optional<std::string> {
        return std::optional<std::string>{"C:/picked/asset.png"};
      });

  QVERIFY(fixture.model->ConvertPresentation(kFirstInputRow, true));
  QVERIFY(fixture.model->BrowseValue(kFirstInputRow));
  QVERIFY(std::get<std::string>(fixture.node->inputProperty(0)->value) ==
          "C:/picked/asset.png");

  QVERIFY(!fixture.model->BrowseValue(kValueRow));
  QVERIFY(!fixture.model->BrowseValue(99));

  NativePathDialog::setFilePicker(
      [](const std::string&, const std::string&)
          -> std::optional<std::string> { return std::optional<std::string>{}; });
  QVERIFY(!fixture.model->BrowseValue(kFirstInputRow));
}

void PropertyModelTest::hintRolesAndBrowse() {
  tg::Controller controller;
  tgtest::RegisterHintsTestType(controller);
  auto graph = controller.createGraph("Graph");
  GraphModel graph_model(graph, controller);
  auto node = graph_model.addNode(tgtest::kHintsNodeTypeId);
  PropertyModel model;
  model.setNode(&graph_model, node);

  constexpr int kRatioRow = 0;
  constexpr int kModeRow = 1;
  constexpr int kTextRow = 2;
  constexpr int kFileRow = 3;
  constexpr int kFolderRow = 4;

  QCOMPARE(model.data(model.index(kRatioRow, 1)).toString(), QString("double"));
  QCOMPARE(model.data(model.index(kModeRow, 1)).toString(), QString("enum"));
  QCOMPARE(model.data(model.index(kTextRow, 1)).toString(), QString("string"));
  QCOMPARE(model.data(model.index(kFileRow, 1)).toString(), QString("file"));
  QCOMPARE(model.data(model.index(kFolderRow, 1)).toString(),
           QString("directory"));

  QCOMPARE(model.data(model.index(kRatioRow, 2),
                      PropertyModel::HintsEditorRole)
               .toInt(),
           static_cast<int>(tg::PropertyEditor::Double));
  QCOMPARE(model.data(model.index(kRatioRow, 2), PropertyModel::HintsMinRole)
               .toDouble(),
           0.0);
  QCOMPARE(model.data(model.index(kRatioRow, 2), PropertyModel::HintsMaxRole)
               .toDouble(),
           10.0);
  QCOMPARE(model.data(model.index(kRatioRow, 2), PropertyModel::HintsStepRole)
               .toDouble(),
           0.5);
  QCOMPARE(
      model.data(model.index(kRatioRow, 2), PropertyModel::HintsDecimalsRole)
          .toInt(),
      2);

  QCOMPARE(model.data(model.index(kModeRow, 2), PropertyModel::HintsEditorRole)
               .toInt(),
           static_cast<int>(tg::PropertyEditor::Enum));
  QCOMPARE(model.data(model.index(kModeRow, 2), PropertyModel::HintsChoicesRole)
               .toStringList()
               .size(),
           2);

  QVERIFY(!model.AllowsBrowse(kRatioRow));
  QVERIFY(!model.AllowsBrowse(kModeRow));
  QVERIFY(!model.AllowsBrowse(kTextRow));
  QVERIFY(model.AllowsBrowse(kFileRow));
  QVERIFY(model.AllowsBrowse(kFolderRow));
}

void PropertyModelTest::externalChangeEmitsDataChanged() {
  Fixture fixture;
  QSignalSpy changed(fixture.model.get(), &QAbstractItemModel::dataChanged);
  QVERIFY(fixture.graph_model->setProperty(fixture.node, kValueRow, 7));
  QCOMPARE(changed.count(), 1);
  QCOMPARE(fixture.model->data(fixture.model->index(kValueRow, 2)).toInt(), 7);
}

void PropertyModelTest::nodeRemovalClearsModel() {
  Fixture fixture;
  QSignalSpy reset(fixture.model.get(), &QAbstractItemModel::modelReset);
  QVERIFY(fixture.graph_model->removeNode(fixture.node));
  QCOMPARE(reset.count(), 1);
  QCOMPARE(fixture.model->rowCount(), 0);
  QVERIFY(fixture.model->node() == nullptr);
}

QTEST_GUILESS_MAIN(PropertyModelTest)
#include "property_model_test.moc"
