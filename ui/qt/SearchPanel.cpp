#include "SearchPanel.hpp"

#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

#include <string>

#include "taskgraph/taskgraph.hpp"

namespace {

inline constexpr int kPanelMargin = 6;

QString HitLabel(const SearchHit& hit) {
  if (hit.graph == nullptr) {
    return {};
  }
  const QString graph_name = QString::fromStdString(hit.graph->getName());
  if (hit.node == nullptr) {
    return graph_name;
  }
  return QString::fromStdString(hit.node->getName()) + QStringLiteral(" — ") +
         graph_name;
}

}  // namespace

SearchPanel::SearchPanel(tg::Controller& controller, QWidget* parent)
    : QWidget(parent), _controller(controller) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(kPanelMargin, kPanelMargin, kPanelMargin,
                             kPanelMargin);
  layout->setSpacing(kPanelMargin);

  _filter = new QLineEdit(this);
  _filter->setObjectName("SearchFilter");
  _filter->setPlaceholderText("Search graphs and nodes");
  _filter->setClearButtonEnabled(true);
  layout->addWidget(_filter);

  _list = new QListWidget(this);
  _list->setObjectName("SearchResults");
  _list->setUniformItemSizes(true);
  layout->addWidget(_list, 1);

  connect(_filter, &QLineEdit::textChanged, this,
          [this] { RebuildResults(); });
  connect(_list, &QListWidget::itemActivated, this,
          [this](QListWidgetItem* item) {
            if (item == nullptr || !on_activate) {
              return;
            }
            const int index = item->data(Qt::UserRole).toInt();
            if (index < 0 || index >= static_cast<int>(_hits.size())) {
              return;
            }
            on_activate(_hits[static_cast<std::size_t>(index)]);
          });
  connect(_list, &QListWidget::itemClicked, this,
          [this](QListWidgetItem* item) {
            if (item == nullptr || !on_activate) {
              return;
            }
            const int index = item->data(Qt::UserRole).toInt();
            if (index < 0 || index >= static_cast<int>(_hits.size())) {
              return;
            }
            on_activate(_hits[static_cast<std::size_t>(index)]);
          });
}

void SearchPanel::Refresh() { RebuildResults(); }

void SearchPanel::FocusFilter() { _filter->setFocus(); }

void SearchPanel::RebuildResults() {
  _hits = CollectSearchHits(_controller.getGraphs(),
                            _filter->text().toStdString());
  _list->clear();
  for (std::size_t index = 0; index < _hits.size(); ++index) {
    auto* item = new QListWidgetItem(HitLabel(_hits[index]), _list);
    item->setData(Qt::UserRole, static_cast<int>(index));
  }
}
