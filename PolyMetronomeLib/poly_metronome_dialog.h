#pragma once

#include "meter.h"
#include "poly_metronome.h"
#include "poly_metronome_export.h"

#include <QDial>
#include <QDialog>
#include <QKeyEvent>
#include <QMouseEvent>

class BpmDial : public QDial
{
    Q_OBJECT
public:
    using QDial::QDial;
signals:
    void value_committed(int value);
protected:
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        QDial::mouseReleaseEvent(e);
        emit value_committed(value());
    }
};

class QLabel;
class QPushButton;
class QSlider;
class PolyMetronome;
class MeterSequenceWidget;
class CountInCard;
class BeatMeterWidget;

class POLYMETRONOME_API PolyMetronomeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PolyMetronomeDialog(QWidget* parent = nullptr);
    ~PolyMetronomeDialog() override;

    PolyMetronomeState state() const;
    void apply_state(const PolyMetronomeState& s);
    void send_key_command(Qt::Key key);

signals:
    void state_changed();

protected:
    void closeEvent(QCloseEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void on_start_stop_clicked();
    void on_bpm_changed(int bpm);
    void on_bpm_committed(int bpm);
    void on_master_volume_changed(int v);
    void on_beat_volume_changed(int v);
    void on_eighth_volume_changed(int v);
    void on_sixteenth_volume_changed(int v);
    void on_triplet_volume_changed(int v);
    void on_quintuplet_volume_changed(int v);
    void on_accent_volume_changed(int v);
    void on_sequence_changed(const MeterSequence& seq);
    void on_sound_mode_toggled(bool checked);

private:
    PolyMetronome* metronome_ = nullptr;
    MeterSequenceWidget* meter_widget_ = nullptr;
    BpmDial* bpm_dial_ = nullptr;
    QLabel* bpm_label_ = nullptr;
    QSlider* master_volume_ = nullptr;
    QSlider* beat_volume_ = nullptr;
    QSlider* eighth_volume_ = nullptr;
    QSlider* sixteenth_volume_ = nullptr;
    QSlider* triplet_volume_ = nullptr;
    QSlider* quintuplet_volume_ = nullptr;
    QSlider* accent_volume_ = nullptr;
    QPushButton* sound_mode_button_ = nullptr;
    QPushButton* start_stop_ = nullptr;
    CountInCard* count_in_ = nullptr;
    BeatMeterWidget* beat_meter_ = nullptr;
};
