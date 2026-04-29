#pragma once

#include "meter.h"

#include <QJsonObject>
#include <QObject>

class AudioEngine;

struct PolyMetronomeState
{
    int bpm = 60;
    float master_volume = 0.8f;
    float accent_volume = 0.0f;
    float quarter_volume = 1.0f;
    float eighth_volume = 0.0f;
    float sixteenth_volume = 0.0f;
    float triplet_volume = 0.0f;
    float quintuplet_volume = 0.0f;
    bool mono_mode = true;
    MeterSequence sequence = MeterSequence::default_4_4();

    QJsonObject to_json() const;
    static PolyMetronomeState from_json(const QJsonObject& obj);
};

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
    void set_sequence(const MeterSequence& seq);
    void set_mono_mode(bool on);

private:
    AudioEngine* audio_ = nullptr;
};
