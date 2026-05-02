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
    static_cache_   = QPixmap();   // invalidate
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
    return QSize(kW, kNeedleH);
}

// Geometry shared by paintEvent and build_static_cache.
namespace {
struct Geom {
    float cx, py, radius;
    float led_gap, led_r;
    float rows_top;
};

Geom compute_geom()
{
    Geom g;
    g.cx       = kW / 2.0f;
    g.radius   = std::min(g.cx - 12.0f, 80.0f);
    g.led_gap  = 14.0f;
    g.led_r    = 5.0f;
    float content_h = g.radius + g.led_gap + g.led_r;
    g.py       = (kNeedleH - content_h) / 2.0f + g.radius;
    g.rows_top = g.py + kRowPad;
    return g;
}
}

void BeatMeterWidget::build_static_cache()
{
    static_cache_ = QPixmap(size());
    static_cache_.fill(Qt::transparent);

    QPainter p(&static_cache_);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r(0, 0, width(), height());

    // Amber background
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

    const Geom g = compute_geom();
    const QPointF pivot(g.cx, g.py);

    // Scale arc filled chord
    QRectF arc_rect(g.cx - g.radius, g.py - g.radius, 2 * g.radius, 2 * g.radius);
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
        QPointF outer(g.cx + g.radius * std::cos(rad), g.py - g.radius * std::sin(rad));
        QPointF inner(g.cx + (g.radius - len) * std::cos(rad),
                      g.py - (g.radius - len) * std::sin(rad));
        p.setPen(QPen(QColor(70, 45, 5), is_center ? 2.0 : 1.0));
        p.drawLine(outer, inner);
    }

    // Pivot cap
    p.setBrush(QColor(55, 32, 5));
    p.setPen(QPen(QColor(30, 15, 0), 1));
    p.drawEllipse(pivot, 6.0, 6.0);
    p.setBrush(QColor(130, 90, 20));
    p.setPen(Qt::NoPen);
    p.drawEllipse(pivot, 3.0, 3.0);

    // LEDs in dark/off state
    const QColor col_off(110, 38, 8);
    int n_rows = sequence_.measures.empty() ? 0 : static_cast<int>(sequence_.measures.size());
    for (int row = 0; row < n_rows; ++row) {
        int n_beats  = sequence_.measures[row].numerator;
        float row_cy = g.rows_top + row * kRowH + kRowH * 0.5f;
        for (int b = 0; b < n_beats; ++b) {
            float bx = 12.0f + (b + 0.5f) * (kW - 24.0f) / n_beats;
            QRadialGradient grad(bx - g.led_r * 0.3f, row_cy - g.led_r * 0.3f, g.led_r * 1.2f);
            grad.setColorAt(0, col_off.lighter(120));
            grad.setColorAt(1, col_off);
            p.setBrush(grad);
            p.setPen(QPen(QColor(40, 15, 3), 1));
            p.drawEllipse(QPointF(bx, row_cy), g.led_r, g.led_r);
        }
    }
}

void BeatMeterWidget::paintEvent(QPaintEvent*)
{
    if (static_cache_.size() != size())
        build_static_cache();

    QPainter p(this);
    p.drawPixmap(0, 0, static_cache_);
    p.setRenderHint(QPainter::Antialiasing);

    const Geom g = compute_geom();
    const QPointF pivot(g.cx, g.py);

    // ── Lit LED overlay (only the active one, drawn over the cached dark one) ─
    if (active_measure_ >= 0 && active_measure_ < static_cast<int>(sequence_.measures.size())
        && active_beat_ >= 0)
    {
        const QColor col_on(255, 80, 15);
        int n_beats = sequence_.measures[active_measure_].numerator;
        if (active_beat_ < n_beats) {
            float row_cy = g.rows_top + active_measure_ * kRowH + kRowH * 0.5f;
            float bx     = 12.0f + (active_beat_ + 0.5f) * (kW - 24.0f) / n_beats;

            QRadialGradient glow(bx, row_cy, g.led_r * 3.0f);
            glow.setColorAt(0, QColor(255, 90, 10, 160));
            glow.setColorAt(1, Qt::transparent);
            p.setPen(Qt::NoPen);
            p.setBrush(glow);
            p.drawEllipse(QPointF(bx, row_cy), g.led_r * 3.0f, g.led_r * 3.0f);

            QRadialGradient grad(bx - g.led_r * 0.3f, row_cy - g.led_r * 0.3f, g.led_r * 1.2f);
            grad.setColorAt(0, col_on.lighter(160));
            grad.setColorAt(1, col_on);
            p.setBrush(grad);
            p.setPen(QPen(QColor(40, 15, 3), 1));
            p.drawEllipse(QPointF(bx, row_cy), g.led_r, g.led_r);
        }
    }

    // ── Needle ────────────────────────────────────────────────────────
    float angle_deg = 0.0f;
    if (running_) {
        double beat_ms  = 60000.0 / bpm_;
        double since_ms = static_cast<double>(elapsed_.elapsed() - last_beat_ms_);
        double phase    = std::min(since_ms / beat_ms, 1.0);
        double swing    = kMaxSwing * (1.0 - 2.0 * phase);
        angle_deg       = static_cast<float>(needle_direction_ * swing);
    }

    float arm_rad = (90.0f - angle_deg) * static_cast<float>(std::numbers::pi_v<double>) / 180.0f;
    QPointF tip(g.cx + (g.radius - 8) * std::cos(arm_rad),
                g.py - (g.radius - 8) * std::sin(arm_rad));

    p.setPen(QPen(QColor(60, 35, 5, 70), 3, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(pivot + QPointF(1.5, 1.5), tip + QPointF(1.5, 1.5));

    p.setPen(QPen(QColor(30, 15, 0), 2, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(pivot, tip);

    p.setPen(QPen(QColor(180, 50, 20), 3, Qt::SolidLine, Qt::RoundCap));
    QPointF tip_base(g.cx + (g.radius - 28) * std::cos(arm_rad),
                     g.py - (g.radius - 28) * std::sin(arm_rad));
    p.drawLine(tip_base, tip);

    // Re-draw pivot cap on top of needle base
    p.setBrush(QColor(55, 32, 5));
    p.setPen(QPen(QColor(30, 15, 0), 1));
    p.drawEllipse(pivot, 6.0, 6.0);
    p.setBrush(QColor(130, 90, 20));
    p.setPen(Qt::NoPen);
    p.drawEllipse(pivot, 3.0, 3.0);
}
