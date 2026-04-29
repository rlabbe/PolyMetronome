#include "meter_edit_dialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
class InvertedSpinBox : public QSpinBox
{
public:
    using QSpinBox::QSpinBox;
    void stepBy(int steps) override { QSpinBox::stepBy(-steps); }
};
}

MeterEditDialog::MeterEditDialog(const MeasureSpec& spec, bool can_delete, bool can_move_left, bool can_move_right, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Edit Measure");

    auto* main = new QVBoxLayout(this);

    auto* toolbar = new QHBoxLayout;
    move_left_btn_ = new QPushButton("◀", this);
    delete_btn_ = new QPushButton("✕", this);
    move_right_btn_ = new QPushButton("▶", this);
    move_left_btn_->setEnabled(can_move_left);
    delete_btn_->setEnabled(can_delete);
    move_right_btn_->setEnabled(can_move_right);
    move_left_btn_->setToolTip("Move left");
    delete_btn_->setToolTip("Delete this measure");
    move_right_btn_->setToolTip("Move right");
    move_left_btn_->setMaximumWidth(40);
    delete_btn_->setMaximumWidth(40);
    move_right_btn_->setMaximumWidth(40);
    toolbar->addWidget(move_left_btn_);
    toolbar->addWidget(delete_btn_);
    toolbar->addWidget(move_right_btn_);
    toolbar->addStretch();
    main->addLayout(toolbar);

    auto* form = new QFormLayout;

    numerator_ = new InvertedSpinBox(this);
    numerator_->setRange(1, 32);
    numerator_->setValue(spec.numerator);

    denominator_ = new QComboBox(this);
    for (int d : { 1, 2, 4, 8, 16, 32 })
        denominator_->addItem(QString::number(d), d);
    int idx = denominator_->findData(spec.denominator);
    denominator_->setCurrentIndex(idx >= 0 ? idx : 2);

    repeat_ = new InvertedSpinBox(this);
    repeat_->setRange(1, 99);
    repeat_->setValue(std::max(1, spec.repeat));

    grouping_ = new QLineEdit(this);
    grouping_->setPlaceholderText("e.g. 2+2+3 (or empty for none)");
    grouping_->setText(spec.grouping.is_empty() ? QString() : spec.grouping.to_string());

    form->addRow("Numerator", numerator_);
    form->addRow("Denominator", denominator_);
    form->addRow("Repeat", repeat_);
    form->addRow("Grouping", grouping_);
    main->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    ok_btn_ = buttons->addButton("OK", QDialogButtonBox::AcceptRole);
    main->addWidget(buttons);

    connect(numerator_, &QSpinBox::valueChanged, this, &MeterEditDialog::on_numerator_changed);
    connect(grouping_, &QLineEdit::textChanged, this, &MeterEditDialog::on_grouping_text_changed);
    connect(ok_btn_, &QPushButton::clicked, this, &MeterEditDialog::on_accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(move_left_btn_, &QPushButton::clicked, this, &MeterEditDialog::on_move_left);
    connect(move_right_btn_, &QPushButton::clicked, this, &MeterEditDialog::on_move_right);
    connect(delete_btn_, &QPushButton::clicked, this, &MeterEditDialog::on_delete);

    update_grouping_validity();
}

MeasureSpec MeterEditDialog::measure() const
{
    MeasureSpec m;
    m.numerator = numerator_->value();
    m.denominator = denominator_->currentData().toInt();
    m.repeat = repeat_->value();
    bool ok = false;
    m.grouping = Grouping::parse(grouping_->text(), m.numerator, &ok);
    if (!ok)
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

void MeterEditDialog::on_move_left()
{
    action_ = ActionMoveLeft;
    accept();
}

void MeterEditDialog::on_move_right()
{
    action_ = ActionMoveRight;
    accept();
}

void MeterEditDialog::on_grouping_text_changed()
{
    update_grouping_validity();
}

void MeterEditDialog::on_numerator_changed()
{
    update_grouping_validity();
}

void MeterEditDialog::update_grouping_validity()
{
    QString txt = grouping_->text().trimmed();
    bool ok = true;
    if (!txt.isEmpty() && txt.compare("auto", Qt::CaseInsensitive) != 0)
        Grouping::parse(txt, numerator_->value(), &ok);
    ok_btn_->setEnabled(ok);
    grouping_->setStyleSheet(ok ? QString() : QStringLiteral("color: rgb(220, 100, 100);"));
}
