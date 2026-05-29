#pragma once

#include "meter.h"

#include <QElapsedTimer>
#include <QPixmap>
#include <QWidget>

class QTimer;
class QFontMetrics;
class PolyMetronome;

class BeatMeterWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BeatMeterWidget(QWidget* parent = nullptr);

    static QSize size_for(const QFontMetrics& fm);

    void set_bpm(int bpm);
    void set_running(bool running);
    void set_sequence(const MeterSequence& seq);
    void set_metronome(PolyMetronome* m) { metronome_ = m; }

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;

private:
    void compute_geometry();
    struct Geom {
        float cx, py, radius;
        float led_gap, led_r;
        float rows_top;
    };

    void build_face_cache();
    void build_static_cache();

    Geom geometry_;
    QPointF pivot_;

    QPixmap face_cache_;
    QPixmap static_cache_;
    QPixmap lit_led_;
    QPointF lit_led_origin_;
    QTimer* frame_timer_;
    PolyMetronome* metronome_ = nullptr;
    int bpm_ = 60;
    bool running_ = false;
    QElapsedTimer elapsed_;
    MeterSequence sequence_;
    int active_measure_ = -1;
    int active_beat_ = -1;
    int needle_direction_ = 1;
    qint64 last_beat_ms_ = -1;

    int w_ = 0;
    int needle_h_ = 0;
    int top_pad_ = 0;
    int row_h_ = 0;
    int row_pad_ = 0;
};
