#pragma once

#include "meter.h"

#include <QDialog>
#include <QWidget>

class QButtonGroup;
class QLineEdit;
class QPushButton;

class StepperEdit : public QWidget
{
    Q_OBJECT

public:
    explicit StepperEdit(QWidget* parent = nullptr);

    void set_range(int min_v, int max_v);
    int value() const { return value_; }
    void set_value(int v);
    QLineEdit* line_edit() const { return edit_; }
    void set_field_height(int h);
    void set_font_for_field(const QFont& f);

signals:
    void value_changed(int v);

private slots:
    void on_dec();
    void on_inc();
    void on_text_edited(const QString& text);

private:
    void apply(int v, bool emit_signal);

    QPushButton* dec_btn_ = nullptr;
    QPushButton* inc_btn_ = nullptr;
    QLineEdit* edit_ = nullptr;
    int min_ = 0;
    int max_ = 99;
    int value_ = 0;
};

class MeterEditDialog : public QDialog
{
    Q_OBJECT

public:
    enum Action
    {
        ActionNone,
        ActionAccept,
        ActionDelete
    };

    MeterEditDialog(const MeasureSpec& spec, bool can_delete, QWidget* parent = nullptr);

    MeasureSpec measure() const;
    Action action() const { return action_; }

private slots:
    void on_accept();
    void on_delete();
    void on_grouping_text_changed();
    void on_beats_changed(int);
    void on_note_value_clicked(int id);

private:
    void update_validity();
    int current_note_value() const;

    StepperEdit* beats_ = nullptr;
    QButtonGroup* note_value_group_ = nullptr;
    StepperEdit* repeat_ = nullptr;
    QLineEdit* grouping_ = nullptr;
    QPushButton* ok_btn_ = nullptr;
    QPushButton* delete_btn_ = nullptr;
    Action action_ = ActionNone;
};
