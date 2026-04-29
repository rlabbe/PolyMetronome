#include "poly_metronome_dialog.h"

#include "meter_sequence_widget.h"
#include "poly_metronome.h"

#include <QDial>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

PolyMetronomeDialog::PolyMetronomeDialog(QWidget* parent)
    : QDialog(parent)
    , metronome_(new PolyMetronome(this))
{
    setWindowTitle("PolyMetronome");

    auto* main = new QVBoxLayout(this);

    meter_widget_ = new MeterSequenceWidget(this);
    main->addWidget(meter_widget_);

    auto* form = new QFormLayout;
    form->setContentsMargins(0, 8, 0, 0);

    bpm_dial_ = new QDial(this);
    bpm_dial_->setRange(30, 240);
    bpm_dial_->setValue(60);
    bpm_dial_->setNotchesVisible(true);
    bpm_dial_->setMinimumSize(120, 120);
    bpm_label_ = new QLabel("60", this);
    bpm_label_->setAlignment(Qt::AlignHCenter);

    auto* bpm_row = new QWidget(this);
    auto* bpm_v = new QVBoxLayout(bpm_row);
    bpm_v->setContentsMargins(0, 0, 0, 0);
    bpm_v->addWidget(bpm_label_);
    bpm_v->addWidget(bpm_dial_, 0, Qt::AlignHCenter);
    form->addRow(bpm_row);

    auto make_slider = [this](int default_pct) {
        auto* s = new QSlider(Qt::Horizontal, this);
        s->setRange(0, 100);
        s->setValue(default_pct);
        return s;
    };

    quarter_volume_ = make_slider(100);
    eighth_volume_ = make_slider(0);
    sixteenth_volume_ = make_slider(0);
    triplet_volume_ = make_slider(0);
    quintuplet_volume_ = make_slider(0);
    accent_volume_ = make_slider(0);
    master_volume_ = make_slider(80);

    form->addRow("Quarter", quarter_volume_);
    form->addRow("Eighth", eighth_volume_);
    form->addRow("Sixteenth", sixteenth_volume_);
    form->addRow("Triplet", triplet_volume_);
    form->addRow("Quintuplet", quintuplet_volume_);
    form->addRow("Accent", accent_volume_);
    form->addRow("Master", master_volume_);

    sound_mode_button_ = new QPushButton(this);
    sound_mode_button_->setCheckable(true);
    sound_mode_button_->setChecked(true);
    form->addRow(sound_mode_button_);

    start_stop_ = new QPushButton("Start", this);
    form->addRow(start_stop_);

    main->addLayout(form);

    connect(meter_widget_, &MeterSequenceWidget::sequence_changed, this, &PolyMetronomeDialog::on_sequence_changed);
    connect(bpm_dial_, &QDial::valueChanged, this, &PolyMetronomeDialog::on_bpm_changed);
    connect(master_volume_, &QSlider::valueChanged, this, &PolyMetronomeDialog::on_master_volume_changed);
    connect(quarter_volume_, &QSlider::valueChanged, this, &PolyMetronomeDialog::on_quarter_volume_changed);
    connect(eighth_volume_, &QSlider::valueChanged, this, &PolyMetronomeDialog::on_eighth_volume_changed);
    connect(sixteenth_volume_, &QSlider::valueChanged, this, &PolyMetronomeDialog::on_sixteenth_volume_changed);
    connect(triplet_volume_, &QSlider::valueChanged, this, &PolyMetronomeDialog::on_triplet_volume_changed);
    connect(quintuplet_volume_, &QSlider::valueChanged, this, &PolyMetronomeDialog::on_quintuplet_volume_changed);
    connect(accent_volume_, &QSlider::valueChanged, this, &PolyMetronomeDialog::on_accent_volume_changed);
    connect(sound_mode_button_, &QPushButton::toggled, this, &PolyMetronomeDialog::on_sound_mode_toggled);
    connect(start_stop_, &QPushButton::clicked, this, &PolyMetronomeDialog::on_start_stop_clicked);

    on_sequence_changed(meter_widget_->sequence());
    on_bpm_changed(bpm_dial_->value());
    on_master_volume_changed(master_volume_->value());
    on_quarter_volume_changed(quarter_volume_->value());
    on_eighth_volume_changed(eighth_volume_->value());
    on_sixteenth_volume_changed(sixteenth_volume_->value());
    on_triplet_volume_changed(triplet_volume_->value());
    on_quintuplet_volume_changed(quintuplet_volume_->value());
    on_accent_volume_changed(accent_volume_->value());
    on_sound_mode_toggled(sound_mode_button_->isChecked());

    adjustSize();
    setFixedHeight(height());
    resize(width() * 2, height());
}

PolyMetronomeDialog::~PolyMetronomeDialog()
{
    if (metronome_->is_running())
        metronome_->stop();
}

