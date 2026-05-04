#include "count_in_card.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

static constexpr int k_card_size = 80;
static constexpr int k_arrow_strip_width = 14;
static constexpr float k_arrow_half_width = 4.0f;
static constexpr float k_arrow_height = 6.0f;
static constexpr int k_arrow_y_offset = 10;
static constexpr int k_card_pad = 2;
static constexpr int k_label_height = 18;
static constexpr int k_big_font_inc = 24;

CountInCard::CountInCard(QWidget* parent)
    : QWidget(parent)
    , value_(0)
{
    setMinimumSize(k_card_size, k_card_size);
    setMaximumSize(k_card_size, k_card_size);
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
    return QSize(k_card_size, k_card_size);
}

void CountInCard::compute_layout()
{
    layout_.card_rect = QRectF(0, 0, k_card_size, k_card_size).adjusted(k_card_pad, k_card_pad, -k_card_pad, -k_card_pad);
    layout_.number_center_y = static_cast<int>(layout_.card_rect.center().y());

    float arrow_x = static_cast<float>(layout_.card_rect.right()) - k_arrow_strip_width / 2.0f;
    layout_.up_arrow_tip = QPointF(arrow_x, layout_.number_center_y - k_arrow_y_offset);
    layout_.down_arrow_tip = QPointF(arrow_x, layout_.number_center_y + k_arrow_y_offset);
}

CountInCard::Zone CountInCard::zone_at(QPoint p) const
{
    if (p.x() < width() - k_arrow_strip_width)
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
    big_font.setPointSize(big_font.pointSize() + k_big_font_inc);
    big_font.setBold(true);
    QFont label_font = font();

    // "count in" at top, centered across full card width
    p.setFont(label_font);
    p.setPen(QColor(160, 165, 175));
    p.drawText(QRectF(layout_.card_rect.left(), layout_.card_rect.top(), layout_.card_rect.width(), k_label_height),
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
            tri << tip << QPointF(tip.x() - k_arrow_half_width, tip.y() + k_arrow_height) << QPointF(tip.x() + k_arrow_half_width, tip.y() + k_arrow_height);
        else
            tri << tip << QPointF(tip.x() - k_arrow_half_width, tip.y() - k_arrow_height) << QPointF(tip.x() + k_arrow_half_width, tip.y() - k_arrow_height);
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
