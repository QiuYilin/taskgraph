#include "MonitorPane.hpp"

#include <QCheckBox>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QAction>
#include <QMenu>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr const char* kMonitorOverflowLabel = "...";
constexpr const char* kMonitorOverflowTooltip = "More";

QString Elide(const QFontMetrics& metrics, const QString& text, int width) {
  return metrics.elidedText(text, Qt::ElideRight, (std::max)(width, 0));
}

}  // namespace

MonitorPane::MonitorPane(const tg::Controller& project,
                         const std::vector<MonitorView>& kinds,
                         std::string kind_id, MonitorWindowState& state,
                         QWidget* parent)
    : QWidget(parent),
      _project(project),
      _kinds(kinds),
      _kind_id(std::move(kind_id)),
      _state(state) {
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);

  auto* toolbar = new QHBoxLayout();
  _source = new QPushButton(kMonitorSourceLabel, this);
  _source->setObjectName("monitorSourceButton");
  _source->setMinimumWidth(kMonitorSourceMinWidth);
  _source->setMaximumWidth(kMonitorSourceMaxWidth);
  _follow = new QCheckBox(kMonitorFollowLabel, this);
  _follow->setObjectName("monitorFollowCheck");
  _follow->setChecked(_state.follow);
  _overflow = new QToolButton(this);
  _overflow->setObjectName("monitorOverflowButton");
  _overflow->setText(kMonitorOverflowLabel);
  _overflow->setToolTip(kMonitorOverflowTooltip);
  _overflow->hide();
  toolbar->addWidget(_source, 1);
  toolbar->addWidget(_follow);
  toolbar->addWidget(_overflow);
  root->addLayout(toolbar);

  _message = new QLabel(this);
  _message->setObjectName("monitorMessage");
  _message->setWordWrap(true);
  _message->setForegroundRole(QPalette::PlaceholderText);
  _body = new QPlainTextEdit(this);
  _body->setObjectName("monitorBody");
  _body->setReadOnly(true);
  _body->hide();
  _status = new QLabel(this);
  _status->setObjectName("monitorStatus");
  _status->setForegroundRole(QPalette::PlaceholderText);
  _status->hide();
  root->addWidget(_message, 1);
  root->addWidget(_body, 1);
  root->addWidget(_status);

  connect(_source, &QPushButton::clicked, this, [this] { ShowSourcePopup(); });
  connect(_follow, &QCheckBox::clicked, this, [this] { ApplyFollowToggle(); });
  connect(_overflow, &QToolButton::clicked, this, [this] {
    QMenu menu(this);
    QAction* follow = menu.addAction(kMonitorFollowLabel);
    follow->setCheckable(true);
    follow->setChecked(_state.follow);
    connect(follow, &QAction::triggered, this, [this] { ApplyFollowToggle(); });
    menu.exec(_overflow->mapToGlobal(QPoint(0, _overflow->height())));
  });
}

const MonitorView* MonitorPane::Kind() const {
  const MonitorView* kind = FindMonitorKind(_kinds, _kind_id);
  // 旧 session 可能没有 kind；只注册了一种时按它恢复。
  if (kind == nullptr && _kind_id.empty() && _kinds.size() == 1) {
    return &_kinds.front();
  }
  return kind;
}

void MonitorPane::Refresh(tg::Node* selected) {
  _selected = selected;
  const MonitorView* kind = Kind();
  if (kind == nullptr) {
    _source->hide();
    _follow->hide();
    _overflow->hide();
    _body->hide();
    _status->hide();
    _message->setText(kMonitorMissingKindMessage);
    _message->show();
    return;
  }
  _source->show();
  const std::vector<MonitorSourceEntry> sources =
      CollectMonitorSources(_project, *kind);
  const char* message = nullptr;
  _resolved = ResolveMonitorSource(_project, selected, *kind, _state, sources,
                                   &message);
  const QString source_text =
      _resolved.label.empty()
          ? QString::fromLatin1(kMonitorSourceLabel)
          : QString::fromLatin1("Source: ") +
                QString::fromStdString(_resolved.label);
  _source->setToolTip(source_text);
  _source->setText(Elide(_source->fontMetrics(), source_text,
                         _source->width() > 0 ? _source->width()
                                              : kMonitorSourceMaxWidth));
  _follow->setChecked(_state.follow);
  if (_resolved.data == nullptr) {
    _body->hide();
    _status->hide();
    _message->setText(message != nullptr ? message : kMonitorNoSourceMessage);
    _message->show();
    UpdateToolbarFit();
    return;
  }
  _message->hide();
  _body->setPlainText(
      QString::fromStdString(MonitorBodyText(*kind, *_resolved.data)));
  _body->show();
  std::string status;
  if (kind->status) {
    status = kind->status(_resolved.data.get());
  }
  const bool room = height() <= 0 || height() >= kMonitorMinContentHeight;
  if (status.empty() || !room) {
    _status->hide();
  } else {
    _status->setText(QString::fromStdString(status));
    _status->show();
  }
  UpdateToolbarFit();
}

