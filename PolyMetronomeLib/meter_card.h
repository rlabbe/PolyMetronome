#pragma once

#include "meter.h"

#include <QPoint>
#include <QWidget>

class QFontMetrics;

class MeterCard : public QWidget
{
    Q_OBJECT

public:
    explicit MeterCard(QWidget* parent = nullptr);

    // Card size derived from the widget's current font metrics so it scales
    // with system DPI / font scale. Callers (e.g. parent containers that need
    // to compute a minimum width without owning a card yet) can query the
    // canonical sizes from a font metrics object.
    static QSize card_size_for(const QFontMetrics& fm);

    void set_measure(const MeasureSpec& m);
    const MeasureSpec& measure() const { return measure_; }

    void set_index(int i) { index_ = i; }
    int index() const { return index_; }
    void set_deletable(bool d) { deletable_ = d; update(); }

    QSize sizeHint() const override;

signals:
    void clicked();
    void measure_changed(const MeasureSpec& m);
    void delete_requested();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void enterEvent(QEnterEvent* e) override;
    void leaveEvent(QEvent* e) override;

private:
    enum class Zone { None, BeatsUp, BeatsDown, NoteUp, NoteDown, DeleteX };

    struct Layout {
        QRectF card_rect;            // padded outer rect
        QRectF numbers_rect;         // text area, excludes arrow strip
        int divider_y;
        int number_text_height;
        int beats_center_y;
        int note_center_y;
        QPointF beats_up_tip;
        QPointF beats_down_tip;
        QPointF note_up_tip;
        QPointF note_down_tip;
    };
    void compute_layout();
    Zone zone_at(QPoint p) const;
    void draw_arrow(QPainter& p, QPointF tip, bool up, QColor color);

    Layout layout_;
    MeasureSpec measure_;
    bool hovered_ = false;
    bool deletable_ = false;
    Zone hover_zone_ = Zone::None;
    int index_ = -1;
    QPoint press_pos_;

    // Cached layout sizes, scaled from font metrics in the constructor.
    int card_w_ = 0;
    int card_h_ = 0;
    int arrow_strip_w_ = 0;
    int arrow_y_offset_ = 0;
    int card_pad_ = 0;
    float arrow_half_w_ = 0.0f;
    float arrow_h_ = 0.0f;
    int delete_box_ = 0;
    int big_font_inc_ = 0;
};
