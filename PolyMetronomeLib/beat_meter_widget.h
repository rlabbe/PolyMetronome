#pragma once

#include "meter.h"

#include <QElapsedTimer>
#include <QPixmap>
#include <QWidget>

class QTimer;

class BeatMeterWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BeatMeterWidget(QWidget* parent = nullptr);

    void set_bpm(int bpm);
    void set_running(bool running);
    void set_sequence(const MeterSequence& seq);

    QSize sizeHint() const override;

public slots:
    void on_beat_tick(int measure_index, int beat_within_measure);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    void build_static_cache();

    QPixmap static_cache_;
    QTimer* frame_timer_;
    int bpm_ = 60;
    bool running_ = false;
    QElapsedTimer elapsed_;
    MeterSequence sequence_;
    int active_measure_   = -1;
    int active_beat_      = -1;
    int needle_direction_ = 1;
    qint64 last_beat_ms_  = -1;
};