void MonitorPane::ApplyFollowToggle() {
  ToggleMonitorFollow(_state, _resolved);
  _follow->setChecked(_state.follow);
  Refresh(_selected);
}

void MonitorPane::ShowSourcePopup() {
  const MonitorView* kind = Kind();
  if (kind == nullptr) {
    return;
  }
  if (_popup == nullptr) {
    _popup = new QWidget(this, Qt::Popup);
    _popup->setObjectName("monitorSourcePopup");
    auto* layout = new QVBoxLayout(_popup);
    _filter = new QLineEdit(_popup);
    _filter->setObjectName("monitorSourceFilter");
    _filter->setPlaceholderText(kMonitorSourceFilterHint);
    _source_list = new QListWidget(_popup);
    _source_list->setObjectName("monitorSourceList");
    layout->addWidget(_filter);
    layout->addWidget(_source_list);
    connect(_filter, &QLineEdit::textChanged, this, [this](const QString& text) {
      _state.source_filter = text.toStdString();
      RebuildSourceList();
    });
    connect(_source_list, &QListWidget::itemClicked, this,
            [this](QListWidgetItem* item) {
              if (item == nullptr || !item->flags().testFlag(Qt::ItemIsEnabled)) {
                return;
              }
              const int index = item->data(Qt::UserRole).toInt();
              if (index < 0 ||
                  index >= static_cast<int>(_popup_sources.size())) {
                return;
              }
              PinSource(_popup_sources[static_cast<std::size_t>(index)]);
              _popup->hide();
            });
  }
  _popup_sources = CollectMonitorSources(_project, *kind);
  _filter->setText(QString::fromStdString(_state.source_filter));
  RebuildSourceList();
  _popup->resize(kMonitorSourceMaxWidth, kMonitorSourcePopupHeight);
  _popup->move(_source->mapToGlobal(QPoint(0, _source->height())));
  _popup->show();
  _filter->setFocus();
}

void MonitorPane::RebuildSourceList() {
  if (_source_list == nullptr) {
    return;
  }
  _source_list->clear();
  bool has_visible = false;
  for (int index = 0; index < static_cast<int>(_popup_sources.size());
       ++index) {
    const MonitorSourceEntry& source =
        _popup_sources[static_cast<std::size_t>(index)];
    if (!MonitorSourceMatchesFilter(source.label, _state.source_filter)) {
      continue;
    }
    has_visible = true;
    auto* item = new QListWidgetItem(QString::fromStdString(source.label));
    item->setData(Qt::UserRole, index);
    _source_list->addItem(item);
  }
  if (!has_visible) {
    auto* item = new QListWidgetItem(kMonitorNoSourceMessage);
    item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
    _source_list->addItem(item);
  }
}

void MonitorPane::PinSource(const MonitorSourceEntry& source) {
  _state.follow = false;
  _state.pinned_node = source.node;
  _state.pinned_port = source.port;
  Refresh(_selected);
}

void MonitorPane::UpdateToolbarFit() {
  if (_follow == nullptr || _source == nullptr || _overflow == nullptr ||
      _source->isHidden()) {
    return;
  }
  const QString follow_label = QString::fromLatin1(kMonitorFollowLabel);
  _follow->setText(follow_label);
  const int follow_full = _follow->sizeHint().width();
  const int overflow_width = _overflow->sizeHint().width();
  const int source_min = kMonitorSourceMinWidth;
  if (width() < source_min + follow_full) {
    _follow->setText(QString());
    _follow->setToolTip(follow_label);
  } else {
    _follow->setToolTip(QString());
  }
  const int follow_now = _follow->sizeHint().width();
  if (width() < source_min + follow_now) {
    _follow->hide();
    _overflow->show();
    if (width() < source_min + overflow_width) {
      _source->setMinimumWidth(0);
    }
  } else {
    _follow->show();
    _overflow->hide();
    _source->setMinimumWidth(source_min);
  }
}

void MonitorPane::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  if (_source != nullptr && !_resolved.label.empty()) {
    const QString source_text =
        QString::fromLatin1("Source: ") + QString::fromStdString(_resolved.label);
    _source->setText(Elide(_source->fontMetrics(), source_text, _source->width()));
  }
  UpdateToolbarFit();
  if (_status != nullptr && !_status->text().isEmpty()) {
    _status->setVisible(height() >= kMonitorMinContentHeight);
  }
}
