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

static constexpr int k_card_size = 80;
static constexpr int k_arrow_strip_width = 14;
static constexpr float k_arrow_half_width = 4.0f;
static constexpr float k_arrow_height = 6.0f;
static constexpr int k_arrow_y_offset = 9;
static constexpr int k_card_pad = 2;
static constexpr int k_big_font_inc = 8;

MeterCard::MeterCard(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setMinimumSize(k_card_size, k_card_size);
    setMaximumSize(k_card_size, k_card_size);
    compute_layout();
}

void MeterCard::set_measure(const MeasureSpec& m)
{
    measure_ = m;
    update();
}

QSize MeterCard::sizeHint() const
{
    return QSize(k_card_size, k_card_size);
}

void MeterCard::compute_layout()
{
    layout_.card_rect = QRectF(0, 0, k_card_size, k_card_size).adjusted(k_card_pad, k_card_pad, -k_card_pad, -k_card_pad);
    layout_.numbers_rect = layout_.card_rect.adjusted(0, 0, -k_arrow_strip_width, 0);

    QFont big_font = font();
    big_font.setPointSize(big_font.pointSize() + k_big_font_inc);
    big_font.setBold(true);
    layout_.number_text_height = QFontMetrics(big_font).height();

    layout_.divider_y = static_cast<int>(layout_.card_rect.center().y());
    layout_.beats_center_y = layout_.divider_y - layout_.number_text_height / 2;
    layout_.note_center_y = layout_.divider_y + layout_.number_text_height / 2;

    float arrow_x = static_cast<float>(layout_.card_rect.right()) - k_arrow_strip_width / 2.0f;
    layout_.beats_up_tip = QPointF(arrow_x, layout_.beats_center_y - k_arrow_y_offset);
    layout_.beats_down_tip = QPointF(arrow_x, layout_.beats_center_y + k_arrow_y_offset);
    layout_.note_up_tip = QPointF(arrow_x, layout_.note_center_y - k_arrow_y_offset);
    layout_.note_down_tip = QPointF(arrow_x, layout_.note_center_y + k_arrow_y_offset);
}

MeterCard::Zone MeterCard::zone_at(QPoint p) const
{
    if (p.x() < width() - k_arrow_strip_width)
        return Zone::None;
    if (p.y() < layout_.divider_y)
        return p.y() < layout_.beats_center_y ? Zone::BeatsUp : Zone::BeatsDown;
    return p.y() < layout_.note_center_y ? Zone::NoteUp : Zone::NoteDown;
}

void MeterCard::draw_arrow(QPainter& p, QPointF tip, bool up, QColor color)
{
    QPolygonF tri;
    if (up)
        tri << tip << QPointF(tip.x() - k_arrow_half_width, tip.y() + k_arrow_height) << QPointF(tip.x() + k_arrow_half_width, tip.y() + k_arrow_height);
    else
        tri << tip << QPointF(tip.x() - k_arrow_half_width, tip.y() - k_arrow_height) << QPointF(tip.x() + k_arrow_half_width, tip.y() - k_arrow_height);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawPolygon(tri);
}

