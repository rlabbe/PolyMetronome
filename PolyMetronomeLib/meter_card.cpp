#include "meter_card.h"

#include <QApplication>
#include <QByteArray>
#include <QDrag>
#include <QFontMetrics>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <algorithm>

static constexpr int kCardSize = 80;
static constexpr int kArrowW = 14;
static constexpr float kAW = 8.0f;
static constexpr float kAH = 6.0f;

MeterCard::MeterCard(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setMinimumSize(kCardSize, kCardSize);
    setMaximumSize(kCardSize, kCardSize);
}

void MeterCard::set_measure(const MeasureSpec& m)
{
    measure_ = m;
    update();
}

QSize MeterCard::sizeHint() const
{
    return QSize(kCardSize, kCardSize);
}

MeterCard::Zone MeterCard::zone_at(QPoint p) const
{
    if (p.x() < width() - kArrowW)
        return Zone::None;
    QFont big = font();
    big.setPointSize(big.pointSize() + 8);
    big.setBold(true);
    int nth = QFontMetrics(big).height();
    int line_y = (height() - 4) / 2 + 2;   // r.center().y()
    int ncy = line_y - nth / 2;
    int dcy = line_y + nth / 2;
    if (p.y() < line_y)
        return p.y() < ncy ? Zone::NumUp : Zone::NumDown;
    else
        return p.y() < dcy ? Zone::DenUp : Zone::DenDown;
}

void MeterCard::draw_arrow(QPainter& p, QPointF tip, bool up, QColor color)
{
    QPolygonF tri;
    if (up)
        tri << tip << QPointF(tip.x() - kAW / 2, tip.y() + kAH) << QPointF(tip.x() + kAW / 2, tip.y() + kAH);
    else
        tri << tip << QPointF(tip.x() - kAW / 2, tip.y() - kAH) << QPointF(tip.x() + kAW / 2, tip.y() - kAH);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawPolygon(tri);
}

void MeterCard::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRectF r = QRectF(rect()).adjusted(2.0, 2.0, -2.0, -2.0);

    QColor bg = hovered_ ? QColor(60, 65, 75) : QColor(45, 48, 55);
    QColor border = hovered_ ? QColor(120, 160, 200) : QColor(80, 85, 95);
    p.setPen(QPen(border, 1.5));
    p.setBrush(bg);
    p.drawRoundedRect(r, 5, 5);

    // Number area excludes arrow strip
    QRectF nr = r.adjusted(0, 0, -kArrowW, 0);

    QFont big = font();
    big.setPointSize(big.pointSize() + 8);
    big.setBold(true);
    p.setFont(big);

    QFontMetrics fm(big);
    int num_text_h = fm.height();
    int line_y = static_cast<int>(r.center().y());

    p.setPen(QColor(230, 230, 235));
    p.drawText(QRect(static_cast<int>(nr.left()), line_y - num_text_h - 1, static_cast<int>(nr.width()), num_text_h),
               Qt::AlignHCenter | Qt::AlignBottom, QString::number(measure_.numerator));

    p.setPen(QPen(QColor(180, 185, 195), 1.5));
    p.drawLine(QPointF(nr.left() + 14, line_y), QPointF(nr.right() - 14, line_y));

    p.setPen(QColor(230, 230, 235));
    p.drawText(QRect(static_cast<int>(nr.left()), line_y + 1, static_cast<int>(nr.width()), num_text_h),
               Qt::AlignHCenter | Qt::AlignTop, QString::number(measure_.denominator));

    if (measure_.repeat > 1) {
        QFont small = font();
        small.setPointSize(std::max(6, small.pointSize() - 1));
        p.setFont(small);
        p.setPen(QColor(210, 210, 215));
        QString rep_text = QString("×") + QString::number(measure_.repeat);
        p.drawText(QRect(static_cast<int>(nr.left()), static_cast<int>(r.top() + 3), static_cast<int>(nr.width()), 14),
                   Qt::AlignRight | Qt::AlignTop, rep_text);
    }

    if (!measure_.grouping.is_empty() && measure_.numerator > 0) {
        int n = measure_.numerator;
        int avail = static_cast<int>(nr.width()) - 10;
        int dot_y = static_cast<int>(r.bottom()) - 8;
        int spacing = std::max(1, avail / std::max(1, n));
        int start_x = static_cast<int>(nr.left()) + 5 + spacing / 2;
        int group_idx = 0;
        int in_group = 0;
        for (int i = 0; i < n; ++i) {
            bool group_start = (in_group == 0);
            int x = start_x + i * spacing;
            QColor dot = group_start ? QColor(220, 200, 100) : QColor(120, 125, 135);
            p.setBrush(dot);
            p.setPen(Qt::NoPen);
            int rad = group_start ? 3 : 2;
            p.drawEllipse(QPoint(x, dot_y), rad, rad);
            ++in_group;
            if (group_idx < static_cast<int>(measure_.grouping.sizes.size()) && in_group >= measure_.grouping.sizes[group_idx]) {
                ++group_idx;
                in_group = 0;
            }
        }
    }

    // Arrow strip — aligned to text centers
    auto arrow_color = [&](Zone z) {
        return hover_zone_ == z ? QColor(200, 220, 255) : QColor(110, 115, 130);
    };
    float ax  = static_cast<float>(r.right()) - kArrowW / 2.0f;
    float ncy = static_cast<float>(line_y - num_text_h / 2);   // numerator text center
    float dcy = static_cast<float>(line_y + num_text_h / 2);   // denominator text center

    draw_arrow(p, QPointF(ax, ncy - 9), true,  arrow_color(Zone::NumUp));
    draw_arrow(p, QPointF(ax, ncy + 9), false, arrow_color(Zone::NumDown));
    draw_arrow(p, QPointF(ax, dcy - 9), true,  arrow_color(Zone::DenUp));
    draw_arrow(p, QPointF(ax, dcy + 9), false, arrow_color(Zone::DenDown));
}

