#include "poly_metronome_dialog.h"

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

    auto* layout = new QFormLayout(this);

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
    layout->addRow(bpm_row);

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

    layout->addRow("Quarter", quarter_volume_);
    layout->addRow("Eighth", eighth_volume_);
    layout->addRow("Sixteenth", sixteenth_volume_);
    layout->addRow("Triplet", triplet_volume_);
    layout->addRow("Quintuplet", quintuplet_volume_);
    layout->addRow("Accent", accent_volume_);

    beats_per_measure_ = new QSlider(Qt::Horizontal, this);
    beats_per_measure_->setRange(1, 16);
    beats_per_measure_->setValue(4);
    beats_per_measure_label_ = new QLabel("4", this);
    auto* bpm_meas_row = new QWidget(this);
    auto* bpm_meas_h = new QHBoxLayout(bpm_meas_row);
    bpm_meas_h->setContentsMargins(0, 0, 0, 0);
    bpm_meas_h->addWidget(beats_per_measure_);
    bpm_meas_h->addWidget(beats_per_measure_label_);
    layout->addRow("Beats/Measure", bpm_meas_row);

    layout->addRow("Master", master_volume_);

    sound_mode_button_ = new QPushButton(this);
    sound_mode_button_->setCheckable(true);
    sound_mode_button_->setChecked(true);
    layout->addRow(sound_mode_button_);

    start_stop_ = new QPushButton("Start", this);
    layout->addRow(start_stop_);

    connect(bpm_dial_, &QDial::valueChanged, this, &PolyMetronomeDialog::on_bpm_changed);
    connect(master_volume_, &QSlider::valueChanged, this, &PolyMetronomeDialog::on_master_volume_changed);
    connect(quarter_volume_, &QSlider::valueChanged, this, &PolyMetronomeDialog::on_quarter_volume_changed);
    connect(eighth_volume_, &QSlider::valueChanged, this, &PolyMetronomeDialog::on_eighth_volume_changed);
    connect(sixteenth_volume_, &QSlider::valueChanged, this, &PolyMetronomeDialog::on_sixteenth_volume_changed);
    connect(triplet_volume_, &QSlider::valueChanged, this, &PolyMetronomeDialog::on_triplet_volume_changed);
    connect(quintuplet_volume_, &QSlider::valueChanged, this, &PolyMetronomeDialog::on_quintuplet_volume_changed);
    connect(accent_volume_, &QSlider::valueChanged, this, &PolyMetronomeDialog::on_accent_volume_changed);
    connect(beats_per_measure_, &QSlider::valueChanged, this, &PolyMetronomeDialog::on_beats_per_measure_changed);
    connect(sound_mode_button_, &QPushButton::toggled, this, &PolyMetronomeDialog::on_sound_mode_toggled);
    connect(start_stop_, &QPushButton::clicked, this, &PolyMetronomeDialog::on_start_stop_clicked);

    on_bpm_changed(bpm_dial_->value());
    on_master_volume_changed(master_volume_->value());
    on_quarter_volume_changed(quarter_volume_->value());
    on_eighth_volume_changed(eighth_volume_->value());
    on_sixteenth_volume_changed(sixteenth_volume_->value());
    on_triplet_volume_changed(triplet_volume_->value());
    on_quintuplet_volume_changed(quintuplet_volume_->value());
    on_accent_volume_changed(accent_volume_->value());
    on_beats_per_measure_changed(beats_per_measure_->value());
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

void PolyMetronomeDialog::on_beats_per_measure_changed(int n)
{
    metronome_->set_beats_per_measure(n);
    beats_per_measure_label_->setText(QString::number(n));
}

void PolyMetronomeDialog::on_sound_mode_toggled(bool checked)
{
    metronome_->set_mono_mode(checked);
    sound_mode_button_->setText(checked ? "Single Pitch" : "Multi Pitch");
}
