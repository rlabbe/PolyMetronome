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

PolyMetronomeDialog::~PolyMetronomeDialog() = default;

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
}

void PolyMetronomeDialog::on_master_volume_changed(int v)
{
    metronome_->set_master_volume(v / 100.0f);
}

void PolyMetronomeDialog::on_quarter_volume_changed(int v)
{
    metronome_->set_quarter_volume(v / 100.0f);
}

void PolyMetronomeDialog::on_eighth_volume_changed(int v)
{
    metronome_->set_eighth_volume(v / 100.0f);
}

void PolyMetronomeDialog::on_sixteenth_volume_changed(int v)
{
    metronome_->set_sixteenth_volume(v / 100.0f);
}

void PolyMetronomeDialog::on_triplet_volume_changed(int v)
{
    metronome_->set_triplet_volume(v / 100.0f);
}

void PolyMetronomeDialog::on_quintuplet_volume_changed(int v)
{
    metronome_->set_quintuplet_volume(v / 100.0f);
}

void PolyMetronomeDialog::on_accent_volume_changed(int v)
{
    metronome_->set_accent_volume(v / 100.0f);
}

void PolyMetronomeDialog::on_sequence_changed(const MeterSequence& seq)
{
    metronome_->set_sequence(seq);
}

void PolyMetronomeDialog::on_sound_mode_toggled(bool checked)
{
    metronome_->set_mono_mode(checked);
    sound_mode_button_->setText(checked ? "Single Pitch" : "Multi Pitch");
}
