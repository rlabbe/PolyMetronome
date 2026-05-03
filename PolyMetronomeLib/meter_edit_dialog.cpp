#include "meter_edit_dialog.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QFocusEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

namespace {
class SelectingLineEdit : public QLineEdit
{
public:
    using QLineEdit::QLineEdit;

protected:
    void focusInEvent(QFocusEvent* e) override
    {
        QLineEdit::focusInEvent(e);
        QTimer::singleShot(0, this, [this]() { selectAll(); });
    }
};

constexpr int field_height = 40;
constexpr int field_font_pt_bump = 4;
}

StepperEdit::StepperEdit(QWidget* parent)
    : QWidget(parent)
{
    auto* h = new QHBoxLayout(this);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(4);

    dec_btn_ = new QPushButton(QStringLiteral("−"), this);
    inc_btn_ = new QPushButton(QStringLiteral("+"), this);
    edit_ = new SelectingLineEdit(this);
    edit_->setValidator(new QIntValidator(min_, max_, edit_));
    edit_->setAlignment(Qt::AlignCenter);

    dec_btn_->setAutoRepeat(true);
    inc_btn_->setAutoRepeat(true);

    h->addWidget(dec_btn_);
    h->addWidget(edit_, 1);
    h->addWidget(inc_btn_);

    connect(dec_btn_, &QPushButton::clicked, this, &StepperEdit::on_dec);
    connect(inc_btn_, &QPushButton::clicked, this, &StepperEdit::on_inc);
    connect(edit_, &QLineEdit::textChanged, this, &StepperEdit::on_text_edited);

    apply(0, false);
}

void StepperEdit::set_range(int min_v, int max_v)
{
    min_ = min_v;
    max_ = max_v;
    edit_->setValidator(new QIntValidator(min_, max_, edit_));
    apply(std::clamp(value_, min_, max_), false);
}

void StepperEdit::set_value(int v)
{
    apply(std::clamp(v, min_, max_), false);
}

void StepperEdit::set_field_height(int h)
{
    edit_->setMinimumHeight(h);
    dec_btn_->setMinimumHeight(h);
    inc_btn_->setMinimumHeight(h);
    dec_btn_->setMinimumWidth(h);
    inc_btn_->setMinimumWidth(h);
    dec_btn_->setMaximumWidth(h);
    inc_btn_->setMaximumWidth(h);
}

void StepperEdit::set_font_for_field(const QFont& f)
{
    edit_->setFont(f);
    dec_btn_->setFont(f);
    inc_btn_->setFont(f);
}

void StepperEdit::apply(int v, bool emit_signal)
{
    int new_value = std::clamp(v, min_, max_);
    bool changed = (new_value != value_);
    value_ = new_value;
    QString text_value = QString::number(new_value);
    if (edit_->text() != text_value) {
        QSignalBlocker b(edit_);
        edit_->setText(text_value);
    }
    if (changed && emit_signal)
        emit value_changed(new_value);
}

void StepperEdit::on_dec()
{
    apply(value_ - 1, true);
}

void StepperEdit::on_inc()
{
    apply(value_ + 1, true);
}

void StepperEdit::on_text_edited(const QString& text)
{
    bool ok = false;
    int v = text.toInt(&ok);
    if (!ok)
        return;
    if (v < min_ || v > max_)
        return;
    if (v == value_)
        return;
    value_ = v;
    emit value_changed(v);
}

