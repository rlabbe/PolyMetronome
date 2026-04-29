#pragma once

#include <QObject>

class AudioEngine;

class PolyMetronome : public QObject
{
    Q_OBJECT

public:
    explicit PolyMetronome(QObject* parent = nullptr);
    ~PolyMetronome() override;

    void start();
    void stop();
    bool is_running() const;

    void set_bpm(int bpm);
    void set_quarter_volume(float v);
    void set_eighth_volume(float v);
    void set_sixteenth_volume(float v);
    void set_triplet_volume(float v);
    void set_quintuplet_volume(float v);
    void set_accent_volume(float v);
    void set_master_volume(float v);
    void set_beats_per_measure(int n);
    void set_mono_mode(bool on);

private:
    AudioEngine* audio_ = nullptr;
};
