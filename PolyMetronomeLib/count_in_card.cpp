#include "count_in_card.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

static constexpr int kArrowW = 14;
static constexpr float kAW = 8.0f;
static constexpr float kAH = 6.0f;
static constexpr int kCardSize = 80;

CountInCard::CountInCard(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(kCardSize, kCardSize);
    setMaximumSize(kCardSize, kCardSize);
    setMouseTracking(true);
}

void CountInCard::set_value(int v)
{
    v = std::max(0, std::min(16, v));
    if (v == value_)
        return;
    value_ = v;
    update();
    emit value_changed(value_);
}

QSize CountInCard::sizeHint() const
{
    return QSize(kCardSize, kCardSize);
}

CountInCard::Zone CountInCard::zone_at(QPoint p) const
{
    if (p.x() < width() - kArrowW)
        return Zone::None;
    return p.y() < height() / 2 ? Zone::Up : Zone::Down;
}

void CountInCard::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRectF r = QRectF(rect()).adjusted(2.0, 2.0, -2.0, -2.0);
    QRectF nr = r.adjusted(0, 0, -kArrowW, 0);

    bool hovered = (hover_zone_ != Zone::None);
    QColor bg = hovered ? QColor(60, 65, 75) : QColor(45, 48, 55);
    QColor border = hovered ? QColor(120, 160, 200) : QColor(80, 85, 95);
    p.setPen(QPen(border, 1.5));
    p.setBrush(bg);
    p.drawRoundedRect(r, 5, 5);

    QFont big = font();
    big.setPointSize(big.pointSize() + 24);
    big.setBold(true);
    QFont small = font();

    static constexpr int kLabelH = 18;

    // "count in" at top, centered across full card width
    p.setFont(small);
    p.setPen(QColor(160, 165, 175));
    p.drawText(QRectF(r.left(), r.top(), r.width(), kLabelH),
               Qt::AlignHCenter | Qt::AlignVCenter, "count in");

    // Number centered in the full card
    QFontMetrics fm(big);
    int big_h  = fm.height();
    int num_cy = static_cast<int>(r.center().y());
    int num_y  = num_cy - big_h / 2;
    p.setFont(big);
    p.setPen(QColor(230, 230, 235));
    p.drawText(QRect(static_cast<int>(r.left()), num_y, static_cast<int>(r.width()), big_h),
               Qt::AlignHCenter | Qt::AlignTop, QString::number(value_));

    auto arrow_color = [&](Zone z) {
        return hover_zone_ == z ? QColor(200, 220, 255) : QColor(110, 115, 130);
    };
    float ax      = static_cast<float>(r.right()) - kArrowW / 2.0f;
    float num_cyf = static_cast<float>(num_cy);

    auto draw = [&](QPointF tip, bool up, QColor color) {
        QPolygonF tri;
        if (up)
            tri << tip << QPointF(tip.x() - kAW/2, tip.y() + kAH) << QPointF(tip.x() + kAW/2, tip.y() + kAH);
        else
            tri << tip << QPointF(tip.x() - kAW/2, tip.y() - kAH) << QPointF(tip.x() + kAW/2, tip.y() - kAH);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawPolygon(tri);
    };

    draw(QPointF(ax, num_cyf - 10), true,  arrow_color(Zone::Up));
    draw(QPointF(ax, num_cyf + 10), false, arrow_color(Zone::Down));
}

void CountInCard::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        switch (zone_at(e->pos())) {
        case Zone::Up:   set_value(value_ + 1); break;
        case Zone::Down: set_value(value_ - 1); break;
        default: break;
        }
    }
}

void CountInCard::mouseMoveEvent(QMouseEvent* e)
{
    Zone z = zone_at(e->pos());
    if (z != hover_zone_) {
        hover_zone_ = z;
        update();
    }
}

void CountInCard::wheelEvent(QWheelEvent* e)
{
    set_value(value_ + (e->angleDelta().y() > 0 ? 1 : -1));
}

void CountInCard::enterEvent(QEnterEvent*)
{
    update();
}

void CountInCard::leaveEvent(QEvent*)
{
    hover_zone_ = Zone::None;
    update();
}
