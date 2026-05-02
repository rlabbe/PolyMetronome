#include "beat_meter_widget.h"

#include <QTimer>
#include <QPainter>
#include <QPainterPath>
#include <cmath>
#include <numbers>

static constexpr float kMaxSwing = 38.0f;   // degrees either side of vertical
static constexpr int   kW        = 210;
static constexpr int   kNeedleH  = 160;     // fixed height of needle/arc region
static constexpr int   kRowH     = 14;      // height per LED row
static constexpr int   kRowPad   = 7;       // padding above and below LED rows

BeatMeterWidget::BeatMeterWidget(QWidget* parent)
    : QWidget(parent)
{
    sequence_ = MeterSequence::default_4_4();
    setFixedSize(sizeHint());
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
        // Pre-position needle at the starting extreme. last_beat_ms_ set far
        // in the past so phase clamps to 1 → angle = direction * cos(π) * max
        // = 1 * -kMaxSwing = -kMaxSwing (left). First real beat will flip
        // direction to -1, giving angle = -1 * cos(0) * max = -kMaxSwing again
        // → no jump. Then swings smoothly to +kMaxSwing over the beat.
        needle_direction_ = 1;
        last_beat_ms_     = -1000000;
        active_measure_   = -1;
        active_beat_      = -1;
        frame_timer_->start();
    } else {
        frame_timer_->stop();
        active_measure_   = -1;
        active_beat_      = -1;
        needle_direction_ = 1;
        last_beat_ms_     = -1000000;
        update();
    }
}

void BeatMeterWidget::set_sequence(const MeterSequence& seq)
{
    sequence_ = seq;
    active_measure_ = -1;
    active_beat_    = -1;
    setFixedSize(sizeHint());
    update();
}

void BeatMeterWidget::on_beat_tick(int measure_index, int beat_within_measure)
{
    active_measure_   = measure_index;
    active_beat_      = beat_within_measure;
    // Anchor one frame in the past so the needle is already one step into
    // its swing on the very next paint, instead of sitting at the extreme.
    last_beat_ms_     = elapsed_.elapsed() - 16;
    needle_direction_ = -needle_direction_;
    update();
}

QSize BeatMeterWidget::sizeHint() const
{
    int n_rows = sequence_.measures.empty() ? 1 : static_cast<int>(sequence_.measures.size());
    return QSize(kW, kNeedleH + kRowPad + n_rows * kRowH + kRowPad);
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

    // ── Needle area geometry ──────────────────────────────────────────
    int n_rows = sequence_.measures.empty() ? 1 : static_cast<int>(sequence_.measures.size());

    const float cx      = kW / 2.0f;
    const float radius  = std::min(cx - 12.0f, 80.0f);
    const float led_gap = 14.0f;
    const float led_r   = 5.0f;
    const float content_h = radius + led_gap + led_r;
    const float py      = (kNeedleH - content_h) / 2.0f + radius;

    const QPointF pivot(cx, py);

    // ── Needle angle (driven by beat_tick → in sync with LEDs/audio) ──
    // phase clamps at 1 so the needle parks at the opposite extreme between
    // beats instead of overshooting/wrapping.
    float angle_deg = 0.0f;
    if (running_) {
        double beat_ms  = 60000.0 / bpm_;
        double since_ms = static_cast<double>(elapsed_.elapsed() - last_beat_ms_);
        double phase    = std::min(since_ms / beat_ms, 1.0);
        double swing    = kMaxSwing * (1.0 - 2.0 * phase);   // linear: +max → -max
        angle_deg       = static_cast<float>(needle_direction_ * swing);
    }

    // ── Scale arc ─────────────────────────────────────────────────────
    QRectF arc_rect(cx - radius, py - radius, 2 * radius, 2 * radius);

    QPainterPath chord;
    chord.moveTo(pivot);
    chord.arcTo(arc_rect, 0, 180);
    chord.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(160, 120, 10, 50));
    p.drawPath(chord);

    p.setPen(QPen(QColor(80, 52, 5), 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawArc(arc_rect, 0, 180 * 16);

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

    p.setPen(QPen(QColor(60, 35, 5, 70), 3, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(pivot + QPointF(1.5, 1.5), tip + QPointF(1.5, 1.5));

    p.setPen(QPen(QColor(30, 15, 0), 2, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(pivot, tip);

    p.setPen(QPen(QColor(180, 50, 20), 3, Qt::SolidLine, Qt::RoundCap));
    QPointF tip_base(cx + (radius - 28) * std::cos(arm_rad),
                     py - (radius - 28) * std::sin(arm_rad));
    p.drawLine(tip_base, tip);

    p.setBrush(QColor(55, 32, 5));
    p.setPen(QPen(QColor(30, 15, 0), 1));
    p.drawEllipse(pivot, 6.0, 6.0);
    p.setBrush(QColor(130, 90, 20));
    p.setPen(Qt::NoPen);
    p.drawEllipse(pivot, 3.0, 3.0);

    // ── LED rows ──────────────────────────────────────────────────────
    const QColor col_on (255, 80, 15);
    const QColor col_off(110, 38,  8);
    const float rows_top = static_cast<float>(kNeedleH) + kRowPad;

    for (int row = 0; row < n_rows; ++row) {
        const MeasureSpec& m = sequence_.measures[row];
        int n_beats  = m.numerator;
        float row_cy = rows_top + row * kRowH + kRowH * 0.5f;

        for (int b = 0; b < n_beats; ++b) {
            float bx = 12.0f + (b + 0.5f) * (kW - 24.0f) / n_beats;
            bool lit = (row == active_measure_ && b == active_beat_);

            if (lit) {
                QRadialGradient glow(bx, row_cy, led_r * 3.0f);
                glow.setColorAt(0, QColor(255, 90, 10, 160));
                glow.setColorAt(1, Qt::transparent);
                p.setPen(Qt::NoPen);
                p.setBrush(glow);
                p.drawEllipse(QPointF(bx, row_cy), led_r * 3.0f, led_r * 3.0f);
            }

            QColor col = lit ? col_on : col_off;
            QRadialGradient grad(bx - led_r * 0.3f, row_cy - led_r * 0.3f, led_r * 1.2f);
            grad.setColorAt(0, col.lighter(lit ? 160 : 120));
            grad.setColorAt(1, col);
            p.setBrush(grad);
            p.setPen(QPen(QColor(40, 15, 3), 1));
            p.drawEllipse(QPointF(bx, row_cy), led_r, led_r);
        }
    }
}
