#include "count_in_card.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

QSize CountInCard::card_size_for(const QFontMetrics& fm)
{
    int u = std::min(fm.height(), 16);
    return QSize(u * 9 / 2, u * 5);
}

CountInCard::CountInCard(QWidget* parent)
    : QWidget(parent)
    , value_(0)
{
    int u = std::min(fontMetrics().height(), 16);
    card_w_ = u * 9 / 2;
    card_h_ = u * 5;
    arrow_strip_w_ = u * 7 / 8;
    arrow_y_offset_ = u * 5 / 8;
    card_pad_ = u / 8;
    label_height_ = u * 9 / 8;
    arrow_half_w_ = u * 0.25f;
    arrow_h_ = u * 0.375f;
    big_font_inc_ = 24;

    setMinimumSize(card_w_, card_h_);
    setMaximumSize(card_w_, card_h_);
    setMouseTracking(true);
    compute_layout();
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
    return QSize(card_w_, card_h_);
}

void CountInCard::compute_layout()
{
    layout_.card_rect = QRectF(0, 0, card_w_, card_h_).adjusted(card_pad_, card_pad_, -card_pad_, -card_pad_);
    layout_.number_center_y = static_cast<int>(layout_.card_rect.center().y());

    float arrow_x = static_cast<float>(layout_.card_rect.right()) - arrow_strip_w_ / 2.0f;
    layout_.up_arrow_tip = QPointF(arrow_x, layout_.number_center_y - arrow_y_offset_);
    layout_.down_arrow_tip = QPointF(arrow_x, layout_.number_center_y + arrow_y_offset_);
}

CountInCard::Zone CountInCard::zone_at(QPoint p) const
{
    if (p.x() < width() - arrow_strip_w_)
        return Zone::None;
    return p.y() < height() / 2 ? Zone::Up : Zone::Down;
}

void CountInCard::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    bool any_hovered = (hover_zone_ != Zone::None);
    QColor background_color = any_hovered ? QColor(60, 65, 75) : QColor(45, 48, 55);
    QColor border_color = any_hovered ? QColor(120, 160, 200) : QColor(80, 85, 95);
    p.setPen(QPen(border_color, 1.5));
    p.setBrush(background_color);
    p.drawRoundedRect(layout_.card_rect, 5, 5);

    QFont big_font = font();
    big_font.setPointSize(big_font.pointSize() + big_font_inc_);
    big_font.setBold(true);
    QFont label_font = font();

    // "count in" at top, centered across full card width
    p.setFont(label_font);
    p.setPen(QColor(160, 165, 175));
    p.drawText(QRectF(layout_.card_rect.left(), layout_.card_rect.top(), layout_.card_rect.width(), label_height_),
               Qt::AlignHCenter | Qt::AlignVCenter, "count in");

    // Number centered in the full card
    QFontMetrics metrics(big_font);
    int big_text_height = metrics.height();
    int number_top = layout_.number_center_y - big_text_height / 2;
    p.setFont(big_font);
    p.setPen(QColor(230, 230, 235));
    p.drawText(QRect(static_cast<int>(layout_.card_rect.left()), number_top, static_cast<int>(layout_.card_rect.width()), big_text_height),
               Qt::AlignHCenter | Qt::AlignTop, QString::number(value_));

    auto arrow_color = [&](Zone z) {
        return hover_zone_ == z ? QColor(200, 220, 255) : QColor(110, 115, 130);
    };

    auto draw = [&](QPointF tip, bool up, QColor color) {
        QPolygonF tri;
        if (up)
            tri << tip << QPointF(tip.x() - arrow_half_w_, tip.y() + arrow_h_) << QPointF(tip.x() + arrow_half_w_, tip.y() + arrow_h_);
        else
            tri << tip << QPointF(tip.x() - arrow_half_w_, tip.y() - arrow_h_) << QPointF(tip.x() + arrow_half_w_, tip.y() - arrow_h_);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawPolygon(tri);
    };

    draw(layout_.up_arrow_tip, true, arrow_color(Zone::Up));
    draw(layout_.down_arrow_tip, false, arrow_color(Zone::Down));
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
