#include "count_in_card.h"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

CountInCard::CountInCard(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(70, 70);
    setMaximumSize(70, 70);
    setCursor(Qt::PointingHandCursor);
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
    return QSize(70, 70);
}

void CountInCard::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRectF r = QRectF(rect()).adjusted(2.0, 2.0, -2.0, -2.0);

    QColor bg = hovered_ ? QColor(60, 65, 75) : QColor(45, 48, 55);
    QColor border = hovered_ ? QColor(120, 160, 200) : QColor(80, 85, 95);
    p.setPen(QPen(border, 1.5));
    p.setBrush(bg);
    p.drawRoundedRect(r, 5, 5);

    QFont small = font();
    p.setFont(small);
    p.setPen(QColor(160, 165, 175));
    p.drawText(QRectF(r.left(), r.top() + 4, r.width(), 14),
               Qt::AlignHCenter | Qt::AlignTop, "count in");

    QFont big = font();
    big.setPointSize(big.pointSize() + 8);
    big.setBold(true);
    p.setFont(big);
    p.setPen(QColor(230, 230, 235));
    p.drawText(r.adjusted(0, 14, 0, 0).toRect(), Qt::AlignCenter, QString::number(value_));
}

void CountInCard::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton)
        set_value(value_ + 1 > 16 ? 0 : value_ + 1);
    else if (e->button() == Qt::RightButton)
        set_value(value_ - 1);
}

void CountInCard::wheelEvent(QWheelEvent* e)
{
    set_value(value_ + (e->angleDelta().y() > 0 ? 1 : -1));
}

void CountInCard::enterEvent(QEnterEvent*)
{
    hovered_ = true;
    update();
}

void CountInCard::leaveEvent(QEvent*)
{
    hovered_ = false;
    update();
}
