#include "meter_sequence_widget.h"

#include "meter_card.h"
#include "meter_edit_dialog.h"

#include <QComboBox>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {
constexpr const char* kCardMimeType = "application/x-meter-card-index";
}

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
    scroll_->setFixedHeight(100);
    scroll_->setFrameShape(QFrame::NoFrame);

    cards_container_ = new QWidget;
    cards_container_->setAcceptDrops(true);
    cards_container_->installEventFilter(this);
    cards_layout_ = new QHBoxLayout(cards_container_);
    cards_layout_->setSpacing(8);
    cards_layout_->setContentsMargins(8, 8, 8, 8);
    cards_layout_->addStretch();
    scroll_->setWidget(cards_container_);
    cards_row_ = new QHBoxLayout;
    cards_row_->setContentsMargins(0, 0, 0, 0);
    cards_row_->setSpacing(6);
    cards_row_->addWidget(scroll_, 1);
    main->addLayout(cards_row_);

    drop_indicator_ = new QFrame(cards_container_);
    drop_indicator_->setFrameShape(QFrame::NoFrame);
    drop_indicator_->setAutoFillBackground(true);
    QPalette pal = drop_indicator_->palette();
    pal.setColor(QPalette::Window, QColor(80, 160, 240));
    drop_indicator_->setPalette(pal);
    drop_indicator_->hide();

    connect(preset_apply_, &QPushButton::clicked, this, &MeterSequenceWidget::on_preset_apply);

    sequence_ = MeterSequence::default_4_4();
    rebuild_cards();
}

void MeterSequenceWidget::set_prefix_widget(QWidget* w)
{
    cards_row_->insertWidget(0, w);
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
        card->set_index(static_cast<int>(i));
        int captured_index = static_cast<int>(i);
        connect(card, &MeterCard::clicked, this, [this, captured_index]() { on_card_clicked(captured_index); });
        connect(card, &MeterCard::measure_changed, this, [this, captured_index](const MeasureSpec& m) {
            if (captured_index < static_cast<int>(sequence_.measures.size())) {
                sequence_.measures[captured_index] = m;
                emit sequence_changed(sequence_);
            }
        });
        cards_layout_->addWidget(card);
        cards_.push_back(card);
    }

    add_btn_ = new QPushButton("+", cards_container_);
    add_btn_->setFixedSize(50, 80);
    QFont f = add_btn_->font();
    f.setPointSize(f.pointSize() + 6);
    f.setBold(true);
    add_btn_->setFont(f);
    add_btn_->setToolTip("Add measure");
    connect(add_btn_, &QPushButton::clicked, this, &MeterSequenceWidget::on_add_clicked);
    cards_layout_->addWidget(add_btn_);
    cards_layout_->addStretch();

    drop_indicator_->raise();
    drop_indicator_->hide();
}

void MeterSequenceWidget::on_card_clicked(int index)
{
    if (index < 0 || index >= static_cast<int>(sequence_.measures.size()))
        return;
    bool can_delete = sequence_.measures.size() > 1;
    MeterEditDialog dlg(sequence_.measures[index], can_delete, this);
    if (dlg.exec() == QDialog::Accepted) {
        switch (dlg.action()) {
        case MeterEditDialog::ActionAccept:
            sequence_.measures[index] = dlg.measure();
            break;
        case MeterEditDialog::ActionDelete:
            if (sequence_.measures.size() > 1)
                sequence_.measures.erase(sequence_.measures.begin() + index);
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

bool MeterSequenceWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == cards_container_) {
        switch (event->type()) {
        case QEvent::DragEnter:
            handle_drag_enter(static_cast<QDragEnterEvent*>(event));
            return true;
        case QEvent::DragMove:
            handle_drag_move(static_cast<QDragMoveEvent*>(event));
            return true;
        case QEvent::DragLeave:
            handle_drag_leave();
            return true;
        case QEvent::Drop:
            handle_drop(static_cast<QDropEvent*>(event));
            return true;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

int MeterSequenceWidget::compute_insertion_index(int cursor_x) const
{
    for (int i = 0; i < static_cast<int>(cards_.size()); ++i) {
        if (cursor_x < cards_[i]->geometry().center().x())
            return i;
    }
    return static_cast<int>(cards_.size());
}

int MeterSequenceWidget::compute_indicator_x(int target) const
{
    if (cards_.empty())
        return 8;
    if (target <= 0)
        return cards_.front()->geometry().left() - 4;
    if (target >= static_cast<int>(cards_.size()))
        return cards_.back()->geometry().right() + 4;
    return (cards_[target - 1]->geometry().right() + cards_[target]->geometry().left()) / 2;
}

void MeterSequenceWidget::handle_drag_enter(QDragEnterEvent* e)
{
    if (e->mimeData()->hasFormat(kCardMimeType))
        e->acceptProposedAction();
    else
        e->ignore();
}

void MeterSequenceWidget::handle_drag_move(QDragMoveEvent* e)
{
    if (!e->mimeData()->hasFormat(kCardMimeType)) {
        e->ignore();
        return;
    }
    e->acceptProposedAction();
    int target = compute_insertion_index(e->position().toPoint().x());
    int x = compute_indicator_x(target);
    int top = 4;
    int h = cards_container_->height() - 8;
    drop_indicator_->setGeometry(x - 1, top, 3, h);
    drop_indicator_->show();
    drop_indicator_->raise();
}

void MeterSequenceWidget::handle_drag_leave()
{
    drop_indicator_->hide();
}

void MeterSequenceWidget::handle_drop(QDropEvent* e)
{
    drop_indicator_->hide();
    if (!e->mimeData()->hasFormat(kCardMimeType)) {
        e->ignore();
        return;
    }
    bool ok = false;
    int source = e->mimeData()->data(kCardMimeType).toInt(&ok);
    if (!ok || source < 0 || source >= static_cast<int>(sequence_.measures.size())) {
        e->ignore();
        return;
    }
    int target = compute_insertion_index(e->position().toPoint().x());
    if (target < 0)
        target = 0;
    if (target > static_cast<int>(sequence_.measures.size()))
        target = static_cast<int>(sequence_.measures.size());
    e->setDropAction(Qt::MoveAction);
    e->accept();

    if (target == source || target == source + 1)
        return;

    auto m = sequence_.measures[source];
    sequence_.measures.erase(sequence_.measures.begin() + source);
    if (target > source)
        --target;
    sequence_.measures.insert(sequence_.measures.begin() + target, m);

    rebuild_cards();
    emit sequence_changed(sequence_);
}
