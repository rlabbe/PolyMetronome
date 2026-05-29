#pragma once

#include "meter.h"

#include <QWidget>
#include <vector>

class QHBoxLayout;
class QScrollArea;
class QComboBox;
class QPushButton;
class QFrame;
class QDragEnterEvent;
class QDragMoveEvent;
class QDragLeaveEvent;
class QDropEvent;
class MeterCard;

class MeterSequenceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MeterSequenceWidget(QWidget* parent = nullptr);

    void set_sequence(const MeterSequence& seq);
    const MeterSequence& sequence() const { return sequence_; }
    void set_prefix_widget(QWidget* w);

signals:
    void sequence_changed(const MeterSequence& seq);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void on_card_clicked(int index);
    void on_add_clicked();

private:
    void rebuild_cards();
    int compute_insertion_index(int cursor_x) const;
    int compute_indicator_x(int target) const;
    void handle_drag_enter(QDragEnterEvent* e);
    void handle_drag_move(QDragMoveEvent* e);
    void handle_drag_leave();
    void handle_drop(QDropEvent* e);

    MeterSequence sequence_;

    QComboBox* preset_combo_ = nullptr;
    QPushButton* preset_apply_ = nullptr;
    QHBoxLayout* cards_row_ = nullptr;
    QScrollArea* scroll_ = nullptr;
    QWidget* cards_container_ = nullptr;
    QHBoxLayout* cards_layout_ = nullptr;
    QPushButton* add_btn_ = nullptr;
    QFrame* drop_indicator_ = nullptr;
    std::vector<MeterCard*> cards_;
};
