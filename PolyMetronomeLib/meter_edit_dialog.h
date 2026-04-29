#pragma once

#include "meter.h"

#include <QDialog>

class QSpinBox;
class QComboBox;
class QLineEdit;
class QPushButton;

class MeterEditDialog : public QDialog
{
    Q_OBJECT

public:
    enum Action
    {
        ActionNone,
        ActionAccept,
        ActionDelete,
        ActionMoveLeft,
        ActionMoveRight
    };

    MeterEditDialog(const MeasureSpec& spec, bool can_delete, bool can_move_left, bool can_move_right, QWidget* parent = nullptr);

    MeasureSpec measure() const;
    Action action() const { return action_; }

private slots:
    void on_accept();
    void on_delete();
    void on_move_left();
    void on_move_right();
    void on_grouping_text_changed();
    void on_numerator_changed();

private:
    void update_grouping_validity();

    QSpinBox* numerator_ = nullptr;
    QComboBox* denominator_ = nullptr;
    QSpinBox* repeat_ = nullptr;
    QLineEdit* grouping_ = nullptr;
    QPushButton* ok_btn_ = nullptr;
    QPushButton* delete_btn_ = nullptr;
    QPushButton* move_left_btn_ = nullptr;
    QPushButton* move_right_btn_ = nullptr;
    Action action_ = ActionNone;
};
