#pragma once

#include <QWidget>

class CountInCard : public QWidget
{
    Q_OBJECT

public:
    explicit CountInCard(QWidget* parent = nullptr);

    int value() const { return value_; }
    void set_value(int v);

    QSize sizeHint() const override;

signals:
    void value_changed(int v);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void enterEvent(QEnterEvent* e) override;
    void leaveEvent(QEvent* e) override;

private:
    enum class Zone { None, Up, Down };
    Zone zone_at(QPoint p) const;

    int value_ = 0;
    Zone hover_zone_ = Zone::None;
};
