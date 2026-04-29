#pragma once

#include "meter.h"

#include <QWidget>
#include <vector>

class QHBoxLayout;
class QScrollArea;
class QComboBox;
class QPushButton;
class MeterCard;

class MeterSequenceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MeterSequenceWidget(QWidget* parent = nullptr);

    void set_sequence(const MeterSequence& seq);
    const MeterSequence& sequence() const { return sequence_; }

signals:
    void sequence_changed(const MeterSequence& seq);

private slots:
    void on_card_clicked(int index);
    void on_add_clicked();
    void on_preset_apply();

private:
    void rebuild_cards();

    MeterSequence sequence_;

    QComboBox* preset_combo_ = nullptr;
    QPushButton* preset_apply_ = nullptr;
    QScrollArea* scroll_ = nullptr;
    QWidget* cards_container_ = nullptr;
    QHBoxLayout* cards_layout_ = nullptr;
    QPushButton* add_btn_ = nullptr;
    std::vector<MeterCard*> cards_;
};