PolyMetronomeState PolyMetronomeDialog::state() const
{
    PolyMetronomeState s;
    s.bpm = bpm_dial_->value();
    s.master_volume = master_volume_->value() / 100.0f;
    s.accent_volume = accent_volume_->value() / 100.0f;
    s.quarter_volume = quarter_volume_->value() / 100.0f;
    s.eighth_volume = eighth_volume_->value() / 100.0f;
    s.sixteenth_volume = sixteenth_volume_->value() / 100.0f;
    s.triplet_volume = triplet_volume_->value() / 100.0f;
    s.quintuplet_volume = quintuplet_volume_->value() / 100.0f;
    s.mono_mode = sound_mode_button_->isChecked();
    s.sequence = meter_widget_->sequence();
    return s;
}

void PolyMetronomeDialog::apply_state(const PolyMetronomeState& s)
{
    {
        QSignalBlocker b1(bpm_dial_);
        QSignalBlocker b2(master_volume_);
        QSignalBlocker b3(accent_volume_);
        QSignalBlocker b4(quarter_volume_);
        QSignalBlocker b5(eighth_volume_);
        QSignalBlocker b6(sixteenth_volume_);
        QSignalBlocker b7(triplet_volume_);
        QSignalBlocker b8(quintuplet_volume_);
        QSignalBlocker b9(sound_mode_button_);
        QSignalBlocker b10(meter_widget_);

        bpm_dial_->setValue(s.bpm);
        master_volume_->setValue(static_cast<int>(s.master_volume * 100.0f));
        accent_volume_->setValue(static_cast<int>(s.accent_volume * 100.0f));
        quarter_volume_->setValue(static_cast<int>(s.quarter_volume * 100.0f));
        eighth_volume_->setValue(static_cast<int>(s.eighth_volume * 100.0f));
        sixteenth_volume_->setValue(static_cast<int>(s.sixteenth_volume * 100.0f));
        triplet_volume_->setValue(static_cast<int>(s.triplet_volume * 100.0f));
        quintuplet_volume_->setValue(static_cast<int>(s.quintuplet_volume * 100.0f));
        sound_mode_button_->setChecked(s.mono_mode);
        meter_widget_->set_sequence(s.sequence);
    }

    bpm_label_->setText(QString::number(s.bpm));
    sound_mode_button_->setText(s.mono_mode ? "Single Pitch" : "Multi Pitch");

    metronome_->set_bpm(s.bpm);
    metronome_->set_master_volume(s.master_volume);
    metronome_->set_accent_volume(s.accent_volume);
    metronome_->set_quarter_volume(s.quarter_volume);
    metronome_->set_eighth_volume(s.eighth_volume);
    metronome_->set_sixteenth_volume(s.sixteenth_volume);
    metronome_->set_triplet_volume(s.triplet_volume);
    metronome_->set_quintuplet_volume(s.quintuplet_volume);
    metronome_->set_mono_mode(s.mono_mode);
    metronome_->set_sequence(s.sequence);
}

void PolyMetronomeDialog::closeEvent(QCloseEvent* event)
{
    if (metronome_->is_running()) {
        metronome_->stop();
        start_stop_->setText("Start");
    }
    QDialog::closeEvent(event);
}

void PolyMetronomeDialog::hideEvent(QHideEvent* event)
{
    if (metronome_->is_running()) {
        metronome_->stop();
        start_stop_->setText("Start");
    }
    QDialog::hideEvent(event);
}

void PolyMetronomeDialog::on_start_stop_clicked()
{
    if (metronome_->is_running()) {
        metronome_->stop();
        start_stop_->setText("Start");
    }
    else {
        metronome_->start();
        start_stop_->setText("Stop");
    }
}

void PolyMetronomeDialog::on_bpm_changed(int bpm)
{
    metronome_->set_bpm(bpm);
    bpm_label_->setText(QString::number(bpm));
    emit state_changed();
}

void PolyMetronomeDialog::on_master_volume_changed(int v)
{
    metronome_->set_master_volume(v / 100.0f);
    emit state_changed();
}

void PolyMetronomeDialog::on_quarter_volume_changed(int v)
{
    metronome_->set_quarter_volume(v / 100.0f);
    emit state_changed();
}

void PolyMetronomeDialog::on_eighth_volume_changed(int v)
{
    metronome_->set_eighth_volume(v / 100.0f);
    emit state_changed();
}

void PolyMetronomeDialog::on_sixteenth_volume_changed(int v)
{
    metronome_->set_sixteenth_volume(v / 100.0f);
    emit state_changed();
}

void PolyMetronomeDialog::on_triplet_volume_changed(int v)
{
    metronome_->set_triplet_volume(v / 100.0f);
    emit state_changed();
}

void PolyMetronomeDialog::on_quintuplet_volume_changed(int v)
{
    metronome_->set_quintuplet_volume(v / 100.0f);
    emit state_changed();
}

void PolyMetronomeDialog::on_accent_volume_changed(int v)
{
    metronome_->set_accent_volume(v / 100.0f);
    emit state_changed();
}

void PolyMetronomeDialog::on_sequence_changed(const MeterSequence& seq)
{
    metronome_->set_sequence(seq);
    emit state_changed();
}

void PolyMetronomeDialog::on_sound_mode_toggled(bool checked)
{
    metronome_->set_mono_mode(checked);
    sound_mode_button_->setText(checked ? "Single Pitch" : "Multi Pitch");
    emit state_changed();
}