void MeterCard::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton || e->button() == Qt::RightButton)
        press_pos_ = e->pos();
}

void MeterCard::mouseMoveEvent(QMouseEvent* e)
{
    // Update arrow hover
    if (!(e->buttons() & (Qt::LeftButton | Qt::RightButton))) {
        Zone z = zone_at(e->pos());
        if (z != hover_zone_) {
            hover_zone_ = z;
            update();
        }
        return;
    }

    // Drag
    if ((e->pos() - press_pos_).manhattanLength() < QApplication::startDragDistance())
        return;
    if (index_ < 0)
        return;

    auto* drag = new QDrag(this);
    auto* mime = new QMimeData;
    mime->setData("application/x-meter-card-index", QByteArray::number(index_));
    drag->setMimeData(mime);
    drag->setPixmap(grab());
    drag->setHotSpot(press_pos_);
    drag->exec(Qt::MoveAction);
}

void MeterCard::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton)
        return;

    static const int denoms[] = { 1, 2, 4, 8, 16, 32 };
    static constexpr int ndenom = 6;

    Zone z = zone_at(press_pos_);
    switch (z) {
    case Zone::NumUp:
        measure_.numerator = std::min(32, measure_.numerator + 1);
        update();
        emit measure_changed(measure_);
        return;
    case Zone::NumDown:
        measure_.numerator = std::max(1, measure_.numerator - 1);
        update();
        emit measure_changed(measure_);
        return;
    case Zone::DenUp:
    case Zone::DenDown: {
        int idx = 0;
        for (int i = 0; i < ndenom; ++i)
            if (denoms[i] == measure_.denominator) { idx = i; break; }
        idx = z == Zone::DenUp ? std::min(ndenom - 1, idx + 1) : std::max(0, idx - 1);
        measure_.denominator = denoms[idx];
        update();
        emit measure_changed(measure_);
        return;
    }
    default:
        break;
    }

    if ((e->pos() - press_pos_).manhattanLength() < QApplication::startDragDistance())
        emit clicked();
}

void MeterCard::enterEvent(QEnterEvent*)
{
    hovered_ = true;
    update();
}

void MeterCard::leaveEvent(QEvent*)
{
    hovered_ = false;
    hover_zone_ = Zone::None;
    update();
}
