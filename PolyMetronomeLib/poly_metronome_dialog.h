#pragma once

#include <QDialog>

class QDial;
class QLabel;
class QPushButton;
class QSlider;
class PolyMetronome;

class PolyMetronomeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PolyMetronomeDialog(QWidget* parent = nullptr);
    ~PolyMetronomeDialog() override;

private slots:
    void on_start_stop_clicked();
    void on_bpm_changed(int bpm);
    void on_master_volume_changed(int v);
    void on_quarter_volume_changed(int v);
    void on_eighth_volume_changed(int v);
    void on_sixteenth_volume_changed(int v);
    void on_triplet_volume_changed(int v);
    void on_quintuplet_volume_changed(int v);
    void on_accent_volume_changed(int v);
    void on_beats_per_measure_changed(int n);
    void on_sound_mode_toggled(bool checked);

private:
    PolyMetronome* metronome_ = nullptr;
    QDial* bpm_dial_ = nullptr;
    QLabel* bpm_label_ = nullptr;
    QSlider* master_volume_ = nullptr;
    QSlider* quarter_volume_ = nullptr;
    QSlider* eighth_volume_ = nullptr;
    QSlider* sixteenth_volume_ = nullptr;
    QSlider* triplet_volume_ = nullptr;
    QSlider* quintuplet_volume_ = nullptr;
    QSlider* accent_volume_ = nullptr;
    QSlider* beats_per_measure_ = nullptr;
    QLabel* beats_per_measure_label_ = nullptr;
    QPushButton* sound_mode_button_ = nullptr;
    QPushButton* start_stop_ = nullptr;
};
