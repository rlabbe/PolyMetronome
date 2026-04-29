#pragma once

#include "meter.h"

#include <QWidget>

class MeterCard : public QWidget
{
    Q_OBJECT

public:
    explicit MeterCard(QWidget* parent = nullptr);

    void set_measure(const MeasureSpec& m);
    const MeasureSpec& measure() const { return measure_; }

    QSize sizeHint() const override;

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void enterEvent(QEnterEvent* e) override;
    void leaveEvent(QEvent* e) override;

private:
    MeasureSpec measure_;
    bool hovered_ = false;
};
