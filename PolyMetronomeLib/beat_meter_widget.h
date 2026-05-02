#pragma once

#include <QElapsedTimer>
#include <QWidget>

class QTimer;

class BeatMeterWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BeatMeterWidget(QWidget* parent = nullptr);

    void set_bpm(int bpm);
    void set_running(bool running);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QTimer* frame_timer_;
    int bpm_ = 60;
    bool running_ = false;
    QElapsedTimer elapsed_;
};
