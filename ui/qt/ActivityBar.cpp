#include "ActivityBar.hpp"

#include <QButtonGroup>
#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QSize>
#include <QToolButton>
#include <QVBoxLayout>

#include "Theme.hpp"

namespace {

inline constexpr int kActivityBarWidthPx =
    static_cast<int>(kActivityBarWidthEpx);
inline constexpr int kActivityBarButtonPx =
    static_cast<int>(kActivityBarWidthEpx);
inline constexpr int kActivityIconPx = static_cast<int>(kActivityIconEpx);
inline constexpr float kIconStrokeWidth = 1.6f;
inline constexpr int kExplorerLineLeft = 5;
inline constexpr int kExplorerLineMid = 10;
inline constexpr int kExplorerLineRight = 17;
inline constexpr int kExplorerLineCount = 3;
inline constexpr int kExplorerLineSpacing = 6;
inline constexpr int kExplorerLineTop = 5;
inline constexpr int kExplorerBoxSize = 3;
inline constexpr float kSearchCircleLeft = 4.0f;
inline constexpr float kSearchCircleTop = 4.0f;
inline constexpr float kSearchCircleSize = 11.0f;
inline constexpr float kSearchHandleStart = 13.5f;
inline constexpr float kSearchHandleEnd = 19.0f;

// 画 Explorer 图标：三行缩进加连接线，代表树。
void PaintExplorerIcon(QPainter& painter) {
  const float line_top = static_cast<float>(kExplorerLineTop);
  const float spacing = static_cast<float>(kExplorerLineSpacing);
  const float left = static_cast<float>(kExplorerLineLeft);
  const float mid = static_cast<float>(kExplorerLineMid);
  const float right = static_cast<float>(kExplorerLineRight);
  const float box = static_cast<float>(kExplorerBoxSize);
  for (int row = 0; row < kExplorerLineCount; ++row) {
    const float y = line_top + spacing * static_cast<float>(row);
    const float indent = row == 0 ? 0.0f : mid - left;
    painter.drawLine(QPointF(left + indent, y), QPointF(right, y));
    painter.drawRect(QRectF(left + indent - box, y - box * 0.5f, box, box));
  }
}

// 画 Search 图标：圆加斜柄。
void PaintSearchIcon(QPainter& painter) {
  painter.drawEllipse(QRectF(kSearchCircleLeft, kSearchCircleTop,
                             kSearchCircleSize, kSearchCircleSize));
  painter.drawLine(QPointF(kSearchHandleStart, kSearchHandleStart),
                   QPointF(kSearchHandleEnd, kSearchHandleEnd));
}

QPixmap MakeActivityIcon(PrimarySideBarView view, const QColor& color) {
  QPixmap pixmap(kActivityIconPx, kActivityIconPx);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(QPen(color, kIconStrokeWidth));
  if (view == PrimarySideBarView::Explorer) {
    PaintExplorerIcon(painter);
  } else {
    PaintSearchIcon(painter);
  }
  return pixmap;
}

}  // namespace

ActivityBar::ActivityBar(QWidget* parent) : QWidget(parent) {
  setObjectName("ActivityBar");
  setFixedWidth(kActivityBarWidthPx);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 4, 0, 4);
  layout->setSpacing(2);

  auto* group = new QButtonGroup(this);
  group->setExclusive(true);

  auto make_button = [this, group, layout](PrimarySideBarView view,
                                           const QString& tooltip) {
    auto* button = new QToolButton(this);
    button->setCheckable(true);
    button->setToolTip(tooltip);
    button->setFixedSize(kActivityBarButtonPx, kActivityBarButtonPx);
    button->setIconSize(QSize(kActivityIconPx, kActivityIconPx));
    button->setAutoRaise(true);
    group->addButton(button);
    layout->addWidget(button, 0, Qt::AlignHCenter);
    connect(button, &QToolButton::clicked, this,
            [this, view] { emit viewRequested(view); });
    return button;
  };
  _explorer = make_button(PrimarySideBarView::Explorer, "Explorer");
  _search = make_button(PrimarySideBarView::Search, "Search");
  layout->addStretch(1);

  SetView(PrimarySideBarView::Explorer);
  ApplyTheme();
}

void ActivityBar::SetView(PrimarySideBarView view) {
  _view = view;
  _explorer->setChecked(view == PrimarySideBarView::Explorer);
  _search->setChecked(view == PrimarySideBarView::Search);
}

void ActivityBar::ApplyTheme() {
  UpdateIcons();
}

void ActivityBar::UpdateIcons() {
  const QColor color = palette().color(QPalette::WindowText);
  _explorer->setIcon(QIcon(MakeActivityIcon(PrimarySideBarView::Explorer, color)));
  _search->setIcon(QIcon(MakeActivityIcon(PrimarySideBarView::Search, color)));
}
