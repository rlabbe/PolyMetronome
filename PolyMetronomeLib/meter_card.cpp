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

MeterCard::MeterCard(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setMinimumSize(70, 70);
    setMaximumSize(70, 70);
}

void MeterCard::set_measure(const MeasureSpec& m)
{
    measure_ = m;
    update();
}

QSize MeterCard::sizeHint() const
{
    return QSize(70, 70);
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

    QFont big = font();
    big.setPointSize(big.pointSize() + 8);
    big.setBold(true);
    p.setFont(big);

    QFontMetrics fm(big);
    int line_y = static_cast<int>(r.center().y());
    int num_text_h = fm.height();

    p.setPen(QColor(230, 230, 235));
    p.drawText(QRect(static_cast<int>(r.left()), line_y - num_text_h - 1, static_cast<int>(r.width()), num_text_h),
               Qt::AlignHCenter | Qt::AlignBottom, QString::number(measure_.numerator));

    p.setPen(QPen(QColor(180, 185, 195), 1.5));
    p.drawLine(QPointF(r.left() + 14, line_y), QPointF(r.right() - 14, line_y));

    p.setPen(QColor(230, 230, 235));
    p.drawText(QRect(static_cast<int>(r.left()), line_y + 1, static_cast<int>(r.width()), num_text_h),
               Qt::AlignHCenter | Qt::AlignTop, QString::number(measure_.denominator));

    if (measure_.repeat > 1) {
        QFont small = font();
        small.setPointSize(std::max(6, small.pointSize() - 1));
        p.setFont(small);
        p.setPen(QColor(210, 210, 215));
        QString rep_text = QString("×") + QString::number(measure_.repeat);
        p.drawText(QRect(static_cast<int>(r.right() - 28), static_cast<int>(r.top() + 4), 24, 14),
                   Qt::AlignRight | Qt::AlignTop, rep_text);
    }

    if (!measure_.grouping.is_empty() && measure_.numerator > 0) {
        int n = measure_.numerator;
        int avail = static_cast<int>(r.width()) - 18;
        int dot_y = static_cast<int>(r.bottom()) - 10;
        int spacing = std::max(1, avail / std::max(1, n));
        int start_x = static_cast<int>(r.left()) + 9 + spacing / 2;
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
}

void MeterCard::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton || e->button() == Qt::RightButton)
        press_pos_ = e->pos();
}

void MeterCard::mouseMoveEvent(QMouseEvent* e)
{
    if (!(e->buttons() & (Qt::LeftButton | Qt::RightButton)))
        return;
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
    if (e->button() == Qt::LeftButton || e->button() == Qt::RightButton)
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
    update();
}
