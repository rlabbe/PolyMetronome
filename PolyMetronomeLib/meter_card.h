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
    void measure_changed(const MeasureSpec& m);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void enterEvent(QEnterEvent* e) override;
    void leaveEvent(QEvent* e) override;

private:
    enum class Zone { None, NumUp, NumDown, DenUp, DenDown };
    Zone zone_at(QPoint p) const;
    void draw_arrow(QPainter& p, QPointF tip, bool up, QColor color);

    MeasureSpec measure_;
    bool hovered_ = false;
    Zone hover_zone_ = Zone::None;
    int index_ = -1;
    QPoint press_pos_;
};