void MeterCard::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QColor background_color = hovered_ ? QColor(60, 65, 75) : QColor(45, 48, 55);
    QColor border_color = hovered_ ? QColor(120, 160, 200) : QColor(80, 85, 95);
    p.setPen(QPen(border_color, 1.5));
    p.setBrush(background_color);
    p.drawRoundedRect(layout_.card_rect, 5, 5);

    QFont big_font = font();
    big_font.setPointSize(big_font.pointSize() + k_big_font_inc);
    big_font.setBold(true);
    p.setFont(big_font);

    p.setPen(QColor(230, 230, 235));
    p.drawText(QRect(static_cast<int>(layout_.numbers_rect.left()), layout_.divider_y - layout_.number_text_height - 1, static_cast<int>(layout_.numbers_rect.width()), layout_.number_text_height),
               Qt::AlignHCenter | Qt::AlignBottom, QString::number(measure_.beats));

    p.setPen(QPen(QColor(180, 185, 195), 1.5));
    p.drawLine(QPointF(layout_.numbers_rect.left() + 14, layout_.divider_y), QPointF(layout_.numbers_rect.right() - 14, layout_.divider_y));

    p.setPen(QColor(230, 230, 235));
    p.drawText(QRect(static_cast<int>(layout_.numbers_rect.left()), layout_.divider_y + 1, static_cast<int>(layout_.numbers_rect.width()), layout_.number_text_height),
               Qt::AlignHCenter | Qt::AlignTop, QString::number(measure_.note_value));

    if (measure_.repeat > 1) {
        QFont small_font = font();
        small_font.setPointSize(std::max(6, small_font.pointSize() - 1));
        p.setFont(small_font);
        p.setPen(QColor(210, 210, 215));
        QString repeat_text = QString("×") + QString::number(measure_.repeat);
        p.drawText(QRect(static_cast<int>(layout_.numbers_rect.left()), static_cast<int>(layout_.card_rect.top() + 3), static_cast<int>(layout_.numbers_rect.width()), 14),
                   Qt::AlignRight | Qt::AlignTop, repeat_text);
    }

    if (!measure_.grouping.is_empty() && measure_.beats > 0) {
        int n_beats = measure_.beats;
        int avail = static_cast<int>(layout_.numbers_rect.width()) - 10;
        int dot_y = static_cast<int>(layout_.card_rect.bottom()) - 8;
        int spacing = std::max(1, avail / std::max(1, n_beats));
        int start_x = static_cast<int>(layout_.numbers_rect.left()) + 5 + spacing / 2;
        int group_idx = 0;
        int in_group = 0;
        for (int i = 0; i < n_beats; ++i) {
            bool group_start = (in_group == 0);
            int x = start_x + i * spacing;
            QColor dot_color = group_start ? QColor(220, 200, 100) : QColor(120, 125, 135);
            p.setBrush(dot_color);
            p.setPen(Qt::NoPen);
            int dot_radius = group_start ? 3 : 2;
            p.drawEllipse(QPoint(x, dot_y), dot_radius, dot_radius);
            ++in_group;
            if (group_idx < static_cast<int>(measure_.grouping.sizes.size()) && in_group >= measure_.grouping.sizes[group_idx]) {
                ++group_idx;
                in_group = 0;
            }
        }
    }

    // Arrow strip — positions come from cached layout (also used by zone_at)
    auto arrow_color = [&](Zone z) {
        return hover_zone_ == z ? QColor(200, 220, 255) : QColor(110, 115, 130);
    };
    draw_arrow(p, layout_.beats_up_tip, true, arrow_color(Zone::BeatsUp));
    draw_arrow(p, layout_.beats_down_tip, false, arrow_color(Zone::BeatsDown));
    draw_arrow(p, layout_.note_up_tip, true, arrow_color(Zone::NoteUp));
    draw_arrow(p, layout_.note_down_tip, false, arrow_color(Zone::NoteDown));
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

    static const int note_values[] = { 1, 2, 4, 8, 16, 32 };
    static constexpr int n_note_values = 6;

    Zone z = zone_at(press_pos_);
    switch (z) {
    case Zone::BeatsUp:
        measure_.beats = std::min(32, measure_.beats + 1);
        update();
        emit measure_changed(measure_);
        return;
    case Zone::BeatsDown:
        measure_.beats = std::max(1, measure_.beats - 1);
        update();
        emit measure_changed(measure_);
        return;
    case Zone::NoteUp:
    case Zone::NoteDown: {
        int idx = 0;
        for (int i = 0; i < n_note_values; ++i)
            if (note_values[i] == measure_.note_value) { idx = i; break; }
        idx = z == Zone::NoteUp ? std::min(n_note_values - 1, idx + 1) : std::max(0, idx - 1);
        measure_.note_value = note_values[idx];
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
