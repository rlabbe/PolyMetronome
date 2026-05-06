// Animation / sync design
// ───────────────────────
// frame_timer_ ticks every kFrameIntervalMs. On each tick the lambda:
//   1. Reads metronome_->processed_samples() — wall-clock-derived sample
//      index since playback start (advances continuously, not in buffer
//      chunks like processedUSecs()).
//   2. Calls metronome_->pop_ticks_through(played), which returns the
//      oldest queued beat whose play_sample has been crossed. AudioEngine
//      pre-schedules ticks ~2 s ahead of audio mixing into pending_ticks_,
//      so each tick crosses its wall-clock instant on time rather than
//      arriving in a clump at the next buffer pull.
//   3. On a popped tick: updates active_measure_ / active_beat_, anchors
//      last_beat_ms_ one frame in the past so the needle is already a step
//      into its swing on the very next paint, and flips needle_direction_.
//   4. Calls update().
//
// paintEvent blits the cached background pixmap (background, arc, ticks,
// pivot cap, dark LEDs), blits the pre-rendered lit-LED pixmap at the
// active beat's position, then draws three line segments for the needle.
// The needle base is offset outward from the pivot so the cached pivot cap
// is never overdrawn — no per-frame ellipse calls.

#include "beat_meter_widget.h"
#include "poly_metronome.h"

#include <QTimer>
#include <QPainter>
#include <QPainterPath>
#include <cmath>
#include <numbers>

static constexpr int   kFrameIntervalMs = 4;
static constexpr float kMaxSwing = 38.0f;   // degrees either side of vertical
static constexpr int   kW = 210;
static constexpr int   kNeedleH = 160;     // fixed height of needle/arc region
static constexpr int   kTopPad = 8;        // gap above arc top
static constexpr int   kRowH = 14;      // height per LED row
static constexpr int   kRowPad = 7;       // padding above and below LED rows

BeatMeterWidget::BeatMeterWidget(QWidget* parent)
    : QWidget(parent)
{
    sequence_ = MeterSequence::default_4_4();
    setFixedSize(sizeHint());
    frame_timer_ = new QTimer(this);
    frame_timer_->setTimerType(Qt::PreciseTimer);
    frame_timer_->setInterval(kFrameIntervalMs);

    connect(frame_timer_, &QTimer::timeout, this, [this]() {
        if (metronome_) {
            qint64 played = metronome_->processed_samples();
            if (auto t = metronome_->pop_ticks_through(played)) {
                active_measure_ = t->measure;
                active_beat_ = t->beat;
                last_beat_ms_ = elapsed_.elapsed() - kFrameIntervalMs;
                needle_direction_ = -needle_direction_;
            }
        }
        update();
    });
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
        last_beat_ms_ = -1000000;
        active_measure_ = -1;
        active_beat_ = -1;
        frame_timer_->start();
    }
    else {
        frame_timer_->stop();
        active_measure_ = -1;
        active_beat_ = -1;
        needle_direction_ = 1;
        last_beat_ms_ = -1000000;
        update();
    }
}

void BeatMeterWidget::set_sequence(const MeterSequence& seq)
{
    sequence_ = seq;
    active_measure_ = -1;
    active_beat_ = -1;
    static_cache_ = QPixmap();   // invalidate
    setFixedSize(sizeHint());
    update();
}

QSize BeatMeterWidget::sizeHint() const
{
    return QSize(kW, kNeedleH);
}


void BeatMeterWidget::compute_geometry()
{
    geometry_.cx = kW / 2.0f;
    geometry_.radius = std::min(geometry_.cx - 12.0f, 80.0f);
    geometry_.led_gap = 14.0f;
    geometry_.led_r = 5.0f;
    geometry_.py = geometry_.radius + kTopPad;
    geometry_.rows_top = geometry_.py + kRowPad;
    pivot_ = QPointF(geometry_.cx, geometry_.py);
}


