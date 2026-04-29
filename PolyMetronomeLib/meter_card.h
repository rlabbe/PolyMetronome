#pragma once

#include "meter.h"

#include <QPoint>
#include <QWidget>

class MeterCard : public QWidget
{
    Q_OBJECT

public:
    explicit MeterCard(QWidget* parent = nullptr);

    void set_measure(const MeasureSpec& m);
    const MeasureSpec& measure() const { return measure_; }

    void set_index(int i) { index_ = i; }
    int index() const { return index_; }

    QSize sizeHint() const override;

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void enterEvent(QEnterEvent* e) override;
    void leaveEvent(QEvent* e) override;

private:
    MeasureSpec measure_;
    bool hovered_ = false;
    int index_ = -1;
    QPoint press_pos_;
};
