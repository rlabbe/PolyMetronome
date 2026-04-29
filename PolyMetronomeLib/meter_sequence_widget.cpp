#include "meter_sequence_widget.h"

#include "meter_card.h"
#include "meter_edit_dialog.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

MeterSequenceWidget::MeterSequenceWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* main = new QVBoxLayout(this);
    main->setContentsMargins(0, 0, 0, 0);

    auto* preset_row = new QHBoxLayout;
    preset_row->addWidget(new QLabel("Preset:", this));
    preset_combo_ = new QComboBox(this);
    for (const auto& p : PresetLibrary::all())
        preset_combo_->addItem(p.name);
    preset_apply_ = new QPushButton("Apply", this);
    preset_row->addWidget(preset_combo_, 1);
    preset_row->addWidget(preset_apply_);
    main->addLayout(preset_row);

    scroll_ = new QScrollArea(this);
    scroll_->setWidgetResizable(true);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->setFixedHeight(126);
    scroll_->setFrameShape(QFrame::StyledPanel);

    cards_container_ = new QWidget;
    cards_layout_ = new QHBoxLayout(cards_container_);
    cards_layout_->setSpacing(8);
    cards_layout_->setContentsMargins(8, 8, 8, 8);
    cards_layout_->addStretch();
    scroll_->setWidget(cards_container_);
    main->addWidget(scroll_);

    connect(preset_apply_, &QPushButton::clicked, this, &MeterSequenceWidget::on_preset_apply);

    sequence_ = MeterSequence::default_4_4();
    rebuild_cards();
}

void MeterSequenceWidget::set_sequence(const MeterSequence& seq)
{
    sequence_ = seq;
    if (sequence_.empty())
        sequence_ = MeterSequence::default_4_4();
    rebuild_cards();
    emit sequence_changed(sequence_);
}

void MeterSequenceWidget::rebuild_cards()
{
    for (auto* c : cards_)
        c->deleteLater();
    cards_.clear();
    if (add_btn_) {
        add_btn_->deleteLater();
        add_btn_ = nullptr;
    }
    while (QLayoutItem* item = cards_layout_->takeAt(0))
        delete item;

    for (size_t i = 0; i < sequence_.measures.size(); ++i) {
        auto* card = new MeterCard(cards_container_);
        card->set_measure(sequence_.measures[i]);
        int captured_index = static_cast<int>(i);
        connect(card, &MeterCard::clicked, this, [this, captured_index]() { on_card_clicked(captured_index); });
        cards_layout_->addWidget(card);
        cards_.push_back(card);
    }

    add_btn_ = new QPushButton("+", cards_container_);
    add_btn_->setFixedSize(50, 100);
    QFont f = add_btn_->font();
    f.setPointSize(f.pointSize() + 6);
    f.setBold(true);
    add_btn_->setFont(f);
    add_btn_->setToolTip("Add measure");
    connect(add_btn_, &QPushButton::clicked, this, &MeterSequenceWidget::on_add_clicked);
    cards_layout_->addWidget(add_btn_);
    cards_layout_->addStretch();
}

void MeterSequenceWidget::on_card_clicked(int index)
{
    if (index < 0 || index >= static_cast<int>(sequence_.measures.size()))
        return;
    bool can_delete = sequence_.measures.size() > 1;
    bool can_move_left = index > 0;
    bool can_move_right = index < static_cast<int>(sequence_.measures.size()) - 1;
    MeterEditDialog dlg(sequence_.measures[index], can_delete, can_move_left, can_move_right, this);
    if (dlg.exec() == QDialog::Accepted) {
        switch (dlg.action()) {
        case MeterEditDialog::ActionAccept:
            sequence_.measures[index] = dlg.measure();
            break;
        case MeterEditDialog::ActionDelete:
            if (sequence_.measures.size() > 1)
                sequence_.measures.erase(sequence_.measures.begin() + index);
            break;
        case MeterEditDialog::ActionMoveLeft:
            if (index > 0)
                std::swap(sequence_.measures[index - 1], sequence_.measures[index]);
            break;
        case MeterEditDialog::ActionMoveRight:
            if (index + 1 < static_cast<int>(sequence_.measures.size()))
                std::swap(sequence_.measures[index], sequence_.measures[index + 1]);
            break;
        default:
            break;
        }
        rebuild_cards();
        emit sequence_changed(sequence_);
    }
}

void MeterSequenceWidget::on_add_clicked()
{
    sequence_.measures.push_back(MeasureSpec{ 4, 4, 1, {} });
    rebuild_cards();
    emit sequence_changed(sequence_);
}

void MeterSequenceWidget::on_preset_apply()
{
    int idx = preset_combo_->currentIndex();
    const auto& presets = PresetLibrary::all();
    if (idx < 0 || idx >= static_cast<int>(presets.size()))
        return;
    sequence_ = presets[idx].sequence;
    rebuild_cards();
    emit sequence_changed(sequence_);
}