void BeatMeterWidget::build_static_cache()
{
    compute_geometry();

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
    p.drawLine(r.topLeft() + QPointF(1, 1), r.topRight() + QPointF(-1, 1));
    p.drawLine(r.topLeft() + QPointF(1, 1), r.bottomLeft() + QPointF(1, -1));
    p.setPen(QPen(QColor(100, 70, 5, 160), 1));
    p.drawLine(r.bottomLeft() + QPointF(1, -1), r.bottomRight() + QPointF(-1, -1));
    p.drawLine(r.topRight() + QPointF(-1, 1), r.bottomRight() + QPointF(-1, -1));

    // Outer border
    p.setPen(QPen(QColor(80, 55, 5), 2));
    p.setBrush(Qt::NoBrush);
    p.drawRect(r.adjusted(1, 1, -1, -1));

    // Scale arc
    QRectF arc_rect(geometry_.cx - geometry_.radius, geometry_.py - geometry_.radius, 2 * geometry_.radius, 2 * geometry_.radius);

    // Arc outline
    p.setPen(QPen(QColor(80, 52, 5), 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawArc(arc_rect, 0, 180 * 16);

    // Tick marks
    static const float kTickAngles[] = { -kMaxSwing, -kMaxSwing * 0.66f, -kMaxSwing * 0.33f,
                                          0,
                                          kMaxSwing * 0.33f,  kMaxSwing * 0.66f,  kMaxSwing };
    for (int i = 0; i < 7; ++i) {
        float tick_deg = kTickAngles[i];
        float rad = (90.0f - tick_deg) * static_cast<float>(std::numbers::pi) / 180.0f;
        bool is_center = (i == 3);
        float len = is_center ? 16.0f : 10.0f;
        QPointF outer(geometry_.cx + geometry_.radius * std::cos(rad), geometry_.py - geometry_.radius * std::sin(rad));
        QPointF inner(geometry_.cx + (geometry_.radius - len) * std::cos(rad),
                      geometry_.py - (geometry_.radius - len) * std::sin(rad));
        p.setPen(QPen(QColor(70, 45, 5), is_center ? 2.0 : 1.0));
        p.drawLine(outer, inner);
    }

    // Pivot cap
    p.setBrush(QColor(55, 32, 5));
    p.setPen(QPen(QColor(30, 15, 0), 1));
    p.drawEllipse(pivot_, 6.0, 6.0);
    p.setBrush(QColor(130, 90, 20));
    p.setPen(Qt::NoPen);
    p.drawEllipse(pivot_, 3.0, 3.0);

    // Pre-render the single lit LED (with glow halo) into its own pixmap so
    // paintEvent can blit it instead of drawing.
    {
        const QColor col_on(255, 80, 15);
        float halo = geometry_.led_r * 3.0f;
        int sz = static_cast<int>(std::ceil(halo * 2.0f)) + 2;
        lit_led_ = QPixmap(sz, sz);
        lit_led_.fill(Qt::transparent);
        lit_led_origin_ = QPointF(halo + 1, halo + 1);
        QPainter lp(&lit_led_);
        lp.setRenderHint(QPainter::Antialiasing);
        QRadialGradient glow(lit_led_origin_, halo);
        glow.setColorAt(0, QColor(255, 90, 10, 160));
        glow.setColorAt(1, Qt::transparent);
        lp.setPen(Qt::NoPen);
        lp.setBrush(glow);
        lp.drawEllipse(lit_led_origin_, halo, halo);
        QRadialGradient grad(lit_led_origin_ + QPointF(-geometry_.led_r * 0.3f, -geometry_.led_r * 0.3f),
                             geometry_.led_r * 1.2f);
        grad.setColorAt(0, col_on.lighter(160));
        grad.setColorAt(1, col_on);
        lp.setBrush(grad);
        lp.setPen(QPen(QColor(40, 15, 3), 1));
        lp.drawEllipse(lit_led_origin_, geometry_.led_r, geometry_.led_r);
    }

    // LEDs in dark/off state
    const QColor col_off(110, 38, 8);
    int n_rows = sequence_.measures.empty() ? 0 : static_cast<int>(sequence_.measures.size());
    for (int row = 0; row < n_rows; ++row) {
        int n_beats = sequence_.measures[row].beats;
        float row_cy = geometry_.rows_top + row * kRowH + kRowH * 0.5f;
        for (int b = 0; b < n_beats; ++b) {
            float bx = 12.0f + (b + 0.5f) * (kW - 24.0f) / n_beats;
            QRadialGradient grad(bx - geometry_.led_r * 0.3f, row_cy - geometry_.led_r * 0.3f, geometry_.led_r * 1.2f);
            grad.setColorAt(0, col_off.lighter(120));
            grad.setColorAt(1, col_off);
            p.setBrush(grad);
            p.setPen(QPen(QColor(40, 15, 3), 1));
            p.drawEllipse(QPointF(bx, row_cy), geometry_.led_r, geometry_.led_r);
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


    // ── Lit LED overlay: blit the pre-rendered pixmap ─
    if (active_measure_ >= 0 && active_measure_ < static_cast<int>(sequence_.measures.size())
        && active_beat_ >= 0)
    {
        int n_beats = sequence_.measures[active_measure_].beats;
        if (active_beat_ < n_beats) {
            float row_cy = geometry_.rows_top + active_measure_ * kRowH + kRowH * 0.5f;
            float bx = 12.0f + (active_beat_ + 0.5f) * (kW - 24.0f) / n_beats;
            p.drawPixmap(QPointF(bx, row_cy) - lit_led_origin_, lit_led_);
        }
    }

    // ── Needle ────────────────────────────────────────────────────────
    float angle_deg = 0.0f;
    if (running_) {
        int note_value = 4;
        if (active_measure_ >= 0 && active_measure_ < static_cast<int>(sequence_.measures.size()))
            note_value = sequence_.measures[active_measure_].note_value;
        float beat_ms = 60000.0f / bpm_ * 4.0f / note_value;
        float since_ms = static_cast<float>(elapsed_.elapsed() - last_beat_ms_);
        float phase = std::min(since_ms / beat_ms, 1.0f);
        float swing = kMaxSwing * (1.0f - 2.0f * phase);
        angle_deg = needle_direction_ * swing;
    }

    float arm_rad = (90.0f - angle_deg) * std::numbers::pi_v<float> / 180.0f;
    const float r_inner = 7.0f;
    const float arm_cos = std::cos(arm_rad);
    const float arm_sin = std::sin(arm_rad);

    QPointF base(pivot_.x() + r_inner * arm_cos,
                 pivot_.y() - r_inner * arm_sin);
    QPointF tip(pivot_.x() + (geometry_.radius - 8) * arm_cos,
                pivot_.y() - (geometry_.radius - 8) * arm_sin);

    p.setPen(QPen(QColor(60, 35, 5, 70), 3, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(base + QPointF(1.5, 1.5), tip + QPointF(1.5, 1.5));

    p.setPen(QPen(QColor(30, 15, 0), 2, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(base, tip);

    p.setPen(QPen(QColor(180, 50, 20), 3, Qt::SolidLine, Qt::RoundCap));
    QPointF tip_base(geometry_.cx + (geometry_.radius - 28) * arm_cos,
                     geometry_.py - (geometry_.radius - 28) * arm_sin);
    p.drawLine(tip_base, tip);
}
