#include "beat_meter_widget.h"

#include <QTimer>
#include <QPainter>
#include <QPainterPath>
#include <cmath>
#include <numbers>

static constexpr float kMaxSwing = 38.0f;   // degrees either side of vertical
static constexpr int   kW        = 210;
static constexpr int   kH        = 190;

BeatMeterWidget::BeatMeterWidget(QWidget* parent)
    : QWidget(parent)
{
    setFixedSize(kW, kH);
    frame_timer_ = new QTimer(this);
    frame_timer_->setInterval(16);
    connect(frame_timer_, &QTimer::timeout, this, [this]() { update(); });
}

void BeatMeterWidget::set_bpm(int bpm)
{
    bpm_ = std::max(1, bpm);
}

void BeatMeterWidget::set_running(bool running)
{
    running_ = running;
    if (running) {
        elapsed_.start();
        frame_timer_->start();
    } else {
        frame_timer_->stop();
        update();
    }
}

QSize BeatMeterWidget::sizeHint() const
{
    return QSize(kW, kH);
}

void BeatMeterWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r(0, 0, width(), height());

    // ── Amber background ──────────────────────────────────────────────
    QLinearGradient bg(r.topLeft(), r.bottomLeft());
    bg.setColorAt(0.0, QColor(240, 195, 45));
    bg.setColorAt(1.0, QColor(195, 148, 18));
    p.fillRect(r, bg);

    // Inner bevel
    p.setPen(QPen(QColor(255, 230, 120, 120), 1));
    p.drawLine(r.topLeft() + QPointF(1,1), r.topRight() + QPointF(-1,1));
    p.drawLine(r.topLeft() + QPointF(1,1), r.bottomLeft() + QPointF(1,-1));
    p.setPen(QPen(QColor(100, 70, 5, 160), 1));
    p.drawLine(r.bottomLeft() + QPointF(1,-1), r.bottomRight() + QPointF(-1,-1));
    p.drawLine(r.topRight() + QPointF(-1,1), r.bottomRight() + QPointF(-1,-1));

    // Outer border
    p.setPen(QPen(QColor(80, 55, 5), 2));
    p.setBrush(Qt::NoBrush);
    p.drawRect(r.adjusted(1,1,-1,-1));

    // ── Geometry ──────────────────────────────────────────────────────
    const float cx     = r.width() / 2.0f;
    const float led_r_val = 8.0f;
    const float led_gap   = 18.0f;                   // pivot to LED centre
    const float radius = std::min(cx - 12.0f, 93.0f);
    // Centre the total content (arc top → LED bottom) vertically
    const float content_h = radius + led_gap + led_r_val;
    const float top_margin = (r.height() - content_h) / 2.0f;
    const float py     = top_margin + radius;

    const QPointF pivot(cx, py);

    // ── Compute arm angle & LED brightness ────────────────────────────
    float angle_deg    = 0.0f;
    float led_bright   = 0.0f;

    if (running_) {
        double beat_ms    = 60000.0 / bpm_;
        double t_ms       = static_cast<double>(elapsed_.elapsed());
        // Use total elapsed time so angle alternates sides each beat
        angle_deg = -kMaxSwing * static_cast<float>(std::cos(std::numbers::pi_v<double> * t_ms / beat_ms));
        double ms_in_beat = std::fmod(t_ms, beat_ms);
        led_bright = std::max(0.0f, 1.0f - static_cast<float>(ms_in_beat) / 200.0f);
    }

    // ── Scale arc ─────────────────────────────────────────────────────
    QRectF arc_rect(cx - radius, py - radius, 2 * radius, 2 * radius);

    // Subtle filled chord for scale area
    QPainterPath chord;
    chord.moveTo(pivot);
    chord.arcTo(arc_rect, 0, 180);
    chord.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(160, 120, 10, 50));
    p.drawPath(chord);

    // Arc outline
    p.setPen(QPen(QColor(80, 52, 5), 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawArc(arc_rect, 0, 180 * 16);

    // Tick marks
    static const float kTickAngles[] = { -kMaxSwing, -kMaxSwing*0.66f, -kMaxSwing*0.33f,
                                          0,
                                          kMaxSwing*0.33f,  kMaxSwing*0.66f,  kMaxSwing };
    for (int i = 0; i < 7; ++i) {
        float tick_deg = kTickAngles[i];
        float rad = (90.0f - tick_deg) * static_cast<float>(std::numbers::pi_v<double>) / 180.0f;
        bool is_center = (i == 3);
        float len = is_center ? 16.0f : 10.0f;
        QPointF outer(cx + radius * std::cos(rad), py - radius * std::sin(rad));
        QPointF inner(cx + (radius - len) * std::cos(rad),
                      py - (radius - len) * std::sin(rad));
        p.setPen(QPen(QColor(70, 45, 5), is_center ? 2.0 : 1.0));
        p.drawLine(outer, inner);
    }

    // ── Needle ────────────────────────────────────────────────────────
    float arm_rad = (90.0f - angle_deg)
                  * static_cast<float>(std::numbers::pi_v<double>) / 180.0f;
    QPointF tip(cx + (radius - 8) * std::cos(arm_rad),
                py - (radius - 8) * std::sin(arm_rad));

    // Shadow
    p.setPen(QPen(QColor(60, 35, 5, 70), 3, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(pivot + QPointF(1.5, 1.5), tip + QPointF(1.5, 1.5));

    // Needle body
    p.setPen(QPen(QColor(30, 15, 0), 2, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(pivot, tip);

    // Tip highlight
    p.setPen(QPen(QColor(180, 50, 20), 3, Qt::SolidLine, Qt::RoundCap));
    QPointF tip_base(cx + (radius - 28) * std::cos(arm_rad),
                     py - (radius - 28) * std::sin(arm_rad));
    p.drawLine(tip_base, tip);

    // Pivot cap
    p.setBrush(QColor(55, 32, 5));
    p.setPen(QPen(QColor(30, 15, 0), 1));
    p.drawEllipse(pivot, 6.0, 6.0);
    p.setBrush(QColor(130, 90, 20));
    p.setPen(Qt::NoPen);
    p.drawEllipse(pivot, 3.0, 3.0);

    // ── LED ───────────────────────────────────────────────────────────
    const float led_y = py + led_gap;
    const float led_r = led_r_val;

    if (led_bright > 0.05f) {
        QRadialGradient glow(cx, led_y, led_r * 3.0f);
        glow.setColorAt(0, QColor(255, 90, 10, static_cast<int>(led_bright * 200)));
        glow.setColorAt(1, Qt::transparent);
        p.setPen(Qt::NoPen);
        p.setBrush(glow);
        p.drawEllipse(QPointF(cx, led_y), led_r * 3.0f, led_r * 3.0f);
    }

    QColor led_on (255, 80, 15);
    QColor led_off(110, 38,  8);
    QColor led_col(
        led_off.red()   + static_cast<int>(led_bright * (led_on.red()   - led_off.red())),
        led_off.green() + static_cast<int>(led_bright * (led_on.green() - led_off.green())),
        led_off.blue()  + static_cast<int>(led_bright * (led_on.blue()  - led_off.blue()))
    );
    QRadialGradient led_grad(cx - led_r * 0.3f, led_y - led_r * 0.3f, led_r * 1.2f);
    led_grad.setColorAt(0, led_col.lighter(150));
    led_grad.setColorAt(1, led_col);
    p.setBrush(led_grad);
    p.setPen(QPen(QColor(40, 15, 3), 1));
    p.drawEllipse(QPointF(cx, led_y), led_r, led_r);
}