MeterEditDialog::MeterEditDialog(const MeasureSpec& spec, bool can_delete, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Edit Measure");

    QFont big = font();
    big.setPointSize(big.pointSize() + field_font_pt_bump);

    auto* main = new QVBoxLayout(this);
    main->setSpacing(12);

    auto* form = new QFormLayout;
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    beats_ = new StepperEdit(this);
    beats_->set_range(1, 32);
    beats_->set_field_height(field_height);
    beats_->set_font_for_field(big);
    beats_->set_value(spec.beats);

    auto* note_value_row = new QWidget(this);
    auto* note_value_layout = new QHBoxLayout(note_value_row);
    note_value_layout->setContentsMargins(0, 0, 0, 0);
    note_value_layout->setSpacing(6);
    note_value_group_ = new QButtonGroup(this);
    note_value_group_->setExclusive(true);
    for (int d : { 1, 2, 4, 8, 16, 32 }) {
        auto* b = new QPushButton(QString::number(d), this);
        b->setCheckable(true);
        b->setFont(big);
        b->setMinimumHeight(field_height);
        b->setMinimumWidth(36);
        if (d == spec.note_value)
            b->setChecked(true);
        note_value_group_->addButton(b, d);
        note_value_layout->addWidget(b);
    }
    if (note_value_group_->checkedId() == -1) {
        if (auto* fallback = note_value_group_->button(4))
            fallback->setChecked(true);
    }

    repeat_ = new StepperEdit(this);
    repeat_->set_range(1, 99);
    repeat_->set_field_height(field_height);
    repeat_->set_font_for_field(big);
    repeat_->set_value(std::max(1, spec.repeat));

    grouping_ = new SelectingLineEdit(this);
    grouping_->setPlaceholderText("e.g. 2+2+3");
    grouping_->setText(spec.grouping.is_empty() ? QString() : spec.grouping.to_string());
    grouping_->setFont(big);
    grouping_->setMinimumHeight(field_height);

    auto make_label = [&](const QString& s) {
        auto* l = new QLabel(s, this);
        l->setFont(big);
        return l;
    };

    form->addRow(make_label("Beats"), beats_);
    form->addRow(make_label("Note Value"), note_value_row);
    form->addRow(make_label("Repeat"), repeat_);
    form->addRow(make_label("Grouping"), grouping_);
    main->addLayout(form);

    auto* buttons = new QDialogButtonBox(this);
    delete_btn_ = buttons->addButton("Delete", QDialogButtonBox::DestructiveRole);
    delete_btn_->setEnabled(can_delete);
    delete_btn_->setFont(big);
    delete_btn_->setMinimumHeight(field_height);
    auto* cancel_btn = buttons->addButton(QDialogButtonBox::Cancel);
    cancel_btn->setFont(big);
    cancel_btn->setMinimumHeight(field_height);
    ok_btn_ = buttons->addButton("OK", QDialogButtonBox::AcceptRole);
    ok_btn_->setFont(big);
    ok_btn_->setMinimumHeight(field_height);
    main->addWidget(buttons);

    connect(beats_, &StepperEdit::value_changed, this, &MeterEditDialog::on_beats_changed);
    connect(grouping_, &QLineEdit::textChanged, this, &MeterEditDialog::on_grouping_text_changed);
    connect(note_value_group_, &QButtonGroup::idClicked, this, &MeterEditDialog::on_note_value_clicked);
    connect(ok_btn_, &QPushButton::clicked, this, &MeterEditDialog::on_accept);
    connect(delete_btn_, &QPushButton::clicked, this, &MeterEditDialog::on_delete);
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);

    update_validity();
    adjustSize();
    resize(minimumSizeHint());
}

int MeterEditDialog::current_note_value() const
{
    int id = note_value_group_->checkedId();
    return id > 0 ? id : 4;
}

MeasureSpec MeterEditDialog::measure() const
{
    MeasureSpec m;
    m.beats = beats_->value() > 0 ? beats_->value() : 4;
    m.note_value = current_note_value();
    m.repeat = repeat_->value();
    bool g_ok = false;
    m.grouping = Grouping::parse(grouping_->text(), m.beats, &g_ok);
    if (!g_ok)
        m.grouping = Grouping{};
    return m;
}

void MeterEditDialog::on_accept()
{
    action_ = ActionAccept;
    accept();
}

void MeterEditDialog::on_delete()
{
    action_ = ActionDelete;
    accept();
}

void MeterEditDialog::on_grouping_text_changed()
{
    update_validity();
}

void MeterEditDialog::on_beats_changed(int)
{
    update_validity();
}

void MeterEditDialog::on_note_value_clicked(int)
{
    update_validity();
}

void MeterEditDialog::update_validity()
{
    int n = beats_->value();
    bool beats_ok = (n >= 1);

    bool grouping_ok = true;
    QString g = grouping_->text().trimmed();
    if (!g.isEmpty() && g.compare("auto", Qt::CaseInsensitive) != 0) {
        if (!beats_ok)
            grouping_ok = false;
        else
            Grouping::parse(g, n, &grouping_ok);
    }

    ok_btn_->setEnabled(beats_ok && grouping_ok);
    grouping_->setStyleSheet(grouping_ok ? QString() : QStringLiteral("color: rgb(220, 100, 100);"));
}
